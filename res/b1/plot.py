from pathlib import Path
import csv
from datetime import datetime
import hashlib
import json
import sys
import zlib

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyArrowPatch

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
DATA = ROOT / 'data/b1'
sys.path.insert(0, str(ROOT / 'tool'))
import lab
import topo

run = json.loads((DATA / 'run.json').read_text())
labels = topo.read_labels(DATA / 'labels.json')
records, paths = {}, []
for node in run['nodes']:
    for info in node['files'].values():
        raw = (DATA / info['file']).read_bytes()
        if (len(raw) != info['bytes'] or hashlib.sha256(raw).hexdigest() != info['sha256'] or
                f'{zlib.crc32(raw):08x}' != info['crc32']):
            raise ValueError(f'Input changed: {info["file"]}')
    result = json.loads((DATA / node['files']['result']['file']).read_text())
    path = DATA / node['files']['topology']['file']
    trace = topo.validate(json.loads(path.read_text()))
    if (result['mac'] != node['mac'] or trace['mac'] != node['mac'] or
            result['run'] != run['run'] or trace['run'] != run['run'] or
            result['build'] != run['build'] or trace['build'] != run['build'] or
            lab.payload_bytes(result) != run['payload_bytes'] or
            lab.payload_bytes(trace) != run['payload_bytes']):
        raise ValueError('Mixed board, run, size or firmware')
    records[node['label']] = (result, trace)
    paths.append(path)
lab.validate([r for r, _ in records.values()])
for info in run['proof_files']:
    raw = (DATA / info['file']).read_bytes()
    if len(raw) != info['bytes'] or hashlib.sha256(raw).hexdigest() != info['sha256']:
        raise ValueError('Changed control evidence')
seed = json.loads((DATA / run['seeding_proof']).read_text())
cut = json.loads((DATA / run['physical_cut_proof']).read_text())
expected = set(range(run['total']))
by_label = {n['label']: n['mac'] for n in run['nodes']}
for label in ('2', '3', '4'):
    for witness in (seed['records'][label], cut['relays'][label]):
        if (witness['mac'] != by_label[label] or witness['run'] != run['run'] or
                witness['build'] != run['build'] or witness['state'] != 'running'):
            raise ValueError('Mixed relay witness')
    if (seed['records'][label]['state'] != 'running' or
            set(seed['records'][label]['received']) != expected or
            cut['relays'][label]['state'] != 'running'):
        raise ValueError('Relays were not verified seeded and active')
if (seed['sink5']['state'] != 'ready' or seed['sink5']['received'] or
        cut['sink5_before_start']['state'] != 'ready' or cut['sink5_before_start']['received'] or
        cut['source_usb_absent'] is not True):
    raise ValueError('Missing delayed-start/source-removal gate')

source_stop = next(c for c in run['stopping'] if c['label'] == '1')
sink_start = next(c for c in run['activation'] if c['label'] == '5')
if not (datetime.fromisoformat(seed['utc']) <= datetime.fromisoformat(source_stop['request_utc']) <
        datetime.fromisoformat(source_stop['ack_utc']) < datetime.fromisoformat(cut['checked_utc']) <
        datetime.fromisoformat(sink_start['request_utc'])):
    raise ValueError('Invalid source-cut/sink-start order')

report = topo.analyse(paths, labels)
for item in report['inputs']:
    item['file'] = str(Path(item['file']).relative_to(ROOT))
report['results'] = []
for label, (result, trace) in records.items():
    first, senders = {}, {}
    for event in trace['events']:
        if event['type'] == 'page' and event['page'] not in first:
            first[event['page']] = event['ms']
            senders[event['page']] = event['sender']
    report['results'].append(dict(label=label, mac=result['mac'], sent=result['sent'],
        received=result['received'], missing=sorted(expected - set(result['received'])),
        elapsed_ms=result['elapsed_ms'], stop_reason=result['stop_reason'],
        first_page_ms={str(k): v for k, v in sorted(first.items())},
        first_page_sender={str(k): v for k, v in sorted(senders.items())},
        rx=result['rx'], tx=result['tx'], duplicates=result['duplicates'], invalid=result['invalid'],
        rx_drop=result['rx_drop'], rx_bad=result['rx_bad'], tx_fail=result['tx_fail'],
        overflow=trace['overflow'], ignored_run=trace['ignored_run']))
