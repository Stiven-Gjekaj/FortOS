#include "fo_hud.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fo_look.h"

static const FOAtlasTextures *gSprites;
static const FOFont *gFont;
static const FOFont *gFont6, *gFont12, *gFont21;
static const FOText *gTexts;
static float gW, gH, gU;
// The part of the world on the screen, for the arrows of the flags.
static float gViewX, gViewY, gViewW, gViewH;
// The round-over dialog fades in for 0.3 s (ajl.java:300).
static float gRoundTime;
// The alpha of the hint of the bomb (act.java:265-270).
static float gHintAlpha;
static char gMapName[32];
// The kill cam shows: the name of the killer, or "" with no kill cam.
static char gKillcam[32];

// WeaponInfoHUD: the shown health and ammo fall with a lerp of 0.2 each
// frame, and rise at once (ajt.java:99-121).
static float gShownHealth, gShownAmmo;

// DeathList: three rows, the newest at the end.
typedef struct {
    char target[32];
    short targetTeam;
    int item;
    int related;
    int suicide;
    float age;
} FOKillRow;

static FOKillRow gRows[3];
static int gRowCount;
static float gListTime;   // The time since the last kill. The list hides 5 s after it.

void fo_hud_init(const FOAtlasTextures *sprites, const FOHudFonts *fonts, const FOText *texts, int width, int height) {
    gSprites = sprites;
    gFont = fonts->pixel4;
    gFont6 = fonts->pixel6;
    gFont12 = fonts->pixel12;
    gFont21 = fonts->pixel21;
    gTexts = texts;
    gW = (float)width;
    gH = (float)height;
    gU = width / 281.0f;
    gRowCount = 0;
    gListTime = 99;
}

static FOSprite sprite(const char *name) {
    return fo_assets_sprite(gSprites, name);
}

static FOSprite team_sprite(short team, const char *name) {
    char full[64];
    snprintf(full, sizeof(full), "%s_%s", team == FO_TEAM_RED ? "red" : "blu", name);
    return sprite(full);
}

// EasyClippableImage (aif.java): the left part of the image, clipped to the
// width shown.
static void draw_clipped(const FOSprite *s, float x, float y, float w, float h, float shown) {
    if (!s->w || shown <= 0) return;
    if (shown > w) shown = w;
    FOSprite part = *s;
    part.u1 = s->u0 + (s->u1 - s->u0) * (shown / w);
    fo_gfx_draw(&part, x, y, shown, h, 0, 0, 0, 0, FO_WHITE);
}

static void draw_stretched(const FOSprite *s, float x, float y, float w, float h) {
    if (s->w) fo_gfx_draw(s, x, y, w, h, 0, 0, 0, 0, FO_WHITE);
}

// A Label of the smallest font with its box at (x, y). The text fills the
// box from its top.
static void label(const char *text, float x, float y, float h, FOColor color) {
    fo_font_draw(gFont, text, x, y + h, gU, color, FO_ALIGN_LEFT);
}

