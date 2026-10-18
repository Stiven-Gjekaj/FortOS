#include "fo_effects.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fo_gfx.h"

#define FO_ART 0.1f
#define FO_EFFECTS 160
#define FO_FRAMES 32

// SpriteEffectsHandler.a (alb.java:36-52).
typedef enum {
    FX_EXPLOSION,
    FX_BULLET_HIT_WALL,
    FX_POOF_STRAIGHT,
    FX_ROCKET_SMOKE,
    FX_RED_SENTRY,
    FX_BLU_SENTRY,
    FX_ZAP,
    FX_WATER_DRIP,
    FX_ROCKFALL,
    FX_BLOOD1,
    FX_BLOOD2,
    FX_BLOOD3,
    FX_HUGE,
    FX_FIRE,
    FX_COUNT,
} FOEffectKind;

typedef struct {
    FOSprite frames[FO_FRAMES];
    int count;
    float duration;   // The time of one frame.
    int loop;
} FOAnim;

static FOAnim gAnims[FX_COUNT];
static FOAtlasTextures gExplosion;
static int gExplosionLoaded;

// SpriteEffectsHandler.b (alb.java:53-177).
typedef struct {
    int active;
    FOEffectKind kind;
    float x, y;          // j and k
    float xscale;        // c
    float rotation;      // m, in radians.
    float begin;         // e
    float frame;         // l: a custom frame time, or 0.
    int player;          // n: the id of the player that it follows, or 0.
    int bullet;          // n: the index of the rocket that it follows, or -1.
    long long spawn;     // The spawn time of that rocket.
    float o, p;          // The offset from the object.
    float fadeStart;     // f
    float fadeTime;      // g
} FOEffect;

static FOEffect gEffects[FO_EFFECTS];

// PeriodicalMapSpriteEffect (acz.java).
typedef struct {
    FOEffectKind kind;
    float x, y;
    int minMs, maxMs;
    int flip;
    float next;
} FOPeriodic;

static FOPeriodic gPeriodic[8];
static int gPeriodicCount;
static int gSmoke[FO_MAX_BULLETS];   // The effect of the smoke of each rocket, or -1.

static void load_anim(FOAnim *a, const FOAtlasTextures *atlas, const char *pattern, int count, float duration, int loop) {
    a->count = 0;
    a->duration = duration;
    a->loop = loop;
    for (int i = 0; i < count && i < FO_FRAMES; i++) {
        char name[48];
        snprintf(name, sizeof(name), pattern, i + 1);
        a->frames[a->count++] = fo_assets_sprite(atlas, name);
    }
}

void fo_effects_init(const FOAtlasTextures *s) {
    // The frames of alb.java:27-32 and 165-177, from the lists of wn.java.
    load_anim(&gAnims[FX_EXPLOSION], s, "explosion%d", 6, 0.06666667f, 0);
    load_anim(&gAnims[FX_BULLET_HIT_WALL], s, "bullet_hit_wall%d", 3, 0.06666667f, 0);
    load_anim(&gAnims[FX_POOF_STRAIGHT], s, "poof_straight%d", 3, 0.06666667f, 0);
    load_anim(&gAnims[FX_ROCKET_SMOKE], s, "rocket_smoke%d", 8, 0.1f, 1);
    load_anim(&gAnims[FX_RED_SENTRY], s, "red_sentry_destroy%d", 17, 0.06666667f, 0);
    load_anim(&gAnims[FX_BLU_SENTRY], s, "blu_sentry_destroy%d", 17, 0.06666667f, 0);
    load_anim(&gAnims[FX_ZAP], s, "zap%d", 3, 0.06666667f, 0);
    load_anim(&gAnims[FX_WATER_DRIP], s, "water_drip%d", 9, 0.06666667f, 0);
    load_anim(&gAnims[FX_ROCKFALL], s, "rockfall_angled%d", 8, 0.06666667f, 0);
    load_anim(&gAnims[FX_BLOOD1], s, "blood_1_%d", 4, 0.06666667f, 0);
    load_anim(&gAnims[FX_BLOOD2], s, "blood_2_%d", 4, 0.06666667f, 0);
    load_anim(&gAnims[FX_BLOOD3], s, "blood_3_%d", 4, 0.06666667f, 0);
    load_anim(&gAnims[FX_FIRE], s, "fire1_%d", 10, 0.06666667f, 1);
    gAnims[FX_HUGE].count = 0;
    for (int i = 0; i < FO_MAX_BULLETS; i++) gSmoke[i] = -1;
}

