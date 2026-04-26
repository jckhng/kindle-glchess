#!/bin/sh

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
EXT_DIR="$(dirname "$SELF_DIR")"

if [ -x "$EXT_DIR/bin/armhf/stockfish" ]; then
    exec "$EXT_DIR/bin/armhf/stockfish" "$@"
fi

if [ -x "/mnt/us/extensions/gnomegames/bin/stockfish.sh" ]; then
    exec /mnt/us/extensions/gnomegames/bin/stockfish.sh "$@"
fi

if [ -x "/mnt/us/extensions/gnomegames/bin/armhf/stockfish" ]; then
    exec /mnt/us/extensions/gnomegames/bin/armhf/stockfish "$@"
fi

echo "No Stockfish binary found for Kindle GlChess." >&2
exit 127