static void draw_weapon_info(const FOPlayer *p) {
    float u = gU;
    float healthW = 65 * u, healthH = 10 * u, menuW = 16 * u;
    float x = (menuW + u + ((gW / 2 - 40 * u - u - menuW) / 2)) - healthW / 2;
    float y = gH - healthH;
    short team = p->team;
    // The health bar.
    float health = p->health, max = fo_class_health(p->cls);
    if (health > gShownHealth) gShownHealth = health;
    gShownHealth += (health - gShownHealth) * 0.2f;
    FOSprite empty = team_sprite(team, "weaponinfo_health_empty"), full = team_sprite(team, "weaponinfo_health_full");
    draw_stretched(&empty, x, y, healthW, healthH);
    // The ammo bar.
    float ammoW = 60 * u, ammoH = 5 * u;
    float ax = x + 4 * u, ay = y - ammoH;
    FOSprite aempty = team_sprite(team, "weaponinfo_ammo_empty"), afull = team_sprite(team, "weaponinfo_ammo_full");
    draw_stretched(&aempty, ax, ay, ammoW, ammoH);
    float shown = 1;
    if (p->weapon.def && p->weapon.ammo >= 0.5f) {
        float ammo = p->weapon.ammo;
        if (ammo > gShownAmmo) gShownAmmo = ammo;
        gShownAmmo += (ammo - gShownAmmo) * 0.2f;
        shown = gShownAmmo / p->weapon.def->ammo * ammoW * 56 / 60;
    }
    // The label "ammo" on its background.
    const char *ammoText = fo_text_get(gTexts, "ammo");
    float lw = fo_font_width(gFont, ammoText, u), lh = fo_font_line(gFont, u);
    float lx = ax + 7 * u, ly = ay - lh + u;
    FOSprite bg = team_sprite(team, "weaponinfo_bgcolor");
    draw_stretched(&bg, lx + lw / 2 - (lw + 2 * u) / 2, ly - u, lw + 2 * u, lh + 2 * u);
    label(ammoText, lx, ly, lh, fo_rgb(0xffb866));
    // The third bar (ajt.java:124-147): the cloak of the agent, the steel
    // of the builder, and the uber of the dealer.
    const char *key = NULL;
    float value = 0, top = 100;
    if (p->cls == FO_CLASS_SPY) {
        key = "cloak";
        value = p->cloakEnergy;
    } else if (p->cls == FO_CLASS_ENGINEER) {
        key = "steel";
        value = p->steel;
    } else if (p->cls == FO_CLASS_MEDIC) {
        key = "rapidrush";
        value = p->uberCharge;
        top = 20;
    }
    if (key) {
        float tw = 56 * u, th = 6 * u;
        float tx = x + 8 * u, ty = gH - healthH - ammoH - th;
        FOSprite tempty = team_sprite(team, "weaponinfo_thirdbar_empty");
        FOSprite tfull = team_sprite(team, "weaponinfo_thirdbar_full");
        draw_stretched(&tempty, tx, ty, tw, th);
        draw_clipped(&tfull, tx, ty, tw, th, value < 0.5f ? 1 : value / top * tw);
        const char *text = fo_text_get(gTexts, key);
        float mw = fo_font_width(gFont, text, u), mh = fo_font_line(gFont, u);
        float mx = tx + tw - mw - 3 * u, my = ty - mh;
        draw_stretched(&bg, mx - 1.5f * u, my - u, mw + 2 * u, mh + u);
        label(text, mx, my, mh, fo_rgb(0xffb866));
    }
    // The fills of the ammo bar and the health bar come last, as the actors
    // p and o of ajt.java. The third bar covers the label "ammo".
    draw_clipped(&afull, ax, ay, ammoW, ammoH, shown + ammoW * 4 / 60);
    draw_clipped(&full, x, y, healthW, healthH, gShownHealth / max * healthW * 55 / 65 + healthW * 10 / 65);
}

// DeathList.a(...) (aic.java:76-107): a new row for each kill.
void fo_hud_events(const FOWorld *world, int localId, float dt) {
    gListTime += dt;
    for (int i = 0; i < gRowCount; i++) gRows[i].age += dt;
    for (int i = 0; i < world->event_count; i++) {
        const FOEvent *e = &world->events[i];
        // DeathList.a(...) (aic.java:90-91): a kill with no weapon, as a
        // death with no damage, makes no row.
        if (e->kind != FO_EVENT_KILL || e->item == 0) continue;
        const FOPlayer *target = NULL;
        for (int k = 0; k < FO_MAX_PLAYERS; k++) {
            if (world->players[k].used && world->players[k].id == e->b) target = &world->players[k];
        }
        if (!target) continue;
        if (gRowCount == 3) {
            memmove(&gRows[0], &gRows[1], sizeof(FOKillRow) * 2);
            gRowCount = 2;
        }
        FOKillRow *r = &gRows[gRowCount++];
        memset(r, 0, sizeof(*r));
        snprintf(r->target, sizeof(r->target), "%s", target->name);
        r->targetTeam = target->team;
        r->item = e->item;
        r->related = e->a == localId || e->b == localId || e->c == localId;
        r->suicide = e->a == e->b;
        gListTime = 0;
    }
}

static const char *icon_name(int item) {
    switch (item) {
    case 31: return "sawngun";
    case 32: return "rocket_launcher";
    case 33: case 47: return "flamethrower";
    case 34: case 50: return "grenade_launcher";
    case 35: case 46: return "minigun";
    case 36: return "shotgun";
    case 37: case 51: return "needle_squirter";
    case 38: case 49: return "sniper_rifle";
    case 39: case 48: return "revolver";
    case 40: return "sentry";
    case 41: return "spyknife";
    default: return NULL;
    }
}

