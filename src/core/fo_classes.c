// The class abilities of the world: the burn of the torcher, the sentry
// of the builder, the heal and the uber of the dealer, and the cloak, the
// disguise, and the backstab of the agent (xn.java:257-397, ws.java,
// xu.java).

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "fo_world.h"
#include "fo_world_private.h"

// --- The small tests -----------------------------------------------------------

int fo_player_protected(const FOWorld *world, const FOPlayer *p) {
    if (p->uber) return 1;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *m = &world->players[i];
        if (m->used && m->healTarget == p->id && m->uber) return 1;
    }
    return 0;
}

int fo_player_healer(const FOWorld *world, const FOPlayer *p) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *m = &world->players[i];
        if (m->used && m->healTarget == p->id) return m->id;
    }
    return 0;
}

int fo_player_disguised(const FOWorld *world, const FOPlayer *p) {
    return p->disguise.active && (world->now - p->disguise.start) / 1000.0f - 0.5f >= 0.9f;
}

static float distance(const FOPlayer *a, const FOPlayer *b) {
    float ax, ay, bx, by;
    fo_player_position(a, &ax, &ay);
    fo_player_position(b, &bx, &by);
    return sqrtf((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}

// --- The cloak and the disguise ------------------------------------------------

// Player.e(boolean) (xn.java:1066-1075). The capture the flag mode does
// not let the flag carrier cloak; fo_world_mode_allows tells it.
void fo_player_set_cloak(FOWorld *world, FOPlayer *p, int on) {
    if (p->cloaked == on) return;
    if (on && p->backstab) return;
    if (on && !fo_world_mode_allows(world, p)) return;
    p->cloaked = on;
    p->cloakTime = world->now;
}

// Player.a(xn) (xn.java:1099-1159): the disguise. NULL ends it.
static void set_disguise(FOWorld *world, FOPlayer *p, const FOPlayer *target) {
    if (p->backstab || !fo_world_mode_allows(world, p)) return;
    if (!target) {
        p->disguise.active = 0;
        return;
    }
    if (p->disguise.active && p->disguise.target == target->id) return;
    if (target->cls < 1 || target->cls > 9 || target->team == FO_TEAM_NONE) return;
    if (p->cloaked) fo_player_set_cloak(world, p, 0);
    FODisguise *d = &p->disguise;
    d->active = 1;
    d->target = target->id;
    d->cls = target->cls;
    d->team = target->team;
    snprintf(d->name, sizeof(d->name), "%s", target->name);
    d->start = world->now;
}

void fo_player_disguise(FOWorld *world, FOPlayer *p, int exclude) {
    if (!p->alive || p->cls != FO_CLASS_SPY || p->backstab || p->cloaked) return;
    const FOPlayer *choices[FO_MAX_PLAYERS];
    int n = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *e = &world->players[i];
        if (e->used && e->team != p->team && e->team != FO_TEAM_NONE && e->cls != 0 && e->id != exclude) {
            choices[n++] = e;
        }
    }
    if (n == 0) return;
    int pick = (int)(fo_world_random(world) * n);
    if (pick >= n) pick = n - 1;
    set_disguise(world, p, choices[pick]);
}

void fo_player_undisguise(FOWorld *world, FOPlayer *p) {
    if (p->alive && p->cls == FO_CLASS_SPY) set_disguise(world, p, NULL);
}

// --- The backstab ----------------------------------------------------------------

// Player.b(boolean, boolean) (xn.java:1081-1097).
static void set_backstab(FOWorld *world, FOPlayer *p, int on, int facing) {
    if (p->backstab == on) return;
    if (!on) {
        p->backstab = 0;
        p->backstabTime = 0;
        return;
    }
    if (p->cls == FO_CLASS_SPY && fo_player_disguised(world, p)) set_disguise(world, p, NULL);
    if (p->cls != FO_CLASS_SPY || !p->disguise.active || fo_player_disguised(world, p)) {
        p->backstab = 1;
        p->backstabTime = world->now;
        p->backstabHit = 0;
        fo_player_set_cloak(world, p, 0);
        p->backstabFacing = facing;
    }
}

void fo_player_backstab(FOWorld *world, FOPlayer *p) {
    if (p->alive && p->cls == FO_CLASS_SPY) set_backstab(world, p, 1, p->facingRight);
}

// The vertical test of a backstab (xn.java:325-332): the same height, up
// to 1.5 m lower, or up to 1.8 m higher.
static int stab_height(float y, float ty) {
    if (y == ty) return 1;
    if (y > ty && y - ty < 1.5f) return 1;
    return y < ty && ty - y < 1.8000001f;
}

static void backstab_update(FOWorld *world, FOPlayer *p) {
    float secs = (world->now - p->backstabTime) / 1000.0f;
    if (!p->backstabHit && secs >= 0.628f) {
        float x, y;
        fo_player_position(p, &x, &y);
        int facing = p->backstabFacing;
        int sound = 0;
        const FOWeaponDef *knife = fo_weapon_def(FO_ITEM_KNIFE);
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            FOPlayer *t = &world->players[i];
            if (!t->used || t->id == p->id || t->team == p->team || fo_player_protected(world, t)) continue;
            float tx, ty;
            fo_player_position(t, &tx, &ty);
            if (fabsf(x - tx) < 1.8000001f && (tx <= x || facing) && (tx >= x || !facing) && stab_height(y, ty)) {
                fo_player_damage(world, t, p->id, FO_ITEM_KNIFE, (float)knife->damage);
                if (!sound) {
                    fo_world_event(world, FO_EVENT_SOUND, "backstab", tx, ty);
                    sound = 1;
                }
            }
        }
        // The sentries: the original does not test the team here.
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            FOSentry *s = &world->players[i].sentry;
            if (!s->active) continue;
            float sx, sy;
            fo_phys_body_position(s->obj.body, &sx, &sy);
            if (fabsf(x - sx) < 1.8000001f && (sx <= x || facing) && (sx >= x || !facing) && stab_height(y, sy)) {
                fo_sentry_damage(world, s, p->id, FO_ITEM_KNIFE, 200);
            }
        }
        p->backstabHit = 1;
    }
    if (secs >= 1.1f) set_backstab(world, p, 0, p->facingRight);
}

