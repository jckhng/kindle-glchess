#!/bin/sh

EXT_DIR="/mnt/us/extensions/kindle-chess"
LAUNCHER="$EXT_DIR/launch_kindleglchess.sh"
LOG_FILE="/mnt/us/kindle-glchess-shortcut.log"

{
    echo "----- Kindle GlChess shortcut $(date) -----"
    echo "cwd=$(pwd)"
    echo "launcher=$LAUNCHER"
} >>"$LOG_FILE" 2>&1

if [ ! -f "$LAUNCHER" ]; then
    echo "launcher not found: $LAUNCHER" >>"$LOG_FILE" 2>&1
    exit 1
fi

chmod 755 "$EXT_DIR"/*.sh "$EXT_DIR/bin/stockfish.sh" 2>/dev/null || true

cd "$EXT_DIR" || exit 1

export DISPLAY="${DISPLAY:-:0}"
export HOME="${HOME:-/mnt/us}"
export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

nohup /bin/sh "$LAUNCHER" --restart >>"$LOG_FILE" 2>&1 &
exit 0
