#!/bin/bash
set -e

# CRITICAL: unset CCACHE_DISABLE baked into container image!
unset CCACHE_DISABLE
export CCACHE_DISABLE=0

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
    ln -sf "$CCACHE_BIN" /tmp/ccache-bin/x86_64-redhat-linux-gcc
    ln -sf "$CCACHE_BIN" /tmp/ccache-bin/x86_64-redhat-linux-g++
    export PATH="/tmp/ccache-bin:$PATH"
fi

echo "=== Initial CCACHE stats ==="
ccache -s || true

cd Telegram
./configure.sh "$@"
cmake --build ../out --config "${CONFIG:-Release}" -- -j $(nproc)

echo "=== Final CCACHE stats ==="
ccache -s || true
