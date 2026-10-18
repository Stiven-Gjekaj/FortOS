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
#include "fo_maps.h"
#include "fo_world.h"

// The size of one art pixel in meters, as GANGFORT draws each region.
#define FO_ART 0.1f
// The id of the local player in single player (xa.java:24).
#define FO_LOCAL_ID 69

static const char *kClassNames[10] = {"", "scout", "soldier", "pyro", "demoman", "heavy",
                                      "engineer", "medic", "sniper", "spy"};

static struct {
    char error[512];
    int width, height;
    float time;
    FOAtlasTextures sprites;
    FOFont pixel6, pixel12;
    FOMap map;
    int mapLoaded;
    FOCamera camera;
    FOControls controls;
    int localClass;
    short localTeam;
    int debugMove;
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
    if (!fo_assets_atlas(&G.sprites, "textures/sprites.atlas") || !fo_font_load(&G.pixel6, "fonts/pixel6.fnt") ||
        !fo_font_load(&G.pixel12, "fonts/pixel12.fnt") || !start_map(FO_MAP_GANTOWN)) {
        fail("The APK does not hold the files of GANGFORT 2.1.");
        return 0;
    }
    fo_controls_layout(&G.controls, width, height);
    fprintf(stderr, "start: %.3f s\n", wall_seconds() - start);
    return 1;
}

// GameScreen.render, section 1 (aeb.java:142-235), and the shot of the
// single player controller (xa.java:52-55): the input of the local player.
static void apply_input(FOPlayer *p) {
    if (!p || !p->alive) return;
    FOWorld *world = G.map.world;
    if (fo_controls_jump(&G.controls)) fo_player_jump(world, p);
    p->move = G.debugMove ? G.debugMove : fo_controls_move(&G.controls);
    int touched = fo_controls_shoot_touched(&G.controls) || G.debugShoot;
    int moved = fo_controls_shoot_moved(&G.controls);
    if (moved && p->cls != FO_CLASS_SNIPER) {
        fo_player_set_aim(p, fo_controls_shoot_angle(&G.controls));
    } else if (G.debugShoot) {
        fo_player_set_aim(p, G.debugAim);
    } else {
        fo_player_set_aim(p, 0);
    }
    int shoot = 0;
    if (touched) {
        if (!moved && !G.debugShoot && !p->facingRight) fo_player_set_aim(p, 180);
        // The input shoots only with ammo (aeb.java:183).
        shoot = p->cls != FO_CLASS_SNIPER && p->weapon.ammo > 0;
    }
    if (shoot && fo_player_can_shoot(world, p)) fo_player_shoot(world, p, p->weapon.angle * 0.017453292f);
}

static void draw_player(const FOPlayer *p) {
    if (!p->alive) return;
    char name[64];
    snprintf(name, sizeof(name), "%s_%s_armsoverhead", p->team == FO_TEAM_RED ? "red" : "blu", kClassNames[p->cls]);
    FOSprite s = fo_assets_sprite(&G.sprites, name);
    float x, y;
    fo_player_position(p, &x, &y);
    float w = s.w * FO_ART, h = s.h * FO_ART;
    fo_gfx_draw(&s, x - w / 2, y - 0.95f, w, h, 0, 0, 0, !p->facingRight, FO_WHITE);
}

static void draw_world(void) {
    float hw = G.camera.viewW / 2, hh = G.camera.viewH / 2;
    float left = G.camera.shownX - hw, bottom = G.camera.shownY - hh;
    float right = G.camera.shownX + hw, top = G.camera.shownY + hh;
    fo_gfx_view(left, bottom, right, top);
    fo_map_draw_back(&G.map, &G.sprites, G.camera.shownX, left, bottom, right, top);
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (G.map.world->players[i].used) draw_player(&G.map.world->players[i]);
    }
    fo_map_draw_front(&G.map, &G.sprites, left, bottom, right, top);
}

static void draw_hud(void) {
    fo_gfx_view(0, 0, (float)G.width, (float)G.height);
    float u = G.width / 281.0f;
    FOPlayer *p = local_player();
    fo_controls_draw(&G.controls, &G.sprites, p ? p->team : FO_TEAM_BLU, p && p->cls == FO_CLASS_SNIPER, NULL);
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
    fo_map_update(&G.map, (float)dt);
    if (p && p->alive) {
        float x, y;
        fo_player_position(p, &x, &y);
        fo_camera_update(&G.camera, (float)dt, x, y, G.map.world->map.width, G.map.world->map.height);
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
    fo_controls_touch(&G.controls, id, phase, x, G.height - y);
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
            fo_world_spawn(G.map.world, b, cls);
            float x, y;
            fo_player_position(local_player(), &x, &y);
            fo_phys_body_set_transform(b->obj.body, x + f, y + 1, 0);
        }
    }
}
