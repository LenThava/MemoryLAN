#!/usr/bin/env python3
import argparse
import csv
import hashlib
import html
import io
import json
from pathlib import Path

from files import mac_id, node_id, number


def integer(value, name, low=0, high=0xffffffff):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"Invalid {name}")
    return value


def validate(record):
    if (not isinstance(record, dict) or type(record.get("schema")) is not int or
            record["schema"] != 1 or record.get("kind") != "mlan-topology"):
        raise ValueError("Expected a topology log")
    mac_id(record.get("mac"))
    node_id(record.get("node"))
    run = number(record.get("run"))
    if record.get("clock") != "local_ms_since_start":
        raise ValueError("Unknown topology clock")
    if not isinstance(record.get("build"), str) or not record["build"]:
        raise ValueError("Missing firmware build")
    duration = integer(record.get("elapsed_ms"), "duration")
    capacity = integer(record.get("capacity"), "event capacity", 1, 65536)
    integer(record.get("overflow"), "event overflow")
    integer(record.get("ignored_run"), "ignored-run count")
    events = record.get("events")
    if not isinstance(events, list) or len(events) > capacity:
        raise ValueError("Invalid topology event list")
    previous = 0
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("Invalid topology event")
        mac_id(event.get("sender"))
        if type(event.get("run")) is not int or event["run"] != run:
            raise ValueError("Event belongs to another run")
        integer(event.get("seq"), "transmission sequence")
        timestamp = integer(event.get("ms"), "event time", 0, duration)
        if timestamp < previous:
            raise ValueError("Topology events are out of time order")
        previous = timestamp
        integer(event.get("rssi"), "RSSI", -128, 127)
        if event.get("type") not in ("beacon", "page", "other"):
            raise ValueError("Unknown topology event type")
        if event.get("page") is not None:
            integer(event["page"], "page number", 0, 255)
    return record


def read_labels(path):
    if path is None:
        return {}
    labels = json.loads(Path(path).read_text())
    if not isinstance(labels, dict):
        raise ValueError("Labels must map full MAC addresses to labels and locations")
    used = set()
    for mac, entry in labels.items():
        mac_id(mac)
        if not isinstance(entry, dict):
            raise ValueError("Invalid board label")
        label = entry.get("label")
        location = entry.get("location", "")
        if (not isinstance(label, str) or not label or len(label) > 32 or
                any(c in label for c in "\r\n\t") or label in used):
            raise ValueError("Board labels must be distinct short strings")
        if not isinstance(location, str) or len(location) > 160:
            raise ValueError("Invalid board location")
        used.add(label)
    return labels


def analyse(paths, labels=None):
    labels = labels or {}
    records, inputs, observers = [], [], set()
    for path in paths:
        path = Path(path).resolve()
        raw = path.read_bytes()
        if len(raw) > 65536:
            raise ValueError("Topology file exceeds transfer limit")
        record = validate(json.loads(raw))
        if record["mac"] in observers:
            raise ValueError("Duplicate observer: do not mix exports or repeated runs")
        observers.add(record["mac"])
        records.append(record)
        inputs.append(dict(file=str(path), sha256=hashlib.sha256(raw).hexdigest()))
    if not records or len({r["run"] for r in records}) != 1:
        raise ValueError("Supply topology logs from exactly one run")
    if len({r["build"] for r in records}) != 1:
        raise ValueError("Mixed firmware builds: use the same binary on every board")
    nodes = observers | set(labels)
    edges = {}
    warnings = ["Times are local to each receiver; they are not synchronised across boards.",
                "An arrow means the receiver heard the sender; it does not prove a two-way link.",
                "Counts are recorded receptions, not transmission totals or radio loss rates.",
                "No observation means not heard during this local window, not proof of no radio path."]
    for record in records:
        if record["overflow"]:
            warnings.append(f'{record["mac"]}: {record["overflow"]} observations omitted after the buffer filled.')
        if record["ignored_run"]:
            warnings.append(f'{record["mac"]}: {record["ignored_run"]} frames from other runs ignored.')
        for event in record["events"]:
            sender, receiver = event["sender"], record["mac"]
            nodes.add(sender)
            if sender == receiver:
                continue
            edge = edges.setdefault((sender, receiver), dict(sender=sender, receiver=receiver,
                observations=0, beacons=0, pages=0, other=0, rssi_min=127, rssi_max=-128,
                first_ms=event["ms"], last_ms=event["ms"]))
            edge["observations"] += 1
            edge[{"beacon": "beacons", "page": "pages", "other": "other"}[event["type"]]] += 1
            edge["rssi_min"] = min(edge["rssi_min"], event["rssi"])
            edge["rssi_max"] = max(edge["rssi_max"], event["rssi"])
            edge["last_ms"] = event["ms"]
    ordered = sorted(nodes, key=lambda mac: (labels.get(mac, {}).get("label", mac), mac))
    missing = sorted(nodes - observers)
    if missing:
        warnings.append("No receiver log supplied for: " + ", ".join(missing) + ". Their matrix columns are blank.")
    return dict(schema=1, kind="mlan-topology-report", run=records[0]["run"],
                clock="local_ms_since_start", inputs=inputs, warnings=warnings,
                nodes=[dict(mac=mac, label=labels.get(mac, {}).get("label", mac),
                            location=labels.get(mac, {}).get("location", ""),
                            receiver_log_supplied=mac in observers) for mac in ordered],
                receivers=[{key: record[key] for key in ("mac", "node", "build", "elapsed_ms",
                           "capacity", "overflow", "ignored_run")} for record in records],
                links=[edges[key] for key in sorted(edges)])


