# Plan

The [1 November house run](res/h1/README.md) recorded eleven directed links.
Nodes 3 and 4 received five pages; 2 and 5 received four, both missing page 3.
All five finished and all ten files were retrieved. The user confirmed the
room assignments; exact receiver start times were not recorded.

The [source-off run](res/h2/README.md) completed with 5/5 pages at 2 and 3,
2/5 at 4 and 0/5 at delayed receiver 5. Board 5 heard one beacon from 4,
but no pages. The run does not demonstrate cached-copy delivery to 5;
exact transition times and the shared active window remain unverified.

The five boards have completed the [page-size check](res/size/README.md).
All six 32/64/120-byte size runs delivered five pages to every receiver, but a
separate earlier repeat delivered only four. Delivery times varied even with
the same page size. All 60 size-run files survived hardware resets unchanged.
The [earlier run](res/usb1/README.md) also checked power disconnection.

First, establish which nodes can hear each other and keep every log clearly
assigned to its board and test run. The intended topology is 1-2-3-4-5.
Extra links are acceptable if they are recorded.

## Next steps

1. Do a controlled source-off check at the USB hub, with no walk. Verify that
   relays 2 and 3 have all five pages in their live results, then stop/export
   source 1 and unplug only that board. Start receiver 5 over USB while the
   relays are still RUNNING. Record control times and check the 256-event buffer.
   Use that small test before repeating source-off delivery around the house.
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
workflow and a measured house topology graph. Both house runs and their graphs are saved.
Firmware `ca6fe90` is installed on
all five boards with fixed identity colours and selectable page sizes. Speed
tuning comes later.

To investigate missing pages, add sender-side page attempts and final cache
contents as a later diagnostic. These are not recorded yet.

After a repeatable source-off baseline, compare 5, 10 and 20 pages with
the ten-page cache. Cache policy comparisons also wait until the baseline is
stable.

## Files

- `esp32/`: firmware
- `tool/`: retrieval and analysis
- `data/`: saved measurements
- `res/`: plots and reports

Build and retrieval commands are in [test/README.md](test/README.md).
