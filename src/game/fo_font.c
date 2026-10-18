#include "fo_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fo_assets.h"

int fo_font_load(FOFont *font, const char *path) {
    memset(font, 0, sizeof(*font));
    char *text = (char *)fo_assets_read(path, NULL);
    if (!text) return 0;
    int ok = fo_fnt_parse(&font->fnt, text);
    free(text);
    if (!ok) return 0;
    const char *slash = strrchr(path, '/');
    char page[256];
    snprintf(page, sizeof(page), "%.*s%s", slash ? (int)(slash - path + 1) : 0, path, font->fnt.page_file);
    font->page = fo_assets_texture(page);
    return font->page.id != 0;
}

float fo_font_width(const FOFont *font, const char *text, float scale) {
    return fo_fnt_width(&font->fnt, text) * scale;
}

float fo_font_line(const FOFont *font, float scale) {
    return font->fnt.line_height * scale;
}

// The width of the line that starts at text, up to '\n' or the end.
static float line_width(const FOFont *font, const char *text, float scale) {
    float width = 0;
    while (*text && *text != '\n') {
        const FOGlyph *g = fo_fnt_glyph(&font->fnt, fo_utf8_next(&text));
        if (g) width += g->advance * scale;
    }
    return width;
}

void fo_font_draw(const FOFont *font, const char *text, float x, float y, float scale, FOColor color, int align) {
    const FOTexture *t = &font->page;
    if (!t->id) return;
    const char *line = text;
    float top = y;
    while (line) {
        float pen = x;
        if (align != FO_ALIGN_LEFT) {
            float w = line_width(font, line, scale);
            pen -= align == FO_ALIGN_CENTER ? w / 2 : w;
        }
        const char *p = line;
        while (*p && *p != '\n') {
            const FOGlyph *g = fo_fnt_glyph(&font->fnt, fo_utf8_next(&p));
            if (!g) continue;
            if (g->w > 0 && g->h > 0) {
                FOSprite s = {t, (float)g->x / t->width, (float)g->y / t->height, (float)(g->x + g->w) / t->width,
                              (float)(g->y + g->h) / t->height, g->w, g->h};
                fo_gfx_draw(&s, pen + g->xoff * scale, top - (g->yoff + g->h) * scale, g->w * scale, g->h * scale, 0, 0,
                            0, 0, color);
            }
            pen += g->advance * scale;
        }
        line = *p == '\n' ? p + 1 : NULL;
        top -= font->fnt.line_height * scale;
    }
}
