#!/bin/bash
set -e

SERIAL="${1:-}"
if [ -z "$SERIAL" ]; then
    echo "Usage: ./deploy.sh <adb-serial>"
    echo ""
    echo "Available devices:"
    adb devices -l | grep -v "^List" | grep -v "^$" | sed 's/^/  /'
    exit 1
fi

ADB="adb -s $SERIAL"
echo "Deploying to device $SERIAL ..."

# Find binary: prefer output/, fall back to release/
BIN=""
MODELS=""
if [ -f output/xiaozhi-r328 ]; then
    BIN=output/xiaozhi-r328
    MODELS=output/models
elif [ -f release/xiaozhi-r328 ]; then
    BIN=release/xiaozhi-r328
    MODELS=release/models
else
    echo "Error: no binary found. Run build first or use release/ binary."
    exit 1
fi

# Push binary
echo "[1/3] Pushing binary ($BIN) ..."
$ADB push "$BIN" /mnt/UDISK/xiaozhi-r328

# Push models
echo "[2/3] Pushing models ..."
$ADB push "$MODELS/" /mnt/UDISK/models/

# Push config (only if not exists on device, to preserve token)
echo "[3/3] Pushing config ..."
$ADB shell "test -f /mnt/UDISK/config.json" || $ADB push config.json /mnt/UDISK/config.json

# Set executable permission
$ADB shell "chmod +x /mnt/UDISK/xiaozhi-r328"

echo ""
echo "Done! Run with:"
echo "  adb -s $SERIAL shell \"cd /mnt/UDISK && ./xiaozhi-r328 -c config.json\""
