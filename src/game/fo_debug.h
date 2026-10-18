// fo_debug: lets a script on the Mac drive the game over SSH.
// A debug build looks at /tmp/FortOS.cmd two times each second. When
// the file changed, the app does each line in it. The commands are:
//   tap X Y              a tap at the pixel (X, Y) of the landscape screen
//   drag X Y X2 Y2       a drag from (X, Y) to (X2, Y2)
//   camera X Y           moves the camera to the point (X, Y) in meters
// fo_game_debug does each other command.

#ifndef FO_DEBUG_H
#define FO_DEBUG_H

void fo_debug_poll(double dt);

#endif
