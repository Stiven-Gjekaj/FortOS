#include "fo_controls.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "fo_game.h"
#include "fo_world.h"

static void stick_init(FOStick *s, float x, float y, float unit) {
    s->x = x;
    s->y = y;
    s->size = 49 * unit;
    s->knob = 23 * unit;
    s->touch = -1;
    s->dx = s->dy = 0;
}

static void button_init(FOButton *b, float x, float y, float w, float h) {
    b->x = x;
    b->y = y;
    b->w = w;
    b->h = h;
    b->touch = -1;
    b->pressed = 0;
}

// The controls that can move, in the order of FOControls.places.
enum { FO_PLACE_WALK, FO_PLACE_SHOOT, FO_PLACE_ABILITY, FO_PLACE_SECOND, FO_PLACE_THIRD, FO_PLACE_LEFT,
       FO_PLACE_RIGHT, FO_PLACE_JUMP, FO_PLACE_COUNT };
static const char *const kPlaceNames[FO_PLACE_COUNT] = {"walk", "shoot", "ability", "second", "third",
                                                       "left", "right", "jump"};
static const int kPlaceButtons[FO_PLACE_COUNT] = {-1, -1, FO_BUTTON_ABILITY, FO_BUTTON_SECOND, FO_BUTTON_THIRD,
                                                  FO_BUTTON_LEFT, FO_BUTTON_RIGHT, FO_BUTTON_JUMP};

// The control is a part of the layout.
static int place_used(int arrows, int place) {
    if (place == FO_PLACE_WALK) return !arrows;
    if (place == FO_PLACE_LEFT || place == FO_PLACE_RIGHT || place == FO_PLACE_JUMP) return arrows;
    return 1;
}

static void place_get(const FOControls *c, int place, float *x, float *y) {
    if (place == FO_PLACE_WALK || place == FO_PLACE_SHOOT) {
        const FOStick *s = place == FO_PLACE_WALK ? &c->walk : &c->shoot;
        *x = s->x;
        *y = s->y;
    } else {
        const FOButton *b = &c->buttons[kPlaceButtons[place]];
        *x = b->x;
        *y = b->y;
    }
}

static void place_set(FOControls *c, int place, float x, float y) {
    if (place == FO_PLACE_WALK || place == FO_PLACE_SHOOT) {
        FOStick *s = place == FO_PLACE_WALK ? &c->walk : &c->shoot;
        s->x = x;
        s->y = y;
    } else {
        FOButton *b = &c->buttons[kPlaceButtons[place]];
        b->x = x;
        b->y = y;
    }
}

static void place_size(const FOControls *c, int place, float *w, float *h) {
    if (place == FO_PLACE_WALK || place == FO_PLACE_SHOOT) {
        *w = *h = (place == FO_PLACE_WALK ? &c->walk : &c->shoot)->size;
    } else {
        const FOButton *b = &c->buttons[kPlaceButtons[place]];
        *w = b->w;
        *h = b->h;
    }
}

// ControlsHUD.n(): the controls take the places of the layout.
static void apply_places(FOControls *c) {
    for (int i = 0; i < FO_PLACE_COUNT; i++) place_set(c, i, c->places[c->arrows][i][0], c->places[c->arrows][i][1]);
    c->buttons[FO_BUTTON_LEFT].shown = c->buttons[FO_BUTTON_RIGHT].shown = c->buttons[FO_BUTTON_JUMP].shown = c->arrows;
}

