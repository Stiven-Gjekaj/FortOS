#!/bin/sh
# Build FortOS.app into build/.
# Usage: scripts/app.sh [--debug]
# With --debug, the app reads commands from /tmp/FortOS.cmd (see src/game/fo_debug.c).
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
APP="$ROOT/build/FortOS.app"
DEBUG=""
if [ "$1" = "--debug" ]; then DEBUG="-DFO_DEBUG"; fi
rm -rf "$APP"
mkdir -p "$APP"

# shellcheck disable=SC2046
"$ROOT/build.sh" "$APP/FortOS" $DEBUG -I"$ROOT/src/core" -I"$ROOT/src/gfx" -I"$ROOT/src/game" -I"$ROOT/src/ui" -I"$ROOT/src/audio" \
    -I"$ROOT/src/physics" -I"$ROOT/src/third_party" \
    $(ls "$ROOT"/src/core/*.c "$ROOT"/src/gfx/*.c "$ROOT"/src/gfx/*.m "$ROOT"/src/game/*.c "$ROOT"/src/audio/*.m "$ROOT"/src/ui/*.m 2>/dev/null) \
    $(ls "$ROOT"/src/physics/*.cpp "$ROOT"/src/third_party/Box2D/*/*.cpp "$ROOT"/src/third_party/Box2D/*/*/*.cpp) \
    -lz -framework Foundation -framework UIKit -framework CoreGraphics -framework QuartzCore \
    -framework OpenGLES -framework ImageIO -framework CoreText -framework AudioToolbox \
    2>&1 | grep -v 'liblaunch.dylib' >&2 || true
[ -f "$APP/FortOS" ]

cp "$ROOT/resources/Info.plist" "$APP/Info.plist"
cp "$ROOT"/resources/*.png "$APP/"
printf 'APPL????' > "$APP/PkgInfo"
echo "$APP"
