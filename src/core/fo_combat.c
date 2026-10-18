// The weapons, the projectiles, the damage, and the kill credit of the
// world: Weapon (alz.java), Bullet (xg.java), and the death of Player.

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "fo_world.h"
#include "fo_world_private.h"

#define FO_DEG 0.017453292f

void fo_world_event(FOWorld *world, FOEventKind kind, const char *name, float x, float y) {
    if (world->event_count >= FO_MAX_EVENTS) return;
    FOEvent *e = &world->events[world->event_count++];
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    if (name) snprintf(e->name, sizeof(e->name), "%s", name);
    e->x = x;
    e->y = y;
    e->flip = 1;
}

void fo_world_sound(FOWorld *world, const char *name, float x, float y, int attach) {
    fo_world_event(world, FO_EVENT_SOUND, name, x, y);
    if (world->event_count > 0 && world->events[world->event_count - 1].kind == FO_EVENT_SOUND) {
        world->events[world->event_count - 1].a = attach;
    }
}

// --- The aim and the muzzle -------------------------------------------------

void fo_player_set_aim(FOPlayer *p, float degrees) {
    float a = fmodf(fmodf(degrees, 360.0f) + 360.0f, 360.0f);
    if (a > 180.0f) a -= 360.0f;
    p->weapon.angle = a;
}

void fo_player_muzzle(const FOPlayer *p, float degrees, float *mx, float *my) {
    const FOArm *arm = fo_class_arm(p->cls);
    const FOWeaponDef *def = p->weapon.def;
    float px, py;
    fo_player_position(p, &px, &py);
    float jx = arm->jx;
    if (!p->facingRight) jx = (p->obj.w - jx) - arm->h;
    float x = jx + (px - p->obj.w / 2) + arm->kx;
    float y = (py - p->obj.h / 2) + arm->jy + arm->ky;
    if (def) {
        float a = degrees * FO_DEG;
        float iy = p->facingRight ? def->iy : -def->iy;
        x = (float)(x + (def->ix * cos(a) - iy * sin(a)));
        y = (float)(cos(a) * iy + def->ix * sin(a) + y);
    }
    *mx = x;
    *my = y;
}

// Weapon.i(): a ray from the player to a point past the muzzle. The shot
// fails when the ray meets a wall or a door.
typedef struct {
    int hit;
} FOWallRay;

static float wall_report(void *context, FOPhysFixture *f, float px, float py, float nx, float ny, float fraction) {
    (void)px;
    (void)py;
    (void)nx;
    (void)ny;
    (void)fraction;
    FOObj *o = fo_world_fixture_obj(f);
    if (!o || (o->kind != FO_OBJ_GROUND && o->kind != FO_OBJ_DOOR)) return 1;
    ((FOWallRay *)context)->hit = 1;
    return 0;
}

static int muzzle_in_wall(FOWorld *world, const FOPlayer *p) {
    float mx, my, px, py;
    fo_player_muzzle(p, p->weapon.angle, &mx, &my);
    fo_player_position(p, &px, &py);
    float dx = mx - px, dy = my - py, len = sqrtf(dx * dx + dy * dy);
    if (len > 0) {
        mx += dx / len * 3.0f * 0.1f;
        my += dy / len * 3.0f * 0.1f;
    }
    FOWallRay ray = {0};
    fo_phys_ray(world->phys, px, py, mx, my, wall_report, &ray);
    return ray.hit;
}

int fo_player_can_shoot(FOWorld *world, const FOPlayer *p) {
    const FOWeapon *w = &p->weapon;
    if (!w->def || !p->alive || muzzle_in_wall(world, p)) return 0;
    // The authority does not test the ammo (alz.java:134-137).
    return world->now - w->lastShot >= w->def->interval;
}

// --- The projectiles ---------------------------------------------------------

static void bullet_off(FOBullet *b) {
    b->active = 0;
    if (b->obj.body) {
        fo_phys_body_set_active(b->obj.body, 0);
        fo_phys_body_set_transform(b->obj.body, -5, -5, 0);
    }
}

