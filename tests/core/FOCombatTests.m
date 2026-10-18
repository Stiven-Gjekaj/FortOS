// Tests of the weapons, the projectiles, the damage, and the kill credit.
// Each test builds its own map with a long floor.

#import "FOTest.h"
#include <math.h>
#include "fo_world.h"

static const char *kFlat =
    "<map width=\"1\" height=\"1\" tilewidth=\"1000\" tileheight=\"300\">\n"
    " <objectgroup name=\"collisions\">\n"
    "  <object id=\"1\" x=\"0\" y=\"280\" width=\"1000\" height=\"20\"/>\n"
    "  <object id=\"2\" x=\"600\" y=\"200\" width=\"10\" height=\"80\"/>\n"
    " </objectgroup>\n"
    " <objectgroup name=\"areas\">\n"
    "  <object id=\"3\" name=\"spawnarea_blu\" x=\"0\" y=\"200\" width=\"80\" height=\"80\"/>\n"
    "  <object id=\"4\" name=\"spawnarea_red\" x=\"920\" y=\"200\" width=\"80\" height=\"80\"/>\n"
    "  <object id=\"5\" name=\"fridge_blu\" x=\"100\" y=\"240\" width=\"30\" height=\"40\"/>\n"
    " </objectgroup>\n"
    "</map>\n";

static void run(FOWorld *world, int frames) {
    for (int i = 0; i < frames; i++) fo_world_update(world, 1 / 60.0f);
}

static FOPlayer *standing(FOWorld *world, int id, short team, int cls, float x) {
    FOPlayer *p = fo_world_add_player(world, id, team);
    fo_world_spawn(world, p, cls);
    fo_phys_body_set_transform(p->obj.body, x, 3.5f, 0);
    run(world, 60);
    return p;
}

static int activeBullets(FOWorld *world) {
    int n = 0;
    for (int i = 0; i < FO_MAX_BULLETS; i++) n += world->bullets[i].active;
    return n;
}

FO_TEST(testCombatSpawnGivesTheWeaponOfTheClass) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    FO_EXPECT(p->weapon.def != NULL);
    FO_EQUAL_INT(p->weapon.def->item, FO_ITEM_ROCKET_LAUNCHER);
    FO_EQUAL_INT((int)p->weapon.ammo, 20);
    // A loadout weapon of the class replaces the default one.
    fo_player_set_loadout(p, FO_CLASS_HEAVY, 0, FO_ITEM_UDDERGUN);
    fo_world_spawn(world, p, FO_CLASS_HEAVY);
    FO_EQUAL_INT(p->weapon.def->item, FO_ITEM_UDDERGUN);
    // A loadout weapon of another class does not.
    fo_player_set_loadout(p, FO_CLASS_SCOUT, 0, FO_ITEM_UDDERGUN);
    FO_EQUAL_INT(p->loadout[FO_CLASS_SCOUT].weapon, 0);
    fo_world_spawn(world, p, FO_CLASS_SCOUT);
    FO_EQUAL_INT(p->weapon.def->item, FO_ITEM_SAWNGUN);
    fo_world_free(world);
}

FO_TEST(testCombatShotUsesAmmoAndWaits) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    FO_EXPECT(fo_player_shoot(world, p, 0));
    FO_EQUAL_INT((int)p->weapon.ammo, 19);
    FO_EQUAL_INT(activeBullets(world), 1);
    // The rocket launcher waits 800 ms between two shots.
    run(world, 47);
    FO_EXPECT(!fo_player_shoot(world, p, 0));
    run(world, 2);
    FO_EXPECT(fo_player_shoot(world, p, 0));
    fo_world_free(world);
}

FO_TEST(testCombatRocketHitsAnEnemy) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_HEAVY, 26);
    fo_player_shoot(world, p, 0);
    run(world, 60);
    FO_EQUAL_INT((int)e->health, 300 - 60);
    FO_EQUAL_INT(activeBullets(world), 0);
    fo_world_free(world);
}

FO_TEST(testCombatBulletPassesATeammate) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    FOPlayer *mate = standing(world, 2, FO_TEAM_BLU, FO_CLASS_HEAVY, 24);
    FOPlayer *e = standing(world, 3, FO_TEAM_RED, FO_CLASS_HEAVY, 30);
    fo_player_shoot(world, p, 0);
    run(world, 90);
    FO_EQUAL_INT((int)mate->health, 300);
    FO_EQUAL_INT((int)e->health, 240);
    fo_world_free(world);
}

