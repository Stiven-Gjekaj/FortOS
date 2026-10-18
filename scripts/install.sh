#!/bin/sh
# Build FortOS.app, copy it to the phone at "ssh iphone4s", and open it.
# It takes the phone with scripts/phone.sh first. Give it back with
# "scripts/phone.sh give" after the test.
# It also copies the APK of GANGFORT to the phone when the phone does not
# have it.
# Usage: scripts/install.sh [--debug]
# GANGFORT_APK sets the path of the APK on this Mac. The default is the
# APK in reference/.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
# Other projects test on the same phone. Take it, or stop when it is busy.
"$ROOT/scripts/phone.sh" take
APK=${GANGFORT_APK:-$(ls "$ROOT"/reference/*.apk 2>/dev/null | head -1)}
"$ROOT/scripts/app.sh" "$@" >/dev/null

if ! ssh iphone4s 'test -f /var/mobile/Media/FortOS/GANGFORT.apk'; then
    [ -f "$APK" ] || { echo "install: no APK of GANGFORT; set GANGFORT_APK" >&2; exit 1; }
    ssh iphone4s 'mkdir -p /var/mobile/Media/FortOS && chown mobile:mobile /var/mobile/Media/FortOS'
    scp -q "$APK" iphone4s:/var/mobile/Media/FortOS/GANGFORT.apk
fi

ssh iphone4s 'killall FortOS >/dev/null 2>&1; rm -rf /Applications/FortOS.app'
# The sftp server of the phone cannot make a new directory, so copy with tar.
# COPYFILE_DISABLE stops macOS tar from adding "._" files.
COPYFILE_DISABLE=1 tar --no-xattrs -C "$ROOT/build" -cf - FortOS.app | ssh iphone4s 'tar -C /Applications -xf -'
# uicache must run as the mobile user, so that SpringBoard sees the new icon.
ssh iphone4s 'cd /Applications/FortOS.app && ldid -S FortOS &&
    chown -R root:admin /Applications/FortOS.app &&
    chmod 755 FortOS &&
    su mobile -c uicache &&
    open io.github.stivengjekaj.fortos'
