// Tests of the class abilities: the burn, the grenade splash, the sniper
// rifle, the sentry, the heal and the uber, and the cloak, the disguise,
// and the backstab of the spy. Each test builds its own map.

#import "FOTest.h"
#include <math.h>
#include "fo_world.h"

static const char *kFlat =
    "<map width=\"1\" height=\"1\" tilewidth=\"1000\" tileheight=\"300\">\n"
    " <objectgroup name=\"collisions\">\n"
    "  <object id=\"1\" x=\"0\" y=\"280\" width=\"1000\" height=\"20\"/>\n"
    " </objectgroup>\n"
    " <objectgroup name=\"areas\">\n"
    "  <object id=\"3\" name=\"spawnarea_blu\" x=\"0\" y=\"200\" width=\"80\" height=\"80\"/>\n"
    "  <object id=\"4\" name=\"spawnarea_red\" x=\"920\" y=\"200\" width=\"80\" height=\"80\"/>\n"
    "  <object id=\"5\" name=\"nosentryarea\" x=\"700\" y=\"200\" width=\"50\" height=\"80\"/>\n"
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

FO_TEST(testClassFlameSetsFire) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *pyro = standing(world, 1, FO_TEAM_BLU, FO_CLASS_PYRO, 20);
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_HEAVY, 21.5f);
    // Shoot until one flame hits.
    for (int i = 0; i < 30 && !e->burning; i++) {
        fo_player_shoot(world, pyro, 0);
        run(world, 1);
    }
    FO_EXPECT(e->burning);
    float before = e->health;
    // The pyro stops. The fire hurts 5 each second for 5 s.
    run(world, 120);
    FO_EXPECT(fabsf((before - e->health) - 10) < 0.5f);
    run(world, 60 * 4);
    FO_EXPECT(!e->burning);
    fo_world_free(world);
}

FO_TEST(testClassGrenadeSplash) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *demo = standing(world, 1, FO_TEAM_BLU, FO_CLASS_DEMOMAN, 20);
    FOPlayer *near = standing(world, 2, FO_TEAM_RED, FO_CLASS_HEAVY, 26);
    FOPlayer *mate = standing(world, 3, FO_TEAM_BLU, FO_CLASS_HEAVY, 24);
    fo_player_set_aim(demo, 5);
    FO_EXPECT(fo_player_shoot(world, demo, 5 * 0.017453292f));
    run(world, 200);
    // The grenade hit the enemy or went off near it: 50 at most. A
    // teammate takes no damage.
    FO_EXPECT(near->health < 300 && near->health >= 250);
    FO_EQUAL_INT((int)mate->health, 300);
    fo_world_free(world);
}

FO_TEST(testClassGrenadeBouncesThenGoesOff) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *demo = standing(world, 1, FO_TEAM_BLU, FO_CLASS_DEMOMAN, 20);
    fo_player_set_aim(demo, -20);
    FO_EXPECT(fo_player_shoot(world, demo, -20 * 0.017453292f));
    run(world, 30);
    int active = 0;
    for (int i = 0; i < FO_MAX_BULLETS; i++) active += world->bullets[i].active && world->bullets[i].grounded;
    FO_EQUAL_INT(active, 1);
    // It goes off at 2.7 s.
    run(world, 150);
    active = 0;
    for (int i = 0; i < FO_MAX_BULLETS; i++) active += world->bullets[i].active;
    FO_EQUAL_INT(active, 0);
    fo_world_free(world);
}

FO_TEST(testClassSniperCharge) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *sniper = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SNIPER, 20);
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_HEAVY, 40);
    // Aim for 2.5 s: the charge is 50, and the damage is 40 + 55.
    for (int i = 0; i < 150; i++) {
        fo_player_set_aim(sniper, 0.001f);
        run(world, 1);
    }
    FO_EXPECT(fabsf(sniper->sniperCharge - 50) < 1);
    FO_EXPECT(fo_player_shoot(world, sniper, 0.001f * 0.017453292f));
    FO_EXPECT(e->health <= 300 - 94 && e->health >= 300 - 96);
    // With no aim the charge goes to 0.
    fo_player_set_aim(sniper, 0);
    run(world, 1);
    FO_EQUAL_INT((int)sniper->sniperCharge, 0);
    fo_world_free(world);
}

FO_TEST(testClassSentryBuildsAndShoots) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *eng = standing(world, 1, FO_TEAM_BLU, FO_CLASS_ENGINEER, 20);
    FO_EQUAL_INT((int)eng->steel, 100);
    fo_player_ability(world, eng);
    FO_EXPECT(eng->sentry.active);
    FO_EQUAL_INT((int)eng->steel, 0);
    run(world, 30);
    FO_EXPECT(eng->sentry.built != 0);
    run(world, 60);
    // An enemy walks in range: the sentry turns and shoots.
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_HEAVY, 27);
    run(world, 120);
    FO_EXPECT(e->health < 300);
    // The charge comes back at 2.5 each second.
    FO_EXPECT(eng->steel > 9 && eng->steel < 12);
    // The same action destroys the sentry.
    fo_player_ability(world, eng);
    FO_EXPECT(!eng->sentry.active);
    fo_world_free(world);
}

