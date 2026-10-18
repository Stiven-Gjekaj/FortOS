<div align="center">

<img src="assets/brand/logo-320.png" alt="FortOS" width="200">

### A demake of GANGFORT for the iPhone 4S on iOS 6

_Native C and OpenGL ES 2.0, offline matches against bots, your own copy of the game_

![iOS 6](https://img.shields.io/badge/ios-6.1-1f5fc4?style=for-the-badge&logo=apple&logoColor=ffffff)
![armv7](https://img.shields.io/badge/armv7-d8261c?style=for-the-badge)
![C](https://img.shields.io/badge/c-1a1a2e?style=for-the-badge&logo=c&logoColor=ffffff)
![OpenGL ES 2.0](https://img.shields.io/badge/opengl_es-2.0-5586a4?style=for-the-badge&logo=opengl&logoColor=ffffff)
![Cydia](https://img.shields.io/badge/cydia-6d83a2?style=for-the-badge)
[![MIT licence](https://img.shields.io/badge/mit_licence-3a3f47?style=for-the-badge)](LICENSE)
[![Release](https://img.shields.io/github/v/release/Stiven-Gjekaj/FortOS?style=for-the-badge&label=release&color=16181c)](https://github.com/Stiven-Gjekaj/FortOS/releases/latest)
[![Ko-fi](https://img.shields.io/badge/ko--fi-support_this_project-f26a1b?style=for-the-badge&logo=ko-fi&logoColor=ffffff)](https://ko-fi.com/stivengjekaj)

<p align="center">
  <a href="#overview"><b>Overview</b></a> |
  <a href="#you-need-a-copy-of-gangfort"><b>GANGFORT.apk</b></a> |
  <a href="#install"><b>Install</b></a> |
  <a href="#build"><b>Build</b></a> |
  <a href="docs/decisions.md"><b>Decisions</b></a> |
  <a href="TODO.md"><b>Roadmap</b></a>
</p>

</div>

---

> [!NOTE]
> **Version 0.3.0 is out.** A player walks, jumps, aims, and shoots on the
> three maps of single player with the touch sticks of GANGFORT, at 60
> frames each second on the iPhone 4S. The other classes, the game modes,
> and the bots come in the next versions. [TODO.md](TODO.md) lists them.
> [CHANGELOG.md](CHANGELOG.md) lists the changes of each release.

> [!IMPORTANT]
> This is a fan project. It is not from the makers of GANGFORT. The
> repository and the .deb hold no file of GANGFORT. You need your own copy.

---

<table>
<tr>
<td width="50%" valign="top">
<img src="assets/screenshots/fight.png" alt="The blue soldier with a rocket launcher on a slope. The dead red scout lies below, beside the sawngun it dropped. The kill feed shows the rocket icon.">
<p align="center"><i>A kill</i></p>
</td>
<td width="50%" valign="top">
<img src="assets/screenshots/bomb_canyon.png" alt="The spawn room of the blue team on a rock in bomb_canyon, with the two touch sticks">
<p align="center"><i>bomb_canyon</i></p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<img src="assets/screenshots/map.png" alt="The gantown map: a wooden house with platforms in front of the sky">
<p align="center"><i>The gantown map</i></p>
</td>
<td width="50%" valign="top">
<img src="assets/screenshots/classes.png" alt="The nine classes of the blue team in a row in a dark tunnel">
<p align="center"><i>The nine classes</i></p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<img src="assets/screenshots/flag_egypt.png" alt="The blue spawn room in the pyramid of flag_egypt, between two spawn doors">
<p align="center"><i>flag_egypt</i></p>
</td>
<td width="50%" valign="top">
</td>
</tr>
</table>

## Overview

**FortOS** plays GANGFORT on an iPhone 4S with
iOS 6.1.3. GANGFORT itself does not run on iOS 6. FortOS is a new engine in
C, with the rules of GANGFORT 2.1.

GANGFORT has a single player mode against bots. FortOS ports it: the
local game server, the bots, their paths on each map, the 9 classes, the
weapons, and the 3 game modes. The rules come from the code of the Android
APK of GANGFORT 2.1. FortOS has no network code.

The app reads the art, the maps, the fonts, the text, and the sounds from
the APK of your own copy. It holds no file of GANGFORT.

## You need a copy of GANGFORT

The art, the sounds, the maps, and the text of GANGFORT belong to their
owners. FortOS reads them from the APK of GANGFORT 2.1 for Android.

1. Get the APK of your copy of GANGFORT 2.1.
2. Copy it to `/var/mobile/Media/FortOS/GANGFORT.apk` on the phone, with
   an AFC tool or with `scp`.

On its first start, the app decodes the textures and keeps them in
`/var/mobile/Media/FortOS/cache/`.

## Install

You need an iPhone, iPod touch, or iPad with iOS 6 and a jailbreak.

1. Download the `.deb` file from the
   [latest release](https://github.com/Stiven-Gjekaj/FortOS/releases/latest).
2. Copy it to the phone, for example with `scp` to `/tmp/`.
3. On the phone, as root: `dpkg -i /tmp/io.github.stivengjekaj.fortos_*.deb`.

The FortOS icon comes on the home screen. To remove the app:
`dpkg -r io.github.stivengjekaj.fortos`.

On iOS 7 or later, or with an APT that has TLS 1.2, you can also add the
Cydia source `https://stiven-gjekaj.github.io/cydia/`. Cydia on iOS 6
cannot read it, because its APT stops at TLS 1.0 and GitHub accepts only
TLS 1.2. See [the repository](https://github.com/Stiven-Gjekaj/cydia).

## Build

You need a Mac with Xcode and the iOS 9.3 SDK of Theos.

1. Download `iPhoneOS9.3.sdk.tar.xz` from the releases of
   [theos/sdks](https://github.com/theos/sdks/releases) and unpack it into
   `sdks/`. The result is `sdks/iPhoneOS9.3.sdk`. Git does not track `sdks/`.
2. Run `./fix-sdk.sh` one time. It changes the SDK stubs for the new Apple
   linker and for iOS 6.
3. Build:

| Command | Result |
| ------- | ------ |
| `scripts/app.sh` | `build/FortOS.app` |
| `scripts/deb.sh` | The Cydia package in `build/` |
| `scripts/test.sh` | The tests, on the Mac |
| `scripts/test.sh --phone` | The tests, on the phone |
| `scripts/install.sh` | Builds the app, copies it to the phone, and opens it |
| `scripts/install.sh --debug` | The same, with the debug commands of `src/game/fo_debug.c` |
| `scripts/tap.sh` | Sends debug commands, as `"tap X Y"`, to a debug build |
| `scripts/shot.sh` | Saves a screenshot of the phone |
| `scripts/release.sh` | Tests, packs, tags, and publishes a release |
| `python3 tools/logo.py` | The logo and the icons of the app |

The phone scripts use `ssh iphone4s`. Set up that host in `~/.ssh/config`,
for example with a USB tunnel of `iproxy` on port 2222. `scripts/install.sh`
copies the APK in `reference/` to the phone, or the APK in `GANGFORT_APK`.

`build.sh` builds one armv7 binary. It compiles with ARC and links without
ARC, and all code is in ARM mode (`-marm`). The new Apple linker makes Thumb
code that crashes on iOS 6 before `main` starts.

## Support

If you find FortOS useful, you can support its development here.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/stivengjekaj)

## Licence

The code is under the MIT licence. See [LICENSE](LICENSE). GANGFORT and its
art, sounds, maps, and text belong to their owners.
