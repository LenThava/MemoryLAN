"""Check real firmware uuencode output through the laptop's decoder."""
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / "tool"))
from files import read_transfer

info, raw = read_transfer((line.rstrip(b"\r\n") for line in sys.stdin.buffer), 7)
record = json.loads(raw)
assert info["node"] == record["node"] == "1234"
assert record["state"] == "done"
assert record["sent"] == 3 and record["received"] == [0, 1, 2]
assert raw.endswith(b"\n")
print("firmware-to-Python file transfer: passed")
