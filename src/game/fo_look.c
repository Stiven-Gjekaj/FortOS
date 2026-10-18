#include "fo_look.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define Z 0.1f

static const char *kClassNames[10] = {"", "scout", "soldier", "pyro", "demoman", "heavy",
                                      "engineer", "medic", "sniper", "spy"};

// The regions of one class in one team.
typedef struct {
    FOSprite idle, squat, up, middle, down;
    FOSprite run[6], walk[6];
    int hasWalk;
    FOSprite body[2];
    FOSprite head;
    FOSprite arm;
    FOSprite death[7];
    FOSprite bodytop;
} FOClassLook;

// The numbers of each class in art pixels (player-render.md, 2.3).
typedef struct {
    float ex, ey;      // e: the head
    float legsX;       // G.x
    float burnX, burnY;
    float deathX, deathY;
} FOClassData;

static const FOClassData kData[10] = {
    {0, 0, 0, 0, 0, 0, 0},
    {0.5f, 5, 0, 0, 7, -1, -1.5f},
    {0.5f, 7, 0, 0, 8, -1.5f, -1.5f},
    {2, 7, 1, 1, 6, 0, -2.5f},
    {0.5f, 3, 0, -1, 6, 1, -2.5f},
    {0, 8, 0, -1, 8, 0.5f, -3},
    {0.5f, 7, 1, 0, 5, 0.5f, -3.5f},
    {0.5f, 5, 0, 0, 7, -0.5f, -2},
    {0, 7, -1, -1, 7, 2.5f, -1.5f},
    {1.5f, 6, 0, 0, 7, 1, -2},
};

static const FOAtlasTextures *gSprites;
static FOClassLook gLooks[10][3];
static FOSprite gBurning[7];
static FOSprite gBackstab[3][15];     // By team.
static FOSprite gPrepareDisguise[3][5];
static FOSprite gCigarette[5];
static FOSprite gDisguiseSmoke[12];
static FOSprite gSentryLegs, gTurret[3], gAssemble[3][8];
static FOSprite gBeam[3], gBarEmpty, gBarFull;
static FOSprite gWeapons[64][3];        // By item id and team.
static FOSprite gFlashes[64];
static FOSprite gBullets[64][6];
static int gBulletFrames[64];

// The region "team_class_part" when it is there, else "class_part": the
// team prefix of each part changes from class to class.
static FOSprite find(const char *team, const char *cls, const char *part) {
    char name[96];
    snprintf(name, sizeof(name), "%s_%s_%s", team, cls, part);
    FOSprite s = fo_assets_sprite(gSprites, name);
    if (s.w) return s;
    snprintf(name, sizeof(name), "%s_%s", cls, part);
    return fo_assets_sprite(gSprites, name);
}

static FOSprite named(const char *name) {
    return fo_assets_sprite(gSprites, name);
}

static void init_weapon(int item, const char *sprite, const char *flash, const char *bullets[], int frames) {
    for (int t = 1; t <= 2; t++) {
        char name[64];
        snprintf(name, sizeof(name), sprite, t == 2 ? "red" : "blu");
        gWeapons[item][t] = named(name);
    }
    if (flash) gFlashes[item] = named(flash);
    for (int i = 0; i < frames; i++) gBullets[item][i] = named(bullets[i]);
    gBulletFrames[item] = frames;
}

