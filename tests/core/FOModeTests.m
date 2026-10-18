// Tests of fo_modes: king of the hill, the bomb, capture the flag, and the
// round flow. Each test builds its own map text.

#import "FOTest.h"
#include <math.h>
#include <stdio.h>
#include "fo_world_private.h"

// A map of 100 x 30 m with a floor at y 2 m. The BLU room is at the left
// and the RED room at the right. extraBoxes and extraAreas go into the
// object groups.
static NSString *mapText(NSString *extraBoxes, NSString *extraAreas) {
    return [NSString stringWithFormat:
        @"<map width=\"1\" height=\"1\" tilewidth=\"1000\" tileheight=\"300\">\n"
        @" <objectgroup name=\"collisions\">\n"
        @"  <object id=\"1\" x=\"0\" y=\"280\" width=\"1000\" height=\"20\"/>\n%@"
        @" </objectgroup>\n"
        @" <objectgroup name=\"areas\">\n"
        @"  <object id=\"3\" name=\"spawnarea_blu\" x=\"0\" y=\"200\" width=\"80\" height=\"80\"/>\n"
        @"  <object id=\"4\" name=\"spawnarea_red\" x=\"920\" y=\"200\" width=\"80\" height=\"80\"/>\n%@"
        @" </objectgroup>\n"
        @"</map>\n", extraBoxes ?: @"", extraAreas ?: @""];
}

static FOWorld *newWorld(NSString *extraBoxes, NSString *extraAreas) {
    return fo_world_new(mapText(extraBoxes, extraAreas).UTF8String, 7);
}

static void run(FOWorld *world, float seconds) {
    int frames = (int)(seconds * 60 + 0.5f);
    for (int i = 0; i < frames; i++) fo_world_update(world, 1 / 60.0f);
}

static FOPlayer *playerAt(FOWorld *world, int id, short team, int cls, float x) {
    FOPlayer *p = fo_world_add_player(world, id, team);
    fo_world_spawn(world, p, cls);
    fo_phys_body_set_transform(p->obj.body, x, 3.5f, 0);
    return p;
}

static void moveTo(FOPlayer *p, float x) {
    fo_phys_body_set_transform(p->obj.body, x, 3.5f, 0);
}

static int hasSound(const FOWorld *world, const char *name) {
    for (int i = 0; i < world->event_count; i++) {
        if (world->events[i].kind == FO_EVENT_SOUND && strcmp(world->events[i].name, name) == 0) return 1;
    }
    return 0;
}

// --- King of the hill -----------------------------------------------------

// The point is x 40 to 46 m, y 2 to 6 m.
static NSString *const kothArea = @"  <object id=\"5\" name=\"koth_capturepoint\" x=\"400\" y=\"240\" width=\"60\" height=\"40\"/>\n";

FO_TEST(testKothOnePlayerTakesTheNeutralPointIn14Seconds) {
    FOWorld *world = newWorld(nil, kothArea);
    fo_world_koth(world);
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 43);
    run(world, 13.5f);
    FO_EQUAL_INT(world->mode.owner, 0);
    FO_EXPECT(fabsf(world->mode.progress - 13.5f * 7.142857f) < 1.0f);
    run(world, 1.0f);
    FO_EQUAL_INT(world->mode.owner, FO_TEAM_RED);
    FO_EQUAL_INT(red->score, 5);
    // Only the timer of the owner runs.
    run(world, 2.0f);
    FO_EXPECT(world->mode.redTimer < 179.0f && world->mode.redTimer > 177.0f);
    FO_EXPECT(world->mode.bluTimer == 180.0f);
    fo_world_free(world);
}

FO_TEST(testKothAdvantageTakesTheOwnedPoint) {
    FOWorld *world = newWorld(nil, kothArea);
    fo_world_koth(world);
    world->mode.owner = FO_TEAM_RED;
    world->mode.lastOwner = FO_TEAM_RED;
    world->mode.progress = 100;
    playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 41);
    FOPlayer *b1 = playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 43);
    FOPlayer *b2 = playerAt(world, 3, FO_TEAM_BLU, FO_CLASS_SOLDIER, 45);
    // The advantage is one player: 100 / 7.142857 is 14 s.
    run(world, 13.5f);
    FO_EQUAL_INT(world->mode.owner, FO_TEAM_RED);
    run(world, 1.0f);
    FO_EQUAL_INT(world->mode.owner, FO_TEAM_BLU);
    FO_EXPECT(world->mode.progress == -100.0f);
    FO_EQUAL_INT(b1->score, 5);
    FO_EQUAL_INT(b2->score, 5);
    fo_world_free(world);
}