def matrix_csv(report):
    output = io.StringIO()
    writer = csv.writer(output, lineterminator="\n")
    nodes = report["nodes"]
    links = {(e["sender"], e["receiver"]): e["observations"] for e in report["links"]}
    writer.writerow(["sender_mac / receiver_mac", *(n["mac"] for n in nodes)])
    for sender in nodes:
        writer.writerow([sender["mac"], *[
            "" if not receiver["receiver_log_supplied"] or sender["mac"] == receiver["mac"]
            else links.get((sender["mac"], receiver["mac"]), 0) for receiver in nodes]])
    return output.getvalue()


def graph_svg(report):
    nodes = report["nodes"]
    width, height, centre = max(760, 180 * len(nodes)), 520, 240
    positions = {n["mac"]: 90 + (width - 180) * i / max(1, len(nodes) - 1)
                 for i, n in enumerate(nodes)}
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
           '<rect width="100%" height="100%" fill="white"/>',
           '<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse"><path d="M0 0L10 5L0 10z" fill="#333"/></marker></defs>',
           f'<g font-family="sans-serif" fill="#222"><text x="24" y="30" font-size="19">Run {report["run"]}: directly heard transmissions</text>',
           '<text x="24" y="54" font-size="12">Arrow: sender → receiver. Labels on arrows count received frames.</text>']
    for edge in report["links"]:
        a, b = positions[edge["sender"]], positions[edge["receiver"]]
        direction = -1 if a < b else 1
        y = centre + direction * 23
        curve = centre + direction * min(180, 65 + abs(a - b) * 0.15)
        midpoint = (a + b) / 2
        out.append(f'<path d="M{a:g} {y:g}Q{midpoint:g} {curve:g} {b:g} {y:g}" fill="none" stroke="#555" marker-end="url(#arrow)"/>')
        out.append(f'<text x="{midpoint:g}" y="{(y + curve) / 2 + direction * 6:g}" text-anchor="middle" font-size="11">{edge["observations"]}</text>')
    for node in nodes:
        x = positions[node["mac"]]
        style = 'fill="white" stroke="#222"' if node["receiver_log_supplied"] else 'fill="white" stroke="#888" stroke-dasharray="4 3"'
        out.extend([f'<circle cx="{x:g}" cy="{centre}" r="23" {style}/>',
                    f'<text x="{x:g}" y="{centre+4}" text-anchor="middle" font-size="12">{html.escape(node["label"] if len(node["label"]) <= 5 else "")}</text>',
                    f'<text x="{x:g}" y="{centre+42}" text-anchor="middle" font-size="11">{html.escape(node["mac"])}</text>'])
        if node["location"]:
            out.append(f'<text x="{x:g}" y="{centre+59}" text-anchor="middle" font-size="11">{html.escape(node["location"])}</text>')
    out.extend(['<text x="24" y="455" font-size="12">Missing arrows are not proof that communication is impossible.</text>',
                '<text x="24" y="476" font-size="12">Dashed node: no receiver log supplied. Receiver clocks are independent.</text>'])
    omitted = sum(r["overflow"] for r in report["receivers"])
    if omitted:
        out.append(f'<text x="24" y="497" font-size="12">Warning: buffers omitted {omitted} observations; inspect summary.json.</text>')
    out.append('</g></svg>')
    return "\n".join(out) + "\n"


def write_report(report, directory, protected=()):
    directory = Path(directory).resolve()
    products = {"summary.json": json.dumps(report, indent=2) + "\n",
                "matrix.csv": matrix_csv(report), "graph.svg": graph_svg(report)}
    protected = {Path(p).resolve() for p in protected}
    if any((directory / name).resolve() in protected for name in products):
        raise ValueError("Output would overwrite an input file")
    directory.mkdir(parents=True, exist_ok=True)
    for name, content in products.items():
        (directory / name).write_text(content)
    return directory / "summary.json"


def main():
    parser = argparse.ArgumentParser(description="Show directed reachability from one measured run")
    parser.add_argument("files", nargs="+")
    parser.add_argument("--labels")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    try:
        report = analyse(args.files, read_labels(args.labels))
        protected = args.files + ([args.labels] if args.labels else [])
        print(write_report(report, args.out, protected))
    except (ValueError, OSError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
