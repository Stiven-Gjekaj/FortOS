# Changelog

## 0.2.0 (2026-10-18)

- The physics of GANGFORT: Box2D 2.3.1 with the gravity, the scale, and
  the time step of the original.
- The three maps of single player: gantown, bomb_canyon, and flag_egypt,
  with their collisions, areas, and spawn doors.
- The player body of the original. A player walks at the speed of the
  class, jumps 2.4 m, and the rusher jumps a second time in the air.
- Teammates pass through each other. Enemies block each other. A player
  cannot go into the spawn room of the other team.
- The spawn doors open for the team and slide into the wall.
- A death area kills the player, and the player comes back in the spawn
  room 10 s later.
- The two touch sticks of GANGFORT: the left stick walks and jumps.
- The camera follows the player and stays inside the map.
- The vignette is now one pass over the frame, so the maps keep 60 frames
  each second.

## 0.1.0 (2026-10-18)

- The app reads the art and the fonts from the APK of GANGFORT 2.1 at
  `/var/mobile/Media/FortOS/GANGFORT.apk`. It keeps the decoded textures
  in a cache beside the APK.
- The gantown map, with the parallax of the background and the moving
  clouds, at 60 frames each second on the iPhone 4S. A drag moves the
  camera.
- The 9 classes of each team, and the pixel fonts of GANGFORT.
- The vignette of GANGFORT.
