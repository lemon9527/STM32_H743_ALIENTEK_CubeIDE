#!/usr/bin/env python3
from PIL import Image
import os
d = '/Users/lemonliu/IOT/STM32_H743_ALIENTEK_CubeIDE/Core/Src/icon_resource/bottom_bar'
for f in sorted(os.listdir(d)):
    if f.endswith('.png'):
        img = Image.open(os.path.join(d, f))
        print(f'{f}: {img.size[0]}x{img.size[1]}')