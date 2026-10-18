// Tests of fo_atlas. Each test writes its own atlas text.

#import "FOTest.h"
#include "fo_atlas.h"

static const char *kTwoPages =
    "\n"
    "one.png\n"
    "size: 2048,256\n"
    "format: RGBA8888\n"
    "filter: Nearest,Nearest\n"
    "repeat: none\n"
    "hat_red\n"
    "  rotate: false\n"
    "  xy: 437, 3\n"
    "  size: 14, 13\n"
    "  orig: 16, 15\n"
    "  offset: 1, 2\n"
    "  index: -1\n"
    "legs\n"
    "  rotate: true\n"
    "  xy: 10, 20\n"
    "  size: 4, 9\n"
    "  orig: 4, 9\n"
    "  offset: 0, 0\n"
    "  index: 3\n"
    "\n"
    "two.png\r\n"
    "size: 512, 64\r\n"
    "format: RGBA8888\r\n"
    "bg\r\n"
    "  xy: 2, 4\r\n"
    "  size: 100, 50\r\n";

FO_TEST(testAtlasReadsPagesAndRegions) {
    FOAtlas atlas;
    FO_EXPECT(fo_atlas_parse(&atlas, kTwoPages));
    FO_EQUAL_INT(atlas.page_count, 2);
    FO_EQUAL(@(atlas.pages[0].file), @"one.png");
    FO_EQUAL_INT(atlas.pages[0].w, 2048);
    FO_EQUAL_INT(atlas.pages[1].h, 64);
    FO_EQUAL_INT(atlas.region_count, 3);

    const FORegion *hat = fo_atlas_find(&atlas, "hat_red", -1);
    FO_EXPECT(hat != NULL);
    FO_EQUAL_INT(hat->page, 0);
    FO_EQUAL_INT(hat->x, 437);
    FO_EQUAL_INT(hat->y, 3);
    FO_EQUAL_INT(hat->w, 14);
    FO_EQUAL_INT(hat->h, 13);
    FO_EQUAL_INT(hat->orig_w, 16);
    FO_EQUAL_INT(hat->off_y, 2);
    FO_EQUAL_INT(hat->rotate, 0);

    const FORegion *bg = fo_atlas_find(&atlas, "bg", -1);
    FO_EXPECT(bg != NULL);
    FO_EQUAL_INT(bg->page, 1);
    FO_EQUAL_INT(bg->w, 100);
    fo_atlas_free(&atlas);
}

FO_TEST(testAtlasFindsTheIndex) {
    FOAtlas atlas;
    fo_atlas_parse(&atlas, kTwoPages);
    FO_EXPECT(fo_atlas_find(&atlas, "legs", 3) != NULL);
    FO_EXPECT(fo_atlas_find(&atlas, "legs", 2) == NULL);
    FO_EQUAL_INT(fo_atlas_find(&atlas, "legs", -1)->rotate, 1);
    FO_EXPECT(fo_atlas_find(&atlas, "nothing", -1) == NULL);
    fo_atlas_free(&atlas);
}

FO_TEST(testAtlasRefusesEmptyText) {
    FOAtlas atlas;
    FO_EXPECT(!fo_atlas_parse(&atlas, ""));
    FO_EXPECT(!fo_atlas_parse(&atlas, "\n\n"));
}
