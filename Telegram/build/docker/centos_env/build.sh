#!/bin/bash
set -e

# CRITICAL: unset CCACHE_DISABLE baked into container image!
unset CCACHE_DISABLE

export CC=gcc
export CXX=g++

export CCACHE_DIR="${CCACHE_DIR:-/usr/src/tdesktop/.ccache}"
mkdir -p "$CCACHE_DIR"

echo "=== Initial CCACHE stats ==="
ccache -s || true

cd Telegram
./configure.sh "$@"
cmake --build ../out --config "${CONFIG:-Release}" -- -j $(nproc)

echo "=== Final CCACHE stats ==="
ccache -s || true
