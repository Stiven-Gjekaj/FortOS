#include "fo_screens.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fo_game.h"
#include "fo_items.h"
#include "fo_look.h"
#include "fo_sound.h"

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
static FOLook gClassLooks[10];  // The frames of the hat of each character.

static FOLoadout gLoadouts[10];

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

void fo_screens_loadouts(const FOLoadout loadouts[10]) {
    memcpy(gLoadouts, loadouts, sizeof(gLoadouts));
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

// The back button of the menu screens: servers_back, 38 x 17 units, at
// the top left (agv.java:58-65).
static FOBox back_box(void) {
    return (FOBox){9 * gU, gH - 17 * gU - 7 * gU, 38 * gU, 17 * gU};
}

static void back_draw(void) {
    FOSprite s = sprite("servers_back");
    FOBox b = back_box();
    image(&s, b.x, b.y, b.w, b.h);
}

static int back_tap(float x, float y) {
    FOBox b = back_box();
    return inside(x, y, b.x, b.y, b.w, b.h);
}

// The background and the title of a menu screen (aii at 3 units from the
// top).
static void menu_screen(const char *title) {
    fo_gfx_begin((int)gW, (int)gH, (FOColor){0.1484375f, 0.13671875f, 0.1328125f, 1});
    fo_gfx_view(0, 0, gW, gH);
    band(35.0f * gH / 150.0f);
    if (title) big_label(title, gW / 2 - big_w(title) / 2, gH - big_h() - 3 * gU, 0xece4cc, 0xe1661e);
}

// --- SplashScreen (ahe.java) ----------------------------------------------

static float gSplashTime;
static int gSplashSound;

void fo_splash_open(void) {
    gSplashTime = 0;
    gSplashSound = 0;
}

int fo_splash_draw(float dt) {
    gSplashTime += dt;
    fo_gfx_begin((int)gW, (int)gH, (FOColor){0.1484375f, 0.13671875f, 0.1328125f, 1});
    fo_gfx_view(0, 0, gW, gH);
    // splash_hit plays at the start. It decodes first, in a few frames.
    if (!gSplashSound && gSplashTime < 0.5f && fo_sound_ready("splash_hit")) {
        fo_sound_global("splash_hit");
        gSplashSound = 1;
    }
    // The logo falls from the top to the middle in 0.1 s. Then its 11
    // frames play at 12 each second, and the last frame stays.
    float w = 373 * gU, h = 242 * gU;
    float t = gSplashTime / 0.1f;
    float y = gH + (gH / 2 - h / 2 - gH) * (t < 1 ? t : 1);
    int frame = gSplashTime < 0.1f ? 0 : (int)floorf((gSplashTime - 0.1f) / 0.083333336f);
    if (frame > 10) frame = 10;
    if (S.splash) {
        char name[16];
        snprintf(name, sizeof(name), "splash%d", frame + 1);
        FOSprite s = fo_assets_sprite(S.splash, name);
        image(&s, gW / 2 - w / 2, y, w, h);
    }
    return gSplashTime >= 2.5f;
}

// --- MainMenuScreen (afk.java) --------------------------------------------

// The buttons from the top: single player, items, and credits. The
// original also has multiplayer, and quit on Android.
#define FO_MAIN_BUTTONS 3
static const char *const kMainKeys[FO_MAIN_BUTTONS] = {"single_player", "items", "credits"};

typedef struct {
    FOBox buttons[FO_MAIN_BUTTONS];
    FOBox settings;
    FOBox demoman;
} FOMainLayout;

static FOMainLayout main_layout(void) {
    FOMainLayout l;
    float u = gU, v = gV;
    float w = 91 * u;
    for (int i = 0; i < FO_MAIN_BUTTONS; i++) {
        float need = button_fit(text(kMainKeys[i]), 84 * u);
        if (need > w) w = need;
    }
    float bh = 18 * u, gap = 3 * v;
    // The group is in the middle. The original adds 3 pixels, not 3 units,
    // for each gap (afk.java:93).
    float gx = gW / 2 - w / 2, gy = gH / 2 - (FO_MAIN_BUTTONS * 3.0f + FO_MAIN_BUTTONS * bh) / 2;
    for (int i = 0; i < FO_MAIN_BUTTONS; i++) {
        l.buttons[i] = (FOBox){gx, gy + (FO_MAIN_BUTTONS - 1 - i) * (bh + gap), w, bh};
    }
    float sw = 19 * u, sh = 17 * u, sm = 6 * u;
    l.settings = (FOBox){gW - sw - sm, gH - sh - sm, sw, sh};
    float dw = 243 * u, dh = 105 * dw / 243;
    l.demoman = (FOBox){gW / 2 - dw / 2, gH / 2 - dh / 2 - 18 * v, dw, dh};
    return l;
}

void fo_main_draw(void) {
    menu_screen(NULL);
    FOMainLayout l = main_layout();
    FOSprite demoman = sprite("mainmenu_demoman");
    image(&demoman, l.demoman.x, l.demoman.y, l.demoman.w, l.demoman.h);
    for (int i = 0; i < FO_MAIN_BUTTONS; i++) button(text(kMainKeys[i]), l.buttons[i]);
    FOSprite gear = ui("mainmenu_settings");
    image(&gear, l.settings.x, l.settings.y, l.settings.w, l.settings.h);
}

FOScreenResult fo_main_tap(float x, float y) {
    FOScreenResult r = {0};
    FOMainLayout l = main_layout();
    FOScreenAction actions[FO_MAIN_BUTTONS] = {FO_SCREEN_SINGLE, FO_SCREEN_ITEMS, FO_SCREEN_CREDITS};
    for (int i = 0; i < FO_MAIN_BUTTONS; i++) {
        if (inside(x, y, l.buttons[i].x, l.buttons[i].y, l.buttons[i].w, l.buttons[i].h)) r.action = actions[i];
    }
    if (inside(x, y, l.settings.x, l.settings.y, l.settings.w, l.settings.h)) r.action = FO_SCREEN_SETTINGS;
    r.click = r.action != FO_SCREEN_NONE;
    return r;
}

// --- CreditsScreen (adf.java) ---------------------------------------------

void fo_credits_draw(void) {
    menu_screen(text("credits"));
    back_draw();
    // The lines from the bottom: each title has its names under it, 2 units
    // away, and 5 units over the names under them.
    struct {
        const char *s;
        int title;
    } lines[8] = {
        {"jonas, gustas, lukas, misha,\nvycka, robertas, marius,\ndomas and other homies", 0},
        {text("special_thanks"), 1},
        {"dmitry jbanov\n(disyman)", 0},
        {text("music_sounds"), 1},
        {"gintas zenevskis", 0},
        {text("code"), 1},
        {"kostic milos\n(harveydentmd)", 0},
        {text("art"), 1},
    };
    float ys[8], hs[8], y = 0, lh = text_h(S.pixel6);
    for (int i = 0; i < 8; i++) {
        int n = 1;
        for (const char *c = lines[i].s; *c; c++) n += *c == '\n';
        hs[i] = n * lh;
        if (i > 0) y += hs[i - 1] + (lines[i].title ? 2 : 5) * gU;
        ys[i] = y;
    }
    float bandY = 35.0f * gH / 150.0f;
    float base = bandY + 33 * gU - ys[7] / 2;
    for (int i = 0; i < 8; i++) {
        label(S.pixel6, lines[i].s, 0, base + ys[i], gW, hs[i], lines[i].title ? 0x8f846d : 0xe7dec1, 1);
    }
}

FOScreenResult fo_credits_tap(float x, float y) {
    FOScreenResult r = {0};
    if (back_tap(x, y)) r.action = FO_SCREEN_BACK;
    r.click = r.action != FO_SCREEN_NONE;
    return r;
}

// --- SettingsScreen (ago.java) --------------------------------------------

// The checkboxes from the bottom. The original also has the notifications.
static const char *const kSettingKeys[FO_SETTING_COUNT] = {"show_killcam", "play_theme_music", "enable_vignette_effect"};
static int gSettings[FO_SETTING_COUNT];

typedef struct {
    FOBox controls;
    FOBox boxes[FO_SETTING_COUNT];   // The whole checkbox: the box and its text.
} FOSettingsLayout;

void fo_settings_open(const int on[FO_SETTING_COUNT]) {
    for (int i = 0; i < FO_SETTING_COUNT; i++) gSettings[i] = on[i];
}

// GangfortUICheckbox (aik.java): a box of 16 x 14 units, and the text 3
// units to its right.
static FOSettingsLayout settings_layout(void) {
    FOSettingsLayout l;
    float u = gU, gap = 7 * u;
    float lh = text_h(S.pixel6), ch = 14 * u > lh ? 14 * u : lh;
    float bw = 84 * u, bh = 18 * u;
    float total = bh + FO_SETTING_COUNT * (gap + ch);
    float bandY = 35.0f * gH / 150.0f;
    float gy = bandY + 33 * u - total / 2;
    l.controls = (FOBox){gW / 2 - bw / 2, gy, bw, bh};
    // The group of the checkboxes has its middle at the middle of the top
    // checkbox (ago.java:95).
    float top = 16 * u + 3 * u + text_w(S.pixel6, text(kSettingKeys[FO_SETTING_COUNT - 1]));
    float gx = gW / 2 - top / 2;
    float y = gy + bh + gap;
    for (int i = 0; i < FO_SETTING_COUNT; i++) {
        float w = 16 * u + 3 * u + text_w(S.pixel6, text(kSettingKeys[i]));
        l.boxes[i] = (FOBox){gx, y, w, ch};
        y += ch + gap;
    }
    return l;
}

void fo_settings_draw(void) {
    float u = gU;
    menu_screen(text("settings"));
    back_draw();
    FOSettingsLayout l = settings_layout();
    button(text("controls"), l.controls);
    float lh = text_h(S.pixel6);
    for (int i = 0; i < FO_SETTING_COUNT; i++) {
        FOBox b = l.boxes[i];
        FOSprite box = ui(gSettings[i] ? "checkbox_checked" : "checkbox_unchecked");
        image(&box, b.x, b.y, 16 * u, 14 * u);
        const char *s = text(kSettingKeys[i]);
        label(S.pixel6, s, b.x + 19 * u, b.y + 7 * u - lh / 2 - u, text_w(S.pixel6, s), lh, 0xe7dec2, 0);
    }
}

FOScreenResult fo_settings_tap(float x, float y) {
    FOScreenResult r = {0};
    FOSettingsLayout l = settings_layout();
    if (back_tap(x, y)) r.action = FO_SCREEN_BACK;
    if (inside(x, y, l.controls.x, l.controls.y, l.controls.w, l.controls.h)) r.action = FO_SCREEN_CONTROLS;
    for (int i = 0; i < FO_SETTING_COUNT; i++) {
        FOBox b = l.boxes[i];
        if (inside(x, y, b.x, b.y, b.w, b.h)) {
            gSettings[i] = !gSettings[i];
            r.action = FO_SCREEN_SETTING;
            r.setting = i;
            r.value = gSettings[i];
        }
    }
    r.click = r.action != FO_SCREEN_NONE;
    return r;
}

// --- CustomizeTouchControlsScreen (adh.java) ------------------------------

typedef struct {
    float helpY;
    FOBox box;               // The checkbox and its text.
    FOBox reset;
} FOSetupLayout;

static FOSetupLayout setup_layout(void) {
    FOSetupLayout l;
    float u = gU, lh = text_h(S.pixel6);
    float titleY = gH - big_h() - 3 * u;
    l.helpY = titleY - lh - 9 * u;
    const char *s = text("use_double_joysticks");
    float w = 16 * u + 3 * u + text_w(S.pixel6, s), h = 14 * u > lh ? 14 * u : lh;
    l.box = (FOBox){gW / 2 - w / 2, l.helpY - lh - 9 * u, w, h};
    l.reset = (FOBox){gW - 19 * u - 9 * u, gH - 17 * u - 7 * u, 19 * u, 17 * u};
    return l;
}

void fo_setup_draw(int sticks) {
    float u = gU, lh = text_h(S.pixel6);
    fo_gfx_begin((int)gW, (int)gH, (FOColor){0.1484375f, 0.13671875f, 0.1328125f, 1});
    fo_gfx_view(0, 0, gW, gH);
    const char *title = text("controls");
    big_label(title, gW / 2 - big_w(title) / 2, gH - big_h() - 3 * u, 0xece4cc, 0xe1661e);
    back_draw();
    FOSetupLayout l = setup_layout();
    const char *help = text("drag_and_drop");
    label(S.pixel6, help, 0, l.helpY, gW, lh, 0xe7dec1, 1);
    FOSprite box = ui(sticks ? "checkbox_checked" : "checkbox_unchecked");
    image(&box, l.box.x, l.box.y, 16 * u, 14 * u);
    const char *s = text("use_double_joysticks");
    label(S.pixel6, s, l.box.x + 19 * u, l.box.y + 7 * u - lh / 2 - u, text_w(S.pixel6, s), lh, 0xe7dec2, 0);
    FOSprite reset = ui("reset_button");
    image(&reset, l.reset.x, l.reset.y, l.reset.w, l.reset.h);
}

FOScreenResult fo_setup_tap(float x, float y, int sticks) {
    FOScreenResult r = {0};
    FOSetupLayout l = setup_layout();
    if (back_tap(x, y)) r.action = FO_SCREEN_BACK;
    if (inside(x, y, l.reset.x, l.reset.y, l.reset.w, l.reset.h)) r.action = FO_SCREEN_CONTROLS_RESET;
    if (inside(x, y, l.box.x, l.box.y, l.box.w, l.box.h)) {
        r.action = FO_SCREEN_CONTROLS_STICKS;
        r.value = !sticks;
    }
    r.click = r.action != FO_SCREEN_NONE;
    return r;
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
    back_draw();
}

FOScreenResult fo_single_tap(float x, float y) {
    FOScreenResult r = {0};
    FOSingleLayout l = single_layout();
    if (back_tap(x, y)) {
        r.action = FO_SCREEN_BACK;
        r.click = 1;
        return r;
    }
    // Each arrow takes taps in three times its size (agv.java:64-79).
    if (gMap > 0 && inside(x, y, l.left.x - l.left.w, l.left.y - l.left.h, 3 * l.left.w, 3 * l.left.h)) {
        gMap--;
        r.click = 1;
    } else if (gMap < 2 && inside(x, y, l.right.x - l.right.w, l.right.y - l.right.h, 3 * l.right.w, 3 * l.right.h)) {
        gMap++;
        r.click = 1;
    }
    for (int i = 0; i < 3; i++) {
        if (inside(x, y, l.bots[i].x, l.bots[i].y, l.bots[i].w, l.bots[i].h)) {
            int n = 8 + 2 * i;
            r.click = 1;
            if (n != gPlayers) {
                gSelectorFrom = gPlayers;
                gPlayers = n;
                gSelectorMove = 0;
            }
        }
    }
    if (inside(x, y, l.play.x, l.play.y, l.play.w, l.play.h)) {
        r.action = FO_SCREEN_PLAY;
        r.click = 1;
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
    FOScreenResult r = {0};
    FOTeamLayout l = team_layout();
    if (inside(x, y, l.red.x, l.red.y, l.red.w, l.red.h)) {
        r.action = FO_SCREEN_TEAM;
        r.click = 1;
        r.team = FO_TEAM_RED;
    } else if (inside(x, y, l.blu.x, l.blu.y, l.blu.w, l.blu.h)) {
        r.action = FO_SCREEN_TEAM;
        r.click = 1;
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
        fo_look_preview(world, c, team, gLoadouts[c].hat, gLoadouts[c].weapon, l.px[c], l.py[c], u,
                        gClassRandom[c] + world->now / 1000.0f, gW, gH, &gClassLooks[c]);
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
    FOScreenResult r = {0};
    FOClassLayout l = class_layout();
    if (inside(x, y, l.chars.x, l.chars.y, l.chars.w, l.chars.h)) {
        for (int c = 1; c <= 9; c++) {
            float dx = x - l.px[c], dy = y - l.py[c];
            if (sqrtf(dx * dx + dy * dy) < 15 * gU) {
                select_class(c);
                r.click = 1;
            }
        }
    }
    if (inside(x, y, l.go.x, l.go.y, l.go.w, l.go.h)) {
        r.action = FO_SCREEN_CLASS;
        r.click = 1;
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
    FOScreenResult r = {0};
    FOMenuLayout l = menu_layout();
    FOScreenAction actions[4] = {FO_SCREEN_RESUME, FO_SCREEN_CHANGE_CLASS, FO_SCREEN_SWITCH_TEAM, FO_SCREEN_QUIT};
    for (int i = 0; i < 4; i++) {
        if (inside(x, y, l.buttons[i].x, l.buttons[i].y, l.buttons[i].w, l.buttons[i].h)) {
            r.action = actions[i];
            r.click = 1;
        }
    }
    return r;
}

// --- ItemsScreen (afb.java) -----------------------------------------------

// The order of the characters on the panel (afb.java:118).
static const int kItemsOrder[9] = {1, 6, 3, 4, 2, 5, 7, 9, 8};
static FOLook gItemLooks[10];

// The shadow under a character of ItemsEquippedUICharacter (ait.java:45-66).
static float char_shadow_offset(int cls) {
    switch (cls) {
    case 3: return 1.25f * gU;
    case 4: return 0.25f * gU;
    case 5: case 6: case 9: return 0.5f * gU;
    case 8: return -0.75f * gU;
    default: return 0;
    }
}

// An ait: 9 x 19 units, with (x, y) at its bottom left. team 2 is red.
static void item_char(const FOWorld *world, int cls, float x, float y, int shadow, FOLook *look) {
    float u = gU;
    if (shadow) {
        FOSprite s = ui("item_char_bottom_shadow");
        image(&s, x + char_shadow_offset(cls) + 4.5f * u - s.w * u / 2, y - 2 * u, s.w * u, s.h * u);
    }
    fo_look_preview(world, cls, FO_TEAM_RED, gLoadouts[cls].hat, gLoadouts[cls].weapon, x + 4.5f * u, y + 9.5f * u, u,
                    gClassRandom[cls] + world->now / 1000.0f, gW, gH, look);
}

typedef struct {
    FOBox panel;
    FOBox chars[9];          // The bottom left of each character, 9 x 19 units.
} FOItemsLayout;

static FOItemsLayout items_layout(void) {
    FOItemsLayout l;
    float u = gU;
    FOSprite bg = ui("items_charsbg");
    float bw = bg.w * u, bh = bg.h * u, bandY = 35.0f * gH / 150.0f;
    l.panel = (FOBox){gW / 2 - bw / 2, bandY + 33 * u - bh / 2, bw, bh};
    float f10 = 20 * u, step = (bw - 2 * f10) / 8;
    for (int i = 0; i < 9; i++) l.chars[i] = (FOBox){l.panel.x + f10 + i * step - 9 * u, l.panel.y + 12 * u, 9 * u, 19 * u};
    return l;
}

void fo_items_draw(const FOWorld *world) {
    menu_screen(text("items"));
    back_draw();
    FOItemsLayout l = items_layout();
    FOSprite bg = ui("items_charsbg");
    image(&bg, l.panel.x, l.panel.y, l.panel.w, l.panel.h);
    for (int i = 0; i < 9; i++) {
        int c = kItemsOrder[i];
        item_char(world, c, l.chars[i].x, l.chars[i].y, 1, &gItemLooks[c]);
    }
}

FOScreenResult fo_items_tap(float x, float y) {
    FOScreenResult r = {0};
    if (back_tap(x, y)) r.action = FO_SCREEN_BACK;
    FOItemsLayout l = items_layout();
    // Each character takes taps in twice its size (afb.java:126-128).
    for (int i = 0; i < 9; i++) {
        FOBox b = l.chars[i];
        if (inside(x, y, b.x - b.w / 2, b.y - b.h / 2, 2 * b.w, 2 * b.h)) {
            r.action = FO_SCREEN_ITEMS_CLASS;
            r.cls = kItemsOrder[i];
        }
    }
    r.click = r.action != FO_SCREEN_NONE;
    return r;
}

// --- ItemSelectionScreen (aeo.java) and ItemsList (aiu.java) ---------------

#define FO_SELECT_MAX 48
static const char *const kClassShadows[10] = {"",           "shadow_rusher", "shadow_marine", "shadow_torcher",
                                              "shadow_yash", "shadow_mass",   "shadow_builder", "shadow_dealer",
                                              "shadow_camper", "shadow_agent"};

static struct {
    int cls;
    FOLoadout loadout;
    int items[FO_SELECT_MAX];
    int count;
    int selected, previous;  // f of aiu, and the cell before it.
    int armed;               // v of aeo: a second tap on this cell equips.
    float selectTime;        // The time since the last selection.
    float scroll;            // scrollY of the ScrollPane, 0 at the top.
    int touch;               // The finger on the list, or -1.
    float touchY, touchScroll, touchMove;
    FOLook look;
} gSel;

void fo_select_open(int cls) {
    memset(&gSel, 0, sizeof(gSel));
    gSel.cls = cls;
    gSel.loadout = gLoadouts[cls];
    gSel.touch = -1;
    gSel.armed = -1;
    // All the items are owned: the hats, and the weapons of the store.
    // The default weapons are not in the list (they have no picture).
    for (int i = 0; i < FO_ITEM_COUNT && gSel.count < FO_SELECT_MAX; i++) {
        const FOItem *it = &kFOItems[i];
        if (it->icon && fo_item_allowed(it->id, cls)) gSel.items[gSel.count++] = it->id;
    }
    // aiu.a(aax, aay): the first item is selected. aeo.show(): the hat of
    // the class is selected when it has one.
    gSel.armed = 0;
    for (int i = 0; i < gSel.count; i++) {
        if (gSel.loadout.hat && gSel.items[i] == gSel.loadout.hat) gSel.selected = gSel.armed = i;
    }
    gSel.previous = gSel.selected;
    gSel.selectTime = 1;
}

FOLoadout fo_select_loadout(void) {
    return gSel.loadout;
}

static int item_equipped(int id) {
    return id && (gSel.loadout.hat == id || gSel.loadout.weapon == id);
}

typedef struct {
    FOBox list, inner, info, shadowBg, shadow, character;
    float cellW, cellH, titleY;
} FOSelectLayout;

static FOSelectLayout select_layout(void) {
    FOSelectLayout l;
    float u = gU, bandY = 35.0f * gH / 150.0f;
    l.titleY = gH - big_h() - 3 * u;
    l.list = (FOBox){9 * u, (l.titleY - 3 * u) / 2 - 42 * u, 85 * u, 84 * u};
    l.inner = (FOBox){l.list.x + u, l.list.y + 2 * u, 83 * u, 81 * u};
    l.cellW = l.inner.w / 2;
    l.cellH = 41 * u;
    FOSprite info = ui("items_info_bg_small");
    l.info = (FOBox){gW - info.w * u, bandY + 33 * u - info.h * u / 2, info.w * u, info.h * u};
    FOSprite sbg = ui("items_selection_shadowbg");
    float sx = ((l.list.x + l.list.w) + l.info.x) / 2 - sbg.w * u / 2;
    l.shadowBg = (FOBox){sx, bandY + 2 * u, sbg.w * u, sbg.h * u};
    FOSprite sh = ui(kClassShadows[gSel.cls]);
    l.shadow = (FOBox){sx + l.shadowBg.w / 2 - sh.w * u / 2, l.shadowBg.y + 2 * u, sh.w * u, sh.h * u};
    l.character = (FOBox){sx + 29 * u, l.shadowBg.y + 3 * u, 9 * u, 19 * u};
    return l;
}

static float select_max_scroll(const FOSelectLayout *l) {
    float content = ((gSel.count + 1) / 2) * l->cellH;
    return content > l->inner.h ? content - l->inner.h : 0;
}

// A small random number from the item id, so that the holes of a cell stay
// in their places (aiu.java:96-109 takes them at random).
static float cell_random(unsigned int *seed) {
    *seed = *seed * 1103515245u + 12345u;
    return (float)((*seed >> 16) & 0x7fff) / 32767.0f;
}

static void select_list_draw(const FOSelectLayout *l) {
    float u = gU;
    FOSprite bg = ui("items_list_bg");
    image(&bg, l->list.x, l->list.y, l->list.w, l->list.h);
    fo_gfx_clip((int)floorf(l->inner.x), (int)floorf(l->inner.y), (int)ceilf(l->inner.w), (int)ceilf(l->inner.h));
    FOSprite row = ui("items_list_row_bg");
    for (int i = 0; i < gSel.count; i++) {
        int r = i / 2, c = i % 2;
        float cx = l->inner.x + c * l->cellW;
        float cy = l->inner.y + l->inner.h - (r + 1) * l->cellH + gSel.scroll;
        if (cy > l->inner.y + l->inner.h || cy + l->cellH < l->inner.y) continue;
        if (c == 0 && r % 2 == 0) image(&row, cx, cy, 2 * l->cellW, l->cellH);
        unsigned int seed = (unsigned int)gSel.items[i] * 2654435761u;
        int holes = 1 + (int)(cell_random(&seed) * 1.999f);
        for (int k = 0; k < holes; k++) {
            char name[24];
            snprintf(name, sizeof(name), "items_list_hole%d", 1 + (int)(cell_random(&seed) * 4.999f));
            FOSprite h = ui(name);
            float hw = h.w * u, hh = h.h * u, hx = 0, hy = 0;
            for (int t = 0; t < 20; t++) {
                hx = hw + (l->cellW - 2 * hw) * cell_random(&seed);
                hy = hh + (l->cellH - 2 * hh) * cell_random(&seed);
                float dx = hx - l->cellW / 2, dy = hy - l->cellH / 2;
                if (dx * dx + dy * dy >= 16 * u * 16 * u) break;
            }
            image(&h, cx + hx, cy + hy, hw, hh);
        }
        fo_look_item_icon(S.ui, gSel.items[i], cx + l->cellW / 2, cy + l->cellH / 2 - u, u);
    }
    // The selector moves one unit from the old cell, rests after 0.15 s,
    // and changes its colour each 0.3 s (aiu.java:247-276).
    if (gSel.count > 0) {
        int i = gSel.selected, p = gSel.previous;
        float dx = 0, dy = 0;
        if (gSel.selectTime < 0.15f) {
            if (p % 2 == 0 && i % 2 == 1) dx = u;
            else if (p % 2 == 1 && i % 2 == 0) dx = -u;
            if (p - i >= 2 || (i % 2 == p % 2 && p > i) || (i % 2 == 1 && p % 2 == 0 && p > i)) dy = u;
            else if (p - i <= -2 || (i % 2 == p % 2 && p < i) || (i % 2 == 0 && p % 2 == 1 && p < i)) dy = -u;
        }
        int white = gSel.selectTime > 0.15f && (int)floorf((gSel.selectTime - 0.15f) / 0.3f) % 2 == 1;
        FOSprite sel = ui(white ? "items_list_selector_white" : "items_list_selector_orange");
        float cx = l->inner.x + (i % 2) * l->cellW, cy = l->inner.y + l->inner.h - (i / 2 + 1) * l->cellH + gSel.scroll;
        image(&sel, cx + (i % 2 == 1 ? 0.5f * u : 0) + dx, cy + u + dy, sel.w * u, sel.h * u);
    }
    fo_gfx_clip(0, 0, 0, 0);
    // The darkening at the ends while more items are out of the view.
    FOSprite more = ui("items_list_more_darkening");
    float top = gSel.scroll / (10 * u), bottom = (floorf(gSel.count / 2.0f) * l->cellH - (gSel.scroll + l->inner.h)) / (10 * u);
    top = top < 0 ? 0 : top > 1 ? 1 : top;
    bottom = bottom < 0 ? 0 : bottom > 1 ? 1 : bottom;
    if (more.w) {
        float gx = l->list.x, gy = l->list.y;
        fo_gfx_draw(&more, gx, gy + l->inner.h - u, l->list.w, 4 * u, 0, 0, 0, 0, (FOColor){1, 1, 1, top});
        fo_gfx_draw(&more, gx, gy + u, l->list.w, 4 * u, 0, 0, 0, 0, (FOColor){1, 1, 1, bottom});
    }
}

void fo_select_draw(const FOWorld *world, float dt) {
    float u = gU, lh = text_h(S.pixel6);
    gSel.selectTime += dt;
    fo_gfx_begin((int)gW, (int)gH, (FOColor){0.1484375f, 0.13671875f, 0.1328125f, 1});
    fo_gfx_view(0, 0, gW, gH);
    band(35.0f * gH / 150.0f);
    FOSelectLayout l = select_layout();
    FOSprite info = ui("items_info_bg_small");
    image(&info, l.info.x, l.info.y, l.info.w, l.info.h);
    const char *title = text(kClassKeys[gSel.cls]);
    big_label(title, gW / 2 - big_w(title) / 2, l.titleY, 0xece4cc, 0xe1661e);
    back_draw();
    if (gSel.count == 0) {
        label(S.pixel6, text("you_have_no_items"), 0, 35.0f * gH / 150.0f + 33 * u - lh / 2, gW, lh, 0xece4cc, 1);
        return;
    }
    int id = gSel.items[gSel.selected];
    const char *hint = text(item_equipped(id) ? "click_item_unequip" : "click_item_equip");
    label(S.pixel6, hint, 0, l.titleY - 7 * u - lh, gW, lh, 0xe7dec1, 1);
    select_list_draw(&l);
    // The title and the description of the item in the box at the right.
    const FOItem *it = fo_item(id);
    if (it) {
        float cx = l.info.x + l.info.w / 2;
        // ItemDataHelper.a(...) keeps the title and the description in
        // lower case (aao.java:150). pixel6 has no capital letters.
        char t[96], d[256];
        snprintf(t, sizeof(t), "%s", text(it->title));
        snprintf(d, sizeof(d), "%s", text(it->description));
        for (char *c = t; *c; c++) *c = (char)tolower((unsigned char)*c);
        for (char *c = d; *c; c++) *c = (char)tolower((unsigned char)*c);
        label(S.pixel6, t, cx - text_w(S.pixel6, t) / 2, l.info.y + 57 * u - lh / 2, text_w(S.pixel6, t), lh, 0xece4cc, 1);
        int lines = 1;
        for (const char *c = d; *c; c++) lines += *c == '\n';
        label(S.pixel6, d, cx, l.info.y + 36 * u - lines * lh / 2, 0, lines * lh, 0xece4cc, 1);
    }
    FOSprite sbg = ui("items_selection_shadowbg");
    image(&sbg, l.shadowBg.x, l.shadowBg.y, l.shadowBg.w, l.shadowBg.h);
    FOSprite sh = ui(kClassShadows[gSel.cls]);
    image(&sh, l.shadow.x, l.shadow.y, l.shadow.w, l.shadow.h);
    FOLoadout keep = gLoadouts[gSel.cls];
    gLoadouts[gSel.cls] = gSel.loadout;
    item_char(world, gSel.cls, l.character.x, l.character.y, 0, &gSel.look);
    gLoadouts[gSel.cls] = keep;
}

// aer.java: the first tap on a cell selects it. A tap on the selected cell
// equips the item, or takes it off. A weapon that comes off gives the
// default weapon back.
static void select_tap(int i) {
    if (i < 0 || i >= gSel.count) return;
    int id = gSel.items[i];
    if (gSel.armed == i) {
        const FOItem *it = fo_item(id);
        int on = item_equipped(id);
        if (it && it->kind == FO_ITEM_HAT) gSel.loadout.hat = on ? 0 : id;
        if (it && it->kind == FO_ITEM_WEAPON) gSel.loadout.weapon = on ? 0 : id;
    }
    gSel.armed = i;
    if (gSel.selected != i) {
        gSel.previous = gSel.selected;
        gSel.selected = i;
        gSel.selectTime = 0;
    }
}

FOScreenResult fo_select_touch(int id, int phase, float x, float y) {
    FOScreenResult r = {0};
    FOSelectLayout l = select_layout();
    if (phase == FO_TOUCH_BEGAN) {
        if (back_tap(x, y)) {
            r.action = FO_SCREEN_BACK;
            r.click = 1;
            return r;
        }
        if (gSel.touch < 0 && inside(x, y, l.inner.x, l.inner.y, l.inner.w, l.inner.h)) {
            gSel.touch = id;
            gSel.touchY = y;
            gSel.touchScroll = gSel.scroll;
            gSel.touchMove = 0;
        }
        return r;
    }
    if (id != gSel.touch) return r;
    // The list follows the finger. A finger that moved less than 3 units is a tap.
    float d = y - gSel.touchY;
    if (fabsf(d) > gSel.touchMove) gSel.touchMove = fabsf(d);
    gSel.scroll = gSel.touchScroll + d;
    float max = select_max_scroll(&l);
    if (gSel.scroll < 0) gSel.scroll = 0;
    if (gSel.scroll > max) gSel.scroll = max;
    if (phase == FO_TOUCH_ENDED) {
        gSel.touch = -1;
        if (gSel.touchMove < 3 * gU && inside(x, y, l.inner.x, l.inner.y, l.inner.w, l.inner.h)) {
            int c = x < l.inner.x + l.cellW ? 0 : 1;
            int row = (int)floorf((l.inner.y + l.inner.h + gSel.scroll - y) / l.cellH);
            int i = row * 2 + c;
            if (i < gSel.count) {
                select_tap(i);
                r.click = 1;
            }
        }
    }
    return r;
}
