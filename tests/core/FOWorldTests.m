// Tests of fo_world: the bodies of the map, and the walk and the jump of a
// player. Each test builds its own map text.

#import "FOTest.h"
#include <math.h>
#include <stdio.h>
#include "fo_world.h"

// A map of 100 x 30 m with a floor at y 2 m. The BLU room is at the left
// and the RED room at the right. A wall stands at x 50 to 51 m, from the
// floor up to y 6 m.
static NSString *mapText(NSString *extraBoxes) {
    return [NSString stringWithFormat:
        @"<map width=\"1\" height=\"1\" tilewidth=\"1000\" tileheight=\"300\">\n"
        @" <objectgroup name=\"collisions\">\n"
        @"  <object id=\"1\" x=\"0\" y=\"280\" width=\"1000\" height=\"20\"/>\n"
        @"  <object id=\"2\" x=\"500\" y=\"240\" width=\"10\" height=\"40\"/>\n%@"
        @" </objectgroup>\n"
        @" <objectgroup name=\"areas\">\n"
        @"  <object id=\"3\" name=\"spawnarea_blu\" x=\"0\" y=\"200\" width=\"80\" height=\"80\"/>\n"
        @"  <object id=\"4\" name=\"spawnarea_red\" x=\"920\" y=\"200\" width=\"80\" height=\"80\"/>\n"
        @" </objectgroup>\n"
        @"</map>\n", extraBoxes ?: @""];
}

static FOWorld *newWorld(NSString *extraBoxes) {
    return fo_world_new(mapText(extraBoxes).UTF8String, 7);
}

static void run(FOWorld *world, int frames) {
    for (int i = 0; i < frames; i++) fo_world_update(world, 1 / 60.0f);
}

static FOPlayer *standingPlayer(FOWorld *world, short team, int cls, float x) {
    FOPlayer *p = fo_world_add_player(world, team * 10 + cls, team);
    fo_world_spawn(world, p, cls);
    fo_phys_body_set_transform(p->obj.body, x, 4, 0);
    run(world, 60);
    return p;
}

FO_TEST(testWorldSpawnsInTheRoomAndLands) {
    FOWorld *world = newWorld(nil);
    FO_EXPECT(world != NULL);
    FOPlayer *p = fo_world_add_player(world, 1, FO_TEAM_BLU);
    fo_world_spawn(world, p, FO_CLASS_SOLDIER);
    float x, y;
    fo_player_position(p, &x, &y);
    // The room is x 0 to 8 m. The point keeps 2.25 m from each side.
    FO_EXPECT(x >= 2.25f && x <= 5.75f);
    FO_EQUAL_INT((int)p->health, 200);
    run(world, 120);
    fo_player_position(p, &x, &y);
    FO_EXPECT(fo_player_grounded(p));
    // The floor is at y 2 m. The bottom circle reaches 0.95 + 0.4 m down.
    FO_EXPECT(y > 2.8f && y < 3.2f);
    fo_world_free(world);
}

FO_TEST(testWorldWalkSpeedOfEachClass) {
    for (int cls = 1; cls <= 9; cls++) {
        FOWorld *world = newWorld(nil);
        FOPlayer *p = standingPlayer(world, FO_TEAM_BLU, cls, 20);
        p->move = FO_MOVE_RIGHT;
        run(world, 10);
        float vx, vy;
        fo_player_velocity(p, &vx, &vy);
        FO_EXPECT(fabsf(vx - fo_class_speed(cls)) < 0.01f);
        FO_EXPECT(p->facingRight);
        p->move = FO_MOVE_STOP;
        run(world, 2);
        fo_player_velocity(p, &vx, &vy);
        FO_EXPECT(fabsf(vx) < 0.01f);
        fo_world_free(world);
    }
}

FO_TEST(testWorldJumpHeight) {
    FOWorld *world = newWorld(nil);
    FOPlayer *p = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    float x, y0, y, top = 0;
    fo_player_position(p, &x, &y0);
    FO_EXPECT(fo_player_jump(world, p));
    // A second jump in the air does nothing for a class that is not the rusher.
    run(world, 12);
    FO_EXPECT(!fo_player_jump(world, p));
    for (int i = 0; i < 60; i++) {
        run(world, 1);
        fo_player_position(p, &x, &y);
        if (y > top) top = y;
    }
    // v^2 / 2g = 8.5^2 / 30 = 2.41 m.
    FO_EXPECT(fabsf(top - y0 - 2.41f) < 0.15f);
    FO_EXPECT(world->jumps == 0);
    fo_world_free(world);
}

FO_TEST(testWorldRusherJumpsTwice) {
    FOWorld *world = newWorld(nil);
    FOPlayer *p = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_SCOUT, 20);
    FO_EXPECT(fo_player_jump(world, p));
    run(world, 12);
    FO_EXPECT(fo_player_jump(world, p));
    float vx, vy;
    fo_player_velocity(p, &vx, &vy);
    FO_EXPECT(fabsf(vy - 6.8f) < 0.01f);
    run(world, 12);
    FO_EXPECT(!fo_player_jump(world, p));
    run(world, 120);
    FO_EXPECT(fo_player_grounded(p));
    FO_EXPECT(p->doubleJumpReady);
    fo_world_free(world);
}

