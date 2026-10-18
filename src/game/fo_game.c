#include "fo_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "fo_assets.h"
#include "fo_camera.h"
#include "fo_controls.h"
#include "fo_debug.h"
#include "fo_font.h"
#include "fo_gfx.h"
#include "fo_hud.h"
#include "fo_look.h"
#include "fo_maps.h"
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
    FOFont pixel4, pixel6, pixel12;
    FOText *texts;
    FOMap map;
    int mapLoaded;
    FOLook looks[FO_MAX_PLAYERS];
    FOCamera camera;
    FOControls controls;
    int localClass;
    short localTeam;
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
} G;

static void fail(const char *message) {
    snprintf(G.error, sizeof(G.error), "%s", message);
    fprintf(stderr, "fo_game: %s\n", message);
}

const char *fo_game_error(void) {
    return G.error;
}

static double wall_seconds(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + now.tv_usec / 1e6;
}

static FOPlayer *local_player(void) {
    return G.mapLoaded ? fo_world_player(G.map.world, FO_LOCAL_ID) : NULL;
}

// Loads the map and spawns the local player in it.
static int start_map(FOMapId id) {
    if (G.mapLoaded) fo_map_free(&G.map);
    G.mapLoaded = 0;
    double start = wall_seconds();
    if (!fo_map_load(&G.map, id, (unsigned int)(start * 1000))) return 0;
    G.mapLoaded = 1;
    FOPlayer *p = fo_world_add_player(G.map.world, FO_LOCAL_ID, G.localTeam);
    fo_world_spawn(G.map.world, p, G.localClass);
    float x, y;
    fo_player_position(p, &x, &y);
    fo_camera_init(&G.camera, G.width, G.height, x, y);
    fprintf(stderr, "map %s: %.3f s, textures %.1f MB\n", fo_map_name(id), wall_seconds() - start,
            fo_assets_texture_bytes() / 1048576.0);
    return 1;
}

int fo_game_start(const char *apk_path, int width, int height) {
    freopen("/tmp/FortOS.log", "w", stderr);
    setvbuf(stderr, NULL, _IOLBF, 0);
    double start = wall_seconds();
    G.width = width;
    G.height = height;
    G.localClass = FO_CLASS_SOLDIER;
    G.localTeam = FO_TEAM_BLU;
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
    if (!fo_assets_atlas(&G.sprites, "textures/sprites.atlas") || !fo_font_load(&G.pixel4, "fonts/pixel4.fnt") ||
        !fo_font_load(&G.pixel6, "fonts/pixel6.fnt") ||
        !fo_font_load(&G.pixel12, "fonts/pixel12.fnt") || !start_map(FO_MAP_GANTOWN)) {
        fail("The APK does not hold the files of GANGFORT 2.1.");
        return 0;
    }
    fo_controls_layout(&G.controls, width, height);
    fo_look_init(&G.sprites);
    G.scopeMask = fo_assets_texture("textures/sniper_aim_alpha_mask.png");
    char *texts = (char *)fo_assets_read("texts/gangforttexts.properties", NULL);
    G.texts = texts ? fo_text_parse(texts) : NULL;
    free(texts);
    fo_hud_init(&G.sprites, &G.pixel4, G.texts, width, height);
    FOPlayer *you = local_player();
    if (you) snprintf(you->name, sizeof(you->name), "%s", fo_text_get(G.texts, "you"));
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
    // The layers of wz.java: the dead bodies, the projectiles, the players.
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used) fo_look_dead(world, &world->players[i], now);
    }
    fo_look_bullets(world, now);
    fo_look_pickups(world);
    FOPlayer *viewer = local_player();
    fo_look_beams(world);
    fo_look_health_bars(world, viewer);
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *p = &world->players[i];
        if (p->used && p->alive) fo_look_player(world, p, &G.looks[i], now, world->accumulator, viewer);
    }
    fo_look_sentries(world, now);
    fo_map_draw_front(&G.map, &G.sprites, left, bottom, right, top);
    draw_scope(viewer);
}

static void draw_hud(void) {
    fo_gfx_view(0, 0, (float)G.width, (float)G.height);
    float u = G.width / 281.0f;
    FOPlayer *p = local_player();
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
    fo_controls_draw(&G.controls, &G.sprites, p ? p->team : FO_TEAM_BLU, p && p->cls == FO_CLASS_SNIPER, G.icons);
    char text[128];
    snprintf(text, sizeof(text), "%.0f fps", G.fps);
    fo_font_draw(&G.pixel6, text, G.width - 3 * u, G.height - 3 * u, u * 0.5f, fo_rgb(0xffb866), FO_ALIGN_RIGHT);
}

void fo_game_frame(double dt) {
    fo_debug_poll(dt);
    G.time += (float)dt;
    G.fpsFrames++;
    G.fpsTime += (float)dt;
    if (G.fpsTime >= 1) {
        G.fps = G.fpsFrames / G.fpsTime;
        fprintf(stderr, "fps %.1f calls %d update %.2f ms draw %.2f ms\n", G.fps, fo_gfx_draw_calls(),
                G.updateTime * 1000 / G.fpsFrames, G.drawTime * 1000 / G.fpsFrames);
        G.updateTime = G.drawTime = 0;
        G.fpsFrames = 0;
        G.fpsTime = 0;
    }
    double t0 = wall_seconds();
    FOPlayer *p = local_player();
    apply_input(p);
    fo_world_update(G.map.world, (float)dt);
    for (int i = 0; i < FO_MAX_PLAYERS; i++) G.looks[i].stateTime += (float)dt;
    fo_hud_events(G.map.world, FO_LOCAL_ID, (float)dt);
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
        fo_camera_update(&G.camera, (float)dt, x, y, world->map.width, world->map.height);
    }
    double t1 = wall_seconds();
    fo_gfx_begin(G.width, G.height, fo_rgb(0x000000));
    draw_world();
    draw_hud();
    if (!G.noVignette) fo_gfx_vignette();
    fo_gfx_flush();
    G.updateTime += t1 - t0;
    G.drawTime += wall_seconds() - t1;
}

void fo_game_touch(int id, int phase, float x, float y) {
    if (!fo_controls_touch(&G.controls, id, phase, x, G.height - y) && phase == FO_TOUCH_BEGAN) {
        tap_world(x, G.height - y);
    }
}

void fo_game_debug(const char *command) {
    int n;
    char name[32];
    if (sscanf(command, "vignette %d", &n) == 1) G.noVignette = !n;
    if (sscanf(command, "map %d", &n) == 1 && n >= 0 && n < FO_MAP_COUNT) start_map((FOMapId)n);
    if (sscanf(command, "class %d", &n) == 1 && n >= 1 && n <= 9) {
        G.localClass = n;
        FOPlayer *p = local_player();
        if (p) fo_world_spawn(G.map.world, p, n);
    }
    if (sscanf(command, "team %31s", name) == 1) {
        G.localTeam = strcmp(name, "red") == 0 ? FO_TEAM_RED : FO_TEAM_BLU;
        start_map(G.map.id);
    }
    FOPlayer *p = local_player();
    if (sscanf(command, "move %d", &n) == 1) G.debugMove = n == FO_MOVE_STOP ? 0 : n;
    if (p && strncmp(command, "jump", 4) == 0) fo_player_jump(G.map.world, p);
    float f;
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