// The shape of the body of a projectile: a box of 0.1 m, the grenade of
// 0.4 x 0.2 m, or the apple of 0.2 m (all.java:104-120, ali.java:128-146).
static void bullet_shape(FOWorld *world, FOBullet *b, int shape) {
    if (b->obj.body && b->shape == shape) return;
    if (b->obj.body) fo_phys_body_free(world->phys, b->obj.body);
    b->obj.kind = FO_OBJ_BULLET;
    b->obj.w = b->obj.h = 0.1f;
    b->obj.body = fo_phys_body_new(world->phys, FO_BODY_DYNAMIC, -5, -5, 0, &b->obj);
    fo_phys_body_set_bullet(b->obj.body, 1);
    b->shape = shape;
    if (shape == 0) {
        fo_phys_body_set_fixed_rotation(b->obj.body, 1);
        FOFixtureDef def = {0.5f, 0.2f, 0, FO_CAT_BULLET, FO_CAT_GROUND, 0};
        fo_phys_box(b->obj.body, 0.05f, 0.05f, 0, 0, 0, &def);
    } else {
        // A grenade turns, slides with friction 1.2, and bounces at 0.3.
        fo_phys_body_set_fixed_rotation(b->obj.body, 0);
        FOFixtureDef def = {0.5f, 1.2f, 0.3f, FO_CAT_BULLET, FO_CAT_GROUND, 0};
        float hw = shape == 1 ? 0.2f : 0.1f;
        fo_phys_box(b->obj.body, hw, 0.1f, 0, 0, 0, &def);
        fo_phys_body_set_mass(b->obj.body, 1.0f);
    }
}

// Bullet.a(...) (xg.java): a free bullet of the pool flies from (x, y).
FOBullet *fo_combat_spawn(FOWorld *world, FOPlayer *owner, const FOWeaponDef *def, float x, float y, float vx,
                          float vy) {
    FOBullet *b = NULL;
    for (int i = 0; i < FO_MAX_BULLETS; i++) {
        if (!world->bullets[i].active) {
            b = &world->bullets[i];
            break;
        }
    }
    if (!b) return NULL;
    int shape = def->kind != FO_SHOT_GRENADE ? 0 : def->item == FO_ITEM_APPLE_LAUNCHER ? 2 : 1;
    bullet_shape(world, b, shape);
    unsigned short mask = FO_CAT_GROUND;
    mask |= owner->team == FO_TEAM_RED ? FO_CAT_BLU | FO_CAT_BLU_SENTRY : FO_CAT_RED | FO_CAT_RED_SENTRY;
    fo_phys_body_set_mask(b->obj.body, FO_CAT_BULLET, mask);
    fo_phys_body_set_gravity_scale(b->obj.body, def->gravity);
    fo_phys_body_set_active(b->obj.body, 1);
    fo_phys_body_set_transform(b->obj.body, x, y, atan2f(vy, vx));
    fo_phys_body_set_velocity(b->obj.body, vx, vy);
    fo_phys_body_set_angular_velocity(b->obj.body, 0);
    fo_phys_body_set_awake(b->obj.body, 1);
    b->active = 1;
    b->owner = owner->id;
    b->team = owner->team;
    b->def = def;
    b->spawn = world->now;
    b->damage = def->damage;
    b->angle = atan2f(vy, vx);
    b->grounded = 0;
    b->obj.dead = 0;
    // Weapon.a(float, ...): the shot sets the time and plays the sound.
    if (def->item != FO_ITEM_SENTRY) owner->weapon.lastShot = world->now;
    fo_world_event(world, FO_EVENT_SOUND, def->sound, x, y);
    world->events[world->event_count - 1].a = owner->id;
    return b;
}

static FOBullet *spawn_bullet(FOWorld *world, FOPlayer *owner, float x, float y, float vx, float vy) {
    return fo_combat_spawn(world, owner, owner->weapon.def, x, y, vx, vy);
}

// Weapon.a(xg) and the overrides: the projectile stops.
static void bullet_stop(FOWorld *world, FOBullet *b) {
    float x, y;
    fo_phys_body_position(b->obj.body, &x, &y);
    if (b->def->kind == FO_SHOT_ROCKET || b->def->kind == FO_SHOT_GRENADE ||
        b->def->item == FO_ITEM_PAPER_AIRPLANE) {
        fo_world_event(world, FO_EVENT_EFFECT, "explosion", x, y);
    }
    bullet_off(b);
}

