// fo_atlas: reads the texture atlas files of libGDX, such as
// res/textures/sprites.atlas. An atlas has one or more pages. Each page is
// a PNG file with named regions.

#ifndef FO_ATLAS_H
#define FO_ATLAS_H

typedef struct {
    char name[64];
    int page;           // The index of the page that holds the region.
    int x, y;           // The top left texel in the page.
    int w, h;           // The size in the page.
    int rotate;         // 1 when the packer turned the region.
    int orig_w, orig_h; // The size before the packer cut the empty edges.
    int off_x, off_y;   // The place of the cut region in the original.
    int index;          // The frame number, or -1.
} FORegion;

typedef struct {
    char file[64];      // The name of the PNG file of the page.
    int w, h;
} FOPage;

typedef struct {
    FOPage *pages;
    int page_count;
    FORegion *regions;
    int region_count;
} FOAtlas;

// Reads the text of an atlas file. Returns 0 when the text is not an
// atlas. fo_atlas_free frees the lists.
int fo_atlas_parse(FOAtlas *atlas, const char *text);
void fo_atlas_free(FOAtlas *atlas);

// The region with the name and the index, or NULL. An index of -1 finds
// the first region with the name.
const FORegion *fo_atlas_find(const FOAtlas *atlas, const char *name, int index);

#endif
