from pathlib import Path
import hashlib
import json
import sys
import zlib
import math

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, Circle

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
DATA = ROOT / 'data/h1'
sys.path.insert(0, str(ROOT / 'tool'))
import topo
import lab

run = json.loads((DATA / 'run.json').read_text())
records = {}
paths = []
for node in run['nodes']:
    for info in node['files'].values():
        raw = (DATA / info['file']).read_bytes()
        if (len(raw) != info['bytes'] or hashlib.sha256(raw).hexdigest() != info['sha256'] or
                f'{zlib.crc32(raw):08x}' != info['crc32']):
            raise ValueError(f'Input changed: {info["file"]}')
    result = json.loads((DATA / node['files']['result']['file']).read_text())
    trace_path = DATA / node['files']['topology']['file']
    trace = topo.validate(json.loads(trace_path.read_text()))
    if (result['mac'] != node['mac'] or trace['mac'] != node['mac'] or
            result['run'] != run['run'] or trace['run'] != run['run'] or
            result['build'] != run['build'] or trace['build'] != run['build'] or
            lab.payload_bytes(result) != run['payload_bytes'] or
            lab.payload_bytes(trace) != run['payload_bytes']):
        raise ValueError('Mixed identity, run or firmware')
    paths.append(trace_path)
    records[node['label']] = (result, trace)

lab.validate([result for result, _ in records.values()])
report = topo.analyse(paths, topo.read_labels(DATA / 'labels.json'))
for item in report['inputs']:
    item['file'] = str(Path(item['file']).relative_to(ROOT))
report['results'] = []
for label, (result, trace) in records.items():
    first = {}
    senders = {}
    for event in trace['events']:
        if event['type'] == 'page':
            if event['page'] not in first:
                first[event['page']] = event['ms']
                senders[event['page']] = event['sender']
    report['results'].append(dict(label=label, mac=result['mac'], sent=result['sent'],
        received=result['received'], missing=sorted(set(range(run['total'])) - set(result['received'])),
        elapsed_ms=result['elapsed_ms'], stop_reason=result['stop_reason'],
        rx=result['rx'], tx=result['tx'], duplicates=result['duplicates'],
        rx_drop=result['rx_drop'], rx_bad=result['rx_bad'],
        tx_fail=result['tx_fail'], invalid=result['invalid'],
        first_page_ms={str(page): ms for page, ms in sorted(first.items())},
        first_page_sender={str(page): mac for page, mac in sorted(senders.items())},
        completed_local_ms=max(first.values()) if label != '1' and set(first) == set(range(run['total'])) else None))
topo.write_report(report, OUT, paths)

fig, ax = plt.subplots(figsize=(7.0, 4.2))
for label, style, color in [('2', '-.', '#a97700'), ('3', '-', '#245caa'),
                            ('4', '--', '#b93832'), ('5', ':', '#555555')]:
    result, trace = records[label]
    row = next(row for row in report['results'] if row['label'] == label)
    times = sorted(row['first_page_ms'].values())
    ax.step([0, *(t / 1000 for t in times), trace['elapsed_ms'] / 1000],
            [0, *range(1, len(times) + 1), len(times)], where='post',
            color=color, linestyle=style, linewidth=1.5,
            label=f'Node {label}: {len(times)}/{run["total"]}')
ax.set(xlim=(0, 240), ylim=(-0.05, 5.3), xticks=range(0, 241, 60), yticks=range(6),
       xlabel='Time since receiver start (s)', ylabel='Different pages first received')
ax.spines[['top', 'right']].set_visible(False)
ax.grid(axis='y', color='0.88', linewidth=0.6)
ax.legend(frameon=False, loc='lower right')
fig.text(0.12, 0.965, f'House test, run {run["run"]}', fontsize=11)
fig.text(0.12, 0.025, 'Recorded receptions; source remained on. Each receiver uses its own local clock.', fontsize=8)
fig.subplots_adjust(left=0.12, right=0.98, top=0.90, bottom=0.18)
for extension in ('svg', 'png'):
    fig.savefig(OUT / f'arrivals.{extension}', dpi=180, facecolor='white')