static void draw_kill_feed(void) {
    if (gListTime > 5.0f || gRowCount == 0) return;
    float d = 8 * gU, c = gU, e = gU;
    float top = gH;
    float listH = 30 + 3 * d;
    for (int i = 0; i < gRowCount; i++) {
        const FOKillRow *r = &gRows[i];
        float alpha = r->age / 0.3f;
        if (alpha > 1) alpha = 1;
        char name[64];
        const char *icon = icon_name(r->item);
        if (r->suicide || !icon) {
            snprintf(name, sizeof(name), "%s_killicon_suicide", r->related ? "related" : "unrelated");
        } else {
            snprintf(name, sizeof(name), "%s_killicon_%s", r->related ? "related" : "unrelated", icon);
        }
        FOSprite ic = sprite(name);
        if (!ic.w) continue;
        // The text is 0.75 of the smallest font.
        float scale = 0.75f * gU;
        float textW = fo_font_width(gFont, r->target, scale);
        float iconH = d - c * 2, iconW = iconH * ic.w / ic.h;
        float rowW = iconW + c * 3 + textW + e * 2;
        float rowX = gW - rowW - 10;
        float rowY = top - listH + (listH - (d + 10) * (i + 1));
        FOSprite bg = sprite(r->related ? "deathlist_related_bg" : "deathlist_unrelated_bg");
        FOColor bgTint = {1, 1, 1, alpha};
        if (bg.w) {
            fo_gfx_draw(&bg, rowX + e, rowY, rowW - e * 2, d, 0, 0, 0, 0, bgTint);
            fo_gfx_draw(&bg, rowX, rowY + e, e, d - e * 2, 0, 0, 0, 0, bgTint);
            fo_gfx_draw(&bg, rowX + rowW - e, rowY + e, e, d - e * 2, 0, 0, 0, 0, bgTint);
        }
        fo_gfx_draw(&ic, rowX + c + c, rowY + c, iconW, iconH, 0, 0, 0, 0, bgTint);
        FOColor color = r->targetTeam == FO_TEAM_RED ? fo_rgba(0xa94d43, alpha) : fo_rgba(0x627283, alpha);
        float textH = fo_font_line(gFont, scale);
        fo_font_draw(gFont, r->target, rowX + c * 3 + iconW, rowY + d / 2 + textH, scale, color, FO_ALIGN_LEFT);
    }
}

// DeathRespawnMessage (aie.java) and GameScreen (aeb.java:399-414).
static void draw_respawn(const FOWorld *world, const FOPlayer *p) {
    if (p->alive || !p->deathTime) return;
    int seconds = (int)ceilf((10000 - (float)(world->now - p->deathTime)) / 1000.0f);
    if (seconds < 1) return;
    char text[128];
    fo_text_format(fo_text_get(gTexts, "respawn_text"), seconds, text, sizeof(text));
    float h = fo_font_line(gFont, gU);
    fo_font_draw(gFont, text, gW / 2, (gH * 25 / 150) / 2 + h / 2, gU, FO_WHITE, FO_ALIGN_CENTER);
}

// mm:ss of a time in seconds.
static void clock_text(char *out, size_t size, int seconds) {
    if (seconds < 0) seconds = 0;
    int m = seconds % 86400 % 3600 / 60, sec = seconds % 86400 % 3600 % 60;
    snprintf(out, size, "%02d:%02d", m, sec);
}

// A timer image of 40 x 16 units with its time in the middle.
static void draw_timer(const char *image, float x, int seconds) {
    float u = gU, w = 40 * u, h = 16 * u;
    FOSprite s = sprite(image);
    if (s.w) fo_gfx_draw(&s, x, gH - h - u, w, h, 0, 0, 0, 0, FO_WHITE);
    char text[16];
    clock_text(text, sizeof(text), seconds);
    // The label box is half a unit higher than the image (ahk.java:37).
    float lh = fo_font_line(gFont, u);
    float y = gH - h - u / 2;
    fo_font_draw(gFont, text, x + w / 2, y + h / 2 + lh / 2, u, FO_WHITE, FO_ALIGN_CENTER);
}

// The screen point of a world point.
static void to_screen(float x, float y, float *sx, float *sy) {
    *sx = (x - gViewX) / gViewW * gW;
    *sy = (y - gViewY) / gViewH * gH;
}

