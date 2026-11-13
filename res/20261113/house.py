#!/usr/bin/env python3
"""House and cut-off runs: pages per board and who heard whom. Reads data/<date>/<run>/b*.json and .top.

Board clocks are matched on frames two boards both heard (two-copy builds send each frame twice,
~155 ms apart, so the biggest cluster of differences is used). Delivery runs are cut to the 300 s
after the source started; cut-off runs (source stopped early, board 5 late) use each board's whole run.
heard = distinct sequence numbers received from a sender / span of those numbers (an estimate).

Usage: python3 res/20261113/house.py 2026111301 2026111302 ...
"""
import json, statistics, sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = {m: v['label'] for m, v in json.loads((ROOT / 'data' / 'boards.json').read_text()).items()}
POS = {'1': 'laptop', '2': 'corridor', '3': 'living room', '4': 'cellar', '5': 'hobby room'}
WINDOW = 300000


def offsets(tops):
    first = {o: {} for o in tops}
    for o, t in tops.items():
        for e in t['events']:
            first[o].setdefault((e['sender'], e['seq']), e['ms'])
    pairs = {}
    for a in tops:
        for b in tops:
            if a >= b:
                continue
            d = [first[a][k] - first[b][k] for k in first[a].keys() & first[b].keys()]
            if len(d) < 3:
                continue
            best = max(d, key=lambda x: sum(abs(y - x) <= 20 for y in d))
            cluster = [y for y in d if abs(y - best) <= 20]
            if len(cluster) >= 3:
                pairs[(a, b)] = statistics.median(cluster)  # start(b) - start(a)
    off, todo = {'1': 0.0}, ['1']
    while todo:
        a = todo.pop()
        for (x, y), m in pairs.items():
            if x == a and y not in off:
                off[y] = off[a] + m
                todo.append(y)
            if y == a and x not in off:
                off[x] = off[a] - m
                todo.append(x)
    return off


for run in sys.argv[1:]:
    d = ROOT / 'data' / run[:8] / run
    plan = json.loads((d / 'plan.json').read_text())
    cutoff = 'source-off' in plan.get('experiment', '')
    recs = {str(i): json.loads((d / f'b{i}.json').read_text()) for i in range(1, 6)}
    tops = {str(i): json.loads((d / f'b{i}.top').read_text()) for i in range(1, 6)}
    off = offsets(tops)
    print(f'\n### {run} ({"cut-off test" if cutoff else "delivery run"}, {plan.get("placement")}, '
          f'repeat {plan.get("repeat_ms", 0)} ms, build {recs["1"]["build"]})')
    print('  starts vs source (s):', {k: round(v / 1000, 1) for k, v in sorted(off.items())},
          f'| source ran {recs["1"]["elapsed_ms"] / 1000:.1f} s')
    for o in sorted(tops):
        windowed = not cutoff and o in off
        lo, hi = (-off[o], WINDOW - off[o]) if windowed else (0, float('inf'))
        first, by = {}, defaultdict(list)
        for e in tops[o]['events']:
            if e['type'] == 'page' and e['page'] not in first:
                first[e['page']] = (round((e['ms'] + off.get(o, 0)) / 1000, 1), LAB[e['sender']],
                                    '' if lo <= e['ms'] <= hi else 'after window')
            s = LAB[e['sender']]
            if s != o and lo <= e['ms'] <= hi:
                by[s].append(e)
        r = recs[o]
        print(f'  board {o} ({POS[o]}): pages {r["received"]}; rx {r["rx"]} tx {r["tx"]} dup {r["duplicates"]} '
              f'drop {r["rx_drop"]} bad {r["rx_bad"]}; first copies (s since source start, from board):'
              f'{"" if o in off else " [local clock]"}')
        for p, v in sorted(first.items()):
            print(f'      page {p}: {v[0]:7.1f} s from {v[1]} {v[2]}')
        for s, ev in sorted(by.items()):
            seqs = Counter(e['seq'] for e in ev)
            span = max(seqs) - min(seqs) + 1
            print(f'      heard {s} ({POS[s]}): {len(seqs)}/{span} = {len(seqs) / span:.2f}, '
                  f'pages {len({e["seq"] for e in ev if e["type"] == "page"})}, '
                  f'RSSI {statistics.median(e["rssi"] for e in ev):.0f}'
                  f'{"" if windowed else " (whole run)"}')
