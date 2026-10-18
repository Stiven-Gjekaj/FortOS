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

## The look

- **The arm turns on the GPU** (0.3, 2026-10-18). The original turns the
  left arm and the dead body at art resolution in a framebuffer of 64 x 64
  pixels, so the pixels stay hard. Reason: one framebuffer pass for each
  player in each frame is too slow on the A5. The arm is 2 pixels wide, so
  the difference is small.
- **The vignette and the scope have the numbers of the original shaders**,
  but they run in other places: the vignette is one multiply pass over the
  frame, and the scope shader makes the mask coordinate in the vertex
  shader. Reason: speed on the A5.
- **The pickup of a dropped weapon is a box.** The original uses a polygon
  around the art of each weapon. Reason: the box only changes how the
  weapon rolls.

- **The carried flag is 0.7 m above the carrier.** The original adds the
  height of the legs image of the carrier, and the core has no images.
  0.7 m is the legs of most classes. Reason: the flag only falls from
  this place when the carrier dies.
- **The carried flag does not keep a fall speed.** The original moves the
  flag each frame, but gravity still adds to its speed. A drop then
  carries that speed into the ground. FortOS sets the speed to 0 while a
  player carries the flag.
- **The round-over dialog does not show the classes yet.** The original
  shows the class of each player, and these run on the bottom bar. The
  rest of the dialog is as in the original.
- **The place of the hill sign comes from the map.** The property
  `koth_cp_indicator_pos` of `gantown.tmx` is (582, 205.5). The original
  scales it by 0.1 and does not turn the y axis, so the sign is at
  (58.2, 20.55). FortOS uses the same place.

- **Some bot names are not in FortOS.** The original list of 139 names
  (xa.java:17) has slurs, a pun on Hitler, a terror leader, a homophobic
  tag, and the names of real politicians. FortOS removes these 17 names
  and keeps the other 122, also the repeats. Reason: FortOS is a public
  project.
- **The bot code follows the bytecode where JADX is wrong.** The search of
  zu.java skips a closed waypoint with a cost that is not smaller. JADX
  shows an update of its parent, which would make a loop. The edge test of
  aks.java goes on to the next object. JADX shows a return. FortOS reads
  these parts with dexdump.
- **A paused bot keeps its speed.** The navigator of the original does not
  call Player.h(float) while it waits, so nothing sets the speed of the
  bot, and the friction of the ground stops it. FortOS skips the walk of
  the player in that frame. The same is true for a bot at the end of a
  round.
- **Quit goes to the single player screen.** The original goes to the
  main menu, which comes in 0.8. The single player screen has no back
  button until then.
- **The characters of the class dialog have no hats.** The hats come with
  the items in 0.8.

## The quirks of the original that stay

- The ray of the sniper rifle ends at the point (cos, sin) x 300 of the
  world, not 300 m from the muzzle (alw.java:89-90).
- The revolver shoots from 1 m in front of the arm, because its muzzle is
  not scaled (alq.java:18).
- The backstab destroys any sentry in front of the spy, also a sentry of
  its own team (xn.java:348-361).
- The uber takes 3 each second, but the heal gives 1 back each second, so
  an uber lasts 10 s while the medic heals.
- A capture of a flag gives 2 points to each BLU player, also when RED
  captures (acn.java:64-86).
- In capture the flag, no player holds the objective, because the test
  asks for the flag of the own team (acn.java:48-53). So the kill of a
  carrier gives no extra point.
- The arrow of the BLU flag on the HUD aims from the place of the RED
  arrow (ahy.java:86-88).
- After the bomb goes off, the camera goes up 100 m each second, so it is
  at the top of the fall area at once (acl.java:76-88).
- A shot of a bot takes 2 ammo: one in the weapon, and one more in the
  aimer (alz.java:122, zi.java:171).
- A red bot always wants the bomb, because the place of a player that is
  not in the list of pushers is 0 (act.java:316-327, zk.java:110).
- The path search adds squared lengths, so the bots like many short links
  more than one long link (zu.java:57).
- The medic bot follows the idle human player too, and near the objective
  it leaves its patient for the objective (zs.java:73-123).

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