void fo_look_init(const FOAtlasTextures *sprites) {
    gSprites = sprites;
    for (int cls = 1; cls <= 9; cls++) {
        for (int t = 1; t <= 2; t++) {
            const char *team = t == 2 ? "red" : "blu";
            const char *c = kClassNames[cls];
            FOClassLook *l = &gLooks[cls][t];
            char part[32];
            l->idle = find(team, c, "legs_idle");
            l->squat = find(team, c, "legs_squated");
            l->up = find(team, c, "legs_inair_up");
            l->middle = find(team, c, "legs_inair_middle");
            l->down = find(team, c, "legs_inair_down");
            for (int i = 0; i < 6; i++) {
                snprintf(part, sizeof(part), "legs_run%d", i + 1);
                l->run[i] = find(team, c, part);
                snprintf(part, sizeof(part), "legs_walk%d", i + 1);
                l->walk[i] = find(team, c, part);
            }
            l->hasWalk = l->walk[0].w != 0;
            l->body[0] = find(team, c, "body_idle1");
            l->body[1] = find(team, c, "body_idle2");
            l->head = find(team, c, "head");
            l->arm = find(team, c, "arm_left");
            for (int i = 0; i < 7; i++) {
                snprintf(part, sizeof(part), "death_forward%d", i + 1);
                l->death[i] = find(team, c, part);
            }
            l->bodytop = find(team, c, "bodytop");
        }
    }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof(name), "player_burning%d", i + 1);
        gBurning[i] = named(name);
    }
    for (int t = 1; t <= 2; t++) {
        const char *team = t == 2 ? "red" : "blu";
        char name[48];
        // The backstab: 4 frames of the start, 7 of the stab, and the 4
        // first frames back (wn.java:68-69).
        static const int prepare[4] = {1, 3, 4, 5};
        int n = 0;
        for (int i = 0; i < 4; i++) {
            snprintf(name, sizeof(name), "%s_spy_prepare_backstab%d", team, prepare[i]);
            gBackstab[t][n++] = named(name);
        }
        for (int i = 1; i <= 7; i++) {
            snprintf(name, sizeof(name), "%s_spy_backstab%d", team, i);
            gBackstab[t][n++] = named(name);
        }
        for (int i = 3; i >= 0; i--) {
            snprintf(name, sizeof(name), "%s_spy_prepare_backstab%d", team, prepare[i]);
            gBackstab[t][n++] = named(name);
        }
        for (int i = 0; i < 5; i++) {
            snprintf(name, sizeof(name), "%s_spy_prepare_disguise%d", team, i + 1);
            gPrepareDisguise[t][i] = named(name);
        }
        snprintf(name, sizeof(name), "%s_sentry_turret", team);
        gTurret[t] = named(name);
        for (int i = 0; i < 8; i++) {
            snprintf(name, sizeof(name), "%s_sentry_assemble%d", team, i + 1);
            gAssemble[t][i] = named(name);
        }
    }
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "spy_cigarette_smoke%d", i + 1);
        gCigarette[i] = named(name);
    }
    for (int i = 0; i < 12; i++) {
        char name[32];
        snprintf(name, sizeof(name), "spy_disguise_smoke%d", i + 1);
        gDisguiseSmoke[i] = named(name);
    }
    gSentryLegs = named("sentry_legs");
    gBeam[1] = named("medic_healingTextureBlu");
    gBeam[2] = named("medic_healingTextureRed");
    gBarEmpty = named("redbar_empty");
    gBarFull = named("redbar_full");
    static const char *sawn[] = {"bullet_sawngun1", "bullet_sawngun2", "bullet_sawngun3"};
    static const char *rocket[] = {"bullet_rocket_launcher"};
    static const char *flame[] = {"bullet_flamethrower1", "bullet_flamethrower2", "bullet_flamethrower3",
                                  "bullet_flamethrower4"};
    static const char *grenade[] = {"bullet_grenade_launcher"};
    static const char *minigun[] = {"bullet_minigun1", "bullet_minigun2", "bullet_minigun3"};
    static const char *shotgun[] = {"bullet_shotgun1", "bullet_shotgun2", "bullet_shotgun3"};
    static const char *needle[] = {"bullet_needle_squirter"};
    static const char *sniper[] = {"bullet_sniper_rifle"};
    static const char *revolver[] = {"bullet_revolver1", "bullet_revolver2", "bullet_revolver3"};
    static const char *udder[] = {"bullet_uddergun_1", "bullet_uddergun_2", "bullet_uddergun_3"};
    static const char *colorful[] = {"bullet_colorful_flamethrower1", "bullet_colorful_flamethrower2",
                                     "bullet_colorful_flamethrower3", "bullet_colorful_flamethrower4",
                                     "bullet_colorful_flamethrower5", "bullet_colorful_flamethrower6"};
    static const char *apple[] = {"bullet_applelauncher_1", "bullet_applelauncher_2"};
    static const char *paper[] = {"bullet_paper_airplane_launcher"};
    init_weapon(FO_ITEM_SAWNGUN, "sawngun", "muzzleflash_sawngun", sawn, 3);
    init_weapon(FO_ITEM_ROCKET_LAUNCHER, "rocket_launcher", "muzzleflash_rocket_launcher", rocket, 1);
    init_weapon(FO_ITEM_FLAMETHROWER, "%s_flamethrower", NULL, flame, 4);
    init_weapon(FO_ITEM_GRENADE_LAUNCHER, "grenade_launcher", "muzzleflash_grenade_launcher", grenade, 1);
    init_weapon(FO_ITEM_MINIGUN, "minigun", "muzzleflash_minigun", minigun, 3);
    init_weapon(FO_ITEM_SHOTGUN, "shotgun", "muzzleflash_shotgun", shotgun, 3);
    init_weapon(FO_ITEM_NEEDLE_SQUIRTER, "%s_needle_squirter", "muzzleflash_needle_squirter", needle, 1);
    init_weapon(FO_ITEM_SNIPER_RIFLE, "sniper_rifle", "muzzleflash_sniper_rifle", sniper, 1);
    init_weapon(FO_ITEM_REVOLVER, "revolver", "muzzleflash_revolver", revolver, 3);
    init_weapon(FO_ITEM_UDDERGUN, "uddergun_1", NULL, udder, 3);
    init_weapon(FO_ITEM_COLORFUL_FLAMETHROWER, "colorful_flamethrower", NULL, colorful, 6);
    init_weapon(FO_ITEM_JUSTICE_REVOLVER, "justice_revolver", "muzzleflash_revolver", revolver, 3);
    init_weapon(FO_ITEM_APPLE_LAUNCHER, "applelauncher_3", "muzzleflash_applelauncher", apple, 2);
    init_weapon(FO_ITEM_PAPER_AIRPLANE, "paper_airplane_launcher", NULL, paper, 1);
    init_weapon(FO_ITEM_SENTRY, "", NULL, minigun, 3);
}

