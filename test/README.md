# Trial preparation

Optional firmware for one source and up to 256 numbered 32-byte pages.
Trial controls are enabled only when `MLAN_TRIAL` is defined.
The core correctness fixes also apply to the ordinary MemoryLAN build.
No trial starts automatically after boot.

On the Waveshare trial build: amber = prepared, blue = running, green = finished
and saved, red = interrupted/storage error, off = no trial. These state colours
replace the normal packet-activity LED and still need checking on real boards.

From `~/mlan`:

```
./go test
./go lab
./go port
./go load /dev/cu.usbmodemXXXX
./go ctrl /dev/cu.usbmodemXXXX dump
```

`load` flashes one Waveshare ESP32-S3-Zero. Identify the board first.
The `node` field in `dump` identifies it. Label physical boards A through E.
Use the same firmware binary on all participating boards. Keep other test
firmware and BleBC demos off during a run.

Without the local `go` wrapper, run `sh test/run.sh` from the checkout.
Build using `make -C esp32 BOARD=WaveshareS3Zero compile EXTRA_FLAGS=-DMLAN_TRIAL`
(with the installed ESP32 Arduino core); `python3 tool/lab.py --help` lists the
portable USB/export/plot commands. The wrapper only supplies local paths.

## First bench run

Choose a fresh run number and the source's four-digit hexadecimal node ID.
Prepare each board, including the source (example source ID `1234`):

```
./go ctrl PORT prep 1 1234 5 1000 120
```

Arguments: run, source, page count, injection interval in milliseconds,
maximum LOCAL run duration in seconds. Preparation survives a power cycle.
An existing record must be exported and explicitly cleared before preparation.

1. Put each receiver on its final USB power supply. Click BOOT once to start.
2. Start the laptop-connected source last: `./go ctrl PORT start`.
3. The source injects the configured pages once. Cache repair continues.
4. Briefly click BOOT once on each running board to finish and save in place, or
   let its local timer expire. Wait for green before unplugging. Red means keep
   power and inspect with `dump`. Do not move active boards together for collection.
5. Connect each board to USB and run `./go dump PORT`. Reboot never resumes a
   finished/interrupted run. Files are saved under `~/mlan/log` without overwrite.
6. Include the source record when plotting:
   `./go plot log/SOURCE.json log/B.json log/C.json`.
7. After checking exports, `./go ctrl PORT clear` removes only this trial record.

`./go ctrl PORT stop` is the USB equivalent of finishing with BOOT.
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
- Peer samples are observations of directly heard beacons, not proof of each
  page's route. The peer array stores up to 16 IDs with the last RSSI/relative
  observation time. It is not a continuous topology trace.
- Keep transmit power, firmware, payload, interval, power supplies and locations
  fixed for repetitions. Do not add deep sleep or Wi-Fi during the baseline.
- Host tests use Arduino, SHA marker, FreeRTOS and NVS substitutes. They do not
  test actual radio behaviour, FreeRTOS scheduling or flash power-loss durability.

## Hardware gates still required

1. Two-board delivery, then B repairing C's missing pages with source A off.
   Finish and save A before unplugging it; keep B running and start C afterwards.
2. Save, unplug, reconnect and retrieve the identical finished result.
3. Interrupt power during a run; confirm INTERRUPTED, not a normal loss result.
4. Validate power-bank runtime and map who hears whom before five-node trials.
5. Agree a common-cutoff method before comparing recovery curves across nodes.

Reference APIs: [Preferences](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html),
[NVS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html),
[FreeRTOS queues](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos_idf.html).
