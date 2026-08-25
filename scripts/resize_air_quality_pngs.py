#!/usr/bin/env python3
"""
Resize air_quality PNG images to 53x53 (overwrite).
Usage: python3 scripts/resize_air_quality_pngs.py
"""
import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
AQ_DIR = os.path.join(SCRIPT_DIR, "..", "Core", "Src", "icon_resource", "air_quality")
TARGET_W = TARGET_H = 53


def main():
    files = sorted([f for f in os.listdir(AQ_DIR) if f.endswith('.png')])
    if not files:
        print("No PNG files found in", AQ_DIR)
        return
    for fname in files:
        path = os.path.join(AQ_DIR, fname)
        img = Image.open(path).convert('RGBA')
        w, h = img.size
        if (w, h) == (TARGET_W, TARGET_H):
            print(f"{fname}: already {TARGET_W}x{TARGET_H}")
            continue
        img = img.resize((TARGET_W, TARGET_H), Image.LANCZOS)
        img.save(path)
        print(f"Resized {fname}: {w}x{h} -> {TARGET_W}x{TARGET_H}")

if __name__ == '__main__':
    main()
