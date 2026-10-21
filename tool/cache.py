#!/usr/bin/env python3

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def figure(rows):
    baseline = [row for row in rows if row["scenario"].startswith("cold_")]
    parts = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="800" height="450" viewBox="0 0 800 450">',
        '<rect width="800" height="450" fill="white"/>',
        '<g font-family="sans-serif" fill="#17212b">',
        '<text x="40" y="34" font-size="22">Content retained in a 10-page cache</text>',
        '<text x="40" y="58" font-size="13">Deterministic host experiment using the actual C++ MemoryLAN core</text>',
        '<text x="40" y="79" font-size="12">No radio, topology, packet-loss or hardware power-cycle measurement</text>',
    ]
    for n in range(0, 21, 5):
        y = 325 - n * 10
        parts += [
            f'<path d="M80 {y} H740" stroke="#e0e5e9"/>',
            f'<text x="65" y="{y + 4}" text-anchor="end" font-size="12">{n}</text>',
        ]
    for i, row in enumerate(baseline):
        x = 150 + i * 210
        for offset, key, color in [(0, "unique_injected", "#c5ccd2"), (60, "resident_count", "#26759e")]:
            value = row[key]
            parts += [
                f'<rect x="{x + offset}" y="{325 - value * 10}" width="48" height="{value * 10}" fill="{color}"/>',
                f'<text x="{x + offset + 24}" y="{315 - value * 10}" text-anchor="middle" font-size="14">{value}</text>',
            ]
        parts.append(f'<text x="{x + 54}" y="350" text-anchor="middle" font-size="14">{row["unique_injected"]} unique pages</text>')
    parts += [
        '<rect x="85" y="374" width="14" height="14" fill="#c5ccd2"/>',
        '<text x="107" y="386" font-size="13">Injected</text>',
        '<rect x="220" y="374" width="14" height="14" fill="#26759e"/>',
        '<text x="242" y="386" font-size="13">Resident after injection</text>',
        '<text x="40" y="423" font-size="12">These are cache snapshots, not delivery percentages. Full scenario details are in cache.json / cache.csv.</text>',
        '</g></svg>',
    ]
    return "\n".join(parts) + "\n"


def refresh_figure(rows):
    cold = next(row for row in rows if row["scenario"] == "cold_20")
    warm = next(row for row in rows if row["scenario"] == "refreshed_20")
    added = set(warm["resident_ids"]) - set(cold["resident_ids"])
    displaced = set(cold["resident_ids"]) - set(warm["resident_ids"])
    assert added == {0} and displaced == {10}
    parts = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="960" height="310" viewBox="0 0 960 310">',
        '<rect width="960" height="310" fill="white"/>',
        '<g font-family="sans-serif" fill="#17212b">',
        '<text x="35" y="34" font-size="22">Refreshing page 0 changes which 10 pages remain</text>',
        '<text x="35" y="59" font-size="13">Actual C++ MemoryLAN core on the host; cache capacity 10; no radio measurements</text>',
        '<text x="35" y="94" font-size="13">Distinct page IDs</text>',
    ]
    for i in range(20):
        parts.append(f'<text x="{307 + i * 29}" y="94" text-anchor="middle" font-size="12">{i}</text>')
    for row, y, label in [(cold, 112, "No duplicate injections"), (warm, 170, "With 10 duplicate injections")]:
        parts.append(f'<text x="35" y="{y + 22}" font-size="14">{label}</text>')
        for i in range(20):
            present = i in row["resident_ids"]
            fill = "#26759e" if present else "#edf0f2"
            stroke = "white"
            if row is warm and i in added:
                fill = "#25806a"
            if row is warm and i in displaced:
                stroke = "#b65e27"
            color = "white" if present else "#59646c"
            parts += [
                f'<rect x="{295 + i * 29}" y="{y}" width="24" height="34" rx="3" fill="{fill}" stroke="{stroke}" stroke-width="2"/>',
                f'<text x="{307 + i * 29}" y="{y + 22}" text-anchor="middle" font-size="12" fill="{color}">{i}</text>',
            ]
    parts += [
        '<rect x="35" y="231" width="14" height="14" fill="#26759e"/>',
        '<text x="57" y="243" font-size="13">Resident</text>',
        '<rect x="163" y="231" width="14" height="14" fill="#edf0f2"/>',
        '<text x="185" y="243" font-size="13">Absent</text>',
        '<text x="295" y="243" font-size="13">Green: page 0 retained. Orange outline: page 10 displaced.</text>',
        '<text x="35" y="279" font-size="12">The second run refreshes page 0 before each new page 10-19. This demonstrates the existing cache mechanism.</text>',
        '</g></svg>',
    ]
    return "\n".join(parts) + "\n"