FO_TEST(testKothContestedPointFreezes) {
    FOWorld *world = newWorld(nil, kothArea);
    fo_world_koth(world);
    world->mode.progress = 30;
    playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 42);
    playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 44);
    run(world, 3.0f);
    FO_EXPECT(world->mode.progress == 30.0f);
    fo_world_free(world);
}

FO_TEST(testKothOwnerDecaysBackWithNoAttacker) {
    FOWorld *world = newWorld(nil, kothArea);
    fo_world_koth(world);
    world->mode.owner = FO_TEAM_BLU;
    world->mode.lastOwner = FO_TEAM_BLU;
    world->mode.progress = -50;
    run(world, 2.0f);
    FO_EXPECT(fabsf(world->mode.progress - (-56.0f)) < 0.2f);
    fo_world_free(world);
}

FO_TEST(testKothHiddenSpyDoesNotCount) {
    FOWorld *world = newWorld(nil, kothArea);
    fo_world_koth(world);
    FOPlayer *spy = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SPY, 43);
    spy->cloaked = 1;
    run(world, 2.0f);
    FO_EXPECT(world->mode.progress == 0.0f);
    spy->cloaked = 0;
    run(world, 1.0f);
    FO_EXPECT(world->mode.progress > 6.0f);
    fo_world_free(world);
}

FO_TEST(testKothAttackerOnTheOwnedPointHoldsTheObjective) {
    FOWorld *world = newWorld(nil, kothArea);
    fo_world_koth(world);
    world->mode.owner = FO_TEAM_BLU;
    world->mode.lastOwner = FO_TEAM_BLU;
    world->mode.progress = -100;
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 43);
    FOPlayer *blu = playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    run(world, 0.1f);
    FO_EXPECT(fo_world_holds_objective(world, red));
    FO_EXPECT(!fo_world_holds_objective(world, blu));
    fo_world_free(world);
}

FO_TEST(testPlayersStopAtTheEndOfARound) {
    FOWorld *world = newWorld(nil, kothArea);
    fo_world_koth(world);
    FOPlayer *p = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SCOUT, 20);
    run(world, 0.5f);
    p->move = FO_MOVE_RIGHT;
    run(world, 0.5f);
    float vx, vy;
    fo_player_velocity(p, &vx, &vy);
    FO_EXPECT(vx > 5.0f);
    world->mode.redTimer = 0;
    run(world, 0.1f);
    fo_player_velocity(p, &vx, &vy);
    FO_EXPECT(vx == 0.0f);
    FO_EQUAL_INT(p->move, FO_MOVE_STOP);
    fo_world_free(world);
}

FO_TEST(testKothRoundEndsAndANewRoundStartsAfter15Seconds) {
    FOWorld *world = newWorld(nil, kothArea);
    fo_world_koth(world);
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 43);
    world->mode.owner = FO_TEAM_RED;
    world->mode.lastOwner = FO_TEAM_RED;
    world->mode.progress = 100;
    world->mode.redTimer = 0.5f;
    red->score = 9;
    run(world, 1.0f);
    FO_EXPECT(fo_mode_over(world));
    FO_EQUAL_INT(fo_mode_winner(world), FO_TEAM_RED);
    run(world, 14.0f);
    FO_EXPECT(fo_mode_over(world));
    FO_EQUAL_INT(red->score, 9);
    run(world, 1.0f);
    FO_EXPECT(!fo_mode_over(world));
    FO_EQUAL_INT(red->score, 0);
    FO_EXPECT(world->mode.redTimer > 179.0f);
    FO_EQUAL_INT(world->mode.owner, 0);
    // KOTH keeps the teams. The player spawns in the own room.
    FO_EQUAL_INT(red->team, FO_TEAM_RED);
    float x, y;
    fo_player_position(red, &x, &y);
    FO_EXPECT(x > 92.0f);
    fo_world_free(world);
}

