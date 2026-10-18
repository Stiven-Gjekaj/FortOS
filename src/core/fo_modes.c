// fo_modes: the game modes of the maps and the round flow of the local
// server. King of the hill is KothMapGameMode (acq.java). The bomb is
// BombMapGameMode (acl.java), RollableBomb (act.java), and BombCheckpoint
// (xf.java). Capture the flag is CtfMapGamemode (acn.java) and CtfFlag
// (acm.java). The rules are in reference/notes/modes-maps.md.

#include <math.h>
#include <string.h>

#include "fo_world_private.h"

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static void obj_position(const FOObj *o, float *x, float *y) {
    fo_phys_body_position(o->body, x, y);
}

static FOPlayer *player_by_id(FOWorld *world, int id) {
    if (id < 0) return NULL;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used && world->players[i].id == id) return &world->players[i];
    }
    return NULL;
}

static const FOPlayer *local_player(const FOWorld *world) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used && world->players[i].id == world->localId) return &world->players[i];
    }
    return NULL;
}

// A cloaked or disguised spy does not count on an objective
// (xn.w() != 9 || (!xn.G() && xn.I() == null)).
static int hidden_spy(const FOPlayer *p) {
    return p->cls == FO_CLASS_SPY && (p->cloaked || p->disguise.active);
}

static int in_area(const FOWorld *world, FOAreaKind kind, float x, float y) {
    const FORect *r = fo_tmx_area(&world->map, kind);
    return r && fo_rect_contains(r, x, y);
}

static int player_in_area(const FOWorld *world, const FOPlayer *p, FOAreaKind kind) {
    float x, y;
    fo_player_position(p, &x, &y);
    return in_area(world, kind, x, y);
}

static int in_death_area(const FOWorld *world, float x, float y) {
    for (int a = 0; a < world->map.area_count; a++) {
        if (world->map.areas[a].kind == FO_AREA_DEATH && fo_rect_contains(&world->map.areas[a].rect, x, y)) return 1;
    }
    return 0;
}

// --- King of the hill -----------------------------------------------------

// KothMapGameMode.a(int): the speed of the capture by the advantage.
static float koth_rate(int n) {
    if (n == 1) return 7.142857f;
    if (n == 2) return 10.752688f;
    if (n == 3) return 13.157895f;
    if (n >= 4) return 14.925374f;
    return 0;
}

static void koth_count(const FOWorld *world, int spies, int *red, int *blu) {
    *red = *blu = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *p = &world->players[i];
        if (!p->used || !p->alive) continue;
        if (!spies && hidden_spy(p)) continue;
        if (!player_in_area(world, p, FO_AREA_KOTH)) continue;
        if (p->team == FO_TEAM_RED) (*red)++;
        else if (p->team == FO_TEAM_BLU) (*blu)++;
    }
}

// KothMapGameMode.a(float) (acq.java:24-121).
static void koth_update(FOWorld *world, float dt) {
    FOMode *m = &world->mode;
    int red, blu;
    koth_count(world, 0, &red, &blu);
    if (m->owner == FO_TEAM_BLU) m->bluTimer -= dt;
    else if (m->owner == FO_TEAM_RED) m->redTimer -= dt;
    if (m->bluTimer < 0) m->bluTimer = 0;
    if (m->redTimer < 0) m->redTimer = 0;
    int wasOver = m->wasOver;
    if (m->redTimer > 0 && m->bluTimer > 0) {
        if (m->owner == 0) {
            if (red > blu) m->progress += koth_rate(red - blu) * dt;
            else if (blu > red) m->progress -= koth_rate(blu - red) * dt;
        } else if (m->owner == FO_TEAM_BLU) {
            if (red > blu) m->progress += koth_rate(red - blu) * dt;
        } else if (m->owner == FO_TEAM_RED && blu > red) {
            m->progress -= koth_rate(blu - red) * dt;
        }
        if (m->owner == FO_TEAM_BLU && m->progress > -100 && (blu > red || red == 0)) {
            m->progress -= 3.0f * dt;
        } else if (m->owner == FO_TEAM_RED && m->progress < 100 && (red > blu || blu == 0)) {
            m->progress += 3.0f * dt;
        }
        short capped = 0;
        if (m->owner == FO_TEAM_BLU && m->progress >= 0) {
            m->progress = 100;
            m->owner = capped = FO_TEAM_RED;
        } else if (m->owner == FO_TEAM_RED && m->progress <= 0) {
            m->progress = -100;
            m->owner = capped = FO_TEAM_BLU;
        } else if (m->owner == 0 && m->progress >= 100) {
            m->owner = capped = FO_TEAM_RED;
        } else if (m->owner == 0 && m->progress <= -100) {
            m->owner = capped = FO_TEAM_BLU;
        }
        // Each player of the team that takes the point and stands on it
        // gets 5 points. This loop counts the spies too, as the original.
        if (capped) {
            for (int i = 0; i < FO_MAX_PLAYERS; i++) {
                FOPlayer *p = &world->players[i];
                if (p->used && p->alive && p->team == capped && player_in_area(world, p, FO_AREA_KOTH)) p->score += 5;
            }
        }
    }
    if (m->owner != m->lastOwner) {
        if (m->lastOwner != -1 && (!wasOver || fo_mode_over(world))) {
            fo_world_event(world, FO_EVENT_SOUND, "cp_owner_changed", 0, 0);
        }
        m->lastOwner = m->owner;
    }
    m->wasOver = fo_mode_over(world);
}

