# Notes

- 5 ESP32-S3-Zero boards, labelled 1–5.
- BLE, one source. Logs identify the board by MAC and the test by run number.
- Hub and house tests are saved in [runs](res/README.md).
- New house run: source pinned, all four receivers got 5/5 pages.
- Observed pairs: 1–2, 1–3, 2–3, 3–4, 4–5.

Next:
- Check Tschudin's next code update.
- Compare normal and pinned source mode at the same house positions.
- Keep drawing the topology from the logs; do not assume a chain.
- Prepare ten-minute tests later. Check shared running time and log capacity first.

Still open:
- Why earlier runs missed pages.
- Record page-send attempts and cache contents.
- DONE stops forwarding. Boards need enough shared running time.
- Longer tests may fill the 512-event log.