// --- The burn ------------------------------------------------------------------

// Player.a(alz, long) (xn.java:1044-1050): a flame sets the fire.
void fo_player_burn(FOWorld *world, FOPlayer *p, int attacker, int item, long long until) {
    if (fo_player_protected(world, p)) return;
    p->burning = 1;
    p->burnUntil = until;
    p->burnAttacker = attacker;
    p->burnItem = item;
}

// --- The sentry ----------------------------------------------------------------

// Player.b(Vector2, boolean): a new sentry at the place of the builder.
static void build_sentry(FOWorld *world, FOPlayer *p) {
    FOSentry *s = &p->sentry;
    if (s->obj.body) fo_phys_body_free(world->phys, s->obj.body);
    memset(s, 0, sizeof(*s));
    float x, y;
    fo_player_position(p, &x, &y);
    s->obj.kind = FO_OBJ_SENTRY;
    s->obj.w = 1.1f;
    s->obj.h = 1.4f;
    s->owner = p->id;
    s->team = p->team;
    s->facingRight = p->facingRight;
    s->aim = s->facingRight ? 0 : 3.1415927f;
    s->want = s->aim;
    s->health = 200;
    s->obj.body = fo_phys_body_new(world->phys, FO_BODY_DYNAMIC, x, y, s->aim, &s->obj);
    fo_phys_body_set_fixed_rotation(s->obj.body, 1);
    FOFixtureDef def = {0, 5.0f, 0, p->team == FO_TEAM_RED ? FO_CAT_RED_SENTRY : FO_CAT_BLU_SENTRY, 1555, 0};
    fo_phys_box(s->obj.body, s->obj.w / 2, s->obj.h / 2, 0, 0, 0, &def);
    // The angle of the body turns the box. The original sets it to 0 at
    // once (f(0.0f)).
    fo_phys_body_set_transform(s->obj.body, x, y, 0);
    s->active = 1;
    p->steel = 0;
}

void fo_sentry_remove(FOWorld *world, FOSentry *s) {
    if (!s->active) return;
    float x, y;
    fo_phys_body_position(s->obj.body, &x, &y);
    fo_world_event(world, FO_EVENT_EFFECT, s->team == FO_TEAM_RED ? "red_sentry_destroy" : "blu_sentry_destroy",
                   x + (s->facingRight ? 0.2f : -0.2f), y + 0.5f);
    world->events[world->event_count - 1].flip = s->facingRight ? 1 : -1;
    fo_world_event(world, FO_EVENT_SOUND, "explosion", x, y);
    s->active = 0;
    fo_phys_body_set_active(s->obj.body, 0);
}

