// fo_world: the game world of GANGFORT, as GameWorld (wt.java) and the
// objects in it. It owns the Box2D world, the map bodies, and the players.
// All times are milliseconds of the world clock, which moves with each
// frame. The original reads the wall clock (System.currentTimeMillis) in
// the same places.

#ifndef FO_WORLD_H
#define FO_WORLD_H

#include "fo_phys.h"
#include "fo_tmx.h"
#include "fo_weapon.h"

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
    FO_OBJ_BULLET,      // Bullet (xg)
    FO_OBJ_CORPSE,      // PlayerDeadBody (xs)
    FO_OBJ_PICKUP,      // WeaponPickup (xx)
    FO_OBJ_BOMB,        // RollableBomb (act)
    FO_OBJ_CHECKPOINT,  // BombCheckpoint (xf)
    FO_OBJ_FLAG,        // CtfFlag (acm)
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

// One entry of DamageHistoryHolder (acu.java, acv.java).
typedef struct {
    long long time;
    float damage;
    int attacker;
    int item;
    int used;
} FODamage;

#define FO_DAMAGE_MAX 32

// The weapon that a player holds (Weapon, alz.java).
typedef struct {
    const FOWeaponDef *def;
    float ammo;           // z
    long long lastShot;   // c
    float angle;          // d: the aim in degrees from -180 to 180. 0 is no aim.
} FOWeapon;

// PlayerDeadBody (xs.java): the body falls after a death, lands, and
// turns to the slope under it. It goes 9 s after the death.
typedef struct {
    FOObj obj;
    int active;
    long long start;         // o: the time of the death.
    long long stop;          // p: the time of the landing, or 0.
    float turn;              // r: the angle at the end of the turn, in degrees.
    long long turnStart;     // s
    float turnTime;          // t: ms of the turn.
    int facingRight;         // n
} FOCorpse;

// WeaponPickup (xx.java): the weapon that a dead player drops. A touch
// gives 30 % of the full ammo to the weapon of any player.
typedef struct {
    FOObj obj;
    int active;
    const FOWeaponDef *def;
    short team;            // The team of the owner, for the sprite.
} FOPickup;

// SentryGun (xu.java): the gun of a builder. It falls, lands, builds in
// 0.8 s, then turns to the nearest enemy in 10.5 m and shoots.
typedef struct {
    FOObj obj;
    int active;
    int owner;               // p: the id of the builder.
    short team;              // A
    int facingRight;         // u
    float health;            // B
    float aim;               // x: the angle of the turret in radians.
    float want;              // s: the angle to the target.
    int target;              // q: the id of the target, or 0.
    int landed;              // F
    long long built;         // D: the time that the build started, or 0.
    long long lastShot;
    long long alerted;       // z
    FODamage damage[FO_DAMAGE_MAX];
} FOSentry;

// SpyDisguiseData (add.java): the player that a spy looks like.
typedef struct {
    int active;
    long long start;         // i
    int target;              // c
    int cls;                 // d
    short team;              // e
    char name[24];           // f
} FODisguise;

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
    long long landTime;      // The time of the last landing, for the squat frame.
    int jumpSoundUp, jumpSoundDouble;  // an and ap: the jump sounds played.
    float lastVy;            // ao
    long long deathTime;     // B: the time of death, or 0.
    long long hurtTime;      // A: the time that the health last went down.
    FOWeapon weapon;
    int primaryItem;         // The item id of the weapon of the loadout, or 0.
    int score;               // P
    FODamage damage[FO_DAMAGE_MAX];
    int killer;              // q: the id of the last killer.
    FOCorpse corpse;
    char name[24];           // The nickname (ai in xn.java).
    FOPickup pickup;
    // The class abilities (fields of xn.java).
    float steel;             // ah: the charge of the sentry of a builder, to 100.
    FOSentry sentry;
    float cloakEnergy;       // W
    int cloaked;             // U
    long long cloakTime;     // V: the time of the last change of the cloak.
    int backstab;            // X
    long long backstabTime;  // Y
    int backstabHit;         // Z: the hit test of this backstab is done.
    int backstabFacing;      // aa
    FODisguise disguise;     // ac
    int healTarget;          // ad: the id of the player that the medic heals.
    int uber;                // af
    float uberCharge;        // ae: to 20.
    float healScore;         // ag
    int burning;             // L
    long long burnUntil;     // M
    int burnAttacker;        // N: the player of the flame,
    int burnItem;            //    and its weapon.
    float sniperCharge;      // G of SniperRifle, to 100.
} FOPlayer;

