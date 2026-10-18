#include "fo_debug.h"

#ifdef FO_DEBUG

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "fo_game.h"

static const char *kCommands = "/tmp/FortOS.cmd";
static double gWait;
static struct timespec gLastChange;
static int gStarted;

void fo_debug_poll(double dt) {
    gWait -= dt;
    if (gWait > 0) return;
    gWait = 0.5;
    // A file that root writes over SSH cannot be deleted by the app, which
    // runs as mobile. So the app does a file only when its time changes.
    struct stat info;
    if (stat(kCommands, &info) != 0) {
        gStarted = 1;
        return;
    }
    // A file from an earlier start is old. Do only the commands that come
    // after this start.
    if (!gStarted) {
        gStarted = 1;
        gLastChange = info.st_mtimespec;
        return;
    }
    if (info.st_mtimespec.tv_sec == gLastChange.tv_sec && info.st_mtimespec.tv_nsec == gLastChange.tv_nsec) return;
    gLastChange = info.st_mtimespec;
    FILE *file = fopen(kCommands, "r");
    if (!file) return;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        float x, y, x2, y2;
        if (sscanf(line, "drag %f %f %f %f", &x, &y, &x2, &y2) == 4) {
            // A drag in eight steps, as a finger moves.
            fo_game_touch(1, FO_TOUCH_BEGAN, x, y);
            for (int i = 1; i <= 8; i++) {
                fo_game_touch(1, FO_TOUCH_MOVED, x + (x2 - x) * i / 8, y + (y2 - y) * i / 8);
            }
            fo_game_touch(1, FO_TOUCH_ENDED, x2, y2);
            fprintf(stderr, "debug: drag %.0f %.0f to %.0f %.0f\n", x, y, x2, y2);
        } else if (sscanf(line, "hold %f %f", &x, &y) == 2) {
            // A finger that stays down until "release".
            fo_game_touch(2, FO_TOUCH_BEGAN, x, y);
            fprintf(stderr, "debug: hold %.0f %.0f\n", x, y);
        } else if (sscanf(line, "release %f %f", &x, &y) == 2) {
            fo_game_touch(2, FO_TOUCH_ENDED, x, y);
            fprintf(stderr, "debug: release %.0f %.0f\n", x, y);
        } else if (sscanf(line, "tap %f %f", &x, &y) == 2) {
            fo_game_touch(1, FO_TOUCH_BEGAN, x, y);
            fo_game_touch(1, FO_TOUCH_ENDED, x, y);
            fprintf(stderr, "debug: tap %.0f %.0f\n", x, y);
        } else {
            fo_game_debug(line);
        }
    }
    fclose(file);
    remove(kCommands);
}

#else

void fo_debug_poll(double dt) {
    (void)dt;
}

#endif
