#include "fo_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <mach/mach.h>
#include "fo_assets.h"
#include "fo_camera.h"
#include "fo_controls.h"
#include "fo_debug.h"
#include "fo_font.h"
#include "fo_gfx.h"
#include "fo_bots.h"
#include "fo_hud.h"
#include "fo_screens.h"
#include "fo_sound.h"
#include "fo_effects.h"
#include "fo_ambient.h"
#include "fo_audio.h"
#include "fo_look.h"
#include "fo_maps.h"
#include "fo_prefs.h"
#include "fo_items.h"
#include "fo_world.h"

// The size of one art pixel in meters, as GANGFORT draws each region.
#define FO_ART 0.1f
// The id of the local player in single player (xa.java:24).
#define FO_LOCAL_ID 69

static struct {
    char error[512];
    int width, height;
    float time;
    FOAtlasTextures sprites;
    FOAtlasTextures ui;       // res/textures/ui.atlas: the menus.
    FOAtlasTextures splash;   // res/textures/splash.atlas, until the splash ends.
    int splashLoaded;
    FOPrefs prefs;            // The settings, next to the APK.
    FOLoadout loadouts[10];   // The items of each class (items_equipment_settings_array).
    int hatScores[FO_MAX_PLAYERS];
    FOWorld *menuWorld;       // An empty world: its clock moves the characters of the menus.  // s of GoldenHat: the score at the last golden drops.
    FOFont pixel4, pixel6, pixel12, pixel21;
    FOFont cjk;               // res/fonts/japanese.fnt or korean.fnt.
    char language[8], country[8];
    FOText *texts;
    FOMap map;
    int mapLoaded;
    FOBots bots;
    int maxPlayers;           // The players of a match with the human: 8, 10, or 12.
    int screen;               // FO_VIEW_*.
    int dialog;               // The dialog over the match, or FO_DIALOG_NONE.
    int botsJoined;           // h of xa.java: the bots joined the match.
    FOMixer *mixer;
    int wasOver;              // The round was over in the last frame.
    // The kill cam (aeb.java:237-286, 723-788).
    int killcam;              // o
    int captured;             // n: the frame of the kill cam is in snapshot.
    float killStart;          // p
    float deathAt;            // q: the time of the death, or 0.
    int wasDead;              // r
    int sentryKill;           // t: a sentry killed the local player.
    int follow;               // The id of the player that the camera follows while dead.
    float shotX, shotY;       // s: the killer on the screen.
    FOTexture snapshot;
    // The fade of the music (aeb.java:791-796).
    float musicFrom, musicTo, musicStart, musicTime;
    FOLook looks[FO_MAX_PLAYERS];
    FOCamera camera;
    FOControls controls;
    int localClass;
    short localTeam;
    int selectClass;          // The class of the item selection screen.
    short setupTeam;          // The colours of the control setup: a random team (adh.java:66).
    int debugMove;
    int aimHeld;
    FOTexture scopeMask;     // res/textures/sniper_aim_alpha_mask.png             // j of aeb.java: the shoot stick was held in the last frame.
    const char *icons[FO_BUTTON_COUNT];
    int debugShoot;
    float debugAim;
    int noVignette;           // A move of the debug build, or 0.
    double updateTime, drawTime;  // The sums of the times of the parts, for the speed log.
    // The speed log.
    float fpsTime;
    int fpsFrames;
    float fps;
    float frameDt;            // The time of the last frame, for the fades.
    int debugScore;
} G;

enum { FO_VIEW_SPLASH = 0, FO_VIEW_MAIN, FO_VIEW_CREDITS, FO_VIEW_SETTINGS, FO_VIEW_CONTROLS, FO_VIEW_ITEMS,
       FO_VIEW_SELECT, FO_VIEW_SINGLE, FO_VIEW_MATCH };
enum { FO_DIALOG_NONE = 0, FO_DIALOG_TEAM, FO_DIALOG_CLASS, FO_DIALOG_MENU };

static void fail(const char *message) {
    snprintf(G.error, sizeof(G.error), "%s", message);
    fprintf(stderr, "fo_game: %s\n", message);
}

// GameScreen.a(float, long) (aeb.java:791-796): the music goes to volume
// in the time.
static float music_now(void) {
    if (G.musicTime <= 0) return G.musicTo;
    float t = (G.time - G.musicStart) / G.musicTime;
    return (t < 1 ? t : 1) * (G.musicTo - G.musicFrom) + G.musicFrom;
}

static void music_fade(float volume, float seconds) {
    G.musicFrom = music_now();
    G.musicTo = volume;
    G.musicStart = G.time;
    G.musicTime = seconds;
}

// The settings of SettingsScreen, with the keys of the original (akb.java).
static int setting(const char *key) {
    return fo_prefs_int(&G.prefs, key, 1) != 0;
}

// The loadouts in the file: loadout_<class>_hat and loadout_<class>_weapon.
static void loadouts_load(void) {
    char key[48];
    for (int c = 1; c <= 9; c++) {
        snprintf(key, sizeof(key), "loadout_%d_hat", c);
        int hat = fo_prefs_int(&G.prefs, key, 0);
        snprintf(key, sizeof(key), "loadout_%d_weapon", c);
        int weapon = fo_prefs_int(&G.prefs, key, 0);
        // fo_player_set_loadout keeps only the items that the class can use.
        FOPlayer check;
        memset(&check, 0, sizeof(check));
        fo_player_set_loadout(&check, c, hat, weapon);
        G.loadouts[c] = check.loadout[c];
    }
    fo_screens_loadouts(G.loadouts);
}

static void loadouts_save(void) {
    char key[48];
    for (int c = 1; c <= 9; c++) {
        snprintf(key, sizeof(key), "loadout_%d_hat", c);
        fo_prefs_set_int(&G.prefs, key, G.loadouts[c].hat);
        snprintf(key, sizeof(key), "loadout_%d_weapon", c);
        fo_prefs_set_int(&G.prefs, key, G.loadouts[c].weapon);
    }
    if (!fo_prefs_save(&G.prefs)) fprintf(stderr, "prefs: cannot write %s\n", G.prefs.path);
    fo_screens_loadouts(G.loadouts);
}

static const char *const kSettingPrefs[FO_SETTING_COUNT] = {"killcam_enabled", "theme_music_enabled",
                                                            "vignette_enabled"};

void fo_game_locale(const char *language, const char *country) {
    snprintf(G.language, sizeof(G.language), "%s", language ? language : "");
    snprintf(G.country, sizeof(G.country), "%s", country ? country : "");
}

