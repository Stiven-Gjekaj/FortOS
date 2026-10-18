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
- **The APK has an offline mode with bots.** Single player starts a local
  game server and adds bots. The bots use waypoint graphs that the code
  holds for each map, a path search, and one brain for each kind of class.
  FortOS ports this code. It does not make new bots.
- **The rules of single player are the rules of FortOS.** In single
  player, the client is the authority for the damage, the deaths, the
  kill credit, and the game modes. FortOS ports that code.
- **The odd rules of the original stay.** For example, a bot that changes
  its class outside its spawn room dies, and a capture in capture the flag
  gives 2 points to each BLU player. Reason: the original is the
  reference. A change goes into this file with its reason.

## The world

- **The physics is Box2D 2.3, in C++.** Reason: GANGFORT uses Box2D 2.3
  through libGDX. The same engine gives the same jumps, slides, and
  collisions. The game core calls it through a small C interface.
- **The view width is the view width of the original**, 22.1875 m.
  Reason: a player then sees the same part of the map as in the original.
  The art is drawn with nearest filtering.
- **1.0 has the 3 maps of single player.** `ctf_egypt` is in the APK, but
  the original never loads it and has no bot path for it.

## The assets of GANGFORT

- **The repository is public and holds no file of GANGFORT.** The licence
  of the new code is MIT. The art, the sounds, the fonts, the maps, and the
  text of GANGFORT belong to their owners.
- **The app reads the assets from the APK of the owner of the phone.** The
  owner copies the APK of GANGFORT 2.1 to
  `/var/mobile/Media/FortOS/GANGFORT.apk`. AFC tools and `scp` reach this
  directory. At the first start, the app unpacks `assets/res/` of the APK
  into its cache. Reason: the .deb then holds no file of GANGFORT, so it
  can go to GitHub and to the Cydia repository like the other 4S apps. The
  APK is a zip file, as `Balatro.love` is for Balatro4S.
- **The sounds are Ogg Vorbis, decoded on the phone with stb_vorbis.**
  Reason: the APK holds Ogg files, and Balatro4S already uses stb_vorbis.
- **The tests do not read the assets.** A test builds the state that it
  needs.

- **The tests hold the numbers of the original code. There is no oracle
  that runs the Java code** (0.2, 2026-10-18). The roadmap had an oracle as
  in Balatro4S. It needs a dex to jar tool, the desktop natives of libGDX
  1.9.3, and an x86_64 Java under Rosetta, because those natives have no
  arm64 build. Reason: the physics is the same Box2D 2.3.1, and each rule
  is a short formula in one place of the Java code. A test with the values
  of that place finds a fault for less work.
- **The world clock replaces the wall clock.** The original reads
  System.currentTimeMillis in the rules, for example for the 100 ms lock of
  a jump. FortOS reads a clock that moves with each frame. Reason: the
  tests can then run a world for a given time with no wait.

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
  OpenGL. The tests of the core operate on the Mac and on the phone. Box2D
  is the one C++ part.

## Releases

- Each big part of the roadmap is one minor release. A release gets a git
  tag `vX.Y.Z`, a changelog entry, a GitHub release with the .deb, and an
  entry in the Cydia repository.
- **An agent may make a full release with no review** (2026-10-18). When
  a milestone is complete and its tests pass on the phone, the agent
  pushes the commits, adds the tag, publishes the GitHub release with the
  .deb, adds the .deb to the Cydia repository, and closes the issues.
