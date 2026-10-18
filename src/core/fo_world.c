#include "fo_world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FO_STEP (1.0f / 60.0f)
#define FO_DEG 0.017453292f

// --- Small helpers ------------------------------------------------------

float fo_angle_norm(float angle) {
    return (float)((double)angle - 6.2831854820251465 * floor((3.1415927f + angle) / 6.2831855f));
}

float fo_world_random(FOWorld *world) {
    // xorshift32. The numbers do not need to agree with Java.
    unsigned int x = world->seed ? world->seed : 2463534242u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    world->seed = x;
    return (x >> 8) / 16777216.0f;
}

float fo_class_health(int cls) {
    static const float kHealth[10] = {100, 125, 200, 175, 175, 300, 125, 150, 125, 125};
    return cls >= 1 && cls <= 9 ? kHealth[cls] : 100;
}

float fo_class_speed(int cls) {
    static const float kFactor[10] = {1, 1.18f, 0.8f, 1, 0.93f, 0.77f, 0.9f, 1.07f, 1, 1};
    return 8.0f * (cls >= 1 && cls <= 9 ? kFactor[cls] : 1);
}

static void touch_add(FOTouchSet *set, FOObj *obj) {
    int found = 0;
    for (int i = 0; i < set->count; i++) {
        if (set->objs[i] == obj) found = 1;
    }
    if (!found && set->count < FO_TOUCH_MAX) set->objs[set->count++] = obj;
    if (obj->kind == FO_OBJ_GROUND) set->ground++;
}

static void touch_remove(FOTouchSet *set, FOObj *obj) {
    for (int i = 0; i < set->count; i++) {
        if (set->objs[i] == obj) {
            memmove(&set->objs[i], &set->objs[i + 1], sizeof(FOObj *) * (size_t)(set->count - i - 1));
            set->count--;
            break;
        }
    }
    if (obj->kind == FO_OBJ_GROUND) set->ground--;
}

static int touch_has(const FOTouchSet *set, const FOObj *obj) {
    for (int i = 0; i < set->count; i++) {
        if (set->objs[i] == obj) return 1;
    }
    return 0;
}

// aks.i(): removes the objects that are gone.
static void touch_purge(FOTouchSet *set) {
    for (int i = 0; i < set->count;) {
        if (set->objs[i]->dead) {
            touch_remove(set, set->objs[i]);
        } else {
            i++;
        }
    }
}

static FOObj *fixture_obj(FOPhysFixture *fixture) {
    return (FOObj *)fo_phys_body_user(fo_phys_fixture_body(fixture));
}

static float obj_angle(const FOObj *obj) {
    return obj->body ? fo_phys_body_angle(obj->body) : 0;
}

// --- The map --------------------------------------------------------------

static void add_static_box(FOWorld *world, FOObj *obj, FOObjKind kind, const FOTmxBox *box, float friction,
                           unsigned short category, unsigned short mask) {
    obj->kind = kind;
    obj->w = box->w;
    obj->h = box->h;
    obj->body = fo_phys_body_new(world->phys, FO_BODY_STATIC, box->x, box->y, box->angle, obj);
    fo_phys_body_set_fixed_rotation(obj->body, 1);
    FOFixtureDef def = {0.5f, friction, 0, category, mask, 0};
    fo_phys_box(obj->body, box->w / 2, box->h / 2, box->w * 0.5f, -box->h * 0.5f, 0, &def);
}

// The wall of a spawn room, which blocks the players of the other team
// (akm.java:143-159). team is the team of the room.
static void add_barrier(FOWorld *world, FOObj *obj, const FORect *room, short team) {
    FOTmxBox box = {room->x, room->y + room->h, room->w, room->h, 0};
    add_static_box(world, obj, FO_OBJ_BARRIER, &box, 0, FO_CAT_BARRIER,
                   team == FO_TEAM_RED ? FO_CAT_BLU : FO_CAT_RED);
}

static void on_begin(void *context, FOPhysFixture *a, FOPhysFixture *b, float px, float py, float nx, float ny) {
    FOWorld *world = context;
    if (world->begin_count >= (int)(sizeof(world->begins) / sizeof(world->begins[0]))) return;
    world->begins[world->begin_count].a = a;
    world->begins[world->begin_count].b = b;
    world->begins[world->begin_count].px = px;
    world->begins[world->begin_count].py = py;
    world->begins[world->begin_count].nx = nx;
    world->begins[world->begin_count].ny = ny;
    world->begin_count++;
}

static void player_contact_end(FOPlayer *p, FOPhysFixture *fixture, FOObj *other);