// I18NBundle.createBundle: the texts of the language and the country, then
// of the language, then the default texts.
static FOText *load_texts(void) {
    char path[96];
    char *text = (char *)fo_assets_read("texts/gangforttexts.properties", NULL);
    FOText *texts = text ? fo_text_parse(text) : NULL;
    free(text);
    for (int k = 0; k < 2 && G.language[0]; k++) {
        if (k == 0) snprintf(path, sizeof(path), "texts/gangforttexts_%s.properties", G.language);
        else if (G.country[0]) snprintf(path, sizeof(path), "texts/gangforttexts_%s_%s.properties", G.language, G.country);
        else break;
        text = (char *)fo_assets_read(path, NULL);
        FOText *more = text ? fo_text_parse(text) : NULL;
        free(text);
        if (!more) continue;
        fo_text_set_parent(more, texts);
        texts = more;
        fprintf(stderr, "texts: %s\n", path);
    }
    return texts;
}

const char *fo_game_error(void) {
    return G.error;
}

// The resident memory of the app, for the speed log.
static double resident_mb(void) {
    struct task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) return 0;
    return info.resident_size / 1048576.0;
}

static double wall_seconds(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + now.tv_usec / 1e6;
}

static FOPlayer *local_player(void) {
    return G.mapLoaded ? fo_world_player(G.map.world, FO_LOCAL_ID) : NULL;
}

// Loads the map. The local player joins with no team and no class, and
// the team dialog opens (xa.java:28-52, aeb.java:98). With play, the local
// player takes the team and the class of the debug commands at once.
static int start_map(FOMapId id, int play) {
    fo_sound_stop_all();
    G.wasOver = 0;
    G.killcam = 0;
    G.deathAt = 0;
    G.wasDead = 0;
    G.follow = 0;
    fo_hud_killcam(NULL);
    fo_sound_fade(1.0f, 0);
    // The music of a match (aeb.java:98-99).
    music_fade(0.4f, 0);
    if (G.mapLoaded) fo_map_free(&G.map);
    G.mapLoaded = 0;
    double start = wall_seconds();
    if (!fo_map_load(&G.map, id, (unsigned int)(start * 1000))) return 0;
    G.mapLoaded = 1;
    G.screen = FO_VIEW_MATCH;
    G.map.world->localId = FO_LOCAL_ID;
    FOPlayer *p = fo_world_add_player(G.map.world, FO_LOCAL_ID, FO_TEAM_NONE);
    // SingleplayerClient (xa.java:35): the local player is "you".
    if (G.texts) snprintf(p->name, sizeof(p->name), "%s", fo_text_get(G.texts, "you"));
    // SingleplayerGameController (xa.java:38-50): the items of each class.
    for (int c = 1; c <= 9; c++) fo_player_set_loadout(p, c, G.loadouts[c].hat, G.loadouts[c].weapon);
    memset(G.hatScores, 0, sizeof(G.hatScores));
    fo_bots_init(&G.bots, G.map.world, G.maxPlayers, FO_LOCAL_ID);
    G.botsJoined = 0;
    memset(G.looks, 0, sizeof(G.looks));
    // GameCamera (wp.java:20-22): the camera starts on the objective.
    float x, y;
    fo_mode_objective(G.map.world, &x, &y);
    fo_camera_init(&G.camera, G.width, G.height, x, y);
    fo_hud_map(fo_map_name(id));
    fo_effects_reset(id, G.time);
    fo_ambient_reset(G.map.world);
    G.dialog = FO_DIALOG_TEAM;
    if (play) {
        fo_world_change_team(G.map.world, p, G.localTeam);
        fo_world_change_class(G.map.world, p, G.localClass);
        fo_bots_fill(&G.bots, G.map.world);
        G.botsJoined = 1;
        G.dialog = FO_DIALOG_NONE;
        fo_player_position(p, &x, &y);
        fo_camera_init(&G.camera, G.width, G.height, x, y);
    }
    fprintf(stderr, "map %s: %.3f s, textures %.1f MB\n", fo_map_name(id), wall_seconds() - start,
            fo_assets_texture_bytes() / 1048576.0);
    return 1;
}

// Opens a menu screen. A match ends, and the music plays at 1.
static void open_screen(int screen) {
    if (G.mapLoaded) {
        fo_sound_stop_all();
        G.killcam = 0;
        fo_hud_killcam(NULL);
        fo_map_free(&G.map);
        G.mapLoaded = 0;
    }
    music_fade(1.0f, 0);
    G.screen = screen;
    G.dialog = FO_DIALOG_NONE;
    if (screen == FO_VIEW_SINGLE) fo_single_open();
    if (screen == FO_VIEW_CONTROLS) G.setupTeam = rand() % 2 ? FO_TEAM_RED : FO_TEAM_BLU;
    if (screen == FO_VIEW_SETTINGS) {
        int on[FO_SETTING_COUNT];
        for (int i = 0; i < FO_SETTING_COUNT; i++) on[i] = setting(kSettingPrefs[i]);
        fo_settings_open(on);
    }
}

static void open_single(void) {
    open_screen(FO_VIEW_SINGLE);
}

int fo_game_start(const char *apk_path, int width, int height) {
    freopen("/tmp/FortOS.log", "w", stderr);
    setvbuf(stderr, NULL, _IOLBF, 0);
    double start = wall_seconds();
    G.width = width;
    G.height = height;
    G.localClass = FO_CLASS_SOLDIER;
    G.localTeam = FO_TEAM_BLU;
    G.maxPlayers = 12;
    // The settings are in the folder of the APK.
    char prefs[512];
    snprintf(prefs, sizeof(prefs), "%s", apk_path);
    char *slash = strrchr(prefs, '/');
    snprintf(slash ? slash + 1 : prefs, sizeof(prefs) - (slash ? (size_t)(slash + 1 - prefs) : 0), "FortOS.prefs");
    fo_prefs_load(&G.prefs, prefs);
    if (!fo_assets_open(apk_path)) {
        fail("Copy the APK of GANGFORT 2.1 from your copy of the game to\n"
             "/var/mobile/Media/FortOS/GANGFORT.apk\n\n"
             "FortOS needs it for the art, the maps, and the sounds.");
        return 0;
    }
    if (!fo_gfx_init()) {
        fail("OpenGL ES 2.0 does not start.");
        return 0;
    }
    // The music plays from the start. The sounds decode on a second thread.
    G.mixer = fo_mixer_new(FO_AUDIO_RATE);
    if (G.mixer) {
        fo_sound_init(G.mixer);
        if (!fo_audio_start(G.mixer)) fprintf(stderr, "audio: no output\n");
    }
    G.texts = load_texts();
    if (!fo_assets_atlas(&G.sprites, "textures/sprites.atlas") || !fo_font_load(&G.pixel4, "fonts/pixel4.fnt") ||
        !fo_font_load(&G.pixel6, "fonts/pixel6.fnt") ||
        !fo_font_load(&G.pixel12, "fonts/pixel12.fnt") || !fo_font_load(&G.pixel21, "fonts/pixel21.fnt") ||
        !fo_assets_atlas(&G.ui, "textures/ui.atlas")) {
        fail("The APK does not hold the files of GANGFORT 2.1.");
        return 0;
    }
    // ResourceManager.c() and alf.a(...) (akz.java:49-56, alf.java:104-128):
    // Japanese and Korean draw all the texts with one font, at 4, 6, 12, and
    // 21 fortieths of its size.
    const char *cjk = strcmp(G.language, "ja") == 0 ? "fonts/japanese.fnt" : strcmp(G.language, "ko") == 0 ? "fonts/korean.fnt" : NULL;
    if (cjk && fo_font_load(&G.cjk, cjk)) {
        G.pixel4 = fo_font_sized(&G.cjk, 4 / 40.0f);
        G.pixel6 = fo_font_sized(&G.cjk, 6 / 40.0f);
        G.pixel12 = fo_font_sized(&G.cjk, 12 / 40.0f);
        G.pixel21 = fo_font_sized(&G.cjk, 21 / 40.0f);
    }
    fo_controls_layout(&G.controls, width, height);
    fo_controls_load(&G.controls, &G.prefs);
    fo_look_init(&G.sprites);
    fo_effects_init(&G.sprites);
    fo_ambient_init(&G.sprites);
    G.scopeMask = fo_assets_texture("textures/sniper_aim_alpha_mask.png");
    FOHudFonts fonts = {&G.pixel4, &G.pixel6, &G.pixel12, &G.pixel21};
    fo_hud_init(&G.sprites, &fonts, G.texts, width, height);
    G.splashLoaded = fo_assets_atlas(&G.splash, "textures/splash.atlas");
    FOScreensSetup screens = {&G.sprites, &G.ui,     G.splashLoaded ? &G.splash : NULL,
                              &G.pixel4,  &G.pixel6, &G.pixel12,
                              &G.pixel21, G.texts,   width,
                              height};
    fo_screens_init(&screens);
    loadouts_load();
    // SplashScreen: no music until the main menu (ahh.java).
    G.screen = FO_VIEW_SPLASH;
    music_fade(0.0f, 0);
    fo_splash_open();
    fprintf(stderr, "start: %.3f s\n", wall_seconds() - start);
    return 1;
}