// KothMapGameMode.d(xn): the player stands on the point of the other team,
// and the team of the player has more players on it.
static int koth_holds(const FOWorld *world, const FOPlayer *p) {
    int red, blu;
    koth_count(world, 1, &red, &blu);
    if (!player_in_area(world, p, FO_AREA_KOTH)) return 0;
    if (p->team == FO_TEAM_RED && world->mode.owner == FO_TEAM_BLU && red > blu) return 1;
    return p->team == FO_TEAM_BLU && world->mode.owner == FO_TEAM_RED && blu > red;
}

// --- The bomb -------------------------------------------------------------

// RollableBomb.c(xn) (act.java:257-259): the player pushes the bomb.
static int bomb_pusher(const FOWorld *world, const FOPlayer *p) {
    if (!p || !p->used || !p->alive || hidden_spy(p)) return 0;
    float bx, by, x, y;
    obj_position(&world->mode.bomb.obj, &bx, &by);
    fo_player_position(p, &x, &y);
    return fabsf(x - bx) < 2.2f && fabsf(y - by) < 1.5f;
}

static int list_has(const int *list, int count, int id) {
    for (int i = 0; i < count; i++) {
        if (list[i] == id) return 1;
    }
    return 0;
}

// Keeps the pushers that still push, in their order.
static void list_purge(FOWorld *world, int *list, int *count) {
    int n = 0;
    for (int i = 0; i < *count; i++) {
        if (bomb_pusher(world, player_by_id(world, list[i]))) list[n++] = list[i];
    }
    *count = n;
}

// RollableBomb.c(int): the speed by the number of pushers.
static float bomb_speed(int n) {
    if (n == 2) return 1.3f;
    if (n == 3) return 1.4f;
    if (n >= 4) return 1.5f;
    return 1.0f;
}

// RollableBomb.u() and v() (act.java:356-373): the ground under the bomb
// has a slope, and the slope goes up by more than one degree.
static void bomb_slopes(const FOBomb *b, int *sloped, int *up) {
    float last = 0;
    *sloped = 0;
    for (int i = 0; i < b->slopeCount; i++) {
        float a = fo_angle_norm(fo_phys_body_angle(b->slopes[i]->body));
        if (a != 0) {
            *sloped = 1;
            last = a;
        }
    }
    *up = fo_angle_norm(last) > 0.017453292f;
}

// BombMapGameMode.f() (acl.java:90-96): the big explosion at the end.
static void bomb_blast(FOWorld *world) {
    FOMode *m = &world->mode;
    m->blastTime = world->now;
    fo_world_event(world, FO_EVENT_EFFECT, "hugeExplosion", m->blastX, m->blastY);
    fo_world_event(world, FO_EVENT_SOUND, "cartpush_explosion", m->blastX, m->blastY);
}