// --- The bomb -------------------------------------------------------------

// The bomb starts at x 20 m and rolls to +x for RED.
static FOWorld *bombWorld(NSString *extraBoxes, NSString *extraAreas) {
    FOWorld *world = newWorld(extraBoxes, extraAreas);
    fo_world_bomb(world, 1, 20, 3.5f, 90, 5);
    return world;
}

static float bombX(const FOWorld *world) {
    float x, y;
    fo_phys_body_position(world->mode.bomb.obj.body, &x, &y);
    return x;
}

static float bombVX(const FOWorld *world) {
    float vx, vy;
    fo_phys_body_velocity(world->mode.bomb.obj.body, &vx, &vy);
    return vx;
}

FO_TEST(testBombRedPusherRollsTheBombForward) {
    FOWorld *world = bombWorld(nil, nil);
    run(world, 1.0f);
    float x0 = bombX(world);
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 20);
    for (int i = 0; i < 180; i++) {
        moveTo(red, bombX(world));
        run(world, 1 / 60.0f);
    }
    FO_EXPECT(world->mode.bomb.target == 1.0f);
    // vx lerps to 1 at the rate 1 * dt: about 0.95 after 3 s.
    FO_EXPECT(bombVX(world) > 0.9f && bombVX(world) <= 1.0f);
    FO_EXPECT(bombX(world) > x0 + 1.5f);
    FO_EXPECT(fo_world_holds_objective(world, red));
    fo_world_free(world);
}

FO_TEST(testBombSpeedGrowsWithThePushers) {
    float expect[5] = {0, 1.0f, 1.3f, 1.4f, 1.5f};
    for (int n = 1; n <= 4; n++) {
        FOWorld *world = bombWorld(nil, nil);
        for (int i = 0; i < n; i++) playerAt(world, i + 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 19.5f + i * 0.3f);
        run(world, 1 / 60.0f);
        FO_EXPECT(fabsf(world->mode.bomb.target - expect[n]) < 0.0001f);
        fo_world_free(world);
    }
}

FO_TEST(testBombBluPushesBackOnly7SecondsAfterRed) {
    FOWorld *world = bombWorld(nil, nil);
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 20);
    run(world, 0.5f);
    moveTo(red, 60);
    FOPlayer *blu = playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20);
    float t = 0;
    while (t < 6.5f) {
        moveTo(blu, bombX(world));
        run(world, 1 / 60.0f);
        t += 1 / 60.0f;
    }
    FO_EXPECT(world->mode.bomb.target == 0.0f);
    while (t < 7.5f) {
        moveTo(blu, bombX(world));
        run(world, 1 / 60.0f);
        t += 1 / 60.0f;
    }
    FO_EXPECT(world->mode.bomb.target == -1.0f);
    fo_world_free(world);
}

FO_TEST(testBombEqualPushersStopTheBomb) {
    FOWorld *world = bombWorld(nil, nil);
    playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 19.5f);
    playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 20.5f);
    run(world, 0.5f);
    FO_EXPECT(world->mode.bomb.target == 0.0f);
    fo_world_free(world);
}

FO_TEST(testBombHiddenSpyDoesNotPush) {
    FOWorld *world = bombWorld(nil, nil);
    FOPlayer *spy = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SPY, 20);
    spy->disguise.active = 1;
    run(world, 1 / 60.0f);
    FO_EQUAL_INT(world->mode.bomb.redCount, 0);
    FO_EXPECT(world->mode.bomb.target == 0.0f);
    fo_world_free(world);
}

FO_TEST(testBombCheckpointStopsTheRollBack) {
    FOWorld *world = bombWorld(nil, nil);
    fo_world_bomb_checkpoint(world, 25, 3.1f);
    FO_EXPECT(!world->mode.checkpoints[0].active);
    // The bomb is 0.55 + 1.5 m past the gate: the gate closes.
    fo_phys_body_set_transform(world->mode.bomb.obj.body, 27.2f, 3.5f, 0);
    run(world, 0.1f);
    FO_EXPECT(world->mode.checkpoints[0].active);
    FOPlayer *blu = playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 27);
    for (int i = 0; i < 60 * 12; i++) {
        moveTo(blu, bombX(world));
        run(world, 1 / 60.0f);
    }
    FO_EXPECT(world->mode.bomb.target < 0);
    // The gate fixture ends at x 25.35 m, and the bomb has a radius of 1.5 m.
    FO_EXPECT(bombX(world) > 26.8f);
    FO_EXPECT(world->mode.checkpoints[0].active);
    fo_world_free(world);
}

