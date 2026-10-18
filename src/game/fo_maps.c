#include "fo_maps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fo_layer.h"

#define FO_ART 0.1f

typedef struct {
    short team;
    float x, y, w, h;
    const char *sprite;
} FODoorDef;

typedef struct {
    const char *name;
    const char *tmx;
    const char *atlas;
    int silos;          // 1 when the map has the silos layer.
    int front;          // 1 when the map has the spawnbasesfront layer.
    float cloudY;
    FODoorDef doors[4];
    int door_count;
} FOMapDef;

static const FOMapDef kMaps[FO_MAP_COUNT] = {
    {"gts_gantown", "maps/gts_gantown/gantown.tmx", "maps/gts_gantown/gantown.atlas", 1, 1, 0,
     {{1, 7.75f, 23.6f, 1.3000001f, 3.0f, "spawndoors_blu"}, {2, 108.3f, 23.6f, 1.2f, 3.0f, "spawndoors_red"}}, 2},
    {"bomb_canyon", "maps/bomb_canyon/bomb_canyon.tmx", "maps/bomb_canyon/bomb_canyon.atlas", 0, 1, 0,
     {{1, 96.950005f, 23.800001f, 1.3000001f, 3.0f, "spawndoors_blu"},
      {1, 88.05f, 23.800001f, 1.3000001f, 3.0f, "spawndoors_blu"},
      {2, 7.7000003f, 14.8f, 1.2f, 3.0f, "spawndoors_red"}},
     3},
    {"flag_egypt", "maps/flag_egypt/flag_egypt.tmx", "maps/flag_egypt/flag_egypt.atlas", 0, 0, 16.0f,
     {{1, 3.3f, 16.9f, 1.4f, 3.0f, "spawndoors2_blu"},
      {1, 12.5f, 16.9f, 1.4f, 3.0f, "spawndoors2_blu"},
      {2, 107.5f, 16.9f, 1.4f, 3.0f, "spawndoors2_red"},
      {2, 116.700005f, 16.9f, 1.4f, 3.0f, "spawndoors2_red"}},
     4},
};

const char *fo_map_name(FOMapId id) {
    return kMaps[id].name;
}

// The map classes, b(): the clouds start at the right edge with a new speed.
static void reset_clouds(FOMap *map) {
    map->cloudX = map->world->map.width;
    // MathUtils.random(1, 10) * 0.1: from 0.1 to 1 m/s.
    map->cloudSpeed = (1 + (rand() % 1000) / 1000.0f * 9) * 0.1f;
}

int fo_map_load(FOMap *map, FOMapId id, unsigned int seed) {
    memset(map, 0, sizeof(*map));
    map->id = id;
    const FOMapDef *def = &kMaps[id];
    char *tmx = (char *)fo_assets_read(def->tmx, NULL);
    if (!tmx) return 0;
    map->world = fo_world_new(tmx, seed);
    free(tmx);
    if (!map->world || !fo_assets_atlas_cells(&map->atlas, def->atlas, 1)) {
        fo_map_free(map);
        return 0;
    }
    for (int i = 0; i < def->door_count; i++) {
        const FODoorDef *d = &def->doors[i];
        fo_world_add_door(map->world, d->team, d->x, d->y, d->w, d->h);
    }
    // The game mode of each map: GantownMap (acp.java), BombCanyonMap
    // (acj.java:34-39), and FlagEgyptMap (aco.java:42-43).
    FOWorld *w = map->world;
    switch (id) {
    case FO_MAP_GANTOWN: fo_world_koth(w); break;
    case FO_MAP_BOMB_CANYON:
        fo_world_bomb(w, 1, 15.3f, 19.0f, 99.200005f, 2.3f);
        fo_world_bomb_checkpoint(w, 47.0f, 10.6f);
        fo_world_bomb_checkpoint(w, 78.5f, 16.9f);
        fo_world_bomb_checkpoint(w, 13.1f, 16.9f);
        break;
    case FO_MAP_FLAG_EGYPT: fo_world_ctf(w, 113.65f, 11.1f, 6.75f, 11.1f); break;
    default: break;
    }
    map->cloudY = def->cloudY;
    reset_clouds(map);
    return 1;
}