// Draws sprite with its centre at (cx, cy), mirrored when s is -1, and
// turned by deg degrees around the centre.
static void draw_centered(const FOSprite *sprite, float cx, float cy, int s, float deg, FOColor tint) {
    if (!sprite->w) return;
    float w = sprite->w * Z, h = sprite->h * Z;
    fo_gfx_draw(sprite, cx - w / 2, cy - h / 2, w, h, w / 2, h / 2, deg * 0.017453292f, s < 0, tint);
}

// The extra angle of the arm of each weapon (alz.a(boolean) overrides).
static float arm_offset(const FOWeapon *w, long long nowMs) {
    if (!w->def) return 0;
    switch (w->def->item) {
    case FO_ITEM_SAWNGUN:
    case FO_ITEM_GRENADE_LAUNCHER:
    case FO_ITEM_APPLE_LAUNCHER:
        return 20;
    case FO_ITEM_ROCKET_LAUNCHER:
    case FO_ITEM_REVOLVER:
    case FO_ITEM_JUSTICE_REVOLVER:
        return 90;
    case FO_ITEM_NEEDLE_SQUIRTER:
        return 10;
    case FO_ITEM_PAPER_AIRPLANE:
        return 25;
    case FO_ITEM_MINIGUN:
    case FO_ITEM_UDDERGUN: {
        // alf.a(10, 16, lastShot, k): the arm drops at each shot.
        float k = (float)(nowMs - w->lastShot) / w->def->interval;
        k = k < 0 ? 0 : k > 1 ? 1 : k;
        return 10 + k * 6;
    }
    default:
        return 0;
    }
}

