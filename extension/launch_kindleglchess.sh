#!/bin/sh

EXT_DIR="/mnt/us/extensions/kindle-chess"
APP_BIN="$EXT_DIR/bin/armhf/kindle-chess"
APP_LOG="/mnt/us/kindle-glchess.log"
APP_TITLE="Kindle GlChess"
APP_LOADER="$EXT_DIR/lib/armhf/ld-linux-armhf.so.3"
RUNTIME_MODE="${KINDLE_CHESS_RUNTIME:-auto}"

if [ ! -x "$APP_BIN" ]; then
    echo "$APP_TITLE binary not found: $APP_BIN" >"$APP_LOG"
    exit 1
fi

if [ "$1" = "--restart" ]; then
    pkill -f "$APP_BIN" 2>/dev/null
    pkill -f "$APP_LOADER.*$APP_BIN" 2>/dev/null
    sleep 1
fi

if pgrep -f "$APP_BIN" >/dev/null 2>&1; then
    exit 0
fi

LIB_PATHS=""
for dir in "$EXT_DIR/lib/armhf" "$EXT_DIR/bin/armhf"
do
    if [ -d "$dir" ]; then
        if [ -n "$LIB_PATHS" ]; then
            LIB_PATHS="$LIB_PATHS:$dir"
        else
            LIB_PATHS="$dir"
        fi
    fi
done

if [ -z "$LIB_PATHS" ]; then
    for dir in \
        "/mnt/us/extensions/gnomegames/lib/armhf" \
        "/mnt/us/extensions/gnomegames/bin/armhf"
    do
        if [ -d "$dir" ]; then
            if [ -n "$LIB_PATHS" ]; then
                LIB_PATHS="$LIB_PATHS:$dir"
            else
                LIB_PATHS="$dir"
            fi
        fi
    done
fi

ENGINE_PATH="$EXT_DIR/bin/stockfish.sh"
if [ ! -x "$ENGINE_PATH" ]; then
    ENGINE_PATH="/mnt/us/extensions/gnomegames/bin/stockfish.sh"
fi

cd "$EXT_DIR" || exit 1

{
    echo "----- $APP_TITLE launch $(date) -----"
    echo "app=$APP_BIN"
    echo "engine=$ENGINE_PATH"
    echo "runtime_mode=$RUNTIME_MODE"
    echo "lib_paths=$LIB_PATHS"
} >>"$APP_LOG"

if [ -f "$APP_LOADER" ] && [ ! -x "$APP_LOADER" ]; then
    chmod 755 "$APP_LOADER" 2>/dev/null || true
fi

try_launch() {
    mode="$1"
    shift

    echo "Trying runtime: $mode" >>"$APP_LOG"
    DISPLAY=:0 KINDLE_CHESS_ENGINE="$ENGINE_PATH" "$@" >>"$APP_LOG" 2>&1 &
    pid="$!"
    sleep 1

    if kill -0 "$pid" 2>/dev/null || pgrep -f "$APP_BIN" >/dev/null 2>&1; then
        echo "Started runtime: $mode pid=$pid" >>"$APP_LOG"
        exit 0
    fi

    wait "$pid" 2>/dev/null
    code="$?"
    echo "Runtime failed: $mode exit=$code" >>"$APP_LOG"
}

case "$RUNTIME_MODE" in
    direct)
        try_launch direct "$APP_BIN"
        ;;
    loader)
        if [ -x "$APP_LOADER" ]; then
            try_launch loader "$APP_LOADER" --library-path "$LIB_PATHS" "$APP_BIN"
        fi
        ;;
    ldpath)
        LD_LIBRARY_PATH="${LIB_PATHS}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" try_launch ldpath "$APP_BIN"
        ;;
    auto|*)
        try_launch direct "$APP_BIN"
        if [ -x "$APP_LOADER" ]; then
            try_launch loader "$APP_LOADER" --library-path "$LIB_PATHS" "$APP_BIN"
        fi
        LD_LIBRARY_PATH="${LIB_PATHS}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" try_launch ldpath "$APP_BIN"
        ;;
esac

echo "All launch methods failed." >>"$APP_LOG"
exit 1