// The splash of a grenade (all.java:64-95): 50 at the body of each enemy,
// less with the distance past the half diagonal of the target, to 0 at
// 1.5 m more. It hurts the enemy sentries too.
static void grenade_explode(FOWorld *world, FOBullet *b) {
    float gx, gy;
    fo_phys_body_position(b->obj.body, &gx, &gy);
    const float reach = 15.0f * 0.1f;
    int r = b->def->damage;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *t = &world->players[i];
        if (!t->used || t->team == b->team) continue;
        if (t->alive) {
            float diag = sqrtf(t->obj.w * t->obj.w + t->obj.h * t->obj.h);
            float tx, ty;
            fo_player_position(t, &tx, &ty);
            float dist = sqrtf((tx - gx) * (tx - gx) + (ty - gy) * (ty - gy));
            float past = dist < diag ? 0 : dist - diag;
            past = past < 0 ? 0 : past > reach ? reach : past;
            int damage = r - (int)(past * r / reach);
            if (damage > 0) fo_player_damage(world, t, b->owner, b->def->item, (float)damage);
        }
        FOSentry *s = &t->sentry;
        if (s->active) {
            float diag = sqrtf(s->obj.w * s->obj.w + s->obj.h * s->obj.h);
            float sx, sy;
            fo_phys_body_position(s->obj.body, &sx, &sy);
            float dist = sqrtf((sx - gx) * (sx - gx) + (sy - gy) * (sy - gy));
            float past = dist < diag ? 0 : dist - diag;
            past = past < 0 ? 0 : past > reach ? reach : past;
            fo_sentry_damage(world, s, b->owner, b->def->item, (float)(r - (int)(past * r / reach)));
        }
    }
    bullet_stop(world, b);
}

static FOPlayer *player_by_id(FOWorld *world, int id) {
    return fo_world_player(world, id);
}

// RocketLauncher.a(xg, Object, ...) (alr.java:78-86): a hit near the
// shooter in the air pushes the shooter up, and hurts the shooter as much.
static void rocket_jump(FOWorld *world, FOBullet *b, float hx, float hy) {
    FOPlayer *g = player_by_id(world, b->owner);
    if (!g || !g->alive) return;
    float gx, gy;
    fo_player_position(g, &gx, &gy);
    float dist = sqrtf((gx - hx) * (gx - hx) + (gy - hy) * (gy - hy));
    float push = 30.0f / dist;
    if (push > 12.0f) push = 12.0f;
    if (fabsf(gy - hy) < 4.0f && fabsf(gx - hx) < 2.0f && !fo_player_grounded(g) && gy > hy) {
        fo_player_launch(world, g, push);
        fo_player_damage(world, g, g->id, b->def->item, push);
    }
}

// Weapon.a(xg, Object, Vector2, Vector2) (alz.java:154-178).
static void bullet_hit(FOWorld *world, FOBullet *b, FOObj *other, float px, float py) {
    if (!b->active) return;
    const FOWeaponDef *def = b->def;
    if (def->kind == FO_SHOT_GRENADE) {
        // GrenadeLauncher.a(xg, ...): the ground only marks the grenade. A
        // player or a sentry sets it off.
        if (other->kind == FO_OBJ_GROUND) b->grounded = 1;
        if (other->kind == FO_OBJ_PLAYER || other->kind == FO_OBJ_SENTRY) grenade_explode(world, b);
        return;
    }
    if (other->kind == FO_OBJ_GROUND || other->kind == FO_OBJ_DOOR) {
        bullet_stop(world, b);
        if (def->wallEffect && def->kind != FO_SHOT_ROCKET) fo_world_event(world, FO_EVENT_EFFECT, "bullet_hit_wall", px, py);
    } else if (other->kind == FO_OBJ_PLAYER) {
        FOPlayer *target = (FOPlayer *)other;
        if (target->team != b->team) {
            fo_player_damage(world, target, b->owner, def->item, (float)b->damage);
            float tx, ty, bx, by;
            fo_player_position(target, &tx, &ty);
            fo_phys_body_position(b->obj.body, &bx, &by);
            if (def->kind == FO_SHOT_FLAME) {
                // Flamethrower.a(xg, xn): 5 s of fire, 10 s for the colorful one.
                long long length = def->item == FO_ITEM_COLORFUL_FLAMETHROWER ? 10000 : 5000;
                fo_player_burn(world, target, b->owner, def->item, world->now + length);
            } else {
                fo_world_event(world, FO_EVENT_EFFECT, "blood", tx, ty);
                world->events[world->event_count - 1].flip = bx < tx ? -1 : 1;
                world->events[world->event_count - 1].a = target->id;
            }
            bullet_stop(world, b);
        }
    } else if (other->kind == FO_OBJ_SENTRY) {
        FOSentry *s = (FOSentry *)other;
        if (s->team != b->team) {
            fo_sentry_damage(world, s, b->owner, def->item, (float)b->damage);
            bullet_stop(world, b);
        }
    }
    if (def->kind == FO_SHOT_ROCKET) rocket_jump(world, b, px, py);
}

