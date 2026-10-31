# Results

These are initial setup measurements and visualisation prototypes.

| Run | Setting | Report |
|---|---|---|
| 2026103102 | Five boards at the Mac; fixed identity colours | [Results](usb2/README.md) |
| 2026103101 | Five boards at the Mac; full MAC/topology logging | [Results](usb1/README.md) |
| 2026092801 | Two nearby boards | [Results](runs/2026092801.md) |
| 2026092901 | Five nearby boards | [Results](runs/2026092901.md) |
| 2026092902 | Five boards in the house | [Results](runs/2026092902.md) |

The latest run and raw inputs are under [data/usb2](../data/usb2/).
The historical inputs and their hashes are in [data/runs.json](../data/runs.json).
Repeated exports are not additional physical experiments. The house test did
not establish a working five-node path; node 3 stopped after 433 ms.

![House test](runs/2026092902.svg)

## Cache experiment

The actual C++ cache core was run with deterministic hardware substitutes.
This tests cache retention and refresh behaviour; it does not measure radio
delivery. Conditions, source hashes and results are in [cache.json](cach/cache.json).

![Cache refresh](cach/refresh.svg)

Generate fresh outputs from a checkout:

```
python3 tool/report.py data/runs.json --out /path/to/new/reports
python3 tool/cache.py --out /path/to/new/cache-results
```

Each command requires a fresh output location. See [PLAN.md](../PLAN.md) for
the next experiments. The 31 October run checked LittleFS/uuencode retrieval
and completed-file retention on all five boards. House placement remains to be
tested with the new firmware.
