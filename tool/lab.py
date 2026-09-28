#!/usr/bin/env python3
"""USB control and plots for the optional MemoryLAN trial firmware."""

import argparse
import html
import json
import os
from pathlib import Path
import re
import select
import sys
import termios
import time


def exchange(port, command):
    if not re.fullmatch(r"/dev/(cu|tty)\.usbmodem[\w.-]+", port):
        raise ValueError("Use the exact USB-modem port reported by ./go port")
    if "\n" in command or "\r" in command or len(command) > 94:
        raise ValueError("Invalid command")
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[1] = attrs[3] = 0
        attrs[2] = termios.CS8 | termios.CLOCAL | termios.CREAD
        attrs[4] = attrs[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIFLUSH)
        os.write(fd, (command + "\n").encode("ascii"))
        pending = b""
        end = time.monotonic() + 8
        while time.monotonic() < end:
            readable, _, _ = select.select([fd], [], [], 0.25)
            if not readable:
                continue
            chunk = os.read(fd, 4096)
            if not chunk:
                raise RuntimeError("Board disconnected")
            pending += chunk
            if len(pending) > 65536:
                raise RuntimeError("Unexpected serial output")
            while b"\n" in pending:
                line, pending = pending.split(b"\n", 1)
                if not line.startswith(b"@MLAN "):
                    continue
                reply = json.loads(line[6:])
                if command == "dump" and reply.get("kind") == "mlan-run":
                    return reply
                if command != "dump" and "ok" in reply:
                    if reply.get("message", "").startswith("trial firmware"):
                        continue
                    if not reply["ok"]:
                        raise RuntimeError(reply.get("message", "Board rejected command"))
                    return reply
        raise RuntimeError("No trial response; check firmware, cable and port. Do not retry start blindly—dump status first.")
    finally:
        os.close(fd)


def integer(record, key, minimum=0, maximum=0xffffffff):
    value = record.get(key)
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"Invalid {key}")
    return value


def validate(records):
    if len(records) < 2:
        raise ValueError("Include the source and at least one receiving node")
    nodes = set()
    config = None
    for r in records:
        if not isinstance(r, dict) or type(r.get("schema")) is not int or r["schema"] != 1 or r.get("kind") != "mlan-run":
            raise ValueError("Not a supported MemoryLAN record")
        if r.get("state") != "done" or r.get("storage_ok") is not True:
            raise ValueError("Only finished, successfully saved runs can be graphed")
        for key in ("node", "source"):
            if not isinstance(r.get(key), str) or not re.fullmatch(r"[0-9a-f]{4}", r[key]):
                raise ValueError(f"Invalid {key}")
        if r["node"] in nodes:
            raise ValueError("Duplicate node: do not mix repeated runs in one plot")
        nodes.add(r["node"])
        integer(r, "run", 1)
        total = integer(r, "total", 1, 256)
        integer(r, "sent", 0, total)
        integer(r, "gap_ms", 100, 60000)
        integer(r, "limit_ms", 1000, 3600000)
        integer(r, "elapsed_ms", 1)
        integer(r, "stop_reason", 1, 2)
        for key in ("duplicates", "invalid", "rx", "tx", "rx_drop", "rx_bad", "tx_fail"):
            integer(r, key)
        if r.get("cutoff") != "local" or not isinstance(r.get("build"), str) or not r["build"]:
            raise ValueError("Missing build or observation-window information")
        ids = r.get("received")
        if not isinstance(ids, list) or any(type(i) is not int or not 0 <= i < total for i in ids):
            raise ValueError("Invalid received-page IDs")
        if len(ids) != len(set(ids)):
            raise ValueError("Received-page IDs must already be deduplicated")
        signature = tuple(r[k] for k in ("run", "source", "total", "gap_ms", "limit_ms", "build"))
        if config is not None and signature != config:
            raise ValueError("Mixed run/configuration/firmware records")
        config = signature
    sources = [r for r in records if r["node"] == r["source"]]
    if len(sources) != 1:
        raise ValueError("Include exactly one source record")
    source = sources[0]
    if source["sent"] != source["total"] or set(source["received"]) != set(range(source["sent"])):
        raise ValueError("Source did not finish the planned injection")
    if any(r["sent"] for r in records if r is not source):
        raise ValueError("A receiver record reports local injection")
    return source


def svg_start(width, height, title, subtitle):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
            '<rect width="100%" height="100%" fill="white"/>',
            '<style>text{font-family:Arial,sans-serif;fill:#17232c} .small{font-size:12px}</style>',
            f'<text x="24" y="32" font-size="20">{html.escape(title)}</text>',
            f'<text x="24" y="56" class="small">{html.escape(subtitle)}</text>']