// WeaponPickup.a(xn) (xx.java): the touch of a player.
static void pickup_touch(FOWorld *world, FOPickup *k, FOPlayer *p) {
    // A pickup that fades gives nothing (xx.java:66).
    if (!k->active || k->fade || !p->weapon.def) return;
    p->weapon.ammo += p->weapon.def->ammo * 0.3f;
    if (p->weapon.ammo > p->weapon.def->ammo) p->weapon.ammo = p->weapon.def->ammo;
    k->active = 0;
    fo_phys_body_set_active(k->obj.body, 0);
    fo_phys_body_set_transform(k->obj.body, -5, -5, 0);
    // The original counts a statistic for the local player here
    // (xx.java:62-64, xb.java). It plays no sound.
}

void fo_combat_contact(FOWorld *world, FOObj *a, FOObj *b, float px, float py) {
    if (a->kind == FO_OBJ_BULLET) bullet_hit(world, (FOBullet *)a, b, px, py);
    if (b->kind == FO_OBJ_BULLET) bullet_hit(world, (FOBullet *)b, a, px, py);
    if (a->kind == FO_OBJ_PICKUP && b->kind == FO_OBJ_PLAYER) pickup_touch(world, (FOPickup *)a, (FOPlayer *)b);
    if (b->kind == FO_OBJ_PICKUP && a->kind == FO_OBJ_PLAYER) pickup_touch(world, (FOPickup *)b, (FOPlayer *)a);
}

// Weapon.w() (alz.java:366-372): the death drops the weapon at the place
// where the player held it. FortOS gives the pickup a box of the size of
// the weapon sprite. The original uses a polygon around the art.
void fo_combat_drop(FOWorld *world, FOPlayer *p) {
    const FOWeaponDef *def = p->weapon.def;
    if (!def || def->w <= 0) return;
    FOPickup *k = &p->pickup;
    if (!k->obj.body) {
        k->obj.kind = FO_OBJ_PICKUP;
        k->obj.body = fo_phys_body_new(world->phys, FO_BODY_DYNAMIC, -5, -5, 0, &k->obj);
    } else {
        // A new weapon can have another size: make the body again.
        fo_phys_body_free(world->phys, k->obj.body);
        k->obj.body = fo_phys_body_new(world->phys, FO_BODY_DYNAMIC, -5, -5, 0, &k->obj);
    }
    k->obj.w = def->w;
    k->obj.h = def->h;
    FOFixtureDef box = {0.5f, 0.2f, 0, FO_CAT_PICKUP, 14, 0};
    fo_phys_box(k->obj.body, def->w / 2, def->h / 2, 0, 0, 0, &box);
    // The place of the weapon sprite (alz.java:71-94), without the bob.
    const FOArm *arm = fo_class_arm(p->cls);
    float X, Y;
    fo_player_position(p, &X, &Y);
    int s = p->facingRight ? 1 : -1;
    float ax = s > 0 ? (arm->jx + arm->h) : (arm->bodyW - arm->jx - arm->h);
    float Px = X - arm->bodyW / 2 + ax, Py = Y - p->obj.h / 2 + arm->jy + arm->ky;
    float A = p->weapon.angle;
    float phi = A * FO_DEG;
    if (!p->facingRight && A == 0) phi = 3.1415927f;
    float ox = def->hx, oy = s * def->hy;
    float cx = Px + (float)(ox * cos(phi) - oy * sin(phi)), cy = Py + (float)(oy * cos(phi) + ox * sin(phi));
    float rot = A + ((!p->facingRight && A != 0) ? 180 : 0);
    fo_phys_body_set_active(k->obj.body, 1);
    fo_phys_body_set_transform(k->obj.body, cx, cy, rot * FO_DEG);
    fo_phys_body_set_velocity(k->obj.body, 0, 0);
    fo_phys_body_set_awake(k->obj.body, 1);
    k->active = 1;
    k->def = def;
    k->team = p->team;
    k->fade = 0;
}