void fo_controls_layout(FOControls *c, int width, int height) {
    float u = width / 281.0f;
    float W = (float)width, H = (float)height;
    float f = (49 * u - 36 * u - 2 * u) / 3.0f;
    memset(c, 0, sizeof(*c));
    c->unit = u;
    c->width = width;
    c->height = height;
    c->dragTouch = -1;
    for (int i = 0; i < FO_CONTROL_TOUCHES; i++) c->touches[i].id = -1;
    stick_init(&c->walk, 0, 0, u);
    stick_init(&c->shoot, 0, 0, u);
    button_init(&c->buttons[FO_BUTTON_ABILITY], 0, 0, 18 * u, 18 * u);
    button_init(&c->buttons[FO_BUTTON_SECOND], 0, 0, 18 * u, 18 * u);
    button_init(&c->buttons[FO_BUTTON_THIRD], 0, 0, 18 * u, 18 * u);
    button_init(&c->buttons[FO_BUTTON_MENU], u, H - 17 * u, 16 * u, 16 * u);
    button_init(&c->buttons[FO_BUTTON_LEFT], 0, 0, 37 * u, 23 * u);
    button_init(&c->buttons[FO_BUTTON_RIGHT], 0, 0, 37 * u, 23 * u);
    button_init(&c->buttons[FO_BUTTON_JUMP], 0, 0, 22 * u, 22 * u);
    c->buttons[FO_BUTTON_MENU].shown = 1;
    // doubleJoysticksModeData (ahq.java:430-447).
    float(*d)[2] = c->places[0];
    float b2x = W - 18 * u - f, b2y = 49 * u + 2 * u;
    d[FO_PLACE_WALK][0] = u, d[FO_PLACE_WALK][1] = u;
    d[FO_PLACE_SHOOT][0] = W - 49 * u - u, d[FO_PLACE_SHOOT][1] = u;
    d[FO_PLACE_ABILITY][0] = b2x, d[FO_PLACE_ABILITY][1] = b2y;
    d[FO_PLACE_SECOND][0] = W - 18 * u - f - 18 * u - f, d[FO_PLACE_SECOND][1] = u + 49 * u;
    d[FO_PLACE_THIRD][0] = b2x, d[FO_PLACE_THIRD][1] = b2y + 18 * u + f;
    // arrowsModeData (ahq.java:410-429).
    float(*a)[2] = c->places[1];
    float jx = W - 22 * u - f, ay = 49 * u + 4 * u;
    a[FO_PLACE_LEFT][0] = u, a[FO_PLACE_LEFT][1] = u;
    a[FO_PLACE_RIGHT][0] = 37 * u + 2 * u, a[FO_PLACE_RIGHT][1] = u;
    a[FO_PLACE_SHOOT][0] = W - 49 * u - u, a[FO_PLACE_SHOOT][1] = u;
    a[FO_PLACE_JUMP][0] = jx, a[FO_PLACE_JUMP][1] = 49 * u + 2 * u;
    a[FO_PLACE_ABILITY][0] = jx - 18 * u - f, a[FO_PLACE_ABILITY][1] = ay;
    a[FO_PLACE_SECOND][0] = jx - 18 * u - f, a[FO_PLACE_SECOND][1] = ay + 18 * u + f;
    a[FO_PLACE_THIRD][0] = jx + 2 * u, a[FO_PLACE_THIRD][1] = ay + 18 * u + f;
    // The walk stick of the arrows layout is not used. It keeps the place of
    // the other layout.
    a[FO_PLACE_WALK][0] = u, a[FO_PLACE_WALK][1] = u;
    d[FO_PLACE_LEFT][0] = a[FO_PLACE_LEFT][0], d[FO_PLACE_LEFT][1] = a[FO_PLACE_LEFT][1];
    d[FO_PLACE_RIGHT][0] = a[FO_PLACE_RIGHT][0], d[FO_PLACE_RIGHT][1] = a[FO_PLACE_RIGHT][1];
    d[FO_PLACE_JUMP][0] = a[FO_PLACE_JUMP][0], d[FO_PLACE_JUMP][1] = a[FO_PLACE_JUMP][1];
    apply_places(c);
}

void fo_controls_set_arrows(FOControls *c, int arrows) {
    c->arrows = arrows != 0;
    apply_places(c);
}

// The places are in UI units in the file, so a saved layout fits each
// screen.
void fo_controls_load(FOControls *c, const FOPrefs *prefs) {
    char key[48];
    for (int m = 0; m < 2; m++) {
        for (int i = 0; i < FO_PLACE_COUNT; i++) {
            for (int k = 0; k < 2; k++) {
                snprintf(key, sizeof(key), "controls_%s_%s_%c", m ? "arrows" : "sticks", kPlaceNames[i], k ? 'y' : 'x');
                c->places[m][i][k] = fo_prefs_float(prefs, key, c->places[m][i][k] / c->unit) * c->unit;
            }
        }
    }
    c->arrows = fo_prefs_int(prefs, "controls_arrows", c->arrows) != 0;
    apply_places(c);
}

void fo_controls_save(const FOControls *c, FOPrefs *prefs) {
    char key[48];
    for (int m = 0; m < 2; m++) {
        for (int i = 0; i < FO_PLACE_COUNT; i++) {
            for (int k = 0; k < 2; k++) {
                snprintf(key, sizeof(key), "controls_%s_%s_%c", m ? "arrows" : "sticks", kPlaceNames[i], k ? 'y' : 'x');
                fo_prefs_set_float(prefs, key, c->places[m][i][k] / c->unit);
            }
        }
    }
    fo_prefs_set_int(prefs, "controls_arrows", c->arrows);
}

int fo_controls_edit_touch(FOControls *c, int id, int phase, float x, float y) {
    if (phase == FO_TOUCH_BEGAN) {
        // The last control on top takes the touch, as the stage of libGDX.
        for (int i = FO_PLACE_COUNT - 1; i >= 0; i--) {
            if (!place_used(c->arrows, i)) continue;
            float px, py, w, h;
            place_get(c, i, &px, &py);
            place_size(c, i, &w, &h);
            if (x >= px && x <= px + w && y >= py && y <= py + h) {
                c->dragId = i;
                c->dragTouch = id;
                c->dragX = x - px;
                c->dragY = y - py;
                return 1;
            }
        }
        return 0;
    }
    if (id != c->dragTouch) return 0;
    if (phase == FO_TOUCH_MOVED) {
        float px = x - c->dragX, py = y - c->dragY;
        place_set(c, c->dragId, px, py);
        c->places[c->arrows][c->dragId][0] = px;
        c->places[c->arrows][c->dragId][1] = py;
    } else {
        c->dragTouch = -1;
    }
    return 1;
}

