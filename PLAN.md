# Notes

- 5 ESP32-S3-Zero boards, labelled 1–5.
- BLE, one source. Logs identify the board by MAC and the test by run number.
- Hub tests, two house tests and a source-off check are saved in [runs](res/README.md).

Next:
- Check Tschudin's next code update.
- Repeat a short hub test, then a house test.
- Compare normal mode with pinned source pages and periodic repeats.
- Record who hears whom. Do not assume a 1–2–3–4–5 chain.
- Draw the topology from those logs.

Still open:
- Why some pages are missing.
- Check the new source mode on the boards.
- Record page-send attempts and cache contents.
- DONE stops forwarding. Boards need enough shared running time.
- Longer tests may fill the 512-event log.