// The ground under the bomb tilts by 10 degrees one way or the other way.
static float slopeTarget(NSString *rotation, int pushers, int *hint) {
    NSString *ramp = [NSString stringWithFormat:
        @"  <object id=\"9\" x=\"100\" y=\"250\" width=\"300\" height=\"10\" rotation=\"%@\"/>\n", rotation];
    FOWorld *world = newWorld(ramp, nil);
    fo_world_bomb(world, 1, 25, 8, 90, 5);
    world->localId = 1;
    run(world, 2.0f);
    float x, y;
    fo_phys_body_position(world->mode.bomb.obj.body, &x, &y);
    for (int i = 0; i < pushers; i++) {
        FOPlayer *p = fo_world_add_player(world, i + 1, FO_TEAM_RED);
        fo_world_spawn(world, p, FO_CLASS_SOLDIER);
        fo_phys_body_set_transform(p->obj.body, x - 0.5f + i * 0.4f, y, 0);
        fo_phys_body_set_gravity_scale(p->obj.body, 0);
        fo_phys_body_set_velocity(p->obj.body, 0, 0);
    }
    fo_phys_body_set_gravity_scale(world->mode.bomb.obj.body, 0);
    fo_phys_body_set_velocity(world->mode.bomb.obj.body, 0, 0);
    run(world, 1 / 60.0f);
    float target = world->mode.bomb.target;
    *hint = world->mode.bomb.hint;
    FO_EXPECT(world->mode.bomb.slopeCount > 0);
    fo_world_free(world);
    return target;
}

FO_TEST(testBombSlopeNeedsTwoPushersOneWay) {
    int hintA, hintB;
    float a = slopeTarget(@"10", 1, &hintA);
    float b = slopeTarget(@"-10", 1, &hintB);
    // One slope gives 1.2. On the other slope one pusher cannot roll the
    // bomb, and the local pusher sees the hint.
    FO_EXPECT((fabsf(a - 1.2f) < 0.0001f && b == 0 && hintB) || (fabsf(b - 1.2f) < 0.0001f && a == 0 && hintA));
    NSString *hard = a == 0 ? @"10" : @"-10";
    int hint;
    float two = slopeTarget(hard, 2, &hint);
    FO_EXPECT(fabsf(two - 1.3f * 8.5f) < 0.001f);
    FO_EXPECT(!hint);
}

// The finish area and the fall area are both x 60 to 70 m.
static NSString *const bombAreas =
    @"  <object id=\"6\" name=\"bombfinisharea\" x=\"600\" y=\"200\" width=\"100\" height=\"80\"/>\n"
    @"  <object id=\"7\" name=\"bombfallarea\" x=\"600\" y=\"150\" width=\"100\" height=\"130\"/>\n";

FO_TEST(testBombFinishEndsTheRoundAfter4500Ms) {
    FOWorld *world = bombWorld(nil, bombAreas);
    fo_phys_body_set_transform(world->mode.bomb.obj.body, 65, 3.5f, 0);
    fo_world_update(world, 1 / 60.0f);
    FO_EXPECT(world->mode.bomb.finished != 0);
    int shake = 0;
    for (int i = 0; i < world->event_count; i++) {
        if (world->events[i].kind == FO_EVENT_SHAKE && world->events[i].item == 3000) shake = 1;
    }
    FO_EXPECT(shake);
    FO_EXPECT(hasSound(world, "cartpush_explosion"));
    FO_EXPECT(fo_mode_bomb_down(world));
    float timer = world->mode.bombTimer;
    run(world, 4.3f);
    FO_EXPECT(!fo_mode_over(world));
    // The timer stops while the bomb is down.
    FO_EXPECT(world->mode.bombTimer == timer);
    run(world, 0.3f);
    FO_EXPECT(fo_mode_over(world));
    FO_EQUAL_INT(fo_mode_winner(world), FO_TEAM_RED);
    fo_world_free(world);
}

