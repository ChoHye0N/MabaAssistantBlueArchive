# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Sidecar protocol tests. The fake engine needs only numpy; the RapidOCR test runs when
rapidocr is installed (python -m unittest discover ocr/tests)."""

import base64
import io
import json
import os
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER = os.path.join(os.path.dirname(HERE), "server.py")
sys.path.insert(0, os.path.dirname(HERE))

import server  # noqa: E402


def request(rid, width=4, height=2, lang="korean", data=None, **extra):
    raw = data if data is not None else bytes(width * height * 3)
    req = {"id": rid, "op": "recognize", "lang": lang, "width": width, "height": height,
           "format": "bgr24", "data": base64.b64encode(raw).decode()}
    req.update(extra)
    return json.dumps(req)


class ServeTests(unittest.TestCase):
    def run_lines(self, lines, engine=None):
        out = []
        server.serve(engine or server.FakeEngine("42"), io.StringIO("\n".join(lines) + "\n"),
                     out.append)
        return out

    def test_ready_then_results(self):
        out = self.run_lines([request(1)])
        self.assertTrue(out[0]["ready"])
        self.assertEqual(out[0]["engine"], "fake")
        self.assertEqual(out[1], {"id": 1, "ok": True,
                                  "results": [{"text": "42", "box": [0, 0, 4, 2], "score": 1.0}]})

    def test_errors_do_not_stop_the_loop(self):
        out = self.run_lines([
            "not json",
            request(2, data=b"\x00"),            # wrong size
            json.dumps({"id": 3, "op": "fly"}),
            request(4, lang=""),
            json.dumps({"id": 5, "op": "ping"}),
        ])
        self.assertEqual([o.get("ok") for o in out[1:]], [False, False, False, False, True])
        self.assertIsNone(out[1]["id"])
        self.assertIn("expected 24 bytes", out[2]["error"])
        self.assertEqual(out[5]["id"], 5)

    def test_shutdown_stops(self):
        out = self.run_lines([json.dumps({"id": 1, "op": "shutdown"}), request(2)])
        self.assertEqual(len(out), 2)

    def test_process_end_to_end(self):
        proc = subprocess.run(
            [sys.executable, SERVER, "--engine", "fake", "--fake-text", "한글 7"],
            input=(request(9) + "\n").encode("utf-8"), capture_output=True, timeout=60)
        lines = proc.stdout.decode("utf-8").splitlines()
        self.assertEqual(proc.returncode, 0, proc.stderr.decode(errors="replace"))
        self.assertTrue(json.loads(lines[0])["ready"])
        self.assertEqual(json.loads(lines[1])["results"][0]["text"], "한글 7")


try:
    import rapidocr  # noqa: F401
    HAVE_RAPIDOCR = True
except ImportError:
    HAVE_RAPIDOCR = False


@unittest.skipUnless(HAVE_RAPIDOCR and os.environ.get("MABA_OCR_MODEL_TESTS") == "1",
                     "set MABA_OCR_MODEL_TESTS=1 with rapidocr installed (downloads models)")
class RapidOcrTests(unittest.TestCase):
    def test_reads_digits(self):
        import cv2
        import numpy as np

        img = np.full((60, 240, 3), 255, np.uint8)
        cv2.putText(img, "120/240", (8, 45), cv2.FONT_HERSHEY_SIMPLEX, 1.4, (0, 0, 0), 3)
        engine = server.RapidOcrEngine()
        for detect in (False, True):
            results = engine.recognize(img, 240, 60, "korean", detect)
            self.assertEqual("".join(r["text"] for r in results), "120/240")


if __name__ == "__main__":
    unittest.main()
