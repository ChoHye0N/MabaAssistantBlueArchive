# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""OCR sidecar for MABA Core: one JSON object per line on stdin/stdout.

Protocol (docs/ARCHITECTURE.md, "OCR sidecar protocol"):
  -> {"ready": true, "engine": "rapidocr", "version": "3.10.0"}            once, at start
  <- {"id": 1, "op": "recognize", "lang": "korean", "width": W, "height": H,
      "format": "bgr24", "data": "<base64>", "detect": true}
  -> {"id": 1, "ok": true, "results": [{"text": "120", "box": [x, y, w, h], "score": 0.99}]}
  <- {"id": 2, "op": "ping"}       -> {"id": 2, "ok": true}
  <- {"id": 3, "op": "shutdown"}   -> {"id": 3, "ok": true}, then exit
Errors never end the loop: {"id": n, "ok": false, "error": "..."}.

"detect": false skips text detection and recognizes the whole image as one line (fast; good
for tight digit crops). The default runs detection + recognition and returns boxes.

Engines: "rapidocr" (RapidOCR on ONNX Runtime with PaddleOCR models; downloaded on first use
into the rapidocr package folder) and "fake" (for tests; answers with --fake-text).
"""

from __future__ import annotations

import argparse
import base64
import json
import sys
from typing import Any, Callable, Dict, List, Optional, TextIO

PROTOCOL_VERSION = 1


class FakeEngine:
    name = "fake"
    version = "1"

    def __init__(self, text: str) -> None:
        self.text = text

    def recognize(self, image: Any, width: int, height: int, lang: str,
                  detect: bool) -> List[Dict[str, Any]]:
        return [{"text": self.text, "box": [0, 0, width, height], "score": 1.0}]


class RapidOcrEngine:
    """RapidOCR with one engine per recognition language, created lazily."""

    name = "rapidocr"

    # Model routes tried in order per language; the first one RapidOCR knows is used.
    # PP-OCRv5 mobile has dedicated Korean, English and Thai models; Traditional Chinese and
    # Japanese have dedicated models only in PP-OCRv4.
    ROUTES = (("PPOCRV5", "MOBILE"), ("PPOCRV4", "MOBILE"), ("PPOCRV6", "SMALL"))

    def __init__(self) -> None:
        import importlib.metadata

        import numpy  # noqa: F401  (fail early with a clear message if missing)
        from rapidocr import ModelType, OCRVersion, RapidOCR

        self._rapid = RapidOCR
        self._versions = OCRVersion
        self._types = ModelType
        self._engines: Dict[str, Any] = {}
        self.version = importlib.metadata.version("rapidocr")

    def _engine(self, lang: str) -> Any:
        if lang in self._engines:
            return self._engines[lang]
        errors = []
        for version, model_type in self.ROUTES:
            params = {
                "Rec.lang_type": lang,
                "Rec.ocr_version": getattr(self._versions, version),
                "Rec.model_type": getattr(self._types, model_type),
                "Global.log_level": "error",
            }
            try:
                engine = self._rapid(params=params)
                # Model files are resolved on first use: probe with a tiny image.
                import numpy as np
                engine(np.full((32, 64, 3), 255, np.uint8), use_det=False, use_cls=False,
                       use_rec=True)
            except Exception as e:  # unsupported route: try the next one
                errors.append(f"{version}/{model_type}: {e}")
                continue
            self._engines[lang] = engine
            return engine
        raise ValueError(f"no recognition model for language '{lang}': " + "; ".join(errors))

    def recognize(self, image: Any, width: int, height: int, lang: str,
                  detect: bool) -> List[Dict[str, Any]]:
        engine = self._engine(lang)
        if not detect:
            out = engine(image, use_det=False, use_cls=False, use_rec=True)
            return [{"text": str(t), "box": [0, 0, width, height], "score": float(s)}
                    for t, s in zip(out.txts or (), out.scores or ())]
        out = engine(image, use_det=True, use_cls=False, use_rec=True)
        results = []
        boxes = out.boxes if out.boxes is not None else []
        for quad, text, score in zip(boxes, out.txts or (), out.scores or ()):
            xs = [float(p[0]) for p in quad]
            ys = [float(p[1]) for p in quad]
            x, y = max(int(min(xs)), 0), max(int(min(ys)), 0)
            w = max(int(round(max(xs))) - x, 1)
            h = max(int(round(max(ys))) - y, 1)
            results.append({"text": str(text), "box": [x, y, w, h], "score": float(score)})
        return results


def decode_image(req: Dict[str, Any]) -> Any:
    import numpy as np

    width, height = int(req["width"]), int(req["height"])
    if req.get("format", "bgr24") != "bgr24":
        raise ValueError("only format 'bgr24' is supported")
    if width <= 0 or height <= 0 or width * height > 4096 * 4096:
        raise ValueError("bad image size")
    data = base64.b64decode(req["data"], validate=True)
    if len(data) != width * height * 3:
        raise ValueError(f"expected {width * height * 3} bytes, got {len(data)}")
    return np.frombuffer(data, np.uint8).reshape(height, width, 3)


def handle(req: Dict[str, Any], engine: Any) -> Dict[str, Any]:
    op = req.get("op")
    rid = req.get("id")
    if op == "ping" or op == "shutdown":
        return {"id": rid, "ok": True}
    if op != "recognize":
        return {"id": rid, "ok": False, "error": f"unknown op: {op!r}"}
    lang = req.get("lang")
    if not isinstance(lang, str) or not lang:
        return {"id": rid, "ok": False, "error": "missing 'lang'"}
    image = decode_image(req)
    results = engine.recognize(image, int(req["width"]), int(req["height"]), lang,
                               bool(req.get("detect", True)))
    return {"id": rid, "ok": True, "results": results}


def serve(engine: Any, stdin: TextIO, out: Callable[[Dict[str, Any]], None]) -> int:
    out({"ready": True, "engine": engine.name, "version": engine.version,
         "protocol": PROTOCOL_VERSION})
    for line in stdin:
        line = line.strip()
        if not line:
            continue
        rid: Optional[Any] = None
        try:
            req = json.loads(line)
            if not isinstance(req, dict):
                raise ValueError("request must be a JSON object")
            rid = req.get("id")
            res = handle(req, engine)
        except Exception as e:  # report and keep serving
            res = {"id": rid, "ok": False, "error": f"{type(e).__name__}: {e}"}
        out(res)
        if res.get("ok") and isinstance(req, dict) and req.get("op") == "shutdown":
            break
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--engine", choices=["rapidocr", "fake"], default="rapidocr")
    parser.add_argument("--fake-text", default="0")
    args = parser.parse_args(argv)

    # The protocol owns stdout. Anything else (library logs, prints) goes to stderr.
    protocol = sys.stdout
    protocol.reconfigure(encoding="utf-8", newline="\n")
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout = sys.stderr

    def out(obj: Dict[str, Any]) -> None:
        protocol.write(json.dumps(obj, ensure_ascii=False) + "\n")
        protocol.flush()

    try:
        engine = FakeEngine(args.fake_text) if args.engine == "fake" else RapidOcrEngine()
    except Exception as e:
        out({"error": f"{type(e).__name__}: {e}"})
        return 1
    return serve(engine, sys.stdin, out)


if __name__ == "__main__":
    sys.exit(main())