// GangfortUIJoystick.a(float, float): the knob follows the finger inside
// half the base. The listener gives the point less half the knob, and the
// rest place is the corner of the knob, so the two halves cancel.
static void stick_move(FOStick *s, float x, float y) {
    float restX = s->x + s->size / 2 - s->knob / 2, restY = s->y + s->size / 2 - s->knob / 2;
    float dx = (x - s->knob / 2) - restX, dy = (y - s->knob / 2) - restY;
    float d2 = dx * dx + dy * dy, r = s->size / 2;
    if (d2 > r * r) {
        float k = (float)(r / sqrt(d2));
        dx *= k;
        dy *= k;
    }
    s->dx = dx;
    s->dy = dy;
}

static int inside(float x, float y, float bx, float by, float w, float h) {
    return x >= bx && x <= bx + w && y >= by && y <= by + h;
}

// Keeps the place of each finger, for the arrows.
static void track(FOControls *c, int id, int phase, float x, float y) {
    int free = -1;
    for (int i = 0; i < FO_CONTROL_TOUCHES; i++) {
        if (c->touches[i].id == id) {
            if (phase == FO_TOUCH_BEGAN || phase == FO_TOUCH_MOVED) {
                c->touches[i].x = x;
                c->touches[i].y = y;
            } else {
                c->touches[i].id = -1;
            }
            return;
        }
        if (c->touches[i].id < 0 && free < 0) free = i;
    }
    if (free >= 0 && (phase == FO_TOUCH_BEGAN || phase == FO_TOUCH_MOVED)) {
        c->touches[free].id = id;
        c->touches[free].x = x;
        c->touches[free].y = y;
    }
}

// A finger is on the button.
static int held(const FOControls *c, int button) {
    const FOButton *b = &c->buttons[button];
    if (!b->shown) return 0;
    for (int i = 0; i < FO_CONTROL_TOUCHES; i++) {
        if (c->touches[i].id >= 0 && inside(c->touches[i].x, c->touches[i].y, b->x, b->y, b->w, b->h)) return 1;
    }
    return 0;
}

int fo_controls_touch(FOControls *c, int id, int phase, float x, float y) {
    FOStick *sticks[2] = {&c->walk, &c->shoot};
    track(c, id, phase, x, y);
    if (phase == FO_TOUCH_BEGAN) {
        for (int i = 0; i < 2; i++) {
            FOStick *s = sticks[i];
            if (i == 0 && c->arrows) continue;
            if (s->touch < 0 && inside(x, y, s->x, s->y, s->size, s->size)) {
                s->touch = id;
                stick_move(s, x, y);
                return 1;
            }
        }
        for (int i = 0; i < FO_BUTTON_COUNT; i++) {
            FOButton *b = &c->buttons[i];
            if (b->shown && b->touch < 0 && inside(x, y, b->x, b->y, b->w, b->h)) {
                b->touch = id;
                b->pressed = 1;
                return 1;
            }
        }
        return 0;
    }
    for (int i = 0; i < 2; i++) {
        FOStick *s = sticks[i];
        if (s->touch != id) continue;
        if (phase == FO_TOUCH_MOVED) {
            stick_move(s, x, y);
        } else {
            s->touch = -1;
            s->dx = s->dy = 0;
        }
        return 1;
    }
    for (int i = 0; i < FO_BUTTON_COUNT; i++) {
        FOButton *b = &c->buttons[i];
        if (b->touch != id) continue;
        if (phase == FO_TOUCH_ENDED) b->touch = -1;
        return 1;
    }
    return 0;
}

void fo_controls_reset(FOControls *c) {
    for (int i = 0; i < FO_CONTROL_TOUCHES; i++) c->touches[i].id = -1;
    c->dragTouch = -1;
    c->walk.touch = c->shoot.touch = -1;
    c->walk.dx = c->walk.dy = c->shoot.dx = c->shoot.dy = 0;
    for (int i = 0; i < FO_BUTTON_COUNT; i++) {
        c->buttons[i].touch = -1;
        c->buttons[i].pressed = 0;
    }
}

