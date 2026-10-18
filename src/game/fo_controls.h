// fo_controls: the touch controls of GANGFORT (ControlsHUD, ahq.java, and
// GangfortUIJoystick, aim.java): the layout with two sticks, and the layout
// with two arrows, a jump button, and the shoot stick. The player can move
// each control on the control setup screen. All sizes are UI units of 1/281
// of the width of the screen. The coordinates are pixels with y up.

#ifndef FO_CONTROLS_H
#define FO_CONTROLS_H

#include "fo_assets.h"
#include "fo_prefs.h"

typedef struct {
    float x, y;        // The bottom left corner of the base.
    float size;        // The side of the base (49 units).
    float knob;        // The side of the knob (23 units).
    int touch;         // The id of the finger, or -1.
    float dx, dy;      // The move of the knob from its rest place.
} FOStick;

typedef struct {
    float x, y, w, h;
    int touch;
    int pressed;       // 1 after a touch down, until fo_controls_take reads it.
    int shown;
} FOButton;

enum {
    FO_BUTTON_ABILITY = 0,  // smallButton2: sentry, cloak, or uber.
    FO_BUTTON_SECOND = 1,   // smallButton3: backstab.
    FO_BUTTON_THIRD = 2,    // smallButton4: disguise.
    FO_BUTTON_MENU = 3,     // The pause menu.
    FO_BUTTON_LEFT = 4,     // The arrows layout: the left arrow,
    FO_BUTTON_RIGHT = 5,    // the right arrow,
    FO_BUTTON_JUMP = 6,     // and smallButton1, the jump.
    FO_BUTTON_COUNT = 7,
};

#define FO_CONTROL_TOUCHES 8

typedef struct {
    float unit;
    int width, height;
    int arrows;              // 1 for the arrows layout (not useDoubleJoystick).
    // The place of each control that can move, in each layout: [arrows]
    // [walk, shoot, ability, second, third, left, right, jump][x, y].
    float places[2][8][2];
    FOStick walk, shoot;
    FOButton buttons[FO_BUTTON_COUNT];
    // The fingers on the screen, for the arrows: a finger that slides onto
    // an arrow presses it (ClickListener.enter, ahr.java).
    struct {
        int id;
        float x, y;
    } touches[FO_CONTROL_TOUCHES];
    // The control that a finger drags on the setup screen (ahx.java).
    int dragId, dragTouch;
    float dragX, dragY;
} FOControls;

// The default places of the two layouts (ahq.java:404-450). The layout
// with two sticks is the default.
void fo_controls_layout(FOControls *c, int width, int height);

// Changes the layout. The controls keep their places in each layout.
void fo_controls_set_arrows(FOControls *c, int arrows);

// Reads the layout and the places from prefs, and writes them. A control
// with no saved place keeps its default place.
void fo_controls_load(FOControls *c, const FOPrefs *prefs);
void fo_controls_save(const FOControls *c, FOPrefs *prefs);

// The setup screen (CustomizeTouchControlsScreen, adh.java): a finger
// moves the control under it. Returns 1 when a control takes the touch.
int fo_controls_edit_touch(FOControls *c, int id, int phase, float x, float y);

// Draws the controls of the layout for the setup screen, with empty
// ability buttons (ahq.k()).
void fo_controls_draw_edit(const FOControls *c, const FOAtlasTextures *sprites, int team);

// Gives one touch to the controls. Returns 1 when a control takes it.
int fo_controls_touch(FOControls *c, int id, int phase, float x, float y);

// Releases each control, for example when a dialog opens.
void fo_controls_reset(FOControls *c);

// The move code of the walk stick or the arrows: FO_MOVE_LEFT,
// FO_MOVE_RIGHT, or FO_MOVE_STOP.
int fo_controls_move(const FOControls *c);

// The jump: the walk stick points up, from 45 to 135 degrees, or the jump
// button was pressed since the last call.
int fo_controls_jump(FOControls *c);

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