FO_TEST(testCombatKillGivesPoints) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *a = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    FOPlayer *b = standing(world, 2, FO_TEAM_BLU, FO_CLASS_SCOUT, 18);
    FOPlayer *e = standing(world, 3, FO_TEAM_RED, FO_CLASS_SCOUT, 30);
    // b does 40 first, then a does 60 + 60 and kills: a is the killer
    // with the most damage, and b assists.
    fo_player_damage(world, e, b->id, FO_ITEM_SAWNGUN, 40);
    fo_player_damage(world, e, a->id, FO_ITEM_ROCKET_LAUNCHER, 60);
    FO_EXPECT(e->alive);
    fo_player_damage(world, e, a->id, FO_ITEM_ROCKET_LAUNCHER, 60);
    FO_EXPECT(!e->alive);
    FO_EQUAL_INT(a->score, 2);
    FO_EQUAL_INT(b->score, 1);
    FO_EQUAL_INT(e->killer, a->id);
    int sawKill = 0;
    for (int i = 0; i < world->event_count; i++) {
        if (world->events[i].kind == FO_EVENT_KILL) {
            sawKill = 1;
            FO_EQUAL_INT(world->events[i].a, a->id);
            FO_EQUAL_INT(world->events[i].c, b->id);
            FO_EQUAL_INT(world->events[i].item, FO_ITEM_ROCKET_LAUNCHER);
        }
    }
    FO_EXPECT(sawKill);
    fo_world_free(world);
}

FO_TEST(testCombatMostDamageIsTheKiller) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *a = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    FOPlayer *b = standing(world, 2, FO_TEAM_BLU, FO_CLASS_SCOUT, 18);
    FOPlayer *e = standing(world, 3, FO_TEAM_RED, FO_CLASS_SCOUT, 30);
    fo_player_damage(world, e, b->id, FO_ITEM_SAWNGUN, 100);
    fo_player_damage(world, e, a->id, FO_ITEM_ROCKET_LAUNCHER, 60);
    FO_EQUAL_INT(e->killer, b->id);
    FO_EQUAL_INT(b->score, 2);
    FO_EQUAL_INT(a->score, 1);
    fo_world_free(world);
}

FO_TEST(testCombatDeathWithNoDamageGivesTheVictimThePoints) {
    // KillEventData.java:69-72: with no damage in the history, the victim
    // is the killer, and xn.java:788 gives it 2 points.
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *e = standing(world, 3, FO_TEAM_RED, FO_CLASS_SCOUT, 30);
    fo_player_set_health(world, e, 0);
    FO_EQUAL_INT(e->score, 2);
    FO_EQUAL_INT(e->killer, 3);
    int kills = 0, item = -1;
    for (int i = 0; i < world->event_count; i++) {
        if (world->events[i].kind == FO_EVENT_KILL) kills++, item = world->events[i].item;
    }
    FO_EQUAL_INT(kills, 1);
    FO_EQUAL_INT(item, 0);
    fo_world_free(world);
}

FO_TEST(testCombatWallBlocksTheShot) {
    FOWorld *world = fo_world_new(kFlat, 3);
    // The wall is at x 60 to 61 m. The player stands with the muzzle in it.
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 59.4f);
    fo_player_set_aim(p, 1);
    FO_EXPECT(!fo_player_can_shoot(world, p));
    fo_player_set_aim(p, 179);
    p->facingRight = 0;
    FO_EXPECT(fo_player_can_shoot(world, p));
    fo_world_free(world);
}

FO_TEST(testCombatRocketJump) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    fo_player_jump(world, p);
    run(world, 20);
    float x, y0, y1, vx, vy;
    fo_player_position(p, &x, &y0);
    fo_player_set_aim(p, -90);
    FO_EXPECT(fo_player_shoot(world, p, -90 * 0.017453292f));
    run(world, 12);
    fo_player_velocity(p, &vx, &vy);
    fo_player_position(p, &x, &y1);
    // The blast pushed the soldier up and took the same health.
    FO_EXPECT(p->health < 200 && p->health >= 188);
    FO_EXPECT(vy > 0);
    FO_EQUAL_INT(p->score, 0);
    fo_world_free(world);
}