// A projectile (Bullet, xg.java).
typedef struct {
    FOObj obj;
    int active;              // o()
    int owner;               // The id of the player that shot it.
    short team;
    const FOWeaponDef *def;
    long long spawn;         // p
    int damage;              // n
    float angle;
    int shape;               // The shape of the body: 0 bullet, 1 grenade, 2 apple.
    int grounded;            // o of a grenade: it touched the ground.
} FOBullet;

#define FO_MAX_BULLETS 160

// The events of one frame, for the sounds, the effects, and the kill feed.
typedef enum {
    FO_EVENT_SOUND,          // name at (x, y)
    FO_EVENT_EFFECT,         // name at (x, y). flip is 1 or -1.
    FO_EVENT_KILL,           // a kills b with item, c assists.
    FO_EVENT_HIT,            // a hits b for value damage.
    FO_EVENT_SHAKE,          // The camera shakes for item ms with the strength value.
} FOEventKind;

typedef struct {
    FOEventKind kind;
    char name[32];
    float x, y, value;
    int flip;
    int a, b, c, item;
} FOEvent;

#define FO_MAX_EVENTS 128

// The game modes (MapGameMode, acs.java, and its subclasses).
typedef enum {
    FO_MODE_NONE = 0,
    FO_MODE_KOTH,   // KothMapGameMode (acq.java): gts_gantown
    FO_MODE_BOMB,   // BombMapGameMode (acl.java): bomb_canyon
    FO_MODE_CTF,    // CtfMapGamemode (acn.java): flag_egypt
} FOModeKind;

#define FO_MAX_CHECKPOINTS 4

// BombCheckpoint (xf.java): a gate that stops the bomb from rolling back.
typedef struct {
    FOObj obj;
    float x, y;
    int active;              // n
    long long changed;       // o: the time of the last change, for the animation.
} FOCheckpoint;

// RollableBomb (act.java).
typedef struct {
    FOObj obj;
    int forward;             // p: 1 when RED pushes it to +x.
    float startX, startY;    // t
    int red[FO_MAX_PLAYERS], redCount;   // w: the RED pushers, in order.
    int blu[FO_MAX_PLAYERS], bluCount;   // x
    long long redPush;       // r: the last time that RED pushed.
    long long finished;      // s: the time that the bomb went into the finish area.
    long long hintTime;      // F
    int hint;                // v: the local player sees the uphill hint.
    float target;            // q: the speed that the bomb wants.
    float lastX;             // K
    FOObj *slopes[8];        // y: the ground under the bomb.
    int slopeCount;
} FOBomb;

// CtfFlag (acm.java).
typedef struct {
    FOObj obj;
    short team;              // m: the team of the flag.
    int carrier;             // n: the id of the carrier, or -1.
    int dropped;             // o: the flag lies on the ground.
    float returnTime;        // p: seconds to the return.
    float homeX, homeY;      // q
    float angle;
    int facingLeft;
} FOFlag;

typedef struct {
    FOModeKind kind;
    // King of the hill.
    float progress;          // a: -100 BLU to 100 RED.
    float redTimer;          // b
    float bluTimer;          // c
    short owner;             // d
    short lastOwner;         // e: -1 before the first update.
    int wasOver;             // f
    // The bomb.
    FOBomb bomb;
    FOCheckpoint checkpoints[FO_MAX_CHECKPOINTS];
    int checkpointCount;
    float bombTimer;         // e of acl.java: 420 s.
    float blastX, blastY;    // d of acl.java: the place of the big explosion.
    long long blastTime;     // c of acl.java.
    // Capture the flag.
    FOFlag flags[3];         // By team.
    int redScore, bluScore;  // d and c of acn.java.
    int lastBluScore, lastRedScore;      // e and f: for the sounds.
    int lastRedCarrier, lastBluCarrier;  // g and h
} FOMode;

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
    FOBullet bullets[FO_MAX_BULLETS];
    FOEvent events[FO_MAX_EVENTS];
    int event_count;
    FOMode mode;
    long long roundEnd;      // d of ws.java: the time of the next round, or 0.
    int localId;             // The id of the local player, for the hint of the bomb.
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

