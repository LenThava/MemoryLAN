from pathlib import Path
import csv
import hashlib
import json
import statistics
import subprocess
import sys

base = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(base / 'tool'))
from lab import validate
from topo import analyse

data = base / 'data/h3'
out = Path(__file__).resolve().parent
manifest = json.loads((data / 'files.json').read_text())
for entry in manifest['files']:
    raw = (data / entry['file']).read_bytes()
    if hashlib.sha256(raw).hexdigest() != entry['sha256']:
        raise ValueError('Changed input: ' + entry['file'])

settings = json.loads((data / 'settings.json').read_text())
labels = settings['boards']
records = [json.loads((data / f'b{i}.json').read_text()) for i in range(1, 6)]
traces = [json.loads((data / f'b{i}.top').read_text()) for i in range(1, 6)]
source = validate(records)
report = analyse([data / f'b{i}.top' for i in range(1, 6)], labels)
if source['run'] != manifest['run'] or source['run'] != settings['run']:
    raise ValueError('Mixed run numbers')
for i, (record, trace) in enumerate(zip(records, traces), 1):
    if labels[record['mac']]['label'] != str(i):
        raise ValueError('Board label does not match filename')
    for key in ('mac', 'node', 'run', 'build', 'elapsed_ms', 'payload_bytes',
                'source_pinned', 'repeat_ms', 'repeat_attempts'):
        if record[key] != trace[key]:
            raise ValueError('Result/topology mismatch: ' + key)

with (out / 'contacts.csv').open('w') as file:
    writer = csv.writer(file)
    writer.writerow(['sender', 'receiver', 'sender_mac', 'receiver_mac', 'beacons', 'page_frames'])
    for edge in report['links']:
        writer.writerow([labels[edge['sender']]['label'], labels[edge['receiver']]['label'],
                         edge['sender'], edge['receiver'], edge['beacons'], edge['pages']])

pages = []
with (out / 'pages.csv').open('w') as file:
    writer = csv.writer(file)
    writer.writerow(['receiver', 'page', 'first_local_ms', 'sender', 'sender_mac'])
    for trace in traces:
        if trace['mac'] == source['mac']:
            continue
        first = {}
        for event in trace['events']:
            if event['type'] == 'page':
                first.setdefault(event['page'], event)
        for page, event in sorted(first.items()):
            row = [labels[trace['mac']]['label'], page, event['ms'],
                   labels[event['sender']]['label'], event['sender']]
            writer.writerow(row)
            pages.append(row)

frames = [{(e['sender'], e['run'], e['seq']): e['ms']
           for e in reversed(t['events'])} for t in traces]
constraints = []
for a in range(5):
    for b in range(a + 1, 5):
        common = frames[a].keys() & frames[b].keys()
        if len(common) >= 3:
            values = [frames[a][key] - frames[b][key] for key in common]
            constraints.append(dict(a=a + 1, b=b + 1, samples=len(common),
                                    start_b_minus_a_ms=statistics.median(values),
                                    spread_ms=max(values) - min(values)))
source_index = next(i for i, r in enumerate(records, 1) if r['mac'] == source['mac'])
offsets = {source_index: 0}
for _ in range(5):
    for edge in constraints:
        a, b, delta = edge['a'], edge['b'], edge['start_b_minus_a_ms']
        if a in offsets:
            offsets.setdefault(b, offsets[a] + delta)
        if b in offsets:
            offsets.setdefault(a, offsets[b] - delta)
timing = dict(method='Approximate start offsets from shared sender/run/sequence frames',
              source_board=source_index, offsets_ms=offsets, constraints=constraints)
if len(offsets) == 5:
    timing['common_window_ms'] = min(offsets[i] + records[i - 1]['elapsed_ms'] for i in offsets) - max(offsets.values())
(out / 'timing.json').write_text(json.dumps(timing, indent=2) + '\n')

rooms = {1: 'Quelle\\nLaptop', 2: 'Korridor', 3: 'Wohnzimmer', 4: 'Keller', 5: 'Hobbyraum'}
dot = ['digraph house {',
       '  graph [layout=neato, start=12, overlap=false, splines=true, sep="+12",',
       '         outputorder=edgesfirst, fontname="Helvetica", fontsize=11, labelloc=b,',
       '         label="Hauslauf 2026110503\\nPfeile: Sender → Empfänger. Gesamter Lauf; Anordnung schematisch."];',
       '  node [shape=circle, fixedsize=true, width=1.25, height=1.25, fontname="Helvetica", fontsize=12];',
       '  edge [arrowsize=0.7, len=1.7];']
for n in range(1, 6):
    text = str(n) + '\\n' + rooms[n]
    dot.append(f'  n{n} [label="{text}"];')
edges = {(e['sender'], e['receiver']): e for e in report['links']}
used = set()
for pair, edge in edges.items():
    if pair in used:
        continue
    a, b = pair
    reverse = (b, a)
    style = 'solid' if edge['pages'] else 'dashed'
    both = reverse in edges and bool(edges[reverse]['pages']) == bool(edge['pages'])
    direction = 'both' if both else 'forward'
    dot.append(f'  n{labels[a]["label"]} -> n{labels[b]["label"]} [dir={direction}, style={style}];')
    used.add(pair)
    if both:
        used.add(reverse)
dot.append('}')
(out / 'graph.dot').write_text('\n'.join(dot) + '\n')
subprocess.run(['neato', '-Tsvg', str(out / 'graph.dot'), '-o', str(out / 'graph.svg')], check=True)
print('Run', source['run'], ':', len(report['links']), 'directed contacts;', len(pages), 'first page arrivals')