sink, trace = records['5']
source_frames = sum(e['sender'] == run['source_mac'] for e in trace['events'])
row = next(r for r in report['results'] if r['label'] == '5')
report['cache_check'] = dict(relays_seeded_before_cut=['2', '3', '4'], sink_ready_before_cut=True,
    source_usb_absent_before_sink_start=True, source_frames_at_sink=source_frames,
    sink_received=sink['received'], sink_missing=row['missing'],
    conclusion='Partial cached-copy delivery observed after source removal.' if sink['received'] and not source_frames else 'Inspect evidence.',
    scope='One nearby bench run; no full-delivery, house-range or forced multi-hop claim.')
topo.write_report(report, OUT, paths)

with (OUT / 'pages.csv').open('w', newline='') as stream:
    writer = csv.writer(stream, lineterminator='\n')
    writer.writerow(['page', 'received_by_5', 'first_local_ms', 'immediate_sender_label', 'sender_mac'])
    for page in range(run['total']):
        key = str(page)
        sender = row['first_page_sender'].get(key)
        writer.writerow([page, page in sink['received'], row['first_page_ms'].get(key, ''),
                         labels[sender]['label'] if sender else '', sender or ''])

arrivals = sorted((ms, int(page)) for page, ms in row['first_page_ms'].items())
fig, ax = plt.subplots(figsize=(7.0, 4.2))
ax.step([0, *(ms / 1000 for ms, _ in arrivals), sink['elapsed_ms'] / 1000],
        [0, *range(1, len(arrivals) + 1), len(arrivals)], where='post', color='black', linewidth=1.5)
for count, (ms, page) in enumerate(arrivals, 1):
    ax.annotate(f'Page {page}', (ms / 1000, count), xytext=(5, 7),
                textcoords='offset points', fontsize=8)
ax.set(xlim=(0, 95), ylim=(-0.05, 5.3), xticks=range(0, 91, 15), yticks=range(6),
       xlabel='Time since receiver 5 started (s)', ylabel='Different pages received by 5')
ax.spines[['top', 'right']].set_visible(False)
ax.grid(axis='y', color='0.88', linewidth=0.6)
fig.text(0.12, 0.965, f'Cache check, run {run["run"]}: receiver 5', fontsize=11)
fig.text(0.12, 0.025, 'Source 1 was already removed. Page 4 was still missing at the intentional stop.', fontsize=8)
fig.subplots_adjust(left=0.12, right=0.98, top=0.90, bottom=0.18)
for ext in ('svg', 'png'):
    fig.savefig(OUT / f'arrivals.{ext}', dpi=180, facecolor='white')
plt.close(fig)

positions = {'2': (0, 2), '3': (0, 1), '4': (0, 0), '5': (3.2, 1)}
fills = {'2': '#f2df83', '3': '#a8bfdf', '4': '#e5aaa6', '5': 'white'}
by_sender = {}
for page, mac in row['first_page_sender'].items():
    by_sender.setdefault(labels[mac]['label'], []).append(int(page))
fig, ax = plt.subplots(figsize=(6.4, 3.7))
for sender, pages in sorted(by_sender.items()):
    a, b = positions[sender], positions['5']
    ax.add_patch(FancyArrowPatch(a, b, arrowstyle='-|>', mutation_scale=13,
        shrinkA=17, shrinkB=17, color='#444444', linewidth=1.3))
    text = ('Page ' if len(pages) == 1 else 'Pages ') + ', '.join(map(str, sorted(pages)))
    ax.text((a[0] + b[0]) / 2, (a[1] + b[1]) / 2 + (0.12 if sender != '4' else -0.17),
            text, ha='center', fontsize=10, bbox=dict(facecolor='white', edgecolor='none', pad=1))
for label, pos in positions.items():
    ax.add_patch(Circle(pos, 0.2, facecolor=fills[label], edgecolor='#222222', zorder=3))
    ax.text(*pos, label, ha='center', va='center', fontsize=12, zorder=4)
ax.text(-0.45, 2.7, 'Source 1: off before receiver 5 started', fontsize=10)
ax.set(xlim=(-0.55, 3.7), ylim=(-0.45, 3.0))
ax.set_aspect('equal'); ax.axis('off')
fig.text(0.05, 0.95, 'Immediate senders of the first received copies', fontsize=11)
fig.text(0.05, 0.03, 'These arrows identify recorded receptions, not a complete multi-hop route.', fontsize=8)
fig.subplots_adjust(left=0.05, right=0.98, top=0.87, bottom=0.1)
for ext in ('svg', 'png'):
    fig.savefig(OUT / f'copies.{ext}', dpi=180, facecolor='white')
plt.close(fig)
print(OUT)
