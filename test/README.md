# Commands

From the repo root. Arduino CLI, ESP32 core 3.3.12 and Python 3.
Build once; use the same build on all five boards.

## Build and upload

```sh
make -C esp32 BOARD=WaveshareS3Zero compile EXTRA_FLAGS=-DMLAN_TRIAL
ls /dev/cu.usbmodem*
PORT=/dev/cu.usbmodemXXXX
python3 tool/files.py id "$PORT"
make -C esp32 BOARD=WaveshareS3Zero PORT="$PORT" upload
```

Replace `XXXX` with the real port. Upload one board at a time.
Check the MAC again after reconnecting; ports can change.

## Prepare and start

Choose the run number once and write it down. Use it for all five boards.
`5d60` is our source board 1. Settings: five 32-byte pages, 1 s gap, 300 s per board.
For an old run, use its old run number to retrieve both files below.
Then clear that record before preparing the new run.

```sh
RUN=$(date +%s)
python3 tool/lab.py ctl "$PORT" dump
python3 tool/lab.py ctl "$PORT" prep "$RUN" 5d60 5 1000 300 32
```

Repeat preparation on each board, changing `PORT` but keeping `RUN`.
For the house test, move prepared boards to their final power supplies.
Start 5, 4, 3, 2, then source 1. Hold BOOT briefly, or use USB:

```sh
python3 tool/lab.py ctl "$PORT" start
```

Wait for all boards to finish before moving them. DONE also stops forwarding.
Fast magenta blink: keep power and inspect `dump`.
To stop early over USB: `python3 tool/lab.py ctl "$PORT" stop`.

## Retrieve

For each finished board, set its current `PORT` and the saved `RUN`.

```sh
python3 tool/files.py id "$PORT"
python3 tool/files.py get "$PORT" "$RUN" --out "log/$RUN"
python3 tool/files.py get "$PORT" "$RUN" --topology --out "log/$RUN"
```

Check that both files were saved. Only then clear the current record:

```sh
python3 tool/lab.py ctl "$PORT" clear
```

- `.log`: received page IDs and counters. `.top`: who heard whom, with MACs.
- A beacon contact does not mean a data page arrived. Times are local per board.
- Pages are cached in RAM. Finished log files are stored in LittleFS.
- A power cut during a run loses the latest RAM observations.
- The trace holds 256 events per board; check overflow before interpreting it.
- Every new physical test gets a new run number. Source-off tests need active relays.