// RollableBomb.a(float) (act.java:121-255), the part of the server.
static void bomb_update(FOWorld *world, float dt) {
    FOMode *m = &world->mode;
    FOBomb *b = &m->bomb;
    list_purge(world, b->red, &b->redCount);
    list_purge(world, b->blu, &b->bluCount);
    int localPushes = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *p = &world->players[i];
        if (!bomb_pusher(world, p)) continue;
        if (p->team == FO_TEAM_RED && !list_has(b->red, b->redCount, p->id)) b->red[b->redCount++] = p->id;
        else if (p->team == FO_TEAM_BLU && !list_has(b->blu, b->bluCount, p->id)) b->blu[b->bluCount++] = p->id;
        if (p->id == world->localId) localPushes = 1;
    }
    int count = 0;
    float dir = 0;
    short team = 0;
    if (b->redCount > b->bluCount) {
        dir = b->forward ? 1.0f : -1.0f;
        b->redPush = world->now;
        count = b->redCount;
        team = FO_TEAM_RED;
    } else if (b->bluCount > b->redCount && world->now - b->redPush >= 7000) {
        dir = b->forward ? -1.0f : 1.0f;
        count = b->bluCount;
        team = FO_TEAM_BLU;
    }
    float factor;
    int hint = 0;
    int sloped, up;
    bomb_slopes(b, &sloped, &up);
    if (sloped) {
        float f = 0;
        int needsTwo = 0;
        if (up && dir > 0) f = 1.2f;
        if (up && dir < 0) {
            f = 8.5f;
            needsTwo = 1;
        }
        if (!up && dir > 0) {
            f = 8.5f;
            needsTwo = 1;
        }
        if (!up && dir < 0) f = 1.2f;
        if (needsTwo && count < 2) {
            const FOPlayer *local = local_player(world);
            f = 0;
            if (local && team == local->team) hint = 1;
        }
        factor = f;
    } else {
        factor = 1.0f;
    }
    if (hint) b->hintTime = world->now;
    if (world->now - b->hintTime < 1000) hint = 1;
    b->hint = localPushes && hint;
    b->target = dir * bomb_speed(count) * factor;
    float vx, vy;
    fo_phys_body_velocity(b->obj.body, &vx, &vy);
    if (fo_mode_bomb_down(world) || fo_mode_over(world)) {
        vx = 0;
    } else {
        vx = clampf(lerpf(vx, b->target, 1.0f * dt), -1.5f, 1.5f);
    }
    fo_phys_body_set_velocity(b->obj.body, vx, vy);
    float x, y;
    obj_position(&b->obj, &x, &y);
    if (dt > 0) fo_phys_body_set_angular_velocity(b->obj.body, -(x - b->lastX) / dt);
    b->lastX = x;
    if (!in_area(world, FO_AREA_BOMB_FINISH, x, y)) {
        b->finished = 0;
    } else if (b->finished == 0) {
        b->finished = world->now;
        FOEvent *e = NULL;
        fo_world_event(world, FO_EVENT_SHAKE, "", x, y);
        if (world->event_count > 0) e = &world->events[world->event_count - 1];
        if (e && e->kind == FO_EVENT_SHAKE) {
            e->item = 3000;
            e->value = 1.5f;
        }
        bomb_blast(world);
    }
}

// BombCheckpoint.a(float) (xf.java:56-64): the gate closes behind the bomb.
static void checkpoint_set(FOWorld *world, FOCheckpoint *c, int active) {
    fo_phys_body_set_active(c->obj.body, active);
    c->active = active;
    c->changed = world->now;
}

static void checkpoint_update(FOWorld *world, FOCheckpoint *c) {
    float bx, by;
    obj_position(&world->mode.bomb.obj, &bx, &by);
    if (bx > c->x && bx - c->x >= c->obj.w / 2 + 1.5f && !c->active) {
        checkpoint_set(world, c, 1);
    } else if (bx < c->x && c->x - bx >= c->obj.w / 2 + 1.5f && c->active) {
        checkpoint_set(world, c, 0);
    }
}

// BombMapGameMode.a(float) (acl.java:98-111): the timer of the round.
static void bomb_mode_update(FOWorld *world, float dt) {
    FOMode *m = &world->mode;
    if (!fo_mode_bomb_down(world) && !fo_mode_over(world)) m->bombTimer -= dt;
    if (m->bombTimer < 0) m->bombTimer = 0;
}

// RollableBomb.r() (act.java:320-324).
static void bomb_reset(FOWorld *world) {
    FOBomb *b = &world->mode.bomb;
    b->redPush = 0;
    fo_phys_body_set_transform(b->obj.body, b->startX, b->startY, 0);
    fo_phys_body_set_velocity(b->obj.body, 0, 0);
}