// CtfHUD (ahy.java): the captures of each team and an arrow to each flag.
static void draw_ctf(const FOWorld *world) {
    const FOMode *m = &world->mode;
    float u = gU;
    float redHomeX = m->flags[FO_TEAM_RED].homeX, bluHomeX = m->flags[FO_TEAM_BLU].homeX;
    int redLeft = redHomeX < bluHomeX;
    FOSprite bg = sprite(redLeft ? "ctf_hud_redblue" : "ctf_hud_bluered");
    if (!bg.w) return;
    float bw = bg.w * u, bh = bg.h * u;
    float bx = gW / 2 - bw / 2, by = gH - bh - u;
    fo_gfx_draw(&bg, bx, by, bw, bh, 0, 0, 0, 0, FO_WHITE);
    float leftX = bx + 14 * u, rightX = bx + 33 * u, textY = by + 17 * u;
    float arrowLX = bx + 13.5f * u, arrowRX = bx + bw - 13.5f * u, arrowY = by + 7.5f * u;
    float redX = redLeft ? leftX : rightX, bluX = redLeft ? rightX : leftX;
    float redArrow = redLeft ? arrowLX : arrowRX, bluArrow = redLeft ? arrowRX : arrowLX;
    char text[16];
    float lh = fo_font_line(gFont, u);
    snprintf(text, sizeof(text), "%d/4", m->redScore);
    float rw = fo_font_width(gFont, text, u);
    fo_font_draw(gFont, text, redX - rw / 2, textY + lh / 2, u, FO_WHITE, FO_ALIGN_LEFT);
    // The original places the BLU label with the width of the RED label.
    snprintf(text, sizeof(text), "%d/4", m->bluScore);
    fo_font_draw(gFont, text, bluX - rw / 2, textY + lh / 2, u, FO_WHITE, FO_ALIGN_LEFT);
    float r = 8.5f * u;
    const char *names[3] = {"", "ctf_hud_blue_arrow", "ctf_hud_red_arrow"};
    for (int t = FO_TEAM_BLU; t <= FO_TEAM_RED; t++) {
        float fx, fy, sx, sy;
        fo_phys_body_position(m->flags[t].obj.body, &fx, &fy);
        to_screen(fx, fy, &sx, &sy);
        // The original aims both arrows from the place of the RED arrow.
        float a = atan2f(sy - arrowY, sx - redArrow);
        float cx = t == FO_TEAM_RED ? redArrow : bluArrow;
        FOSprite s = sprite(names[t]);
        if (s.w) fo_gfx_draw(&s, cx + r * cosf(a), arrowY + r * sinf(a), s.w * u, s.h * u, 0, 0, a, 0, FO_WHITE);
    }
}

// The objective HUD at the top middle of the screen.
static void draw_objective(const FOWorld *world, const FOPlayer *local) {
    const FOMode *m = &world->mode;
    float u = gU;
    if (m->kind == FO_MODE_BOMB) {
        // BombHUD (ahk.java): the timer in the color of the local team.
        int blu = local && local->team == FO_TEAM_BLU;
        draw_timer(blu ? "timer_blu" : "timer_red", gW / 2 - 20 * u, (int)ceilf(m->bombTimer));
    } else if (m->kind == FO_MODE_KOTH) {
        // KothHUD (aiy.java): the RED timer and the BLU timer side by side.
        float w = 40 * u;
        draw_timer("timer_red", gW / 2 - (w + u + w) / 2, (int)m->redTimer);
        draw_timer("timer_blu", gW / 2 + u / 2, (int)m->bluTimer);
    } else if (m->kind == FO_MODE_CTF) {
        draw_ctf(world);
    }
}

void fo_hud_killcam(const char *killer) {
    snprintf(gKillcam, sizeof(gKillcam), "%s", killer ? killer : "");
}

void fo_hud_map(const char *name) {
    snprintf(gMapName, sizeof(gMapName), "%s", name);
}

void fo_hud_view(float left, float bottom, float width, float height) {
    gViewX = left;
    gViewY = bottom;
    gViewW = width;
    gViewH = height;
}

void fo_hud_draw(const FOWorld *world, const FOPlayer *local, float dt) {
    (void)dt;
    // GameScreen.e() (aeb.java:636-668): the bars hide while the local player
    // is dead, at the end of a round, and while the bomb is down. The
    // objective hides under the round-over dialog.
    int over = fo_mode_over(world);
    if (local && local->cls && local->alive && !over && !fo_mode_bomb_down(world)) draw_weapon_info(local);
    if (!over) draw_objective(world, local);
    // GameScreen.e() (aeb.java:675-680): the kill feed hides in the kill cam,
    // and the bars of the kill cam cover the objective.
    if (gKillcam[0]) fo_hud_death_cam(gKillcam);
    else draw_kill_feed();
    if (local) draw_respawn(world, local);
}

// RollableBomb.a(alh) (act.java:262-301): under the bomb, the local pusher
// reads that two teammates must push it uphill. Call it with the view of
// the world.
void fo_hud_bomb_hint(const FOWorld *world, float dt) {
    const FOMode *m = &world->mode;
    if (m->kind != FO_MODE_BOMB) return;
    float target = m->bomb.hint ? 1.0f : 0.0f;
    gHintAlpha += (target - gHintAlpha) * (dt * 3.0f);
    if (fabsf(gHintAlpha - target) < 0.005f) gHintAlpha = target;
    if (gHintAlpha <= 0) return;
    float x, y;
    fo_phys_body_position(m->bomb.obj.body, &x, &y);
    // The glyphs are 0.1 m for each font pixel, times 0.75 (act.java:94-98).
    float a = 0.075f;
    FOColor c = {1, 1, 1, gHintAlpha};
    float top = y - m->bomb.obj.h / 2 - 0.4f;
    fo_font_draw(gFont, fo_text_get(gTexts, "uphill_hint_text1"), x, top, a, c, FO_ALIGN_CENTER);
    fo_font_draw(gFont, fo_text_get(gTexts, "uphill_hint_text2"), x, top - 0.4f, a, c, FO_ALIGN_CENTER);
}