def main():
    parser = argparse.ArgumentParser(description="MemoryLAN cache experiment")
    parser.add_argument("--out", type=Path, default=Path("out/cache"))
    args = parser.parse_args()
    outputs = ["cache.json", "cache.csv", "cache.svg", "refresh.svg"]
    if any((args.out / name).exists() for name in outputs):
        parser.error("output files already exist; choose a fresh --out directory")
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="mlan-cache-") as tmp:
        exe = str(Path(tmp) / "cache")
        cmd = [
            os.environ.get("CXX", "clang++"), "-std=c++17", "-O1",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-DMLAN_TRIAL",
            "-Wno-writable-strings", "-Wno-deprecated-declarations",
            "-Itest/inc", "-Iesp32", "test/cache.cpp", "esp32/io_dev.cpp",
            "esp32/util.cpp", "-o", exe,
        ]
        subprocess.run(cmd, cwd=root, check=True)
        rows = json.loads(subprocess.check_output([exe], cwd=root, text=True))
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    sources = ["esp32/memoryLAN.cpp", "esp32/memoryLAN.h", "esp32/io_dev.cpp", "esp32/util.cpp", "esp32/trial_data.h"]
    dirty = subprocess.run(["git", "diff", "--quiet", "HEAD", "--", *sources], cwd=root).returncode
    result = {
        "schema": 1,
        "kind": "mlan-cache-host",
        "revision": revision,
        "core_worktree_changed": bool(dirty),
        "implementation": "esp32/memoryLAN.cpp, compiled with existing test/inc substitutes",
        "source_sha256": {
            path: hashlib.sha256((root / path).read_bytes()).hexdigest()
            for path in sources + [
                "esp32/io_dev.h", "esp32/io_buf.h", "esp32/util.h", "test/cache.cpp", "tool/cache.py",
                "test/inc/Arduino.h", "test/inc/sodium/crypto_hash_sha256.h",
            ]
        },
        "conditions": {
            "cache_capacity": 10,
            "payload_bytes": 32,
            "random_value": 0,
            "beacons": "disabled",
            "radio": "stubbed; no frames arrive",
            "idle": "test millis advanced by one hour; no wall-clock wait",
            "reset": "new MemoryLAN object; models RAM initialization only",
        },
        "limitations": [
            "Single-switch cache experiment, not a network simulation or radio test.",
            "No NVS, LittleFS, real clock, hardware reset, power-loss durability or BLE is exercised.",
            "unique_injected counts distinct pages presented before any scenario transition.",
            "resident_ids are ordered oldest to newest; they do not mean pages delivered to another node.",
            "refreshed_20 reinserts page 0 before each of pages 10 through 19.",
        ],
        "results": rows,
    }
    args.out.mkdir(parents=True, exist_ok=True)
    with (args.out / "cache.json").open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    with (args.out / "cache.csv").open("x", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        for row in rows:
            writer.writerow({**row, "resident_ids": ";".join(map(str, row["resident_ids"]))})
    with (args.out / "cache.svg").open("x") as stream:
        stream.write(figure(rows))
    with (args.out / "refresh.svg").open("x") as stream:
        stream.write(refresh_figure(rows))
    print(f"Host cache experiment: {args.out.resolve()}")
    for row in rows:
        print(f"{row['scenario']:14} resident={row['resident_count']:2} page0={row['page0_resident']} ids={row['resident_ids']}")


if __name__ == "__main__":
    main()
