from collections import Counter, defaultdict
from pathlib import Path
from statistics import median
import hashlib
import json
import math
import subprocess


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
IMAGES = ROOT / 'images'
IMAGES.mkdir(exist_ok=True)
LOG = REPO / 'data' / '20261113'
LABELS = {str(v['label']): mac for mac, v in json.loads((REPO / 'data' / 'boards.json').read_text()).items()}
BOARDS = tuple(range(1, 6))
RECEIVERS = tuple(range(2, 6))
PROFILES = {
    'A': ('CODED', '1', '50--100', 1, '9539f99'),
    'B': ('CODED', 'offen', '50--100', 1, 'bleB'),
    'F': ('CODED', 'offen', '50--100', 2, 'bleF'),
    'E': ('CODED', 'offen', 'Standard', 2, 'bleE'),
    'D': ('1M', 'offen', 'Standard', 2, 'bleD'),
}
RUNS = [(1, 'Haus', 'E'), (2, 'Haus', 'A'), (3, 'Haus', 'D'),
        (4, 'Haus, Cachetest', 'E'), (5, 'Haus, Cachetest', 'A'),
        (6, 'Hub', 'A'), (7, 'Hub', 'B'), (8, 'Hub', 'F'), (9, 'Hub', 'E')]
# Relative start estimates from shared sender/run/sequence observations.
# A cache sink has only two anchors per observer pair, with four in total.
OFFSETS = {
    1: {1: 0, 2: -10590, 3: -22615, 4: -33553, 5: -42805},
    2: {1: 0, 2: -9207, 3: -23414, 4: -35850, 5: -46033},
    3: {1: 0, 3: -25668},
    4: {1: 0, 2: -14204, 3: -26139, 4: -37880, 5: 239356},
    5: {1: 0, 2: -12135, 3: -26192, 4: -38902, 5: 258299},
    6: dict(zip(BOARDS, (0, -51, -72, -90, -111))),
    7: dict(zip(BOARDS, (0, -69, -92, -116, -140))),
    8: dict(zip(BOARDS, (0, -51, -76, -99, -119))),
    9: dict(zip(BOARDS, (0, -51, -74, -97, -121))),
}


def seconds(ms, digits=1):
    return f'{ms / 1000:.{digits}f}'.replace('.', ',')


def table(columns, header, rows, size='small'):
    lines = [r'\begin{center}', '\\' + size, r'\begin{tabular}{' + columns + r'}\toprule',
             ' & '.join(header) + r' \\ \midrule']
    lines.extend(' & '.join(map(str, row)) + r' \\' for row in rows)
    lines.extend([r'\bottomrule\end{tabular}', r'\end{center}'])
    return '\n'.join(lines)


def textline(text):
    return r'{\small ' + text + '}'


def load(number, profile):
    run = 2026111300 + number
    folder = LOG / str(run)
    records, traces, hashes = {}, {}, {}
    for i in BOARDS:
        for ext, target in [('json', records), ('top', traces)]:
            path = folder / f'b{i}.{ext}'
            raw = path.read_bytes()
            hashes[str(path)] = hashlib.sha256(raw).hexdigest()
            target[i] = json.loads(raw)
        record, trace = records[i], traces[i]
        assert record['mac'] == trace['mac'] == LABELS[str(i)]
        for key in ('build', 'run', 'payload_bytes', 'elapsed_ms', 'source_pinned', 'repeat_ms'):
            assert record[key] == trace[key], (run, i, key)
        assert record['run'] == run and record['build'].startswith(PROFILES[profile][-1] + ' ')
        assert record['state'] == 'done' and record['storage_ok']
        assert trace['overflow'] == 0 and trace['ignored_run'] == 0
        assert record['rx_drop'] == record['tx_fail'] == 0
        assert all(e['run'] == run and 0 <= e['ms'] <= trace['elapsed_ms'] for e in trace['events'])
    assert len({record['build'] for record in records.values()}) == 1
    assert all(record['total'] == 5 and record['payload_bytes'] == 32 and record['gap_ms'] == 1000 for record in records.values())
    assert all(record['source_pinned'] == (number in (4, 5)) for record in records.values())
    sender = {v: int(k) for k, v in LABELS.items()}
    edges, first = {}, {}
    for i, trace in traces.items():
        first[i] = {}
        for event in sorted(trace['events'], key=lambda e: e['ms']):
            s = sender[event['sender']]
            if s != i:
                edge = edges.setdefault((s, i), {'raw': Counter(), 'unique': {}})
                edge['raw'][event['type']] += 1
                edge['unique'].setdefault(event['seq'], event['type'])
            if event['type'] == 'page':
                first[i].setdefault(event['page'], (event['ms'], s))
        if i in RECEIVERS:
            assert set(first[i]) == set(records[i]['received']), (run, i)
    assert all(set(edge['raw']) <= {'beacon', 'page'} for edge in edges.values())
    return records, traces, edges, first, hashes


