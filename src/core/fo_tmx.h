// fo_tmx: reads the map data of a Tiled .tmx file of GANGFORT, as
// MapDataLoader (akm.java) does. It reads the size of the map, the
// "collisions" boxes, the "areas" rectangles, and the map properties. All
// values are in meters with y up, at 0.1 m for each pixel.

#ifndef FO_TMX_H
#define FO_TMX_H

#define FO_TMX_MAX_BOXES 160
#define FO_TMX_MAX_AREAS 32

// A static box of the ground. (x, y) is the top left corner. The box turns
// by angle radians around that corner, as Box2D turns the body.
typedef struct {
    float x, y, w, h;
    float angle;
} FOTmxBox;

// A rectangle with its bottom left corner at (x, y).
typedef struct {
    float x, y, w, h;
} FORect;

// The kinds of areas, in the order of MapData (acr.java).
typedef enum {
    FO_AREA_SPAWN_RED,
    FO_AREA_SPAWN_BLU,
    FO_AREA_KOTH,
    FO_AREA_FRIDGE_RED,
    FO_AREA_FRIDGE_BLU,
    FO_AREA_FLIES,
    FO_AREA_BIRDS,
    FO_AREA_NO_SENTRY,
    FO_AREA_DEATH,
    FO_AREA_BOMB_FINISH,
    FO_AREA_BOMB_FALL,
    FO_AREA_CAPTURE_BLU,
    FO_AREA_CAPTURE_RED,
    FO_AREA_KIND_COUNT,
} FOAreaKind;

typedef struct {
    FOAreaKind kind;
    FORect rect;
} FOTmxArea;

typedef struct {
    float width, height;  // The size of the map in meters.
    FOTmxBox boxes[FO_TMX_MAX_BOXES];
    int box_count;        // The boxes of the file, then the 4 edge walls.
    FOTmxArea areas[FO_TMX_MAX_AREAS];
    int area_count;
    float koth_indicator_x, koth_indicator_y;
} FOTmx;

// Reads the text of a .tmx file. Returns 0 when it is not a map.
int fo_tmx_parse(FOTmx *map, const char *text);

// The first area of the kind, or NULL.
const FORect *fo_tmx_area(const FOTmx *map, FOAreaKind kind);

// 1 when the point is inside the rectangle, as Rectangle.contains.
int fo_rect_contains(const FORect *rect, float x, float y);

#endif
