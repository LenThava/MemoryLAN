#!/usr/bin/env python3

import argparse
import hashlib
import html
import json
from pathlib import Path
import re
import textwrap

from lab import integer, validate


def checked_file(base, entry):
    path = (base / entry["file"]).resolve()
    if not path.is_relative_to(base.resolve()):
        raise ValueError("Evidence files must be inside the manifest folder")
    expected = entry.get("sha256", "")
    if not isinstance(expected, str) or not re.fullmatch(r"[0-9a-f]{64}", expected):
        raise ValueError("Missing SHA-256 for evidence file")
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError(f"SHA-256 mismatch: {entry['file']}")
    return raw


def load_manifest(path):
    path = Path(path)
    manifest = json.loads(path.read_text())
    if not isinstance(manifest, dict) or manifest.get("schema") != 1 or manifest.get("kind") != "mlan-evidence":
        raise ValueError("Unsupported evidence manifest")
    boards = manifest.get("boards", {})
    if not isinstance(boards, dict) or not boards:
        raise ValueError("Missing physical board labels")
    labels = set()
    for node, board in boards.items():
        if not re.fullmatch(r"[0-9a-f]{4}", node) or not isinstance(board, dict):
            raise ValueError("Invalid board label")
        label = integer(board, "label", 1, 99)
        if label in labels or not isinstance(board.get("location"), str):
            raise ValueError("Duplicate or invalid physical board label")
        labels.add(label)
    runs = manifest.get("runs")
    if not isinstance(runs, list) or not runs:
        raise ValueError("No runs in manifest")
    seen = set()
    reports = []
    for run in runs:
        run_id = integer(run, "run", 1)
        if run_id in seen:
            raise ValueError("Duplicate physical run in manifest")
        seen.add(run_id)
        records = [json.loads(checked_file(path.parent, entry)) for entry in run["records"]]
        source = validate(records)
        if source["run"] != run_id or source["source"] != run.get("source"):
            raise ValueError("Manifest and record metadata differ")
        evidence = run.get("evidence", [])
        for entry in evidence:
            checked_file(path.parent, entry)
            if not isinstance(entry.get("description"), str):
                raise ValueError("Evidence needs an observation description")
        reports.append(summarize(records, boards, run))
    return reports


def summarize(records, boards, run):
    source = validate(records)
    total = source["sent"]
    minimum_ms = max(1000, (total - 1) * source["gap_ms"])
    rows = []
    for record in records:
        if record["node"] not in boards:
            raise ValueError("Record has no physical board label")
        board = boards[record["node"]]
        warnings = []
        short = record["elapsed_ms"] < minimum_ms
        if short:
            warnings.append("Inadequate window: shorter than the nominal source injection span")
        if record["stop_reason"] == 1:
            warnings.append("Manual stop: local observation ended explicitly")
        if record["elapsed_ms"] > record["limit_ms"]:
            warnings.append(f"Local timer overrun: {record['elapsed_ms'] - record['limit_ms']} ms")
        if record["rx_drop"]:
            warnings.append(f"Local receive queue dropped {record['rx_drop']} frames")
        peers = record.get("peers")
        if not isinstance(peers, list):
            raise ValueError("Invalid peer observations")
        for peer in peers:
            if not isinstance(peer, dict) or not re.fullmatch(r"[0-9a-f]{4}", str(peer.get("id", ""))):
                raise ValueError("Invalid peer identity")
            integer(peer, "rssi", -128, 127)
            integer(peer, "samples", 1)
            integer(peer, "last_ms", 0, record["elapsed_ms"])
        is_source = record["node"] == source["node"]
        rows.append({"label": board["label"], "node": record["node"],
                     "location": board["location"], "role": "source" if is_source else "receiver",
                     "observed_pages": len(record["received"]), "received": record["received"],
                     "missing_by_local_cutoff": sorted(set(range(total)) - set(record["received"])),
                     "delivery_percent": None if short or is_source else 100 * len(record["received"]) / total,
                     "window_assessment": "inadequate" if short else "minimum duration met; overlap unverified",
                     "elapsed_ms": record["elapsed_ms"], "limit_ms": record["limit_ms"],
                     "stop_reason": record["stop_reason"],
                     **{key: record[key] for key in ("sent", "duplicates", "invalid", "rx", "tx", "rx_drop", "rx_bad", "tx_fail")},
                     "direct_heard_peers": peers, "warnings": warnings})
    rows.sort(key=lambda row: row["label"])
    return {"schema": 1, "kind": "mlan-diagnostic-report", "run": source["run"],
            "source": source["node"], "injected": total, "setting": run["setting"],
            "build": source["build"], "minimum_window_ms": minimum_ms,
            "limitations": ["Historical setup pilot; one physical trial, not independent repeated exports",
                            "Local start/stop windows are unsynchronised; overlap is unverified",
                            "Short-window screening uses the nominal injection span; it is a diagnostic rule, not a minimum transfer time",
                            "Missing application pages do not measure physical-link packet loss",
                            "Direct-heard peers do not establish data routes or continuous connectivity",
                            "No per-page arrival timestamps; no recovery-time curve can be reconstructed"],
            "notes": run.get("notes", []), "provenance": run["records"],
            "observational_evidence": run.get("evidence", []), "nodes": rows}


