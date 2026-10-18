// Tests of fo_bots: the server rules of a class change, and the bots on a
// flat map. Each test builds its own map
// and its own graph.

#import "FOTest.h"
#include <math.h>
#include <string.h>
#include "fo_bots.h"
#include "fo_world_private.h"

// A map of 100 x 30 m with a floor at y 2 m. The BLU room is at the left
// and the RED room at the right. The KOTH point is x 45 to 55 m.
static NSString *mapText(NSString *extraBoxes) {
    return [NSString stringWithFormat:
        @"<map width=\"1\" height=\"1\" tilewidth=\"1000\" tileheight=\"300\">\n"
        @" <objectgroup name=\"collisions\">\n"
        @"  <object id=\"1\" x=\"0\" y=\"280\" width=\"1000\" height=\"20\"/>\n%@"
        @" </objectgroup>\n"
        @" <objectgroup name=\"areas\">\n"
        @"  <object id=\"3\" name=\"spawnarea_blu\" x=\"0\" y=\"200\" width=\"80\" height=\"80\"/>\n"
        @"  <object id=\"4\" name=\"spawnarea_red\" x=\"920\" y=\"200\" width=\"80\" height=\"80\"/>\n"
        @"  <object id=\"5\" name=\"koth_capturepoint\" x=\"450\" y=\"240\" width=\"100\" height=\"40\"/>\n"
        @"  <object id=\"6\" name=\"fridge_blu\" x=\"0\" y=\"200\" width=\"60\" height=\"80\"/>\n"
        @"  <object id=\"7\" name=\"fridge_red\" x=\"940\" y=\"200\" width=\"60\" height=\"80\"/>\n"
        @" </objectgroup>\n"
        @"</map>\n", extraBoxes ?: @""];
}

static FOWorld *newWorld(NSString *extraBoxes) {
    FOWorld *world = fo_world_new(mapText(extraBoxes).UTF8String, 11);
    fo_world_koth(world);
    return world;
}

static void run(FOWorld *world, FOBots *bots, float seconds) {
    int frames = (int)(seconds * 60 + 0.5f);
    for (int i = 0; i < frames; i++) {
        if (bots) fo_bots_update(bots, world, 1 / 60.0f);
        fo_world_update(world, 1 / 60.0f);
    }
}

// Waypoints 1 to 20 on the floor, every 5 m from x 2.5 m, with walk links
// both ways. 1 is the BLU spawn, 20 the RED spawn.
static FOGraphPoint gLinePoints[20];
static FOGraphLink gLineLinks[19];
static FOGraphCamp gLineCamps[2] = {{4, FO_TEAM_BLU, 1, 1}, {17, FO_TEAM_RED, 0, 0}};

static FOGraphData lineGraph(void) {
    for (int i = 0; i < 20; i++) {
        gLinePoints[i].id = i + 1;
        gLinePoints[i].x = 2.5f + i * 5.0f;
        gLinePoints[i].y = 3.0f;
    }
    for (int i = 0; i < 19; i++) {
        gLineLinks[i].from = i + 1;
        gLineLinks[i].to = i + 2;
        gLineLinks[i].ab = FO_LINK_WALK;
        gLineLinks[i].ba = FO_LINK_WALK;
    }
    FOGraphData d = {gLinePoints, 20, gLineLinks, 19, gLineCamps, 2};
    return d;
}

static void startBots(FOBots *bots, FOWorld *world, int maxPlayers) {
    fo_bots_init(bots, world, maxPlayers, 69);
    FOGraphData d = lineGraph();
    fo_graph_build(&bots->graph, &d, 20, 1);
}

// --- The server rules of a class change -----------------------------------

FO_TEST(testClassChangeInTheSpawnRoomRespawnsAtOnce) {
    FOWorld *world = newWorld(nil);
    FOPlayer *p = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_world_spawn(world, p, FO_CLASS_SOLDIER);
    run(world, NULL, 0.5f);
    fo_world_change_class(world, p, FO_CLASS_PYRO);
    FO_EXPECT(p->alive);
    FO_EQUAL_INT(p->cls, FO_CLASS_PYRO);
    FO_EXPECT(p->weapon.def && p->weapon.def->cls == FO_CLASS_PYRO);
    fo_world_free(world);
}

FO_TEST(testClassChangeOutOfTheSpawnRoomKills) {
    FOWorld *world = newWorld(nil);
    FOPlayer *p = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_world_spawn(world, p, FO_CLASS_SOLDIER);
    fo_phys_body_set_transform(p->obj.body, 50, 3, 0);
    run(world, NULL, 0.5f);
    fo_world_change_class(world, p, FO_CLASS_PYRO);
    FO_EXPECT(!p->alive);
    FO_EQUAL_INT(p->cls, FO_CLASS_PYRO);
    // While it waits, a new change only sets the class.
    fo_world_change_class(world, p, FO_CLASS_HEAVY);
    FO_EXPECT(!p->alive);
    FO_EQUAL_INT(p->cls, FO_CLASS_HEAVY);
    run(world, NULL, 10.1f);
    FO_EXPECT(p->alive);
    FO_EQUAL_INT((int)p->health, 300);
    fo_world_free(world);
}