FO_TEST(testBombTimerEndGivesBluTheRoundAndSwapsTheTeams) {
    FOWorld *world = bombWorld(nil, nil);
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 40);
    FOPlayer *blu = playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 45);
    fo_phys_body_set_transform(world->mode.bomb.obj.body, 30, 3.5f, 0);
    world->mode.bombTimer = 0.5f;
    run(world, 1.0f);
    FO_EXPECT(fo_mode_over(world));
    FO_EQUAL_INT(fo_mode_winner(world), FO_TEAM_BLU);
    run(world, 15.0f);
    FO_EXPECT(!fo_mode_over(world));
    FO_EQUAL_INT(red->team, FO_TEAM_BLU);
    FO_EQUAL_INT(blu->team, FO_TEAM_RED);
    FO_EXPECT(world->mode.bombTimer > 419.0f);
    FO_EXPECT(fabsf(bombX(world) - 20) < 0.5f);
    // The old RED player spawns in the BLU room.
    float x, y;
    fo_player_position(red, &x, &y);
    FO_EXPECT(x < 8.0f);
    fo_world_free(world);
}

// --- Capture the flag -----------------------------------------------------

// BLU scores at x 10 to 16 m, RED at x 84 to 90 m. A death pit is at x 48
// to 52 m.
static NSString *const ctfAreas =
    @"  <object id=\"6\" name=\"ctf_flagcapturearea_blu\" x=\"100\" y=\"200\" width=\"60\" height=\"80\"/>\n"
    @"  <object id=\"7\" name=\"ctf_flagcapturearea_red\" x=\"840\" y=\"200\" width=\"60\" height=\"80\"/>\n"
    @"  <object id=\"8\" name=\"playerdeatharea\" x=\"480\" y=\"200\" width=\"40\" height=\"80\"/>\n";

static FOWorld *ctfWorld(void) {
    FOWorld *world = newWorld(nil, ctfAreas);
    fo_world_ctf(world, 70, 2.75f, 30, 2.75f);
    return world;
}

static void flagPosition(const FOFlag *f, float *x, float *y) {
    fo_phys_body_position(f->obj.body, x, y);
}

FO_TEST(testCtfEnemyTakesTheFlagAndTeammateDoesNot) {
    FOWorld *world = ctfWorld();
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 70);
    run(world, 0.5f);
    FO_EQUAL_INT(world->mode.flags[FO_TEAM_RED].carrier, -1);
    moveTo(red, 30);
    run(world, 0.5f);
    FO_EQUAL_INT(world->mode.flags[FO_TEAM_BLU].carrier, 1);
    FO_EXPECT(!fo_world_mode_allows(world, red));
    // The flag follows the carrier.
    moveTo(red, 40);
    run(world, 0.1f);
    float fx, fy;
    flagPosition(&world->mode.flags[FO_TEAM_BLU], &fx, &fy);
    FO_EXPECT(fabsf(fx - 40) < 1.0f);
    fo_world_free(world);
}

FO_TEST(testCtfHiddenSpyCannotTakeTheFlag) {
    FOWorld *world = ctfWorld();
    FOPlayer *spy = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SPY, 30);
    spy->cloaked = 1;
    run(world, 0.5f);
    FO_EQUAL_INT(world->mode.flags[FO_TEAM_BLU].carrier, -1);
    fo_world_free(world);
}

FO_TEST(testCtfCaptureScoresAndSendsTheFlagHome) {
    FOWorld *world = ctfWorld();
    FOPlayer *blu = playerAt(world, 1, FO_TEAM_BLU, FO_CLASS_SCOUT, 70);
    FOPlayer *mate = playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 60);
    FOPlayer *red = playerAt(world, 3, FO_TEAM_RED, FO_CLASS_SOLDIER, 40);
    run(world, 0.2f);
    FO_EQUAL_INT(world->mode.flags[FO_TEAM_RED].carrier, 1);
    moveTo(blu, 13);
    run(world, 0.2f);
    FO_EQUAL_INT(world->mode.bluScore, 1);
    FO_EQUAL_INT(world->mode.flags[FO_TEAM_RED].carrier, -1);
    FO_EXPECT(!world->mode.flags[FO_TEAM_RED].dropped);
    float fx, fy;
    flagPosition(&world->mode.flags[FO_TEAM_RED], &fx, &fy);
    FO_EXPECT(fabsf(fx - 70) < 0.5f);
    // 3 to the carrier and 2 to each BLU player.
    FO_EQUAL_INT(blu->score, 5);
    FO_EQUAL_INT(mate->score, 2);
    FO_EQUAL_INT(red->score, 0);
    fo_world_free(world);
}

