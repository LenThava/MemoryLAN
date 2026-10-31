# Trial preparation

Optional firmware for one source and up to 256 numbered 32-byte pages.
Trial controls are enabled only when `MLAN_TRIAL` is defined.
The core correctness fixes also apply to the ordinary MemoryLAN build.
No trial starts automatically after boot.
Trial builds suppress routine radio/core debug output; USB commands and JSON
results remain enabled. Ordinary builds keep their debug output.

Assign each Waveshare board once with `./go ctrl PORT label N` (1 through 5).
The number survives a restart and `clear`: 1 green, 2 yellow, 3 blue, 4 red,
5 white. `id` reports the stored label alongside the full MAC.
Slow blinking means prepared; fast blinking means running. A steady identity
colour means finished with both result and topology archives saved. Magenta
blinking means interrupted, storage error or an unavailable archive: keep power
and inspect `dump`. Colour does not indicate successful page delivery.
Unlabelled boards retain the old amber/blue/green state colours.

## From a checkout

Firmware builds have been checked with Arduino CLI 1.5.1 and the ESP32 Arduino
core 3.3.12. Host tests require `clang++` and Python 3. USB tooling currently
accepts macOS USB-modem port names and uses Python's standard library.

From the repository root (these commands do not flash or start a trial):

```
sh test/run.sh
make -C esp32 BOARD=WaveshareS3Zero compile EXTRA_FLAGS=-DMLAN_TRIAL
python3 tool/lab.py --help
```

After identifying the exact board port, upload that build to one board:

```
make -C esp32 BOARD=WaveshareS3Zero PORT=/dev/cu.usbmodemXXXX upload
python3 tool/lab.py ctl /dev/cu.usbmodemXXXX dump
python3 tool/lab.py dump /dev/cu.usbmodemXXXX --out log
```

Replace `XXXX` with the real port. `ctl ... dump` prints the result; `dump ...`
saves a new JSON file. Do not run plain `make`: its default includes uploading.

## Local shortcuts

On the prepared Mac, the unversioned `~/mlan/go` wrapper supplies local paths.
It is not required when using the commands above. From `~/mlan`:

```
./go test
./go lab
./go port
./go load /dev/cu.usbmodemXXXX
./go ctrl /dev/cu.usbmodemXXXX dump
```

`load` flashes one Waveshare ESP32-S3-Zero. Identify the board first.
The `mac` field is the full default six-byte hardware address. The four-digit
`node` field remains a legacy alias used by the existing trial source selector.
Label physical boards 1 through 5 and record the actual full addresses and
locations in `data/boards.json`. It contains the five verified addresses and
intended house locations. Do not infer an address from an old short ID. Its
format is a JSON object keyed by twelve lowercase MAC hex digits, with `label`
and `location` fields for each board. Save the actual placement with each run.

Use the same firmware binary on all five boards. The new BLE envelope carries
the current transmitter's full MAC, run ID and transmission sequence outside
the unchanged MemoryLAN page. Old and new radio firmware must not be mixed.
Keep other test firmware and BleBC demos off during a run.

## First bench run

Choose a fresh run number shared by every board in that physical run, and the
source's four-digit hexadecimal legacy node ID. Give repetitions new run IDs.
Prepare each board, including the source (example source ID `1234`):

```
./go ctrl PORT prep 1 1234 5 1000 120
```

Arguments: run, source, page count, configured minimum spacing in milliseconds,
LOCAL target duration in seconds. Sending is synchronous, so spacing and elapsed
duration can overrun; this is not an exact-rate generator or a hard deadline.
Preparation survives a power cycle.
An existing record must be exported and explicitly cleared before preparation.

1. Place every board on its final power supply while it is still READY. After
   all boards are positioned, visit the receivers and hold BOOT for about half
   a second to start each one, then release. Choose a local duration long enough
   to leave a shared observation window after this walk. Note the activation
   order; simultaneous button presses are not required. Extra presses cannot
   stop a running trial.
2. Start the laptop-connected source last: `./go ctrl PORT start`.
3. The source injects the configured pages once. Cache repair continues.
4. Let each local timer expire to finish and save in place. Wait for the steady
   identity colour before unplugging. Magenta means keep power and inspect `dump`.
   Keep boards at their positions until all have finished. Do not move active
   boards together for collection.
5. Connect each board to USB, identify it and retrieve both archived files with
   the commands below. Reboot never resumes a finished/interrupted run.
   Use `./go dump PORT` as the NVS fallback if the result archive is unavailable.
6. Plot topology files first. For the existing page-count plots, include the
   source result: `./go plot log/SOURCE.json log/B.json log/C.json`.
7. After checking exports, `./go ctrl PORT clear` removes only this trial record.

`./go ctrl PORT stop` can finish early over USB. BOOT only starts a prepared run.
Use `dump` to check state; do not retry `start` blindly after a timeout.
Use new run numbers for repetitions. `./go log` still shows the old smoke log,
not a new experiment result.

## Interpretation and limits

- DONE stores one result blob in NVS, checked by read-back. A reboot during
  RUNNING becomes INTERRUPTED. There are no periodic data checkpoints; an
  interrupted run loses its latest RAM observations and is excluded from plots.
- Start/final writes are outside normal packet collection. No per-packet flash
  writes, persistent content cache or extra application retransmissions are added.
  Sender metadata is carried outside the cached content, so forwarding does not
  change the page's identity.
- The current controls use **independent local observation windows**. These are
  not a synchronised global cutoff. Record activation/finish order and durations.
  Do not claim equal recovery time, one-way latency or causal per-hop loss.