// Collections.sort keeps the order of the list for equal scores
// (ajl.java:260-261). qsort does not, so the place in the list decides.
static int by_score(const void *a, const void *b) {
    const FOPlayer *pa = *(const FOPlayer *const *)a, *pb = *(const FOPlayer *const *)b;
    if (pb->score != pa->score) return pb->score - pa->score;
    return pa < pb ? -1 : pa > pb;
}

static void draw_box(const char *name, float x, float y, float w, float h, float alpha) {
    FOSprite s = sprite(name);
    FOColor c = {1, 1, 1, alpha};
    if (s.w) fo_gfx_draw(&s, x, y, w, h, 0, 0, 0, 0, c);
}

static FOColor faded(unsigned int hex, float alpha) {
    return fo_rgba(hex, alpha);
}

// The runners of the round-over dialog (RoundOverDialog.b and .a,
// ajl.java:403-816): the winners run after the losers on the bottom bar
// and shoot at the sky.
#define FO_RUNNERS FO_MAX_PLAYERS
#define FO_RUNNER_SHOTS 256

typedef struct {
    FORunner r;
    FOLook look;
    float x;          // F.x or E.x: the middle on the screen.
    float K, N, O;    // The aim, and the two ends of its swing.
    int rising;       // E: the aim goes up.
    float lastShot;   // L
    int shots;
} FOHudRunner;

typedef struct {
    int cls;
    float start;
    float x, y;       // b: the muzzle on the screen.
    float vx, vy;     // c: m/s.
    float gravity;
    int life;         // The flames go after their life.
} FOHudShot;

static FOHudRunner gRunners[FO_RUNNERS];
static int gRunnerCount;
static FOHudShot gShots[FO_RUNNER_SHOTS];
static int gShotCount;

static float hud_rand(float lo, float hi) {
    return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}

// RoundOverDialog.a(wt, int, xn) (ajl.java:274-288): the winners run to
// the side of their own fridge when blu wins, else to the other side.
static void runners_start(const FOWorld *world, int winner) {
    gRunnerCount = 0;
    gShotCount = 0;
    const FORect *red = fo_tmx_area(&world->map, FO_AREA_FRIDGE_RED);
    const FORect *blu = fo_tmx_area(&world->map, FO_AREA_FRIDGE_BLU);
    int z2 = red && blu ? blu->x > red->x : 1;
    int right = winner == FO_TEAM_BLU ? z2 : !z2;
    float J = 19 * gH / 150, I = J * 0.90000004f / 1.9f;
    for (int i = 0; i < FO_MAX_PLAYERS && gRunnerCount < FO_RUNNERS; i++) {
        const FOPlayer *p = &world->players[i];
        if (!p->used || p->cls == 0 || p->team == 0) continue;
        FOHudRunner *n = &gRunners[gRunnerCount++];
        memset(n, 0, sizeof(*n));
        n->r.cls = p->cls;
        n->r.team = p->team;
        n->r.right = right;
        n->r.shotAgo = 1000;
        n->lastShot = -1000;
        if (p->team == winner) {
            if (right) {
                n->N = hud_rand(11, 15);
                n->O = hud_rand(7, 9);
                n->x = -(hud_rand(I / 2 * 10, I / 2 * 30) + I / 2 * 30);
            } else {
                n->N = 360 - hud_rand(7, 9);
                n->O = 360 - hud_rand(11, 15);
                n->x = gW + hud_rand(I / 2 * 10, I / 2 * 30) + I / 2 * 30;
            }
        } else {
            n->r.armsUp = 1;
            n->r.vx = 2;
            n->x = right ? -hud_rand(I / 2 * 5, I / 2 * 20) : gW + hud_rand(I / 2 * 5, I / 2 * 20);
        }
    }
}