def graph(number, edges, hub):
    positions = ({1: (0, 70), 2: (-70, 18), 3: (-44, -64), 4: (44, -64), 5: (70, 18)} if hub else
                 {1: (0, 45), 2: (0, -35), 3: (93, 5), 4: (186, 5), 5: (270, 5)})
    lines = ['digraph G {', 'graph [overlap=true,splines=true,outputorder=edgesfirst,pad=.14];',
             'node [shape=circle,width=.34,height=.34,fixedsize=true,fontname="Helvetica",fontsize=12];',
             'edge [arrowsize=.54,penwidth=1];']
    for i, (x, y) in positions.items():
        lines.append(f'{i} [pos="{x},{y}!"{",penwidth=1.6" if i == 1 else ""}];')
    used = set()
    for (s, r), edge in sorted(edges.items()):
        if (s, r) in used:
            continue
        has_page = 'page' in edge['unique'].values()
        reverse = edges.get((r, s))
        both = reverse is not None and ('page' in reverse['unique'].values()) == has_page
        lines.append(f'{s} -> {r} [dir={"both" if both else "forward"},style={"solid" if has_page else "dashed"}];')
        used.add((s, r))
        if both:
            used.add((r, s))
    dot = IMAGES / f'g{number}.dot'
    dot.write_text('\n'.join(lines + ['}']) + '\n')
    subprocess.run(['neato', '-n2', '-Tpdf', str(dot), '-o', str(IMAGES / f'g{number}.pdf')], check=True)


def plot(number, records, first):
    local = number == 3
    offsets = OFFSETS[number]
    xmax = {1: 300, 2: 300, 3: 420, 4: 540, 5: 560}.get(number, 25)
    ticks = {25: '0,5,10,15,20,25', 300: '0,60,120,180,240,300',
             420: '0,60,120,180,240,300,360,420', 540: '0,100,200,300,400,500',
             560: '0,100,200,300,400,500'}[xmax]
    xlabel = 'Sekunden seit jeweiligem Boardstart' if local else 'Sekunden seit Source-Start (geschätzt)'
    lines = [r'\begin{center}\begin{tikzpicture}\begin{axis}[',
             r'width=16.2cm,height=5.4cm,xmin=0,xmax=' + str(xmax) + r',ymin=-.12,ymax=5.55,',
             r'xtick={' + ticks + r'},ytick={0,1,2,3,4,5},axis lines=left,grid=major,major grid style={gray!17},',
             r'xlabel={' + xlabel + r'},ylabel={Verschiedene Seiten},',
             r'tick label style={font=\footnotesize},label style={font=\footnotesize},',
             r'legend style={draw=none,font=\footnotesize,at={(.5,-.27)},anchor=north,legend columns=4}]']
    for i, style in zip(RECEIVERS, ('ochre,dashdotted', 'navy,solid', 'brick,dashed', 'black,densely dotted')):
        shift = 0 if local else offsets[i]
        start = max(0, shift / 1000)
        end = min(xmax, (shift + records[i]['elapsed_ms']) / 1000)
        times = sorted((ms + shift) / 1000 for ms, _ in first[i].values())
        n = sum(t <= start for t in times)
        points = [(start, n)]
        for t in times:
            if start < t <= end:
                n += 1
                points.append((t, n))
        points.append((end, n))
        coords = ' '.join(f'({t:.3f},{count})' for t, count in points)
        lines.extend([r'\addplot[' + style + r',thick,const plot,no marks] coordinates {' + coords + '};',
                      r'\addlegendentry{Board ' + str(i) + '}'])
    if number in (4, 5):
        stop = records[1]['elapsed_ms'] / 1000
        join = offsets[5] / 1000
        for x, label in [(stop, 'Stopp 1'), (join, 'Start 5')]:
            lines.append(r'\draw[gray,densely dashed] (axis cs:' + str(x) + r',0) -- (axis cs:' + str(x) + r',5.5);')
            lines.append(r'\node[font=\scriptsize,fill=white,inner sep=1pt] at (axis cs:' + str(x) + r',.65) {' + label + '};')
    lines.append(r'\end{axis}\end{tikzpicture}\end{center}')
    return '\n'.join(lines)


