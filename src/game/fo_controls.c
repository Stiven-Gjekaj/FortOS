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

static void button_init(FOButton *b, float x, float y, float size) {
    b->x = x;
    b->y = y;
    b->size = size;
    b->touch = -1;
    b->pressed = 0;
}

void fo_controls_layout(FOControls *c, int width, int height) {
    float u = width / 281.0f;
    float W = (float)width, H = (float)height;
    float f = (49 * u - 36 * u - 2 * u) / 3.0f;
    c->unit = u;
    // doubleJoysticksModeData (ahq.java:414-447).
    stick_init(&c->walk, u, u, u);
    stick_init(&c->shoot, W - 49 * u - u, u, u);
    float b2x = W - 18 * u - f, b2y = 49 * u + 2 * u;
    button_init(&c->buttons[FO_BUTTON_ABILITY], b2x, b2y, 18 * u);
    button_init(&c->buttons[FO_BUTTON_SECOND], W - 18 * u - f - 18 * u - f, u + 49 * u, 18 * u);
    button_init(&c->buttons[FO_BUTTON_THIRD], b2x, b2y + 18 * u + f, 18 * u);
    button_init(&c->buttons[FO_BUTTON_MENU], u, H - 17 * u, 16 * u);
    c->buttons[FO_BUTTON_MENU].shown = 1;
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

static int inside(float x, float y, float bx, float by, float size) {
    return x >= bx && x <= bx + size && y >= by && y <= by + size;
}

int fo_controls_touch(FOControls *c, int id, int phase, float x, float y) {
    FOStick *sticks[2] = {&c->walk, &c->shoot};
    if (phase == FO_TOUCH_BEGAN) {
        for (int i = 0; i < 2; i++) {
            FOStick *s = sticks[i];
            if (s->touch < 0 && inside(x, y, s->x, s->y, s->size)) {
                s->touch = id;
                stick_move(s, x, y);
                return 1;
            }
        }
        for (int i = 0; i < FO_BUTTON_COUNT; i++) {
            FOButton *b = &c->buttons[i];
            if (b->shown && b->touch < 0 && inside(x, y, b->x, b->y, b->size)) {
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
    c->walk.touch = c->shoot.touch = -1;
    c->walk.dx = c->walk.dy = c->shoot.dx = c->shoot.dy = 0;
    for (int i = 0; i < FO_BUTTON_COUNT; i++) {
        c->buttons[i].touch = -1;
        c->buttons[i].pressed = 0;
    }
}

int fo_controls_move(const FOControls *c) {
    if (c->walk.touch < 0) return FO_MOVE_STOP;
    if (c->walk.dx > 0) return FO_MOVE_RIGHT;
    if (c->walk.dx < 0) return FO_MOVE_LEFT;
    return FO_MOVE_STOP;
}

int fo_controls_jump(const FOControls *c) {
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

void fo_controls_draw(const FOControls *c, const FOAtlasTextures *sprites, int team, int sniper,
                      const char *icons[FO_BUTTON_COUNT]) {
    const char *t = team == FO_TEAM_RED ? "red" : "blu";
    char name[64];
    snprintf(name, sizeof(name), "%s_joystick_bg_walk", t);
    draw_stick(&c->walk, sprites, name);
    snprintf(name, sizeof(name), sniper ? "%s_joystick_bg_sniper" : "%s_joystick_bg", t);
    draw_stick(&c->shoot, sprites, name);
    for (int i = 0; i < FO_BUTTON_COUNT; i++) {
        const FOButton *b = &c->buttons[i];
        const char *icon = i == FO_BUTTON_MENU ? "menu" : icons ? icons[i] : NULL;
        if (!b->shown || !icon) continue;
        snprintf(name, sizeof(name), "%s_%s", t, icon);
        draw_region(sprites, name, b->x, b->y, b->size, b->size);
    }
}
