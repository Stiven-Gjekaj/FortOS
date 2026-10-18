#include "fo_hud.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const FOAtlasTextures *gSprites;
static const FOFont *gFont;
static const FOText *gTexts;
static float gW, gH, gU;

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

void fo_hud_init(const FOAtlasTextures *sprites, const FOFont *smallest, const FOText *texts, int width, int height) {
    gSprites = sprites;
    gFont = smallest;
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
        if (e->kind != FO_EVENT_KILL) continue;
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

void fo_hud_draw(const FOWorld *world, const FOPlayer *local, float dt) {
    (void)dt;
    if (local && local->cls) draw_weapon_info(local);
    draw_kill_feed();
    if (local) draw_respawn(world, local);
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