static float frand(float lo, float hi) {
    return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}

static int irand(int lo, int hi) {
    return lo + rand() % (hi - lo + 1);
}

// SpriteEffectsHandler.a(Vector2, a) (alb.java:234-292).
static FOEffect *spawn(FOEffectKind kind, float x, float y, float now) {
    if (gAnims[kind].count == 0) return NULL;
    FOEffect *e = NULL;
    for (int i = 0; i < FO_EFFECTS; i++) {
        if (!gEffects[i].active) {
            e = &gEffects[i];
            break;
        }
    }
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->active = 1;
    e->kind = kind;
    e->x = x;
    e->y = y;
    e->xscale = 1;
    e->begin = now;
    e->bullet = -1;
    // The zap of gantown faces a random side and has a random speed.
    if (kind == FX_ZAP) {
        e->xscale = rand() % 2 ? 1.0f : -1.0f;
        e->frame = frand(0.07692308f, 0.14285715f);
    }
    return e;
}

static void add_periodic(FOEffectKind kind, float x, float y, int minMs, int maxMs, int flip, float now) {
    if (gPeriodicCount >= 8) return;
    FOPeriodic *p = &gPeriodic[gPeriodicCount++];
    p->kind = kind;
    p->x = x;
    p->y = y;
    p->minMs = minMs;
    p->maxMs = maxMs;
    p->flip = flip;
    p->next = now + irand(minMs, maxMs) / 1000.0f;
}

void fo_effects_reset(FOMapId map, float now) {
    memset(gEffects, 0, sizeof(gEffects));
    for (int i = 0; i < FO_MAX_BULLETS; i++) gSmoke[i] = -1;
    gPeriodicCount = 0;
    if (map == FO_MAP_BOMB_CANYON && !gExplosionLoaded) {
        // The big blast of the bomb (alb.java:177): 26 frames of
        // explosion.atlas at 12 frames each second.
        gExplosionLoaded = fo_assets_atlas(&gExplosion, "textures/explosion.atlas");
        if (gExplosionLoaded) load_anim(&gAnims[FX_HUGE], &gExplosion, "explosion%d", 26, 0.083333336f, 0);
    } else if (map != FO_MAP_BOMB_CANYON && gExplosionLoaded) {
        fo_assets_atlas_free(&gExplosion);
        gExplosionLoaded = 0;
        gAnims[FX_HUGE].count = 0;
    }
    if (map == FO_MAP_GANTOWN) {
        // GTS_Gantown (acp.java:38-44).
        add_periodic(FX_ZAP, 36.100002f, 7.5f, 3000, 6000, 0, now);
        add_periodic(FX_ZAP, 64.8f, 6.9f, 3000, 6000, 0, now);
        add_periodic(FX_WATER_DRIP, 80.200005f, 8.85f, 2000, 4000, 0, now);
        add_periodic(FX_ROCKFALL, 76.4f, 9.0f, 15000, 20000, 0, now);
        add_periodic(FX_ROCKFALL, 47.9f, 9.0f, 20000, 40000, 0, now);
        add_periodic(FX_ROCKFALL, 39.5f, 9.0f, 15000, 20000, 1, now);
        add_periodic(FX_ROCKFALL, 68.200005f, 9.1f, 20000, 40000, 1, now);
    } else if (map == FO_MAP_FLAG_EGYPT) {
        // FLAG_Eypt (aco.java:36-37): two fires that burn all the time.
        spawn(FX_FIRE, 56.7f, 7.25f, now);
        spawn(FX_FIRE, 63.9f, 7.25f, now);
    }
}

