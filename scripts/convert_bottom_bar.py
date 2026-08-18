#!/usr/bin/env python3
"""
Convert Bottom_bar.png to raw RGB565 binary for firmware embedding.
Crop 3px from left and right, then scale to width 320 maintaining aspect ratio.
"""
from PIL import Image
import struct
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)

IN_PATH = os.path.join(PROJECT_DIR, "Core", "Src", "Bottom_bar.png")
OUT_RGB = os.path.join(PROJECT_DIR, "Core", "Src", "bottom_bar.raw")

TARGET_WIDTH = 320
CROP_PX = 3

img = Image.open(IN_PATH)
print(f"Original: {img.size[0]}x{img.size[1]} mode={img.mode}")

# Crop 3px from left and right
cropped = img.crop((CROP_PX, 0, img.size[0] - CROP_PX, img.size[1]))
print(f"After crop: {cropped.size[0]}x{cropped.size[1]}")

# Scale to width 320
new_w = TARGET_WIDTH
new_h = int(round((cropped.size[1] * new_w) / cropped.size[0]))
scaled = cropped.resize((new_w, new_h), Image.LANCZOS)
print(f"After scale: {scaled.size[0]}x{scaled.size[1]}")

# Convert to RGBA
if scaled.mode != "RGBA":
    scaled = scaled.convert("RGBA")

# Composite onto black background
bg = Image.new("RGBA", scaled.size, (0, 0, 0, 255))
composite = Image.alpha_composite(bg, scaled)

# Convert to RGB565 and write
pixels = list(composite.getdata())
with open(OUT_RGB, "wb") as f:
    for (r, g, b, a) in pixels:
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        f.write(struct.pack("<H", rgb565))

print(f"RGB565 data: {OUT_RGB} ({os.path.getsize(OUT_RGB)} bytes)")
print(f"Dimensions:  {new_w}x{new_h}")