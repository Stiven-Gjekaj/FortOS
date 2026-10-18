// fo_game: the game loop of FortOS. The view controller calls these
// functions with a current GL context.

#ifndef FO_GAME_H
#define FO_GAME_H

enum {
    FO_TOUCH_BEGAN = 0,
    FO_TOUCH_MOVED = 1,
    FO_TOUCH_ENDED = 2,
};

// Opens the APK, loads the assets, and starts the first screen. width and
// height are the size of the drawable in pixels. Returns 0 on failure;
// fo_game_error then tells the reason.
int fo_game_start(const char *apk_path, int width, int height);
const char *fo_game_error(void);

// Updates and draws one frame. dt is the time since the last frame in
// seconds.
void fo_game_frame(double dt);

// One touch. id tells the fingers apart. x and y are pixels with (0, 0)
// at the top left, as UIKit gives them.
void fo_game_touch(int id, int phase, float x, float y);

// A command of the debug build, as "shot". See fo_debug.c.
void fo_game_debug(const char *command);

#endif
