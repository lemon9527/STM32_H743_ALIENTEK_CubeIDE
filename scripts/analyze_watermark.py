#!/usr/bin/env python3
"""Detailed analysis of the bottom-right area to find the 豆包AI生成 watermark."""
from PIL import Image
from collections import Counter

src = '/Users/lemonliu/IOT/STM32_H743_ALIENTEK_CubeIDE/Core/Src/icon_resource/bottom_bar/bottom_label.png'
img = Image.open(src).convert('RGBA')
w, h = img.size

new_w = 320
new_h = round(h * new_w / w)
img_scaled = img.resize((new_w, new_h), Image.LANCZOS)
pixels = img_scaled.load()

# For each row in the bottom area, show which columns have non-black pixels
print("--- Bottom-right area: non-black pixels by row and column range ---")
for y in range(90, new_h):
    cols = []
    for x in range(200, new_w):
        r, g, b, a = pixels[x, y]
        if r > 15 or g > 15 or b > 15:  # non-black
            cols.append(x)
    if cols:
        # Find contiguous ranges
        ranges = []
        start = cols[0]
        end = cols[0]
        for c in cols[1:]:
            if c == end + 1:
                end = c
            else:
                ranges.append((start, end))
                start = c
                end = c
        ranges.append((start, end))
        range_str = ", ".join(f"{s}-{e}" if s != e else f"{s}" for s, e in ranges)
        print(f"  y={y:3d}: {range_str} ({len(cols)} px)")

# Also check if there's text at the very bottom-right
print(f"\n--- Bottom-right corner pixel values ---")
for y in range(new_h-20, new_h):
    for x in range(new_w-60, new_w):
        r, g, b, a = pixels[x, y]
        if r > 20 or g > 20 or b > 20:
            print(f"  ({x},{y}): RGB({r},{g},{b}) alpha={a}")