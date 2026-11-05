"""Topology tests use synthetic identities and observations, never board measurements."""
import copy
import csv
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).parents[1] / "tool"))
import topo

A, B, C = "001122334455", "66778899aabb", "ccddeeff0011"


def event(sender=A, ms=100, kind="beacon", page=None):
    return dict(sender=sender, run=7, seq=1, ms=ms, rssi=-60, type=kind, page=page)


def record(mac=B, events=None):
    return dict(mac=mac, node="1234", run=7, schema=1, kind="mlan-topology", build="synthetic-test",
                clock="local_ms_since_start", elapsed_ms=1000, capacity=256, overflow=0,
                ignored_run=0, events=events if events is not None else [event()])


class TopologyTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mlan-topo-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def write(self, name, data):
        path = self.directory / name
        path.write_text(json.dumps(data))
        return path

    def test_direction_and_missing_receiver_log(self):
        path = self.write("b.top", record(events=[event(), event(ms=200, kind="page", page=2)]))
        report = topo.analyse([path])
        self.assertEqual(len(report["links"]), 1)
        link = report["links"][0]
        self.assertEqual((link["sender"], link["receiver"], link["observations"]), (A, B, 2))
        self.assertEqual((link["beacons"], link["pages"]), (1, 1))
        matrix = list(csv.reader(io.StringIO(topo.matrix_csv(report))))
        self.assertEqual(matrix, [["sender_mac / receiver_mac", A, B], [A, "", "2"], [B, "", ""]])
        self.assertTrue(any("No receiver log supplied" in w for w in report["warnings"]))

    def test_explicit_zero_and_independent_directions(self):
        a = self.write("a.top", record(A, []))
        b = self.write("b.top", record(B, [event(A)]))
        report = topo.analyse([a, b])
        rows = list(csv.reader(io.StringIO(topo.matrix_csv(report))))
        self.assertEqual(rows[1], [A, "", "1"])
        self.assertEqual(rows[2], [B, "0", ""])
        self.assertEqual({r["mac"] for r in report["receivers"]}, {A, B})

    def test_no_merging_repeated_runs_or_duplicate_exports(self):
        a = self.write("a.top", record())
        with self.assertRaisesRegex(ValueError, "Duplicate observer"):
            topo.analyse([a, a])
        other = record(A, [])
        other["run"] = 8
        b = self.write("a2.top", other)
        with self.assertRaisesRegex(ValueError, "exactly one run"):
            topo.analyse([a, b])

    def test_event_validation(self):
        changes = ({"sender": "18c7"}, {"run": 8}, {"ms": 1001}, {"ms": True},
                   {"rssi": -129}, {"seq": -1}, {"page": 256}, {"type": "received"})
        for change in changes:
            value = record(events=[dict(event(), **change)])
            with self.subTest(change=change), self.assertRaises(ValueError):
                topo.validate(value)
        with self.assertRaisesRegex(ValueError, "time order"):
            topo.validate(record(events=[event(ms=200), event(ms=100)]))
        for change in ({"schema": True}, {"mac": "bad"}, {"clock": "global"},
                       {"overflow": -1}, {"capacity": 0}, {"node": "123456"}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                topo.validate(dict(record(), **change))

    def test_mixed_firmware_is_rejected(self):
        a = self.write("a.top", record(A, []))
        other = record(B)
        other["build"] = "different-test-build"
        b = self.write("b.top", other)
        with self.assertRaisesRegex(ValueError, "Mixed firmware"):
            topo.analyse([a, b])

    def test_payload_sizes(self):
        for size in (32, 64, 120):
            value = dict(record(), payload_bytes=size)
            self.assertEqual(topo.validate(value)["payload_bytes"], size)
        for size in (None, True, 0, 128, "64"):
            with self.subTest(size=size), self.assertRaises(ValueError):
                topo.validate(dict(record(), payload_bytes=size))
        a = self.write("a.top", record(A, []))
        b = self.write("b.top", dict(record(B), payload_bytes=64))
        with self.assertRaisesRegex(ValueError, "Mixed trial payload"):
            topo.analyse([a, b])

    def test_capacity_and_overflow_are_visible(self):
        value = record()
        value.update(capacity=1, overflow=4, ignored_run=3)
        path = self.write("full.top", value)
        report = topo.analyse([path])
        self.assertTrue(any("4 observations omitted" in w for w in report["warnings"]))
        self.assertTrue(any("3 frames from other runs" in w for w in report["warnings"]))
        value["events"].append(event())
        with self.assertRaises(ValueError):
            topo.validate(value)

    def test_source_mode_defaults_and_mixed_runs(self):
        a = self.write("a.top", record(A, []))
        normal = dict(record(), source_pinned=False, repeat_ms=0, repeat_attempts=0)
        b = self.write("b.top", normal)
        report = topo.analyse([a, b])
        self.assertEqual((report["source_pinned"], report["repeat_ms"]), (False, 0))
        self.assertEqual(report["receivers"][0]["repeat_attempts"], 0)
        pinned = dict(record(A, []), source_pinned=True, repeat_ms=5000, repeat_attempts=7)
        a = self.write("a.top", pinned)
        with self.assertRaisesRegex(ValueError, "Mixed source-mode"):
            topo.analyse([a, b])
        b = self.write("b.top", dict(record(), source_pinned=True, repeat_ms=5000, repeat_attempts=0))
        report = topo.analyse([a, b])
        self.assertEqual((report["source_pinned"], report["repeat_ms"]), (True, 5000))
        self.assertEqual(report["receivers"][0]["repeat_attempts"], 7)
        b = self.write("b.top", dict(record(), source_pinned=True, repeat_ms=1000, repeat_attempts=0))
        with self.assertRaisesRegex(ValueError, "Mixed source-mode"):
            topo.analyse([a, b])

    def test_invalid_source_mode(self):
        changes = ({"repeat_ms": 5000},
                   {"source_pinned": True, "repeat_ms": 0, "repeat_attempts": 0},
                   {"source_pinned": False, "repeat_ms": 0, "repeat_attempts": 1},
                   {"source_pinned": True, "repeat_ms": 5000, "repeat_attempts": True})
        for change in changes:
            with self.subTest(change=change), self.assertRaises(ValueError):
                topo.validate(dict(record(), **change))

    def test_labels_preserve_hardware_identity_and_escape_svg(self):
        labels = {A: dict(label="1", location="room & window"), B: dict(label="2", location="<door>")}
        label_path = self.write("labels.json", labels)
        path = self.write("b.top", record())
        report = topo.analyse([path], topo.read_labels(label_path))
        self.assertEqual(report["nodes"][0]["mac"], A)
        svg = topo.graph_svg(report)
        ET.fromstring(svg)
        self.assertIn("room &amp; window", svg)
        self.assertIn("&lt;door&gt;", svg)
        labels[B]["label"] = "1"
        with self.assertRaises(ValueError):
            topo.read_labels(self.write("labels.json", labels))

    def test_report_writes_and_protects_inputs(self):
        path = self.write("b.top", record())
        before = path.read_bytes()
        report = topo.analyse([path])
        output = self.directory / "out"
        result = topo.write_report(report, output, [path])
        self.assertEqual(json.loads(result.read_text())["run"], 7)
        self.assertEqual(path.read_bytes(), before)
        self.assertEqual({p.name for p in output.iterdir()}, {"summary.json", "matrix.csv", "graph.svg"})
        with self.assertRaisesRegex(ValueError, "overwrite"):
            topo.write_report(report, output, [output / "summary.json"])

    def test_current_prototype_files_are_not_topology_measurements(self):
        with self.assertRaisesRegex(ValueError, "Expected a topology log"):
            topo.validate(dict(record(), kind="mlan-trial"))


if __name__ == "__main__":
    unittest.main()
