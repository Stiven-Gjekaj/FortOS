#include "fo_screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fo_look.h"

static FOScreensSetup S;
static float gW, gH, gU, gV;

// SingleplayerScreen: the map, the players, and the selector of the
// players, which blinks and slides (agv.java:160-187).
static int gMap;
static int gPlayers;
static float gSelectorTime;
static float gSelectorMove;     // The time since the last change of the players.
static int gSelectorFrom;       // The players before the last change.

// ClassSelectionDialog.
static int gClass;
static short gClassTeam;
static float gClassTime;        // The time since the last choice of a class.
static int gClassFrom;
static float gClassRandom[10];  // b of each ait: a random start of the idle animation.

static const char *const kMaps[3] = {"flag_egypt", "bomb_canyon", "gts_gantown"};
static const char *const kMapImages[3] = {"flag_egypt_image", "bomb_canyon_image", "gts_gantown_image"};

void fo_screens_init(const FOScreensSetup *setup) {
    S = *setup;
    gW = (float)setup->width;
    gH = (float)setup->height;
    gU = gW / 281.0f;
    gV = gH / 150.0f;
    for (int i = 0; i < 10; i++) gClassRandom[i] = (float)((i * 7919) % 1000) / 1000.0f;
}

// --- Drawing helpers ------------------------------------------------------

static FOSprite ui(const char *name) {
    return fo_assets_sprite(S.ui, name);
}

static FOSprite sprite(const char *name) {
    return fo_assets_sprite(S.sprites, name);
}

static const char *text(const char *key) {
    return S.texts ? fo_text_get(S.texts, key) : key;
}

static void image(const FOSprite *s, float x, float y, float w, float h) {
    if (s->w) fo_gfx_draw(s, x, y, w, h, 0, 0, 0, 0, FO_WHITE);
}

static int inside(float x, float y, float bx, float by, float bw, float bh) {
    return x >= bx && x <= bx + bw && y >= by && y <= by + bh;
}

// A Label: the text in the middle of its box (x, y, w, h) when centre is
// 1, or at the left of it. A Label of libGDX puts its text in the middle
// of its height.
static void label(const FOFont *font, const char *s, float x, float y, float w, float h, unsigned int color, int centre) {
    int lines = 1;
    for (const char *c = s; *c; c++) lines += *c == '\n';
    float lh = fo_font_line(font, gU);
    float top = y + h / 2 + lines * lh / 2;
    if (centre) fo_font_draw(font, s, x + w / 2, top, gU, fo_rgb(color), FO_ALIGN_CENTER);
    else fo_font_draw(font, s, x, top, gU, fo_rgb(color), FO_ALIGN_LEFT);
}

static float text_w(const FOFont *font, const char *s) {
    return fo_font_width(font, s, gU);
}

static float text_h(const FOFont *font) {
    return fo_font_line(font, gU);
}

// GangfortUIBigLabel (aii.java): the text, and a shadow one unit to the
// right and down.
static float big_w(const char *s) {
    return text_w(S.pixel21, s);
}

static float big_h(void) {
    return text_h(S.pixel21);
}

static void big_label(const char *s, float x, float y, unsigned int color, unsigned int shadow) {
    float w = big_w(s), h = big_h();
    label(S.pixel21, s, x + gU, y - gU, w, h, shadow, 0);
    label(S.pixel21, s, x, y, w, h, color, 0);
}

// GangfortUIButton (aij.java): two sides and a middle, with the text of
// pixel12 and its shadow.
typedef struct {
    float x, y, w, h;
} FOBox;

static float button_fit(const char *s, float width) {
    float need = text_w(S.pixel12, s) + 6 * gU;
    return need > width ? need : width;
}

static void button(const char *s, FOBox b) {
    FOSprite left = ui("button_side_left"), middle = ui("button_middle"), right = ui("button_side_right");
    float lw = left.w * gU, rw = right.w * gU;
    float mx = b.x + lw - gU, mw = b.w - lw - rw + 2 * gU;
    image(&left, b.x, b.y, lw, b.h);
    image(&middle, mx, b.y, mw, b.h);
    image(&right, mx + mw - gU, b.y, rw, b.h);
    float ly = b.y + b.h / 2 - 6 * gU + gU;
    label(S.pixel12, s, b.x, ly - 0.9f * gU, b.w, 12 * gU, 0x62584d, 1);
    label(S.pixel12, s, b.x, ly, b.w, 12 * gU, 0x1a1815, 1);
}

