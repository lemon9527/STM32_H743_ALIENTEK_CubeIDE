#!/bin/bash
# Send frame data to STM32H743 via UART for QSPI Flash programming.
# Uses stty + dd for reliable serial I/O on macOS.
#
# Usage: ./program_frames.sh <port> <data.bin>

PORT="${1:-/dev/cu.usbserial-2120}"
DATA_FILE="${2:-scripts/input1_data.bin}"

if [ ! -f "$DATA_FILE" ]; then
    echo "ERROR: file not found: $DATA_FILE"
    exit 1
fi

SIZE=$(wc -c < "$DATA_FILE" | tr -d ' ')
echo "Data file: $DATA_FILE ($((SIZE / 1024)) KB)"
echo "Port: $PORT"

# Configure serial port
stty -f "$PORT" 115200 cs8 -cstopb -parenb -icanon -echo min 1 time 0 2>/dev/null
if [ $? -ne 0 ]; then
    echo "ERROR: Cannot configure $PORT. Is it in use? Close serial monitor first."
    exit 1
fi

# Function: read a line from serial port (byte by byte with dd)
read_serial_line() {
    local timeout_sec="${1:-5}"
    local result=""
    local byte=""
    local start=$(date +%s)
    local now

    while true; do
        now=$(date +%s)
        if [ $((now - start)) -ge "$timeout_sec" ]; then
            return 1
        fi
        byte=$(dd bs=1 count=1 < "$PORT" 2>/dev/null)
        if [ $? -ne 0 ] || [ -z "$byte" ]; then
            sleep 0.1
            continue
        fi
        if [ "$byte" = $'\n' ]; then
            break
        fi
        if [ "$byte" != $'\r' ]; then
            result="${result}${byte}"
        fi
    done
    echo "$result" | xargs
    return 0
}

# Wait for READY from MCU
echo "Waiting for READY (press reset if needed)..."
while true; do
    LINE=$(read_serial_line 5)
    if [ $? -ne 0 ]; then
        echo "ERROR: No READY from MCU. Press reset button on board."
        exit 1
    fi
    if [ -n "$LINE" ]; then
        echo "  MCU: $LINE"
    fi
    if [ "$LINE" = "READY" ]; then
        break
    fi
done

# Send total size (4 bytes LE)
printf "Sending total size: %d bytes (%d KB)\n" "$SIZE" "$((SIZE / 1024))"
python3 -c "
import struct, sys
sys.stdout.buffer.write(struct.pack('<I', $SIZE))
" > "$PORT"

# Wait for ERASE_OK
echo "Waiting for ERASE_OK..."
while true; do
    LINE=$(read_serial_line 120)
    if [ $? -ne 0 ]; then
        echo "ERROR: Timeout waiting for ERASE_OK"
        exit 1
    fi
    if [ -n "$LINE" ]; then
        echo "  MCU: $LINE"
    fi
    if [ "$LINE" = "ERASE_OK" ]; then
        break
    fi
    if echo "$LINE" | grep -q "ERROR"; then
        echo "MCU reported error"
        exit 1
    fi
done

# Send data
echo "Sending $SIZE bytes..."
START=$(date +%s)
dd if="$DATA_FILE" of="$PORT" bs=256 status=progress 2>/dev/null
END=$(date +%s)
ELAPSED=$((END - START))
[ "$ELAPSED" -eq 0 ] && ELAPSED=1
echo "Data sent: $((SIZE / 1024)) KB in ${ELAPSED}s ($((SIZE / ELAPSED / 1024)) KB/s)"

# Wait for DONE
echo "Waiting for DONE..."
while true; do
    LINE=$(read_serial_line 30)
    if [ $? -ne 0 ]; then
        echo "(timeout waiting for DONE, data may still be OK)"
        break
    fi
    if [ -n "$LINE" ]; then
        echo "  MCU: $LINE"
    fi
    if [ "$LINE" = "DONE" ]; then
        break
    fi
    if echo "$LINE" | grep -q "ERROR"; then
        echo "MCU reported error"
        exit 1
    fi
done

echo "QSPI Flash programming complete!"