// --- The bots ---------------------------------------------------------------

FO_TEST(testBotsFillTheTeamsWithDifferentClasses) {
    FOWorld *world = newWorld(nil);
    FOPlayer *you = fo_world_add_player(world, 69, FO_TEAM_BLU);
    snprintf(you->name, sizeof(you->name), "you");
    fo_world_spawn(world, you, FO_CLASS_SOLDIER);
    FOBots bots;
    startBots(&bots, world, 12);
    fo_bots_fill(&bots, world);
    int count = 0, red = 0, blu = 0;
    int classes[3][10];
    memset(classes, 0, sizeof(classes));
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *p = &world->players[i];
        if (!p->used) continue;
        count++;
        if (p->team == FO_TEAM_RED) red++;
        if (p->team == FO_TEAM_BLU) blu++;
        FO_EXPECT(p->alive);
        FO_EXPECT(p->cls >= 1 && p->cls <= 9);
        classes[p->team][p->cls]++;
        FO_EXPECT(strlen(p->name) > 0);
        for (int k = 0; k < i; k++) {
            if (world->players[k].used) FO_EXPECT(strcmp(world->players[k].name, p->name) != 0);
        }
    }
    FO_EQUAL_INT(count, 12);
    FO_EQUAL_INT(red, 6);
    FO_EQUAL_INT(blu, 6);
    // Each bot takes a class that its team has the least of.
    for (int t = 1; t <= 2; t++) {
        for (int k = 1; k <= 9; k++) FO_EXPECT(classes[t][k] <= 1);
    }
    // The first bots on the team of the human get the first names.
    int named = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used && strcmp(world->players[i].name, "gintrux69") == 0) {
            FO_EQUAL_INT(world->players[i].team, FO_TEAM_BLU);
            named = 1;
        }
    }
    FO_EXPECT(named);
    fo_world_free(world);
}

static FOBot *botWithClass(FOBots *bots, FOWorld *world, int id, int cls) {
    FOBot *b = fo_bots_add(bots, world, id, "bot");
    FOPlayer *p = fo_world_player(world, id);
    fo_world_spawn(world, p, cls);
    return b;
}

FO_TEST(testBotWalksToThePointOfTheHill) {
    FOWorld *world = newWorld(nil);
    FOBots bots;
    startBots(&bots, world, 2);
    // The first bot goes to RED, because no team has more players.
    botWithClass(&bots, world, 70, FO_CLASS_SOLDIER);
    FOPlayer *p = fo_world_player(world, 70);
    FO_EQUAL_INT(p->team, FO_TEAM_RED);
    float x, y;
    fo_player_position(p, &x, &y);
    FO_EXPECT(x > 90.0f);
    // The point is x 45 to 55 m. The bot goes there while RED does not own
    // it, and leaves when RED owns it.
    int onPoint = 0;
    for (int k = 0; k < 60 * 30 && world->mode.owner != FO_TEAM_RED; k++) {
        run(world, &bots, 1 / 60.0f);
        fo_player_position(p, &x, &y);
        if (x > 45.0f && x < 55.0f) onPoint = 1;
    }
    FO_EXPECT(onPoint);
    FO_EQUAL_INT(world->mode.owner, FO_TEAM_RED);
    fo_world_free(world);
}

FO_TEST(testBotShootsAnEnemyInView) {
    FOWorld *world = newWorld(nil);
    FOBots bots;
    startBots(&bots, world, 2);
    botWithClass(&bots, world, 70, FO_CLASS_SOLDIER);
    FOPlayer *bot = fo_world_player(world, 70);
    FOPlayer *dummy = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_world_spawn(world, dummy, FO_CLASS_HEAVY);
    fo_phys_body_set_transform(bot->obj.body, 60, 3, 0);
    fo_phys_body_set_transform(dummy->obj.body, 52, 3, 0);
    float ammo = bot->weapon.ammo;
    int frame = 0;
    while (frame < 60 * 4 && bot->weapon.ammo == ammo) {
        fo_bots_update(&bots, world, 1 / 60.0f);
        fo_world_update(world, 1 / 60.0f);
        frame++;
    }
    // The first shot of a bot takes 2 ammo, as in the original.
    FO_EXPECT(ammo - bot->weapon.ammo == 2.0f);
    run(world, &bots, 3.0f);
    FO_EXPECT(dummy->health < 300.0f || !dummy->alive);
    fo_world_free(world);
}

FO_TEST(testBotDoesNotShootThroughAWall) {
    // A wall from the floor to the top of the map at x 55 to 56 m.
    FOWorld *world = newWorld(@"  <object id=\"9\" x=\"550\" y=\"0\" width=\"10\" height=\"280\"/>\n");
    FOBots bots;
    startBots(&bots, world, 2);
    botWithClass(&bots, world, 70, FO_CLASS_SOLDIER);
    FOPlayer *bot = fo_world_player(world, 70);
    FOPlayer *dummy = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_world_spawn(world, dummy, FO_CLASS_HEAVY);
    fo_phys_body_set_transform(bot->obj.body, 60, 3, 0);
    fo_phys_body_set_transform(dummy->obj.body, 50, 3, 0);
    float ammo = bot->weapon.ammo;
    for (int i = 0; i < 60 * 3; i++) {
        fo_phys_body_set_transform(dummy->obj.body, 50, 3, 0);
        fo_bots_update(&bots, world, 1 / 60.0f);
        fo_world_update(world, 1 / 60.0f);
    }
    FO_EXPECT(bot->weapon.ammo == ammo);
    FO_EQUAL_INT((int)dummy->health, 300);
    fo_world_free(world);
}

