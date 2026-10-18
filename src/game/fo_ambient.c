#include "fo_ambient.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fo_gfx.h"
#include "fo_world.h"

#define FO_ART 0.1f
#define FO_FLIES 256
#define FO_BIRDS 3

// Flie (xk.java): a fly that moves in small steps in its zone.
typedef struct {
    FORect zone;      // j
    float x, y;
    float toX, toY;   // k
    float fromX, fromY;  // l
    float stepStart;  // m
    float step;       // n: the time of a step.
    int burned;       // o
} FOFly;

// Bird (xc.java): a bird that sits on the ground and flies away.
typedef struct {
    float x, y;
    int flying;       // p
    int right;        // q: it flies to the right.
    int lookRight;    // r
    float idleTime;   // s
    float idleStart;  // t
    float flyStart;   // u
    float vx, vy;     // l
} FOBird;

static FOFly gFlies[FO_FLIES];
static int gFlyCount;
static FOBird gBirds[FO_BIRDS];
static int gBirdCount;
static FOSprite gFly, gIdle[7], gFlap[4];
static float gViewX, gViewY, gViewW, gViewH;

static FOObj *fixture_obj(FOPhysFixture *fixture) {
    return (FOObj *)fo_phys_body_user(fo_phys_fixture_body(fixture));
}

static float frand(float lo, float hi) {
    return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}

void fo_ambient_init(const FOAtlasTextures *sprites) {
    gFly = fo_assets_sprite(sprites, "flie");
    char name[32];
    for (int i = 0; i < 7; i++) {
        snprintf(name, sizeof(name), "birds_idle%d", i + 1);
        gIdle[i] = fo_assets_sprite(sprites, name);
    }
    for (int i = 0; i < 4; i++) {
        snprintf(name, sizeof(name), "birds_fly%d", i + 1);
        gFlap[i] = fo_assets_sprite(sprites, name);
    }
}

// GameCamera.a(x, y, w, h): the box is in the view.
static int in_view(float x, float y, float w, float h) {
    return fabsf(x - (gViewX + gViewW / 2)) <= gViewW / 2 + w / 2 && fabsf(y - (gViewY + gViewH / 2)) <= gViewH / 2 + h / 2;
}

typedef struct {
    int hit;
    float x, y;
} FOAmbientHit;

// xe.java: the nearest map ground on the ray.
static float ray_nearest(void *context, FOPhysFixture *fixture, float px, float py, float nx, float ny, float fraction) {
    (void)nx;
    (void)ny;
    FOObj *o = fixture_obj(fixture);
    if (!o || o->kind != FO_OBJ_GROUND) return 1.0f;
    FOAmbientHit *h = context;
    h->hit = 1;
    h->x = px;
    h->y = py;
    return fraction;
}

// xd.java: any map ground on the ray.
static float ray_any(void *context, FOPhysFixture *fixture, float px, float py, float nx, float ny, float fraction) {
    (void)px;
    (void)py;
    (void)nx;
    (void)ny;
    (void)fraction;
    FOObj *o = fixture_obj(fixture);
    if (!o || o->kind != FO_OBJ_GROUND) return 1.0f;
    ((FOAmbientHit *)context)->hit = 1;
    return 0.0f;
}

// Bird.b(boolean) (xc.java:140-167): the bird sits in a zone with no other
// bird, on the ground under a random point. With hidden, only in a zone
// out of the view.
static void bird_land(const FOWorld *world, FOBird *b, int hidden) {
    const FORect *zones[16];
    int n = 0;
    for (int a = 0; a < world->map.area_count && n < 16; a++) {
        if (world->map.areas[a].kind != FO_AREA_BIRDS) continue;
        const FORect *r = &world->map.areas[a].rect;
        int taken = 0;
        for (int k = 0; k < gBirdCount; k++) taken |= fo_rect_contains(r, gBirds[k].x, gBirds[k].y);
        if (!taken && (!hidden || !in_view(r->x, r->y, r->w, r->h))) zones[n++] = r;
    }
    if (n == 0) return;
    const FORect *r = zones[rand() % n];
    float x = r->x + r->w * frand(0, 1);
    // The ray goes from the top of the zone to its bottom left corner, as
    // the original.
    FOAmbientHit h = {0, 0, 0};
    fo_phys_ray(world->phys, x, r->y + r->h, r->x, r->y, ray_nearest, &h);
    if (h.hit) {
        b->x = x;
        b->y = h.y;
        b->flying = 0;
    }
}