// The ability buttons of ControlsHUD.a(xn) (ahq.java:119-250): which ones
// show, and their icons.
static void update_buttons(FOPlayer *p, const char *icons[FO_BUTTON_COUNT]) {
    FOWorld *world = G.map.world;
    FOButton *ability = &G.controls.buttons[FO_BUTTON_ABILITY];
    FOButton *second = &G.controls.buttons[FO_BUTTON_SECOND];
    FOButton *third = &G.controls.buttons[FO_BUTTON_THIRD];
    ability->shown = second->shown = third->shown = 0;
    icons[FO_BUTTON_ABILITY] = icons[FO_BUTTON_SECOND] = icons[FO_BUTTON_THIRD] = NULL;
    icons[FO_BUTTON_MENU] = "menu";
    if (!p || !p->alive) return;
    if (p->cls == FO_CLASS_ENGINEER) {
        int blocked = fo_world_in_own_spawn(world, p) || fo_world_in_area(world, p, FO_AREA_NO_SENTRY);
        int ready = p->sentry.active && fo_sentry_is_ready(world, &p->sentry);
        if ((p->sentry.active && !ready) || (blocked && !p->sentry.active)) {
            ability->shown = 0;
        } else {
            ability->shown = ready || p->steel >= 100.0f;
        }
        icons[FO_BUTTON_ABILITY] = p->sentry.active ? "destroy_sentry" : "build_sentry";
    } else if (p->cls == FO_CLASS_SPY) {
        ability->shown = 1;
        icons[FO_BUTTON_ABILITY] = "toggle_cloak";
        if (!p->cloaked) {
            second->shown = 1;
            icons[FO_BUTTON_SECOND] = "backstab";
            int enemies = 0;
            for (int i = 0; i < FO_MAX_PLAYERS; i++) {
                const FOPlayer *e = &world->players[i];
                if (e->used && e->team != p->team && e->team != FO_TEAM_NONE && e->cls != 0) enemies++;
            }
            third->shown = !p->disguise.active && enemies > 0 && !p->backstab;
            icons[FO_BUTTON_THIRD] = "disguise";
        }
    } else if (p->cls == FO_CLASS_MEDIC) {
        ability->shown = p->uberCharge >= 20.0f;
        icons[FO_BUTTON_ABILITY] = "uber_activate";
    }
}

// GameScreen.render, section 1 (aeb.java:142-235), and the shot of the
// single player controller (xa.java:52-55): the input of the local player.
static void apply_input(FOPlayer *p) {
    if (!p || !p->alive) {
        G.aimHeld = 0;
        return;
    }
    FOWorld *world = G.map.world;
    if (fo_controls_jump(&G.controls)) fo_player_jump(world, p);
    p->move = G.debugMove ? G.debugMove : fo_controls_move(&G.controls);
    int touched = fo_controls_shoot_touched(&G.controls) || G.debugShoot;
    int moved = fo_controls_shoot_moved(&G.controls) || G.debugShoot;
    if (touched) {
        // A touch of the stick sets the aim, even with no move of the knob.
        float angle = G.debugShoot ? G.debugAim : fo_controls_shoot_angle(&G.controls);
        if (!G.debugShoot && G.controls.shoot.dx == 0 && G.controls.shoot.dy == 0) angle = 0;
        fo_player_set_aim(p, angle);
        G.aimHeld = 1;
    } else {
        // The sniper shoots when the finger leaves the stick, and the
        // camera stays still for 420 ms (aeb.java:167-170).
        if (G.aimHeld && p->cls == FO_CLASS_SNIPER && fo_player_can_shoot(world, p)) {
            fo_player_shoot(world, p, p->weapon.angle * 0.017453292f);
            G.camera.lockTime = 0.42f;
        }
        G.aimHeld = 0;
        fo_player_set_aim(p, 0);
    }
    // The other classes shoot while the knob is out a quarter of the base,
    // and only with ammo (aeb.java:172-190).
    int shoot = moved && p->cls != FO_CLASS_SNIPER && p->weapon.ammo > 0;
    if (shoot && fo_player_can_shoot(world, p)) fo_player_shoot(world, p, p->weapon.angle * 0.017453292f);
    // The ability buttons.
    if (fo_controls_take(&G.controls, FO_BUTTON_ABILITY)) fo_player_ability(world, p);
    if (fo_controls_take(&G.controls, FO_BUTTON_SECOND) && p->cls == FO_CLASS_SPY) fo_player_backstab(world, p);
    if (fo_controls_take(&G.controls, FO_BUTTON_THIRD) && p->cls == FO_CLASS_SPY) {
        if (p->disguise.active) {
            fo_player_undisguise(world, p);
        } else {
            fo_player_disguise(world, p, FO_LOCAL_ID);
        }
    }
}