// SentryGun.a(alz, int) (xu.java): the damage, and the kill credit at 0.
void fo_sentry_damage(FOWorld *world, FOSentry *s, int attacker, int item, float damage) {
    if (!s->active) return;
    FODamage *d = &s->damage[0];
    for (int i = 0; i < FO_DAMAGE_MAX; i++) {
        if (!s->damage[i].used) {
            d = &s->damage[i];
            break;
        }
        if (s->damage[i].time < d->time) d = &s->damage[i];
    }
    d->time = world->now;
    d->damage = damage;
    d->attacker = attacker;
    d->item = item;
    d->used = 1;
    s->health -= damage;
    if (s->health <= 0) {
        fo_combat_credit(world, s->damage, s->owner, 1);
        fo_sentry_remove(world, s);
    }
}

void fo_sentry_land(FOWorld *world, FOSentry *s) {
    (void)world;
    s->landed = 1;
}

typedef struct {
    int blocked;
} FOWallTest;

static float wall_only(void *context, FOPhysFixture *f, float px, float py, float nx, float ny, float fraction) {
    (void)px;
    (void)py;
    (void)nx;
    (void)ny;
    (void)fraction;
    FOObj *o = fo_world_fixture_obj(f);
    if (!o || (o->kind != FO_OBJ_GROUND && o->kind != FO_OBJ_DOOR)) return 1;
    ((FOWallTest *)context)->blocked = 1;
    return 0;
}

static int clear_line(FOWorld *world, float x1, float y1, float x2, float y2) {
    FOWallTest t = {0};
    fo_phys_ray(world->phys, x1, y1, x2, y2, wall_only, &t);
    return !t.blocked;
}

// SentryGun.A(): the build is done 0.8 s after it starts.
int fo_sentry_ready(const FOWorld *world, const FOSentry *s) {
    return s->built != 0 && world->now - s->built >= 800;
}

static void sentry_update(FOWorld *world, FOSentry *s, float dt) {
    float sx, sy, vx, vy;
    fo_phys_body_position(s->obj.body, &sx, &sy);
    fo_phys_body_velocity(s->obj.body, &vx, &vy);
    if (!fo_sentry_ready(world, s)) {
        if (s->landed && vy >= 0 && s->built == 0) {
            s->built = world->now;
            fo_world_event(world, FO_EVENT_SOUND, "sentry_built", sx, sy);
            // B(): the enemies now collide with it, and it stays still.
            unsigned short cat = s->team == FO_TEAM_RED ? FO_CAT_RED_SENTRY : FO_CAT_BLU_SENTRY;
            fo_phys_body_set_mask(s->obj.body, cat, 1555 | (s->team == FO_TEAM_RED ? FO_CAT_BLU : FO_CAT_RED));
            fo_phys_body_set_type(s->obj.body, FO_BODY_STATIC);
        }
        return;
    }
    int before = s->target;
    if (s->target) {
        FOPlayer *t = fo_world_player(world, s->target);
        float tx, ty;
        if (!t || !t->alive) {
            s->target = 0;
        } else {
            fo_player_position(t, &tx, &ty);
            if (sqrtf((tx - sx) * (tx - sx) + (ty - sy) * (ty - sy)) >= 10.5f || !clear_line(world, sx, sy, tx, ty)) {
                s->target = 0;
            }
        }
    }
    if (!s->target) {
        // The first enemy in the list that is near, as the original: it
        // stops at the first one in range, even when a wall hides it.
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            FOPlayer *t = &world->players[i];
            if (!t->used || !t->alive || t->team == s->team) continue;
            if (t->cls == FO_CLASS_SPY && (t->disguise.active || t->cloaked)) continue;
            float tx, ty;
            fo_player_position(t, &tx, &ty);
            if (sqrtf((tx - sx) * (tx - sx) + (ty - sy) * (ty - sy)) < 10.5f) {
                if (clear_line(world, sx, sy, tx, ty)) s->target = t->id;
                break;
            }
        }
    }
    if (s->target) {
        FOPlayer *t = fo_world_player(world, s->target);
        float tx, ty;
        fo_player_position(t, &tx, &ty);
        s->want = (float)atan2(-(tx - sx), ty - sy) + 1.5707964f;
    } else {
        s->want = s->facingRight ? 0 : 3.1415927f;
    }
    // The turret turns at 3.5 rad/s.
    if (s->aim > s->want) {
        s->aim -= dt * 3.5f;
        if (s->aim < s->want) s->aim = s->want;
    } else {
        s->aim += dt * 3.5f;
        if (s->aim > s->want) s->aim = s->want;
    }
    const FOWeaponDef *gun = fo_weapon_def(FO_ITEM_SENTRY);
    if (s->target && world->now - s->lastShot >= gun->interval && fabsf(s->want - s->aim) < 0.15f) {
        // SentryGunWeapon.a(float, boolean) and b(float): the muzzle is 1 m
        // along the turret from its pivot.
        float px = (sx - 1.7f / 2) + 0.4f + 0.5f, py = ((sy - 1.2f / 2) + 0.6f - 0.2f) + 0.6f;
        float c = cosf(s->aim), sn = sinf(s->aim);
        float mx = px + gun->ix * c - gun->iy * sn, my = py + gun->ix * sn + gun->iy * c;
        FOPlayer *owner = fo_world_player(world, s->owner);
        if (owner) fo_combat_spawn(world, owner, gun, mx, my, c * gun->speed, sn * gun->speed);
        s->lastShot = world->now;
    }
    if (before != s->target && s->target && world->now - s->alerted > 1000) {
        fo_world_event(world, FO_EVENT_SOUND, "sentry_alerted", sx, sy);
        s->alerted = world->now;
    }
}