def counts_matrix(edges):
    rows = []
    for s in BOARDS:
        row = [s]
        for r in BOARDS:
            counts = Counter(edges.get((s, r), {}).get('unique', {}).values())
            row.append('--' if s == r else f'{counts["beacon"]}/{counts["page"]}')
        rows.append(row)
    return table('c*{5}{r}', ['S / E', '1', '2', '3', '4', '5'], rows, 'footnotesize')


def receipt_table(number, first):
    rows = []
    for page in range(5):
        row = [page]
        for i in RECEIVERS:
            event = first[i].get(page)
            row.append('fehlt' if event is None else seconds(event[0], 3) + f' ({event[1]})')
        rows.append(row)
    rows.append(['5/5 nach', *[seconds(max(v[0] for v in first[i].values()), 3) if len(first[i]) == 5 else '--' for i in RECEIVERS]])
    return table('ccccc', ['Seite', 'Board 2', 'Board 3', 'Board 4', 'Board 5'], rows, 'footnotesize')


RUN_NOTES = {
    1: '4 und 5 empfingen die Seiten 0, 3 und 4. Erste protokollierte Sender von Seite 0 ergeben $1\\to3\\to5\\to4$ um ca. 0,2 s ab Source-Start.',
    2: '4 und 5 hörten Beacons und blieben bei 0/5. Über die Grenze zwischen 1--3 und 4--5 wurden 18 Sendevorgänge abwärts und 13 aufwärts gehört.',
    3: 'Board 3 hatte nach geschätzt 300 s ab Source-Start 2/5 und nach ca. 382 s 5/5. Für 2, 4 und 5 fehlt eine gemeinsame Zeitreferenz. 5 protokollierte keine Frames.',
    4: 'Board 5 war ca. 54,5 s nach dem Source-Stopp aktiv. Erste Kopien aller fünf Seiten von 4. Die letzte kam 243,641 s nach Start von 5.',
    5: 'Board 5 war geschätzt ca. 74,5 s nach dem Source-Stopp aktiv. Die Schätzung beruht auf vier gemeinsamen Frames. 5 hörte 19 Beacons von 4.',
    6: 'Letzter Erstempfang nach ca. 19,3 s ab Source-Start. Seite 1 tauchte bei allen vier Empfängern erst um 19,3 s auf.',
    7: 'Letzter Erstempfang nach ca. 4,6 s ab Source-Start. Alle 20 gerichteten Kontakte wurden beobachtet.',
    8: 'Letzter Erstempfang nach ca. 4,2 s ab Source-Start. Abstand zwischen beobachteten Kopien 99--123 ms.',
    9: 'Letzter Erstempfang nach ca. 4,1 s ab Source-Start. Abstand zwischen beobachteten Kopien 149--160 ms.',
}