// A tap on the world (aed.java): a medic heals the teammate under the
// finger, in 8 m of the medic and 1.8 m of the tap.
static void tap_world(float sx, float sy) {
    FOPlayer *p = local_player();
    if (!p || !p->alive || p->cls != FO_CLASS_MEDIC) return;
    float wx = G.camera.shownX - G.camera.viewW / 2 + sx / G.width * G.camera.viewW;
    float wy = G.camera.shownY - G.camera.viewH / 2 + sy / G.height * G.camera.viewH;
    float px, py;
    fo_player_position(p, &px, &py);
    FOWorld *world = G.map.world;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *t = &world->players[i];
        if (!t->used || !t->alive || t->id == p->id) continue;
        if (t->team != p->team && !(t->cls == FO_CLASS_SPY && t->disguise.active)) continue;
        float tx, ty;
        fo_player_position(t, &tx, &ty);
        float dm = sqrtf((tx - px) * (tx - px) + (ty - py) * (ty - py));
        float dt = sqrtf((tx - wx) * (tx - wx) + (ty - wy) * (ty - wy));
        if (dm <= 8.0f && dt <= 1.8f) {
            fo_player_heal(world, p, t->id);
            return;
        }
    }
}

// The pass of the scope (aeb.java:290-318): the blurred map over the
// world, with a sharp cone in front of the rifle.
static void draw_scope(const FOPlayer *p) {
    if (!p || !p->alive || p->cls != FO_CLASS_SNIPER || !p->weapon.def || p->weapon.angle == 0) return;
    FOSprite blurred = fo_assets_sprite(&G.map.atlas, "blurred_map");
    if (!blurred.w || !G.scopeMask.id) return;
    const FOLook *look = &G.looks[p - G.map.world->players];
    float a = p->weapon.angle * 0.017453292f;
    float mw = G.scopeMask.width * 0.1f, mh = G.scopeMask.height * 0.1f;
    float mx = look->scopeX - mw / 2 + (mw / 2) * cosf(a);
    float my = look->scopeY - mh / 2 + (mw / 2) * sinf(a);
    fo_gfx_scope_begin(&G.scopeMask, mx, my, mw, mh, a);
    float w = blurred.w * 0.1f, h = blurred.h * 0.1f;
    float mapW = G.map.world->map.width, mapH = G.map.world->map.height;
    fo_gfx_draw(&blurred, -(w - mapW) / 2, -(h - mapH) / 2, w, h, 0, 0, 0, 0, FO_WHITE);
    fo_gfx_scope_end();
}

static void draw_world(void) {
    float hw = G.camera.viewW / 2, hh = G.camera.viewH / 2;
    float left = G.camera.shownX - hw, bottom = G.camera.shownY - hh;
    float right = G.camera.shownX + hw, top = G.camera.shownY + hh;
    fo_gfx_view(left, bottom, right, top);
    fo_map_draw_back(&G.map, &G.sprites, G.camera.shownX, left, bottom, right, top);
    FOWorld *world = G.map.world;
    float now = world->now / 1000.0f;
    fo_ambient_draw(G.time);
    fo_look_bomb(world);
    fo_effects_draw_behind(world, G.time);
    // The layers of wz.java: the dead bodies, the projectiles, the players.
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used) fo_look_dead(world, &world->players[i], now);
    }
    fo_look_flags(world, now);
    // The map property koth_cp_indicator_pos of gantown.tmx is (582, 205.5).
    // The original scales it by 0.1 and does not turn the y axis (akm.java:113-127).
    if (G.map.id == FO_MAP_GANTOWN) fo_look_koth(world, 58.2f, 20.55f);
    fo_look_bullets(world, now);
    fo_look_drops(world);
    fo_look_pickups(world);
    FOPlayer *viewer = local_player();
    fo_look_beams(world);
    fo_look_health_bars(world, viewer);
    fo_look_fallen_hats(world, now);
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *p = &world->players[i];
        if (!p->used || !p->alive) continue;
        fo_look_player(world, p, &G.looks[i], now, world->accumulator, viewer);
        // The hat falls from the place where the player was drawn.
        if (G.looks[i].hatShown) fo_player_hat_place(p, G.looks[i].hatX, G.looks[i].hatY);
    }
    fo_look_sentries(world, now);
    fo_effects_draw_sentry(world, G.time);
    fo_hud_bomb_hint(world, (float)G.frameDt);
    fo_map_draw_doors(&G.map, &G.sprites);
    fo_effects_draw_top(world, G.time);
    fo_map_draw_front(&G.map, &G.sprites, left, bottom, right, top);
    draw_scope(viewer);
}

static void draw_hud(void) {
    fo_gfx_view(0, 0, (float)G.width, (float)G.height);
    float u = G.width / 281.0f;
    FOPlayer *p = local_player();
    fo_hud_view(G.camera.shownX - G.camera.viewW / 2, G.camera.shownY - G.camera.viewH / 2, G.camera.viewW,
                G.camera.viewH);
    fo_hud_draw(G.map.world, p, 0);
    if (p && p->alive && p->cls == FO_CLASS_SNIPER && p->weapon.angle != 0) {
        const FOLook *look = &G.looks[p - G.map.world->players];
        float a = p->weapon.angle * 0.017453292f;
        float wx = look->scopeX - cosf(a) * 1.0f, wy = look->scopeY - sinf(a) * 1.0f;
        float sx = (wx - (G.camera.shownX - G.camera.viewW / 2)) / G.camera.viewW * G.width;
        float sy = (wy - (G.camera.shownY - G.camera.viewH / 2)) / G.camera.viewH * G.height;
        fo_hud_sniper(sx, sy, a, p->sniperCharge);
    }
    update_buttons(p, G.icons);
    // GameScreen.e() (aeb.java:636-660): the controls hide while the local
    // player is dead, at the end of a round, and while the bomb is down.
    if (G.dialog == FO_DIALOG_NONE && p && p->alive && !fo_mode_over(G.map.world) && !fo_mode_bomb_down(G.map.world)) {
        fo_controls_draw(&G.controls, &G.sprites, p ? p->team : FO_TEAM_BLU, p && p->cls == FO_CLASS_SNIPER, G.icons);
    }
    fo_hud_round_over(G.map.world, p, G.frameDt);
    char text[128];
    snprintf(text, sizeof(text), "%.0f fps", G.fps);
    fo_font_draw(&G.pixel6, text, G.width - 3 * u, G.height - 3 * u, u * 0.5f, fo_rgb(0xffb866), FO_ALIGN_RIGHT);
}

