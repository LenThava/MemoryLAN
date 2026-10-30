import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / "tool"))
from files import read_transfer

topology = sys.argv[1:] == ["--topology"]
info, raw = read_transfer((line.rstrip(b"\r\n") for line in sys.stdin.buffer), 7, topology)
record = json.loads(raw)
assert info["node"] == record["node"] == "1234"
assert info["mac"] == record["mac"] == "123456789abc"
if topology:
    assert info["name"] == "123456789abc-7.top"
    assert record["kind"] == "mlan-topology" and record["elapsed_ms"] == 200
    assert record["overflow"] == 0 and record["ignored_run"] == 0
    assert record["events"] == [
        dict(sender="aabbccddeeff", run=7, seq=9, ms=123, rssi=-51, type="beacon", page=None),
        dict(sender="aabbccddeeff", run=7, seq=10, ms=200, rssi=-61, type="page", page=2),
    ]
else:
    assert info["name"] == "123456789abc-7.log"
    assert record["state"] == "done"
    assert record["sent"] == 3 and record["received"] == [0, 1, 2]
assert raw.endswith(b"\n")
print(f'firmware-to-Python {"topology" if topology else "result"} transfer: passed')
