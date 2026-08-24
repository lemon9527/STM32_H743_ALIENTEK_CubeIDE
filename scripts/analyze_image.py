#!/usr/bin/env python3
"""Analyze the new scaled image to find text regions."""
from PIL import Image
import os
from collections import Counter

src = '/Users/lemonliu/IOT/STM32_H743_ALIENTEK_CubeIDE/Core/Src/icon_resource/bottom_bar/bottom_label.png'
img = Image.open(src).convert('RGBA')
w, h = img.size

# Scale to width=320
new_w = 320
new_h = round(h * new_w / w)
img_scaled = img.resize((new_w, new_h), Image.LANCZOS)
pixels = img_scaled.load()

# Save a raw copy for analysis
img_scaled.save('/tmp/bottom_label_320.png')

# Find background color (most common pixel in corners)
corner_colors = []
for x in range(0, 20):
    for y in range(0, 20):
        corner_colors.append(pixels[x, y])
    for y in range(new_h-20, new_h):
        corner_colors.append(pixels[x, y])
for x in range(new_w-20, new_w):
    for y in range(0, 20):
        corner_colors.append(pixels[x, y])
    for y in range(new_h-20, new_h):
        corner_colors.append(pixels[x, y])

bg_color = Counter(corner_colors).most_common(1)[0][0]
print(f"Background color: RGB({bg_color[0]},{bg_color[1]},{bg_color[2]}) alpha={bg_color[3]}")
bg_r, bg_g, bg_b, bg_a = bg_color

threshold = 30

# Scan all non-background pixels row by row
print(f"\n--- All non-background pixels by row ---")
row_counts = {}
for y in range(new_h):
    count = 0
    for x in range(new_w):
        r, g, b, a = pixels[x, y]
        if abs(r - bg_r) > threshold or abs(g - bg_g) > threshold or abs(b - bg_b) > threshold:
            count += 1
    if count > 0:
        row_counts[y] = count

for y in sorted(row_counts.keys()):
    print(f"  y={y}: {row_counts[y]} text pixels")

# Analyze bottom-right area (y>=100, x>=230)
print(f"\n--- Bottom-right: non-background pixels (y>=100, x>=230) ---")
text_pixels_bottom = []
for y in range(100, new_h):
    for x in range(230, new_w):
        r, g, b, a = pixels[x, y]
        if abs(r - bg_r) > threshold or abs(g - bg_g) > threshold or abs(b - bg_b) > threshold:
            text_pixels_bottom.append((x, y, r, g, b))

if text_pixels_bottom:
    min_x = min(p[0] for p in text_pixels_bottom)
    max_x = max(p[0] for p in text_pixels_bottom)
    min_y = min(p[1] for p in text_pixels_bottom)
    max_y = max(p[1] for p in text_pixels_bottom)
    print(f"  Bounds: x={min_x}..{max_x}, y={min_y}..{max_y} ({len(text_pixels_bottom)} pixels)")

# Analyze middle area (y=30..100)
print(f"\n--- Middle: non-background pixels (y=30..100, x=60..260) ---")
text_pixels_mid = []
for y in range(30, 100):
    for x in range(60, 260):
        r, g, b, a = pixels[x, y]
        if abs(r - bg_r) > threshold or abs(g - bg_g) > threshold or abs(b - bg_b) > threshold:
            text_pixels_mid.append((x, y, r, g, b))

if text_pixels_mid:
    min_x = min(p[0] for p in text_pixels_mid)
    max_x = max(p[0] for p in text_pixels_mid)
    min_y = min(p[1] for p in text_pixels_mid)
    max_y = max(p[1] for p in text_pixels_mid)
    print(f"  Bounds: x={min_x}..{max_x}, y={min_y}..{max_y} ({len(text_pixels_mid)} pixels)")