// RoundOverDialog.b.b(float) (ajl.java:452-490) and .a.b(float).
static void runner_move(FOHudRunner *n, float dt, float now) {
    float step = gH * 4 / 150 * fo_class_speed(n->r.cls) * dt * (n->r.right ? 1 : -1);
    n->look.stateTime += dt;
    if (n->r.armsUp) {
        n->x += step;
        return;
    }
    const FOWeaponDef *def = fo_weapon_default(n->r.cls);
    float interval = def ? def->interval / 1000.0f : 1;
    float slow = n->shots > 0 && now - n->lastShot < interval && def ? def->afterShot : 1;
    n->x += step * slow;
    n->r.vx = n->r.cls == FO_CLASS_SNIPER ? 4 : slow * 1.8f;
    if (n->r.cls == FO_CLASS_SNIPER) {
        n->K = 0;
    } else if (!n->rising) {
        n->K -= 13 * dt;
        if (n->K < n->O) {
            n->K = n->O;
            n->rising = 1;
        }
    } else {
        n->K += 13 * dt;
        if (n->K > n->N) {
            n->K = n->N;
            n->rising = 0;
        }
    }
    n->r.aim = n->K + (n->r.right ? 0 : 180);
    if (n->r.cls == FO_CLASS_SNIPER) n->r.aim = 0;
    if (n->x > 0 && n->x < gW && def && now - n->lastShot >= interval && gShotCount < FO_RUNNER_SHOTS) {
        n->lastShot = now;
        n->shots++;
        float a = n->K + (n->r.right ? 0 : 180);
        float mx, my;
        FORunner aim = n->r;
        aim.aim = a;
        fo_look_runner_muzzle(&aim, &mx, &my);
        float v = gH / 150;
        FOHudShot *s = &gShots[gShotCount++];
        s->cls = n->r.cls;
        s->start = now;
        s->x = n->x + mx * 10 * v;
        s->y = 19 * v / 2 + my * 10 * v;
        s->vx = cosf(a * 0.017453292f) * def->speed;
        s->vy = sinf(a * 0.017453292f) * def->speed;
        s->gravity = -15 * def->gravity;
        s->life = def->kind == FO_SHOT_FLAME ? def->life : 0;
    }
    n->r.shotAgo = now - n->lastShot;
}

// RoundOverDialog.b.a(float, SpriteBatch) (ajl.java:574-596): the shots
// fly with the gravity, 10.3 UI units each meter.
static void shots_draw(float now) {
    float scale = 10.3f * gU, v = gH / 150;
    int k = 0;
    for (int i = 0; i < gShotCount; i++) {
        FOHudShot *s = &gShots[i];
        float t = now - s->start;
        float x = s->x + s->vx * t * scale;
        float y = s->y + (0.5f * s->gravity * t * t + s->vy * t) * scale;
        if ((s->life && t >= s->life / 1000.0f) || x < -gW || x > 2 * gW || y < -gH) continue;
        float angle = atan2f(s->vy + s->gravity * t, s->vx);
        fo_look_runner_shot(s->cls, x, y, angle, t, v);
        gShots[k++] = *s;
    }
    gShotCount = k;
}

static void runners_draw(const FOWorld *world, float dt, float now) {
    shots_draw(now);
    float v = gH / 150;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < gRunnerCount; i++) {
            FOHudRunner *n = &gRunners[i];
            if (n->r.armsUp != (pass == 0)) continue;
            runner_move(n, dt, now);
            fo_look_runner(world, &n->r, &n->look, n->x, 19 * v / 2, v, gW, gH);
        }
    }
}