static void on_end(void *context, FOPhysFixture *a, FOPhysFixture *b) {
    (void)context;
    FOObj *oa = fixture_obj(a), *ob = fixture_obj(b);
    if (oa && oa->kind == FO_OBJ_PLAYER && ob) player_contact_end((FOPlayer *)oa, a, ob);
    if (ob && ob->kind == FO_OBJ_PLAYER && oa) player_contact_end((FOPlayer *)ob, b, oa);
}

FOWorld *fo_world_new(const char *tmx_text, unsigned int seed) {
    FOWorld *world = calloc(1, sizeof(FOWorld));
    if (!world) return NULL;
    if (!fo_tmx_parse(&world->map, tmx_text)) {
        free(world);
        return NULL;
    }
    world->seed = seed ? seed : 1;
    world->phys = fo_phys_world_new(0, -15);
    FOContactListener listener = {world, on_begin, on_end};
    fo_phys_world_listener(world->phys, &listener);
    for (int i = 0; i < world->map.box_count; i++) {
        add_static_box(world, &world->ground[i], FO_OBJ_GROUND, &world->map.boxes[i], 0.7f, FO_CAT_GROUND, 0x7fff);
    }
    world->ground_count = world->map.box_count;
    const FORect *red = fo_tmx_area(&world->map, FO_AREA_SPAWN_RED);
    const FORect *blu = fo_tmx_area(&world->map, FO_AREA_SPAWN_BLU);
    if (red) add_barrier(world, &world->barriers[0], red, FO_TEAM_RED);
    if (blu) add_barrier(world, &world->barriers[1], blu, FO_TEAM_BLU);
    return world;
}

void fo_world_free(FOWorld *world) {
    if (!world) return;
    fo_phys_world_free(world->phys);
    free(world);
}

void fo_world_add_door(FOWorld *world, short team, float x, float y, float w, float h) {
    if (world->door_count >= FO_MAX_DOORS) return;
    FODoor *d = &world->doors[world->door_count++];
    d->obj.kind = FO_OBJ_DOOR;
    d->obj.w = w;
    d->obj.h = h;
    d->team = team;
    d->x = x;
    d->y = y;
    d->openY = y + h - 0.1f;
    d->targetY = y;
    d->obj.body = fo_phys_body_new(world->phys, FO_BODY_STATIC, x, y, 0, &d->obj);
    fo_phys_body_set_fixed_rotation(d->obj.body, 1);
    // The default fixture of libGDX: density 0, friction 0.2.
    FOFixtureDef def = {0, 0.2f, 0, FO_CAT_GROUND, 0, 0};
    fo_phys_box(d->obj.body, w / 2, h / 2, 0, 0, 0, &def);
}

// SpawnDoor.a(float) (xw.java): opens for a teammate near the door, and
// slides to the open or closed place at 25 m/s.
static void door_update(FOWorld *world, FODoor *d, float dt) {
    int near = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *p = &world->players[i];
        if (!p->used || p->team != d->team || !p->alive) continue;
        float x, y;
        fo_player_position(p, &x, &y);
        if (fabsf(x - d->x) < 2.0f && y <= d->openY && fabsf(d->y - y) < 1.9f) near++;
    }
    if (near > 0 && !d->open) {
        d->open = 1;
        d->targetY = d->openY;
        fo_phys_body_set_mask(d->obj.body, FO_CAT_GROUND, 0);
    } else if (near == 0 && d->open) {
        d->open = 0;
        d->targetY = d->y;
        fo_phys_body_set_mask(d->obj.body, FO_CAT_GROUND, 0x7fff);
    }
    float x, y;
    fo_phys_body_position(d->obj.body, &x, &y);
    if (fabs(y - d->targetY) > 0.0010000000149011613) {
        if (y - d->targetY > 0) {
            y -= 25.0f * dt;
            if (y < d->targetY) y = d->targetY;
        } else {
            y += 25.0f * dt;
            if (y > d->targetY) y = d->targetY;
        }
        fo_phys_body_set_transform(d->obj.body, x, y, 0);
    }
}

// --- The players ----------------------------------------------------------

FOPlayer *fo_world_player(FOWorld *world, int id) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used && world->players[i].id == id) return &world->players[i];
    }
    return NULL;
}

