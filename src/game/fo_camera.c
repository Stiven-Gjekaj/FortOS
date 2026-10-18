#include "fo_camera.h"

#include <stdlib.h>

#define FO_VIEW_W 22.1875f

void fo_camera_init(FOCamera *c, int width, int height, float x, float y) {
    c->viewW = FO_VIEW_W;
    c->viewH = FO_VIEW_W / ((float)width / height);
    c->shakeTime = c->shakeLength = c->shakeAmount = 0;
    c->lockTime = 0;
    fo_camera_jump(c, x, y);
    c->shownX = x;
    c->shownY = y;
}

void fo_camera_jump(FOCamera *c, float x, float y) {
    c->x = c->targetX = x;
    c->y = c->targetY = y;
}

void fo_camera_shake(FOCamera *c, int ms, float amount) {
    c->shakeLength = c->shakeTime = ms / 1000.0f;
    c->shakeAmount = amount;
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static float random_unit(void) {
    return (float)rand() / (float)RAND_MAX * 2 - 1;
}

void fo_camera_update(FOCamera *c, float dt, float tx, float ty, float mapW, float mapH) {
    if (c->lockTime > 0) {
        c->lockTime -= dt;
        return;
    }
    c->targetX = tx;
    c->targetY = ty;
    c->x += (c->targetX - c->x) * 0.075f;
    c->y += (c->targetY - c->y) * 0.075f;
    float x = c->x, y = c->y;
    // The amount of the shake falls to 0 over its time.
    if (c->shakeTime > 0) {
        float amount = c->shakeAmount * c->shakeTime / c->shakeLength;
        x += random_unit() * amount;
        y += random_unit() * amount;
        c->shakeTime -= dt;
    }
    c->shownX = clampf(x, c->viewW * 0.5f, mapW - c->viewW * 0.5f);
    c->shownY = clampf(y, c->viewH * 0.5f, mapH - c->viewH * 0.5f);
}
