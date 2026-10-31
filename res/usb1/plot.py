from pathlib import Path
import hashlib
import json
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
DATA = ROOT / 'data/usb1'
sys.path.insert(0, str(ROOT / 'tool'))
import topo

run = json.loads((DATA / 'run.json').read_text())
records = {}
paths = []
for node in run['nodes']:
    for info in node['files'].values():
        raw = (DATA / info['file']).read_bytes()
        if hashlib.sha256(raw).hexdigest() != info['sha256']:
            raise ValueError(f'Input changed: {info["file"]}')
    result = json.loads((DATA / node['files']['result']['file']).read_text())
    trace_path = DATA / node['files']['topology']['file']
    trace = topo.validate(json.loads(trace_path.read_text()))
    if (result['mac'] != node['mac'] or trace['mac'] != node['mac'] or
            result['run'] != run['run'] or trace['run'] != run['run'] or
            result['build'] != run['build'] or trace['build'] != run['build']):
        raise ValueError('Mixed identity, run or firmware')
    paths.append(trace_path)
    records[node['label']] = (result, trace)

report = topo.analyse(paths, topo.read_labels(DATA / 'labels.json'))
for item in report['inputs']:
    item['file'] = str(Path(item['file']).relative_to(ROOT))
report['results'] = []
for label, (result, trace) in records.items():
    first = {}
    for event in trace['events']:
        if event['type'] == 'page':
            first.setdefault(event['page'], event['ms'])
    report['results'].append(dict(label=label, mac=result['mac'], sent=result['sent'],
        received=result['received'], rx_drop=result['rx_drop'], rx_bad=result['rx_bad'],
        tx_fail=result['tx_fail'], invalid=result['invalid'],
        first_page_ms={str(page): ms for page, ms in sorted(first.items())},
        completed_local_ms=None if label == '1' else max(first.values(), default=None)))
topo.write_report(report, OUT, paths)

fig, ax = plt.subplots(figsize=(7.0, 4.2))
for label, style in [('3', '-'), ('5', '--')]:
    result, trace = records[label]
    row = next(row for row in report['results'] if row['label'] == label)
    times = sorted(row['first_page_ms'].values())
    ax.step([0, *(t / 1000 for t in times), trace['elapsed_ms'] / 1000],
            [0, *range(1, len(times) + 1), len(times)], where='post',
            color='black', linestyle=style, linewidth=1.5, label=f'Node {label}')
ax.set(xlim=(0, 30), ylim=(-0.05, 5.3), xticks=range(0, 31, 5), yticks=range(6),
       xlabel='Time since receiver start (s)', ylabel='Different pages first received')
ax.spines[['top', 'right']].set_visible(False)
ax.grid(axis='y', color='0.88', linewidth=0.6)
ax.legend(frameon=False, loc='lower right')
fig.text(0.12, 0.965, f'USB test, run {run["run"]}', fontsize=11)
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
fig.text(0.1, 0.03, 'One 30-second USB bench run. Counts are received frames, not loss rates.', fontsize=8)
fig.subplots_adjust(left=0.13, right=0.98, top=0.88, bottom=0.16)
for extension in ('svg', 'png'):
    fig.savefig(OUT / f'links.{extension}', dpi=180, facecolor='white')
plt.close(fig)
print(OUT)
