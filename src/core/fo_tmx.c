#include "fo_tmx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FO_PIXEL 0.1f
#define FO_DEG2RAD 0.017453292f

// The value of the attribute name in the tag that starts at tag, or NULL.
// The value goes into out.
static const char *attribute(const char *tag, const char *end, const char *name, char *out, size_t size) {
    size_t n = strlen(name);
    for (const char *p = tag; p && p < end; p++) {
        p = strstr(p, name);
        if (!p || p >= end) return NULL;
        if ((p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n') && p[n] == '=' && p[n + 1] == '"') {
            const char *v = p + n + 2;
            const char *q = strchr(v, '"');
            if (!q || q > end) return NULL;
            snprintf(out, size, "%.*s", (int)(q - v), v);
            return out;
        }
    }
    return NULL;
}

static float number(const char *tag, const char *end, const char *name, float fallback) {
    char value[64];
    return attribute(tag, end, name, value, sizeof(value)) ? strtof(value, NULL) : fallback;
}

static const char *kAreaNames[FO_AREA_KIND_COUNT] = {
    "spawnarea_red", "spawnarea_blu", "koth_capturepoint", "fridge_red", "fridge_blu",
    "fliezone", "birdspawnarea", "nosentryarea", "playerdeatharea", "bombfinisharea",
    "bombfallarea", "ctf_flagcapturearea_blu", "ctf_flagcapturearea_red"};

static void add_box(FOTmx *map, float w, float h, float x, float y, float rotation) {
    if (map->box_count >= FO_TMX_MAX_BOXES) return;
    FOTmxBox *b = &map->boxes[map->box_count++];
    b->x = x;
    b->y = y;
    b->w = w;
    b->h = h;
    // akm.java: bodyDef.angle = (360 - rotation) * deg2rad.
    b->angle = (360.0f - rotation) * FO_DEG2RAD;
}

int fo_tmx_parse(FOTmx *map, const char *text) {
    memset(map, 0, sizeof(*map));
    const char *root = strstr(text, "<map ");
    if (!root) return 0;
    const char *rootEnd = strchr(root, '>');
    if (!rootEnd) return 0;
    float pw = number(root, rootEnd, "width", 0) * number(root, rootEnd, "tilewidth", 0);
    float ph = number(root, rootEnd, "height", 0) * number(root, rootEnd, "tileheight", 0);
    if (pw <= 0 || ph <= 0) return 0;
    map->width = pw * FO_PIXEL;
    map->height = ph * FO_PIXEL;

    for (const char *g = strstr(text, "<objectgroup"); g; g = strstr(g + 1, "<objectgroup")) {
        const char *gEnd = strchr(g, '>');
        const char *close = strstr(g, "</objectgroup>");
        if (!gEnd || !close) break;
        char name[64];
        if (!attribute(g, gEnd, "name", name, sizeof(name))) continue;
        int collisions = strcmp(name, "collisions") == 0, areas = strcmp(name, "areas") == 0;
        if (!collisions && !areas) continue;
        for (const char *o = strstr(gEnd, "<object "); o && o < close; o = strstr(o + 1, "<object ")) {
            const char *oEnd = strchr(o, '>');
            if (!oEnd) break;
            float x = number(o, oEnd, "x", 0) * FO_PIXEL;
            float y = number(o, oEnd, "y", 0) * FO_PIXEL;
            float w = number(o, oEnd, "width", 0) * FO_PIXEL;
            float h = number(o, oEnd, "height", 0) * FO_PIXEL;
            if (collisions) {
                add_box(map, w, h, x, map->height - y, number(o, oEnd, "rotation", 0));
                continue;
            }
            char areaName[64];
            if (!attribute(o, oEnd, "name", areaName, sizeof(areaName))) continue;
            for (int k = 0; k < FO_AREA_KIND_COUNT; k++) {
                if (strcmp(areaName, kAreaNames[k]) != 0 || map->area_count >= FO_TMX_MAX_AREAS) continue;
                FOTmxArea *a = &map->areas[map->area_count++];
                a->kind = (FOAreaKind)k;
                a->rect.x = x;
                a->rect.y = map->height - y - h;
                a->rect.w = w;
                a->rect.h = h;
            }
        }
    }

    for (const char *p = strstr(text, "<property "); p; p = strstr(p + 1, "<property ")) {
        const char *pEnd = strchr(p, '>');
        if (!pEnd) break;
        char name[64];
        if (!attribute(p, pEnd, "name", name, sizeof(name))) continue;
        // As in akm.java, both values are scaled and y is not turned.
        if (strcmp(name, "koth_cp_indicator_pos_x") == 0) map->koth_indicator_x = number(p, pEnd, "value", 0) * FO_PIXEL;
        if (strcmp(name, "koth_cp_indicator_pos_y") == 0) map->koth_indicator_y = number(p, pEnd, "value", 0) * FO_PIXEL;
    }

    // The four walls at the edges of the map (akm.java:131-134).
    add_box(map, map->width, 1, 0, map->height + 1, 0);
    add_box(map, map->width, 1, 0, 0, 0);
    add_box(map, 1, map->height, -1, map->height, 0);
    add_box(map, 1, map->height, map->width, map->height, 0);
    return 1;
}

const FORect *fo_tmx_area(const FOTmx *map, FOAreaKind kind) {
    for (int i = 0; i < map->area_count; i++) {
        if (map->areas[i].kind == kind) return &map->areas[i].rect;
    }
    return NULL;
}

int fo_rect_contains(const FORect *rect, float x, float y) {
    return rect->x <= x && rect->x + rect->w >= x && rect->y <= y && rect->y + rect->h >= y;
}
