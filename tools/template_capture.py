#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Template capture tool: normalize a screenshot to 1280x720, crop a template, mask personal
areas (nickname, UID, ...), and save it with metadata.

GUI (default):
  python tools/template_capture.py [SCREENSHOT.png]
    left-drag   select the template crop
    right-drag  add a mask (masked pixels become transparent and are ignored by matching)
    Ctrl+Z      remove the last mask
Batch (no GUI, for scripted captures):
  python tools/template_capture.py SCREENSHOT.png --crop x,y,w,h [--mask x,y,w,h ...]
      --task main_menu --element mail_icon [--lang common] [--commit] [--game-version V] [--force]

Coordinates are in the normalized 1280x720 space. Screenshots from `maba_cli grab` are already
normalized; other 16:9 screenshots are scaled. Templates go to `assets-local/` (gitignored) unless
--commit is given; see assets/PROVENANCE.md before committing game images.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

TOOL_VERSION = "0.1.0"
NORM_W, NORM_H = 1280, 720
ROOT = Path(__file__).resolve().parent.parent
NAME_PART_RE = re.compile(r"^[a-z0-9]+(?:_[a-z0-9]+)*$")


@dataclass(frozen=True)
class Rect:
    x: int
    y: int
    w: int
    h: int

    @staticmethod
    def parse(text: str) -> "Rect":
        parts = text.split(",")
        if len(parts) != 4:
            raise ValueError(f"expected x,y,w,h: {text!r}")
        x, y, w, h = (int(p) for p in parts)
        return Rect(x, y, w, h)

    @staticmethod
    def from_corners(x0: int, y0: int, x1: int, y1: int) -> "Rect":
        return Rect(min(x0, x1), min(y0, y1), abs(x1 - x0), abs(y1 - y0))

    def valid(self) -> bool:
        return self.w > 0 and self.h > 0

    def inside_frame(self) -> bool:
        return self.valid() and self.x >= 0 and self.y >= 0 and \
            self.x + self.w <= NORM_W and self.y + self.h <= NORM_H

    def intersect(self, other: "Rect") -> "Rect":
        x0, y0 = max(self.x, other.x), max(self.y, other.y)
        x1 = min(self.x + self.w, other.x + other.w)
        y1 = min(self.y + self.h, other.y + other.h)
        return Rect(x0, y0, max(0, x1 - x0), max(0, y1 - y0))

    def as_dict(self) -> dict:
        return {"x": self.x, "y": self.y, "w": self.w, "h": self.h}


def is_16_9(width: int, height: int, tolerance: float = 0.01) -> bool:
    return width > 0 and height > 0 and abs(width / height - 16 / 9) <= tolerance


def normalize(img: Image.Image) -> Image.Image:
    """RGB image scaled to 1280x720 (box filter when shrinking, bilinear when enlarging)."""
    if not is_16_9(*img.size):
        raise ValueError(f"screenshot is {img.size[0]}x{img.size[1]}, not 16:9")
    rgb = img.convert("RGB")
    if rgb.size == (NORM_W, NORM_H):
        return rgb
    shrinking = rgb.size[0] > NORM_W
    return rgb.resize((NORM_W, NORM_H), Image.Resampling.BOX if shrinking else Image.Resampling.BILINEAR)


def load_languages(root: Path = ROOT) -> list[str]:
    data = json.loads((root / "config" / "languages.json").read_text(encoding="utf-8"))
    return [lang["code"] for lang in data["languages"]]


def template_name(task: str, element: str) -> str:
    for label, part in (("task", task), ("element", element)):
        if not NAME_PART_RE.match(part):
            raise ValueError(f"{label} must be lowercase letters, digits and underscores: {part!r}")
    return f"{task}_{element}.png"


def output_path(root: Path, lang: str, task: str, element: str, commit: bool,
                languages: list[str]) -> Path:
    if lang != "common" and lang not in languages:
        raise ValueError(f"unknown language {lang!r}; use 'common' or one of {languages}")
    base = "assets" if commit else "assets-local"
    return root / base / lang / template_name(task, element)