// The body of Player (xn.java:135-216): four circles and a box, a foot
// sensor, and a sensor on each side.
static void make_body(FOWorld *world, FOPlayer *p) {
    float k = 0.90000004f, l = 1.9f;
    p->obj.kind = FO_OBJ_PLAYER;
    p->obj.w = k;
    p->obj.h = l;
    FOPhysBody *body = fo_phys_body_new(world->phys, FO_BODY_DYNAMIC, 0, 0, 0, &p->obj);
    fo_phys_body_set_fixed_rotation(body, 1);
    p->obj.body = body;
    float f = k / 2.0f + 0.025f;
    FOFixtureDef slick = {1.1f, 0.0f, 0, 0, 0, 0};
    FOFixtureDef grip = {1.1f, 0.7f, 0, 0, 0, 0};
    fo_phys_circle(body, f - 0.05f, 0, f / 2.0f + 0.2f, &slick);
    fo_phys_circle(body, f - 0.1f, 0, f / 2.0f + 0.275f, &grip);
    fo_phys_circle(body, f - 0.05f, 0, -f - 0.025f, &slick);
    fo_phys_circle(body, f - 0.1f, 0, -f - 0.1f, &grip);
    fo_phys_box(body, k / 2.0f - 0.05f, l / 2.0f - 0.2f - f / 2.0f, 0, 0, 0, &grip);
    FOFixtureDef foot = {0, 0.2f, 0, FO_CAT_FOOT, 0, 1};
    fo_phys_box(body, k / 2.4f, 0.2f, 0, -l / 2.0f, 0, &foot);
    FOFixtureDef left = {0, 0.2f, 0, FO_CAT_LEFT, 0, 1};
    fo_phys_box(body, 0.060000002f, l / 2.0f - 0.115f - f / 2.0f, -k / 2.0f, 0.115f - f / 2.0f, 0, &left);
    FOFixtureDef right = {0, 0.2f, 0, FO_CAT_RIGHT, 0, 1};
    fo_phys_box(body, 0.060000002f, l / 2.0f - 0.115f - f / 2.0f, k / 2.0f, 0.115f - f / 2.0f, 0, &right);
    fo_phys_body_set_mass(body, 2.58646f);
}

// Player.b(short) (xn.java:975-1002): the filters of the team.
static void set_team_filters(FOPlayer *p) {
    unsigned short footMask = FO_CAT_GROUND, bodyCat, bodyMask = 2195;
    if (p->team == FO_TEAM_RED) {
        footMask |= FO_CAT_BLU | FO_CAT_BLU_SENTRY;
        bodyCat = FO_CAT_RED;
        bodyMask |= 72;
    } else {
        footMask |= FO_CAT_RED | FO_CAT_RED_SENTRY;
        bodyCat = FO_CAT_BLU;
        bodyMask |= 36;
    }
    fo_phys_body_set_mask(p->obj.body, FO_CAT_FOOT, footMask);
    // The four circles and the box have the default category 1 at the
    // start, or the category of the old team.
    unsigned short cats[3] = {1, FO_CAT_RED, FO_CAT_BLU};
    for (int i = 0; i < 3; i++) fo_phys_body_set_filter(p->obj.body, cats[i], bodyCat, bodyMask);
}

FOPlayer *fo_world_add_player(FOWorld *world, int id, short team) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *p = &world->players[i];
        if (p->used) continue;
        memset(p, 0, sizeof(*p));
        p->used = 1;
        p->id = id;
        p->move = FO_MOVE_STOP;
        p->facingRight = 1;
        p->doubleJumpReady = 1;
        make_body(world, p);
        p->team = team;
        set_team_filters(p);
        return p;
    }
    return NULL;
}

void fo_world_spawn(FOWorld *world, FOPlayer *p, int cls) {
    const FORect *room = fo_tmx_area(&world->map, p->team == FO_TEAM_RED ? FO_AREA_SPAWN_RED : FO_AREA_SPAWN_BLU);
    float x = 0, y = 0;
    if (room) {
        float x0 = room->x + 2.25f, x1 = room->x + room->w - 2.25f;
        float y0 = room->y + 1.9f, y1 = room->y + room->h - 1.9f;
        x = x0 + (x1 - x0) * fo_world_random(world);
        y = fo_world_random(world) * (y1 - y0) + y0;
    }
    fo_phys_body_set_transform(p->obj.body, x, y, 0);
    fo_phys_body_set_velocity(p->obj.body, 0, -5);
    fo_phys_body_set_awake(p->obj.body, 1);
    p->cls = cls;
    p->health = fo_class_health(cls);
    p->alive = 1;
    p->deathTime = 0;
}

void fo_player_position(const FOPlayer *p, float *x, float *y) {
    fo_phys_body_position(p->obj.body, x, y);
}

void fo_player_velocity(const FOPlayer *p, float *vx, float *vy) {
    fo_phys_body_velocity(p->obj.body, vx, vy);
}

