#!/usr/bin/env python3
"""
Convert clean_text.png to raw RGB565 + alpha binary files for firmware embedding.
Scales width to 140px, maintains aspect ratio.
Makes white background transparent, keeps black text opaque.
"""
from PIL import Image
import struct
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)

IN_PATH = os.path.join(PROJECT_DIR, "Core", "Src", "clean_text.png")
OUT_RGB = os.path.join(PROJECT_DIR, "Core", "Src", "clean_text_rgb.raw")
OUT_ALPHA = os.path.join(PROJECT_DIR, "Core", "Src", "clean_text_alpha.raw")

TARGET_WIDTH = 140

img = Image.open(IN_PATH)
orig_w, orig_h = img.size
TARGET_HEIGHT = int(round((orig_h * TARGET_WIDTH) / orig_w))
print(f"Original: {orig_w}x{orig_h}")
print(f"Scaled: {TARGET_WIDTH}x{TARGET_HEIGHT}")

img = img.resize((TARGET_WIDTH, TARGET_HEIGHT), Image.LANCZOS)

# Convert to RGBA
if img.mode != "RGBA":
    img = img.convert("RGBA")

# Process pixels: white text on black background
# Black (background) -> transparent (alpha=0)
# White (text) -> opaque (alpha=255)
# Gray (anti-aliased) -> intermediate alpha proportionate to brightness
processed_pixels = []
for (r, g, b, a) in img.getdata():
    # Luminance: bright = text (opaque), dark = background (transparent)
    brightness = (r * 77 + g * 150 + b * 29) // 256  # 0-255 weighted luminance
    # Scale alpha: bright -> 255, dark -> 0
    a_out = brightness
    processed_pixels.append((r, g, b, a_out))

# Count transparent vs opaque
opaque = sum(1 for _, _, _, a in processed_pixels if a > 128)
transparent = sum(1 for _, _, _, a in processed_pixels if a <= 128)
print(f"Processed: {transparent} transparent pixels, {opaque} opaque text pixels")

# Write RGB565 pixels
with open(OUT_RGB, "wb") as f:
    for (r, g, b, a) in processed_pixels:
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        f.write(struct.pack("<H", rgb565))

# Write alpha (1 byte per pixel, 0-255)
with open(OUT_ALPHA, "wb") as f:
    for (r, g, b, a) in processed_pixels:
        f.write(bytes([a]))

print(f"RGB565 data:  {OUT_RGB} ({os.path.getsize(OUT_RGB)} bytes)")
print(f"Alpha data:   {OUT_ALPHA} ({os.path.getsize(OUT_ALPHA)} bytes)")
print(f"Dimensions:   {TARGET_WIDTH}x{TARGET_HEIGHT}")