// RoundOverDialog (ajl.java:58-300): the winner, the top 3 scores of each
// team, the score of the local player, and the runners.
int fo_hud_round_over(const FOWorld *world, const FOPlayer *local, float dt) {
    if (!fo_mode_over(world) || !local) {
        gRoundTime = 0;
        return 0;
    }
    int winner = fo_mode_winner(world);
    if (gRoundTime == 0) runners_start(world, winner);
    gRoundTime += dt;
    float al = gRoundTime / 0.3f;
    if (al > 1) al = 1;
    float u = gU, v = gH / 150.0f, W = gW, H = gH;
    // The bottom bar.
    float barH = H * 24 / 150;
    draw_box("roundover_bottombar", 0, 0, W, barH, al);
    // The banner: the text and its shadow one unit to the right and down.
    const char *banner = fo_text_get(gTexts, winner == FO_TEAM_RED ? "red_team_wins" : "blue_team_wins");
    float bannerH = fo_font_line(gFont21, u);
    float bannerW = fo_font_width(gFont21, banner, u);
    float bannerY = H - bannerH - 2 * v;
    if (winner == FO_TEAM_RED || winner == FO_TEAM_BLU) {
        unsigned int main = winner == FO_TEAM_RED ? 0xc53724 : 0x3f5683;
        fo_font_draw(gFont21, banner, W / 2 - bannerW / 2 + u, bannerY + bannerH - u, u, faded(0xece4cc, al), FO_ALIGN_LEFT);
        fo_font_draw(gFont21, banner, W / 2 - bannerW / 2, bannerY + bannerH, u, faded(main, al), FO_ALIGN_LEFT);
    }
    // The tabs of the teams.
    float tabW = 131 * u, tabH = 17 * u;
    float tabY = H - tabH - 25 * u;
    float redTabX = 8 * u, bluTabX = W - tabW - 8 * u;
    draw_box("roundover_redtab", redTabX, tabY, tabW, tabH, al);
    draw_box("roundover_blutab", bluTabX, tabY, tabW, tabH, al);
    // The panel, with steps at its two sides (ajl.java:214-241).
    float gap = 8 * u;
    float px = 4 * u + gap, py = barH + gap;
    float pw = W - 2 * gap - 8 * u, ph = H - bannerH - tabH - barH - gap - 6 * v;
    draw_box("roundover_bottombar", px, py, pw, ph, al);
    draw_box("roundover_bottombar", px - 2 * u, py + v, 2 * u, ph - 2 * v, al);
    draw_box("roundover_bottombar", px - 3 * u, py + 2 * v, u, ph - 4 * v, al);
    draw_box("roundover_bottombar", px - 4 * u, py + 4 * v, u, ph - 8 * v, al);
    draw_box("roundover_bottombar", px + pw, py + v, 2 * u, ph - 2 * v, al);
    draw_box("roundover_bottombar", px + pw + 2 * u, py + 2 * v, u, ph - 4 * v, al);
    draw_box("roundover_bottombar", px + pw + 3 * u, py + 4 * v, u, ph - 8 * v, al);
    float lineW = W / 2 - 11 * u - 23.5f * u;
    draw_box("roundover_redlinebg", 11 * u, py + 6 * u, lineW, 3 * u, al);
    draw_box("roundover_blulinebg", 23.5f * u + W / 2, py + 6 * u, lineW, 3 * u, al);
    // The score of the local player.
    FOSprite plate = sprite("roundover_yourscore");
    float plW = plate.w * u, plH = plate.h * u;
    float plX = W / 2 - plW / 2, plY = py - plH * 3 / 29;
    draw_box("roundover_yourscore", plX, plY, plW, plH, al);
    const char *yours = fo_text_get(gTexts, "your_score");
    float l12 = fo_font_line(gFont12, u);
    fo_font_draw(gFont12, yours, W / 2, plY + plH + u + l12, u, faded(0xece4cc, al), FO_ALIGN_CENTER);
    char text[64];
    snprintf(text, sizeof(text), "%d", local->score);
    float l6 = fo_font_line(gFont6, u);
    unsigned int scoreColor = local->team == FO_TEAM_RED ? 0x731228 : 0x0f245c;
    fo_font_draw(gFont6, text, W / 2, plY + 8 * u + l6 / 2, u, faded(scoreColor, al), FO_ALIGN_CENTER);
    // The names of the teams and the number of players in each team.
    const char *redName = fo_text_get(gTexts, "red_team"), *bluName = fo_text_get(gTexts, "blue_team");
    float redNameX = redTabX + 12 * u, bluNameX = bluTabX + 94 * u;
    fo_font_draw(gFont12, redName, redNameX, tabY + 2 * u + l12, u, faded(0xece4cc, al), FO_ALIGN_LEFT);
    fo_font_draw(gFont12, bluName, bluNameX, tabY + 2 * u + l12, u, faded(0xece4cc, al), FO_ALIGN_LEFT);
    const FOPlayer *red[FO_MAX_PLAYERS], *blu[FO_MAX_PLAYERS];
    int nRed = 0, nBlu = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *p = &world->players[i];
        if (!p->used) continue;
        if (p->team == FO_TEAM_RED) red[nRed++] = p;
        else if (p->team == FO_TEAM_BLU) blu[nBlu++] = p;
    }
    qsort(red, (size_t)nRed, sizeof(red[0]), by_score);
    qsort(blu, (size_t)nBlu, sizeof(blu[0]), by_score);
    char count[64];
    fo_text_format(fo_text_get(gTexts, "roundenddialog_players"), nRed, count, sizeof(count));
    float redNameW = fo_font_width(gFont12, redName, u);
    fo_font_draw(gFont6, count, redNameX + redNameW + 5 * u, tabY + tabH * 2 / 17 + l6, u, faded(0xece4cc, al),
                 FO_ALIGN_LEFT);
    fo_text_format(fo_text_get(gTexts, "roundenddialog_players"), nBlu, count, sizeof(count));
    float cw = fo_font_width(gFont6, count, u);
    fo_font_draw(gFont6, count, bluNameX - cw - 5 * u, tabY + tabH * 2 / 17 + l6, u, faded(0xece4cc, al),
                 FO_ALIGN_LEFT);
    // The headers of the two lists.
    float hw = pw / 2 - 2.5f * u, hh = 6 * v;
    float hx = 13 * u, hy = py + ph - hh - 9 * u;
    float hx2 = W / 2 + 1.5f * u;
    float l4 = fo_font_line(gFont, u);
    const char *nameText = fo_text_get(gTexts, "name"), *topText = fo_text_get(gTexts, "top3scores");
    float topW = fo_font_width(gFont, topText, u);
    for (int k = 0; k < 2; k++) {
        float x = k ? hx2 : hx;
        draw_box("servers_headerbg", x, hy, hw, hh, al);
        fo_font_draw(gFont, nameText, x + u, hy + hh / 2 + l4 / 2, u, faded(0x332f2d, al), FO_ALIGN_LEFT);
        fo_font_draw(gFont, topText, x + hw - topW, hy + hh / 2 + l4 / 2, u, faded(0x332f2d, al), FO_ALIGN_LEFT);
    }
    draw_box("servers_headerspacingbg", hx + hw, hy + v, 3 * u, 4 * v, al);
    // The mode and the map.
    float infoY = py + ph - v;
    fo_font_draw(gFont, fo_text_get(gTexts, "singleplayer_mode"), hx + u, infoY, u, faded(0xa19681, al), FO_ALIGN_LEFT);
    snprintf(text, sizeof(text), "%s: %s", fo_text_get(gTexts, "map"), gMapName);
    fo_font_draw(gFont, text, W / 2 + 3 * u, infoY, u, faded(0xa19681, al), FO_ALIGN_LEFT);
    // The top 3 of each team: the name in 78 units, the score in 48 units.
    float rowH = 8 * v;
    float listTop = hy;
    for (int k = 0; k < 2; k++) {
        const FOPlayer **list = k ? blu : red;
        int n = k ? nBlu : nRed;
        float x = (k ? hx2 : hx) + u;
        for (int i = 0; i < n && i < 3; i++) {
            float rowY = listTop - i * rowH;
            float ty = rowY - rowH / 2 + l4 / 2;
            fo_font_draw(gFont, list[i]->name, x, ty, u, faded(0x8f846e, al), FO_ALIGN_LEFT);
            snprintf(text, sizeof(text), "%d", list[i]->score);
            fo_font_draw(gFont, text, x + 78 * u + 24 * u, ty, u, faded(0x8f846e, al), FO_ALIGN_CENTER);
        }
    }
    runners_draw(world, dt, gRoundTime);
    return 1;
}

