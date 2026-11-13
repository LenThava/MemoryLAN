#!/usr/bin/env python3
"""Hub runs: share of each sender's transmissions heard, per directed link, and pairwise comparisons.
All boards start over USB within 0.2 s, so each board's whole run is used.
heard = distinct sequence numbers received / span of those numbers (an estimate);
frames = all receptions per transmission (two-copy builds can reach 2).

Usage: python3 res/20261113/hub.py A=2026111306 B=2026111307 F=2026111308 E=2026111309
"""
import json, statistics, sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = {m: v['label'] for m, v in json.loads((ROOT / 'data' / 'boards.json').read_text()).items()}


def load(run):
    d = ROOT / 'data' / run[:8] / run
    recs = {str(i): json.loads((d / f'b{i}.json').read_text()) for i in range(1, 6)}
    tops = {str(i): json.loads((d / f'b{i}.top').read_text()) for i in range(1, 6)}
    links = {}
    for o, t in tops.items():
        by = defaultdict(list)
        for e in t['events']:
            if LAB[e['sender']] != o:
                by[LAB[e['sender']]].append(e)
        for s, ev in by.items():
            seqs = Counter(e['seq'] for e in ev)
            span = max(seqs) - min(seqs) + 1
            links[(s, o)] = (len(seqs) / span, len(ev) / span)
    return recs, tops, links


res, names = {}, []
for arg in sys.argv[1:]:
    name, run = arg.split('=')
    recs, tops, links = load(run)
    res[name] = links
    names.append(name)
    heard = [v[0] for v in links.values()]
    frames = [v[1] for v in links.values()]
    done = []
    for o in '2345':
        first = {}
        for e in tops[o]['events']:
            if e['type'] == 'page':
                first.setdefault(e['page'], e['ms'])
        done.append(round(max(first.values()) / 1000, 1) if len(first) == 5 else None)
    tot = {k: sum(r[k] for r in recs.values()) for k in ('rx', 'tx', 'duplicates', 'rx_drop', 'rx_bad', 'tx_fail')}
    print(f'{name:9s} {run} {recs["1"]["build"]:26s} heard median {statistics.median(heard):.2f} '
          f'({min(heard):.2f}-{max(heard):.2f}); frames per transmission {statistics.median(frames):.2f}; '
          f'5/5 at boards 2-5: {done} s; {tot}')
print()
for i, a in enumerate(names):
    for b in names[i + 1:]:
        diff = [res[b][k][0] - res[a][k][0] for k in res[a].keys() & res[b].keys()]
        print(f'{b} vs {a}: {sum(x > 0 for x in diff)} of {len(diff)} links better, median change {statistics.median(diff):+.2f}')
