# OCR sidecar (Python, PaddleOCR) - planned

Implements `IOcr` (`core/include/maba/ocr.h`) as a local process reached over IPC (named pipe, gRPC or localhost HTTP: decide at M3).

- Input: a normalized 1280x720 frame (or an encoded crop), an ROI and a language code from `config/languages.json`.
- Output: list of `{text, box, confidence}`.
- Pin dependency versions in `requirements.txt` when created; check each license (PaddleOCR code and models) and update the README table.
- Check PaddleOCR model availability for each language (Korean, English, Traditional Chinese, Thai) before promising support.
- Later: export models to ONNX and run them in C++ so users need no Python runtime. Keep Python for training and tooling.