// The wide band of the menus: menu_bgstretch, 66 units high.
static void band(float y) {
    FOSprite s = ui("menu_bgstretch");
    float w = gW * 10.0f;
    image(&s, gW / 2 - w / 2, y, w, 66 * gU);
}

// --- SingleplayerScreen (agv.java) ----------------------------------------

const char *fo_single_map_name(int index) {
    return index >= 0 && index < 3 ? kMaps[index] : kMaps[0];
}

void fo_single_open(void) {
    gMap = 0;
    gPlayers = 12;
    gSelectorFrom = 0;
    gSelectorMove = 99;
    gSelectorTime = 0;
}

typedef struct {
    float bandY;
    FOBox left, right, image;
    FOBox bots[3];
    FOBox play;
} FOSingleLayout;

static FOSingleLayout single_layout(void) {
    FOSingleLayout l;
    float u = gU;
    l.bandY = 35.0f * gH / 150.0f;
    float f2 = 66 * u;
    float aw = 10 * u, ah = 17 * u;
    l.left = (FOBox){19 * u, l.bandY + f2 / 2 - ah / 2, aw, ah};
    l.right = (FOBox){gW - aw - 19 * u, l.bandY + f2 / 2 - ah / 2, aw, ah};
    float iw = 148 * u, ih = 72 * u;
    l.image = (FOBox){gW / 2 - iw / 2, l.bandY + f2 / 2 - ih / 2, iw, ih};
    const char *names[3] = {"singleplayer_selector_bots8", "singleplayer_selector_bots10", "singleplayer_selector_bots12"};
    float x = 7 * u;
    for (int i = 0; i < 3; i++) {
        FOSprite s = ui(names[i]);
        l.bots[i] = (FOBox){x, 3 * u, s.w * u, s.h * u};
        x += s.w * u;
    }
    float pw = button_fit(text("play"), 64 * u);
    l.play = (FOBox){gW - pw - 8 * u, 3 * u, pw, 18 * u};
    return l;
}

void fo_single_draw(float dt) {
    float u = gU;
    gSelectorTime += dt;
    gSelectorMove += dt;
    fo_gfx_begin((int)gW, (int)gH, (FOColor){0.1484375f, 0.13671875f, 0.1328125f, 1});
    fo_gfx_view(0, 0, gW, gH);
    FOSingleLayout l = single_layout();
    band(l.bandY);
    // The title.
    const char *title = text("single_player");
    big_label(title, gW / 2 - big_w(title) / 2, gH - big_h() - 3 * u, 0xece4cc, 0xe1661e);
    // The arrows, and the image and the name of the map.
    FOSprite al = ui("singleplayer_arrow_left"), ar = ui("singleplayer_arrow_right");
    if (gMap > 0) image(&al, l.left.x, l.left.y, l.left.w, l.left.h);
    if (gMap < 2) image(&ar, l.right.x, l.right.y, l.right.w, l.right.h);
    FOSprite map = ui(kMapImages[gMap]);
    image(&map, l.image.x, l.image.y, l.image.w, l.image.h);
    label(S.pixel6, kMaps[gMap], gW / 2, l.image.y - 13 * u, 0, 0, 0xe7dec1, 1);
    // The players: 8, 10, or 12.
    const char *names[3] = {"singleplayer_selector_bots8", "singleplayer_selector_bots10", "singleplayer_selector_bots12"};
    for (int i = 0; i < 3; i++) {
        FOSprite s = ui(names[i]);
        image(&s, l.bots[i].x, l.bots[i].y, l.bots[i].w, l.bots[i].h);
    }
    const char *bots = text("bots");
    float bh = text_h(S.pixel6);
    label(S.pixel6, bots, l.bots[2].x + l.bots[2].w + 8 * u, l.bots[2].y + l.bots[2].h / 2 - bh / 2, text_w(S.pixel6, bots),
          bh, 0xa89d7f, 0);
    int k = gPlayers == 8 ? 0 : gPlayers == 10 ? 1 : 2;
    const char *orange[3] = {"bots_selector_left_orange", "bots_selector_middle_orange", "bots_selector_right_orange"};
    const char *white[3] = {"bots_selector_left_white", "bots_selector_middle_white", "bots_selector_right_white"};
    // The selector changes its colour each 0.3 s after a wait of 0.15 s.
    int blink = gSelectorMove > 0.15f && (int)floorf((gSelectorMove - 0.15f) / 0.3f) % 2 == 1;
    FOSprite sel = ui(blink ? white[k] : orange[k]);
    float sx = l.bots[k].x - (k == 0 ? 2 * u : u), sy = l.bots[k].y - l.bots[k].h / 18.0f;
    if (gSelectorMove < 0.15f && gSelectorFrom != 0) sx += (gPlayers > gSelectorFrom ? 1.0f : -1.0f) * u;
    image(&sel, sx, sy, sel.w * u, sel.h * u);
    button(text("play"), l.play);
}

