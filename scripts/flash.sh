#!/usr/bin/env bash
# Build + flash the StickS3 launcher firmware over USB.
#   ./scripts/flash.sh            # auto-detect the port
#   ./scripts/flash.sh <port>     # e.g. /dev/cu.usbmodem2101
#
# If the device is wedged (a hung app won't auto-reset), single-click the side
# power button or replug USB first, then re-run.
set -euo pipefail
cd "$(dirname "$0")/.."

PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)}"
if [[ -z "${PORT}" ]]; then
  echo "no /dev/cu.usbmodem* found; pass the port explicitly" >&2
  exit 1
fi
echo "flashing $PORT"
pio run -t upload --upload-port "$PORT"
cp .pio/build/sticks3/firmware.factory.bin firmware/stick-s3.bin
echo "done. factory image copied to firmware/stick-s3.bin"
