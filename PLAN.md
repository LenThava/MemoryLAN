# Experiment plan

Question: How much content can a late node recover after its source disappears,
and how does competing content change that recovery?

The agreed topic is runtime behaviour and replication efficiency of a small
MemoryLAN, especially cache size and eviction under intermittent connectivity.
Use the five Waveshare ESP32-S3-Zero boards and BLE. The trial remains one source.
Multiple sources, Wi-Fi retrieval, other boards and energy optimisation can wait.

## Evidence as of 21 October 2026

There are three physical setup trials, not three sets of repeated experiments.
The original JSON files are copied unchanged to `data/`, with SHA-256 hashes and
original export names in `data/runs.json`.

| Run | Setting | Observation |
|---|---|---|
| 2026092801 | Two nearby boards | Receiver got 5/5; finished results survived reported unplug/reconnect |
| 2026092901 | Five nearby boards | Every receiver got 2/5; all boards recorded local receive-queue drops |
| 2026092902 | House | Board 2 got 5/5; board 3 stopped at 433 ms; boards 4 and 5 heard each other but got no source pages |

The five-board setup needs validation even at short range. A single house run
does not establish a distance curve, hop count, per-link loss rate or a complete
five-node path. Board 3's short record is flagged in the new report.

```
python3 tool/report.py data/runs.json --out /path/to/new/report/folder
python3 tool/cache.py --out /path/to/cache/results
```

The cache experiment executes the actual C++ core using deterministic hardware
substitutes. It examines 5, 10 and 20 distinct pages in the current 10-page cache,
duplicate refresh, idle time without input and reconstruction of a switch.
It measures resident content, not radio performance or real battery behaviour.

## Working hypotheses

1. A powered relay can repair a late node after the source disappears, provided
   that the needed content is still cached and the nodes can communicate.
2. Distinct new pages create eviction pressure. Waiting without new pages need
   not evict content. Delay and competing traffic must be varied separately.
3. Refreshing one old page can preserve it, while another page is displaced.
   A hot-page workload may therefore favour popular content over cold content.
4. Rebooting a relay and temporarily isolating a powered relay are different:
   the content cache is RAM, whereas measurement files are persistent flash.

These are proposed experiment questions, not claims of new protocol inventions.
The host refresh case reinserts data at one switch. It does not establish that
repeating an application injection at a remote source refreshes a relay: duplicate
history can suppress flooding. A hardware refresh experiment must log actual
arrivals at the relay, not assume that every source injection was broadcast.

## Measurement vocabulary

- Generated: distinct page IDs successfully injected by the source.
- Ever observed: distinct IDs seen by an application during its local window.
- Resident: IDs currently in a switch's finite content cache.
- Recovered later: IDs seen by a previously isolated node after source removal.
- Local drops: frames rejected by a local receive queue, not measured RF loss.

The current hardware record measures generated/ever-observed sets and counters.
It has no first-arrival timeline or resident-at-disconnection snapshot. These
measurements must be added before producing recovery curves or attributing
missing pages to eviction. The host experiment already records resident sets.

## Gates for the next hardware session

1. Export the existing NVS record from each board before new preparation.
   Flash the same trial build to all boards, then verify identities and build IDs.
2. Validate file retention on one board: finish a short run, list logs, retrieve
   by run number, unplug/reconnect and retrieve identical bytes with matching
   length/CRC32. Keep both exports. Initialise LittleFS only explicitly after
   inspecting/exporting the old record; do not format automatically.
3. Repeat nearby two-board and five-board tests with five pages, three real
   repetitions each. Record local windows, queue drops, send failures and firmware.
   Extra BOOT presses must not stop a running node. Investigate nearby underdelivery
   before a range experiment; increasing a queue alone would not prove a repair.
4. Run A -> B, then remove A and introduce C while B remains running. Keep C
   isolated until A is gone. Verify B got all five pages before source removal.
   Give B enough remaining observation time to forward throughout C's window.
   DONE nodes stop forwarding in the current trial mode.
5. Map actual adjacent links (especially 2-3 and 3-4), then repeat the five-board
   house setup. Multiple neighbours are acceptable; record the observed graph.
   Absence of a direct-heard observation is not proof of radio isolation.

Do not start cache-pressure comparisons until these gates are passed. Local
windows are not globally synchronised; keep activation/stop times in experiment
notes and do not infer one-way latency or per-hop causes from final counts.

## Small evaluation after the gates

Use one source, one relay and one late receiver first. Keep firmware, power,
location, payload and spacing fixed. Compare 5/10/20 generated pages against
cache capacity 10; record all page sets and repeat each condition at least three
times. Begin with one fixed interruption, then vary competing new pages.

An optional policy comparison is current refresh-on-receipt versus strict FIFO.
Implement one alternative only after the measurement path is validated. Evaluate
recovery of cold as well as hot pages; a single overall average can hide bias.
For a late receiver, recovery time can use that receiver's own activation clock.
Keep the relay active throughout, remove the source before receiver activation,
and record that sequence. Simultaneous button presses are unnecessary for this
small experiment; a common five-node cutoff remains a separate design question.

## Code map

| File | Role |
|---|---|
| `esp32/memoryLAN.cpp` | Cache, duplicate history, flooding and Bloom-filter repair |
| `esp32/io_dev_ble.cpp` | BLE broadcast and receive queue |
| `esp32/trial_data.h` | Known 32-byte pages, validation, retained record layout |
| `esp32/trial.h` | Preparation, start/stop, counters, NVS and serial commands |
| `esp32/trial_json.h` | Shared JSON representation for USB and files |
| `esp32/trial_log.h` | LittleFS archive, CRC32 and traditional uuencoding |
| `tool/lab.py` | Original USB controls and record export |
| `tool/files.py` | Identify/list/retrieve archived files; verify and decode |
| `tool/report.py` | Reproducible reports from historical raw inputs |
| `test/cache.cpp`, `tool/cache.py` | Deterministic experiment on the actual C++ cache |

Firmware is C++ using Arduino/NimBLE. Laptop tools are Python; zsh `go` is a
local convenience wrapper. It does not implement the radio protocol.

## References

- [MemoryLAN upstream](https://github.com/tschudin/MemoryLAN)
- [Whitepaper](https://github.com/tschudin/MemoryLAN/blob/main/docs/memoryLAN-whitepaper-20260723.pdf)
- [BleBC](https://github.com/tschudin/BleBC)
- [Public fork, current work](https://github.com/LenThava/MemoryLAN/tree/prep)
- [Previous trial changes](https://github.com/LenThava/MemoryLAN/compare/waveshare...prep)

The current software and result prototypes are published on `prep`, with saved
outputs under `res/`. New firmware still needs board validation. Passing host
tests or compiling firmware does not establish BLE delivery, flash power-loss
durability or a working five-node deployment.