// PlayerRenderData.a(...) (ada.java:665-732): the parts of one player,
// as the class cls of the team. f5 is the time of the start of a disguise,
// or -1.
static void draw_main(const FOWorld *world, const FOPlayer *p, FOLook *look, float now, float X, float Y, int cls,
                      int team, const FOWeapon *w, FOColor tint, float f5) {
    if (cls < 1 || cls > 9 || (team != 1 && team != 2)) return;
    const FOClassLook *l = &gLooks[cls][team];
    const FOClassData *d = &kData[cls];
    const FOArm *arm = fo_class_arm(cls);
    long long nowMs = world->now;
    float vx, vy;
    fo_player_velocity(p, &vx, &vy);
    float Y0 = Y - p->obj.h / 2;
    int s = p->facingRight ? 1 : -1;
    int grounded = fo_player_grounded(p);
    float landed = now - p->landTime / 1000.0f;

    // 3.3: the legs.
    const FOSprite *legs;
    if (fabsf(vx) < 0.3f && landed < 0.25f && vy < 1) {
        legs = &l->squat;
    } else if (!grounded || fabsf(vx) < 0.3f) {
        if (grounded) {
            look->S = -1;
            legs = &l->idle;
        } else if (vy > 2) {
            legs = &l->up;
        } else if (vy < -2) {
            legs = &l->down;
        } else {
            legs = &l->middle;
        }
    } else {
        float speed = fo_class_speed(cls);
        if (w->def && (float)(nowMs - w->lastShot) < w->def->window * 2.0f) speed *= w->def->afterShot;
        float dur = fabsf(speed * 0.01f - 0.1f) + 0.1f;
        int reversed = (vx > 0 && !p->facingRight) || (vx < 0 && p->facingRight);
        const FOSprite *frames = l->hasWalk && fabsf(vx) < 2 ? l->walk : l->run;
        int idx = (int)floorf(look->stateTime / dur) % 6;
        if (reversed) idx = 5 - idx;
        look->S = idx;
        legs = &frames[idx];
    }
    // bob() and idl().
    int bob;
    if (cls != FO_CLASS_SPY && cls != FO_CLASS_PYRO) {
        bob = look->S == 0 || look->S == 3;
    } else {
        bob = (look->S == 0 || look->S == 2 || look->S == 3 || look->S == 5) ? -1 : 0;
    }
    float bobM = bob * Z;
    // Body frame.
    const FOSprite *body = &l->body[0];
    if (grounded && fabsf(vx) < 0.5f && landed >= 0.25f) {
        look->D = look->stateTime;
        body = &l->body[(int)floorf(look->stateTime / 0.8f) % 2];
    }
    float idl = ((int)floorf(look->D / 0.8f) % 2 == 1) ? -Z : 0;

    float lw = legs->w * Z, lh = legs->h * Z;
    float xL = (legs->h - l->idle.h) * Z;
    float fw = l->body[0].w * Z, fh = l->body[0].h * Z;
    // 1. Legs.
    if (legs->w) fo_gfx_draw(legs, X - lw / 2 + s * d->legsX * Z, Y0 + bobM, lw, lh, lw / 2, lh / 2, 0, s < 0, tint);
    // 2. Body, or the frames of the start of a disguise of the spy.
    if (f5 < 0) {
        if (body->w) fo_gfx_draw(body, X - fw / 2, Y0 + lh + bobM + idl, fw, fh, fw / 2, fh / 2, 0, s < 0, tint);
    } else {
        int i = (int)floorf(f5 / 0.1f);
        if (i > 4) i = 4;
        const FOSprite *m = &gPrepareDisguise[team][i];
        if (m->w) {
            float mw = m->w * Z, mh = m->h * Z;
            fo_gfx_draw(m, X - mw / 2 + s * 1 * Z, Y0 + lh + bobM - 6 * Z, mw, mh, mw / 2, mh / 2, 0, s < 0, tint);
        }
    }
    // 3. Head.
    float hw = l->head.w * Z, hh = l->head.h * Z;
    if (l->head.w) {
        fo_gfx_draw(&l->head, X - hw / 2 + s * d->ex * Z, Y0 + lh + bobM + idl + d->ey * Z, hw, hh, hw / 2, hh / 2, 0,
                    s < 0, tint);
    }
    // 4. The top of the body of the spy.
    if (cls == FO_CLASS_SPY && l->bodytop.w) {
        float w = l->bodytop.w * Z, h = l->bodytop.h * Z;
        fo_gfx_draw(&l->bodytop, X - 0.25f + s * 1 * Z, Y0 + 12 * Z + bobM + idl + xL, w, h, w / 2, h / 2, 0, s < 0,
                    tint);
    }
    // 6. The smoke of the cigarette of the spy: 0.5 s, each 2.5 s.
    if (cls == FO_CLASS_SPY && f5 < 0) {
        float ts = now - look->smokeTime;
        if (ts < 0.5f) {
            const FOSprite *c = &gCigarette[(int)floorf(ts / 0.1f) % 5];
            if (c->w) {
                float cw = c->w * Z, ch = c->h * Z;
                fo_gfx_draw(c, X - 0.15f + s * 6 * Z, Y0 + 15 * Z + bobM + idl + xL, cw, ch, cw / 2, ch / 2, 0, s < 0,
                            FO_WHITE);
            }
        } else if (ts >= 2.5f) {
            look->smokeTime = now;
        }
    }
    // 7. The fire.
    if (p->burning) {
        const FOSprite *f = &gBurning[(int)floorf(look->stateTime / 0.1f) % 7];
        if (f->w) {
            float bw = f->w * Z, bh = f->h * Z;
            fo_gfx_draw(f, X - 0.4f + s * d->burnX * Z, Y0 + d->burnY * Z + bobM + idl, bw, bh, bw / 2, bh / 2, 0, s < 0,
                        FO_WHITE);
        }
    }
    // 8. The weapon (4.1).
    float A = w->angle;
    float ax = s > 0 ? (arm->jx + arm->h) : (fw - arm->jx - arm->h);
    float Px = X - fw / 2 + ax;
    float Py = Y0 + arm->jy + arm->ky + idl + bobM + xL;
    if (w->def && w->def->item < 64) {
        const FOSprite *ws = &gWeapons[w->def->item][team];
        float hx = w->def->hx, hy = w->def->hy;
        if (w->def->item == FO_ITEM_MINIGUN || w->def->item == FO_ITEM_UDDERGUN) {
            float k = (float)(nowMs - w->lastShot) / w->def->interval;
            k = k < 0 ? 0 : k > 1 ? 1 : k;
            hx = (6 + k) * Z;
        }
        float phi = A * 0.017453292f;
        if (!p->facingRight && A == 0) phi = 3.1415927f;
        float ox = hx, oy = s * hy;
        float cx = Px + (ox * cosf(phi) - oy * sinf(phi));
        float cy = Py + (oy * cosf(phi) + ox * sinf(phi));
        float rot = A + ((!p->facingRight && A != 0) ? 180 : 0);
        if (w->def->kind == FO_SHOT_HITSCAN && A != 0 && ws->w) {
            // SniperRifle.a(ada) (alw.java:117-145): the rifle at the eye.
            float bx, by;
            fo_player_position(p, &bx, &by);
            float rw = ws->w * Z, rh = ws->h * Z;
            float f = (s > 0 ? 0.6f : 0.3f) + arm->h + arm->jx;
            if (s < 0) f = fw - f;
            float fB = (s > 0 ? 0.3f : -0.1f) + arm->jy + idl + bobM;
            float qx = bx - fw / 2 + f, qy = by - 0.95f + arm->ky + fB;
            float a2 = rw / 2 - 0.5f, b2 = rh / 2 - 0.5f;
            if (s < 0) b2 += 0.3f;
            cx = qx + (a2 * cosf(phi) - b2 * sinf(phi)) - a2;
            cy = qy + (s > 0 ? 0 : 0.6f) + (a2 * sinf(phi) + b2 * cosf(phi)) - b2 + xL;
            // SniperRifle.g(): the scope is (6.5, -0.5) px from the centre.
            float sy = s > 0 ? -0.5f * Z : 0.5f * Z;
            look->scopeX = cx + (6.5f * Z * cosf(phi) - sy * sinf(phi));
            look->scopeY = cy + (sy * cosf(phi) + 6.5f * Z * sinf(phi));
            look->scopeArm = (s > 0 ? 90 : -90) +
                             57.295776f * atan2f((by + arm->ky) - ((cy - rh / 2) + 0.2f), (s * arm->kx + bx) - (cx - s * rw / 2));
        }
        draw_centered(ws, cx, cy, s, rot, tint);
        // The muzzle flash: 20 ms after a shot, only while it aims.
        const FOSprite *flash = &gFlashes[w->def->item];
        if (flash->w && nowMs - w->lastShot < 20 && A != 0) {
            float Pmy = Y0 + arm->jy + arm->ky + idl + bobM;
            float fx = w->def->xx, fy = s * w->def->xy;
            float a = A * 0.017453292f;
            float mx = Px + (fx * cosf(a) - fy * sinf(a)), my = Pmy + (fy * cosf(a) + fx * sinf(a));
            float fwm = flash->w * Z, fhm = flash->h * Z;
            // Scale (1, s): a flip of y is a turn of 180 degrees and a flip of x.
            if (s > 0) {
                fo_gfx_draw(flash, mx - fwm / 2, my - fhm / 2, fwm, fhm, fwm / 2, fhm / 2, a, 0, FO_WHITE);
            } else {
                fo_gfx_draw(flash, mx - fwm / 2, my - fhm / 2, fwm, fhm, fwm / 2, fhm / 2, a + 3.1415927f, 1, FO_WHITE);
            }
        }
    }
    // 9. The left arm, turned around its pivot.
    if (l->arm.w) {
        float armRot = 0;
        if (w->def) {
            if (w->def->kind == FO_SHOT_HITSCAN && A != 0) {
                armRot = look->scopeArm;
            } else {
                armRot = A + (p->facingRight ? arm_offset(w, nowMs) : -arm_offset(w, nowMs));
            }
            if (!p->facingRight && A != 0) armRot -= 180;
        }
        float aw = arm->h, ah = arm->i;
        float kx = arm->kx, ky = arm->ky;
        // With scale (s, 1), the pivot of a mirrored sprite is at w - kx.
        float ox = s > 0 ? kx : aw - kx;
        fo_gfx_draw(&l->arm, Px - ox, Py - ky, aw, ah, ox, ky, armRot * 0.017453292f, s < 0, tint);
    }
}

