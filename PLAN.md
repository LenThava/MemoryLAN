# Plan

The [1 November house run](res/h1/README.md) recorded eleven directed links.
Nodes 3 and 4 received five pages; 2 and 5 received four, both missing page 3.
All five finished and all ten files were retrieved. The user confirmed the
room assignments; exact receiver start times were not recorded.

The five boards have completed the [page-size check](res/size/README.md).
All six 32/64/120-byte size runs delivered five pages to every receiver, but a
separate earlier repeat delivered only four. Delivery times varied even with
the same page size. All 60 size-run files survived hardware resets unchanged.
The [earlier run](res/usb1/README.md) also checked power disconnection.

First, establish which nodes can hear each other and keep every log clearly
assigned to its board and test run. The intended topology is 1-2-3-4-5.
Extra links are acceptable if they are recorded.

## Next steps

1. Keep the confirmed house positions. In the next run, start relays 2--4,
   then source 1. Keep receiver 5 READY until source 1 has been stopped and
   disconnected. Start 5 afterwards to test delivery from cached copies.
   Choose enough shared time for both walks and check the 256-event buffer.
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
workflow and a measured house topology graph. The first house graph is saved.
Firmware `ca6fe90` is installed on
all five boards with fixed identity colours and selectable page sizes. Speed
tuning comes later.

To investigate missing pages, add sender-side page attempts and final cache
contents as a later diagnostic. These are not recorded yet.

After the house baseline, test source-off cache recovery and compare 5, 10 and 20 pages with
the ten-page cache. Cache policy comparisons also wait until the baseline is
stable.

## Files

- `esp32/`: firmware
- `tool/`: retrieval and analysis
- `data/`: saved measurements
- `res/`: plots and reports

Build and retrieval commands are in [test/README.md](test/README.md).
