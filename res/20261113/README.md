# BLE tests · 13 November 2026

[Short run report](short.pdf) · [Full run report](main.pdf) · [Discussion notes](discussion.pdf)

LaTeX: [short](short.tex), [full](main.tex), [discussion](discussion.tex).

- Hub A/B/F/E: all receivers got 5/5 pages. B → F improved reception on 20/20 directed links.
- House E: boards 4 and 5 got 3/5 pages. With A and D they got 0/5.
- Source stopped: board 5 got 5/5 with E and 0/5 with A. In E, board 4 sent all five first copies heard by 5.

Raw files by run: [6 November](../../data/20261106), [7 November](../../data/20261107), [13 November](../../data/20261113). Each completed run has a result and a topology file per board. Run 2026110703 has a plan only. [Board labels and MACs](../../data/boards.json).

| Build | Change |
| --- | --- |
| [A](https://github.com/LenThava/MemoryLAN/commit/9539f9952a474b903c5e89d58fef364e5512aaaf) | Baseline, CODED |
| [B](https://github.com/LenThava/MemoryLAN/tree/bleB) | A, scanner result limit removed |
| [F](https://github.com/LenThava/MemoryLAN/tree/bleF) | B, two advertising events |
| [E](https://github.com/LenThava/MemoryLAN/tree/bleE) | F, default advertising intervals |
| [D](https://github.com/LenThava/MemoryLAN/tree/bleD) | E, 1M PHY |
| [C](https://github.com/LenThava/MemoryLAN/tree/bleC) | B, 1M PHY. Built without a completed run. |

From the repository root:

```sh
shasum -a 256 -c res/20261113/SHA256SUMS
python3 res/20261113/hub.py A=2026111306 B=2026111307 F=2026111308 E=2026111309
python3 res/20261113/house.py 2026111301 2026111302 2026111303 2026111304 2026111305
```

To regenerate the plots and short LaTeX report from the logs, run `python3 res/20261113/build.py` with Graphviz installed.
