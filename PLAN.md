# Plan

The five ESP32-S3-Zero boards have been tested successfully. The setup uses BLE
and one data source.

First, establish which nodes can hear each other and keep every log clearly
assigned to its board and test run. The intended topology is 1-2-3-4-5.
Extra links are acceptable if they are recorded.

## Next steps

1. Flash the same new firmware on all five boards. It uses a new BLE envelope.
2. Read each board's full hardware MAC address and match it to its physical
   label, 1 through 5. Enter the actual addresses and locations in
   `data/boards.json`.
3. Check that saved result and topology files survive unplugging and can be
   retrieved with uuencode.
4. Run a nearby baseline, using one fresh run ID shared by all participating
   boards. Keep firmware, placement, power and sending interval fixed.
5. Place the boards around the house and plot the directed links actually
   observed. Record unexpected links as well as gaps in the intended chain.

## Measurements

Each new log identifies the board by its full six-byte default MAC address.
Topology observations record the sender, receiving board, run ID, local time,
signal strength and transmission sequence. A heard packet is a directed
observation, not proof of a bidirectional connection or an entire page route.
Record buffer overflow and observations from other runs remain visible.

For Monday, 2 November at 10:00, the target is a reliable identity/retrieval
workflow and a measured topology graph. The new firmware still needs flashing
and hardware validation. Speed tuning comes later.

After that, test source-off cache recovery and compare 5, 10 and 20 pages with
the ten-page cache. Cache policy comparisons also wait until the baseline is
stable.

## Files

- `esp32/`: firmware
- `tool/`: retrieval and analysis
- `data/`: saved measurements
- `res/`: plots and reports

Build and retrieval commands are in [test/README.md](test/README.md).