def cut_template(frame: Image.Image, crop: Rect, masks: list[Rect]) -> tuple[Image.Image, list[Rect]]:
    """Crop from a normalized frame; masked pixels get alpha 0. Returns the image and the masks
    actually applied (clipped to the crop, in frame coordinates)."""
    if frame.size != (NORM_W, NORM_H):
        raise ValueError("frame must be normalized to 1280x720 first")
    if not crop.inside_frame():
        raise ValueError(f"crop {crop.as_dict()} must lie inside 1280x720")
    out = frame.crop((crop.x, crop.y, crop.x + crop.w, crop.y + crop.h)).convert("RGBA")
    applied = []
    for m in masks:
        clip = m.intersect(crop)
        if not clip.valid():
            continue
        applied.append(clip)
        hole = Image.new("RGBA", (clip.w, clip.h), (0, 0, 0, 0))
        out.paste(hole, (clip.x - crop.x, clip.y - crop.y))
    if applied and out.getchannel("A").getextrema()[1] == 0:
        raise ValueError("masks cover the whole template; nothing left to match")
    return out, applied


def build_metadata(task: str, element: str, lang: str, crop: Rect, masks: list[Rect],
                   source_size: tuple[int, int], game_version: str = "",
                   client: str = "steam-pc-global", today: _dt.date | None = None) -> dict:
    # No source file path here: it could reveal a local user name.
    return {
        "task": task,
        "element": element,
        "lang": lang,
        "roi": crop.as_dict(),
        "masks": [m.as_dict() for m in masks],
        "source_client": client,
        "source_size": {"w": source_size[0], "h": source_size[1]},
        "game_version": game_version,
        "capture_date": (today or _dt.date.today()).isoformat(),
        "tool": f"template_capture.py {TOOL_VERSION}",
    }


def save_template(path: Path, image: Image.Image, metadata: dict, force: bool = False) -> None:
    meta_path = path.with_suffix(".json")
    if not force and (path.exists() or meta_path.exists()):
        raise FileExistsError(f"{path.name} already exists (use --force to replace it)")
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG")
    meta_path.write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n",
                         encoding="utf-8", newline="\n")