FO_TEST(testCombatBulletEndsAfterItsLife) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SCOUT, 20);
    fo_player_set_aim(p, 60);
    fo_player_shoot(world, p, 60 * 0.017453292f);
    FO_EQUAL_INT(activeBullets(world), 1);
    run(world, 58);
    FO_EQUAL_INT(activeBullets(world), 1);
    run(world, 4);
    FO_EQUAL_INT(activeBullets(world), 0);
    fo_world_free(world);
}

FO_TEST(testCombatShotSlowsTheWalk) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_HEAVY, 20);
    p->move = FO_MOVE_RIGHT;
    fo_player_shoot(world, p, 0);
    run(world, 2);
    float vx, vy;
    fo_player_velocity(p, &vx, &vy);
    FO_EXPECT(fabsf(vx - 6.16f * 0.3f) < 0.01f);
    // The minigun slows the walk for 2 x 100 ms.
    run(world, 14);
    fo_player_velocity(p, &vx, &vy);
    FO_EXPECT(fabsf(vx - 6.16f) < 0.01f);
    fo_world_free(world);
}

FO_TEST(testCombatAimAngleWraps) {
    FOPlayer p = {0};
    fo_player_set_aim(&p, 270);
    FO_EXPECT(fabsf(p.weapon.angle + 90) < 1e-4f);
    fo_player_set_aim(&p, -190);
    FO_EXPECT(fabsf(p.weapon.angle - 170) < 1e-4f);
    fo_player_set_aim(&p, 180);
    FO_EXPECT(fabsf(p.weapon.angle - 180) < 1e-4f);
}

FO_TEST(testCombatCorpseFallsAndGoes) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *e = standing(world, 3, FO_TEAM_RED, FO_CLASS_SCOUT, 30);
    fo_phys_body_set_transform(e->obj.body, 30, 8, 0);
    fo_player_set_health(world, e, 0);
    FO_EXPECT(e->corpse.active);
    run(world, 120);
    float x, y;
    fo_phys_body_position(e->corpse.obj.body, &x, &y);
    FO_EXPECT(e->corpse.stop > 0);
    // The floor is at y 2 m, and the body is 1.9 m high.
    FO_EXPECT(fabsf(y - 2.95f) < 0.1f);
    run(world, 60 * 8);
    FO_EXPECT(!e->corpse.active);
    fo_world_free(world);
}

FO_TEST(testCombatFridgeRefills) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    p->health = 50;
    p->weapon.ammo = 3;
    // The fridge is at x 10 to 13 m.
    fo_phys_body_set_transform(p->obj.body, 11.5f, 3, 0);
    run(world, 2);
    FO_EQUAL_INT((int)p->health, 200);
    FO_EQUAL_INT((int)p->weapon.ammo, 20);
    // The fridge of the other team does nothing.
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_SOLDIER, 25);
    e->health = 50;
    fo_phys_body_set_transform(e->obj.body, 11.5f, 3, 0);
    run(world, 2);
    FO_EQUAL_INT((int)e->health, 50);
    fo_world_free(world);
}

FO_TEST(testCombatPickupGivesAmmo) {
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_HEAVY, 30);
    fo_player_set_health(world, e, 0);
    FO_EXPECT(e->pickup.active);
    run(world, 60);
    // A player of any team that walks over it gets 30 % of the full ammo.
    FOPlayer *p = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SOLDIER, 26);
    p->weapon.ammo = 4;
    p->move = FO_MOVE_RIGHT;
    run(world, 90);
    FO_EXPECT(!e->pickup.active);
    FO_EQUAL_INT((int)p->weapon.ammo, 10);
    fo_world_free(world);
}

FO_TEST(testCombatColorfulFlamethrowerMakesTwoFlames) {
    // alj.java:59-66: one shot, one ammo, two flames.
    FOWorld *world = fo_world_new(kFlat, 3);
    FOPlayer *p = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_player_set_loadout(p, FO_CLASS_PYRO, 0, FO_ITEM_COLORFUL_FLAMETHROWER);
    fo_world_spawn(world, p, FO_CLASS_PYRO);
    fo_phys_body_set_transform(p->obj.body, 20, 3.5f, 0);
    run(world, 60);
    float ammo = p->weapon.ammo;
    FO_EXPECT(fo_player_shoot(world, p, 0));
    FO_EQUAL_INT(activeBullets(world), 2);
    FO_EQUAL_INT((int)(ammo - p->weapon.ammo), 1);
    fo_world_free(world);
}
