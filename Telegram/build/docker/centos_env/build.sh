#!/bin/bash
set -e

export CCACHE_DIR="${CCACHE_DIR:-/usr/src/tdesktop/.ccache}"
mkdir -p "$CCACHE_DIR"

# Ensure ccache wraps all compiler calls unconditionally
CCACHE_BIN="$(which ccache 2>/dev/null || echo /usr/bin/ccache)"
if [ -x "$CCACHE_BIN" ]; then
    mkdir -p /tmp/ccache-bin
    ln -sf "$CCACHE_BIN" /tmp/ccache-bin/gcc
    ln -sf "$CCACHE_BIN" /tmp/ccache-bin/g++
    ln -sf "$CCACHE_BIN" /tmp/ccache-bin/cc
    ln -sf "$CCACHE_BIN" /tmp/ccache-bin/c++
    export PATH="/tmp/ccache-bin:$PATH"
fi

echo "=== Initial CCACHE stats ==="
ccache -s || true

cd Telegram
./configure.sh "$@"
cmake --build ../out --config "${CONFIG:-Release}" -- -j $(nproc)

echo "=== Final CCACHE stats ==="
ccache -s || true
