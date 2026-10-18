#include "fo_layer.h"

#include <math.h>

void fo_layer_draw(const FOLayer *layer, int pass, float x, float y, float art, float z,
                   float left, float bottom, float right, float top) {
    const FOSprite *s = &layer->sprite;
    if (!s->w) return;
    if (!layer->cells) {
        // A layer with no cells blends as one sprite.
        if (pass == FO_PASS_BLEND) {
            fo_gfx_depth(FO_DEPTH_TEST, z);
            fo_gfx_draw(s, x, y, s->w * art, s->h * art, 0, 0, 0, 0, FO_WHITE);
        }
        return;
    }
    int want = pass == FO_PASS_OPAQUE ? FO_CELL_OPAQUE : FO_CELL_MIXED;
    fo_gfx_depth(pass == FO_PASS_OPAQUE ? FO_DEPTH_OPAQUE : FO_DEPTH_TEST, z);
    float cell = FO_CELL * art;
    // The cells in view. Row 0 is the top row of the image.
    int c0 = (int)floorf((left - x) / cell), c1 = (int)floorf((right - x) / cell);
    int r0 = (int)floorf((y + s->h * art - top) / cell), r1 = (int)floorf((y + s->h * art - bottom) / cell);
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 >= layer->cols) c1 = layer->cols - 1;
    if (r1 >= layer->rows) r1 = layer->rows - 1;
    float du = (s->u1 - s->u0) / s->w, dv = (s->v1 - s->v0) / s->h;
    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            if (layer->cells[r * layer->cols + c] != want) continue;
            int px = c * FO_CELL, py = r * FO_CELL;
            int pw = s->w - px < FO_CELL ? s->w - px : FO_CELL;
            int ph = s->h - py < FO_CELL ? s->h - py : FO_CELL;
            FOSprite part = {s->texture, s->u0 + px * du, s->v0 + py * dv, s->u0 + (px + pw) * du,
                             s->v0 + (py + ph) * dv, pw, ph};
            fo_gfx_draw(&part, x + px * art, y + (s->h - py - ph) * art, pw * art, ph * art, 0, 0, 0, 0, FO_WHITE);
        }
    }
}
