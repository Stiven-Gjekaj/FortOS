#include "fo_assets.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "fo_image.h"
#include "fo_zip.h"

static FOZip *gZip;
static long gTextureBytes;

// The cache of the decoded textures: a folder beside the APK. Each file
// holds a header and the RGBA pixels. A new APK, with another size or
// time, makes the cache old.
static struct {
    char dir[1024];
    long long apkSize;
    long long apkTime;
} gCache;

typedef struct {
    char magic[4];  // "FOST": a texture of FortOS.
    int64_t apkSize, apkTime;
    int32_t width, height;
} FOCacheHeader;

int fo_assets_open(const char *apk_path) {
    fo_assets_close();
    gZip = fo_zip_open(apk_path);
    if (!gZip) return 0;
    size_t size = 0;
    unsigned char *atlas = fo_assets_read("textures/sprites.atlas", &size);
    if (!atlas) {
        fo_assets_close();
        return 0;
    }
    free(atlas);

    struct stat info;
    const char *dirEnd = strrchr(apk_path, '/');
    gCache.dir[0] = 0;
    if (dirEnd && stat(apk_path, &info) == 0) {
        snprintf(gCache.dir, sizeof(gCache.dir), "%.*s/cache", (int)(dirEnd - apk_path), apk_path);
        mkdir(gCache.dir, 0755);
        gCache.apkSize = info.st_size;
        gCache.apkTime = info.st_mtime;
    }
    return 1;
}

void fo_assets_close(void) {
    fo_zip_close(gZip);
    gZip = NULL;
}

unsigned char *fo_assets_read(const char *path, size_t *size) {
    char name[512];
    snprintf(name, sizeof(name), "assets/res/%s", path);
    return fo_zip_read(gZip, name, size);
}

static void cache_path(const char *name, char *out, size_t size) {
    char flat[256];
    snprintf(flat, sizeof(flat), "%s", name);
    for (char *p = flat; *p; p++) {
        if (*p == '/') *p = '_';
    }
    snprintf(out, size, "%s/%s.rgba", gCache.dir, flat);
}

static unsigned char *cache_read(const char *path, int *width, int *height) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    FOCacheHeader h;
    unsigned char *pixels = NULL;
    if (fread(&h, sizeof(h), 1, file) == 1 && memcmp(h.magic, "FOST", 4) == 0 && h.apkSize == gCache.apkSize &&
        h.apkTime == gCache.apkTime && h.width > 0 && h.height > 0 && h.width <= 4096 && h.height <= 4096) {
        size_t bytes = (size_t)h.width * h.height * 4;
        pixels = malloc(bytes);
        if (pixels && fread(pixels, 1, bytes, file) != bytes) {
            free(pixels);
            pixels = NULL;
        }
        *width = h.width;
        *height = h.height;
    }
    fclose(file);
    return pixels;
}

static void cache_write(const char *path, const unsigned char *pixels, int width, int height) {
    char temp[1100];
    snprintf(temp, sizeof(temp), "%s.new", path);
    FILE *file = fopen(temp, "wb");
    if (!file) return;
    FOCacheHeader h = {{'F', 'O', 'S', 'T'}, gCache.apkSize, gCache.apkTime, width, height};
    int ok = fwrite(&h, sizeof(h), 1, file) == 1 && fwrite(pixels, (size_t)width * height * 4, 1, file) == 1;
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temp, path) != 0) remove(temp);
}

// Reads the RGBA pixels of a PNG file, from the cache when it can.
static unsigned char *load_pixels(const char *path, int *width, int *height) {
    char cached[1100];
    unsigned char *pixels = NULL;
    if (gCache.dir[0]) {
        cache_path(path, cached, sizeof(cached));
        pixels = cache_read(cached, width, height);
    }
    if (!pixels) {
        size_t size = 0;
        unsigned char *png = fo_assets_read(path, &size);
        if (!png) return NULL;
        pixels = fo_image_decode(png, size, 1, width, height);
        free(png);
        if (!pixels) return NULL;
        if (gCache.dir[0]) cache_write(cached, pixels, *width, *height);
    }
    return pixels;
}

static FOTexture make_texture(const unsigned char *pixels, int width, int height) {
    FOTexture texture = fo_gfx_texture(pixels, width, height);
    gTextureBytes += (long)width * height * 4;
    return texture;
}

FOTexture fo_assets_texture(const char *path) {
    FOTexture texture = {0, 0, 0};
    int width = 0, height = 0;
    unsigned char *pixels = load_pixels(path, &width, &height);
    if (!pixels) return texture;
    texture = make_texture(pixels, width, height);
    free(pixels);
    return texture;
}