// Player.k(float) (xn.java:899-910): sets the health. At 0 the player
// dies.
void fo_player_set_health(FOWorld *world, FOPlayer *player, float health);

// Player.v() (xn.java:777-819): the death of the player.
void fo_player_die(FOWorld *world, FOPlayer *player);

// Player.E(): the player is dead and waits for the respawn.
int fo_player_waiting(const FOPlayer *player);

// Sets the aim of the weapon in degrees, as Weapon.c(float): the angle
// goes into (-180, 180]. 0 is no aim.
void fo_player_set_aim(FOPlayer *player, float degrees);

// Weapon.d(): the weapon can shoot now. In single player the world is the
// authority, and the original then does not test the ammo.
int fo_player_can_shoot(FOWorld *world, const FOPlayer *player);

// Player.a(float, boolean) (xn.java:742-759): shoots in the direction of
// angle radians. Returns 1 when a shot goes out.
int fo_player_shoot(FOWorld *world, FOPlayer *player, float angle);

// The place where the projectiles start (Weapon.d(float)), for the aim in
// degrees.
void fo_player_muzzle(const FOPlayer *player, float degrees, float *x, float *y);

// Player.a(alz, float) (xn.java:1297-1302): damage from the weapon of the
// player with the id attacker.
void fo_player_damage(FOWorld *world, FOPlayer *target, int attacker, int item, float damage);

// The actions of GameServer.a(xn, short, Object) (ws.java:124-210).
// Action 17: the builder builds or destroys the sentry, the medic starts
// the uber, the spy cloaks or uncloaks.
void fo_player_ability(FOWorld *world, FOPlayer *player);
// Action 18: the backstab of the spy.
void fo_player_backstab(FOWorld *world, FOPlayer *player);
// Action 22: the spy looks like a random enemy, not the player exclude.
void fo_player_disguise(FOWorld *world, FOPlayer *player, int exclude);
// Action 23: the spy looks like itself again.
void fo_player_undisguise(FOWorld *world, FOPlayer *player);
// Action 19: the medic heals the player with the id target, or stops at 0.
void fo_player_heal(FOWorld *world, FOPlayer *player, int target);

// The medic that heals the player in uber (N()), and the medic in uber
// (L()): no damage.
int fo_player_protected(const FOWorld *world, const FOPlayer *player);
// The id of a medic that heals the player (P()), or 0.
int fo_player_healer(const FOWorld *world, const FOPlayer *player);
// The disguise is complete (add.b()): 1.4 s after the start.
int fo_player_disguised(const FOWorld *world, const FOPlayer *player);

// BaseGameMap.a(xn): the player is in the spawn room of its team.
int fo_world_in_own_spawn(const FOWorld *world, const FOPlayer *player);
// The player is in an area of the kind.
int fo_world_in_area(const FOWorld *world, const FOPlayer *player, FOAreaKind kind);
// SentryGun.A(): the build of the sentry is done.
int fo_sentry_is_ready(const FOWorld *world, const FOSentry *sentry);

// Player.b(short) (xn.java:979-1025): the player goes to the other team.
void fo_world_set_team(FOWorld *world, FOPlayer *player, short team);

// The game mode of the map. Call one of these after fo_world_new.
void fo_world_koth(FOWorld *world);
void fo_world_bomb(FOWorld *world, int forward, float startX, float startY, float blastX, float blastY);
void fo_world_bomb_checkpoint(FOWorld *world, float x, float y);
void fo_world_ctf(FOWorld *world, float redX, float redY, float bluX, float bluY);

// MapGameMode.h(): the round is over. i(): the team that won, or 0.
int fo_mode_over(const FOWorld *world);
int fo_mode_winner(const FOWorld *world);
// MapGameMode.d(): the bomb is in the fall area. e(): the place where the
// camera looks at the end.
int fo_mode_bomb_down(const FOWorld *world);
void fo_mode_focus(const FOWorld *world, float *x, float *y);
// MapGameMode.b(): the place of the objective, for the bots.
void fo_mode_objective(const FOWorld *world, float *x, float *y);

// Adds an event of the frame.
void fo_world_event(FOWorld *world, FOEventKind kind, const char *name, float x, float y);

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
