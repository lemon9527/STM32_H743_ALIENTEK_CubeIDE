#!/usr/bin/env python3
"""
Convert a raw binary file to a C source file for embedding in firmware.
This avoids --gc-sections issues with objcopy-based embedding.
"""
import sys
import os

def raw_to_c(raw_path, var_name, output_path):
    with open(raw_path, "rb") as f:
        data = f.read()

    # Generate C source
    c_lines = []
    c_lines.append(f"#include <stdint.h>")
    c_lines.append(f"")
    c_lines.append(f"const uint8_t {var_name}[] = {{")
    
    # Write bytes in groups of 16 per line
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_bytes = ", ".join(f"0x{b:02x}" for b in chunk)
        c_lines.append(f"    {hex_bytes},")
    
    c_lines.append(f"}};")
    c_lines.append(f"const unsigned int {var_name}_size = {len(data)};")
    c_lines.append(f"")

    with open(output_path, "w") as f:
        f.write("\n".join(c_lines))

    print(f"Generated {output_path} ({len(data)} bytes)")


if __name__ == "__main__":
    SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
    PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
    SRC_DIR = os.path.join(PROJECT_DIR, "Core", "Src")

    # bottom_bar: 320x134 RGB565
    raw_to_c(
        os.path.join(SRC_DIR, "bottom_bar.raw"),
        "bottom_bar_data",
        os.path.join(SRC_DIR, "bottom_bar_data.c")
    )
    
    # clean_text_rgb: 140x116 RGB565
    raw_to_c(
        os.path.join(SRC_DIR, "clean_text_rgb.raw"),
        "clean_text_rgb_data",
        os.path.join(SRC_DIR, "clean_text_rgb_data.c")
    )
    
    # clean_text_alpha: 140x116 alpha
    raw_to_c(
        os.path.join(SRC_DIR, "clean_text_alpha.raw"),
        "clean_text_alpha_data",
        os.path.join(SRC_DIR, "clean_text_alpha_data.c")
    )