plt.close(fig)

nodes = report['nodes']
counts = {(link['sender'], link['receiver']): link['observations'] for link in report['links']}
matrix = [[float('nan') if sender['mac'] == receiver['mac'] else
           counts.get((sender['mac'], receiver['mac']), 0)
           for sender in nodes] for receiver in nodes]
fig, ax = plt.subplots(figsize=(5.5, 4.8))
ax.imshow(matrix, cmap='Greys', vmin=0, vmax=max(counts.values()))
for row in range(len(nodes)):
    for col in range(len(nodes)):
        value = matrix[row][col]
        text = '-' if row == col else str(value)
        ax.text(col, row, text, ha='center', va='center', fontsize=12,
                color='white' if value > max(counts.values()) / 2 else 'black')
names = [node['label'] for node in nodes]
ax.set(xticks=range(len(nodes)), yticks=range(len(nodes)), xticklabels=names,
       yticklabels=names, xlabel='Sender (node)', ylabel='Receiver (node)')
ax.set_title(f'Recorded receptions, run {run["run"]}', fontsize=11, pad=12)
fig.text(0.1, 0.03, 'One house run; local 240-second windows. Counts are received frames, not loss rates.', fontsize=8)
fig.subplots_adjust(left=0.13, right=0.98, top=0.88, bottom=0.16)
for extension in ('svg', 'png'):
    fig.savefig(OUT / f'links.{extension}', dpi=180, facecolor='white')
plt.close(fig)

positions = {'1': (0, 0), '3': (2, 0), '4': (1, 1.8), '2': (4, 0), '5': (6, 0)}
labels = {node['mac']: node['label'] for node in nodes}
fills = {'1': '#a9d7ac', '2': '#f2df83', '3': '#a8bfdf', '4': '#e5aaa6', '5': 'white'}
fig, ax = plt.subplots(figsize=(8.0, 4.0))
for edge in report['links']:
    a, b = labels[edge['sender']], labels[edge['receiver']]
    start, end = positions[a], positions[b]
    rad = -0.35 if (a, b) == ('3', '5') else 0.15
    ax.add_patch(FancyArrowPatch(start, end, connectionstyle=f'arc3,rad={rad}',
        arrowstyle='-|>', mutation_scale=12, shrinkA=19, shrinkB=19,
        linewidth=1.1, color='#444444', zorder=1))
    dx, dy = end[0] - start[0], end[1] - start[1]
    length = math.hypot(dx, dy)
    middle = ((start[0] + end[0]) / 2, (start[1] + end[1]) / 2)
    offset = 0.5 * rad * length + (0.12 if rad > 0 else -0.12)
    x, y = middle[0] + dy / length * offset, middle[1] - dx / length * offset
    ax.text(x, y, str(edge['observations']), ha='center', va='center', fontsize=9,
            bbox=dict(facecolor='white', edgecolor='none', pad=0.8), zorder=2)
for label, pos in positions.items():
    ax.add_patch(Circle(pos, radius=0.23, facecolor=fills[label], edgecolor='#222222', zorder=3))
    ax.text(*pos, label, ha='center', va='center', fontsize=12, zorder=4)
    if label == '1':
        ax.text(pos[0], pos[1] - 0.45, 'Source', ha='center', fontsize=9)
ax.set(xlim=(-0.6, 6.6), ylim=(-0.8, 2.4))
ax.set_aspect('equal'); ax.axis('off')
fig.text(0.05, 0.95, f'Recorded direct receptions, run {run["run"]}', fontsize=11)
fig.text(0.05, 0.06, 'Arrow: sender to receiver; numbers count received frames, including beacons.', fontsize=8)
fig.text(0.05, 0.02, 'Logical layout, not physical distances. Missing arrows are not proof of no radio path.', fontsize=8)
fig.subplots_adjust(left=0.03, right=0.99, top=0.86, bottom=0.14)
for extension in ('svg', 'png'):
    fig.savefig(OUT / f'graph.{extension}', dpi=180, facecolor='white')
plt.close(fig)
print(OUT)