// GameScreen.a(boolean) (aeb.java:723-770): the kill cam starts 2 s after
// the death, when the killer can be seen, and ends 2 s later.
static void killcam_update(FOPlayer *p) {
    FOWorld *w = G.map.world;
    for (int i = 0; i < w->event_count; i++) {
        const FOEvent *e = &w->events[i];
        if (e->kind == FO_EVENT_KILL && e->b == FO_LOCAL_ID) G.sentryKill = e->item == FO_ITEM_SENTRY;
    }
    int dead = p && fo_player_waiting(p);
    if (p && dead != G.wasDead) {
        G.wasDead = dead;
        if (dead) G.deathAt = G.time;
        else G.follow = 0;
    }
    if (G.deathAt != 0 && G.time - G.deathAt >= 2.0f && !G.killcam && p) {
        const FOPlayer *k = p->killer != FO_LOCAL_ID ? fo_world_player(w, p->killer) : NULL;
        int hidden = k && k->cls == FO_CLASS_SPY && k->cloaked;
        // With the kill cam off, the original shows its ads here (aeb.java:245-249).
        int on = setting("killcam_enabled");
        if (on && k && !hidden && !fo_mode_over(w) && !fo_mode_bomb_down(w) && (k->alive || k->corpse.active)) {
            G.killcam = 1;
            G.captured = 0;
            G.killStart = G.time;
            fo_sound_global("killzoom");
            fo_hud_killcam(k->name);
            fprintf(stderr, "killcam: %s\n", k->name);
            music_fade(0.0f, 0.1f);
            fo_sound_fade(0.0f, 0.1f);
        }
    }
    if (G.killcam && (G.time - G.killStart >= 2.0f || fo_mode_bomb_down(w) || fo_mode_over(w))) {
        G.killcam = 0;
        G.deathAt = 0;
        fo_hud_killcam(NULL);
        fo_sound_fade(1.0f, 0.5f);
        music_fade(0.4f, 0.5f);
    }
}

// GoldenHat.a(float) (abn.java:73-81): at each point of its player, a hat
// in the view drops 3 to 10 golden pixels from its bottom.
static void golden_drops(void) {
    FOWorld *w = G.map.world;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        FOPlayer *p = &w->players[i];
        if (!p->used || !p->alive || p->hat != 1 || !G.looks[i].hatShown || p->score == G.hatScores[i]) continue;
        G.hatScores[i] = p->score;
        float hx = G.looks[i].hatX, hy = G.looks[i].hatY;
        if (fabsf(hx - G.camera.shownX) > G.camera.viewW / 2 + 0.25f || fabsf(hy - G.camera.shownY) > G.camera.viewH / 2 + 0.25f)
            continue;
        const FOItem *it = fo_item(1);
        int n = 3 + rand() % 8;
        for (int k = 0; k < n; k++) {
            fo_world_drops(w, hx - 0.3f + 0.6f * (float)rand() / (float)RAND_MAX, hy - it->h / 2, 1);
        }
    }
}

// The first frame of the kill cam: the camera on the killer, and the frame
// in the snapshot (aeb.java:251-275).
static void killcam_capture(void) {
    FOWorld *w = G.map.world;
    const FOPlayer *p = local_player();
    const FOPlayer *k = p ? fo_world_player(w, p->killer) : NULL;
    float x = G.camera.shownX, y = G.camera.shownY;
    G.follow = 0;
    if (k) {
        if (k->alive) {
            fo_player_position(k, &x, &y);
            G.follow = k->id;
        } else if (k->corpse.obj.body) {
            fo_phys_body_position(k->corpse.obj.body, &x, &y);
        }
        if (G.sentryKill && k->sentry.active && k->sentry.obj.body) {
            fo_phys_body_position(k->sentry.obj.body, &x, &y);
            G.follow = 0;
        }
    }
    fo_camera_init(&G.camera, G.width, G.height, x, y);
    fo_camera_update(&G.camera, 0, x, y, w->map.width, w->map.height);
    draw_world();
    fo_gfx_snapshot(&G.snapshot, G.width, G.height);
    G.shotX = (x - (G.camera.shownX - G.camera.viewW / 2)) / G.camera.viewW * G.width;
    G.shotY = (y - (G.camera.shownY - G.camera.viewH / 2)) / G.camera.viewH * G.height;
    G.captured = 1;
}

// GameScreen.g() (aeb.java:771-788): the frame zooms in 130 ms to 2.5 times
// on the killer.
static void killcam_draw(void) {
    float t = (G.time - G.killStart) / 0.13f;
    if (t > 1) t = 1;
    float zoom = 1.0f - 0.6f * t;
    float W = (float)G.width, H = (float)G.height, fw = W * zoom, fh = H * zoom;
    float cx = G.shotX, cy = G.shotY;
    if (cx < fw / 2) cx = fw / 2;
    if (cx > W - fw / 2) cx = W - fw / 2;
    if (cy < fh / 2) cy = fh / 2;
    if (cy > H - fh / 2) cy = H - fh / 2;
    fo_gfx_view(cx - fw / 2, cy - fh / 2, cx + fw / 2, cy + fh / 2);
    FOSprite s = {&G.snapshot, 0, 1, 1, 0, G.width, G.height};
    fo_gfx_draw(&s, 0, 0, W, H, 0, 0, 0, 0, FO_WHITE);
}

