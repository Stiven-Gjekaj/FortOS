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
            if (id < 0 || id >= FO_FNT_GLYPHS) continue;
            FOGlyph *g = &font->glyphs[id];
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
    return sawCommon && font->line_height > 0;
}

int fo_fnt_width(const FOFnt *font, const char *text) {
    int width = 0, best = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '\n') {
            if (width > best) best = width;
            width = 0;
            continue;
        }
        const FOGlyph *g = &font->glyphs[*p];
        if (g->present) width += g->advance;
    }
    return width > best ? width : best;
}
