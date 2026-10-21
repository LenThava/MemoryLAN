import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).parents[1] / "tool"))
import report


def fixture():
    source = dict(schema=1, kind="mlan-run", state="done", storage_ok=True,
                  node="aaaa", source="aaaa", run=42, total=5, sent=5,
                  gap_ms=1000, limit_ms=300000, elapsed_ms=300000,
                  stop_reason=2, cutoff="local", build="synthetic test fixture",
                  duplicates=0, invalid=0, rx=0, tx=5, rx_drop=0,
                  rx_bad=0, tx_fail=0, received=[0, 1, 2, 3, 4], peers=[])
    receiver = copy.deepcopy(source)
    receiver.update(node="0001", sent=0, received=[])
    boards = {"aaaa": {"label": 1, "location": "source"},
              "0001": {"label": 2, "location": "receiver"}}
    run = dict(run=42, source="aaaa", setting="Synthetic unit test", records=[])
    return [source, receiver], boards, run


class ReportTest(unittest.TestCase):
    def test_short_manual_run_keeps_observation_without_delivery_percentage(self):
        records, boards, run = fixture()
        records[1].update(elapsed_ms=433, stop_reason=1)
        result = report.summarize(records, boards, run)
        short = result["nodes"][1]
        self.assertEqual(short["observed_pages"], 0)
        self.assertIsNone(short["delivery_percent"])
        self.assertEqual(short["window_assessment"], "inadequate")
        self.assertTrue(any("Manual stop" in warning for warning in short["warnings"]))
        self.assertIn("INADEQUATE WINDOW", report.svg(result))
        self.assertNotIn("0.0%", report.svg(result))

    def test_source_diagnostics_and_timer_overrun_remain_visible(self):
        records, boards, run = fixture()
        records[0].update(rx_drop=17, elapsed_ms=312639)
        result = report.summarize(records, boards, run)
        source = result["nodes"][0]
        self.assertEqual(source["rx_drop"], 17)
        self.assertIsNone(source["delivery_percent"])
        self.assertTrue(any("12639 ms" in text for text in source["warnings"]))
        self.assertIn("queue drops 17", report.svg(result))
        self.assertIn("| 1 (aaaa) | source |", report.markdown(result))

    def test_reject_missing_incomplete_mixed_or_interrupted_inputs(self):
        records, boards, run = fixture()
        with self.assertRaises(ValueError):
            report.summarize(records[1:], boards, run)
        for index, key, value in [(0, "sent", 4), (1, "run", 43),
                                  (1, "build", "different"), (1, "state", "interrupted")]:
            changed = copy.deepcopy(records)
            changed[index][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                report.summarize(changed, boards, run)

    def test_board_order_uses_physical_labels_and_valid_svg(self):
        records, boards, run = fixture()
        result = report.summarize(list(reversed(records)), boards, run)
        image = report.svg(result)
        ET.fromstring(image)
        self.assertLess(image.index("Board 1 (aaaa)"), image.index("Board 2 (0001)"))
        self.assertEqual(result["nodes"][1]["delivery_percent"], 0)
        self.assertIn("unsynchronised", " ".join(result["limitations"]))

    def test_manifest_hash_and_metadata_validation(self):
        records, boards, run = fixture()
        with tempfile.TemporaryDirectory(prefix="mlan-report-") as temp:
            base = Path(temp)
            for index, record in enumerate(records):
                raw = (json.dumps(record) + "\n").encode()
                name = f"{index}.json"
                (base / name).write_bytes(raw)
                run["records"].append(dict(file=name, original=name,
                                           sha256=hashlib.sha256(raw).hexdigest()))
            manifest = dict(schema=1, kind="mlan-evidence", boards=boards, runs=[run])
            path = base / "runs.json"
            path.write_text(json.dumps(manifest))
            self.assertEqual(report.load_manifest(path)[0]["run"], 42)
            run["run"] = 43
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "metadata"):
                report.load_manifest(path)
            run["run"] = 42
            path.write_text(json.dumps(manifest))
            (base / "1.json").write_text("{}")
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                report.load_manifest(path)

    def test_actual_three_runs_and_separate_retention_evidence(self):
        path = Path(__file__).parents[1] / "data" / "runs.json"
        results = report.load_manifest(path)
        self.assertEqual([r["run"] for r in results], [2026092801, 2026092901, 2026092902])
        self.assertEqual([len(r["nodes"]) for r in results], [2, 5, 5])
        self.assertEqual(results[0]["nodes"][1]["delivery_percent"], 100)
        self.assertTrue(results[0]["observational_evidence"])
        self.assertFalse(results[1]["observational_evidence"])
        self.assertIn("user-reported physical", report.markdown(results[0]))
        self.assertIsNone(results[2]["nodes"][2]["delivery_percent"])
        self.assertEqual(results[2]["nodes"][0]["rx_drop"], 17)
        with tempfile.TemporaryDirectory(prefix="mlan-report-") as temp:
            outputs = report.write_reports(results, temp)
            self.assertEqual(len(outputs), 9)
            for output in outputs:
                if output.suffix == ".svg":
                    ET.parse(output)
            with self.assertRaisesRegex(ValueError, "Output exists"):
                report.write_reports(results, temp)


if __name__ == "__main__":
    unittest.main()
