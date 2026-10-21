# Plan

The five ESP32-S3-Zero boards have been tested successfully. The setup uses BLE
and one data source.

I want to test what a node can still receive after the source disappears, and
how the amount of cached data affects this.

## Next steps

1. Check that LittleFS logs survive unplugging and can be retrieved with uuencode.
2. Place the five boards around the house and note which neighbours they hear.
3. Send numbered test pages and compare what each board receives.
4. Let a relay receive the pages, disconnect the source, then start another
   receiver. Keep the relay powered and running until the receiver finishes.
5. Compare 5, 10 and 20 pages with the current 10-page cache. Repeat each case
   three times, keeping placement, firmware and sending interval fixed.

## Measurements

Save received page IDs, duplicates, queue drops and run duration for each board.
Add cache snapshots and first-arrival times for the later recovery tests.

Afterwards, consider comparing the current cache policy with strict FIFO.

## Files

- `esp32/`: firmware
- `tool/`: retrieval and analysis
- `data/`: saved measurements
- `res/`: plots and reports

Build and retrieval commands are in [test/README.md](test/README.md).