// The alpha of the cloak (xn.java:419-449): the enemies see nothing and
// the teammates see 0.3, with a fade of 0.3 s.
static float cloak_alpha(const FOWorld *world, const FOPlayer *p, const FOPlayer *viewer) {
    float f = viewer && viewer->team == p->team ? 0.3f : 0.0f;
    float k = (world->now - p->cloakTime) / 300.0f;
    k = k < 0 ? 0 : k > 1 ? 1 : k;
    if (p->cls == FO_CLASS_SPY && p->cloaked) return 1 + (f - 1) * k;
    if (p->cloakTime == 0) return 1;
    return f + (1 - f) * k;
}

// The backstab of the spy (ada.java:854-888): one frame holds the whole body.
static void draw_backstab(const FOWorld *world, const FOPlayer *p, FOLook *look, float now, FOColor tint) {
    float bx, by;
    fo_player_position(p, &bx, &by);
    int s = p->facingRight ? 1 : -1;
    int idx = (int)floorf((now - p->backstabTime / 1000.0f) / (1.1f / 15));
    if (idx > 14) idx = 14;
    if (idx < 0) idx = 0;
    const FOSprite *fr = &gBackstab[p->team][idx];
    if (fr->w) {
        float w = fr->w * Z, h = fr->h * Z;
        float dx = s > 0 ? -5 * Z : -8 * Z;
        fo_gfx_draw(fr, bx - p->obj.w / 2 + dx, by - p->obj.h / 2, w, h, w / 2, h / 2, 0, s < 0, tint);
    }
    look->S = -1;
    look->D = 0;
    (void)world;
}