// --- Capture the flag -----------------------------------------------------

// CtfFlag.a(int, Vector2) (acm.java:127-160): the place of the flag on the
// carrier, by the class.
static void flag_offset(int cls, float *x, float *y) {
    static const float off[10][2] = {{0, 0},       {-0.2f, -0.1f}, {-0.3f, -0.1f}, {-0.3f, -0.1f}, {-0.3f, -0.2f},
                                     {-0.5f, -0.2f}, {-0.3f, -0.2f}, {-0.4f, -0.3f}, {-0.4f, -0.3f}, {-0.3f, -0.3f}};
    if (cls < 1 || cls > 9) cls = 0;
    *x = off[cls][0];
    *y = off[cls][1];
}

// CtfFlag.v() (acm.java:184-190): the flag goes home.
static void flag_home(FOFlag *f) {
    fo_phys_body_set_transform(f->obj.body, f->homeX, f->homeY, 0);
    fo_phys_body_set_velocity(f->obj.body, 0, 0);
    f->angle = 0;
    f->dropped = 0;
    f->facingLeft = 0;
}

// CtfFlag.a(int) (acm.java:192-201).
static void flag_carrier(FOFlag *f, int id) {
    if (f->carrier != -1 && id == -1) {
        f->returnTime = 30.0f;
        f->dropped = 1;
        f->angle = 0;
    }
    f->carrier = id;
    if (id != -1) f->dropped = 0;
}

// CtfFlag.a(float) (acm.java:78-125).
static void flag_update(FOWorld *world, FOFlag *f, float dt) {
    float fx, fy;
    obj_position(&f->obj, &fx, &fy);
    if (f->carrier == -1) {
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            FOPlayer *p = &world->players[i];
            if (!p->used || p->team == f->team || !p->alive || hidden_spy(p)) continue;
            float x, y;
            fo_player_position(p, &x, &y);
            if (fabsf(x - fx) > f->obj.w || fabsf(y - fy) > f->obj.h) continue;
            flag_carrier(f, p->id);
            break;
        }
        if (f->dropped) {
            f->returnTime -= dt;
            if (f->returnTime <= 0) flag_home(f);
        }
    }
    if (f->carrier != -1) {
        FOPlayer *p = player_by_id(world, f->carrier);
        if (!p || p->team == f->team || !p->alive) {
            flag_carrier(f, -1);
        } else {
            float ox, oy, x, y;
            flag_offset(p->cls, &ox, &oy);
            fo_player_position(p, &x, &y);
            int right = p->facingRight;
            // The original adds the height of the legs image of the carrier.
            // The core has no images, so it uses 0.7 m, the legs of most
            // classes. fo_look draws the flag with the true height.
            float px = (right ? ox : -ox) + (right ? 0 : f->obj.w) + ((x - 0.45000002f) - (f->obj.w - 0.90000004f) / 2);
            float py = 0.7f + y + oy;
            fo_phys_body_set_transform(f->obj.body, px, py, 0);
            // FortOS stops the fall of the carried flag. The original keeps the
            // speed of the fall, which a drop then carries into the ground.
            fo_phys_body_set_velocity(f->obj.body, 0, 0);
            f->angle = right ? 0.34906584f : -0.34906584f;
            f->facingLeft = !right;
        }
    }
    obj_position(&f->obj, &fx, &fy);
    if (in_death_area(world, fx, fy)) {
        flag_carrier(f, -1);
        flag_home(f);
    }
}

// CtfMapGamemode.a(float) (acn.java:56-112).
static void ctf_capture(FOWorld *world, FOFlag *f, FOAreaKind area, int *score) {
    if (f->carrier == -1) return;
    FOPlayer *carrier = player_by_id(world, f->carrier);
    if (!carrier || !player_in_area(world, carrier, area)) return;
    flag_carrier(f, -1);
    flag_home(f);
    (*score)++;
    carrier->score += 3;
    // The original gives 2 points to each BLU player for a capture of
    // either team. FortOS keeps this.
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used && world->players[i].team == FO_TEAM_BLU) world->players[i].score += 2;
    }
}