// Bullet.a(float) and Weapon.a(xg, float): the bullet turns to its velocity
// and stops at the end of its life.
void fo_combat_update(FOWorld *world, float dt) {
    (void)dt;
    // WeaponPickup.a(alh) (xx.java:47-61): a pickup goes 600 ms after the
    // start of its fade.
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPickup *k = &world->players[i].pickup;
        if (k->active && k->fade && world->now - k->fade >= 600) {
            k->active = 0;
            fo_phys_body_set_active(k->obj.body, 0);
            fo_phys_body_set_transform(k->obj.body, -5, -5, 0);
        }
    }
    for (int i = 0; i < FO_MAX_BULLETS; i++) {
        FOBullet *b = &world->bullets[i];
        if (!b->active) continue;
        float vx, vy, x, y;
        fo_phys_body_velocity(b->obj.body, &vx, &vy);
        fo_phys_body_position(b->obj.body, &x, &y);
        if (b->def->kind == FO_SHOT_GRENADE) {
            // A grenade turns with its body, and goes off at 90 % of its life.
            b->angle = fo_phys_body_angle(b->obj.body);
            if (world->now - b->spawn >= b->def->life * 9.0f / 10.0f) grenade_explode(world, b);
            continue;
        }
        b->angle = atan2f(vy, vx);
        fo_phys_body_set_transform(b->obj.body, x, y, b->angle);
        if (world->now - b->spawn >= b->def->life) bullet_stop(world, b);
    }
}

// --- The shot -----------------------------------------------------------------

static float spread(FOWorld *world, float amount) {
    // Utils.a(-x, x): a uniform random value.
    return -amount + fo_world_random(world) * 2 * amount;
}

// SniperRifle: the ray stops at the nearest fixture that is not of a
// teammate (alx.java).
typedef struct {
    FOWorld *world;
    short team;
    float best;
    FOObj *hit;
} FOSniperRay;

static float sniper_report(void *context, FOPhysFixture *f, float px, float py, float nx, float ny, float fraction) {
    (void)px;
    (void)py;
    (void)nx;
    (void)ny;
    FOSniperRay *r = context;
    FOObj *o = fo_world_fixture_obj(f);
    if (!o) return 1;
    if (o->kind == FO_OBJ_PLAYER && ((FOPlayer *)o)->team == r->team) return 1;
    if (o->kind == FO_OBJ_SENTRY && ((FOSentry *)o)->team == r->team) return 1;
    if (r->best > fraction) {
        r->best = fraction;
        r->hit = o;
    }
    return 1;
}

static void sniper_shot(FOWorld *world, FOPlayer *p, float mx, float my) {
    const FOWeaponDef *def = p->weapon.def;
    float a = p->weapon.angle * FO_DEG;
    spawn_bullet(world, p, mx, my, (float)cos(a) * def->speed, (float)sin(a) * def->speed);
    // The end of the ray is the point (cos, sin) x 300 of the world, not 300 m
    // from the muzzle (alw.java:89-90). FortOS keeps this.
    FOSniperRay ray = {world, p->team, 1.0f, NULL};
    fo_phys_ray(world->phys, mx, my, cosf(a) * 300.0f, sinf(a) * 300.0f, sniper_report, &ray);
    int damage = (int)(40.0f + (p->sniperCharge / 100.0f) * 110.0f);
    if (ray.hit && ray.hit->kind == FO_OBJ_PLAYER) {
        FOPlayer *t = (FOPlayer *)ray.hit;
        if (t->team != p->team) fo_player_damage(world, t, p->id, def->item, (float)damage);
    } else if (ray.hit && ray.hit->kind == FO_OBJ_SENTRY) {
        FOSentry *s = (FOSentry *)ray.hit;
        if (s->team != p->team) fo_sentry_damage(world, s, p->id, def->item, (float)damage);
    }
}