FO_TEST(testClassSentryNotInNoSentryArea) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *eng = standing(world, 1, FO_TEAM_BLU, FO_CLASS_ENGINEER, 72);
    fo_player_ability(world, eng);
    FO_EXPECT(!eng->sentry.active);
    fo_world_free(world);
}

FO_TEST(testClassSentryDiesAndGivesPoints) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *eng = standing(world, 1, FO_TEAM_BLU, FO_CLASS_ENGINEER, 20);
    fo_player_ability(world, eng);
    run(world, 90);
    // The builder goes away, out of the line of the rockets.
    fo_phys_body_set_transform(eng->obj.body, 10, 3.5f, 0);
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_SOLDIER, 50);
    for (int i = 0; i < 4; i++) {
        fo_player_set_aim(e, 180);
        e->facingRight = 0;
        FO_EXPECT(fo_player_shoot(world, e, 3.1415927f));
        run(world, 60);
    }
    // The last rocket needs 2.8 s to fly 30 m.
    run(world, 180);
    FO_EXPECT(!eng->sentry.active);
    FO_EQUAL_INT(e->score, 2);
    fo_world_free(world);
}

FO_TEST(testClassMedicHealsAndUbers) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *medic = standing(world, 1, FO_TEAM_BLU, FO_CLASS_MEDIC, 20);
    FOPlayer *mate = standing(world, 2, FO_TEAM_BLU, FO_CLASS_HEAVY, 23);
    fo_player_set_health(world, mate, 100);
    fo_player_heal(world, medic, mate->id);
    run(world, 60);
    // 34 health each second.
    FO_EXPECT(fabsf(mate->health - 134) < 1);
    FO_EXPECT(fabsf(medic->uberCharge - 1) < 0.05f);
    // The uber needs a charge of 20.
    fo_player_ability(world, medic);
    FO_EXPECT(!medic->uber);
    medic->uberCharge = 20;
    fo_player_ability(world, medic);
    FO_EXPECT(medic->uber);
    FO_EQUAL_INT(medic->score, 2);
    // In uber the target takes no damage.
    FOPlayer *e = standing(world, 3, FO_TEAM_RED, FO_CLASS_SOLDIER, 40);
    fo_player_damage(world, mate, e->id, FO_ITEM_ROCKET_LAUNCHER, 60);
    FO_EQUAL_INT((int)mate->health, 300);
    // The uber takes 3 each second and the heal gives 1 back, so the uber
    // lasts 20 / 2 s while the medic heals.
    run(world, 60 * 8);
    FO_EXPECT(medic->uber);
    run(world, 60 * 2);
    FO_EXPECT(!medic->uber);
    // Out of range, the heal stops.
    fo_phys_body_set_transform(mate->obj.body, 35, 3.5f, 0);
    run(world, 2);
    FO_EQUAL_INT(medic->healTarget, 0);
    fo_world_free(world);
}

FO_TEST(testClassSpyCloakDrains) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *spy = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SPY, 20);
    fo_player_ability(world, spy);
    FO_EXPECT(spy->cloaked);
    run(world, 60);
    FO_EXPECT(fabsf(spy->cloakEnergy - 90) < 0.5f);
    // A shot ends the cloak.
    fo_player_shoot(world, spy, 0);
    FO_EXPECT(!spy->cloaked);
    run(world, 60);
    FO_EXPECT(fabsf(spy->cloakEnergy - 100) < 0.5f);
    fo_world_free(world);
}

FO_TEST(testClassSpyBackstabKills) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *spy = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SPY, 20);
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_HEAVY, 21.2f);
    spy->facingRight = 1;
    fo_player_backstab(world, spy);
    FO_EXPECT(spy->backstab);
    run(world, 30);
    FO_EXPECT(e->alive);
    // The hit comes 0.628 s after the start.
    run(world, 10);
    FO_EXPECT(!e->alive);
    FO_EQUAL_INT(spy->score, 2);
    run(world, 30);
    FO_EXPECT(!spy->backstab);
    fo_world_free(world);
}

FO_TEST(testClassSpyBackstabMissesBehind) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *spy = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SPY, 20);
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_HEAVY, 18.8f);
    spy->facingRight = 1;
    fo_player_backstab(world, spy);
    run(world, 60);
    FO_EXPECT(e->alive);
    fo_world_free(world);
}

FO_TEST(testClassSpyDisguise) {
    FOWorld *world = fo_world_new(kFlat, 5);
    FOPlayer *spy = standing(world, 1, FO_TEAM_BLU, FO_CLASS_SPY, 20);
    FOPlayer *e = standing(world, 2, FO_TEAM_RED, FO_CLASS_MEDIC, 30);
    fo_player_disguise(world, spy, 0);
    FO_EXPECT(spy->disguise.active);
    FO_EQUAL_INT(spy->disguise.cls, FO_CLASS_MEDIC);
    FO_EQUAL_INT(spy->disguise.team, FO_TEAM_RED);
    FO_EQUAL_INT(spy->disguise.target, e->id);
    FO_EXPECT(!fo_player_disguised(world, spy));
    run(world, 90);
    FO_EXPECT(fo_player_disguised(world, spy));
    // A shot ends a complete disguise.
    fo_player_shoot(world, spy, 0);
    FO_EXPECT(!spy->disguise.active);
    fo_world_free(world);
}