FO_TEST(testCtfRedCaptureAlsoGivesBluPlayersPoints) {
    FOWorld *world = ctfWorld();
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SCOUT, 30);
    FOPlayer *blu = playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 60);
    run(world, 0.2f);
    moveTo(red, 87);
    run(world, 0.2f);
    FO_EQUAL_INT(world->mode.redScore, 1);
    FO_EQUAL_INT(red->score, 3);
    FO_EQUAL_INT(blu->score, 2);
    fo_world_free(world);
}

FO_TEST(testCtfDroppedFlagReturnsAfter30Seconds) {
    FOWorld *world = ctfWorld();
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 30);
    run(world, 0.2f);
    moveTo(red, 36);
    run(world, 0.2f);
    fo_player_set_health(world, red, 0);
    run(world, 1 / 60.0f);
    FOFlag *f = &world->mode.flags[FO_TEAM_BLU];
    FO_EQUAL_INT(f->carrier, -1);
    FO_EXPECT(f->dropped);
    run(world, 29.0f);
    float fx, fy;
    flagPosition(f, &fx, &fy);
    FO_EXPECT(f->dropped && fabsf(fx - 30) > 3.0f);
    run(world, 1.2f);
    flagPosition(f, &fx, &fy);
    FO_EXPECT(!f->dropped && fabsf(fx - 30) < 0.5f);
    fo_world_free(world);
}

FO_TEST(testCtfFlagInADeathAreaReturnsAtOnce) {
    FOWorld *world = ctfWorld();
    FOFlag *f = &world->mode.flags[FO_TEAM_BLU];
    fo_phys_body_set_transform(f->obj.body, 50, 3, 0);
    run(world, 1 / 60.0f);
    float fx, fy;
    flagPosition(f, &fx, &fy);
    FO_EXPECT(fabsf(fx - 30) < 0.5f);
    fo_world_free(world);
}

FO_TEST(testCtfFourCapturesWinTheRound) {
    FOWorld *world = ctfWorld();
    FOPlayer *blu = playerAt(world, 1, FO_TEAM_BLU, FO_CLASS_SCOUT, 70);
    for (int i = 0; i < 4; i++) {
        moveTo(blu, 70);
        run(world, 0.2f);
        moveTo(blu, 13);
        run(world, 0.2f);
    }
    FO_EQUAL_INT(world->mode.bluScore, 4);
    FO_EXPECT(fo_mode_over(world));
    FO_EQUAL_INT(fo_mode_winner(world), FO_TEAM_BLU);
    // A capture after the end does not count.
    moveTo(blu, 70);
    run(world, 0.2f);
    moveTo(blu, 13);
    run(world, 0.2f);
    FO_EQUAL_INT(world->mode.bluScore, 4);
    run(world, 15.0f);
    FO_EQUAL_INT(world->mode.bluScore, 0);
    FO_EQUAL_INT(blu->team, FO_TEAM_BLU);
    fo_world_free(world);
}

FO_TEST(testCtfStolenSoundForTheLocalTeam) {
    FOWorld *world = ctfWorld();
    world->localId = 2;
    playerAt(world, 2, FO_TEAM_BLU, FO_CLASS_SOLDIER, 60);
    run(world, 0.1f);
    FOPlayer *red = playerAt(world, 1, FO_TEAM_RED, FO_CLASS_SOLDIER, 40);
    moveTo(red, 30);
    int heard = 0;
    for (int i = 0; i < 30 && !heard; i++) {
        run(world, 1 / 60.0f);
        heard = hasSound(world, "flag_stolen");
    }
    FO_EXPECT(heard);
    fo_world_free(world);
}