// SpriteEffectsHandler.a(b, float) (alb.java:332-342): the effect stays at
// its last place and fades in the time.
static void fade(FOEffect *e, const FOWorld *world, float time, float now);

static void effect_place(const FOEffect *e, const FOWorld *world, float *x, float *y, float *rotation) {
    *x = e->x;
    *y = e->y;
    *rotation = e->rotation;
    float ox = 0, oy = 0, g = 0;
    int attached = 0;
    if (e->player) {
        const FOPlayer *p = fo_world_player((FOWorld *)world, e->player);
        if (p) {
            fo_player_position(p, &ox, &oy);
            attached = 1;
        }
    } else if (e->bullet >= 0) {
        const FOBullet *b = &world->bullets[e->bullet];
        if (b->active && b->spawn == e->spawn) {
            fo_phys_body_position(b->obj.body, &ox, &oy);
            g = b->angle;
            attached = 1;
        }
    }
    if (attached) {
        *x = ox + e->o * cosf(g) - sinf(g) * e->p;
        *y = cosf(g) * e->p + oy + e->o * sinf(g);
        *rotation = g;
    }
}

static void fade(FOEffect *e, const FOWorld *world, float time, float now) {
    float x, y, r;
    effect_place(e, world, &x, &y, &r);
    e->x = x;
    e->y = y;
    e->rotation = r;
    e->player = 0;
    e->bullet = -1;
    e->fadeStart = now;
    e->fadeTime = time;
}

void fo_effects_update(const FOWorld *world, float now) {
    for (int i = 0; i < world->event_count; i++) {
        const FOEvent *ev = &world->events[i];
        if (ev->kind != FO_EVENT_EFFECT) continue;
        const char *n = ev->name;
        if (strcmp(n, "explosion") == 0) {
            spawn(FX_EXPLOSION, ev->x, ev->y, now);
        } else if (strcmp(n, "bullet_hit_wall") == 0) {
            spawn(FX_BULLET_HIT_WALL, ev->x, ev->y, now);
        } else if (strcmp(n, "poof_straight") == 0) {
            spawn(FX_POOF_STRAIGHT, ev->x, ev->y, now);
        } else if (strcmp(n, "red_sentry_destroy") == 0) {
            spawn(FX_RED_SENTRY, ev->x, ev->y, now);
        } else if (strcmp(n, "blu_sentry_destroy") == 0) {
            spawn(FX_BLU_SENTRY, ev->x, ev->y, now);
        } else if (strcmp(n, "hugeExplosion") == 0) {
            spawn(FX_HUGE, ev->x, ev->y, now);
        } else if (strcmp(n, "blood") == 0) {
            // Weapon.b(xg, xn) (alz.java:190-206): one of three bloods on the
            // target, turned away from the bullet.
            int k = irand(0, 2);
            FOEffect *e = spawn(k == 0 ? FX_BLOOD1 : k == 1 ? FX_BLOOD2 : FX_BLOOD3, ev->x, ev->y, now);
            if (e) {
                float f = (float)ev->flip;
                e->xscale = f;
                e->player = ev->a;
                e->o = -f * ((0.90000004f - FO_ART * 2.0f) / 2.0f);
                e->p = FO_ART * frand(-3.0f, 3.0f);
            }
        }
    }
    // RocketLauncher.a(...) (alr.java:104-112, 66-71): the smoke of a rocket
    // follows it 1.3 m behind, and fades in 0.2 s at its end.
    for (int i = 0; i < FO_MAX_BULLETS; i++) {
        const FOBullet *b = &world->bullets[i];
        int rocket = b->active && b->def && b->def->kind == FO_SHOT_ROCKET;
        FOEffect *e = gSmoke[i] >= 0 ? &gEffects[gSmoke[i]] : NULL;
        if (e && (!e->active || e->bullet != i)) e = NULL;
        if (e && (!rocket || e->spawn != b->spawn)) {
            fade(e, world, 0.2f, now);
            gSmoke[i] = -1;
            e = NULL;
        }
        if (rocket && !e) {
            float x, y;
            fo_phys_body_position(b->obj.body, &x, &y);
            FOEffect *s = spawn(FX_ROCKET_SMOKE, x, y, now);
            if (s) {
                s->bullet = i;
                s->spawn = b->spawn;
                s->o = -13.0f * FO_ART;
                gSmoke[i] = (int)(s - gEffects);
            }
        }
    }
    // PeriodicalMapSpriteEffect (ack.java:106-111).
    for (int i = 0; i < gPeriodicCount; i++) {
        FOPeriodic *p = &gPeriodic[i];
        if (now < p->next) continue;
        FOEffect *e = spawn(p->kind, p->x, p->y, now);
        if (e) e->xscale = p->flip ? -1.0f : 1.0f;
        p->next = now + irand(p->minMs, p->maxMs) / 1000.0f;
    }
}

