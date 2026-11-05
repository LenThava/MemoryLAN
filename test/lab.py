import copy
import importlib.util
import tempfile
import unittest
import json
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

    def test_payload_sizes(self):
        for size in (32, 64, 120):
            records = fixture()
            for record in records:
                record["payload_bytes"] = size
            self.assertEqual(lab.payload_bytes(lab.validate(records)), size)
        records = fixture()
        records[1]["payload_bytes"] = 64
        with self.assertRaisesRegex(ValueError, "Mixed run"):
            lab.validate(records)
        for size in (None, True, 0, 31, 128, "64"):
            records = fixture()
            records[1]["payload_bytes"] = size
            with self.subTest(size=size), self.assertRaises(ValueError):
                lab.validate(records)

    def test_source_mode_and_legacy_defaults(self):
        records = fixture()
        records[1].update(source_pinned=False, repeat_ms=0, repeat_attempts=0)
        self.assertEqual(lab.source_mode(lab.validate(records)), (False, 0, 0))
        for record in records:
            record.update(source_pinned=True, repeat_ms=5000, repeat_attempts=0)
        records[0]["repeat_attempts"] = 7
        self.assertEqual(lab.source_mode(lab.validate(records)), (True, 5000, 7))
        with tempfile.TemporaryDirectory(prefix="mlan-test-") as temp:
            summary = json.loads(lab.plots(records, temp)[2].read_text())
            self.assertEqual((summary["source_pinned"], summary["repeat_ms"], summary["repeat_attempts"]),
                             (True, 5000, 7))

    def test_reject_source_mode_mismatch_and_invalid_values(self):
        records = fixture()
        records[0].update(source_pinned=True, repeat_ms=5000, repeat_attempts=0)
        with self.assertRaisesRegex(ValueError, "Mixed run"):
            lab.validate(records)
        invalid = ({"source_pinned": True},
                   {"source_pinned": 1, "repeat_ms": 5000, "repeat_attempts": 0},
                   {"source_pinned": True, "repeat_ms": 0, "repeat_attempts": 0},
                   {"source_pinned": False, "repeat_ms": 5000, "repeat_attempts": 0},
                   {"source_pinned": True, "repeat_ms": 999, "repeat_attempts": 0},
                   {"source_pinned": True, "repeat_ms": 60001, "repeat_attempts": 0},
                   {"source_pinned": True, "repeat_ms": True, "repeat_attempts": 0},
                   {"source_pinned": False, "repeat_ms": 0, "repeat_attempts": 1},
                   {"source_pinned": False, "repeat_ms": 0, "repeat_attempts": -1},
                   {"source_pinned": False, "repeat_ms": 0, "repeat_attempts": True})
        for change in invalid:
            records = fixture()
            records[0].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                lab.validate(records)
        records = fixture()
        for record in records:
            record.update(source_pinned=True, repeat_ms=5000, repeat_attempts=1)
        with self.assertRaisesRegex(ValueError, "receiver reports source repeat"):
            lab.validate(records)
        records = fixture()
        for record in records:
            record.update(source_pinned=True, repeat_ms=5000, repeat_attempts=0, total=11)
        records[0].update(sent=11, received=list(range(11)))
        with self.assertRaisesRegex(ValueError, "ten-page cache"):
            lab.validate(records)

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

    def test_full_mac_identity_and_alias_conflicts(self):
        records = fixture()
        records[0]["mac"] = "001122334455"
        records[1]["mac"] = "66778899aabb"
        lab.validate(records)
        for invalid in ("66778899AABB", "66778899aab", "66778899aabbcc", None, True):
            altered = copy.deepcopy(records)
            altered[1]["mac"] = invalid
            with self.subTest(mac=invalid), self.assertRaises(ValueError):
                lab.validate(altered)
        altered = copy.deepcopy(records)
        altered[1]["mac"] = records[0]["mac"]
        with self.assertRaisesRegex(ValueError, "Duplicate hardware MAC"):
            lab.validate(altered)
        altered = copy.deepcopy(records)
        altered[1]["node"] = records[0]["node"]
        with self.assertRaisesRegex(ValueError, "Duplicate node alias"):
            lab.validate(altered)

    def test_dump_names_preserve_full_and_legacy_identity(self):
        source, receiver = fixture()
        source["mac"] = "001122334455"
        with tempfile.TemporaryDirectory(prefix="mlan-test-") as temp:
            path = lab.save_dump(source, temp)
            legacy = lab.save_dump(receiver, temp)
            self.assertTrue(path.name.startswith("001122334455-42-"))
            self.assertTrue(legacy.name.startswith("42-0002-"))
            self.assertEqual(json.loads(path.read_text()), source)
            invalid = dict(source, mac="bad")
            with self.assertRaises(ValueError):
                lab.save_dump(invalid, temp)
            self.assertEqual(len(list(Path(temp).iterdir())), 2)

    def test_full_mac_remains_in_summary(self):
        records = fixture()
        records[0]["mac"] = "001122334455"
        records[1]["mac"] = "66778899aabb"
        with tempfile.TemporaryDirectory(prefix="mlan-test-") as temp:
            paths = lab.plots(records, temp)
            summary = json.loads(paths[2].read_text())
            self.assertEqual(summary["mac"], "001122334455")
            self.assertEqual(summary["nodes"][0]["mac"], "66778899aabb")


if __name__ == "__main__":
    unittest.main()