static void ctf_update(FOWorld *world) {
    FOMode *m = &world->mode;
    if (!fo_mode_over(world)) {
        ctf_capture(world, &m->flags[FO_TEAM_RED], FO_AREA_CAPTURE_BLU, &m->bluScore);
        ctf_capture(world, &m->flags[FO_TEAM_BLU], FO_AREA_CAPTURE_RED, &m->redScore);
    }
    const FOPlayer *local = local_player(world);
    if (local) {
        // The sounds for the team of the local player (acn.java:90-111).
        if (local->team == FO_TEAM_RED) {
            if (m->redScore > m->lastRedScore && m->lastRedScore != -1) {
                fo_world_event(world, FO_EVENT_SOUND, "flag_secured", 0, 0);
            }
            if (m->flags[FO_TEAM_RED].carrier > 0 && m->lastRedCarrier == -1) {
                fo_world_event(world, FO_EVENT_SOUND, "flag_stolen", 0, 0);
            }
        }
        m->lastRedScore = m->redScore;
        m->lastRedCarrier = m->flags[FO_TEAM_RED].carrier;
        if (local->team == FO_TEAM_BLU) {
            if (m->bluScore > m->lastBluScore && m->lastBluScore != -1) {
                fo_world_event(world, FO_EVENT_SOUND, "flag_secured", 0, 0);
            }
            if (m->flags[FO_TEAM_BLU].carrier > 0 && m->lastBluCarrier == -1) {
                fo_world_event(world, FO_EVENT_SOUND, "flag_stolen", 0, 0);
            }
        }
        m->lastBluScore = m->bluScore;
        m->lastBluCarrier = m->flags[FO_TEAM_BLU].carrier;
    }
}

// --- Setup ----------------------------------------------------------------

// MapGameMode.c(): a new round.
static void mode_reset(FOWorld *world) {
    FOMode *m = &world->mode;
    switch (m->kind) {
    case FO_MODE_KOTH:
        m->redTimer = 180;
        m->bluTimer = 180;
        m->owner = 0;
        m->progress = 0;
        break;
    case FO_MODE_BOMB:
        bomb_reset(world);
        m->bombTimer = 420;
        break;
    case FO_MODE_CTF:
        m->redScore = 0;
        m->bluScore = 0;
        break;
    default:
        break;
    }
}

void fo_world_koth(FOWorld *world) {
    world->mode.kind = FO_MODE_KOTH;
    world->mode.lastOwner = -1;
    mode_reset(world);
}

void fo_world_bomb(FOWorld *world, int forward, float startX, float startY, float blastX, float blastY) {
    FOMode *m = &world->mode;
    FOBomb *b = &m->bomb;
    m->kind = FO_MODE_BOMB;
    b->forward = forward;
    b->startX = startX;
    b->startY = startY;
    b->obj.kind = FO_OBJ_BOMB;
    b->obj.w = 3.0f;
    b->obj.h = 3.0f;
    b->obj.body = fo_phys_body_new(world->phys, FO_BODY_DYNAMIC, startX, startY, 0, &b->obj);
    fo_phys_body_set_sleeping_allowed(b->obj.body, 0);
    FOFixtureDef def = {5.0f, 0, 0, FO_CAT_BOMB, FO_CAT_CHECKPOINT | FO_CAT_GROUND, 0};
    fo_phys_circle(b->obj.body, 1.5f, 0, 0, &def);
    b->lastX = startX;
    m->blastX = blastX;
    m->blastY = blastY;
    mode_reset(world);
}

void fo_world_bomb_checkpoint(FOWorld *world, float x, float y) {
    FOMode *m = &world->mode;
    if (m->checkpointCount >= FO_MAX_CHECKPOINTS) return;
    FOCheckpoint *c = &m->checkpoints[m->checkpointCount++];
    c->obj.kind = FO_OBJ_CHECKPOINT;
    c->obj.w = 1.1f;
    c->obj.h = 2.2f;
    c->x = x;
    c->y = y;
    c->obj.body = fo_phys_body_new(world->phys, FO_BODY_STATIC, x, y, 0, &c->obj);
    FOFixtureDef def = {1.1f, 0.7f, 0, FO_CAT_CHECKPOINT, FO_CAT_BOMB, 0};
    fo_phys_box(c->obj.body, c->obj.w / 2 - 0.1f, c->obj.h / 2, -0.1f, 0, 0, &def);
    fo_phys_body_set_active(c->obj.body, 0);
}