void fo_map_free(FOMap *map) {
    fo_world_free(map->world);
    fo_assets_atlas_free(&map->atlas);
    memset(map, 0, sizeof(*map));
}

void fo_map_update(FOMap *map, float dt) {
    map->cloudX -= map->cloudSpeed * dt;
    FOSprite clouds = fo_assets_sprite(&map->atlas, "clouds");
    if (map->cloudX < -clouds.w * FO_ART) reset_clouds(map);
}

// The depth of each layer. The nearer layer has the smaller depth.
enum { Z_BG = 9, Z_CLOUDS = 8, Z_SILOS = 7, Z_MAIN = 6, Z_PLATFORMS = 5, Z_FRONT = 2 };

static void draw_layer(const FOMap *map, const char *name, int pass, float x, float y, int z, float left,
                       float bottom, float right, float top) {
    FOLayer layer = fo_assets_layer(&map->atlas, name);
    fo_layer_draw(&layer, pass, x, y, FO_ART, z / 10.0f, left, bottom, right, top);
}

// SpawnDoor.a(alh) (xw.java): the door slides up into the wall, so only
// the part under its closed top shows.
static void draw_doors(const FOMap *map, const FOAtlasTextures *sprites) {
    const FOMapDef *def = &kMaps[map->id];
    for (int i = 0; i < map->world->door_count; i++) {
        const FODoor *d = &map->world->doors[i];
        FOSprite s = fo_assets_sprite(sprites, def->doors[i].sprite);
        if (!s.w) continue;
        float x, y;
        fo_phys_body_position(d->obj.body, &x, &y);
        float shown = d->obj.h - (y - d->y);
        if (shown <= 0) continue;
        float part = shown / d->obj.h;
        if (part > 1) part = 1;
        FOSprite cut = s;
        cut.v0 = s.v1 - (s.v1 - s.v0) * part;
        fo_gfx_draw(&cut, x - d->obj.w / 2, y - d->obj.h / 2, d->obj.w, d->obj.h * part, 0, 0, 0, 0, FO_WHITE);
    }
}

void fo_map_draw_back(const FOMap *map, const FOAtlasTextures *sprites, float camX, float left, float bottom,
                      float right, float top) {
    const FOMapDef *def = &kMaps[map->id];
    for (int pass = FO_PASS_OPAQUE; pass <= FO_PASS_BLEND; pass++) {
        // The parallax of the background and the silos (acp.java:68-69).
        draw_layer(map, "bg", pass, -camX * 0.03f, 0, Z_BG, left, bottom, right, top);
        draw_layer(map, "clouds", pass, map->cloudX, map->cloudY, Z_CLOUDS, left, bottom, right, top);
        if (def->silos) draw_layer(map, "silos", pass, -camX * 0.025f, 0, Z_SILOS, left, bottom, right, top);
        draw_layer(map, "main", pass, 0, 0, Z_MAIN, left, bottom, right, top);
        draw_layer(map, "platforms", pass, 0, 0, Z_PLATFORMS, left, bottom, right, top);
    }
    fo_gfx_depth(FO_DEPTH_OFF, 0);
    (void)sprites;
}

void fo_map_draw_front(const FOMap *map, const FOAtlasTextures *sprites, float left, float bottom, float right,
                       float top) {
    // The doors are in front of the players (spawnDoors in wz.java).
    fo_gfx_depth(FO_DEPTH_OFF, 0);
    draw_doors(map, sprites);
    if (!kMaps[map->id].front) return;
    for (int pass = FO_PASS_OPAQUE; pass <= FO_PASS_BLEND; pass++) {
        draw_layer(map, "spawnbasesfront", pass, 0, 0, Z_FRONT, left, bottom, right, top);
    }
    fo_gfx_depth(FO_DEPTH_OFF, 0);
}
