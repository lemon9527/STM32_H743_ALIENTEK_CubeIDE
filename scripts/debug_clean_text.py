#!/usr/bin/env python3
from PIL import Image

IN_PATH = "/Users/lemonliu/IOT/STM32_H743_ALIENTEK_CubeIDE/Core/Src/clean_text.png"
img = Image.open(IN_PATH)
print(f"Original: {img.size[0]}x{img.size[1]} mode={img.mode}")

if img.mode == "RGBA":
    pixels = list(img.getdata())
    # Check first 10 pixels
    print("First 10 pixels:")
    for i, (r, g, b, a) in enumerate(pixels[:10]):
        print(f"  [{i:2d}]  R={r:3d} G={g:3d} B={b:3d} A={a:3d}")
    non_zero_rgb = sum(1 for (r, g, b, a) in pixels if r + g + b > 0)
    non_zero_a = sum(1 for (r, g, b, a) in pixels if a > 0)
    print(f"Non-zero RGB: {non_zero_rgb} / {len(pixels)}")
    print(f"Non-zero Alpha: {non_zero_a} / {len(pixels)}")
elif img.mode == "RGB":
    pixels = list(img.getdata())
    print("First 10 pixels:")
    for i, (r, g, b) in enumerate(pixels[:10]):
        print(f"  [{i:2d}]  R={r:3d} G={g:3d} B={b:3d}")
    non_zero = sum(1 for (r, g, b) in pixels if r + g + b > 0)
    print(f"Non-zero: {non_zero} / {len(pixels)}")