def plots(records, out):
    source = validate(records)
    rows = sorted((r for r in records if r is not source), key=lambda r: r["node"])
    total = source["sent"]
    subtitle = f'Run {source["run"]}; source {source["node"]}; {total} injected pages. Local, unsynchronised start/stop windows.'
    bars = svg_start(940, 155 + 62 * len(rows), "Distinct content received per node", subtitle)
    bars.append('<text x="24" y="78" class="small">Node IDs are not hop counts. Missing content does not identify physical-link loss.</text>')
    summary = {"run": source["run"], "source": source["node"], "injected": total,
               "cutoff": "local, unsynchronised", "build": source["build"], "nodes": []}
    for index, r in enumerate(rows):
        y = 105 + index * 62
        count = len(r["received"])
        percent = 100 * count / total
        bars.extend([f'<text x="24" y="{y+18}" font-size="15">{r["node"]}</text>',
                     f'<rect x="94" y="{y}" width="560" height="24" fill="#e6edf0"/>',
                     f'<rect x="94" y="{y}" width="{5.6*percent:.2f}" height="24" fill="#176b87"/>',
                     f'<text x="668" y="{y+18}" font-size="14">{count}/{total} ({percent:.1f}%)</text>',
                     f'<text x="94" y="{y+42}" class="small">Window {r["elapsed_ms"]/1000:.1f}s; local drops {r["rx_drop"]}; send failures {r["tx_fail"]}; invalid pages {r["invalid"]}</text>'])
        summary["nodes"].append({"node": r["node"], "received": count, "delivery_percent": percent,
                                 "missing": sorted(set(range(total)) - set(r["received"])),
                                 "elapsed_ms": r["elapsed_ms"], "rx_drop": r["rx_drop"],
                                 "tx_fail": r["tx_fail"], "peers": r.get("peers", [])})
    bars.append('</svg>')
    cell = max(3, min(22, 800 // total))
    width = max(940, 100 + cell * total)
    grid = svg_start(width, 170 + 44 * len(rows), "Which pages arrived?", subtitle)
    grid.append('<text x="24" y="80" class="small">Blue = received; grey = missing by local cutoff. Page numbers start at 0.</text>')
    step = max(1, total // 12)
    for seq in range(0, total, step):
        grid.append(f'<text x="{85+seq*cell}" y="102" class="small">{seq}</text>')
    for index, r in enumerate(rows):
        y = 114 + 44 * index
        grid.append(f'<text x="24" y="{y+18}" font-size="15">{r["node"]}</text>')
        seen = set(r["received"])
        for seq in range(total):
            color = "#176b87" if seq in seen else "#e6edf0"
            grid.append(f'<rect x="{85+seq*cell}" y="{y}" width="{cell-1}" height="24" fill="{color}"><title>Node {r["node"]}, page {seq}: {"received" if seq in seen else "missing"}</title></rect>')
    grid.append('</svg>')
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    artifacts = {f'{source["run"]}-bars.svg': "\n".join(bars),
                 f'{source["run"]}-grid.svg': "\n".join(grid),
                 f'{source["run"]}-summary.json': json.dumps(summary, indent=2) + "\n"}
    if any((out / name).exists() for name in artifacts):
        raise ValueError("Output already exists; use a new output folder to preserve previous results")
    for name, content in artifacts.items():
        with (out / name).open("x") as file:
            file.write(content)
    return [out / name for name in artifacts]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    ctl = sub.add_parser("ctl", help="send one explicit command")
    ctl.add_argument("port")
    ctl.add_argument("command", nargs="+")
    dump = sub.add_parser("dump", help="save a board record without overwriting anything")
    dump.add_argument("port")
    dump.add_argument("--out", default="log")
    plot = sub.add_parser("plot", help="validate completed records and draw SVG charts")
    plot.add_argument("files", nargs="+")
    plot.add_argument("--out", default="out")
    args = parser.parse_args()
    try:
        if args.action == "ctl":
            print(json.dumps(exchange(args.port, " ".join(args.command)), indent=2))
        elif args.action == "dump":
            r = exchange(args.port, "dump")
            if not re.fullmatch(r"[0-9a-f]{4}", str(r.get("node", ""))):
                raise ValueError("Invalid board identity")
            run = integer(r, "run")
            dest = Path(args.out)
            dest.mkdir(parents=True, exist_ok=True)
            path = dest / f'{run}-{r["node"]}-{time.time_ns()}.json'
            with path.open("x") as file:
                json.dump(r, file, indent=2)
                file.write("\n")
            print(path)
        else:
            records = [json.loads(Path(p).read_text()) for p in args.files]
            for path in plots(records, args.out):
                print(path)
    except (ValueError, OSError, RuntimeError, json.JSONDecodeError) as e:
        parser.exit(1, f"Error: {e}\n")


if __name__ == "__main__":
    main()
