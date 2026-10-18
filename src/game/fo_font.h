// fo_font: draws text with the bitmap fonts of GANGFORT, such as
// res/fonts/pixel12.fnt. The coordinates are y-up, as in fo_gfx.

#ifndef FO_FONT_H
#define FO_FONT_H

#include "fo_fnt.h"
#include "fo_gfx.h"

typedef struct {
    FOFnt fnt;
    FOTexture page;
} FOFont;

enum {
    FO_ALIGN_LEFT = 0,
    FO_ALIGN_CENTER = 1,
    FO_ALIGN_RIGHT = 2,
};

// Reads the font at path, for example "fonts/pixel12.fnt", and its page.
int fo_font_load(FOFont *font, const char *path);

// The width of text at scale, where scale is the size of one font pixel.
float fo_font_width(const FOFont *font, const char *text, float scale);

// The height of one line at scale.
float fo_font_line(const FOFont *font, float scale);

// Draws text with the top of its first line at y. The alignment is about
// x. Each '\n' starts a new line.
void fo_font_draw(const FOFont *font, const char *text, float x, float y, float scale, FOColor color, int align);

#endif