def run_page(number, location, profile, records, traces, edges, first):
    hub = location == 'Hub'
    graph(number, edges, hub)
    phy, cap, interval, events, build = PROFILES[profile]
    header = (r'\textbf{' + location + ' ' + profile + r'} \hfill \texttt{' + str(2026111300 + number) + '}' )
    config = (f'Build {build}, {phy}, {events} Event' + ('s' if events != 1 else '') +
              f', Scannerlimit {cap}, Intervall {interval}' + (' ms' if interval != 'Standard' else ''))
    mode = '5 Seiten à 32 Byte, 1 s Abstand'
    if number in (4, 5):
        mode += ', Source mit Pin und Wiederholung alle 5 s, Stopp nach ' + seconds(records[1]['elapsed_ms'], 3) + ' s'
    else:
        mode += ', Normalbetrieb'
    graph_and_matrix = '\n'.join([
        r'\begin{center}\begin{minipage}[c]{.43\linewidth}\centering',
        r'\includegraphics[width=\linewidth,height=3.5cm,keepaspectratio]{images/g' + str(number) + '.pdf}',
        r'\end{minipage}\hfill\begin{minipage}[c]{.53\linewidth}',
        counts_matrix(edges), r'\end{minipage}\end{center}'])
    rows = []
    for i in BOARDS:
        r = records[i]
        ids = ', '.join(map(str, r['received'])) or '--'
        pages = 'Quelle' if i == 1 else f'{len(r["received"])}/5'
        rows.append([i, seconds(r['elapsed_ms']), pages, ids, r['rx'], r['tx'], r['duplicates'], r['rx_bad']])
    status = table('crclrrrr', ['Board', 'Lokal (s)', 'Seiten', 'IDs', 'RX', 'TX', 'Dup.', 'Bad'], rows, 'footnotesize')
    raw = sum(sum(e['raw'].values()) for e in edges.values())
    unique = sum(len(e['unique']) for e in edges.values())
    return '\n\n'.join([
        header, textline(config + r'\par ' + mode), graph_and_matrix,
        textline(r'Gesamte Logs. Pfeil vom Sender zum Empfänger. Durchgezogen bei Seitenframes, gestrichelt bei Beacons allein. Matrixzeile = Sender, Spalte = Empfänger. Zelle = Beacons/Seitenframes nach Sequenz.'),
        status, textline(f'{unique} eindeutige Empfänge, {raw} Roh-Empfänge. RX/TX und Dup. aus der Firmware. Bad = nicht dekodierte Frames. RX-Pufferverluste, TX-Fehler und Überlauf jeweils 0.'),
        plot(number, records, first), receipt_table(number, first),
        textline('Tabelle in Sekunden ab eigenem Boardstart. Unmittelbarer Sender in Klammern.'), textline(RUN_NOTES[number])])


