from pathlib import Path
import csv
import hashlib
import json
import sys
import zlib

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
DATA = ROOT / 'data/size'
sys.path.insert(0, str(ROOT / 'tool'))
import lab
import topo


def read_run(name):
    directory = DATA / name
    manifest = json.loads((directory / 'run.json').read_text())
    labels = topo.read_labels(directory / 'labels.json')
    records, paths, rows = [], [], []
    if len(manifest['nodes']) != 5:
        raise ValueError('Expected five board records')
    for node in manifest['nodes']:
        if labels.get(node['mac'], {}).get('label') != node['label']:
            raise ValueError('Board label differs from placement map')
        for info in node['files'].values():
            raw = (directory / info['file']).read_bytes()
            if (len(raw) != info['bytes'] or
                    hashlib.sha256(raw).hexdigest() != info['sha256'] or
                    f'{zlib.crc32(raw):08x}' != info['crc32']):
                raise ValueError(f'Input changed: {name}/{info["file"]}')
        result = json.loads((directory / node['files']['result']['file']).read_text())
        path = directory / node['files']['topology']['file']
        trace = topo.validate(json.loads(path.read_text()))
        size = lab.payload_bytes(result)
        if (result['mac'] != node['mac'] or trace['mac'] != node['mac'] or
                result['run'] != manifest['run'] or trace['run'] != manifest['run'] or
                result['build'] != manifest['build'] or trace['build'] != manifest['build'] or
                lab.payload_bytes(trace) != size or size != manifest['payload_bytes']):
            raise ValueError('Mixed board, run, firmware or page size')
        records.append(result)
        paths.append(path)
        first, senders = {}, {}
        for event in trace['events']:
            if event['type'] == 'page' and event['page'] not in first:
                first[event['page']] = event['ms']
                senders[event['page']] = event['sender']
        expected = set(range(result['total']))
        if not set(first) <= expected or not set(first) <= set(result['received']):
            raise ValueError('Trace pages differ from received-page summary')
        rows.append(dict(label=node['label'], mac=node['mac'], received=result['received'],
            count=len(result['received']), sent=result['sent'],
            completed_local_ms=max(first.values()) if result['node'] != result['source'] and set(first) == expected else None,
            first_page_ms={str(k): v for k, v in sorted(first.items())},
            first_page_sender={str(k): v for k, v in sorted(senders.items())},
            rx=result['rx'], tx=result['tx'], duplicates=result['duplicates'],
            rx_drop=result['rx_drop'], rx_bad=result['rx_bad'], tx_fail=result['tx_fail'],
            invalid=result['invalid'], overflow=trace['overflow'], ignored_run=trace['ignored_run']))
    lab.validate(records)
    topology = topo.analyse(paths, labels)
    for item in topology['inputs']:
        item['file'] = str(Path(item['file']).relative_to(ROOT))
    return dict(name=name, run=manifest['run'], build=manifest['build'],
                firmware_revision=manifest['firmware_revision'], payload_bytes=manifest['payload_bytes'],
                total=manifest['total'], gap_ms=manifest['gap_ms'],
                local_limit_ms=manifest['local_limit_ms'], activation=manifest['activation'],
                retention=manifest['retention'], results=rows, topology=topology)


manifest = json.loads((DATA / 'runs.json').read_text())
runs = [read_run(name) for name in manifest['runs']]
base = read_run(manifest['baseline']) if manifest.get('baseline') else None
if len({r['run'] for r in runs}) != len(runs):
    raise ValueError('Repeated run ID')
if len({r['build'] for r in runs}) != 1:
    raise ValueError('Size comparison must use the same firmware build')
if len({(r['total'], r['gap_ms'], r['local_limit_ms']) for r in runs}) != 1:
    raise ValueError('Size comparison changes another trial setting')
if set(r['payload_bytes'] for r in runs) != {32, 64, 120}:
    raise ValueError('Expected all three page sizes')
if any(sum(r['payload_bytes'] == size for r in runs) != 2 for size in (32, 64, 120)):
    raise ValueError('Expected two distinct physical runs per size')