static void flag_new(FOWorld *world, FOFlag *f, short team, float x, float y) {
    f->team = team;
    f->carrier = -1;
    f->obj.kind = FO_OBJ_FLAG;
    f->obj.w = 0.5f;
    f->obj.h = 1.5f;
    f->homeX = x;
    f->homeY = y;
    f->obj.body = fo_phys_body_new(world->phys, FO_BODY_DYNAMIC, x, y, 0, &f->obj);
    fo_phys_body_set_fixed_rotation(f->obj.body, 1);
    fo_phys_body_set_sleeping_allowed(f->obj.body, 0);
    // The default fixture of libGDX: density 0, friction 0.2.
    FOFixtureDef def = {0, 0.2f, 0, FO_CAT_FLAG, FO_CAT_GROUND, 0};
    fo_phys_box(f->obj.body, f->obj.w / 2, f->obj.h / 2, 0, 0, 0, &def);
    flag_home(f);
}

void fo_world_ctf(FOWorld *world, float redX, float redY, float bluX, float bluY) {
    FOMode *m = &world->mode;
    m->kind = FO_MODE_CTF;
    flag_new(world, &m->flags[FO_TEAM_RED], FO_TEAM_RED, redX, redY);
    flag_new(world, &m->flags[FO_TEAM_BLU], FO_TEAM_BLU, bluX, bluY);
    m->lastRedScore = m->lastBluScore = -1;
    m->lastRedCarrier = m->lastBluCarrier = -1;
    mode_reset(world);
}

// --- The calls of the world -----------------------------------------------

void fo_modes_contact(FOWorld *world, FOObj *a, FOObj *b, int begin) {
    FOBomb *bomb = &world->mode.bomb;
    if (b->kind == FO_OBJ_BOMB) {
        FOObj *t = a;
        a = b;
        b = t;
    }
    if (a->kind != FO_OBJ_BOMB || b->kind != FO_OBJ_GROUND) return;
    int at = -1;
    for (int i = 0; i < bomb->slopeCount; i++) {
        if (bomb->slopes[i] == b) at = i;
    }
    if (begin && at < 0 && bomb->slopeCount < (int)(sizeof(bomb->slopes) / sizeof(bomb->slopes[0]))) {
        bomb->slopes[bomb->slopeCount++] = b;
    } else if (!begin && at >= 0) {
        memmove(&bomb->slopes[at], &bomb->slopes[at + 1], sizeof(FOObj *) * (size_t)(bomb->slopeCount - at - 1));
        bomb->slopeCount--;
    }
}

// The objects of the mode, in the update of the objects of the world.
void fo_modes_objects(FOWorld *world, float dt) {
    FOMode *m = &world->mode;
    if (m->kind == FO_MODE_BOMB) {
        bomb_update(world, dt);
        for (int i = 0; i < m->checkpointCount; i++) checkpoint_update(world, &m->checkpoints[i]);
    } else if (m->kind == FO_MODE_CTF) {
        flag_update(world, &m->flags[FO_TEAM_RED], dt);
        flag_update(world, &m->flags[FO_TEAM_BLU], dt);
    }
}

// MapGameMode.a(float), in the update of the map.
void fo_modes_update(FOWorld *world, float dt) {
    switch (world->mode.kind) {
    case FO_MODE_KOTH: koth_update(world, dt); break;
    case FO_MODE_BOMB: bomb_mode_update(world, dt); break;
    case FO_MODE_CTF: ctf_update(world); break;
    default: break;
    }
}

// GameServer.a(float) (ws.java:30-57): 15 s after the end of a round, a new
// round starts. Each score goes to 0, the teams change sides on the bomb
// map, and each player spawns again.
void fo_modes_round(FOWorld *world) {
    if (!fo_mode_over(world)) return;
    if (world->roundEnd == 0) {
        world->roundEnd = world->now + 15000;
        return;
    }
    if (world->now < world->roundEnd) return;
    mode_reset(world);
    int swap = world->mode.kind == FO_MODE_BOMB;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *p = &world->players[i];
        if (!p->used) continue;
        p->score = 0;
        if ((p->team != FO_TEAM_BLU && p->team != FO_TEAM_RED) || p->cls == 0) continue;
        if (swap) fo_world_set_team(world, p, p->team == FO_TEAM_RED ? FO_TEAM_BLU : FO_TEAM_RED);
        fo_classes_reset(world, p, 0);
        fo_world_spawn(world, p, p->cls);
    }
    world->roundEnd = 0;
}