FO_TEST(testWorldWallStopsTheWalk) {
    FOWorld *world = newWorld(nil);
    FOPlayer *p = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_SCOUT, 47);
    p->move = FO_MOVE_RIGHT;
    run(world, 120);
    float x, y;
    fo_player_position(p, &x, &y);
    FO_EXPECT(x < 50 && x > 49);
    fo_world_free(world);
}

FO_TEST(testWorldEnemyRoomIsClosed) {
    FOWorld *world = newWorld(nil);
    // A RED player cannot walk into the BLU room at x 0 to 8 m.
    FOPlayer *red = standingPlayer(world, FO_TEAM_RED, FO_CLASS_SCOUT, 12);
    red->move = FO_MOVE_LEFT;
    run(world, 120);
    float x, y;
    fo_player_position(red, &x, &y);
    FO_EXPECT(x > 8);
    fo_world_free(world);
    // A BLU player can.
    world = newWorld(nil);
    FOPlayer *blu = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_SCOUT, 12);
    blu->move = FO_MOVE_LEFT;
    run(world, 120);
    fo_player_position(blu, &x, &y);
    FO_EXPECT(x < 8);
    fo_world_free(world);
}

FO_TEST(testWorldTeammatesPassEnemiesBlock) {
    FOWorld *world = newWorld(nil);
    FOPlayer *a = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_HEAVY, 30);
    FOPlayer *b = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_SCOUT, 27);
    b->move = FO_MOVE_RIGHT;
    run(world, 60);
    float x, y;
    fo_player_position(b, &x, &y);
    FO_EXPECT(x > 33);
    FOPlayer *enemy = standingPlayer(world, FO_TEAM_RED, FO_CLASS_SCOUT, 40);
    enemy->move = FO_MOVE_LEFT;
    run(world, 120);
    float ax, ex;
    fo_player_position(a, &ax, &y);
    fo_player_position(enemy, &ex, &y);
    FO_EXPECT(ex > ax);
    fo_world_free(world);
}

FO_TEST(testWorldDoorOpensForTheTeam) {
    FOWorld *world = newWorld(nil);
    fo_world_add_door(world, FO_TEAM_BLU, 20, 3.5f, 1.3f, 3.0f);
    FOPlayer *red = standingPlayer(world, FO_TEAM_RED, FO_CLASS_SCOUT, 25);
    red->move = FO_MOVE_LEFT;
    run(world, 120);
    float x, y;
    fo_player_position(red, &x, &y);
    FO_EXPECT(x > 20.5f);
    FO_EXPECT(!world->doors[0].open);
    fo_world_free(world);
    // In a world with no enemy at the door, a BLU player goes through.
    world = newWorld(nil);
    fo_world_add_door(world, FO_TEAM_BLU, 20, 3.5f, 1.3f, 3.0f);
    FOPlayer *blu = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_SCOUT, 15);
    blu->move = FO_MOVE_RIGHT;
    run(world, 120);
    fo_player_position(blu, &x, &y);
    FO_EXPECT(x > 21);
    fo_world_free(world);
}

FO_TEST(testWorldPitKillsAndPlayerComesBack) {
    // A pit at x 30 to 35 m: a death area over a hole in the floor.
    NSString *text = [mapText(nil) stringByReplacingOccurrencesOfString:@"</objectgroup>\n</map>"
        withString:@"  <object id=\"9\" name=\"playerdeatharea\" x=\"300\" y=\"250\" width=\"50\" height=\"40\"/>\n"
                   @" </objectgroup>\n</map>"];
    FOWorld *world = fo_world_new(text.UTF8String, 7);
    FOPlayer *p = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_SCOUT, 25);
    FO_EXPECT(p->alive);
    p->move = FO_MOVE_RIGHT;
    run(world, 120);
    FO_EXPECT(!p->alive);
    FO_EXPECT(fo_player_waiting(p));
    FO_EQUAL_INT((int)p->health, 0);
    long long death = p->deathTime;
    // The respawn comes 9999 ms after the death, in the spawn room.
    while (world->now - death < 9990) run(world, 1);
    FO_EXPECT(!p->alive);
    run(world, 2);
    FO_EXPECT(p->alive);
    FO_EQUAL_INT((int)p->health, 125);
    float x, y;
    fo_player_position(p, &x, &y);
    FO_EXPECT(x < 8);
    fo_world_free(world);
}

FO_TEST(testWorldHealthSetter) {
    FOWorld *world = newWorld(nil);
    FOPlayer *p = standingPlayer(world, FO_TEAM_BLU, FO_CLASS_HEAVY, 25);
    fo_player_set_health(world, p, 120);
    FO_EXPECT(p->hurtTime == world->now);
    FO_EXPECT(p->alive);
    fo_player_set_health(world, p, -10);
    FO_EXPECT(!p->alive);
    FO_EQUAL_INT((int)p->health, 0);
    fo_world_free(world);
}

FO_TEST(testWorldRefusesBadMap) {
    FO_EXPECT(fo_world_new("not a map", 1) == NULL);
}