void fo_game_frame(double dt) {
    fo_debug_poll(dt);
    G.time += (float)dt;
    G.frameDt = (float)dt;
    G.fpsFrames++;
    G.fpsTime += (float)dt;
    if (G.fpsTime >= 1) {
        G.fps = G.fpsFrames / G.fpsTime;
        fprintf(stderr, "fps %.1f calls %d update %.2f ms draw %.2f ms memory %.1f MB\n", G.fps, fo_gfx_draw_calls(),
                G.updateTime * 1000 / G.fpsFrames, G.drawTime * 1000 / G.fpsFrames, resident_mb());
        G.updateTime = G.drawTime = 0;
        G.fpsFrames = 0;
        G.fpsTime = 0;
    }
    double t0 = wall_seconds();
    if (G.screen != FO_VIEW_MATCH) {
        if (G.screen == FO_VIEW_SPLASH && fo_splash_draw((float)dt)) {
            // The splash atlas is not used again.
            if (G.splashLoaded) fo_assets_atlas_free(&G.splash);
            G.splashLoaded = 0;
            fo_screens_init(&(FOScreensSetup){&G.sprites, &G.ui, NULL, &G.pixel4, &G.pixel6, &G.pixel12, &G.pixel21,
                                              G.texts, G.width, G.height});
            open_screen(FO_VIEW_MAIN);
        }
        if (G.screen == FO_VIEW_MAIN) fo_main_draw();
        if (G.screen == FO_VIEW_CREDITS) fo_credits_draw();
        if (G.screen == FO_VIEW_SETTINGS) fo_settings_draw();
        if (G.screen == FO_VIEW_CONTROLS) {
            fo_setup_draw(!G.controls.arrows);
            fo_controls_draw_edit(&G.controls, &G.sprites, G.setupTeam);
        }
        if (G.screen == FO_VIEW_ITEMS || G.screen == FO_VIEW_SELECT) {
            if (!G.menuWorld) G.menuWorld = calloc(1, sizeof(FOWorld));
            if (G.menuWorld) {
                G.menuWorld->now = (long long)(G.time * 1000);
                if (G.screen == FO_VIEW_ITEMS) fo_items_draw(G.menuWorld);
                else fo_select_draw(G.menuWorld, (float)dt);
            }
        }
        if (G.screen == FO_VIEW_SINGLE) fo_single_draw((float)dt);
        fo_sound_music(music_now() * setting("theme_music_enabled"));
        fo_gfx_flush();
        return;
    }
    FOPlayer *p = local_player();
    if (G.dialog == FO_DIALOG_NONE) {
        apply_input(p);
        if (fo_controls_take(&G.controls, FO_BUTTON_MENU)) G.dialog = FO_DIALOG_MENU;
    } else if (p) {
        p->move = FO_MOVE_STOP;
    }
    fo_bots_update(&G.bots, G.map.world, (float)dt);
    fo_world_update(G.map.world, (float)dt);
    for (int i = 0; i < FO_MAX_PLAYERS; i++) G.looks[i].stateTime += (float)dt;
    fo_hud_events(G.map.world, FO_LOCAL_ID, (float)dt);
    fo_effects_update(G.map.world, G.time);
    golden_drops();
    fo_ambient_update(G.map.world, G.camera.shownX - G.camera.viewW / 2, G.camera.shownY - G.camera.viewH / 2,
                      G.camera.viewW, G.camera.viewH, (float)dt, G.time);
    fo_map_update(&G.map, (float)dt);
    if (p && p->alive) {
        float x, y;
        fo_player_position(p, &x, &y);
        FOWorld *world = G.map.world;
        // GameCamera.a(alh) (wp.java:69-103): the sniper looks ahead of its aim.
        if (p->cls == FO_CLASS_SNIPER && p->weapon.def && p->weapon.angle != 0) {
            float c = cosf(p->weapon.angle * 0.017453292f), sn = sinf(p->weapon.angle * 0.017453292f);
            float fl = p->obj.w * 1.5f, fm = p->obj.h;
            float vw = G.camera.viewW, vh = G.camera.viewH;
            x += (-vw / 2 + fl) + ((vw / 2 - fl) - (-vw / 2 + fl)) * ((c + 1) / 2);
            y += (-vh / 2 + fm) + ((vh / 2 - fm) - (-vh / 2 + fm)) * ((sn + 1) / 2);
        }
        // The shake of a shot of the player, or of a hit on the player.
        if (p->weapon.def && world->now - p->weapon.lastShot < 100) {
            fo_camera_shake(&G.camera, p->weapon.def->shakeMs, p->weapon.def->shake);
        } else if (world->now - p->hurtTime < 300 && G.camera.shakeTime <= 0) {
            fo_camera_shake(&G.camera, 300, 0.075f);
        }
        if (!fo_mode_bomb_down(world)) fo_camera_update(&G.camera, (float)dt, x, y, world->map.width, world->map.height);
    }
    FOWorld *w = G.map.world;
    // The camera follows the killer after the kill cam (aeb.java:786).
    const FOPlayer *f = G.follow ? fo_world_player(w, G.follow) : NULL;
    if ((!p || !p->alive) && f && f->alive) {
        float fx, fy;
        fo_player_position(f, &fx, &fy);
        fo_camera_update(&G.camera, (float)dt, fx, fy, w->map.width, w->map.height);
    }
    for (int i = 0; i < w->event_count; i++) {
        const FOEvent *e = &w->events[i];
        if (e->kind == FO_EVENT_SHAKE) fo_camera_shake(&G.camera, e->item, e->value);
    }
    // GameWorld.j() (wt.java:149-155): when the bomb is down, the camera
    // looks at the place of the blast.
    if (fo_mode_bomb_down(w)) {
        float fx, fy;
        fo_mode_focus(w, &fx, &fy);
        fo_camera_update(&G.camera, (float)dt, fx, fy, w->map.width, w->map.height);
    }
    // GameWorld.a(float) (wt.java:79-89): the ear is on the local player, and
    // the sounds reach half the diagonal of the view and the way from the
    // camera to the player. With no living player, the ear is the camera.
    float half = sqrtf(G.camera.viewW * G.camera.viewW + G.camera.viewH * G.camera.viewH) / 2;
    float ex = G.camera.shownX, ey = G.camera.shownY, range = half;
    if (p && p->alive) {
        fo_player_position(p, &ex, &ey);
        range = half + sqrtf((ex - G.camera.shownX) * (ex - G.camera.shownX) + (ey - G.camera.shownY) * (ey - G.camera.shownY));
    }
    fo_sound_update(w, FO_LOCAL_ID, ex, ey, range, G.time);
    // The music: 0.4 in a match and 0 at the end of a round. The end plays
    // the sound of the victory or of the loss (aeb.java:325-345).
    int over = fo_mode_over(w);
    if (over && !G.wasOver) {
        music_fade(0.0f, 0.1f);
        if (p) fo_sound_global(p->team == fo_mode_winner(w) ? "game_over_victory" : "game_over_lost");
    } else if (!over && G.wasOver) {
        music_fade(0.4f, 1.0f);
    }
    G.wasOver = over;
    killcam_update(p);
    fo_sound_music(music_now() * setting("theme_music_enabled"));
    double t1 = wall_seconds();
    fo_gfx_begin(G.width, G.height, fo_rgb(0x000000));
    if (G.killcam) {
        if (!G.captured) killcam_capture();
        fo_gfx_begin(G.width, G.height, fo_rgb(0x000000));
        killcam_draw();
        draw_hud();
    } else {
        draw_world();
        draw_hud();
        if (!G.noVignette && setting("vignette_enabled")) fo_gfx_vignette();
    }
    fo_gfx_view(0, 0, (float)G.width, (float)G.height);
    if (G.dialog == FO_DIALOG_TEAM) fo_team_draw(G.map.world);
    else if (G.dialog == FO_DIALOG_CLASS) fo_class_draw(G.map.world, (float)dt);
    else if (G.dialog == FO_DIALOG_MENU) fo_menu_draw();
    fo_gfx_flush();
    G.updateTime += t1 - t0;
    G.drawTime += wall_seconds() - t1;
}

