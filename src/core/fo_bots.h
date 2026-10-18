// fo_bots: the bots of single player. The controller is BotController
// (zm.java), the aim is BotAimer (zi.java), the walk is BotNavigator
// (zn.java), and the path search is PathFinder (zu.java). The brains are
// BotBrain (zk.java) and its subclasses for the builder (zp.java), the
// dealer (zs.java), the camper (zw.java), and the agent (zx.java). The
// rules are in reference/notes/bots-server.md.

#ifndef FO_BOTS_H
#define FO_BOTS_H

#include "fo_graph.h"
#include "fo_world.h"

// The state that the aimer keeps about one target (SeeableGameObjectTarget,
// zv.java).
typedef struct {
    int used;
    float dist2;    // b
    int shootable;  // c
    int seen;       // d
} FOBotTarget;

#define FO_BOT_PATH 128

typedef struct {
    int used;
    int id;                    // The id of the player of the bot.
    // BotController (zm.java).
    long long spawnWait;       // j
    long long waitMs;          // k
    int started;               // h: the first goal is set.
    long long nextClass;       // l: the time of the next choice of a class.
    int brainClass;            // The class of the brain, or 0.
    // BotNavigator (zn.java).
    int paused;                // a
    int path[FO_BOT_PATH];     // d: waypoint indices.
    int pathCount;
    float lastX, lastY;        // e
    long long stuckTime;       // f
    int link;                  // g: the type of the current link, or -1.
    long long linkTime;        // h
    int secondJump;            // i
    int retry;                 // j: the goal to try again, or -1.
    long long retryTime;       // k
    int free;                  // m: the goal is a free point.
    float freeX, freeY;        // l
    // BotAimer (zi.java).
    float turn;                // m: the part of the turn in each frame.
    int rescanMs;              // k
    float goal;                // e: the angle that the aim goes to.
    long long scanTime;        // h
    float idle;                // i
    long long idleTime;        // j
    int target;                // l: the target. A player index, or
                               // FO_MAX_PLAYERS + the index of the owner of a
                               // sentry, or -1.
    FOBotTarget players[FO_MAX_PLAYERS];  // f
    FOBotTarget sentries[FO_MAX_PLAYERS]; // g, by the index of the owner.
    // BotBrain (zk.java).
    long long goalTime;        // d
    long long goalMs;          // g
    long long huntTime;        // e
    long long huntMs;          // f
    float huntChance;          // h
    long long stopTime;        // i
    long long stopMs;          // j
    int stop;                  // b
    // EngineerBotBrain (zp.java).
    float buildFar, moveFar;   // d and e
    int enemyRight;            // f
    long long buildTime;       // g
    long long buildMs;         // h
    // MedicBotBrain (zs.java).
    int follow;                // d: the player index, or -1.
    float followLength;        // e
    long long followTime;      // f
    long long followMs;        // g
    long long healTime;        // h
    long long healMs;          // i
    // SniperBotBrain (zw.java).
    long long campStart;       // d
    int toCamp;                // e
    long long campMs;          // f
    int campRight;             // g
    // SpyBotBrain (zx.java).
    long long lookTime;        // e
    long long lookMs;          // f
    long long waitStart;       // g
    long long waitMs2;         // h
} FOBot;

typedef struct {
    FOBot bots[FO_MAX_PLAYERS];
    FOGraph graph;
    int maxPlayers;
    int localId;
} FOBots;

// Starts the bots of a world. The mode of the world tells the map.
void fo_bots_init(FOBots *bots, FOWorld *world, int maxPlayers, int localId);

// SingleplayerGameController.i() (xa.java:138-206): adds bots until the
// world has maxPlayers players. Each bot picks a team and a class and
// spawns at once.
void fo_bots_fill(FOBots *bots, FOWorld *world);

// Adds one bot with the player id. It picks its team and its class.
FOBot *fo_bots_add(FOBots *bots, FOWorld *world, int id, const char *name);

// The update of each bot (xa.java:55-58). Call it before fo_world_update.
void fo_bots_update(FOBots *bots, FOWorld *world, float dt);

// GameServer.a(xn, 1..9) (ws.java:62-81): the player asks for a class.
// In the own spawn room it spawns at once. Out of it, it dies.
void fo_world_change_class(FOWorld *world, FOPlayer *player, int cls);

#endif