FOScreenResult fo_single_tap(float x, float y) {
    FOScreenResult r = {FO_SCREEN_NONE, 0, 0, 0, 0};
    FOSingleLayout l = single_layout();
    // Each arrow takes taps in three times its size (agv.java:64-79).
    if (gMap > 0 && inside(x, y, l.left.x - l.left.w, l.left.y - l.left.h, 3 * l.left.w, 3 * l.left.h)) gMap--;
    else if (gMap < 2 && inside(x, y, l.right.x - l.right.w, l.right.y - l.right.h, 3 * l.right.w, 3 * l.right.h)) gMap++;
    for (int i = 0; i < 3; i++) {
        if (inside(x, y, l.bots[i].x, l.bots[i].y, l.bots[i].w, l.bots[i].h)) {
            int n = 8 + 2 * i;
            if (n != gPlayers) {
                gSelectorFrom = gPlayers;
                gPlayers = n;
                gSelectorMove = 0;
            }
        }
    }
    if (inside(x, y, l.play.x, l.play.y, l.play.w, l.play.h)) {
        r.action = FO_SCREEN_PLAY;
        r.map = gMap;
        r.players = gPlayers;
    }
    return r;
}

// --- TeamSelectionDialog (ajq.java) ---------------------------------------

typedef struct {
    float bgY, bgH;
    FOBox leftside;
    FOBox red, blu;          // The doors.
    FOBox redPlate, bluPlate;
    float t;
} FOTeamLayout;

static FOTeamLayout team_layout(void) {
    FOTeamLayout l;
    float u = gU;
    FOSprite door = sprite("teamselection_red_doors_unlocked");
    FOSprite bg = sprite("teamselection_bgstretch");
    FOSprite side = sprite("teamselection_leftside");
    FOSprite plate = sprite("teamselection_red_doorplate");
    float dw = door.w * u, dh = door.w ? door.h * dw / door.w : 0;
    l.bgH = bg.h * u;
    l.bgY = gH / 2 - l.bgH / 2;
    l.leftside = (FOBox){0, l.bgY, side.w * u, side.h * u};
    float free = gW - l.leftside.w - dw - dw;
    float f = 10.0f * free / 100.0f;
    l.red = (FOBox){gW - dw - (free - f) / 2, l.bgY + 19 * u, dw, dh};
    l.blu = (FOBox){l.red.x - f - dw, l.bgY + 19 * u, dw, dh};
    float r = door.w ? plate.w * dw / door.w : 0;
    float s = plate.w ? r * plate.h / plate.w : 0;
    l.t = r * 2.0f / 44.0f;
    l.redPlate = (FOBox){l.red.x + (dw - r - l.t) / 2, l.red.y + dh + 5 * u, r, s};
    l.bluPlate = (FOBox){l.blu.x + (dw - r - l.t) / 2, l.blu.y + dh + 5 * u, r, s};
    return l;
}

// The number of players on a door plate, with the icon of a player.
static void plate_count(const FOBox *plate, float t, int count) {
    char s[16];
    snprintf(s, sizeof(s), "x %d", count);
    FOSprite icon = sprite("teamselection_doorplate_icon");
    FOSprite red = sprite("teamselection_red_doorplate");
    float o = red.w ? plate->w * icon.w / red.w : 0, p = red.h ? plate->h * icon.h / red.h : 0;
    float q = 0.5f * gU;
    float tw = text_w(S.pixel6, s), th = text_h(S.pixel6);
    float lx = plate->x + plate->w / 2 - tw / 2 + t + o / 2 + q;
    float ly = plate->y + (plate->h + t) / 2 - th / 2;
    label(S.pixel6, s, lx, ly, tw, th, 0xe6e6dd, 1);
    image(&icon, plate->x + plate->w / 2 - tw / 2 + t - o / 2 - q, ly + th / 2 - p / 2, o, p);
}