- Plots report distinct delivered pages, not raw radio loss. Zero reception is
  valid only for a completed receiver run. Invalid/mixed/interrupted records are
  rejected. Local queue drops, bad frames and send failures remain visible.
  A completed record can still be an inadequate experiment: check elapsed time,
  stop reason and overlapping observation windows before interpreting a plot.
- The old `peers` array remains a summary of directly heard beacons. The separate
  topology file records timestamped direct receptions with full sender MACs.
  Neither record proves an entire page route or a bidirectional connection.
- Keep transmit power, firmware, payload, interval, power supplies and locations
  fixed for repetitions. Do not add deep sleep or Wi-Fi during the baseline.
- Host tests use Arduino, SHA marker, FreeRTOS and NVS substitutes. They do not
  test actual radio behaviour, FreeRTOS scheduling or flash power-loss durability.

## Persistent files and uuencoded retrieval

The trial build adds a LittleFS archive alongside the existing NVS record.
The NVS record layout is unchanged. Preparation/start/final NVS writes still work
if LittleFS is unavailable; use the original `dump` command to export that record.
Finished/interrupted records are archived at stop or next boot when the filesystem
is mounted. New result names are `MAC-RUN.log`; their JSON begins with the full
hardware MAC. Old `NODE-RUN.log` files remain readable. The retained 356-byte NVS
record is unchanged. `clear` removes the NVS record but keeps archived files.
Reusing an archived run number is rejected. Existing logs are not overwritten.

A completed run also writes `MAC-RUN.top`, a separate JSON topology file.
Up to 256 direct-reception observations are collected in RAM while RUNNING.
Each has sender MAC, run, transmission sequence, local milliseconds, RSSI and
packet type. Buffer overflow and ignored observations from other runs are
counted explicitly. A power interruption loses the in-memory topology trace;
the NVS summary is a separate fallback, not a replacement for that trace.

From a checkout (replace `PORT` with the actual USB-modem path):

```
python3 tool/files.py id PORT
python3 tool/files.py list PORT
python3 tool/files.py get PORT RUN --out log
python3 tool/files.py get PORT RUN --topology --out log
```

On the prepared Mac these are `./go id PORT`, `./go files PORT`,
`./go get PORT RUN` and `./go topo PORT RUN`. The firmware commands are `id`, `files`, `dump RUN` and
`topo RUN`. File exports emit a JSON envelope followed by traditional uuencode.
The laptop checks framing, byte count, CRC32, filename, MAC when present, node
and run before saving the original JSON bytes. Repeated exports get new filenames
rather than replacing earlier copies. CRC32 detects accidental transfer changes;
it is not authentication. JSON could also be transferred directly over USB;
uuencode is the chosen file-transfer representation.

To draw the measured topology from one run:

```
python3 tool/topo.py log/*.top --out out/top --labels data/boards.json
```

On the prepared Mac, `./go map FILE.top...` writes a fresh report folder.

Use files from one run only. An arrow means sender -> receiver ("heard by").
Unexpected links are included. Missing logs and trace overflow limit what can
be concluded from absent arrows. Event times are local to each board and must
not be subtracted across boards as if the clocks were synchronised.

LittleFS mounts with `begin(false)`: failed mounting never triggers automatic
formatting. A newly used filesystem may need explicit initialisation. First
export and inspect the existing NVS record, then clear that record. Only when
NVS is EMPTY and LittleFS is unmounted may `ctl PORT fsinit` initialise its
partition. This formats the filesystem partition and may erase pre-existing
filesystem data; inspect the board before issuing it. It does not erase NVS.
Do not change partition schemes or upload a filesystem image during this workflow.

File operations are refused while a trial is running. Observations are saved at
the end; there is no periodic RAM recovery checkpoint. A failed result archive
leaves NVS available. Green still means the NVS result was saved; use `id`/`files`
and retrieve both files to check archives separately. The
[31 October USB bench run](../res/usb1/README.md) checked this path on all five
boards: all ten files matched before and after restart and user-confirmed full
power disconnection. This checks completed logs; the content cache stays in RAM.

## Reproducible analysis without boards

```
python3 tool/report.py data/runs.json --out /path/to/new/report/folder
python3 tool/cache.py --out /path/to/cache/results
```

The first command verifies original input hashes, labels physical boards and
flags short/manual windows, overruns and local drops in the three historical
setup trials. The second runs deterministic cache scenarios against the actual
C++ core. Neither command performs a new hardware experiment. `PLAN.md` records
the hypotheses and the proposed hardware sequence.

## Hardware validation checklist

Firmware `b25c2f8` passed one 30-second USB bench run on all five boards.
MAC mapping, delivery, result/topology retrieval and completed-file retention
were checked. Remaining tests:

1. Map directed reachability at the intended house positions. The target is
   1-2-3-4-5, but recorded extra links are acceptable.
2. Repeat with fresh run IDs and the same settings. Check queue drops, trace
   overflow and overlapping local observation windows each time.
3. Interrupt power during a separate run; confirm INTERRUPTED and do not treat
   a missing topology trace as a normal zero-reception result.
4. Later, test B repairing C's pages with source A off; B must remain running.
   Agree a common-cutoff method before comparing recovery curves. Defer speed
   tuning and cache-policy changes until the identity/retrieval baseline is stable.

Reference APIs: [Preferences](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html),
[NVS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html),
[FreeRTOS queues](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos_idf.html).
