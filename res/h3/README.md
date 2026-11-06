# House, source pinned

- Run `2026110503`, firmware `9539f99`.
- Same house positions: 1 laptop, 2 corridor, 3 living room, 4 cellar, 5 hobby room.
- Five 32-byte pages, one second apart, 300 seconds per board.
- Source 1 stayed on, pinned the originals and repeated one page every five seconds: 58 repeats.
- Boards 2–5 each received 5/5 pages.
- Recorded pairs: 1↔2, 1↔3, 2↔3, 3↔4, 4↔5. Both directions included page frames.
- All first page copies at 4 came from 3; at 5 they came from 4.
- Completion after local start: 2 = 29.543 s, 3 = 41.868 s, 4 = 178.496 s, 5 = 189.911 s.
- No reported queue drops, bad frames, invalid pages, send failures or trace overflow.
- One successful run; it does not isolate the benefit of pinning.

[Raw logs and settings](../../data/h3/) · [Graph](graph.svg) · [Contacts](contacts.csv) · [Page arrivals](pages.csv)

Graph arrows show recorded receptions over the whole run, not a fixed route. Times in the CSV are local. [Timing estimates](timing.json) use shared frames, not synchronised clocks.

Rebuild with Graphviz installed: `python3 res/h3/plot.py`.
