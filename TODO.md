# Roadmap

Each release adds one big part. The risky work comes first: the renderer,
the assets, the physics, and the speed on the A5. The reason for each
choice is in [docs/decisions.md](docs/decisions.md). Each release is a
milestone on GitHub, and each item below is an issue in that milestone.

The counts come from the code of GANGFORT 2.1 for Android: 9 classes, 16
weapons, 3 game modes, 3 maps with bot paths, 34 hats, and 41 sounds.

## 0.1: the foundation (done, 2026-10-18)

- Add the app, install, and package scripts, and the Cydia packaging.
- Read `GANGFORT.apk` from `/var/mobile/Media/FortOS/`, and keep the
  decoded textures in a cache.
- Read the libGDX atlases, and draw sprites with OpenGL ES 2.0, in
  landscape, at 60 frames per second.
- Draw the layers of a map image with the parallax of the original.
- Draw text with the bitmap fonts of the original.
- Measure the memory of the textures of one map on the phone.

## 0.2: the world

- Read the `collisions` and `areas` groups of the `.tmx` maps.
- Add Box2D 2.3 with the gravity, the scale, and the time step of the
  original.
- Add the player body: run, jump, and the collision filters of the teams
  and the spawn rooms.
- Add the death pits and the fall zones.
- Add the camera: the follow, the view width, and the clamp to the map.
- Add the two touch sticks of the original, with their default positions.
- Test the walk, the jump, the doors, and the death areas with the values
  of the original code.

## 0.3: the first fight

- Draw the characters from their parts, with the run, jump, and aim
  animations.
- Add the weapon base: the ammo, the fire interval, the spread, and the
  slow down after a shot.
- Add the projectiles and the hitscan, with the damage and the splash.
- Add the health, the death, the respawn after 10 seconds, and the kill
  credit.
- Add the health fridge and the weapon pickups.
- Add the health bar, the ammo bar, and the kill feed.

## 0.4: all classes

- Add the 9 classes with their health and speed.
- Port the 9 default weapons.
- Add the double jump of the rusher and the rocket jump of the marine.
- Add the burn of the torcher.
- Add the sentry gun of the builder.
- Add the heal and the uber of the dealer.
- Add the charge and the scope shader of the camper.
- Add the cloak, the disguise, and the backstab of the agent.

## 0.5: the game modes

- Add the bomb mode on `bomb_canyon` and `gts_gantown`.
- Add the capture the flag mode on `flag_egypt`.
- Add King of the Hill.
- Add the round end, the team swap, and the points of the original.
- Add the objective HUD and the round over dialog.

## 0.6: the bots

- Port the local game server of single player.
- Port the waypoint graphs of the 3 maps.
- Port the path search and the 7 link types.
- Port the aim and the target choice.
- Port the default brain and the brains of the builder, the dealer, the
  camper, and the agent.
- Add the single player screen: the map, and 8, 10, or 12 players.

## 0.7: the feel

- Play the sounds and the music of the original, decoded on the phone.
- Add the position, the fall off, and the pan of the sounds.
- Add the explosions, the hit effects, and the camera shake.
- Add the kill cam.
- Add the vignette shader.
- Add the flies and the birds of the maps.

## 0.8: the menus and the items

- Add the splash, the main menu, and the credits.
- Add the settings and the control setup screen.
- Add the item screen: the 34 hats and the 5 store weapons, all owned.
- Save the settings, the controls, and the loadout of each class.
- Add the texts in each language of the original.

## 0.9: speed and polish

- Hold 60 frames per second with 12 players on each map.
- Cut the memory: load one map at a time, and free the splash atlas.
- Make the first start and the next starts fast.
- Fix the faults that the play tests find.

## 1.0: the full game

- Compare each class, weapon, mode, and bot with the original once more.

## After 1.0

- Add `ctf_egypt`, with a new waypoint graph. The original has no bot
  path for this map.