void fo_ambient_reset(const FOWorld *world) {
    gFlyCount = 0;
    gBirdCount = 0;
    // BaseGameMap (ack.java:48-58): one fly for each 0.3 square meters of a
    // fly zone, and 3 birds when the map has a bird zone.
    int birds = 0;
    for (int a = 0; a < world->map.area_count; a++) {
        const FORect *r = &world->map.areas[a].rect;
        if (world->map.areas[a].kind == FO_AREA_BIRDS) birds = 1;
        if (world->map.areas[a].kind != FO_AREA_FLIES) continue;
        int count = (int)(r->w * r->h / 0.1f / 3.0f);
        for (int i = 0; i < count && gFlyCount < FO_FLIES; i++) {
            FOFly *f = &gFlies[gFlyCount++];
            memset(f, 0, sizeof(*f));
            f->zone = *r;
            f->x = r->x + frand(0, r->w);
            f->y = r->y + frand(0, r->h);
            f->step = 0.2f;
            f->stepStart = -1000;
        }
    }
    if (birds) {
        for (int i = 0; i < FO_BIRDS; i++) {
            FOBird *b = &gBirds[gBirdCount];
            memset(b, 0, sizeof(*b));
            b->x = -100;
            b->y = -100;
            gBirdCount++;
            bird_land(world, b, 0);
        }
    }
}

// Flie.q() (xk.java:79-91): the next step, up to 0.4 m away.
static void fly_step(FOFly *f, float now) {
    float a = frand(0, 6.2831855f), r = frand(1.0f, 4.0f) * 0.1f;
    f->toX = (float)((f->x + r * cos(a)) - r * sin(a));
    f->toY = (float)(cos(a) * r + f->y + r * sin(a));
    if (!fo_rect_contains(&f->zone, f->toX, f->toY)) {
        f->toX = f->zone.x + frand(0, f->zone.w);
        f->toY = f->zone.y + frand(0, f->zone.h);
    }
    f->stepStart = now;
    f->fromX = f->x;
    f->fromY = f->y;
}

// Bird.b(Vector2) (xc.java:121-138): the bird flies up and away from the
// threat at 6 m/s.
static void bird_flee(FOBird *b, float tx, float now) {
    float lo, hi;
    b->flying = 1;
    if (tx < b->x) {
        lo = 0.43633232f;
        hi = 0.87266463f;
        b->right = 1;
    } else {
        lo = 2.268928f;
        hi = 2.7052603f;
        b->right = 0;
    }
    float a = (hi - lo) * frand(0, 1) + lo;
    b->vx = cosf(a) * 6.0f;
    b->vy = sinf(a) * 6.0f;
    b->flyStart = now;
}

