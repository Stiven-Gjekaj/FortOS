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
TESTS=$(ls "$ROOT"/tests/*.m "$ROOT"/tests/core/*.m 2>/dev/null || true)

if [ "$1" = "--phone" ]; then
    shift
    # shellcheck disable=SC2086
    "$ROOT/build.sh" "$OUT/fotests-armv7" -I"$ROOT/src/core" -I"$ROOT/tests" \
        $CORE $TESTS -lz -framework Foundation >"$OUT/build.log" 2>&1 ||
        { grep -v 'liblaunch.dylib' "$OUT/build.log" >&2; exit 1; }
    ssh iphone4s 'mkdir -p /tmp/fotests'
    scp -q "$OUT/fotests-armv7" iphone4s:/tmp/fotests/fotests
    ssh iphone4s "cd /tmp/fotests && ldid -S fotests && ./fotests $*"
else
    # shellcheck disable=SC2086
    clang -fobjc-arc -ffp-contract=off -Wall -Werror -mmacosx-version-min=10.9 -I"$ROOT/src/core" -I"$ROOT/tests" \
        $CORE $TESTS -lz -framework Foundation -o "$OUT/fotests"
    "$OUT/fotests" "$@"
fi
