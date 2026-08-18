#!/usr/bin/env python3
"""
Send raw RGB565 frame data to STM32H743 via UART for QSPI Flash programming.
Uses pyserial (install: pip3 install pyserial).

Usage:
  python3 program_frames.py /dev/cu.usbserial-2120 input1_data.bin
"""

import sys
import os
import struct
import time
import serial

CHUNK_SIZE = 256
BAUD = 115200


def main(port: str, data_path: str):
    if not os.path.exists(data_path):
        print(f"ERROR: file not found: {data_path}")
        sys.exit(1)

    total_size = os.path.getsize(data_path)
    print(f"Data file: {data_path} ({total_size / 1024:.1f} KB)")
    print(f"Opening {port} at {BAUD} baud...")

    s = serial.Serial(port, BAUD, timeout=5)
    s.reset_input_buffer()

    # Wait for READY
    print("Waiting for READY...")
    while True:
        line = s.readline().decode('ascii', errors='ignore').strip()
        if line:
            print(f"  MCU: {line}")
        if line == "READY":
            break

    # Send total size
    print(f"Sending total size: {total_size} bytes ({total_size / 1024:.0f} KB)")
    s.write(struct.pack("<I", total_size))

    # Wait for ERASE_OK
    s.timeout = 120
    print("Waiting for ERASE_OK...")
    while True:
        line = s.readline().decode('ascii', errors='ignore').strip()
        if line:
            print(f"  MCU: {line}")
        if "ERROR" in line.upper():
            print(f"MCU reported error: {line}")
            s.close()
            sys.exit(1)
        if line == "ERASE_OK":
            break

    # Send data with handshake (wait for ACK after each chunk)
    s.timeout = 5
    print(f"Sending {total_size} bytes ({total_size / 1024:.0f} KB) in {CHUNK_SIZE}-byte chunks...")
    with open(data_path, "rb") as f:
        sent = 0
        t_start = time.time()
        while sent < total_size:
            chunk = f.read(CHUNK_SIZE)
            if not chunk:
                break
            s.write(chunk)
            sent += len(chunk)
            # Wait for MCU to acknowledge this chunk before sending next
            ack = s.readline().decode('ascii', errors='ignore').strip()
            if ack != "ACK":
                print(f"  ERROR: expected ACK, got '{ack}'")
                sys.exit(1)
            if sent % (64 * 1024) == 0:
                elapsed = time.time() - t_start
                kbps = (sent / 1024) / elapsed if elapsed > 0 else 0
                pct = sent * 100 / total_size
                print(f"  {sent / 1024:.0f}/{total_size / 1024:.0f} KB ({pct:.0f}%)  {kbps:.0f} KB/s")

    elapsed = time.time() - t_start
    print(f"Data sent: {sent / 1024:.0f} KB in {elapsed:.1f}s ({sent / elapsed / 1024:.0f} KB/s)")

    # Wait for DONE
    s.timeout = 30
    print("Waiting for DONE...")
    while True:
        line = s.readline().decode('ascii', errors='ignore').strip()
        if line:
            print(f"  MCU: {line}")
        if line == "DONE":
            break
        if "ERROR" in line.upper():
            print(f"MCU reported error: {line}")
            s.close()
            sys.exit(1)

    print("QSPI Flash programming complete!")
    s.close()


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 program_frames.py <serial_port> <data.bin>")
        sys.exit(1)
    main(sys.argv[1], sys.argv[2])