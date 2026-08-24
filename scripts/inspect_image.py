#!/usr/bin/env python3
"""Inspect the scaled image and save a preview for analysis."""
from PIL import Image
import os

src = '/Users/lemonliu/IOT/STM32_H743_ALIENTEK_CubeIDE/Core/Src/icon_resource/bottom_bar/bottom_label.png'
img = Image.open(src).convert('RGBA')
w, h = img.size
print(f"Original: {w}x{h}")

# Scale to width=320
new_w = 320
new_h = round(h * new_w / w)
img_scaled = img.resize((new_w, new_h), Image.LANCZOS)
print(f"Scaled: {new_w}x{new_h}")

# Save preview
out = '/tmp/bottom_label_scaled.png'
img_scaled.save(out)
print(f"Preview saved to {out}")

# Analyze the image - print pixel values at key positions
pixels = img_scaled.load()
print(f"\nPixel analysis (RGB + alpha):")

# Bottom-right corner area (where "豆包AI生成" likely is)
print(f"\n--- Bottom-right corner ---")
for y in range(new_h-20, new_h):
    for x in range(new_w-80, new_w-5, 10):
        r, g, b, a = pixels[x, y]
        if r < 200 or g < 200 or b < 200:  # Non-white pixel
            print(f"  ({x},{y}): RGB({r},{g},{b}) alpha={a}")

# Middle area (where the large number likely is)
print(f"\n--- Middle area ---")
for y in range(new_h//3, 2*new_h//3):
    for x in range(new_w//4, 3*new_w//4, 20):
        r, g, b, a = pixels[x, y]
        if r < 200 or g < 200 or b < 200:  # Non-white/non-transparent pixel
            print(f"  ({x},{y}): RGB({r},{g},{b}) alpha={a}")