void fo_hud_sniper(float x, float y, float angle, float charge) {
    float u = gU;
    // The aim line: 300 x 9 UI units at half alpha, 14 units out.
    FOSprite line = sprite("sniper_aim_line");
    if (line.w) {
        float lw = line.w * u, lh = line.h * u;
        float ox = 14 * u, oy = -lh / 2;
        float px = x + ox * cosf(angle) - oy * sinf(angle), py = y + ox * sinf(angle) + oy * cosf(angle);
        FOColor half = {1, 1, 1, 0.5f};
        fo_gfx_draw(&line, px, py, lw, lh, 0, 0, angle, 0, half);
    }
    // The label "power" and the power icon, one of 5 steps of 20.
    const char *text = fo_text_get(gTexts, "power");
    float tw = fo_font_width(gFont, text, u), th = fo_font_line(gFont, u);
    fo_font_draw(gFont, text, x - tw / 2, (y - th / 2 - u - th / 2) + th, u, fo_rgb(0xd9d9b7), FO_ALIGN_LEFT);
    float c = charge < 0 ? 0 : charge > 100 ? 100 : charge;
    int step = c >= 80 ? 4 : c >= 60 ? 3 : c >= 40 ? 2 : c >= 20 ? 1 : 0;
    char name[32];
    snprintf(name, sizeof(name), "sniperaim_power%d", step + 1);
    FOSprite icon = sprite(name);
    float iw = 9 * u, ih = 5 * u;
    if (icon.w) fo_gfx_draw(&icon, x - iw / 2, u + (y - ih / 2) + ih / 2, iw, ih, 0, 0, 0, 0, FO_WHITE);
}

// DeathCamHUD (aib.java): a dark bar at the top and at the bottom, and the
// name of the killer in the top bar.
void fo_hud_death_cam(const char *killer) {
    float h = gH * 25.0f / 150.0f;
    FOSprite bg = sprite("menu_bgcolor");
    if (bg.w) {
        fo_gfx_draw(&bg, 0, 0, gW, h, 0, 0, 0, 0, FO_WHITE);
        fo_gfx_draw(&bg, 0, gH - h, gW, h, 0, 0, 0, 0, FO_WHITE);
    }
    char text[96];
    snprintf(text, sizeof(text), "%s", fo_text_get(gTexts, "killed_message"));
    char *mark = strstr(text, "{0}");
    if (mark) snprintf(mark, sizeof(text) - (size_t)(mark - text), "%s", killer);
    float lh = fo_font_line(gFont6, gU);
    int lines = 1;
    for (const char *c = text; *c; c++) lines += *c == '\n';
    float top = gH - h / 2 + lines * lh / 2;
    fo_font_draw(gFont6, text, gW / 2, top, gU, FO_WHITE, FO_ALIGN_CENTER);
}
