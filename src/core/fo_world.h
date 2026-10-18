// fo_world: the game world of GANGFORT, as GameWorld (wt.java) and the
// objects in it. It owns the Box2D world, the map bodies, and the players.
// All times are milliseconds of the world clock, which moves with each
// frame. The original reads the wall clock (System.currentTimeMillis) in
// the same places.

#ifndef FO_WORLD_H
#define FO_WORLD_H

#include "fo_phys.h"
#include "fo_tmx.h"

#define FO_MAX_PLAYERS 16
#define FO_TOUCH_MAX 24

// The team ids of GANGFORT.
enum {
    FO_TEAM_NONE = 0,
    FO_TEAM_BLU = 1,
    FO_TEAM_RED = 2,
};

// The class ids of GANGFORT, with the names of the sprites.
enum {
    FO_CLASS_NONE = 0,
    FO_CLASS_SCOUT = 1,     // "rusher"
    FO_CLASS_SOLDIER = 2,   // "marine"
    FO_CLASS_PYRO = 3,      // "torcher"
    FO_CLASS_DEMOMAN = 4,   // "yash"
    FO_CLASS_HEAVY = 5,     // "mass"
    FO_CLASS_ENGINEER = 6,  // "builder"
    FO_CLASS_MEDIC = 7,     // "dealer"
    FO_CLASS_SNIPER = 8,    // "camper"
    FO_CLASS_SPY = 9,       // "agent"
};

// The move codes of the input (GameScreen, aeb.java:152-158).
enum {
    FO_MOVE_LEFT = 1000,
    FO_MOVE_RIGHT = 2000,
    FO_MOVE_STOP = 3000,
};

// The collision categories of GANGFORT (see reference/notes).
enum {
    FO_CAT_BULLET = 1,
    FO_CAT_GROUND = 2,
    FO_CAT_RED = 4,
    FO_CAT_BLU = 8,
    FO_CAT_FOOT = 16,
    FO_CAT_RED_SENTRY = 32,
    FO_CAT_BLU_SENTRY = 64,
    FO_CAT_BARRIER = 128,
    FO_CAT_DEBRIS = 256,
    FO_CAT_LEFT = 512,
    FO_CAT_RIGHT = 1024,
    FO_CAT_PICKUP = 2048,
    FO_CAT_BOMB = 4096,
    FO_CAT_CHECKPOINT = 8192,
    FO_CAT_FLAG = 16384,
};

typedef enum {
    FO_OBJ_GROUND = 1,  // MapGround (xl)
    FO_OBJ_DOOR,        // SpawnDoor (xw)
    FO_OBJ_BARRIER,     // the wall of a spawn room for the other team
    FO_OBJ_PLAYER,      // Player (xn)
    FO_OBJ_SENTRY,      // SentryGun (xu)
} FOObjKind;

// The head of each object with a body. The user data of the body points
// to it. dead is the field h of GameObject: the object is gone.
typedef struct FOObj {
    FOObjKind kind;
    FOPhysBody *body;
    int dead;
    float w, h;
} FOObj;

// PlayerTouchingSensorData (aks.java): the objects that one sensor of a
// player touches, in the order of the contacts.
typedef struct {
    FOObj *objs[FO_TOUCH_MAX];
    int count;
    int ground;  // The number of MapGround objects in the list.
} FOTouchSet;

typedef struct {
    FOObj obj;
    short team;
    float x, y;        // The closed position (m in xw.java).
    float openY;       // The position when open (n in xw.java).
    float targetY;     // q in xw.java.
    int open;
} FODoor;

typedef struct FOPlayer {
    FOObj obj;
    int used;
    int id;
    short team;
    int cls;
    int alive;               // A(): the player is in the world.
    float health;
    FOTouchSet foot, left, right;
    int move;                // The move code of the input.
    long long jumpTime;      // Q: the time of the last jump.
    int doubleJumpReady;     // T: the rusher can jump in the air.
    int facingRight;         // az
    float aim;               // The aim angle of the weapon in degrees. 0 is no aim.
    long long landTime;      // The time of the last landing, for the squat frame.
    int jumpSoundUp, jumpSoundDouble;  // an and ap: the jump sounds played.
    float lastVy;            // ao
    long long deathTime;     // B: the time of death.
} FOPlayer;

#define FO_MAX_DOORS 4
#define FO_MAX_GROUND (FO_TMX_MAX_BOXES)

typedef struct FOWorld {
    FOPhysWorld *phys;
    FOTmx map;
    long long now;           // The world clock in ms.
    float accumulator;
    unsigned int seed;       // The state of the random numbers.
    FOObj ground[FO_MAX_GROUND];
    int ground_count;
    FOObj barriers[2];
    FODoor doors[FO_MAX_DOORS];
    int door_count;
    FOPlayer players[FO_MAX_PLAYERS];
    // The contacts that started in the last step. GameWorld handles them
    // after the step (wt.java:96-140).
    struct {
        FOPhysFixture *a, *b;
        float px, py, nx, ny;
    } begins[256];
    int begin_count;
    // The events of the last frame, for the sounds.
    int jumps;
} FOWorld;

// Makes the world of a map from the text of its .tmx file. Returns NULL
// when the text is not a map.
FOWorld *fo_world_new(const char *tmx_text, unsigned int seed);
void fo_world_free(FOWorld *world);

// Adds a spawn door of team at (x, y), as the map classes do. w and h
// are the size of the door.
void fo_world_add_door(FOWorld *world, short team, float x, float y, float w, float h);

// A random number from 0 to 1, as MathUtils.random().
float fo_world_random(FOWorld *world);

// Adds a player with the id. Returns NULL when the world is full.
FOPlayer *fo_world_add_player(FOWorld *world, int id, short team);
FOPlayer *fo_world_player(FOWorld *world, int id);

// Spawns the player as the class at a random point of the spawn room of
// its team, with full health (wt.java:165-176).
void fo_world_spawn(FOWorld *world, FOPlayer *player, int cls);

// Moves the world by dt seconds, as GameWorld.update.
void fo_world_update(FOWorld *world, float dt);

// The jump of the input (Player.t, xn.java:705-724). Returns 1 when the
// player jumps.
int fo_player_jump(FOWorld *world, FOPlayer *player);

// The position and the velocity of the body of a player.
void fo_player_position(const FOPlayer *player, float *x, float *y);
void fo_player_velocity(const FOPlayer *player, float *vx, float *vy);
int fo_player_grounded(const FOPlayer *player);

// The maximum health and the run speed of a class (xn.java:876, 1415).
float fo_class_health(int cls);
float fo_class_speed(int cls);

// The angle in [-pi, pi), as Utils.a(float) (alf.java:51).
float fo_angle_norm(float angle);

#endif