void fo_team_draw(const FOWorld *world) {
    float u = gU;
    FOTeamLayout l = team_layout();
    FOSprite bottom = sprite("teamselection_bottombgcolor"), top = sprite("teamselection_topbgcolor");
    image(&bottom, 0, 0, gW, gH / 2);
    image(&top, 0, gH / 2, gW, gH / 2);
    FOSprite bg = sprite("teamselection_bgstretch");
    image(&bg, 0, l.bgY, gW * 2, l.bgH);
    FOSprite side = sprite("teamselection_leftside");
    image(&side, l.leftside.x, l.leftside.y, l.leftside.w, l.leftside.h);
    FOSprite colon = sprite("teamselection_leftside_colon_stretch");
    image(&colon, l.leftside.x + 86 * u, l.leftside.y + l.leftside.h, colon.w * u, gH - l.leftside.y - l.leftside.h);
    const char *title = text("select_team");
    float tw = text_w(S.pixel6, title), th = text_h(S.pixel6);
    float tx = l.leftside.x + 39 * u - tw / 2, ty = l.leftside.y + 122 * u - th / 2;
    label(S.pixel6, title, tx, ty - u, tw, th, 0x080a0c, 1);
    label(S.pixel6, title, tx, ty, tw, th, 0xfd9318, 1);
    // The team difference limit of single player is 420, so no door locks.
    FOSprite red = sprite("teamselection_red_doors_unlocked"), blu = sprite("teamselection_blu_doors_unlocked");
    image(&red, l.red.x, l.red.y, l.red.w, l.red.h);
    image(&blu, l.blu.x, l.blu.y, l.blu.w, l.blu.h);
    FOSprite rp = sprite("teamselection_red_doorplate"), bp = sprite("teamselection_blu_doorplate");
    image(&rp, l.redPlate.x, l.redPlate.y, l.redPlate.w, l.redPlate.h);
    image(&bp, l.bluPlate.x, l.bluPlate.y, l.bluPlate.w, l.bluPlate.h);
    int nRed = 0, nBlu = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (!world->players[i].used) continue;
        if (world->players[i].team == FO_TEAM_RED) nRed++;
        else if (world->players[i].team == FO_TEAM_BLU) nBlu++;
    }
    plate_count(&l.redPlate, l.t, nRed);
    plate_count(&l.bluPlate, l.t, nBlu);
}

FOScreenResult fo_team_tap(float x, float y) {
    FOScreenResult r = {FO_SCREEN_NONE, 0, 0, 0, 0};
    FOTeamLayout l = team_layout();
    if (inside(x, y, l.red.x, l.red.y, l.red.w, l.red.h)) {
        r.action = FO_SCREEN_TEAM;
        r.team = FO_TEAM_RED;
    } else if (inside(x, y, l.blu.x, l.blu.y, l.blu.w, l.blu.h)) {
        r.action = FO_SCREEN_TEAM;
        r.team = FO_TEAM_BLU;
    }
    return r;
}

// --- ClassSelectionDialog (ahl.java) --------------------------------------

typedef struct {
    float bandY;
    FOBox chars, info, go;
    float px[10], py[10];    // The middle of the character of each class.
    float titleY;
} FOClassLayout;

static FOClassLayout class_layout(void) {
    FOClassLayout l;
    float u = gU;
    l.bandY = 35.0f * gH / 150.0f;
    float bandMid = l.bandY + 33 * u;
    FOSprite chars = ui("classselection_charsbg"), info = ui("classselection_info_bg");
    l.chars = (FOBox){12 * u, bandMid - chars.h * u / 2, chars.w * u, chars.h * u};
    l.info = (FOBox){gW - info.w * u, bandMid - info.h * u / 2, info.w * u, info.h * u};
    float gw = button_fit(text("go"), 59 * u);
    l.go = (FOBox){gW - gw - 5 * u, 2 * u, gw, 18 * u};
    float x1 = l.chars.x + 15 * u, x2 = l.chars.x + 46 * u, x3 = l.chars.x + 77 * u;
    float y1 = l.chars.y + 21 * u, y2 = l.chars.y + 62 * u, y3 = l.chars.y + 103 * u;
    float xs[10] = {0, x1, x2, x3, x1, x3, x2, x1, x3, x2};
    float ys[10] = {0, y3, y2, y3, y2, y2, y3, y1, y1, y1};
    for (int i = 0; i < 10; i++) {
        l.px[i] = xs[i];
        l.py[i] = ys[i];
    }
    l.titleY = gH - big_h() - 3 * u;
    return l;
}