int fo_player_grounded(const FOPlayer *p) {
    return p->foot.count > 0;
}

// The angle of the ground under the player, or 0 in the air.
static float ground_angle(const FOPlayer *p) {
    return p->foot.count > 0 ? fo_angle_norm(obj_angle(p->foot.objs[p->foot.count - 1])) : 0;
}

// Player.a(Fixture, Object) (xn.java:1329-1370).
static void player_contact_begin(FOWorld *world, FOPlayer *p, FOPhysFixture *fixture, FOObj *other) {
    unsigned short cat = fo_phys_fixture_category(fixture);
    if (cat == FO_CAT_FOOT) {
        int wasInAir = p->foot.count == 0;
        touch_add(&p->foot, other);
        // a(z, z2): the landing resets the double jump.
        if (wasInAir) p->landTime = world->now;
        p->doubleJumpReady = 1;
        return;
    }
    FOTouchSet *set = cat == FO_CAT_LEFT ? &p->left : cat == FO_CAT_RIGHT ? &p->right : NULL;
    if (!set) return;
    if (other->kind == FO_OBJ_PLAYER) {
        if (((FOPlayer *)other)->team != p->team) touch_add(set, other);
    } else if (other->kind == FO_OBJ_SENTRY) {
        // As the original, a sentry on the right goes into the left set.
        touch_add(&p->left, other);
    } else if (other->kind == FO_OBJ_GROUND || other->kind == FO_OBJ_DOOR) {
        touch_add(set, other);
    }
}

// Player.b(Fixture, Object) (xn.java:1372-1382).
static void player_contact_end(FOPlayer *p, FOPhysFixture *fixture, FOObj *other) {
    unsigned short cat = fo_phys_fixture_category(fixture);
    if (cat == FO_CAT_FOOT) touch_remove(&p->foot, other);
    if (cat == FO_CAT_LEFT) touch_remove(&p->left, other);
    if (cat == FO_CAT_RIGHT) touch_remove(&p->right, other);
}

// Player.W() (xn.java:1384-1407): before each step, a player that walks
// into an enemy or a sentry stops.
static void player_before_step(FOPlayer *p) {
    float vx, vy;
    fo_player_velocity(p, &vx, &vy);
    const FOTouchSet *set = vx < 0 ? &p->left : vx > 0 ? &p->right : NULL;
    if (!set) return;
    for (int i = 0; i < set->count; i++) {
        FOObj *o = set->objs[i];
        if ((o->kind == FO_OBJ_PLAYER || o->kind == FO_OBJ_SENTRY) && !touch_has(&p->foot, o)) {
            fo_phys_body_set_velocity(p->obj.body, 0, vy);
            return;
        }
    }
}

// Player.h(float) (xn.java:547-641): the walk.
static void player_move(FOWorld *world, FOPlayer *p) {
    FOPhysBody *body = p->obj.body;
    float speed = fo_class_speed(p->cls);
    float fA = ground_angle(p);
    float vx, vy;
    fo_player_velocity(p, &vx, &vy);
    if (fA == 0.0f) {
        vx = p->move == FO_MOVE_RIGHT ? speed : p->move == FO_MOVE_LEFT ? -speed : 0;
        fo_phys_body_set_velocity(body, vx, vy);
    } else if (fo_player_grounded(p) && world->now - p->jumpTime > 100) {
        if (p->move == FO_MOVE_STOP) {
            fo_phys_body_set_velocity(body, 0, vy > 0 ? 0 : vy);
        } else {
            int steep = fA > FO_DEG;
            float c = (float)cos(fA);
            float f4 = c * speed;
            float fCos2 = (float)(c * sin(fA)) * speed;
            float dst = sqrtf((f4 - speed) * (f4 - speed) + fCos2 * fCos2);
            if (p->move == FO_MOVE_RIGHT) {
                if (steep) {
                    fo_phys_body_set_velocity(body, f4 * 1.8f, -fCos2);
                } else {
                    fo_phys_body_set_velocity(body, speed + dst, 0);
                }
            } else if (p->move == FO_MOVE_LEFT) {
                if (steep) {
                    fo_phys_body_set_velocity(body, -(speed + dst), 0);
                } else {
                    fo_phys_body_set_velocity(body, -f4 * 1.8f, fCos2);
                }
            }
        }
    } else {
        float s = p->move == FO_MOVE_RIGHT ? speed : p->move == FO_MOVE_LEFT ? -speed : 0;
        fo_phys_body_set_velocity(body, s, vy);
    }
    // A wall, a door, or a sentry at the side stops the walk.
    fo_player_velocity(p, &vx, &vy);
    const FOTouchSet *set = vx < 0 ? &p->left : vx > 0 ? &p->right : NULL;
    for (int i = 0; set && i < set->count; i++) {
        FOObj *o = set->objs[i];
        if ((o->kind == FO_OBJ_GROUND || o->kind == FO_OBJ_DOOR || o->kind == FO_OBJ_SENTRY) && !o->dead &&
            !touch_has(&p->foot, o)) {
            fo_player_velocity(p, &vx, &vy);
            fo_phys_body_set_velocity(body, 0, vy);
        }
    }
    if (p->aim == 0) {
        if (p->move == FO_MOVE_RIGHT) p->facingRight = 1;
        if (p->move == FO_MOVE_LEFT) p->facingRight = 0;
    }
}

