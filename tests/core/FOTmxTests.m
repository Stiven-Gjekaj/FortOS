// Tests of fo_tmx. Each test writes its own map text.

#import "FOTest.h"
#include <math.h>
#include "fo_tmx.h"

static const char *kMap =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<map version=\"1.0\" orientation=\"orthogonal\" width=\"1\" height=\"1\" tilewidth=\"1200\" "
    "tileheight=\"200\" nextobjectid=\"9\">\n"
    " <properties>\n"
    "  <property name=\"koth_cp_indicator_pos_x\" value=\"582\"/>\n"
    "  <property name=\"koth_cp_indicator_pos_y\" value=\"205.5\"/>\n"
    " </properties>\n"
    " <tileset firstgid=\"1\" name=\"main\" tilewidth=\"1200\" tileheight=\"200\"/>\n"
    " <objectgroup name=\"collisions\">\n"
    "  <object id=\"86\" x=\"10\" y=\"150\" width=\"200\" height=\"50\"/>\n"
    "  <object id=\"89\" x=\"400\" y=\"100\" width=\"10\" height=\"100\" rotation=\"-90\"/>\n"
    " </objectgroup>\n"
    " <objectgroup name=\"areas\" opacity=\"0.66\">\n"
    "  <object id=\"5\" name=\"spawnarea_blu\" x=\"42\" y=\"100\" width=\"74\" height=\"40\"/>\n"
    "  <object id=\"6\" name=\"playerdeatharea\" x=\"553\" y=\"180\" width=\"24\" height=\"20\"/>\n"
    "  <object id=\"7\" name=\"playerdeatharea\" x=\"624\" y=\"180\" width=\"25\" height=\"20\"/>\n"
    "  <object id=\"8\" name=\"something_else\" x=\"0\" y=\"0\" width=\"1\" height=\"1\"/>\n"
    " </objectgroup>\n"
    "</map>\n";

FO_TEST(testTmxReadsTheSize) {
    FOTmx map;
    FO_EXPECT(fo_tmx_parse(&map, kMap));
    FO_EXPECT(fabsf(map.width - 120) < 1e-4f);
    FO_EXPECT(fabsf(map.height - 20) < 1e-4f);
    FO_EXPECT(fabsf(map.koth_indicator_x - 58.2f) < 1e-4f);
    FO_EXPECT(fabsf(map.koth_indicator_y - 20.55f) < 1e-4f);
}

FO_TEST(testTmxTurnsTheBoxes) {
    FOTmx map;
    fo_tmx_parse(&map, kMap);
    // Two boxes of the file and four walls.
    FO_EQUAL_INT(map.box_count, 6);
    FO_EXPECT(fabsf(map.boxes[0].x - 1) < 1e-4f);
    FO_EXPECT(fabsf(map.boxes[0].y - 5) < 1e-4f);
    FO_EXPECT(fabsf(map.boxes[0].w - 20) < 1e-4f);
    FO_EXPECT(fabsf(map.boxes[0].h - 5) < 1e-4f);
    FO_EXPECT(fabsf(map.boxes[0].angle - 2 * (float)M_PI) < 1e-4f);
    FO_EXPECT(fabsf(map.boxes[1].angle - 450 * (float)M_PI / 180) < 1e-4f);
    // The bottom wall lies at y 0 under the map.
    FO_EXPECT(fabsf(map.boxes[3].y) < 1e-4f);
    FO_EXPECT(fabsf(map.boxes[3].w - 120) < 1e-4f);
}

FO_TEST(testTmxReadsTheAreas) {
    FOTmx map;
    fo_tmx_parse(&map, kMap);
    FO_EQUAL_INT(map.area_count, 3);
    const FORect *spawn = fo_tmx_area(&map, FO_AREA_SPAWN_BLU);
    FO_EXPECT(spawn != NULL);
    FO_EXPECT(fabsf(spawn->x - 4.2f) < 1e-4f);
    // The bottom of the rectangle: 20 - 10 - 4 meters.
    FO_EXPECT(fabsf(spawn->y - 6) < 1e-4f);
    FO_EXPECT(fo_rect_contains(spawn, 5, 7));
    FO_EXPECT(!fo_rect_contains(spawn, 5, 11));
    FO_EXPECT(fo_tmx_area(&map, FO_AREA_SPAWN_RED) == NULL);
    FO_EQUAL_INT(map.areas[2].kind, FO_AREA_DEATH);
}

FO_TEST(testTmxRefusesOtherText) {
    FOTmx map;
    FO_EXPECT(!fo_tmx_parse(&map, "<html></html>"));
}