int fo_player_shoot(FOWorld *world, FOPlayer *p, float angle) {
    FOWeapon *w = &p->weapon;
    if (!w->def || !fo_player_can_shoot(world, p) || p->backstab) return 0;
    // Player.a(float, boolean) (xn.java:742-759).
    if (p->cls == FO_CLASS_MEDIC && p->uber) return 0;
    if (p->cls == FO_CLASS_MEDIC && p->healTarget) p->healTarget = 0;
    if (p->cls == FO_CLASS_SPY && fo_player_disguised(world, p)) fo_player_undisguise(world, p);
    if (p->cloaked) fo_player_set_cloak(world, p, 0);
    // Weapon.a(float, float, boolean) of the base: the time and the ammo.
    w->lastShot = world->now;
    w->ammo -= 1;
    if (w->ammo < 0) w->ammo = 0;
    float c = (float)cos(angle), s = (float)sin(angle);
    float mx, my;
    fo_player_muzzle(p, w->angle, &mx, &my);
    const FOWeaponDef *def = w->def;
    switch (def->kind) {
    case FO_SHOT_BULLET:
    case FO_SHOT_FLAME:
    case FO_SHOT_GRENADE: {
        float dx = c + spread(world, def->spread), dy = s + spread(world, def->spread);
        spawn_bullet(world, p, mx, my, def->speed * dx, def->speed * dy);
        // ColorfulFlamethrower.a(...) (alj.java:59-66) calls the shot of the
        // flamethrower, then makes a second flame with its own spread.
        if (def->item == FO_ITEM_COLORFUL_FLAMETHROWER) {
            dx = c + spread(world, def->spread);
            dy = s + spread(world, def->spread);
            spawn_bullet(world, p, mx, my, def->speed * dx, def->speed * dy);
        }
        break;
    }
    case FO_SHOT_ROCKET:
        spawn_bullet(world, p, mx, my, def->speed * c, def->speed * s);
        break;
    case FO_SHOT_HITSCAN:
        sniper_shot(world, p, mx, my);
        break;
    case FO_SHOT_KNIFE:
        break;
    }
    return 1;
}

// --- The damage and the death ------------------------------------------------

void fo_player_damage(FOWorld *world, FOPlayer *target, int attacker, int item, float damage) {
    if (!target->alive || fo_player_protected(world, target)) return;
    // The free entry of the history, or a new one (acu.java:12-30).
    FODamage *d = NULL;
    for (int i = 0; i < FO_DAMAGE_MAX; i++) {
        if (!target->damage[i].used) {
            d = &target->damage[i];
            break;
        }
    }
    if (!d) {
        // The list is full. The oldest entry goes.
        d = &target->damage[0];
        for (int i = 1; i < FO_DAMAGE_MAX; i++) {
            if (target->damage[i].time < d->time) d = &target->damage[i];
        }
    }
    d->time = world->now;
    d->damage = damage;
    d->attacker = attacker;
    d->item = item;
    d->used = 1;
    fo_world_event(world, FO_EVENT_HIT, NULL, 0, 0);
    FOEvent *e = &world->events[world->event_count - 1];
    e->a = attacker;
    e->b = target->id;
    e->value = damage;
    fo_player_set_health(world, target, target->health - damage);
}