void fo_class_open(int cls, short team) {
    // The original takes 1 when the class is not set: its search for the
    // least played class never runs (ahl.java:385-397).
    gClass = cls >= 1 && cls <= 9 ? cls : 1;
    gClassFrom = gClass;
    gClassTeam = team;
    gClassTime = 99;
}

static void select_class(int cls) {
    if (cls == gClass) return;
    gClassFrom = gClass;
    gClass = cls;
    gClassTime = 0;
}

// ClassSelectionDialog.a(int, int, int): the icon of each line of the
// information.
static const char *class_icon(int line, int cls, int team) {
    const char *t = team == FO_TEAM_RED ? "red" : "blu";
    static char name[40];
    const char *part = NULL;
    if (line == 1 && cls == FO_CLASS_SPY) part = "disguise";
    if (line == 2 && cls == FO_CLASS_ENGINEER) part = "build_sentry";
    if (line == 2 && cls == FO_CLASS_SPY) part = "backstab";
    if (line == 3 && cls == FO_CLASS_SCOUT) part = "jump";
    if (line == 3 && cls == FO_CLASS_ENGINEER) part = "destroy_sentry";
    if (line == 3 && cls == FO_CLASS_MEDIC) part = "uber_activate";
    if (line == 3 && cls == FO_CLASS_SPY) part = "toggle_cloak";
    if (!part) return NULL;
    snprintf(name, sizeof(name), "%s_%s", t, part);
    return name;
}

static const char *const kClassKeys[10] = {"",        "rusher",  "marine", "torcher", "yash",
                                           "mass",    "builder", "dealer", "camper",  "agent"};

void fo_class_draw(const FOWorld *world, float dt) {
    float u = gU;
    gClassTime += dt;
    FOClassLayout l = class_layout();
    FOSprite bg = sprite("menu_bgcolor");
    image(&bg, 0, 0, gW, gH);
    band(l.bandY);
    big_label(text("classselection_title"), 4 * u, l.titleY, 0xece4cc, 0xe1661e);
    FOSprite chars = ui("classselection_charsbg"), info = ui("classselection_info_bg");
    image(&chars, l.chars.x, l.chars.y, l.chars.w, l.chars.h);
    image(&info, l.info.x, l.info.y, l.info.w, l.info.h);
    // The information of the class: three lines and their icons.
    float mid = l.info.x + 14 * u + (l.info.w - 14 * u) / 2;
    float rows[3] = {57 * u, 36 * u, 13 * u};
    for (int k = 0; k < 3; k++) {
        char key[40];
        snprintf(key, sizeof(key), "%s_description%d", kClassKeys[gClass], k + 1);
        float th = text_h(S.pixel6);
        label(S.pixel6, text(key), mid, l.info.y + rows[k] - th / 2, 0, th, 0xece4cc, 1);
        const char *icon = class_icon(k + 1, gClass, gClassTeam);
        if (icon) {
            FOSprite s = sprite(icon);
            image(&s, l.info.x + 14 * u - 9 * u, l.info.y + rows[k] - 9 * u, 18 * u, 18 * u);
        }
    }
    // The nine characters, each on its shadow.
    FOSprite shadow = ui("item_char_bottom_shadow");
    short team = gClassTeam == FO_TEAM_RED ? FO_TEAM_RED : FO_TEAM_BLU;
    for (int c = 1; c <= 9; c++) {
        float off = 0;
        if (c == 3) off = 1.25f * u;
        else if (c == 4) off = 0.25f * u;
        else if (c == 5 || c == 6 || c == 9) off = 0.5f * u;
        else if (c == 8) off = -0.75f * u;
        float ax = l.px[c] - 4.5f * u, ay = l.py[c] - 9.5f * u;
        image(&shadow, ax + off + 4.5f * u - shadow.w * u / 2, ay - 2 * u, shadow.w * u, shadow.h * u);
        fo_look_preview(world, c, team, l.px[c], l.py[c], u, gClassRandom[c] + world->now / 1000.0f, gW, gH);
    }
    // The border moves one unit from the old class, then onto the new one
    // after 0.15 s, and changes its colour each 0.3 s.
    float bx = l.px[gClass] - 15 * u, by = l.py[gClass] - 20 * u;
    if (gClassTime < 0.15f) {
        if (l.px[gClass] > l.px[gClassFrom]) bx += u;
        else if (l.px[gClass] < l.px[gClassFrom]) bx -= u;
        if (l.py[gClass] > l.py[gClassFrom]) by += u;
        else if (l.py[gClass] < l.py[gClassFrom]) by -= u;
    }
    int blink = gClassTime > 0.15f && (int)floorf((gClassTime - 0.15f) / 0.3f) % 2 == 1;
    FOSprite border = ui(blink ? "classselection_border_white" : "classselection_border_orange");
    image(&border, bx, by, 32 * u, 42 * u);
    // The name of the class.
    const char *name = text(kClassKeys[gClass]);
    float nx = l.info.x + 14 * u + (l.info.w - 14 * u) / 2 - big_w(name) / 2;
    if (nx + big_w(name) > gW) nx = gW - big_w(name);
    big_label(name, nx, l.titleY - big_h() - 2 * u, 0xece4cc, 0xe1661e);
    button(text("go"), l.go);
}

