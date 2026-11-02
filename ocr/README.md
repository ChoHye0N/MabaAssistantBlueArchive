# OCR sidecar (Python, RapidOCR)

`server.py` implements the OCR side of `IOcr` (`core/include/maba/ocr_sidecar.h`): Core starts it as a child process and talks one JSON object per line over stdin/stdout. Protocol: `docs/ARCHITECTURE.md`, "OCR sidecar protocol".

Engine: RapidOCR on ONNX Runtime with PaddleOCR models. Same models as PaddleOCR, without the PaddlePaddle framework; the ONNX files can later run natively in C++ through ONNX Runtime so users need no Python.

## Setup
```
python -m venv ocr/.venv
ocr/.venv/Scripts/python -m pip install -r ocr/requirements.txt
```
Core uses `ocr/.venv` automatically (`DefaultOcrCommand`), then the Python runtime bundled with a release (`ocr/python/`), otherwise `python` on PATH. The first request per language downloads its models (a few MB to ~20 MB) into the rapidocr package folder.

## Languages
`config/languages.json` maps each game language to a recognition model code (`ocr_lang`). The sidecar tries PP-OCRv5 mobile, then PP-OCRv4 mobile, then PP-OCRv6 small, and uses the first model RapidOCR has:

| Game language | `ocr_lang` | Model used |
|---|---|---|
| Korean | `korean` | PP-OCRv5 mobile (verified on synthetic digits: `120/240`, `1,234`) |
| English | `en` | PP-OCRv5 mobile |
| Thai | `th` | PP-OCRv5 mobile |
| Traditional Chinese | `chinese_cht` | PP-OCRv4 mobile |
| Japanese | `japan` | PP-OCRv4 mobile |

Accuracy on real game crops is not measured yet (roadmap M3). Read digits where possible: they are language-independent.

## Tests
```
python -m unittest discover ocr/tests                        # fake engine, needs numpy
MABA_OCR_MODEL_TESTS=1 python -m unittest discover ocr/tests # also runs RapidOCR (downloads models)
```
`maba_core_tests` runs the C++ client against this server (fake engine) when `MABA_PYTHON_TESTS=1`.

Licenses: RapidOCR and the PaddleOCR models are Apache-2.0, ONNX Runtime is MIT; the full list of installed packages is in `THIRD_PARTY_NOTICES.md`.
