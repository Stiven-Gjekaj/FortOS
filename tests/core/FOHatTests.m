// Tests of the loadouts, the hats that fall at a death, and the golden
// drops. Each test builds its own map with a long floor.

#import "FOTest.h"
#include "fo_items.h"
#include "fo_world.h"

static const char *kFloor =
    "<map width=\"1\" height=\"1\" tilewidth=\"1000\" tileheight=\"300\">\n"
    " <objectgroup name=\"collisions\">\n"
    "  <object id=\"1\" x=\"0\" y=\"280\" width=\"1000\" height=\"20\"/>\n"
    " </objectgroup>\n"
    " <objectgroup name=\"areas\">\n"
    "  <object id=\"2\" name=\"spawnarea_blu\" x=\"0\" y=\"200\" width=\"80\" height=\"80\"/>\n"
    "  <object id=\"3\" name=\"spawnarea_red\" x=\"920\" y=\"200\" width=\"80\" height=\"80\"/>\n"
    " </objectgroup>\n"
    "</map>\n";

static void run(FOWorld *world, int frames) {
    for (int i = 0; i < frames; i++) fo_world_update(world, 1 / 60.0f);
}

FO_TEST(testHatLoadoutTakesEffectAtTheSpawn) {
    FOWorld *world = fo_world_new(kFloor, 3);
    FOPlayer *p = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_player_set_loadout(p, FO_CLASS_HEAVY, 1, FO_ITEM_UDDERGUN);
    fo_world_spawn(world, p, FO_CLASS_SCOUT);
    FO_EQUAL_INT(p->hat, 0);
    fo_world_spawn(world, p, FO_CLASS_HEAVY);
    FO_EQUAL_INT(p->hat, 1);
    FO_EQUAL_INT(p->weapon.def->item, FO_ITEM_UDDERGUN);
    // A weapon is not a hat, and a hat is not a weapon.
    fo_player_set_loadout(p, FO_CLASS_HEAVY, FO_ITEM_UDDERGUN, 1);
    FO_EQUAL_INT(p->loadout[FO_CLASS_HEAVY].hat, 0);
    FO_EQUAL_INT(p->loadout[FO_CLASS_HEAVY].weapon, 0);
    fo_world_free(world);
}

FO_TEST(testHatFallsAtTheDeathAndGoesAtTheSpawn) {
    FOWorld *world = fo_world_new(kFloor, 3);
    FOPlayer *p = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_player_set_loadout(p, FO_CLASS_SOLDIER, 4, 0);
    fo_world_spawn(world, p, FO_CLASS_SOLDIER);
    fo_phys_body_set_transform(p->obj.body, 50, 3.5f, 0);
    run(world, 30);
    // With no place from the renderer, no hat falls.
    p->hatPlaced = 0;
    fo_player_set_health(world, p, 0);
    FO_EQUAL_INT(p->fallenHat.active, 0);
    fo_world_spawn(world, p, FO_CLASS_SOLDIER);
    fo_player_hat_place(p, 50, 5);
    fo_player_set_health(world, p, 0);
    FO_EQUAL_INT(p->fallenHat.active, 1);
    FO_EQUAL_INT(p->fallenHat.item, 4);
    run(world, 120);
    float x, y;
    fo_phys_body_position(p->fallenHat.obj.body, &x, &y);
    // The floor is at 2 m. The hat lies on it.
    FO_EXPECT(y > 2.0f && y < 2.5f);
    fo_world_spawn(world, p, FO_CLASS_SOLDIER);
    FO_EQUAL_INT(p->fallenHat.active, 0);
    FO_EXPECT(p->fallenHat.obj.body == NULL);
    fo_world_free(world);
}

FO_TEST(testHatGoesNineSecondsAfterTheDeath) {
    FOWorld *world = fo_world_new(kFloor, 3);
    FOPlayer *p = fo_world_add_player(world, 1, FO_TEAM_RED);
    fo_player_set_loadout(p, FO_CLASS_SCOUT, 12, 0);
    fo_world_spawn(world, p, FO_CLASS_SCOUT);
    fo_player_hat_place(p, 40, 5);
    fo_player_set_health(world, p, 0);
    FO_EQUAL_INT(p->fallenHat.active, 1);
    run(world, 8 * 60);
    FO_EQUAL_INT(p->fallenHat.active, 1);
    run(world, 70);
    FO_EQUAL_INT(p->fallenHat.active, 0);
    fo_world_free(world);
}

FO_TEST(testDropsJumpAndGoAfterFourSeconds) {
    FOWorld *world = fo_world_new(kFloor, 3);
    fo_world_drops(world, 30, 4, 5);
    int n = 0;
    for (int i = 0; i < FO_MAX_DROPS; i++) n += world->drops[i].active;
    FO_EQUAL_INT(n, 5);
    run(world, 10);
    float x, y;
    fo_phys_body_position(world->drops[0].obj.body, &x, &y);
    FO_EXPECT(y > 4.0f);
    run(world, 4 * 60);
    n = 0;
    for (int i = 0; i < FO_MAX_DROPS; i++) n += world->drops[i].active;
    FO_EQUAL_INT(n, 0);
    fo_world_free(world);
}
