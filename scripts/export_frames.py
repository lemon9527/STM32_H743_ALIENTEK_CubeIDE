#!/usr/bin/env python3
"""
Export video frames as raw RGB565 for STM32H743 animation playback.

Pipeline:
  1. ffmpeg: extract frames at 20fps, resize to 340x340
  2. PIL: rotate 90° CCW (for portrait LCD orientation), crop to 320x320
  3. numpy: convert RGB → RGB565 little-endian
  4. Output two files:
     frames_hdr.bin  - header (8B) + frame table (N×8B), embedded in ELF
     frames_data.bin - raw RGB565 pixel data (N × 204800 bytes), sent via UART

Output: frames_hdr.bin, frames_data.bin
  HDR Format: [num_frames:4B LE] [version:4B] [N×{offset:4B, size:4B}]
  DATA: N × 204800 bytes raw RGB565

Usage:
  python3 export_frames.py ~/OneDrive/STM32/video/input1.mp4
"""

import subprocess
import os
import sys
import struct
import tempfile
import numpy as np
from PIL import Image

FPS = 20
EXTRACT_SIZE = 340   # ffmpeg extraction size (larger, then crop to center)
SIZE = 320           # final crop size
CROP_MARGIN = 10     # crop from each side
FRAME_RAW_SIZE = SIZE * SIZE * 2   # 204800 bytes per frame
VERSION = 9          # 9 = data offset aligned to sector 4096


def rgb_to_rgb565(pixels):
    """Convert (H, W, 3) uint8 RGB array to (H, W) uint16 RGB565 LE."""
    r = (pixels[:, :, 0].astype(np.uint16) >> 3)
    g = (pixels[:, :, 1].astype(np.uint16) >> 2)
    b = (pixels[:, :, 2].astype(np.uint16) >> 3)
    return (r << 11) | (g << 5) | b


def main(video_path: str):
    if not os.path.exists(video_path):
        print(f"ERROR: video not found: {video_path}")
        sys.exit(1)

    base = os.path.splitext(os.path.basename(video_path))[0]
    hdr_bin = f"{base}_hdr.bin"
    data_bin = f"{base}_data.bin"

    with tempfile.TemporaryDirectory() as tmpdir:
        # Step 1: Extract frames with ffmpeg (340x340, crop later)
        print(f"[1/4] Extracting frames at {FPS}fps, {EXTRACT_SIZE}x{EXTRACT_SIZE}...")
        subprocess.run([
            "ffmpeg", "-y", "-i", video_path,
            "-vf", f"fps={FPS},scale={EXTRACT_SIZE}:{EXTRACT_SIZE}",
            "-q:v", "3",
            f"{tmpdir}/frame_%04d.jpg"
        ], check=True, capture_output=True)

        frames = sorted(f for f in os.listdir(tmpdir) if f.endswith(".jpg"))
        num_frames = len(frames)
        print(f"  Extracted {num_frames} frames")

        # Step 2: Rotate, crop, convert to RGB565
        print(f"[2/4] Rotating, cropping {EXTRACT_SIZE}→{SIZE}, converting to RGB565...")
        raw_blobs = []
        for i, fname in enumerate(frames):
            if i % 20 == 0:
                print(f"  Processing frame {i+1}/{num_frames}...")
            img = Image.open(os.path.join(tmpdir, fname))
            # Rotate 90° CCW for portrait LCD
            img = img.rotate(90, expand=True)
            # Crop 10px from each side: center crop 340x340 → 320x320
            img = img.crop((CROP_MARGIN, CROP_MARGIN,
                            EXTRACT_SIZE - CROP_MARGIN, EXTRACT_SIZE - CROP_MARGIN))
            # Convert to RGB565
            pixels = np.array(img)  # (320, 320, 3) uint8
            rgb565 = rgb_to_rgb565(pixels)  # (320, 320) uint16
            raw_blobs.append(rgb565.tobytes())  # little-endian uint16 bytes

        # Step 3: Build header file (header + frame table, NO pixel data)
        print(f"[3/4] Building {hdr_bin}...")
        data_offset = 4096  # QSPI Flash offset of frame data (sector-aligned)
        with open(hdr_bin, "wb") as f:
            # Header: num_frames (4B) + version (4B)
            f.write(struct.pack("<II", num_frames, VERSION))
            # Frame table: N × {offset:4B, size:4B}
            # All frames are same size (204800 bytes), contiguous
            for i in range(num_frames):
                offset = data_offset + i * FRAME_RAW_SIZE
                f.write(struct.pack("<II", offset, FRAME_RAW_SIZE))

        # Step 4: Build data file (raw RGB565 pixel data, contiguous)
        print(f"[4/4] Building {data_bin}...")
        total_data = 0
        with open(data_bin, "wb") as f:
            for blob in raw_blobs:
                f.write(blob)
                total_data += len(blob)

        hdr_size = os.path.getsize(hdr_bin)
        print(f"\nDone!")
        print(f"  {hdr_bin}: {hdr_size} bytes (header + table)")
        print(f"  {data_bin}: {total_data / 1024:.1f} KB ({num_frames} frames × {FRAME_RAW_SIZE} bytes)")
        print(f"  QSPI total: {hdr_size + total_data:.1f} KB")
        print(f"\nEmbed in ELF: cp {hdr_bin} ../Core/Src/test_frames.bin")
        print(f"Send via UART: python3 program_frames.py <port> {data_bin}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 export_frames.py <video.mp4>")
        sys.exit(1)
    main(sys.argv[1])