// Sorts each FO_CELL x FO_CELL cell of a region into empty, opaque, or
// mixed, from the alpha of the pixels.
static unsigned char *classify(const FORegion *r, const unsigned char *pixels, int pageW) {
    int cols = (r->w + FO_CELL - 1) / FO_CELL, rows = (r->h + FO_CELL - 1) / FO_CELL;
    unsigned char *cells = malloc((size_t)cols * rows);
    if (!cells) return NULL;
    for (int cy = 0; cy < rows; cy++) {
        for (int cx = 0; cx < cols; cx++) {
            int lo = 255, hi = 0;
            for (int y = cy * FO_CELL; y < (cy + 1) * FO_CELL && y < r->h; y++) {
                const unsigned char *row = pixels + ((size_t)(r->y + y) * pageW + r->x) * 4;
                for (int x = cx * FO_CELL; x < (cx + 1) * FO_CELL && x < r->w; x++) {
                    int a = row[x * 4 + 3];
                    if (a < lo) lo = a;
                    if (a > hi) hi = a;
                }
            }
            cells[cy * cols + cx] = hi == 0 ? FO_CELL_EMPTY : lo == 255 ? FO_CELL_OPAQUE : FO_CELL_MIXED;
        }
    }
    return cells;
}

int fo_assets_atlas(FOAtlasTextures *out, const char *path) {
    return fo_assets_atlas_cells(out, path, 0);
}

int fo_assets_atlas_cells(FOAtlasTextures *out, const char *path, int cells) {
    memset(out, 0, sizeof(*out));
    char *text = (char *)fo_assets_read(path, NULL);
    if (!text) return 0;
    int ok = fo_atlas_parse(&out->atlas, text);
    free(text);
    if (!ok) return 0;
    out->pages = calloc((size_t)out->atlas.page_count, sizeof(FOTexture));
    out->cells = calloc((size_t)out->atlas.region_count + 1, sizeof(unsigned char *));
    if (!out->pages || !out->cells) {
        free(out->pages);
        free(out->cells);
        fo_atlas_free(&out->atlas);
        return 0;
    }
    // The pages are beside the atlas file.
    const char *slash = strrchr(path, '/');
    int dirLength = slash ? (int)(slash - path + 1) : 0;
    for (int i = 0; i < out->atlas.page_count; i++) {
        char page[256];
        snprintf(page, sizeof(page), "%.*s%s", dirLength, path, out->atlas.pages[i].file);
        int width = 0, height = 0;
        unsigned char *pixels = load_pixels(page, &width, &height);
        if (pixels) {
            out->pages[i] = make_texture(pixels, width, height);
            for (int j = 0; cells && j < out->atlas.region_count; j++) {
                if (out->atlas.regions[j].page == i) out->cells[j] = classify(&out->atlas.regions[j], pixels, width);
            }
            free(pixels);
        }
        if (!out->pages[i].id) {
            fo_assets_atlas_free(out);
            return 0;
        }
    }
    return 1;
}

void fo_assets_atlas_free(FOAtlasTextures *atlas) {
    if (atlas->pages) {
        for (int i = 0; i < atlas->atlas.page_count; i++) {
            if (atlas->pages[i].id) gTextureBytes -= (long)atlas->pages[i].width * atlas->pages[i].height * 4;
            fo_gfx_texture_free(&atlas->pages[i]);
        }
    }
    free(atlas->pages);
    if (atlas->cells) {
        for (int i = 0; i < atlas->atlas.region_count; i++) free(atlas->cells[i]);
    }
    free(atlas->cells);
    fo_atlas_free(&atlas->atlas);
    memset(atlas, 0, sizeof(*atlas));
}

FOSprite fo_assets_sprite(const FOAtlasTextures *atlas, const char *name) {
    FOSprite sprite = {NULL, 0, 0, 0, 0, 0, 0};
    const FORegion *r = fo_atlas_find(&atlas->atlas, name, -1);
    if (!r) return sprite;
    const FOTexture *t = &atlas->pages[r->page];
    sprite.texture = t;
    sprite.u0 = (float)r->x / t->width;
    sprite.v0 = (float)r->y / t->height;
    sprite.u1 = (float)(r->x + r->w) / t->width;
    sprite.v1 = (float)(r->y + r->h) / t->height;
    sprite.w = r->w;
    sprite.h = r->h;
    return sprite;
}

FOLayer fo_assets_layer(const FOAtlasTextures *atlas, const char *name) {
    FOLayer layer = {fo_assets_sprite(atlas, name), 0, 0, NULL};
    const FORegion *r = fo_atlas_find(&atlas->atlas, name, -1);
    if (r && atlas->cells) {
        layer.cells = atlas->cells[r - atlas->atlas.regions];
        layer.cols = (r->w + FO_CELL - 1) / FO_CELL;
        layer.rows = (r->h + FO_CELL - 1) / FO_CELL;
    }
    return layer;
}

long fo_assets_texture_bytes(void) {
    return gTextureBytes;
}
