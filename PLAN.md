# Notes

- 5 ESP32-S3-Zero boards, labelled 1–5.
- BLE, one source. Logs identify the board by MAC and the test by run number.
- Hub and house tests are saved in [runs](res/README.md).
- 13 November: [BLE comparison](res/20261113/README.md).

Next:
- Check Tschudin's next code update.
- If needed, repeat the A/E source-stop pair with the same placement and join delay.
- Keep drawing the topology from the logs; do not assume a chain.
- Compare within a common five-minute window.

Still open:
- Why earlier runs missed pages.
- Record page-send attempts and cache contents.
- DONE stops forwarding. Boards need enough shared running time.
- Longer tests may fill the 512-event log.
