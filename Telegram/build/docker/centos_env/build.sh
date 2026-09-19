#!/bin/bash
set -e

# CRITICAL: unset CCACHE_DISABLE baked into container image!
unset CCACHE_DISABLE

export CC="$(which gcc 2>/dev/null || echo gcc)"
export CXX="$(which g++ 2>/dev/null || echo g++)"

export CCACHE_DIR="${CCACHE_DIR:-/usr/src/tdesktop/.ccache}"
mkdir -p "$CCACHE_DIR"

if [ -f /usr/src/tdesktop/out/CMakeCache.txt ]; then
    if grep -q "CMAKE_C_COMPILER:FILEPATH=.*ccache" /usr/src/tdesktop/out/CMakeCache.txt; then
        echo "Removing stale CMakeCache.txt referencing ccache as compiler..."
        rm -f /usr/src/tdesktop/out/CMakeCache.txt
    fi
fi

echo "=== Initial CCACHE stats ==="
ccache -s || true

cd Telegram
./configure.sh "$@"
cmake --build ../out --config "${CONFIG:-Release}" -- -j $(nproc)

echo "=== Final CCACHE stats ==="
ccache -s || true