summary = dict(schema=1, kind='mlan-size-comparison', setting=manifest['setting'],
    clock='Receiver-local milliseconds since activation; not synchronised one-way latency.',
    runs=runs, baseline=base)
(OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')

with (OUT / 'runs.csv').open('w', newline='') as stream:
    writer = csv.writer(stream, lineterminator='\n')
    writer.writerow(['run', 'page_bytes', 'pages_injected', 'directed_links', 'receiver_pages', 'trace_overflow'])
    for run in runs:
        writer.writerow([run['run'], run['payload_bytes'], run['total'], len(run['topology']['links']),
            ';'.join(f'{r["label"]}:{r["count"]}/{run["total"]}' for r in run['results'] if r['label'] != '1'),
            sum(r['overflow'] for r in run['results'])])
with (OUT / 'receivers.csv').open('w', newline='') as stream:
    writer = csv.writer(stream, lineterminator='\n')
    writer.writerow(['run', 'page_bytes', 'receiver', 'mac', 'received', 'planned', 'completed_local_ms',
                     'duplicates', 'rx', 'tx', 'rx_drop', 'rx_bad', 'tx_fail', 'invalid', 'overflow'])
    for run in runs:
        for row in run['results']:
            if row['label'] != '1':
                writer.writerow([run['run'], run['payload_bytes'], row['label'], row['mac'], row['count'],
                    run['total'], row['completed_local_ms'], *(row[k] for k in ('duplicates', 'rx', 'tx', 'rx_drop', 'rx_bad', 'tx_fail', 'invalid', 'overflow'))])

colors = {'2': '#a97700', '3': '#245caa', '4': '#b93832', '5': '#555555'}
markers = {'2': '^', '3': 'o', '4': 's', '5': 'D'}
fig, ax = plt.subplots(figsize=(7, 4.4))
size_index = {size: index for index, size in enumerate((32, 64, 120))}
repetitions = {size: 0 for size in size_index}
maximum = 0
for run in runs:
    size = run['payload_bytes']
    repeat = repetitions[size]
    repetitions[size] += 1
    for row in run['results']:
        label = row['label']
        if label == '1':
            continue
        x = size_index[size] + (int(label) - 3.5) * 0.075 + (repeat - 0.5) * 0.028
        value = row['completed_local_ms']
        if value is None:
            y = run['local_limit_ms'] / 1000
            ax.scatter(x, y, color=colors[label], marker='x', s=45)
            ax.annotate(f'{row["count"]}/{run["total"]}', (x, y), xytext=(0, 7),
                        textcoords='offset points', ha='center', fontsize=7)
        else:
            y = value / 1000
            ax.scatter(x, y, facecolors=colors[label] if repeat == 0 else 'white',
                       edgecolors=colors[label], marker=markers[label], s=44, linewidths=1.2)
        maximum = max(maximum, y)
for label in colors:
    ax.scatter([], [], color=colors[label], marker=markers[label], s=35, label=f'Node {label}')
ax.set(xlim=(-0.35, 2.35), ylim=(0, maximum * 1.2 + 0.3),
       xticks=(0, 1, 2), xticklabels=('32', '64', '120'),
       xlabel='MemoryLAN page size (bytes)', ylabel='All five pages received at local time (s)')
ax.spines[['top', 'right']].set_visible(False)
ax.grid(axis='y', color='0.88', linewidth=0.6)
ax.set_axisbelow(True)
ax.legend(frameon=False, ncols=4, loc='upper center', fontsize=9)
fig.text(0.12, 0.965, 'Five-board room test: page-size comparison', fontsize=11)
fig.text(0.12, 0.025, 'Five pages per run; filled/hollow points show the two runs per size. Receiver clocks are local.', fontsize=8)
fig.subplots_adjust(left=0.12, right=0.98, top=0.89, bottom=0.18)
for extension in ('svg', 'png'):
    fig.savefig(OUT / f'completion.{extension}', dpi=180, facecolor='white')
plt.close(fig)
print(OUT)