void fo_look_player(const FOWorld *world, const FOPlayer *p, FOLook *look, float now, float t,
                    const FOPlayer *viewer) {
    if (p->cls < 1 || p->cls > 9 || (p->team != 1 && p->team != 2)) return;
    float bx, by, vx, vy;
    fo_player_position(p, &bx, &by);
    fo_player_velocity(p, &vx, &vy);
    // The render place: the body moved by the part of a step that waits.
    float X = bx + vx * t, Y = by + vy * t + 0.5f * -15.0f * t * t;
    FOColor tint = FO_WHITE;
    // A player in uber is red or blue (xn.java:458-462).
    if (fo_player_protected(world, p)) tint = p->team == FO_TEAM_RED ? fo_rgb(0xff0000) : fo_rgb(0x0000ff);
    tint.a = cloak_alpha(world, p, viewer);
    if (tint.a <= 0) return;
    float since = p->disguise.active ? (world->now - p->disguise.start) / 1000.0f : 0;
    if (p->cls == FO_CLASS_SPY && p->backstab && now - p->backstabTime / 1000.0f < 1.1f) {
        draw_backstab(world, p, look, now, tint);
    } else if (p->cls == FO_CLASS_SPY && p->disguise.active && since > 1.0f) {
        if (viewer && viewer->team == p->team && viewer->id != p->id) {
            draw_main(world, p, look, now, X, Y, p->cls, p->team, &p->weapon, tint, -1);
        } else {
            // The enemies see the target: its class, its team, and its weapon.
            const FOPlayer *target = fo_world_player((FOWorld *)world, p->disguise.target);
            FOWeapon w = p->weapon;
            w.def = target && target->cls == p->disguise.cls ? target->weapon.def : fo_weapon_default(p->disguise.cls);
            draw_main(world, p, look, now, X, Y, p->disguise.cls, p->disguise.team, &w, tint, -1);
        }
    } else {
        float f5 = p->disguise.active && since < 0.5f ? since : -1;
        draw_main(world, p, look, now, X, Y, p->cls, p->team, &p->weapon, tint, f5);
    }
    // The smoke of a disguise, from 0.5 s to 1.4 s.
    if (p->disguise.active && since > 0.5f && since - 0.5f < 0.9f) {
        const FOSprite *sm = &gDisguiseSmoke[(int)floorf((since - 0.5f) / (0.9f / 12)) % 12];
        if (sm->w) {
            float w = sm->w * Z, h = sm->h * Z;
            fo_gfx_draw(sm, bx - w / 2, by - 0.95f, w, h, w / 2, h / 2, 0, 0, tint);
        }
    }
}