static int layer_of(FOEffectKind k) {
    // SpriteEffectsHandler.b.e() (alb.java:93-121): 0 behind the platforms,
    // 1 with the sentries, 2 on top.
    switch (k) {
    case FX_POOF_STRAIGHT:
    case FX_BULLET_HIT_WALL:
    case FX_ROCKET_SMOKE:
    case FX_BLOOD1:
    case FX_BLOOD2:
    case FX_BLOOD3:
    case FX_HUGE: return 0;
    case FX_RED_SENTRY:
    case FX_BLU_SENTRY: return 1;
    default: return 2;
    }
}

// SpriteEffectsHandler.a(alh) (alb.java:179-232).
static void draw_layer(const FOWorld *world, int layer, float now) {
    for (int i = 0; i < FO_EFFECTS; i++) {
        FOEffect *e = &gEffects[i];
        if (!e->active || now - e->begin < 0 || layer_of(e->kind) != layer) continue;
        const FOAnim *a = &gAnims[e->kind];
        if (a->count == 0) {
            e->active = 0;
            continue;
        }
        float dur = e->frame != 0 ? e->frame : a->duration;
        float t = now - e->begin;
        int index = (int)(t / dur);
        if (a->loop) {
            if (e->fadeStart != 0 && now >= e->fadeStart + e->fadeTime) {
                e->active = 0;
                continue;
            }
            index %= a->count;
        } else if (index > a->count - 1) {
            e->active = 0;
            continue;
        }
        const FOSprite *s = &a->frames[index];
        if (!s->w) continue;
        float w = s->w * FO_ART, h = s->h * FO_ART;
        float dx = -w / 2, dy = -h / 2;
        if (e->kind == FX_POOF_STRAIGHT) {
            dy = 0;
        } else if (e->kind == FX_BLOOD1 || e->kind == FX_BLOOD2 || e->kind == FX_BLOOD3) {
            dx = e->xscale == 1.0f ? -w : 0;
        } else if (e->kind == FX_HUGE) {
            dx = 0;
            dy = 0;
        }
        float alpha = e->fadeStart != 0 ? 1.0f - (now - e->fadeStart) / e->fadeTime : 1.0f;
        if (alpha <= 0) continue;
        float x, y, r;
        effect_place(e, world, &x, &y, &r);
        FOColor tint = {1, 1, 1, alpha};
        fo_gfx_draw(s, dx + x, dy + y, w, h, w / 2, h / 2, r, e->xscale < 0, tint);
    }
}

void fo_effects_draw_behind(const FOWorld *world, float now) {
    // The platforms (depth 0.5) hide these effects.
    fo_gfx_depth(FO_DEPTH_TEST, 0.55f);
    draw_layer(world, 0, now);
    fo_gfx_depth(FO_DEPTH_OFF, 0);
}

void fo_effects_draw_sentry(const FOWorld *world, float now) {
    draw_layer(world, 1, now);
}

void fo_effects_draw_top(const FOWorld *world, float now) {
    draw_layer(world, 2, now);
}