def relative(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return path.name


def run_batch(args: argparse.Namespace) -> int:
    source = Image.open(args.screenshot)
    frame = normalize(source)
    crop = Rect.parse(args.crop)
    masks = [Rect.parse(m) for m in args.mask]
    image, applied = cut_template(frame, crop, masks)
    path = output_path(ROOT, args.lang, args.task, args.element, args.commit, load_languages())
    meta = build_metadata(args.task, args.element, args.lang, crop, applied, source.size,
                          args.game_version)
    save_template(path, image, meta, force=args.force)
    print(f"saved {relative(path)} ({crop.w}x{crop.h}, {len(applied)} mask(s))")
    return 0


def run_gui(args: argparse.Namespace) -> int:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    from PIL import ImageTk

    root = tk.Tk()
    root.title("MABA template capture")
    path = args.screenshot or filedialog.askopenfilename(
        title="Screenshot (1280x720 or 16:9)", filetypes=[("PNG", "*.png"), ("Images", "*.png *.jpg *.bmp")])
    if not path:
        return 1
    try:
        source = Image.open(path)
        frame = normalize(source)
    except (OSError, ValueError) as e:
        messagebox.showerror("Cannot open", str(e))
        return 1

    state = {"crop": None, "masks": [], "drag": None, "kind": None}
    photo = ImageTk.PhotoImage(frame)
    canvas = tk.Canvas(root, width=NORM_W, height=NORM_H, highlightthickness=0, cursor="crosshair")
    canvas.grid(row=0, column=0, columnspan=8)
    canvas.create_image(0, 0, anchor="nw", image=photo)

    fields = {}
    for col, (label, default) in enumerate((("task", ""), ("element", ""), ("lang", args.lang),
                                            ("game version", args.game_version))):
        ttk.Label(root, text=label).grid(row=1, column=col * 2, sticky="e", padx=4, pady=6)
        var = tk.StringVar(value=default)
        ttk.Entry(root, textvariable=var, width=16).grid(row=1, column=col * 2 + 1, sticky="w")
        fields[label] = var
    commit_var = tk.BooleanVar(value=args.commit)
    status = tk.StringVar(value="left-drag: crop   right-drag: mask   Ctrl+Z: undo mask")
    ttk.Checkbutton(root, text="save to assets/ (committed)", variable=commit_var).grid(row=2, column=0, columnspan=3, sticky="w", padx=4)
    ttk.Label(root, textvariable=status).grid(row=2, column=3, columnspan=4, sticky="w")

    def clamp(v: int, hi: int) -> int:
        return max(0, min(hi, v))

    def redraw() -> None:
        canvas.delete("overlay")
        if state["crop"]:
            c = state["crop"]
            canvas.create_rectangle(c.x, c.y, c.x + c.w, c.y + c.h, outline="#00e5ff", width=2, tags="overlay")
        for m in state["masks"]:
            canvas.create_rectangle(m.x, m.y, m.x + m.w, m.y + m.h, outline="#ff00aa",
                                    fill="#ff00aa", stipple="gray50", tags="overlay")
        c = state["crop"]
        status.set(f"crop {c.as_dict() if c else '-'}   masks {len(state['masks'])}")

    def press(kind: str):
        def handler(e):
            state["drag"] = (clamp(e.x, NORM_W), clamp(e.y, NORM_H))
            state["kind"] = kind
        return handler

    def motion(e) -> None:
        if not state["drag"]:
            return
        x0, y0 = state["drag"]
        r = Rect.from_corners(x0, y0, clamp(e.x, NORM_W), clamp(e.y, NORM_H))
        canvas.delete("drag")
        canvas.create_rectangle(r.x, r.y, r.x + r.w, r.y + r.h, dash=(4, 2),
                                outline="#ffffff", tags="drag")

    def release(e) -> None:
        if not state["drag"]:
            return
        x0, y0 = state["drag"]
        r = Rect.from_corners(x0, y0, clamp(e.x, NORM_W), clamp(e.y, NORM_H))
        state["drag"] = None
        canvas.delete("drag")
        if r.valid():
            if state["kind"] == "crop":
                state["crop"] = r
            else:
                state["masks"].append(r)
        redraw()

    def undo(_e=None) -> None:
        if state["masks"]:
            state["masks"].pop()
            redraw()

    def save() -> None:
        if not state["crop"]:
            messagebox.showwarning("No crop", "Select the template area first (left-drag).")
            return
        try:
            task, element = fields["task"].get().strip(), fields["element"].get().strip()
            lang = fields["lang"].get().strip()
            image, applied = cut_template(frame, state["crop"], state["masks"])
            out = output_path(ROOT, lang, task, element, commit_var.get(), load_languages())
            meta = build_metadata(task, element, lang, state["crop"], applied, source.size,
                                  fields["game version"].get().strip())
            # Last visual check: exactly what will be saved, masked areas in magenta, enlarged 2x.
            preview = Image.new("RGBA", image.size, (255, 0, 170, 255))
            preview.alpha_composite(image)
            top = tk.Toplevel(root)
            top.title("Preview")
            preview_photo = ImageTk.PhotoImage(
                preview.resize((image.size[0] * 2, image.size[1] * 2), Image.Resampling.NEAREST))
            ttk.Label(top, image=preview_photo).pack(padx=8, pady=8)
            top.update()
            ok = messagebox.askyesno("Save template", f"Save {relative(out)}?\n"
                                     f"{image.size[0]}x{image.size[1]}, {len(applied)} mask(s).\n"
                                     "Check that no nickname, UID or other personal data is visible.",
                                     parent=top)
            top.destroy()
            if not ok:
                return
            save_template(out, image, meta, force=args.force)
            status.set(f"saved {relative(out)}")
        except (ValueError, OSError) as e:
            messagebox.showerror("Cannot save", str(e))

    canvas.bind("<ButtonPress-1>", press("crop"))
    canvas.bind("<ButtonPress-3>", press("mask"))
    canvas.bind("<B1-Motion>", motion)
    canvas.bind("<B3-Motion>", motion)
    canvas.bind("<ButtonRelease-1>", release)
    canvas.bind("<ButtonRelease-3>", release)
    root.bind("<Control-z>", undo)
    ttk.Button(root, text="Undo mask", command=undo).grid(row=2, column=6, sticky="e")
    ttk.Button(root, text="Save", command=save).grid(row=2, column=7, sticky="e", padx=4)
    root.mainloop()
    return 0


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("screenshot", nargs="?", help="screenshot PNG (normalized or any 16:9 size)")
    p.add_argument("--crop", help="batch mode: template area x,y,w,h (1280x720 space)")
    p.add_argument("--mask", action="append", default=[], help="area to mask x,y,w,h (repeatable)")
    p.add_argument("--task", help="task or page name, e.g. main_menu")
    p.add_argument("--element", help="element name, e.g. mail_icon")
    p.add_argument("--lang", default="common", help="'common' for language-neutral, else a code from config/languages.json")
    p.add_argument("--commit", action="store_true", help="save under assets/ instead of assets-local/")
    p.add_argument("--game-version", default="", help="game client version, recorded in metadata")
    p.add_argument("--force", action="store_true", help="replace an existing template")
    args = p.parse_args(argv)
    try:
        if args.crop:
            if not (args.screenshot and args.task and args.element):
                p.error("batch mode needs SCREENSHOT, --task and --element")
            return run_batch(args)
        return run_gui(args)
    except (ValueError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