def hub_summary(results):
    rows, pair_data = [], {}
    for number, profile in [(6, 'A'), (7, 'B'), (8, 'F'), (9, 'E')]:
        records, traces, edges, first, _ = results[number]
        ratios = {pair: len(e['unique']) / (max(e['unique']) - min(e['unique']) + 1) for pair, e in edges.items()}
        assert len(ratios) == 20
        pair_data[profile] = ratios
        last = max(ms + OFFSETS[number][i] for i in RECEIVERS for ms, _ in first[i].values())
        rows.append([profile, str(2026111300 + number), f'{median(ratios.values()):.2f}'.replace('.', ','),
                     sum(len(e['unique']) for e in edges.values()), sum(sum(e['raw'].values()) for e in edges.values()), seconds(last)])
    comparisons = []
    for a, b, change in [('A','B','Scannerlimit entfernt'),('B','F','Zweites Event'),('F','E','Standardintervalle')]:
        deltas = [pair_data[b][k] - pair_data[a][k] for k in pair_data[a]]
        comparisons.append([f'{a} $\\to$ {b}', change, f'{sum(x > 0 for x in deltas)}/20', f'{median(deltas):+.2f}'.replace('-0.00','0.00').replace('.', ',')])
    return '\n\n'.join([
        r'\textbf{Vergleich am Hub}',
        table('lrrrrr', ['Profil', 'Run', 'Median', 'Eindeutig', 'Roh', 'Alle 5/5 (s)'], rows),
        textline('Median über 20 gerichtete Kontakte. Je Kontakt Anzahl gehörter Sequenzen geteilt durch den Bereich von der ersten bis zur letzten gehörten Sequenz. Der Wert lässt Verluste ausserhalb dieses Bereichs aus. Zeit bis 5/5 geschätzt ab Source-Start.'),
        table('llrr', ['Paar', 'Änderung', 'Kontakte besser', 'Median der Differenzen'], comparisons),
        r'''\begin{itemize}
\item B gegen F zeigt den grössten beobachteten Unterschied. Das zweite Event erhöhte den Empfangsschätzer an allen 20 Kontakten. Jede Einstellung lief in dieser Reihe einmal.
\item Für Scannerlimit und Standardintervalle zeigte diese Reihe keinen ähnlich klaren Vorteil. A und E lagen bei den früheren Hubmessungen am 7. November in einem ähnlichen Bereich. Die Einzelvergleiche A/B, B/F und F/E stammen vom 13. November.
\item Innerhalb der pro Sender beobachteten Sequenzbereiche gab es bei A 21/334, B 27/321, F 6/323 und E 9/316 Sequenznummern ohne Empfang an irgendeinem der vier anderen Boards. Bei A/B liegt dieser Anteil über der Erwartung eines einfachen Modells mit unabhängigen Empfängern. Die Ursache bleibt offen.
\item TX zählt Sendevorgänge. Bei F und E tragen zwei Funk-Events dieselbe Sequenz. RX zählt beide Empfänge, wenn beide verarbeitet werden. Dup. zählt wiedererkannte Testseiten im Anwendungslog.
\item Am Hub erhielten alle Empfänger 5/5 Seiten. Die Zeiten enthalten das Erzeugen, Weiterleiten und Nachliefern der Seiten. F hatte vier nicht dekodierte Frames. Die anderen drei Hubprofile hatten null.
\end{itemize}''',
        r'\textbf{Profile im Quellcode}',
        table('lcccr', ['Profil','PHY TX/RX','Scannerlimit','Intervall','Events'],
              [[name,p[0],p[1],p[2] + (' ms' if p[2]!='Standard' else ''),p[3]] for name,p in PROFILES.items()]),
        textline(r'Änderungen in \texttt{esp32/io\_dev\_ble.cpp}. A/B/F/E bilden die Hub-Reihe. D und E unterscheiden sich in der PHY. C wurde gebaut und bleibt ohne abgeschlossenen Messlauf in dieser Auswertung.')])


PREAMBLE = r'''\documentclass[a4paper,11pt]{article}
\usepackage[margin=19mm]{geometry}
\usepackage[T1]{fontenc}
\usepackage[utf8]{inputenc}
\usepackage[ngerman]{babel}
\usepackage{lmodern,graphicx,booktabs,pgfplots,enumitem}
\pgfplotsset{compat=1.18}
\definecolor{ochre}{RGB}{155,112,0}
\definecolor{navy}{RGB}{36,92,170}
\definecolor{brick}{RGB}{185,56,50}
\setlength{\parindent}{0pt}
\setlength{\parskip}{3pt}
\setlength{\tabcolsep}{5pt}
\renewcommand{\arraystretch}{1.06}
\setlength{\topsep}{3pt}
\raggedbottom
\setlist[itemize]{leftmargin=*,itemsep=3pt,topsep=4pt}
\begin{document}
'''