// KillEventData.generateKillEventDataFromDamageHistory: the killer is the
// player with the most damage in the last 120 s. The assister is the
// second, or the medic that heals the killer. Returns the killer, or the
// victim when no damage is in the history.
static int credit(FOWorld *world, const FODamage *history, int victim, int *assister, int *item) {
    int ids[FO_DAMAGE_MAX], items[FO_DAMAGE_MAX], n = 0;
    float sums[FO_DAMAGE_MAX];
    for (int i = 0; i < FO_DAMAGE_MAX; i++) {
        const FODamage *d = &history[i];
        if (!d->used || world->now - d->time >= 120000) continue;
        int k = 0;
        while (k < n && ids[k] != d->attacker) k++;
        if (k == n) {
            ids[n] = d->attacker;
            items[n] = d->item;
            sums[n] = 0;
            n++;
        }
        sums[k] += d->damage;
    }
    int best = -1, second = -1;
    for (int k = 0; k < n; k++) {
        if (!player_by_id(world, ids[k])) continue;
        if (best < 0 || sums[k] > sums[best] || (sums[k] == sums[best] && ids[k] < ids[best])) {
            second = best;
            best = k;
        } else if (second < 0 || sums[k] > sums[second] || (sums[k] == sums[second] && ids[k] < ids[second])) {
            second = k;
        }
    }
    *assister = 0;
    *item = 0;
    if (best < 0) return victim;
    *item = items[best];
    *assister = second >= 0 ? ids[second] : 0;
    FOPlayer *k = player_by_id(world, ids[best]);
    if (k) {
        int medic = fo_player_healer(world, k);
        if (medic) *assister = medic;
    }
    return ids[best];
}

static void kill_event(FOWorld *world, int killer, int victim, int assister, int item, int sentry, float x, float y) {
    fo_world_event(world, FO_EVENT_KILL, sentry ? "sentry" : NULL, x, y);
    FOEvent *e = &world->events[world->event_count - 1];
    e->a = killer;
    e->b = victim;
    e->c = assister;
    e->item = item;
}

void fo_combat_credit(FOWorld *world, const FODamage *history, int victim, int sentry) {
    int assister, item;
    int killer = credit(world, history, victim, &assister, &item);
    if (killer != victim) {
        FOPlayer *k = player_by_id(world, killer);
        if (k) k->score += 2;
        FOPlayer *a = assister ? player_by_id(world, assister) : NULL;
        if (a) a->score += 1;
    }
    kill_event(world, killer, victim, assister, item, sentry, 0, 0);
}

void fo_combat_kill(FOWorld *world, FOPlayer *victim) {
    int assister, item;
    int killer = credit(world, victim->damage, victim->id, &assister, &item);
    // The points (xn.java:788-800): 2 for the kill, 1 more for a spy that
    // hides, 1 more for the holder of the objective. With no damage in the
    // history, KillEventData names the victim as the killer
    // (KillEventData.java:69-72), so the victim takes the points.
    FOPlayer *k = player_by_id(world, killer);
    if (k) {
        k->score += 2;
        if (victim->cls == FO_CLASS_SPY && (victim->cloaked || victim->disguise.active)) k->score += 1;
        if (fo_world_holds_objective(world, victim)) k->score += 1;
        victim->killer = killer;
    }
    FOPlayer *a = assister ? player_by_id(world, assister) : NULL;
    if (a) a->score += 1;
    float x, y;
    fo_player_position(victim, &x, &y);
    kill_event(world, killer, victim->id, assister, item, 0, x, y);
    fo_world_event(world, FO_EVENT_SOUND, "death1", x, y);
}

// Player.c(boolean) (xn.java:1027-1042): the history resets at a spawn.
void fo_combat_reset(FOPlayer *p) {
    memset(p->damage, 0, sizeof(p->damage));
}

// The weapon update of each frame: the charge of the sniper rifle
// (alw.java:63-72), which rises 20 each second while it aims.
void fo_combat_weapon_update(FOPlayer *p, float dt) {
    if (!p->weapon.def || p->weapon.def->kind != FO_SHOT_HITSCAN) return;
    if (p->weapon.angle == 0) {
        p->sniperCharge = 0;
    } else {
        p->sniperCharge += 20.0f * dt;
    }
    if (p->sniperCharge < 0) p->sniperCharge = 0;
    if (p->sniperCharge > 100) p->sniperCharge = 100;
}
