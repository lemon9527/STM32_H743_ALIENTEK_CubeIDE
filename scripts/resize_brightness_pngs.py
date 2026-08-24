#!/usr/bin/env python3
"""
Resize brightness PNG images for LVGL display.
- Icon PNGs (ic_*): scale to height 59px (maintain aspect ratio)
- Text PNGs (txt_*): scale to target height (maintain aspect ratio)
  - High -> height 45
  - Medium -> height 35
  - Off -> height 35
- Skips txt_brightness_title.png (already sized)

Usage:
    python3 scripts/resize_brightness_pngs.py
"""

import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BRIGHTNESS_DIR = os.path.join(
    SCRIPT_DIR, "..", "Core", "Src", "icon_resource", "brightness"
)

ICON_TARGET_H = 59

# Target height for each text type (maintain aspect ratio)
TEXT_TARGET_HEIGHTS = {
    "txt_brightness_high_normal.png":   45,
    "txt_brightness_high_active.png":   45,
    "txt_brightness_medium_normal.png": 35,
    "txt_brightness_medium_active.png": 35,
    "txt_brightness_off_normal.png":    35,
    "txt_brightness_off_active.png":    35,
}


def resize_png(filename: str):
    """Resize a PNG to target height, maintaining aspect ratio."""
    path = os.path.join(BRIGHTNESS_DIR, filename)
    img = Image.open(path)
    orig_w, orig_h = img.size

    if filename.startswith("ic_"):
        target_h = ICON_TARGET_H
        reason = f"icon height {target_h}"
    elif filename in TEXT_TARGET_HEIGHTS:
        target_h = TEXT_TARGET_HEIGHTS[filename]
        reason = f"text height {target_h}"
    else:
        print(f"  SKIP {filename} (no resize rule)")
        return

    target_w = int(round((orig_w * target_h) / orig_h))

    if orig_w == target_w and orig_h == target_h:
        print(f"  {filename}: already {target_w}x{target_h}, skipped")
        return

    print(f"  {filename}: {orig_w}x{orig_h} -> {target_w}x{target_h} ({reason})")
    img = img.resize((target_w, target_h), Image.LANCZOS)
    img.save(path)


def main():
    print(f"Brightness PNG directory: {BRIGHTNESS_DIR}")
    print()

    png_files = sorted([
        f for f in os.listdir(BRIGHTNESS_DIR)
        if f.endswith(".png") and f != "txt_brightness_title.png"
    ])

    if not png_files:
        print("No PNG files found!")
        return

    for fname in png_files:
        resize_png(fname)

    print()
    print("Done. All images resized.")


if __name__ == "__main__":
    main()