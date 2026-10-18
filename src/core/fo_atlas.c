#include "fo_atlas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Copies one line of text into line, without the end of line, and returns
// the start of the next line, or NULL at the end of the text.
static const char *next_line(const char *text, char *line, size_t size) {
    if (!*text) return NULL;
    size_t n = 0;
    while (*text && *text != '\n' && *text != '\r') {
        if (n + 1 < size) line[n++] = *text;
        text++;
    }
    line[n] = 0;
    // Skip one end of line only, so that a blank line stays.
    if (*text == '\r') text++;
    if (*text == '\n') text++;
    return text;
}

// The value after "key:" in a line such as "  xy: 437, 3". Returns NULL
// when the line has another key.
static const char *value_of(const char *line, const char *key) {
    while (*line == ' ' || *line == '\t') line++;
    size_t n = strlen(key);
    if (strncmp(line, key, n) != 0 || line[n] != ':') return NULL;
    line += n + 1;
    while (*line == ' ') line++;
    return line;
}

static int grow(void **list, int count, int *capacity, size_t size) {
    if (count < *capacity) return 1;
    int next = *capacity ? *capacity * 2 : 64;
    void *bigger = realloc(*list, (size_t)next * size);
    if (!bigger) return 0;
    *list = bigger;
    *capacity = next;
    return 1;
}

int fo_atlas_parse(FOAtlas *atlas, const char *text) {
    memset(atlas, 0, sizeof(*atlas));
    int pageCap = 0, regionCap = 0;
    char line[256];
    FORegion *region = NULL;
    // A blank line ends a page. The next line that is not blank names the
    // file of a new page.
    int wantPage = 1;
    while (text && (text = next_line(text, line, sizeof(line)))) {
        if (line[0] == 0) {
            wantPage = 1;
            region = NULL;
            continue;
        }
        const char *v;
        if (wantPage) {
            if (!grow((void **)&atlas->pages, atlas->page_count, &pageCap, sizeof(FOPage))) break;
            FOPage *page = &atlas->pages[atlas->page_count++];
            memset(page, 0, sizeof(*page));
            snprintf(page->file, sizeof(page->file), "%s", line);
            wantPage = 0;
            region = NULL;
            continue;
        }
        if (line[0] != ' ' && line[0] != '\t') {
            if (strchr(line, ':')) {
                // A key of the page, such as "size: 2048,256".
                if ((v = value_of(line, "size"))) {
                    FOPage *page = &atlas->pages[atlas->page_count - 1];
                    sscanf(v, "%d , %d", &page->w, &page->h);
                }
                continue;
            }
            if (!grow((void **)&atlas->regions, atlas->region_count, &regionCap, sizeof(FORegion))) break;
            region = &atlas->regions[atlas->region_count++];
            memset(region, 0, sizeof(*region));
            snprintf(region->name, sizeof(region->name), "%s", line);
            region->page = atlas->page_count - 1;
            region->index = -1;
            continue;
        }
        if (!region) continue;
        if ((v = value_of(line, "rotate"))) {
            region->rotate = strcmp(v, "true") == 0;
        } else if ((v = value_of(line, "xy"))) {
            sscanf(v, "%d , %d", &region->x, &region->y);
        } else if ((v = value_of(line, "size"))) {
            sscanf(v, "%d , %d", &region->w, &region->h);
        } else if ((v = value_of(line, "orig"))) {
            sscanf(v, "%d , %d", &region->orig_w, &region->orig_h);
        } else if ((v = value_of(line, "offset"))) {
            sscanf(v, "%d , %d", &region->off_x, &region->off_y);
        } else if ((v = value_of(line, "index"))) {
            region->index = atoi(v);
        }
    }
    if (atlas->page_count == 0) {
        fo_atlas_free(atlas);
        return 0;
    }
    return 1;
}

void fo_atlas_free(FOAtlas *atlas) {
    free(atlas->pages);
    free(atlas->regions);
    memset(atlas, 0, sizeof(*atlas));
}

const FORegion *fo_atlas_find(const FOAtlas *atlas, const char *name, int index) {
    for (int i = 0; i < atlas->region_count; i++) {
        const FORegion *r = &atlas->regions[i];
        if (strcmp(r->name, name) == 0 && (index < 0 || r->index == index)) return r;
    }
    return NULL;
}
