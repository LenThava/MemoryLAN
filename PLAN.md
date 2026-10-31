# Plan

The five ESP32-S3-Zero boards passed two USB bench runs on 31 October 2026.
In the [latest run](res/usb2/README.md), all four receivers obtained five pages
and all five boards heard each other. The ten result/topology files and stored
board numbers survived a hardware reset. The [earlier run](res/usb1/README.md)
also checked completed files after power disconnection.

First, establish which nodes can hear each other and keep every log clearly
assigned to its board and test run. The intended topology is 1-2-3-4-5.
Extra links are acceptable if they are recorded.

## Next steps

1. Place all boards around the house while they are still READY. Start the
   receivers only after placement, then start the source last. Choose a duration
   that leaves a shared observation window after the activation walk.
2. Use a fresh run ID shared by every participating board. Keep firmware,
   placement, power and sending interval fixed for repetitions.
3. Keep the boards stationary until all have finished. Retrieve both files from
   each board and plot the directed links observed, including unexpected links.

`data/boards.json` maps the verified MACs to labels and intended house locations.
Each run needs its own placement snapshot; the USB bench used a separate map.

## Measurements

Each new log identifies the board by its full six-byte default MAC address.
Topology observations record the sender, receiving board, run ID, local time,
signal strength and transmission sequence. A heard packet is a directed
observation, not proof of a bidirectional connection or an entire page route.
Record buffer overflow and observations from other runs remain visible.

For Monday, 2 November at 10:00, the target is a reliable identity/retrieval
workflow and a measured house topology graph. Firmware `2d9b46c` is installed on
all five boards with fixed identity colours. Speed tuning comes later.

After that, test source-off cache recovery and compare 5, 10 and 20 pages with
the ten-page cache. Cache policy comparisons also wait until the baseline is
stable.

## Files

- `esp32/`: firmware
- `tool/`: retrieval and analysis
- `data/`: saved measurements
- `res/`: plots and reports

Build and retrieval commands are in [test/README.md](test/README.md).