// The beam of each medic to its target (xn.java:450-457).
void fo_look_beams(const FOWorld *world) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *m = &world->players[i];
        if (!m->used || !m->alive || m->cls != FO_CLASS_MEDIC || !m->healTarget) continue;
        const FOPlayer *t = fo_world_player((FOWorld *)world, m->healTarget);
        if (!t || fo_player_waiting(t) || !t->alive) continue;
        float mx, my, tx, ty;
        fo_player_position(m, &mx, &my);
        fo_player_position(t, &tx, &ty);
        float cx = (mx + tx) / 2, cy = (my + ty) / 2;
        float len = sqrtf((tx - mx) * (tx - mx) + (ty - my) * (ty - my));
        const FOSprite *b = &gBeam[m->team == FO_TEAM_RED ? 2 : 1];
        if (b->w) fo_gfx_draw(b, cx - len / 2, cy - 0.125f, len, 0.25f, len / 2, 0.125f, atan2f(ty - my, tx - mx), 0, FO_WHITE);
    }
}

// A local medic sees the health of each teammate (xn.java:493-498).
void fo_look_health_bars(const FOWorld *world, const FOPlayer *viewer) {
    if (!viewer || viewer->cls != FO_CLASS_MEDIC || !gBarEmpty.w) return;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *p = &world->players[i];
        if (!p->used || !p->alive || p->id == viewer->id || p->team != viewer->team) continue;
        float x, y;
        fo_player_position(p, &x, &y);
        // ak: the height of the name, about 0.35 m.
        float by = y + p->obj.h / 2 + 0.35f - 0.3f;
        fo_gfx_draw(&gBarEmpty, x - 0.75f, by, 1.5f, 0.2f, 0, 0, 0, 0, FO_WHITE);
        fo_gfx_draw(&gBarFull, x - 0.75f, by, 1.5f * p->health / fo_class_health(p->cls), 0.2f, 0, 0, 0, 0, FO_WHITE);
    }
}

// SentryGun.a(alh) (xu.java): the legs and the turret, or the build.
void fo_look_sentries(const FOWorld *world, float now) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOSentry *se = &world->players[i].sentry;
        if (!se->active || (se->team != 1 && se->team != 2)) continue;
        float x, y;
        fo_phys_body_position(se->obj.body, &x, &y);
        int s = se->facingRight ? 1 : -1;
        if (fo_sentry_is_ready(world, se)) {
            const FOSprite *legs = &gSentryLegs;
            if (legs->w) {
                float lw = legs->w * Z, lh = legs->h * Z;
                fo_gfx_draw(legs, x - lw / 2, y - lh / 2 - 0.2f, lw, lh, lw / 2, lh / 2, 0, s < 0, FO_WHITE);
            }
            // The turret: 1.7 x 1.2 m from its corner, origin (0.5, 0.6), flipped
            // in y when it points to the left.
            const FOSprite *tu = &gTurret[se->team];
            float deg = fmodf(fmodf(se->aim * 57.295776f, 360) + 360, 360);
            int flipY = deg > 90 && deg < 270;
            float tx = (x - 1.7f / 2) + 0.4f, ty = (y - 1.2f / 2) + 0.6f - 0.2f;
            if (tu->w) {
                if (!flipY) {
                    fo_gfx_draw(tu, tx, ty, 1.7f, 1.2f, 0.5f, 0.6f, se->aim, 0, FO_WHITE);
                } else {
                    // A flip of y is a flip of x and a half turn.
                    fo_gfx_draw(tu, tx, ty, 1.7f, 1.2f, 0.5f, 0.6f, se->aim + 3.1415927f, 1, FO_WHITE);
                }
            }
        } else {
            int frame = se->built == 0 ? 0 : (int)floorf((now - se->built / 1000.0f) / (0.8f / 8));
            if (frame > 7) frame = 7;
            const FOSprite *f = &gAssemble[se->team][frame];
            if (f->w) {
                float w = f->w * Z, h = f->h * Z;
                float ex = se->facingRight ? 0.15f : -0.15f;
                fo_gfx_draw(f, x - w / 2 + ex, 0.1f + y - h / 2, w, h, w / 2, h / 2, 0, s < 0, FO_WHITE);
            }
        }
    }
}

