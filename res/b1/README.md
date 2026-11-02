# Cache check at the Mac

- Run `2026110201`, firmware `ca6fe90`.
- Five nearby boards. Pages travelled over BLE.
- Source 1 generated five 32-byte pages. Boards 2–4 had each received all five.
- Board 5 was waiting. Source 1 was stopped and physically unplugged.
- Then 5 started with an empty cache while 2–4 stayed running.
- Board 5 received page 2 from 3 at 5.260 s and page 3 from 4 at 10.198 s.
- It received page 0 from 2 at 35.116 s and page 1 from 4 at 80.475 s.
- Board 5 stopped at 91.476 s with 4/5 pages. Page 4 was still missing.
- These times start when 5 starts. Its log contains no transmissions from 1.
- Stored copies reached 5; why one page stayed missing is still open.
- Both files were read from every board.

[Raw logs and settings](../../data/b1/) · [Page arrivals](pages.csv)