FO_TEST(testBotDoesNotSeeACloakedAgent) {
    FOWorld *world = newWorld(nil);
    FOBots bots;
    startBots(&bots, world, 2);
    botWithClass(&bots, world, 70, FO_CLASS_SOLDIER);
    FOPlayer *bot = fo_world_player(world, 70);
    FOPlayer *spy = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_world_spawn(world, spy, FO_CLASS_SPY);
    fo_phys_body_set_transform(bot->obj.body, 60, 3, 0);
    float ammo = bot->weapon.ammo;
    for (int i = 0; i < 60 * 3; i++) {
        spy->cloaked = 1;
        spy->cloakEnergy = 100;
        fo_phys_body_set_transform(spy->obj.body, 54, 3, 0);
        fo_bots_update(&bots, world, 1 / 60.0f);
        fo_world_update(world, 1 / 60.0f);
    }
    FO_EXPECT(bot->weapon.ammo == ammo);
    fo_world_free(world);
}

FO_TEST(testMedicBotHealsAHurtTeammate) {
    FOWorld *world = newWorld(nil);
    FOBots bots;
    startBots(&bots, world, 2);
    botWithClass(&bots, world, 70, FO_CLASS_MEDIC);
    FOPlayer *medic = fo_world_player(world, 70);
    FOPlayer *mate = fo_world_add_player(world, 1, medic->team);
    fo_world_spawn(world, mate, FO_CLASS_SOLDIER);
    // Far from the point: near the objective, the medic goes to the objective
    // (zs.java:105).
    fo_phys_body_set_transform(medic->obj.body, 80, 3, 0);
    fo_phys_body_set_transform(mate->obj.body, 83, 3, 0);
    mate->health = 50;
    run(world, &bots, 3.0f);
    FO_EQUAL_INT(medic->healTarget, mate->id);
    FO_EXPECT(mate->health > 50.0f);
    fo_world_free(world);
}

FO_TEST(testAgentBotDisguisesWhenNoEnemySeesIt) {
    FOWorld *world = newWorld(nil);
    FOBots bots;
    startBots(&bots, world, 2);
    botWithClass(&bots, world, 70, FO_CLASS_SPY);
    FOPlayer *spy = fo_world_player(world, 70);
    // An enemy far out of the view of the bot, to copy.
    FOPlayer *enemy = fo_world_add_player(world, 1, spy->team == FO_TEAM_RED ? FO_TEAM_BLU : FO_TEAM_RED);
    fo_world_spawn(world, enemy, FO_CLASS_HEAVY);
    fo_phys_body_set_transform(spy->obj.body, 80, 3, 0);
    fo_phys_body_set_transform(enemy->obj.body, 10, 3, 0);
    run(world, &bots, 2.0f);
    FO_EXPECT(spy->disguise.active);
    FO_EQUAL_INT(spy->disguise.target, enemy->id);
    fo_world_free(world);
}

FO_TEST(testEngineerBotBuildsFarFromItsSpawn) {
    FOWorld *world = newWorld(nil);
    FOBots bots;
    startBots(&bots, world, 2);
    botWithClass(&bots, world, 70, FO_CLASS_ENGINEER);
    FOPlayer *eng = fo_world_player(world, 70);
    FO_EQUAL_INT(eng->team, FO_TEAM_RED);
    // 40 m from the RED spawn waypoint at x 97.5 m, toward BLU.
    fo_phys_body_set_transform(eng->obj.body, 57.5f, 3, 0);
    eng->steel = 100;
    int built = 0;
    for (int i = 0; i < 60 * 4 && !built; i++) {
        fo_bots_update(&bots, world, 1 / 60.0f);
        fo_world_update(world, 1 / 60.0f);
        built = eng->sentry.active;
    }
    FO_EXPECT(built);
    fo_world_free(world);
}

FO_TEST(testBotsStopAtTheEndOfARound) {
    FOWorld *world = newWorld(nil);
    FOBots bots;
    startBots(&bots, world, 2);
    botWithClass(&bots, world, 70, FO_CLASS_SCOUT);
    FOPlayer *p = fo_world_player(world, 70);
    run(world, &bots, 3.0f);
    world->mode.redTimer = 0;
    // The friction of the ground stops the bot in less than a second.
    run(world, &bots, 1.5f);
    float x0, y0, x1, y1;
    fo_player_position(p, &x0, &y0);
    run(world, &bots, 2.0f);
    fo_player_position(p, &x1, &y1);
    FO_EXPECT(fabsf(x1 - x0) < 0.05f);
    fo_world_free(world);
}