void fo_look_dead(const FOWorld *world, const FOPlayer *p, float now) {
    const FOCorpse *c = &p->corpse;
    if (!c->active || p->cls < 1 || p->cls > 9 || (p->team != 1 && p->team != 2)) return;
    const FOClassLook *l = &gLooks[p->cls][p->team];
    const FOClassData *d = &kData[p->cls];
    float start = c->start / 1000.0f;
    int i = (int)floorf((now - start) / 0.1f);
    if (i > 6) i = 6;
    if (i >= 4) {
        if (c->stop == 0) {
            i = 4;
        } else {
            i = (int)floorf((0.5f + now - c->stop / 1000.0f) / 0.1f);
            if (i > 6) i = 6;
            if (i < 4) i = 4;
        }
    }
    const FOSprite *f = &l->death[i];
    if (!f->w) return;
    float alpha = 1 - (now - (start + 8.5f)) / 0.5f;
    alpha = alpha < 0 ? 0 : alpha > 1 ? 1 : alpha;
    float bx, by;
    fo_phys_body_position(c->obj.body, &bx, &by);
    int s = c->facingRight ? 1 : -1;
    float w = f->w * Z, h = f->h * Z;
    float k = c->turnStart ? (float)(world->now - c->turnStart) / c->turnTime : 0;
    k = k < 0 ? 0 : k > 1 ? 1 : k;
    FOColor tint = {1, 1, 1, alpha};
    fo_gfx_draw(f, bx - w / 2 + s * d->deathX * Z, by - h / 2 + d->deathY * Z, w, h, w / 2, h / 2,
                k * c->turn * 0.017453292f, s < 0, tint);
}

void fo_look_bullets(const FOWorld *world, float now) {
    for (int i = 0; i < FO_MAX_BULLETS; i++) {
        const FOBullet *b = &world->bullets[i];
        if (!b->active || !b->def || b->def->item >= 64 || b->def->kind == FO_SHOT_HITSCAN) continue;
        int item = b->def->item;
        int frames = gBulletFrames[item];
        if (!frames) continue;
        // The projectile animations run at 7 frames each second (1/7 s).
        int frame = frames > 1 ? (int)floorf((now - b->spawn / 1000.0f) / 0.14285715f) % frames : 0;
        const FOSprite *s = &gBullets[item][frame];
        if (!s->w) continue;
        // The projectile fades in the last tenth of its life.
        float life = (float)b->def->life;
        float end = b->spawn + life * 0.9f;
        float alpha = 1;
        if (world->now > end) {
            alpha = 1 - (float)(world->now - end) / (life / 10.0f);
            if (alpha < 0) alpha = 0;
        }
        float x, y;
        fo_phys_body_position(b->obj.body, &x, &y);
        float w = s->w * Z, h = s->h * Z;
        float c = cosf(b->angle), sn = sinf(b->angle);
        float px = x - w / 2 + (b->def->jx * c - b->def->jy * sn);
        float py = y - h / 2 + (b->def->jx * sn + b->def->jy * c);
        FOColor tint = {1, 1, 1, alpha};
        fo_gfx_draw(s, px, py, w, h, w / 2, h / 2, b->angle, 0, tint);
    }
}

void fo_look_pickups(const FOWorld *world) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPickup *k = &world->players[i].pickup;
        if (!k->active || !k->def || k->def->item >= 64) continue;
        const FOSprite *s = &gWeapons[k->def->item][k->team == FO_TEAM_RED ? 2 : 1];
        if (!s->w) continue;
        float x, y;
        fo_phys_body_position(k->obj.body, &x, &y);
        float w = s->w * Z, h = s->h * Z;
        fo_gfx_draw(s, x - w / 2, y - h / 2, w, h, w / 2, h / 2, fo_phys_body_angle(k->obj.body), 0, FO_WHITE);
    }
}
