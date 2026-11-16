"""Checks that the manual analyzer rejects injected capture defects."""
import argparse
import array
import contextlib
import csv
import io
import json
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
import limiter_audio as audio
from limiter_oracle import source_sample


class AnalysisTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.parent = Path(os.environ.get("LIMITER_AUDIO_TEST_DIR", audio.ROOT / "build/scratch/limiter-audio-analysis"))
        cls.created_parent = not cls.parent.exists()
        cls.parent.mkdir(parents=True, exist_ok=True)
        cls.scratch = tempfile.TemporaryDirectory(dir=cls.parent)

    @classmethod
    def tearDownClass(cls):
        cls.scratch.cleanup()
        if cls.created_parent:
            cls.parent.rmdir()

    def setUp(self):
        self.folder = Path(self.scratch.name) / self._testMethodName
        self.folder.mkdir()
        self.rate, self.offset = 8000, 2000
        run = {"rate": self.rate, "period": 128, "gain": 0.3, "source": "multitone", "mode": "static"}
        self.reference = array.array("f")
        for i in range(self.rate * 6):
            self.reference.extend(source_sample(run, i - math.ceil(0.005 * self.rate)))
        self.capture = array.array("f", [0] * (2 * self.offset))
        self.capture.extend(value * (0.5 if i % 2 == 0 else 0.25) for i, value in enumerate(self.reference))
        self.capture.extend([0] * (2 * self.offset))
        self.args = argparse.Namespace(output=self.folder, capture=None, min_snr=60, max_error=0.002)
        (self.folder / "run.json").write_text(json.dumps(run))
        (self.folder / "audio-events.csv").write_text("before_frame,after_frame,setting,value,setter_ns\n")
        with (self.folder / "audio-callbacks.csv").open("w", newline="") as file:
            writer = csv.writer(file)
            writer.writerow(("frame", "frames", "status", "render_ns", "capture_ns", "interval_ns"))
            for frame in range(0, len(self.reference) // 2, 128):
                writer.writerow((frame, 128, 0, 1000, 2000, 16000000))

    def analyze(self):
        for name, values in (("audio-rendered.f32", self.reference), ("capture.f32", self.capture)):
            copy = values[:]
            if sys.byteorder != "little":
                copy.byteswap()
            (self.folder / name).write_bytes(copy.tobytes())
        with contextlib.redirect_stdout(io.StringIO()):
            status = audio.analyze(self.args)
        return status, json.loads((self.folder / "analysis.json").read_text())

    def test_alignment_and_fixed_channel_gains(self):
        status, report = self.analyze()
        self.assertEqual(status, 0)
        self.assertEqual(report["comparison"]["offset_frames"], self.offset)
        self.assertTrue(report["comparison"]["exact_match_after_alignment_and_gain"])
        self.assertEqual(report["comparison"]["fixed_monitor_gains"], [0.5, 0.25])

    def test_monitor_silence(self):
        begin = 2 * (self.offset + self.rate * 3)
        self.capture[begin:begin + 640] = array.array("f", [0] * 640)
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertFalse(report["checks"]["monitor_matches_rendered_output"])
        self.assertFalse(report["checks"]["no_detected_monitor_dropout"])

    def test_repeated_frames(self):
        begin = 2 * (self.offset + self.rate * 3)
        self.capture[begin:begin] = self.capture[begin - 128:begin]
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertFalse(report["checks"]["monitor_matches_rendered_output"])
        self.assertEqual({row["offset_frames"] for row in report["comparison"]["local_offsets"]},
                         {self.offset, self.offset + 64})

    def test_gap_already_in_reference(self):
        begin = 2 * self.rate * 3
        self.reference[begin:begin + 640] = array.array("f", [0] * 640)
        begin += 2 * self.offset
        self.capture[begin:begin + 640] = array.array("f", [0] * 640)
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertTrue(report["checks"]["monitor_matches_rendered_output"])
        self.assertFalse(report["checks"]["no_detected_rendered_silence_gap"])

    def test_deleted_frames(self):
        begin = 2 * (self.offset + self.rate * 3)
        del self.capture[begin:begin + 128]
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertFalse(report["checks"]["monitor_matches_rendered_output"])
        self.assertEqual({row["offset_frames"] for row in report["comparison"]["local_offsets"]},
                         {self.offset, self.offset - 64})

    def test_single_sample_click(self):
        self.capture[2 * (self.offset + self.rate * 3)] += 0.5
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertFalse(report["checks"]["monitor_matches_rendered_output"])

    def test_both_recordings_have_wrong_gain(self):
        self.reference = array.array("f", (v * 0.9 for v in self.reference))
        self.capture = array.array("f", (v * 0.9 for v in self.capture))
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertTrue(report["checks"]["monitor_matches_rendered_output"])
        self.assertFalse(report["checks"]["limiter_analytic_waveform_or_steady_gain_matches"])

    def test_both_recordings_have_wrong_delay(self):
        self.reference = array.array("f", [0] * 128) + self.reference[:-128]
        self.capture = array.array("f", [0] * (self.offset * 2))
        self.capture.extend(v * (0.5 if i % 2 == 0 else 0.25) for i, v in enumerate(self.reference))
        self.capture.extend([0] * (self.offset * 2))
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertTrue(report["checks"]["monitor_matches_rendered_output"])
        self.assertFalse(report["checks"]["limiter_analytic_waveform_or_steady_gain_matches"])

    def make_delay_transition(self, fade_ms):
        run = json.loads((self.folder / "run.json").read_text())
        run["mode"] = "delay"
        (self.folder / "run.json").write_text(json.dumps(run))
        (self.folder / "audio-events.csv").write_text(
            "before_frame,after_frame,setting,value,setter_ns\n16000,16128,synth.limiter.attack,250,1000\n")
        start = 16000 + self.rate // 4
        fade = round(fade_ms * 0.001 * self.rate)
        self.reference = array.array("f")
        for frame in range(self.rate * 6):
            alpha = max(0, min(1, (frame - start + 1) / fade))
            old, new = source_sample(run, frame - 40), source_sample(run, frame - self.rate // 4)
            self.reference.extend(old[c] + alpha * (new[c] - old[c]) for c in range(2))
        self.capture = array.array("f", [0] * (self.offset * 2))
        self.capture.extend(v * (0.5 if i % 2 == 0 else 0.25) for i, v in enumerate(self.reference))
        self.capture.extend([0] * (self.offset * 2))

    def test_valid_isolated_delay_fade(self):
        self.make_delay_transition(20)
        status, report = self.analyze()
        self.assertEqual(status, 0)
        self.assertEqual(report["limiter_oracle"]["waveform_coverage_fraction"], 1)

    def test_both_recordings_have_wrong_fade_duration(self):
        self.make_delay_transition(40)
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertTrue(report["checks"]["monitor_matches_rendered_output"])
        self.assertFalse(report["checks"]["limiter_isolated_crossfade_coefficient_is_valid"])

    def make_dc(self, amplitude):
        run = json.loads((self.folder / "run.json").read_text())
        run["source"] = "dc"
        (self.folder / "run.json").write_text(json.dumps(run))
        self.reference = array.array("f", (v for frame in range(self.rate * 6)
                                          for v in ((amplitude, -amplitude / 2) if frame >= 40 else (0, 0))))
        self.capture = array.array("f", [0] * (self.offset * 2))
        self.capture.extend(v * (0.5 if i % 2 == 0 else 0.25) for i, v in enumerate(self.reference))
        self.capture.extend([0] * (self.offset * 2))

    def test_steady_dc_limit(self):
        self.make_dc(0.35)
        status, report = self.analyze()
        self.assertEqual(status, 0)

    def test_both_recordings_have_wrong_limit(self):
        self.make_dc(0.30)
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertTrue(report["checks"]["monitor_matches_rendered_output"])
        self.assertFalse(report["checks"]["limiter_analytic_waveform_or_steady_gain_matches"])

    def test_render_budget_exceeded(self):
        path = self.folder / "audio-callbacks.csv"
        path.write_text(path.read_text().replace("128,0,1000,2000", "128,0,20000000,20001000"))
        status, report = self.analyze()
        self.assertEqual(status, 1)
        self.assertFalse(report["checks"]["render_calls_within_frame_budget"])

    def test_nonfinite_capture_rejected(self):
        self.capture[self.rate * 3] = float("nan")
        with self.assertRaisesRegex(ValueError, "Nonfinite"):
            self.analyze()


if __name__ == "__main__":
    unittest.main()