int fo_controls_move(const FOControls *c) {
    if (c->arrows) {
        if (held(c, FO_BUTTON_LEFT)) return FO_MOVE_LEFT;
        if (held(c, FO_BUTTON_RIGHT)) return FO_MOVE_RIGHT;
        return FO_MOVE_STOP;
    }
    if (c->walk.touch < 0) return FO_MOVE_STOP;
    if (c->walk.dx > 0) return FO_MOVE_RIGHT;
    if (c->walk.dx < 0) return FO_MOVE_LEFT;
    return FO_MOVE_STOP;
}

int fo_controls_jump(FOControls *c) {
    // ahq.e(): the jump button is pressed in the frame of its touch.
    if (c->arrows) return fo_controls_take(c, FO_BUTTON_JUMP);
    if (c->walk.touch < 0 || (c->walk.dx == 0 && c->walk.dy == 0)) return 0;
    float a = atan2f(c->walk.dy, c->walk.dx) * 57.295776f;
    return a >= 45 && a <= 135;
}

int fo_controls_shoot_touched(const FOControls *c) {
    return c->shoot.touch >= 0;
}

int fo_controls_shoot_moved(const FOControls *c) {
    float quarter = c->shoot.size / 4;
    return c->shoot.touch >= 0 && (fabsf(c->shoot.dx) >= quarter || fabsf(c->shoot.dy) >= quarter);
}

float fo_controls_shoot_angle(const FOControls *c) {
    return atan2f(c->shoot.dy, c->shoot.dx) * 57.295776f;
}

int fo_controls_take(FOControls *c, int button) {
    int pressed = c->buttons[button].pressed;
    c->buttons[button].pressed = 0;
    return pressed;
}

static void draw_region(const FOAtlasTextures *sprites, const char *name, float x, float y, float w, float h) {
    FOSprite s = fo_assets_sprite(sprites, name);
    if (s.w) fo_gfx_draw(&s, x, y, w, h, 0, 0, 0, 0, FO_WHITE);
}

static void draw_stick(const FOStick *s, const FOAtlasTextures *sprites, const char *base) {
    draw_region(sprites, base, s->x, s->y, s->size, s->size);
    float restX = s->x + s->size / 2 - s->knob / 2, restY = s->y + s->size / 2 - s->knob / 2;
    draw_region(sprites, "joystick_knob", restX + s->dx, restY + s->dy, s->knob, s->knob);
}

static void draw_button(const FOButton *b, const FOAtlasTextures *sprites, const char *team, const char *icon) {
    char name[64];
    snprintf(name, sizeof(name), "%s_%s", team, icon);
    draw_region(sprites, name, b->x, b->y, b->w, b->h);
}

void fo_controls_draw(const FOControls *c, const FOAtlasTextures *sprites, int team, int sniper,
                      const char *icons[FO_BUTTON_COUNT]) {
    const char *t = team == FO_TEAM_RED ? "red" : "blu";
    char name[64];
    if (c->arrows) {
        draw_button(&c->buttons[FO_BUTTON_LEFT], sprites, t, "arrow_left");
        draw_button(&c->buttons[FO_BUTTON_RIGHT], sprites, t, "arrow_right");
        draw_button(&c->buttons[FO_BUTTON_JUMP], sprites, t, "jump");
    } else {
        snprintf(name, sizeof(name), "%s_joystick_bg_walk", t);
        draw_stick(&c->walk, sprites, name);
    }
    snprintf(name, sizeof(name), sniper ? "%s_joystick_bg_sniper" : "%s_joystick_bg", t);
    draw_stick(&c->shoot, sprites, name);
    for (int i = 0; i <= FO_BUTTON_MENU; i++) {
        const FOButton *b = &c->buttons[i];
        const char *icon = i == FO_BUTTON_MENU ? "menu" : icons ? icons[i] : NULL;
        if (!b->shown || !icon) continue;
        draw_button(b, sprites, t, icon);
    }
}

void fo_controls_draw_edit(const FOControls *c, const FOAtlasTextures *sprites, int team) {
    const char *t = team == FO_TEAM_RED ? "red" : "blu";
    char name[64];
    // The order of the group of ControlsHUD: the arrows, the jump, the
    // small buttons, and the sticks on top.
    if (c->arrows) {
        draw_button(&c->buttons[FO_BUTTON_LEFT], sprites, t, "arrow_left");
        draw_button(&c->buttons[FO_BUTTON_RIGHT], sprites, t, "arrow_right");
        draw_button(&c->buttons[FO_BUTTON_JUMP], sprites, t, "jump");
    }
    for (int i = FO_BUTTON_ABILITY; i <= FO_BUTTON_THIRD; i++) draw_button(&c->buttons[i], sprites, t, "empty_smallbutton");
    if (!c->arrows) {
        snprintf(name, sizeof(name), "%s_joystick_bg_walk", t);
        draw_stick(&c->walk, sprites, name);
    }
    snprintf(name, sizeof(name), "%s_joystick_bg", t);
    draw_stick(&c->shoot, sprites, name);
}