FOScreenResult fo_class_tap(float x, float y) {
    FOScreenResult r = {FO_SCREEN_NONE, 0, 0, 0, 0};
    FOClassLayout l = class_layout();
    if (inside(x, y, l.chars.x, l.chars.y, l.chars.w, l.chars.h)) {
        for (int c = 1; c <= 9; c++) {
            float dx = x - l.px[c], dy = y - l.py[c];
            if (sqrtf(dx * dx + dy * dy) < 15 * gU) select_class(c);
        }
    }
    if (inside(x, y, l.go.x, l.go.y, l.go.w, l.go.h)) {
        r.action = FO_SCREEN_CLASS;
        r.cls = gClass;
    }
    return r;
}

// --- MenuDialog (aja.java) ------------------------------------------------

typedef struct {
    FOBox buttons[4];        // resume, change class, switch team, quit.
} FOMenuLayout;

static FOMenuLayout menu_layout(void) {
    FOMenuLayout l;
    float u = gU, v = gV;
    const char *keys[4] = {"resume", "change_class", "switch_team", "quit"};
    float f4 = 90 * u;
    for (int i = 0; i < 4; i++) {
        float need = text_w(S.pixel12, text(keys[i])) + 6 * u;
        if (need > f4) f4 = need;
    }
    FOSprite blue = ui("bluebutton_middle");
    float inviteH = blue.h * u;
    float f5 = 3 * v, f6 = 17 * u;
    // The invite button is only in multiplayer, but its place still counts.
    float quitY = -u + inviteH + f5;
    float gx = gW / 2 - f4 / 2, gy = gH / 2 - (f5 * 4 + f6 * 4 + inviteH) / 2;
    float ys[4] = {quitY + 3 * (f6 + f5), quitY + 2 * (f6 + f5), quitY + (f6 + f5), quitY};
    for (int i = 0; i < 4; i++) l.buttons[i] = (FOBox){gx, gy + ys[i], f4, 18 * u};
    return l;
}

void fo_menu_draw(void) {
    float u = gU, v = gV;
    FOSprite bg = sprite("menu_bgcolor");
    image(&bg, 0, 0, gW, gH);
    band(gH / 2 - 33 * u);
    FOSprite heavy = sprite("pausemenu_heavy"), spy = sprite("pausemenu_spy");
    image(&heavy, 7 * u, 1 * v, 87 * u, 137 * u);
    image(&spy, gW - 7 * u - 87 * u, 2 * v, 87 * u, 137 * u);
    FOMenuLayout l = menu_layout();
    const char *keys[4] = {"resume", "change_class", "switch_team", "quit"};
    for (int i = 0; i < 4; i++) button(text(keys[i]), l.buttons[i]);
}

FOScreenResult fo_menu_tap(float x, float y) {
    FOScreenResult r = {FO_SCREEN_NONE, 0, 0, 0, 0};
    FOMenuLayout l = menu_layout();
    FOScreenAction actions[4] = {FO_SCREEN_RESUME, FO_SCREEN_CHANGE_CLASS, FO_SCREEN_SWITCH_TEAM, FO_SCREEN_QUIT};
    for (int i = 0; i < 4; i++) {
        if (inside(x, y, l.buttons[i].x, l.buttons[i].y, l.buttons[i].w, l.buttons[i].h)) r.action = actions[i];
    }
    return r;
}
