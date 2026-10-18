# Decisions

This file records each choice of the owner and each technical decision.
Each entry gives the reason. The owner made these choices on 2026-10-18.

## What the app is

- **FortOS is a demake of GANGFORT for the iPhone 4S on iOS 6.1.3**
  (jailbroken). It is a native app, installed as a .deb package.
- **The game is a native rewrite.** The game logic is new C code. The
  renderer is OpenGL ES 2.0. Reason: GANGFORT uses libGDX on RoboVM, and its
  iOS build needs iOS 26. The A5 has two cores at 800 MHz and 512 MB of
  memory. A native rewrite is fast, and it needs no Java runtime.
- **The matches are offline, against bots.** FortOS has no network code.
  Reason: the official servers are not available to a demake, and bots need
  no server.

## The reference for the rules

- **The code of the Android APK of the owner's copy is the reference for
  the rules.** Each weapon, class, speed, damage value, and game mode comes
  from that code. Reason: the code of the iOS app is in a binary with
  FairPlay encryption, and the jars in the iOS app are empty stubs. The code
  of the APK has no encryption.
- **FortOS does not decrypt the iOS binary.**
- **The APK is in `reference/`, and git does not track `reference/`.** No
  file of GANGFORT goes into the repository.

## The assets of GANGFORT

- **The repository is public and holds no file of GANGFORT.** The licence
  of the new code is MIT. The art, the sounds, the fonts, the maps, and the
  text of GANGFORT belong to their owners.
- **The app reads the assets from the copy of the owner of the phone.** The
  owner copies the `res/` directory of GANGFORT to
  `/var/mobile/Media/FortOS/res/`. AFC tools and `scp` reach this
  directory. Reason: the .deb then holds no file of GANGFORT, so it can go
  to GitHub and to the Cydia repository like the other 4S apps.
- **The tests do not read the assets.** A test builds the state that it
  needs.

## Identity

- Bundle ID `io.github.stivengjekaj.fortos`, display name `FortOS`.
- iPhone only. iOS 6.0 or later. Landscape only. Reason: GANGFORT is a
  landscape game. The screen of the 4S is 480 x 320 points (960 x 640
  pixels).

## Toolchain

- The same toolchain as Balatro4S, Groovelet, RetroTube, and ZettaLite:
  `build.sh`, the iPhoneOS 9.3 SDK with `fix-sdk.sh`, clang for
  `armv7-apple-ios6.0`, ARM mode only, ARC for Objective-C.
- **The game core is plain C99**, in `src/core`. It has no UIKit and no
  OpenGL. The tests of the core operate on the Mac and on the phone.

## Releases

- Each big part of the roadmap is one minor release. A release gets a git
  tag `vX.Y.Z`, a changelog entry, a GitHub release with the .deb, and an
  entry in the Cydia repository.
- **An agent may make a full release with no review** (2026-10-18). When
  a milestone is complete and its tests pass on the phone, the agent
  pushes the commits, adds the tag, publishes the GitHub release with the
  .deb, adds the .deb to the Cydia repository, and closes the issues.