// The answer to a tap on a screen or a dialog.
static void screen_action(FOScreenResult r) {
    FOPlayer *p = local_player();
    if (r.click) fo_sound_global("menu_click1");
    switch (r.action) {
    case FO_SCREEN_PLAY:
        // SingleplayerScreen.ahc (ahc.java:22-28).
        G.maxPlayers = r.players;
        start_map(r.map == 0 ? FO_MAP_FLAG_EGYPT : r.map == 1 ? FO_MAP_BOMB_CANYON : FO_MAP_GANTOWN, 0);
        break;
    case FO_SCREEN_TEAM:
        // The team callback opens the class dialog (aeg.java, aec.java).
        if (p) fo_world_change_team(G.map.world, p, (short)r.team);
        G.dialog = FO_DIALOG_CLASS;
        fo_class_open(p ? p->cls : 0, p ? p->team : FO_TEAM_BLU);
        break;
    case FO_SCREEN_CLASS:
        // GameScreen.aeh and SingleplayerGameController.a(short)
        // (xa.java:105-115): the bots join after the first class.
        if (p) fo_world_change_class(G.map.world, p, r.cls);
        if (!G.botsJoined) {
            fo_bots_fill(&G.bots, G.map.world);
            G.botsJoined = 1;
        }
        G.dialog = FO_DIALOG_NONE;
        break;
    case FO_SCREEN_RESUME: G.dialog = FO_DIALOG_NONE; break;
    case FO_SCREEN_CHANGE_CLASS:
        G.dialog = FO_DIALOG_CLASS;
        fo_class_open(p ? p->cls : 0, p ? p->team : FO_TEAM_BLU);
        break;
    case FO_SCREEN_SWITCH_TEAM: G.dialog = FO_DIALOG_TEAM; break;
    case FO_SCREEN_QUIT: open_screen(FO_VIEW_MAIN); break;
    case FO_SCREEN_BACK:
        // The control setup saves the places and goes back to the settings
        // (adh.java:105-109). The other screens go to the main menu.
        if (G.screen == FO_VIEW_SELECT) {
            // ItemSelectionScreen.hide() saves the items of the class (aeo.java:281-297).
            G.loadouts[G.selectClass] = fo_select_loadout();
            loadouts_save();
            open_screen(FO_VIEW_ITEMS);
        } else if (G.screen == FO_VIEW_CONTROLS) {
            fo_controls_save(&G.controls, &G.prefs);
            if (!fo_prefs_save(&G.prefs)) fprintf(stderr, "prefs: cannot write %s\n", G.prefs.path);
            open_screen(FO_VIEW_SETTINGS);
        } else {
            open_screen(FO_VIEW_MAIN);
        }
        break;
    case FO_SCREEN_CONTROLS: open_screen(FO_VIEW_CONTROLS); break;
    case FO_SCREEN_ITEMS: open_screen(FO_VIEW_ITEMS); break;
    case FO_SCREEN_ITEMS_CLASS:
        fo_select_open(r.cls);
        G.selectClass = r.cls;
        open_screen(FO_VIEW_SELECT);
        break;
    case FO_SCREEN_CONTROLS_RESET:
        fo_controls_layout(&G.controls, G.width, G.height);
        fo_controls_save(&G.controls, &G.prefs);
        fo_prefs_save(&G.prefs);
        break;
    case FO_SCREEN_CONTROLS_STICKS: fo_controls_set_arrows(&G.controls, !r.value); break;
    case FO_SCREEN_SINGLE: open_screen(FO_VIEW_SINGLE); break;
    case FO_SCREEN_CREDITS: open_screen(FO_VIEW_CREDITS); break;
    case FO_SCREEN_SETTINGS: open_screen(FO_VIEW_SETTINGS); break;
    case FO_SCREEN_SETTING:
        fo_prefs_set_int(&G.prefs, kSettingPrefs[r.setting], r.value);
        if (!fo_prefs_save(&G.prefs)) fprintf(stderr, "prefs: cannot write %s\n", G.prefs.path);
        break;
    default: break;
    }
}

void fo_game_touch(int id, int phase, float x, float y) {
    float sy = G.height - y;
    if (G.screen != FO_VIEW_MATCH) {
        if (G.screen == FO_VIEW_CONTROLS) {
            if (phase == FO_TOUCH_BEGAN) {
                FOScreenResult r = fo_setup_tap(x, sy, !G.controls.arrows);
                if (r.action != FO_SCREEN_NONE) {
                    screen_action(r);
                    return;
                }
            }
            fo_controls_edit_touch(&G.controls, id, phase, x, sy);
            return;
        }
        if (G.screen == FO_VIEW_SELECT) {
            screen_action(fo_select_touch(id, phase, x, sy));
            return;
        }
        if (phase != FO_TOUCH_BEGAN) return;
        if (G.screen == FO_VIEW_ITEMS) screen_action(fo_items_tap(x, sy));
        else if (G.screen == FO_VIEW_MAIN) screen_action(fo_main_tap(x, sy));
        else if (G.screen == FO_VIEW_CREDITS) screen_action(fo_credits_tap(x, sy));
        else if (G.screen == FO_VIEW_SETTINGS) screen_action(fo_settings_tap(x, sy));
        else if (G.screen == FO_VIEW_SINGLE) screen_action(fo_single_tap(x, sy));
        return;
    }
    if (G.dialog != FO_DIALOG_NONE) {
        if (phase != FO_TOUCH_BEGAN) return;
        if (G.dialog == FO_DIALOG_TEAM) screen_action(fo_team_tap(x, sy));
        else if (G.dialog == FO_DIALOG_CLASS) screen_action(fo_class_tap(x, sy));
        else if (G.dialog == FO_DIALOG_MENU) screen_action(fo_menu_tap(x, sy));
        return;
    }
    if (!fo_controls_touch(&G.controls, id, phase, x, sy) && phase == FO_TOUCH_BEGAN) tap_world(x, sy);
}

