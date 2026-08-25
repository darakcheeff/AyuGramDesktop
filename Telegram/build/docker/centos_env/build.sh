#!/bin/bash
set -e

export CCACHE_DIR="${CCACHE_DIR:-/usr/src/tdesktop/.ccache}"
mkdir -p "$CCACHE_DIR"
echo "=== Initial CCACHE stats ==="
ccache -s || true

cd Telegram
./configure.sh "$@"
cmake --build ../out --config "${CONFIG:-Release}" -- -j $(nproc)

echo "=== Final CCACHE stats ==="
ccache -s || true
