import copy
import importlib.util
import tempfile
import unittest
from pathlib import Path
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location("lab", Path(__file__).parents[1] / "tool/lab.py")
lab = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lab)


def fixture():
    source = dict(schema=1, kind="mlan-run", state="done", storage_ok=True, node="0001",
                  source="0001", run=42, total=5, sent=5, gap_ms=1000, limit_ms=60000,
                  elapsed_ms=10000, stop_reason=1, cutoff="local", build="synthetic test",
                  duplicates=0, invalid=0, rx=0, tx=5, rx_drop=0, rx_bad=0, tx_fail=0,
                  received=[0, 1, 2, 3, 4], peers=[])
    receiver = copy.deepcopy(source)
    receiver.update(node="0002", sent=0, received=[0, 2, 4])
    return [source, receiver]


class LabTest(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(lab.validate(fixture())["sent"], 5)

    def test_reject(self):
        for key, value in [("schema", True), ("state", "interrupted"), ("storage_ok", False),
                           ("run", 43), ("build", "different"), ("node", "0001"),
                           ("received", [0, 0]), ("received", [5]), ("received", [True]),
                           ("sent", 1), ("rx_drop", -1), ("cutoff", "global")]:
            records = fixture()
            records[1][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                lab.validate(records)

    def test_invalid_record_type(self):
        for invalid in (None, [], "not a record", 1):
            with self.subTest(value=invalid), self.assertRaises(ValueError):
                lab.validate([fixture()[0], invalid])

    def test_source_required_and_complete(self):
        with self.assertRaises(ValueError):
            lab.validate(fixture()[1:])
        records = fixture()
        records[0]["sent"] = 4
        with self.assertRaises(ValueError):
            lab.validate(records)

    def test_zero_reception_and_svg(self):
        records = fixture()
        records[1]["received"] = []
        lab.validate(records)
        with tempfile.TemporaryDirectory(prefix="mlan-test-") as temp:
            paths = lab.plots(records, temp)
            for path in paths[:2]:
                ET.parse(path)
                self.assertIn("unsynchronised", path.read_text())
            with self.assertRaises(ValueError):
                lab.plots(records, temp)


if __name__ == "__main__":
    unittest.main()
