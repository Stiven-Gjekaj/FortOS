// fo_assets: reads the files of GANGFORT from the APK of the owner of the
// phone, and makes textures and sprites from them. The decoded pixels go
// into a cache beside the APK, so that the next start is fast.

#ifndef FO_ASSETS_H
#define FO_ASSETS_H

#include <stddef.h>
#include "fo_atlas.h"
#include "fo_gfx.h"

// An atlas with a texture for each page. cells holds the cell classes of
// each region, when fo_assets_atlas_cells made them.
typedef struct {
    FOAtlas atlas;
    FOTexture *pages;
    unsigned char **cells;
} FOAtlasTextures;

// The size of a cell of a map layer in art pixels, and the cell classes.
#define FO_CELL 16
enum {
    FO_CELL_EMPTY = 0,
    FO_CELL_OPAQUE = 1,
    FO_CELL_MIXED = 2,
};

// A big image of a map, with the class of each cell. The cells go row
// by row from the top, as the pixels do.
typedef struct {
    FOSprite sprite;
    int cols, rows;
    const unsigned char *cells;  // cols * rows classes, or NULL.
} FOLayer;

// Opens the APK at apk_path. Returns 0 when it is not a zip archive or it
// does not hold the files of GANGFORT.
int fo_assets_open(const char *apk_path);
void fo_assets_close(void);

// Reads the file at path under assets/res/ of the APK, for example
// "texts/gangforttexts.properties". The caller frees the buffer. The
// buffer ends with a zero byte that size does not count.
unsigned char *fo_assets_read(const char *path, size_t *size);

// Reads an atlas, for example "textures/sprites.atlas", and makes the
// texture of each page. Returns 0 when a file is not there.
int fo_assets_atlas(FOAtlasTextures *out, const char *path);
void fo_assets_atlas_free(FOAtlasTextures *atlas);

// As fo_assets_atlas. With cells at 1, it also sorts the cells of each
// region into empty, opaque, and mixed, for fo_assets_layer.
int fo_assets_atlas_cells(FOAtlasTextures *out, const char *path, int cells);

// The layer of the region with the name, with its cells.
FOLayer fo_assets_layer(const FOAtlasTextures *atlas, const char *name);

// The sprite of the region with the name. The sprite has w = 0 when the
// atlas does not have the region.
FOSprite fo_assets_sprite(const FOAtlasTextures *atlas, const char *name);

// Makes a texture from a PNG file, for example
// "textures/sniper_aim_alpha_mask.png".
FOTexture fo_assets_texture(const char *path);

// The number of bytes of texture memory that the loaded textures use.
long fo_assets_texture_bytes(void);

#endif
