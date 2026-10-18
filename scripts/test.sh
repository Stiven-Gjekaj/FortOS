#!/bin/sh
# Build and run the tests.
# Usage: scripts/test.sh [--phone] [NAME]
# With no option, the tests of src/core operate on this Mac.
# With --phone, the same tests operate on the iPhone at "ssh iphone4s".
# NAME runs only the tests with NAME in their name.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/tests"
mkdir -p "$OUT"
CORE=$(ls "$ROOT"/src/core/*.c "$ROOT"/src/core/*.m 2>/dev/null || true)
# The physics: the bridge and Box2D, in C++.
PHYS=$(ls "$ROOT"/src/physics/*.cpp "$ROOT"/src/third_party/Box2D/*/*.cpp "$ROOT"/src/third_party/Box2D/*/*/*.cpp 2>/dev/null)
INCLUDES="-I$ROOT/src/core -I$ROOT/src/physics -I$ROOT/src/third_party -I$ROOT/tests"
TESTS=$(ls "$ROOT"/tests/*.m "$ROOT"/tests/core/*.m 2>/dev/null || true)

if [ "$1" = "--phone" ]; then
    shift
    # shellcheck disable=SC2086
    "$ROOT/build.sh" "$OUT/fotests-armv7" $INCLUDES \
        $CORE $PHYS $TESTS -lz -framework Foundation >"$OUT/build.log" 2>&1 ||
        { grep -v 'liblaunch.dylib' "$OUT/build.log" >&2; exit 1; }
    ssh iphone4s 'mkdir -p /tmp/fotests'
    scp -q "$OUT/fotests-armv7" iphone4s:/tmp/fotests/fotests
    ssh iphone4s "cd /tmp/fotests && ldid -S fotests && ./fotests $*"
else
    # Box2D builds one time into objects of its own, with no -Werror.
    mkdir -p "$OUT/phys"
    OBJS=""
    for src in $PHYS; do
        obj="$OUT/phys/$(basename "$src" .cpp).o"
        if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
            clang++ -std=c++11 -ffp-contract=off -O1 -mmacosx-version-min=11.0 $INCLUDES -c "$src" -o "$obj"
        fi
        OBJS="$OBJS $obj"
    done
    # shellcheck disable=SC2086
    clang -fobjc-arc -ffp-contract=off -Wall -Werror -mmacosx-version-min=11.0 $INCLUDES \
        $CORE $TESTS $OBJS -lz -lc++ -framework Foundation -o "$OUT/fotests"
    "$OUT/fotests" "$@"
fi
