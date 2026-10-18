// fo_maps: the three maps of single player, as the map classes of
// GANGFORT (GTS_Gantown acp.java, BOMB_Canyon acj.java, FLAG_Eypt
// aco.java). It loads the art and the world of a map, and draws the map.

#ifndef FO_MAPS_H
#define FO_MAPS_H

#include "fo_assets.h"
#include "fo_world.h"

typedef enum {
    FO_MAP_GANTOWN = 0,
    FO_MAP_BOMB_CANYON = 1,
    FO_MAP_FLAG_EGYPT = 2,
    FO_MAP_COUNT = 3,
} FOMapId;

typedef struct {
    FOMapId id;
    FOAtlasTextures atlas;
    FOWorld *world;
    float cloudX, cloudY, cloudSpeed;
} FOMap;

// The name of the map in the files and in the single player screen.
const char *fo_map_name(FOMapId id);

// Loads the art and makes the world. Returns 0 when a file is not there.
int fo_map_load(FOMap *map, FOMapId id, unsigned int seed);
void fo_map_free(FOMap *map);

// Moves the clouds (the map classes, a(float)).
void fo_map_update(FOMap *map, float dt);

// Draws the layers behind the players, from the background to the
// platforms. The view is the rectangle of fo_gfx_view.
void fo_map_draw_back(const FOMap *map, const FOAtlasTextures *sprites, float camX, float left, float bottom,
                      float right, float top);

// Draws the parts in front of the players: the spawn doors and the fronts
// of the spawn rooms.
void fo_map_draw_front(const FOMap *map, const FOAtlasTextures *sprites, float left, float bottom, float right,
                       float top);

#endif