void fo_game_debug(const char *command) {
    int n;
    char name[32];
    if (sscanf(command, "vignette %d", &n) == 1) G.noVignette = !n;
    if (sscanf(command, "map %d", &n) == 1 && n >= 0 && n < FO_MAP_COUNT) start_map((FOMapId)n, 1);
    if (sscanf(command, "class %d", &n) == 1 && n >= 1 && n <= 9) {
        G.localClass = n;
        FOPlayer *p = local_player();
        if (p) fo_world_change_class(G.map.world, p, n);
    }
    if (sscanf(command, "players %d", &n) == 1 && n >= 1 && n <= FO_MAX_PLAYERS) {
        G.maxPlayers = n;
        start_map(G.mapLoaded ? G.map.id : FO_MAP_GANTOWN, 1);
    }
    if (sscanf(command, "team %31s", name) == 1) {
        G.localTeam = strcmp(name, "red") == 0 ? FO_TEAM_RED : FO_TEAM_BLU;
        start_map(G.mapLoaded ? G.map.id : FO_MAP_GANTOWN, 1);
    }
    if (strncmp(command, "single", 6) == 0) open_single();
    int hat, weapon;
    if (sscanf(command, "loadout %d %d %d", &n, &hat, &weapon) == 3 && n >= 1 && n <= 9) {
        FOPlayer check;
        memset(&check, 0, sizeof(check));
        fo_player_set_loadout(&check, n, hat, weapon);
        G.loadouts[n] = check.loadout[n];
        loadouts_save();
    }
    if (strncmp(command, "mainmenu", 8) == 0) open_screen(FO_VIEW_MAIN);
    if (strncmp(command, "credits", 7) == 0) open_screen(FO_VIEW_CREDITS);
    if (strncmp(command, "settings", 8) == 0) open_screen(FO_VIEW_SETTINGS);
    if (strncmp(command, "controls", 8) == 0) open_screen(FO_VIEW_CONTROLS);
    if (strncmp(command, "items", 5) == 0) open_screen(FO_VIEW_ITEMS);
    // The commands below need a match.
    if (!G.mapLoaded) return;
    FOPlayer *p = local_player();
    if (sscanf(command, "move %d", &n) == 1) G.debugMove = n == FO_MOVE_STOP ? 0 : n;
    if (p && strncmp(command, "ability", 7) == 0) fo_player_ability(G.map.world, p);
    if (p && strncmp(command, "backstab", 8) == 0) fo_player_backstab(G.map.world, p);
    if (p && strncmp(command, "disguise", 8) == 0) fo_player_disguise(G.map.world, p, FO_LOCAL_ID);
    if (p && sscanf(command, "heal %d", &n) == 1) fo_player_heal(G.map.world, p, n);
    if (p && strncmp(command, "die", 3) == 0) fo_player_set_health(G.map.world, p, 0);
    if (p && strncmp(command, "dump", 4) == 0) {
        float x, y;
        fo_player_position(p, &x, &y);
        fprintf(stderr, "dump: player %d class %d at %.2f %.2f health %.0f steel %.0f\n", p->id, p->cls, x, y, p->health,
                p->steel);
        const FOSentry *se = &p->sentry;
        if (se->obj.body) fo_phys_body_position(se->obj.body, &x, &y);
        fprintf(stderr, "dump: sentry active %d landed %d built %lld at %.2f %.2f health %.0f\n", se->active,
                se->landed, se->built, x, y, se->health);
        const FOFallenHat *fh = &p->fallenHat;
        if (fh->obj.body) fo_phys_body_position(fh->obj.body, &x, &y);
        fprintf(stderr, "dump: hat %d placed %d at %.2f %.2f fallen %d at %.2f %.2f\n", p->hat, p->hatPlaced, p->hatX,
                p->hatY, fh->active, fh->obj.body ? x : 0, fh->obj.body ? y : 0);
        const FOMode *md = &G.map.world->mode;
        for (int t = 1; t <= 2 && md->kind == FO_MODE_CTF; t++) {
            fo_phys_body_position(md->flags[t].obj.body, &x, &y);
            fprintf(stderr, "dump: flag %d at %.2f %.2f carrier %d dropped %d\n", t, x, y, md->flags[t].carrier,
                    md->flags[t].dropped);
        }
    }
    if (strncmp(command, "players", 7) == 0 && (command[7] == 0 || command[7] == '\n')) {
        FOWorld *w = G.map.world;
        const FOMode *md = &w->mode;
        float bx = 0, by = 0;
        if (md->kind == FO_MODE_BOMB) fo_phys_body_position(md->bomb.obj.body, &bx, &by);
        fprintf(stderr, "audio voices %d\n", G.mixer ? fo_mixer_voices(G.mixer) : -1);
        fprintf(stderr, "mode %d over %d koth owner %d progress %.0f red %.0f blu %.0f bomb %.2f %.2f timer %.0f "
                        "ctf %d-%d carriers %d %d\n",
                md->kind, fo_mode_over(w), md->owner, md->progress, md->redTimer, md->bluTimer, bx, by, md->bombTimer,
                md->redScore, md->bluScore, md->flags[1].carrier, md->flags[2].carrier);
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            const FOPlayer *q = &w->players[i];
            if (!q->used) continue;
            float x = 0, y = 0;
            if (q->alive) fo_player_position(q, &x, &y);
            const FOBot *b = NULL;
            for (int k = 0; k < FO_MAX_PLAYERS; k++) {
                if (G.bots.bots[k].used && G.bots.bots[k].id == q->id) b = &G.bots.bots[k];
            }
            float vx = 0, vy = 0;
            if (q->alive) fo_player_velocity(q, &vx, &vy);
            int first = b && b->pathCount > 0 ? G.bots.graph.points[b->path[0]].id : 0;
            int last = b && b->pathCount > 0 ? G.bots.graph.points[b->path[b->pathCount - 1]].id : 0;
            fprintf(stderr, "player %d %-10s team %d class %d alive %d at %6.2f %6.2f v %5.2f %5.2f move %d hp %3.0f "
                            "ammo %3.0f score %d path %d (%d..%d) free %d paused %d nomove %d target %d\n",
                    q->id, q->name, q->team, q->cls, q->alive, x, y, vx, vy, q->move, q->health, q->weapon.ammo, q->score,
                    b ? b->pathCount : -1, first, last, b ? b->free : -1, b ? b->paused : -1, q->noMove,
                    b ? b->target : -1);
        }
    }
    if (p && strncmp(command, "jump", 4) == 0) fo_player_jump(G.map.world, p);
    float f;
    // The game modes: set the time of the round, move the local player to x,
    // or move the bomb to x.
    FOMode *m = &G.map.world->mode;
    if (sscanf(command, "modetime %f", &f) == 1) {
        if (m->kind == FO_MODE_BOMB) m->bombTimer = f;
        if (m->kind == FO_MODE_KOTH && m->owner == FO_TEAM_BLU) m->bluTimer = f;
        if (m->kind == FO_MODE_KOTH && m->owner != FO_TEAM_BLU) m->redTimer = f;
    }
    float g;
    if (p && sscanf(command, "goto %f %f", &f, &g) == 2) {
        fo_phys_body_set_transform(p->obj.body, f, g, 0);
        fo_phys_body_set_awake(p->obj.body, 1);
    }
    if (m->kind == FO_MODE_BOMB && sscanf(command, "bombat %f %f", &f, &g) == 2) {
        fo_phys_body_set_transform(m->bomb.obj.body, f, g, 0);
        fo_phys_body_set_awake(m->bomb.obj.body, 1);
    }
    if (m->kind == FO_MODE_CTF && sscanf(command, "score %d %d", &n, &G.debugScore) == 2) {
        m->redScore = n;
        m->bluScore = G.debugScore;
    }
    if (sscanf(command, "shoot %f", &f) == 1) {
        G.debugShoot = 1;
        G.debugAim = f;
    }
    if (strncmp(command, "stop", 4) == 0) G.debugShoot = 0;
    if (sscanf(command, "bot %d %d %f", &n, &n, &f) == 3) {
        int team = 0, cls = 0;
        sscanf(command, "bot %d %d %f", &team, &cls, &f);
        static int nextId = 100;
        FOPlayer *b = fo_world_add_player(G.map.world, nextId++, (short)team);
        if (b) {
            snprintf(b->name, sizeof(b->name), "bot%d", b->id);
            fo_world_spawn(G.map.world, b, cls);
            float x, y;
            fo_player_position(local_player(), &x, &y);
            fo_phys_body_set_transform(b->obj.body, x + f, y + 1, 0);
        }
    }
}
