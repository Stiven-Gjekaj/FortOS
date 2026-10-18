# Changelog

## 0.5.0 (2026-10-18)

- The game modes of the three maps:
  - King of the Hill on `gts_gantown`: each team has 180 s on its timer,
    the point turns at the speeds of GANGFORT, and a sign under the point
    shows its owner and the capture.
  - The bomb on `bomb_canyon`: RED rolls it, BLU rolls it back 7 s after
    the last RED push, a slope needs two pushers, the gates stop a roll
    back, and the round ends 4.5 s after the bomb falls into the pit.
  - Capture the flag on `flag_egypt`: an enemy takes the flag, a carrier
    cannot cloak or disguise, a dropped flag goes home after 30 s, and 4
    captures win.
- The points of the modes: 5 for a capture of the hill, 3 for a capture
  of a flag, and 2 for each BLU player.
- The end of a round: 15 s later a new round starts, each score goes to
  0, and on the bomb map the teams change sides.
- The objective HUD of each mode: the timers, and the captures with an
  arrow to each flag.
- The round-over dialog with the winner, the top 3 scores of each team,
  and your score.
- The uphill hint under the bomb, and the camera that follows the blast.
- The controls and the bars hide while you are dead and at the end of a
  round, as in GANGFORT.
- Fix: the fills of the ammo bar and the health bar are drawn last, as in
  GANGFORT.

## 0.4.0 (2026-10-18)

- All 9 classes, with their weapons and abilities:
  - the flamethrower and its fire, which burns 5 each second for 5 s,
  - the grenades, which bounce and go off with the splash of GANGFORT,
  - the sniper rifle, which charges while it aims and hits at once, with
    the scope shader, the power, the aim line, and the camera that looks
    ahead,
  - the sentry of the builder, which builds, turns, and shoots,
  - the heal, the uber, and the beam of the dealer,
  - the cloak, the disguise, and the backstab of the agent.
- The ability buttons of GANGFORT for each class.
- The third bar of the HUD: the cloak, the steel, or the uber.
- A shot shakes the camera, and so does a hit on the player.
- Fix: a touch of the right stick aims, and a shot needs the knob out a
  quarter of the stick, as in GANGFORT.

## 0.3.0 (2026-10-18)

- The players are drawn from their parts as in GANGFORT: the legs with
  the run, walk, jump, and squat frames, the body, the head, the weapon,
  and the arm that turns with the aim.
- The right stick aims and shoots. A shot uses ammo, waits for the fire
  interval of the weapon, and slows the walk for a short time.
- The bullets, the rockets, and the rocket jump of the soldier, with the
  damage of each weapon of GANGFORT.
- The kill credit of GANGFORT: the player with the most damage in the
  last 120 s gets the kill and 2 points, and the second gets the assist
  and 1 point.
- The dead body falls, lands, turns to the slope, and goes after 9 s.
- The dead player drops the weapon. A touch gives 30 % of the ammo.
- The fridge in the spawn room fills the health and the ammo.
- The health bar, the ammo bar, the kill feed, and the respawn message.
- The texts come from the APK.

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