int fo_world_mode_allows(const FOWorld *world, const FOPlayer *p) {
    if (world->mode.kind != FO_MODE_CTF) return 1;
    return world->mode.flags[FO_TEAM_RED].carrier != p->id && world->mode.flags[FO_TEAM_BLU].carrier != p->id;
}

int fo_world_holds_objective(const FOWorld *world, const FOPlayer *p) {
    const FOMode *m = &world->mode;
    switch (m->kind) {
    case FO_MODE_KOTH: return koth_holds(world, p);
    case FO_MODE_BOMB: {
        // BombMapGameMode.b(xn) and RollableBomb.a(xn) (act.java:316-318).
        if (p->team != FO_TEAM_RED || m->bomb.bluCount >= m->bomb.redCount) return 0;
        float bx, by, x, y;
        obj_position(&m->bomb.obj, &bx, &by);
        fo_player_position(p, &x, &y);
        return fabsf(x - bx) < 2.0f && fabsf(y - by) < 1.5f;
    }
    case FO_MODE_CTF:
        // CtfMapGamemode.b(xn) (acn.java:48-53) asks for the flag of the own
        // team, which a player of that team cannot carry. So it is always
        // 0. FortOS keeps this.
        return (p->team == FO_TEAM_RED && m->flags[FO_TEAM_RED].carrier == p->id) ||
               (p->team == FO_TEAM_BLU && m->flags[FO_TEAM_BLU].carrier == p->id);
    default: return 0;
    }
}

// --- The queries ----------------------------------------------------------

int fo_mode_bomb_down(const FOWorld *world) {
    if (world->mode.kind != FO_MODE_BOMB) return 0;
    float x, y;
    obj_position(&world->mode.bomb.obj, &x, &y);
    return in_area(world, FO_AREA_BOMB_FALL, x, y);
}

int fo_mode_over(const FOWorld *world) {
    const FOMode *m = &world->mode;
    switch (m->kind) {
    case FO_MODE_KOTH: return m->bluTimer <= 0 || m->redTimer <= 0;
    case FO_MODE_BOMB:
        return m->bombTimer <= 0 ||
               (fo_mode_bomb_down(world) && m->bomb.finished != 0 && world->now - m->bomb.finished >= 4500);
    case FO_MODE_CTF: return m->redScore >= 4 || m->bluScore >= 4;
    default: return 0;
    }
}

int fo_mode_winner(const FOWorld *world) {
    const FOMode *m = &world->mode;
    switch (m->kind) {
    case FO_MODE_KOTH: return m->redTimer <= 0 ? FO_TEAM_RED : m->bluTimer <= 0 ? FO_TEAM_BLU : 0;
    case FO_MODE_BOMB: return fo_mode_bomb_down(world) ? FO_TEAM_RED : FO_TEAM_BLU;
    case FO_MODE_CTF: return m->redScore >= 4 ? FO_TEAM_RED : m->bluScore >= 4 ? FO_TEAM_BLU : 0;
    default: return 0;
    }
}

// BombMapGameMode.e() (acl.java:76-88): after the blast, the camera goes up
// to the top of the fall area.
void fo_mode_focus(const FOWorld *world, float *x, float *y) {
    const FOMode *m = &world->mode;
    *x = *y = 0;
    if (m->kind != FO_MODE_BOMB) return;
    obj_position(&m->bomb.obj, x, y);
    *y -= 3.0f;
    if (m->blastTime != 0 && world->now - m->blastTime < 19500) {
        *y += (float)(world->now - m->blastTime) / 10.0f;
        const FORect *fall = fo_tmx_area(&world->map, FO_AREA_BOMB_FALL);
        if (fall && *y > fall->y + fall->h) *y = fall->y + fall->h;
    }
}

void fo_mode_objective(const FOWorld *world, float *x, float *y) {
    const FOMode *m = &world->mode;
    *x = *y = 0;
    if (m->kind == FO_MODE_KOTH) {
        const FORect *r = fo_tmx_area(&world->map, FO_AREA_KOTH);
        if (r) {
            *x = r->x + r->w / 2;
            *y = r->y + r->h / 2;
        }
    } else if (m->kind == FO_MODE_BOMB) {
        obj_position(&m->bomb.obj, x, y);
    } else if (m->kind == FO_MODE_CTF) {
        *x = world->map.width / 2;
        *y = world->map.height / 2;
    }
}
