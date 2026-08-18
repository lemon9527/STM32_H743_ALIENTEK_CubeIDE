#!/usr/bin/env python3
from PIL import Image
import struct
import os

IN_PATH = "/Users/lemonliu/IOT/STM32_H743_ALIENTEK_CubeIDE/Core/Src/Bottom_bar.png"
OUT_RGB = "/Users/lemonliu/IOT/STM32_H743_ALIENTEK_CubeIDE/Core/Src/bottom_bar.raw"

TARGET_WIDTH = 320
CROP_PX = 3

img = Image.open(IN_PATH)
print(f"Original: {img.size[0]}x{img.size[1]} mode={img.mode}")

cropped = img.crop((CROP_PX, 0, img.size[0] - CROP_PX, img.size[1]))
print(f"After crop: {cropped.size[0]}x{cropped.size[1]}")

new_w = TARGET_WIDTH
new_h = int(round((cropped.size[1] * new_w) / cropped.size[0]))
scaled = cropped.resize((new_w, new_h), Image.LANCZOS)
print(f"After scale: {scaled.size[0]}x{scaled.size[1]}")

if scaled.mode != "RGBA":
    scaled = scaled.convert("RGBA")

bg = Image.new("RGBA", scaled.size, (0, 0, 0, 255))
composite = Image.alpha_composite(bg, scaled)

pixels = list(composite.getdata())
print(f"Total pixels: {len(pixels)}")

# Print first 10 pixels
print("\nFirst 10 pixels (RGBA):")
for i, (r, g, b, a) in enumerate(pixels[:10]):
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    print(f"  [{i:2d}]  R={r:3d} G={g:3d} B={b:3d} A={a:3d}  rgb565=0x{rgb565:04X}")

# Check if non-zero
non_zero = sum(1 for (r, g, b, a) in pixels if r + g + b > 0)
print(f"\nNon-zero pixels: {non_zero} / {len(pixels)}")

with open(OUT_RGB, "wb") as f:
    for (r, g, b, a) in pixels:
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        f.write(struct.pack("<H", rgb565))

print(f"\nWrote to {OUT_RGB}, size={os.path.getsize(OUT_RGB)} bytes")
