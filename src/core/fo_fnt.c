#include "fo_fnt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The number after "key=" in a line, or fallback when the line does not
// have the key.
static int number(const char *line, const char *key, int fallback) {
    size_t n = strlen(key);
    for (const char *p = line; (p = strstr(p, key)); p += n) {
        if ((p == line || p[-1] == ' ') && p[n] == '=') return atoi(p + n + 1);
    }
    return fallback;
}

static int by_code(const void *a, const void *b) {
    return ((const FOGlyphCode *)a)->code - ((const FOGlyphCode *)b)->code;
}

int fo_fnt_parse(FOFnt *font, const char *text) {
    memset(font, 0, sizeof(*font));
    int sawCommon = 0;
    char line[512];
    while (*text) {
        size_t n = 0;
        while (*text && *text != '\n' && *text != '\r') {
            if (n + 1 < sizeof(line)) line[n++] = *text;
            text++;
        }
        line[n] = 0;
        while (*text == '\n' || *text == '\r') text++;

        if (strncmp(line, "info ", 5) == 0) {
            const char *pad = strstr(line, " padding=");
            if (pad) {
                sscanf(pad + 9, "%d,%d,%d,%d", &font->pad_top, &font->pad_right, &font->pad_bottom, &font->pad_left);
            }
        } else if (strncmp(line, "common ", 7) == 0) {
            font->line_height = number(line, "lineHeight", 0);
            font->base = number(line, "base", 0);
            font->page_w = number(line, "scaleW", 0);
            font->page_h = number(line, "scaleH", 0);
            sawCommon = 1;
        } else if (strncmp(line, "page ", 5) == 0) {
            const char *file = strstr(line, "file=\"");
            if (file) {
                file += 6;
                const char *end = strchr(file, '"');
                if (end) snprintf(font->page_file, sizeof(font->page_file), "%.*s", (int)(end - file), file);
            }
        } else if (strncmp(line, "char ", 5) == 0) {
            int id = number(line, "id", -1);
            FOGlyph *g;
            if (id < 0) continue;
            if (id < FO_FNT_GLYPHS) {
                g = &font->glyphs[id];
            } else if (font->extra_count < FO_FNT_EXTRA) {
                font->extra[font->extra_count].code = id;
                g = &font->extra[font->extra_count++].glyph;
            } else {
                continue;
            }
            g->x = number(line, "x", 0);
            g->y = number(line, "y", 0);
            g->w = number(line, "width", 0);
            g->h = number(line, "height", 0);
            g->xoff = number(line, "xoffset", 0);
            g->yoff = number(line, "yoffset", 0);
            g->advance = number(line, "xadvance", 0);
            g->present = 1;
        }
    }
    qsort(font->extra, (size_t)font->extra_count, sizeof(font->extra[0]), by_code);
    return sawCommon && font->line_height > 0;
}

const FOGlyph *fo_fnt_glyph(const FOFnt *font, int code) {
    if (code >= 0 && code < FO_FNT_GLYPHS) return font->glyphs[code].present ? &font->glyphs[code] : NULL;
    FOGlyphCode key = {code, {0, 0, 0, 0, 0, 0, 0, 0}};
    const FOGlyphCode *g = bsearch(&key, font->extra, (size_t)font->extra_count, sizeof(font->extra[0]), by_code);
    return g ? &g->glyph : NULL;
}

int fo_utf8_next(const char **text) {
    const unsigned char *p = (const unsigned char *)*text;
    int c = p[0], n = 0, code = 0;
    if (c < 0x80) n = 0, code = c;
    else if ((c & 0xe0) == 0xc0) n = 1, code = c & 0x1f;
    else if ((c & 0xf0) == 0xe0) n = 2, code = c & 0x0f;
    else if ((c & 0xf8) == 0xf0) n = 3, code = c & 0x07;
    else n = -1;
    for (int i = 1; i <= n; i++) {
        if ((p[i] & 0xc0) != 0x80) {
            n = -1;
            break;
        }
        code = (code << 6) | (p[i] & 0x3f);
    }
    if (n < 0) {
        *text += 1;
        return c;
    }
    *text += n + 1;
    return code;
}

int fo_fnt_width(const FOFnt *font, const char *text) {
    int width = 0, best = 0;
    while (*text) {
        int c = fo_utf8_next(&text);
        if (c == '\n') {
            if (width > best) best = width;
            width = 0;
            continue;
        }
        const FOGlyph *g = fo_fnt_glyph(font, c);
        if (g) width += g->advance;
    }
    return width > best ? width : best;
}