// --- The actions ---------------------------------------------------------------

int fo_sentry_is_ready(const FOWorld *world, const FOSentry *s) {
    return fo_sentry_ready(world, s);
}

int fo_world_in_area(const FOWorld *world, const FOPlayer *p, FOAreaKind kind) {
    float x, y;
    fo_player_position(p, &x, &y);
    for (int i = 0; i < world->map.area_count; i++) {
        if (world->map.areas[i].kind == kind && fo_rect_contains(&world->map.areas[i].rect, x, y)) return 1;
    }
    return 0;
}

void fo_player_ability(FOWorld *world, FOPlayer *p) {
    if (!p->alive) return;
    if (p->cls == FO_CLASS_SPY) {
        if (p->cloaked) {
            fo_player_set_cloak(world, p, 0);
        } else if (!fo_player_protected(world, p)) {
            fo_player_set_cloak(world, p, 1);
        }
    } else if (p->cls == FO_CLASS_MEDIC) {
        if (p->uberCharge >= 20.0f && !p->uber) {
            p->uber = 1;
            p->score += 2;
            float x, y;
            fo_player_position(p, &x, &y);
            fo_world_sound(world, "medic_uber_begin", x, y, p->id);
        }
    } else if (p->cls == FO_CLASS_ENGINEER) {
        if (!p->sentry.active && p->steel >= 100.0f && !fo_world_in_area(world, p, FO_AREA_NO_SENTRY)) {
            build_sentry(world, p);
        } else if (p->sentry.active) {
            fo_sentry_remove(world, &p->sentry);
        }
    }
}

void fo_player_heal(FOWorld *world, FOPlayer *p, int target) {
    (void)world;
    if (!p->alive || p->cls != FO_CLASS_MEDIC || target == p->id) return;
    p->healTarget = target;
}

static void end_uber(FOWorld *world, FOPlayer *p) {
    if (!p->uber) return;
    p->uber = 0;
    float x, y;
    fo_player_position(p, &x, &y);
    fo_world_sound(world, "medic_uber_end", x, y, p->id);
}

// --- The update of each class -----------------------------------------------------

typedef struct {
    int blocked;
} FOHealRay;