void fo_ambient_update(const FOWorld *world, float viewX, float viewY, float viewW, float viewH, float dt, float now) {
    gViewX = viewX;
    gViewY = viewY;
    gViewW = viewW;
    gViewH = viewH;
    // Flie.a(float) (xk.java:24-66): a fly moves only in the view, and a
    // flame of a flamethrower near it burns it until it is out of the view.
    for (int i = 0; i < gFlyCount; i++) {
        FOFly *f = &gFlies[i];
        int seen = in_view(f->x, f->y, 0.1f, 0.1f);
        if (f->burned) {
            if (!seen) f->burned = 0;
            else continue;
        }
        if (!seen) continue;
        for (int k = 0; k < FO_MAX_BULLETS; k++) {
            const FOBullet *b = &world->bullets[k];
            if (!b->active || !b->def || b->def->kind != FO_SHOT_FLAME) continue;
            float bx, by;
            fo_phys_body_position(b->obj.body, &bx, &by);
            if (sqrtf((bx - f->x) * (bx - f->x) + (by - f->y) * (by - f->y)) < 0.3f) f->burned = 1;
        }
        if (now - f->stepStart >= f->step) {
            fly_step(f, now);
            f->step = (float)(150 + rand() % 201) / 1000.0f;
        }
        float t = (now - f->stepStart) / f->step;
        f->x = f->fromX + (f->toX - f->fromX) * t;
        f->y = t * (f->toY - f->fromY) + f->fromY;
    }
    // Bird.a(float) (xc.java:52-86).
    for (int i = 0; i < gBirdCount; i++) {
        FOBird *b = &gBirds[i];
        if (!b->flying) {
            if (!in_view(b->x, b->y, 0.1f, 0.1f)) continue;
            for (int k = 0; k < FO_MAX_BULLETS && !b->flying; k++) {
                const FOBullet *bu = &world->bullets[k];
                if (!bu->active) continue;
                float bx, by;
                fo_phys_body_position(bu->obj.body, &bx, &by);
                if (sqrtf((bx - b->x) * (bx - b->x) + (by - b->y) * (by - b->y)) < 1.3000001f) bird_flee(b, bx, now);
            }
            for (int k = 0; k < FO_MAX_PLAYERS && !b->flying; k++) {
                const FOPlayer *p = &world->players[k];
                if (!p->used || !p->obj.body) continue;
                float px, py;
                fo_player_position(p, &px, &py);
                if (sqrtf((px - b->x) * (px - b->x) + (py - b->y) * (py - b->y)) > 3.0f) continue;
                FOAmbientHit h = {0, 0, 0};
                if (px != b->x || py != b->y) fo_phys_ray(world->phys, b->x, b->y, px, py, ray_any, &h);
                if (!h.hit) bird_flee(b, px, now);
            }
        }
        if (b->flying) {
            b->x += b->vx * dt;
            b->y += b->vy * dt;
            if (now - b->flyStart >= 30.0f) bird_land(world, b, 1);
        }
    }
}

void fo_ambient_draw(float now) {
    // mapLayer5 of wz.java: behind the platforms (depth 0.5).
    fo_gfx_depth(FO_DEPTH_TEST, 0.55f);
    if (gFly.w) {
        for (int i = 0; i < gFlyCount; i++) {
            const FOFly *f = &gFlies[i];
            if (f->burned) continue;
            fo_gfx_draw(&gFly, f->x - 0.05f, f->y - 0.05f, 0.1f, 0.1f, 0, 0, 0, 0, FO_WHITE);
        }
    }
    for (int i = 0; i < gBirdCount; i++) {
        FOBird *b = &gBirds[i];
        if (!in_view(b->x, b->y, 0.1f, 0.1f)) continue;
        // Bird.a(alh) (xc.java:88-119): a sitting bird looks to a new side
        // each 1.2 to 3 s and plays its idle frames once.
        if (now - b->idleStart >= b->idleTime) {
            b->idleStart = now;
            b->lookRight = rand() % 2;
            b->idleTime = frand(1.2f, 3.0f);
        }
        const FOSprite *s;
        int right;
        if (!b->flying) {
            int k = (int)((now - b->idleStart) / 0.1f);
            s = &gIdle[k > 6 ? 6 : k];
            right = b->lookRight;
        } else {
            s = &gFlap[(int)(now / 0.06666667f) % 4];
            right = b->right;
        }
        if (!s->w) continue;
        float w = s->w * FO_ART, h = s->h * FO_ART;
        fo_gfx_draw(s, b->x - w / 2, b->y, w, h, w / 2, h / 2, 0, !right, FO_WHITE);
    }
    fo_gfx_depth(FO_DEPTH_OFF, 0);
}