def markdown(report):
    lines = [f"# Setup pilot {report['run']}", "", report["setting"], "",
             f"Source {report['source']} injected {report['injected']} pages. Firmware: `{report['build']}`.", "",
             "| Board | Role | Observed pages | Local window | Queue drops | Warnings |",
             "|---|---|---|---|---|---|"]
    for row in report["nodes"]:
        count = f"{row['observed_pages']}/{report['injected']}"
        if row["window_assessment"] == "inadequate":
            count += " observed; inadequate window (no delivery percentage)"
        lines.append(f"| {row['label']} ({row['node']}) | {row['role']} | {count} | "
                     f"{row['elapsed_ms']/1000:.3f} s | {row['rx_drop']} | {'; '.join(row['warnings']) or 'None recorded'} |")
    lines += ["", "Observed pages at the source are locally injected pages.", "",
              "Direct-heard beacon observations (not page routes):", ""]
    for row in report["nodes"]:
        peers = ", ".join(f"{p['id']} (last RSSI {p['rssi']}, {p['samples']} samples)" for p in row["direct_heard_peers"])
        lines.append(f"- Board {row['label']}: {peers or 'none recorded'}.")
    lines += ["", "Interpretation limits:", ""]
    lines += [f"- {note}." for note in report["limitations"] + report["notes"]]
    if report["observational_evidence"]:
        lines += ["", "Separate observational evidence:", ""]
        lines += [f"- {entry['description']} Evidence: `{entry['file']}`; SHA-256 `{entry['sha256']}`."
                  for entry in report["observational_evidence"]]
    lines += ["", "Input provenance (SHA-256 verified before reporting):", ""]
    lines += [f"- `{entry['file']}`: `{entry['sha256']}`; original `{entry['original']}`."
              for entry in report["provenance"]]
    return "\n".join(lines) + "\n"


def svg(report):
    esc = html.escape
    width, height = 1080, 150 + len(report["nodes"]) * 110
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             f'<rect width="{width}" height="{height}" fill="white"/>',
             '<style>text{font-family:Arial,sans-serif;fill:#17232c}.small{font-size:12px}</style>',
             f'<text x="24" y="32" font-size="22">Setup pilot {report["run"]}: observed application pages</text>',
             f'<text x="24" y="56" class="small">{esc(report["setting"])}</text>',
             '<text x="24" y="78" class="small">Independent local windows; no physical-link loss or page-route inference. Source row shows injection.</text>']
    for index, row in enumerate(report["nodes"]):
        y = 104 + index * 110
        color = "#b66a17" if row["window_assessment"] == "inadequate" else "#176b87"
        label = f"Board {row['label']} ({row['node']})"
        lines += [f'<text x="24" y="{y+18}" font-size="15">{label}</text>',
                  f'<rect x="195" y="{y}" width="400" height="25" fill="#e6edf0"/>']
        if row["window_assessment"] == "inadequate":
            value = f"{row['observed_pages']}/{report['injected']} observed; INADEQUATE WINDOW"
        else:
            bar = 400 * row["observed_pages"] / report["injected"]
            lines.append(f'<rect x="195" y="{y}" width="{bar:.2f}" height="25" fill="{color}"/>')
            value = f"{row['observed_pages']}/{report['injected']} {'injected' if row['role']=='source' else 'observed'}"
        lines += [f'<text x="610" y="{y+18}" font-size="15">{esc(value)}</text>',
                  f'<text x="195" y="{y+44}" class="small">Local window {row["elapsed_ms"]/1000:.3f}s; queue drops {row["rx_drop"]}; tx failures {row["tx_fail"]}; invalid pages {row["invalid"]}</text>']
        message = "; ".join(row["warnings"]) or "Minimum window met; actual overlap remains unverified"
        for line, text in enumerate(textwrap.wrap(message, width=115)):
            lines.append(f'<text x="195" y="{y+65+line*16}" class="small">{esc(text)}</text>')
    lines += [f'<text x="24" y="{height-15}" class="small">Original records and provenance retained; repeated exports count as one run. See accompanying JSON and Markdown.</text>', '</svg>']
    return "\n".join(lines) + "\n"


def write_reports(reports, out):
    out = Path(out)
    files = {}
    for report in reports:
        stem = str(report["run"])
        files[stem + ".json"] = json.dumps(report, indent=2) + "\n"
        files[stem + ".md"] = markdown(report)
        files[stem + ".svg"] = svg(report)
    if any((out / name).exists() for name in files):
        raise ValueError("Output exists; select a new folder to preserve previous reports")
    out.mkdir(parents=True, exist_ok=True)
    for name, content in files.items():
        with (out / name).open("x") as file:
            file.write(content)
    return [out / name for name in files]


def main():
    parser = argparse.ArgumentParser(description="MemoryLAN trial reports")
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    try:
        for path in write_reports(load_manifest(args.manifest), args.out):
            print(path)
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