int fo_player_jump(FOWorld *world, FOPlayer *p) {
    if (world->now - p->jumpTime < 100) return 0;
    int air = 0;
    if (!fo_player_grounded(p)) {
        if (p->cls != FO_CLASS_SCOUT || !p->doubleJumpReady) return 0;
        p->doubleJumpReady = 0;
        air = 1;
    }
    // Player.i(float) (xn.java:726-740): a jump against a steep slope
    // loses the speed along x.
    float speed = air ? 8.5f * 0.8f : 8.5f;
    float fA = ground_angle(p);
    float vx, vy, keep = 0;
    fo_player_velocity(p, &vx, &vy);
    if (fA == 0.0f) {
        keep = vx;
    } else {
        int steep = fA > FO_DEG;
        if ((p->move != FO_MOVE_RIGHT || steep) && (p->move != FO_MOVE_LEFT || !steep)) keep = vx;
    }
    fo_phys_body_set_velocity(p->obj.body, keep, speed);
    p->jumpTime = world->now;
    return 1;
}

// The jump sounds (xn.java:529-544): one when a jump starts, and one for
// the double jump of the rusher.
static void jump_sounds(FOWorld *world, FOPlayer *p) {
    float vx, vy;
    fo_player_velocity(p, &vx, &vy);
    if (fo_player_grounded(p)) {
        if (vy <= 0) {
            p->jumpSoundUp = 0;
            p->jumpSoundDouble = 0;
        }
    } else if (vy > 1.0f && !p->jumpSoundUp) {
        world->jumps++;
        p->jumpSoundUp = 1;
    } else if (p->cls == FO_CLASS_SCOUT && vy > 0 && vy > p->lastVy && !p->jumpSoundDouble && vy - p->lastVy > 1.0f) {
        world->jumps++;
        p->jumpSoundDouble = 1;
    }
    p->lastVy = vy;
}

static void player_update(FOWorld *world, FOPlayer *p, float dt) {
    (void)dt;
    touch_purge(&p->foot);
    touch_purge(&p->left);
    touch_purge(&p->right);
    if (!p->alive) return;
    player_move(world, p);
    jump_sounds(world, p);
    // The vertical speed stays in [-20, 20] (xn.java:407).
    float vx, vy;
    fo_player_velocity(p, &vx, &vy);
    fo_phys_body_set_velocity(p->obj.body, vx, vy < -20 ? -20 : vy > 20 ? 20 : vy);
}

// GameWorld.s() (wt.java:96-140): the contacts of the last step.
static void dispatch_begins(FOWorld *world) {
    for (int i = 0; i < world->begin_count; i++) {
        FOObj *oa = fixture_obj(world->begins[i].a), *ob = fixture_obj(world->begins[i].b);
        if (!oa || !ob) continue;
        if (oa->kind == FO_OBJ_PLAYER) player_contact_begin(world, (FOPlayer *)oa, world->begins[i].a, ob);
        if (ob->kind == FO_OBJ_PLAYER) player_contact_begin(world, (FOPlayer *)ob, world->begins[i].b, oa);
    }
    world->begin_count = 0;
}

void fo_world_update(FOWorld *world, float dt) {
    if (dt < 0) dt = 0;
    if (dt > 0.1f) dt = 0.1f;
    world->jumps = 0;
    world->now += (long long)(dt * 1000.0f + 0.5f);
    world->accumulator += dt;
    while (world->accumulator >= 0.016666668f) {
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            if (world->players[i].used && world->players[i].alive) player_before_step(&world->players[i]);
        }
        fo_phys_step(world->phys, 0.016666668f, 8, 8);
        world->accumulator -= 0.016666668f;
        dispatch_begins(world);
    }
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used) player_update(world, &world->players[i], dt);
    }
    for (int i = 0; i < world->door_count; i++) door_update(world, &world->doors[i], dt);
}
