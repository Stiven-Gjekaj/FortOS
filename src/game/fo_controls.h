// fo_controls: the touch controls of GANGFORT (ControlsHUD, ahq.java, and
// GangfortUIJoystick, aim.java), in the layout with two sticks. All sizes
// are UI units of 1/281 of the width of the screen. The coordinates are
// pixels with y up.

#ifndef FO_CONTROLS_H
#define FO_CONTROLS_H

#include "fo_assets.h"

typedef struct {
    float x, y;        // The bottom left corner of the base.
    float size;        // The side of the base (49 units).
    float knob;        // The side of the knob (23 units).
    int touch;         // The id of the finger, or -1.
    float dx, dy;      // The move of the knob from its rest place.
} FOStick;

typedef struct {
    float x, y, size;
    int touch;
    int pressed;       // 1 after a touch down, until fo_controls_take reads it.
    int shown;
} FOButton;

enum {
    FO_BUTTON_ABILITY = 0,  // smallButton2: sentry, cloak, or uber.
    FO_BUTTON_SECOND = 1,   // smallButton3: backstab.
    FO_BUTTON_THIRD = 2,    // smallButton4: disguise.
    FO_BUTTON_MENU = 3,     // The pause menu.
    FO_BUTTON_COUNT = 4,
};

typedef struct {
    float unit;
    FOStick walk, shoot;
    FOButton buttons[FO_BUTTON_COUNT];
} FOControls;

void fo_controls_layout(FOControls *c, int width, int height);

// Gives one touch to the controls. Returns 1 when a control takes it.
int fo_controls_touch(FOControls *c, int id, int phase, float x, float y);

// Releases each control, for example when a dialog opens.
void fo_controls_reset(FOControls *c);

// The move code of the walk stick: FO_MOVE_LEFT, FO_MOVE_RIGHT, or
// FO_MOVE_STOP.
int fo_controls_move(const FOControls *c);

// 1 while the walk stick points up, from 45 to 135 degrees.
int fo_controls_jump(const FOControls *c);

// 1 while a finger is on the shoot stick.
int fo_controls_shoot_touched(const FOControls *c);

// 1 when the knob of the shoot stick moved a quarter of the base or more.
int fo_controls_shoot_moved(const FOControls *c);

// The direction of the shoot stick in degrees, as atan2.
float fo_controls_shoot_angle(const FOControls *c);

// Returns 1 one time after each touch down on the button.
int fo_controls_take(FOControls *c, int button);

// Draws the controls in the colours of team. sniper uses the base of the
// sniper for the shoot stick. icons names the region of each ability
// button, or NULL.
void fo_controls_draw(const FOControls *c, const FOAtlasTextures *sprites, int team, int sniper,
                      const char *icons[FO_BUTTON_COUNT]);

#endif