HOUSE_SUMMARY = r'''\textbf{Haus, Cache und Einordnung}

\begin{center}\small
\begin{tabular}{llcccc}\toprule
Run & Aufbau & 2 & 3 & 4 & 5 \\ \midrule
2026111301 & Haus E, Normalbetrieb & 5/5 & 5/5 & 3/5 & 3/5 \\
2026111302 & Haus A, Normalbetrieb & 5/5 & 5/5 & 0/5 & 0/5 \\
2026111303 & Haus D, Normalbetrieb & 5/5 & 5/5 & 0/5 & 0/5 \\
2026111304 & Haus E, Source gestoppt & 5/5 & 5/5 & 5/5 & 5/5 \\
2026111305 & Haus A, Source gestoppt & 5/5 & 5/5 & 5/5 & 0/5 \\
\bottomrule\end{tabular}\end{center}

{\small Seiten am Ende der lokalen Logs. In D hatte Board 3 innerhalb der ersten 300 s ab Source-Start 2/5. In E und A ist das gemeinsame 300-s-Fenster aus dem Zeitabgleich rekonstruierbar. Die Normalversuche speicherten 420 s je Board.}

\begin{itemize}
\item E lieferte im Normalbetrieb Seiten 0, 3 und 4 an 4 und 5. D zeigte fünf Beacons von 3 bei 4. Bei 5 wurde mit D kein Frame protokolliert. Die Hausläufe E und D vergleichen die PHY bei einer Platzierung pro Lauf.
\item Beim Cachetest hatten 2--4 mit beiden Builds alle Seiten vor dem Software-Stopp der Source erhalten. Sie liefen während der vollen 300 s von 5. In E erreichten alle fünf Seiten Board 5 über den unmittelbaren Sender 4. In A hörte 5 Beacons von 4 und blieb bei 0/5.
\item Im E-Cachetest hörte 5 von 4 insgesamt 42 eindeutige Sendevorgänge, davon sieben Seitenframes. Die acht rohen Seitenempfänge enthalten eine zweite Kopie. Während A beobachteten andere Boards Seiten von 4. Die Zustellung zu 5 blieb aus.
\item Die Source-Phase enthielt Pinning und zyklische Wiederholung. Fünf Seiten passten in zehn Cacheplätze. Ein eigener Funkversuch mit belegtem Cache und protokolliertem Cachebestand kann die Verdrängung untersuchen. Die bestehende C++-Hostmessung prüft die Verdrängung ohne Funk.
\item Die beiden Cacheläufe hatten unterschiedliche Einstiegspausen. A/E ändert drei BLE-Einstellungen gemeinsam. Eine Wiederholung mit gleichem Ablauf würde den Hausvergleich stärken.
\end{itemize}

\textbf{Zeit und Quellen}

{\small Zeitabgleich aus gemeinsam gehörten Sendersequenzen. Bei zwei Events können Empfänger verschiedene Kopien derselben Sequenz hören. Hubzeiten sind an gemeinsam doppelt gehörten Frames ausgerichtet. Die Cachekurven verwenden geschätzte Starts. Die Ankunftstabellen behalten die lokalen Rohzeiten. Topologie und Matrix zeigen die gesamten Logs.}

\begin{center}\footnotesize
\begin{tabular}{ccll}\toprule
Board & Farbe & MAC & Standort im Haus \\ \midrule
1 & Grün & \texttt{ac276eb0f9b4} & Laptop / Quelle \\
2 & Gelb & \texttt{ac276eb0f9fc} & Korridor \\
3 & Blau & \texttt{ac276eb0059c} & Wohnzimmer \\
4 & Rot & \texttt{ac276eafeb88} & Keller \\
5 & Weiss & \texttt{ac276eb00868} & Hobbyraum \\
\bottomrule\end{tabular}\end{center}

{\small Rohdaten unter \texttt{data/20261113}, ein Ordner pro Lauf. Pro Lauf fünf Ergebnislogs und fünf Topologielogs. Die Firmwarevarianten stehen im Branch \texttt{ble-tests}. Stand 13. November 2026.}
'''


results, pages, hashes = {}, [], {}
for number, location, profile in RUNS:
    results[number] = load(number, profile)
    pages.append(run_page(number, location, profile, *results[number][:4]))
    hashes.update(results[number][4])
pages.extend([hub_summary(results), HOUSE_SUMMARY])
assert len(pages) == 11 and len(hashes) == 90
for path, digest in hashes.items():
    assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest
(ROOT / 'main.tex').write_text(PREAMBLE + '\n\\newpage\n'.join(pages) + '\n\\end{document}\n')
print('Generated nine measured run pages and two summary pages from 90 verified log files.')
