# Trial preparation

Optional firmware for one source and up to 256 numbered 32-byte pages.
Trial controls are enabled only when `MLAN_TRIAL` is defined.
The core correctness fixes also apply to the ordinary MemoryLAN build.
No trial starts automatically after boot.
Trial builds suppress routine radio/core debug output; USB commands and JSON
results remain enabled. Ordinary builds keep their debug output.

On the Waveshare trial build: amber = prepared, blue = running, green = finished
and saved, red = interrupted/storage error, off = no trial. These state colours
replace the normal packet-activity LED; green does not imply successful delivery.

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
The `node` field in `dump` identifies it. Label physical boards 1 through 5.
Use the same firmware binary on all participating boards. Keep other test
firmware and BleBC demos off during a run.

## First bench run

Choose a fresh run number and the source's four-digit hexadecimal node ID.
Prepare each board, including the source (example source ID `1234`):

```
./go ctrl PORT prep 1 1234 5 1000 120
```

Arguments: run, source, page count, configured minimum spacing in milliseconds,
LOCAL target duration in seconds. Sending is synchronous, so spacing and elapsed
duration can overrun; this is not an exact-rate generator or a hard deadline.
Preparation survives a power cycle.
An existing record must be exported and explicitly cleared before preparation.

1. Put each receiver on its final USB power supply. Hold BOOT for about half a
   second to start, then release. Extra presses cannot stop a running trial.
2. Start the laptop-connected source last: `./go ctrl PORT start`.
3. The source injects the configured pages once. Cache repair continues.
4. Let each local timer expire to finish and save in place. Wait for green before
   unplugging. Red means keep power and inspect with `dump`.
   Do not move active boards together for collection.
5. Connect each board to USB and run `./go dump PORT`. Reboot never resumes a
   finished/interrupted run. Files are saved under `~/mlan/log` without overwrite.
6. Include the source record when plotting:
   `./go plot log/SOURCE.json log/B.json log/C.json`.
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
  writes, cache persistence, extra application retransmissions or hop headers.
- The current controls use **independent local observation windows**. These are
  not a synchronised global cutoff. Record activation/finish order and durations.
  Do not claim equal recovery time, one-way latency or causal per-hop loss.
- Plots report distinct delivered pages, not raw radio loss. Zero reception is
  valid only for a completed receiver run. Invalid/mixed/interrupted records are
  rejected. Local queue drops, bad frames and send failures remain visible.
  A completed record can still be an inadequate experiment: check elapsed time,
  stop reason and overlapping observation windows before interpreting a plot.
- Peer samples are observations of directly heard beacons, not proof of each
  page's route. The peer array stores up to 16 IDs with the last RSSI/relative
  observation time. It is not a continuous topology trace.
- Keep transmit power, firmware, payload, interval, power supplies and locations
  fixed for repetitions. Do not add deep sleep or Wi-Fi during the baseline.
- Host tests use Arduino, SHA marker, FreeRTOS and NVS substitutes. They do not
  test actual radio behaviour, FreeRTOS scheduling or flash power-loss durability.

## Persistent files and uuencoded retrieval

The trial build adds a LittleFS archive alongside the existing NVS record.
The NVS record layout is unchanged. Preparation/start/final NVS writes still work
if LittleFS is unavailable; use the original `dump` command to export that record.
Finished/interrupted records are archived at stop or next boot when the filesystem
is mounted. Archive names are `NODE-RUN.log`; their contents are the same JSON
representation used by `dump`. `clear` removes the NVS record but keeps archived
files. Reusing an archived run number is rejected. Existing log files are never
overwritten with different content.

From a checkout (replace `PORT` with the actual USB-modem path):

```
python3 tool/files.py id PORT
python3 tool/files.py list PORT
python3 tool/files.py get PORT RUN --out log
```

On the prepared Mac these are `./go id PORT`, `./go files PORT`, and
`./go get PORT RUN`. The firmware commands are `id`, `files`, and `dump RUN`.
The latter emits a JSON envelope followed by traditional uuencode data. The
laptop checks framing, byte count, CRC32, filename, node and run before saving
the original JSON bytes. Repeated exports get new filenames rather than replacing
an earlier copy. CRC32 detects accidental transfer changes; it is not authentication.

LittleFS mounts with `begin(false)`: failed mounting never triggers automatic
formatting. A newly used filesystem may need explicit initialisation. First
export and inspect the existing NVS record, then clear that record. Only when
NVS is EMPTY and LittleFS is unmounted may `ctl PORT fsinit` initialise its
partition. This formats the filesystem partition and may erase pre-existing
filesystem data; inspect the board before issuing it. It does not erase NVS.
Do not change partition schemes or upload a filesystem image during this workflow.

File operations are refused while a trial is running. Archives are small final
summaries, not continuous per-packet logs; there is still no periodic RAM recovery
checkpoint. A failed archive leaves NVS available and reports that the file is
unavailable. Green still means the NVS result was saved; use `id`/`files` to check
the archive separately. The new file path has host coverage and a firmware build;
actual LittleFS retention and USB retrieval still require a board test.

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

The start-only BOOT and quiet-debug changes pass host tests and compile, but
have not yet been flashed or checked on boards.

1. Two-board delivery, then B repairing C's missing pages with source A off.
   Finish and save A before unplugging it; keep B running and start C afterwards.
2. Save, unplug, reconnect and retrieve the identical finished result.
3. Interrupt power during a run; confirm INTERRUPTED, not a normal loss result.
4. Validate power-bank runtime and map who hears whom before five-node trials.
5. Agree a common-cutoff method before comparing recovery curves across nodes.

Reference APIs: [Preferences](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html),
[NVS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html),
[FreeRTOS queues](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos_idf.html).