void fo_classes_update(FOWorld *world, FOPlayer *p, float dt) {
    if (p->cls == FO_CLASS_MEDIC) {
        FOPlayer *t = p->healTarget ? fo_world_player(world, p->healTarget) : NULL;
        if (p->uber) {
            p->uberCharge -= 3.0f * dt;
            if (p->uberCharge <= 0) {
                p->uberCharge = 0;
                end_uber(world, p);
            }
            if (p->healTarget && t) fo_player_set_health(world, t, fo_class_health(t->cls));
        }
        if (p->healTarget) {
            if (!t || !t->alive) {
                p->healTarget = 0;
            } else if ((t->cls == FO_CLASS_SPY && t->cloaked) || distance(p, t) > 8.0f) {
                p->healTarget = 0;
            } else {
                float px, py, tx, ty;
                fo_player_position(p, &px, &py);
                fo_player_position(t, &tx, &ty);
                if (!clear_line(world, px, py, tx, ty)) {
                    p->healTarget = 0;
                } else {
                    p->uberCharge += dt;
                    if (p->uberCharge > 20.0f) p->uberCharge = 20.0f;
                }
            }
        }
    } else if (p->cls == FO_CLASS_ENGINEER) {
        p->steel += 2.5f * dt;
        if (p->steel > 100.0f) p->steel = 100.0f;
    } else if (p->cls == FO_CLASS_SPY) {
        if (p->cloaked) {
            p->cloakEnergy -= 10.0f * dt;
            if (p->cloakEnergy <= 0) {
                p->cloakEnergy = 0;
                fo_player_set_cloak(world, p, 0);
            }
        } else {
            p->cloakEnergy += 15.0f * dt;
            if (p->cloakEnergy > 100.0f) p->cloakEnergy = 100.0f;
        }
        if (p->backstab) backstab_update(world, p);
    }
    // A player that a medic heals gets 34 health each second (xn.java:369-371).
    if (fo_player_healer(world, p) && !fo_player_waiting(p)) {
        float max = fo_class_health(p->cls);
        float h = p->health + 34.0f * dt;
        fo_player_set_health(world, p, h > max ? max : h < 0 ? 0 : h);
    }
    // The points of a medic: 1 for each 400 health of healing time.
    if (p->healTarget) {
        FOPlayer *t = fo_world_player(world, p->healTarget);
        if (!t || !t->alive) {
            p->healTarget = 0;
        } else {
            p->healScore += 34.0f * dt;
            if (p->healScore >= 400.0f) {
                p->healScore = 0;
                p->score += 1;
            }
        }
    }
}

// The rest of Player.a(float) (xn.java:384-397): the fridge stops the fire,
// the fire ends at its time, and the fire hurts 5 each second.
void fo_classes_after(FOWorld *world, FOPlayer *p, float dt, int inFridge) {
    if (inFridge) {
        p->steel = 100.0f;
        p->cloakEnergy = 100.0f;
        p->burning = 0;
    }
    if (world->now > p->burnUntil) p->burning = 0;
    if (p->burning && !fo_player_waiting(p)) {
        fo_player_damage(world, p, p->burnAttacker, p->burnItem, 5.0f * dt);
    }
}

void fo_classes_sentries(FOWorld *world, float dt) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOSentry *s = &world->players[i].sentry;
        if (s->active) sentry_update(world, s, dt);
    }
}

// Player.c(boolean) (xn.java:1027-1042): a spawn resets the abilities.
// The sentry stays when the builder dies (keep is 1), and goes on a team
// change or a new round.
void fo_classes_reset(FOWorld *world, FOPlayer *p, int keepSentry) {
    p->cloaked = 0;
    p->uberCharge = 0;
    if (p->healTarget) p->healTarget = 0;
    p->burning = 0;
    if (!keepSentry) fo_sentry_remove(world, &p->sentry);
    p->steel = 100.0f;
    p->cloakEnergy = 100.0f;
    p->disguise.active = 0;
    p->backstab = 0;
    p->uber = 0;
}

// Player.v() (xn.java:814-817): the death.
void fo_classes_death(FOWorld *world, FOPlayer *p) {
    p->uberCharge = 0;
    p->cloakEnergy = 100.0f;
    p->steel = 100.0f;
    p->healTarget = 0;
    end_uber(world, p);
    fo_classes_reset(world, p, 1);
}

int fo_world_in_own_spawn(const FOWorld *world, const FOPlayer *p) {
    if (p->team == FO_TEAM_BLU) return fo_world_in_area(world, p, FO_AREA_SPAWN_BLU);
    if (p->team == FO_TEAM_RED) return fo_world_in_area(world, p, FO_AREA_SPAWN_RED);
    return 0;
}
