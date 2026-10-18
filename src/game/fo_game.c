#include "fo_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "fo_assets.h"
#include "fo_debug.h"
#include "fo_font.h"
#include "fo_gfx.h"
#include "fo_layer.h"

// The size of one art pixel in meters, as GANGFORT draws each region.
#define FO_ART 0.1f
// The width of the view of the world in meters (GameCamera, wp.java:26).
#define FO_VIEW_W 22.1875f

static const char *kClassNames[9] = {"scout", "soldier", "pyro", "demoman", "heavy",
                                     "engineer", "medic", "sniper", "spy"};

static struct {
    char error[512];
    int width, height;
    float time;
    FOAtlasTextures sprites;
    FOAtlasTextures map;
    FOFont pixel6, pixel12;
    // The camera center in meters, and the size of the map.
    float camX, camY;
    float mapW, mapH;
    float cloudX, cloudSpeed;
    // The drag of the camera.
    int dragging;
    float dragX, dragY;
    // The speed log.
    float fpsTime;
    int fpsFrames;
    float fps;
    // The switches of the debug build, to measure the cost of each part.
    int noVignette;
    int layerMask;
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

int fo_game_start(const char *apk_path, int width, int height) {
    freopen("/tmp/FortOS.log", "w", stderr);
    setvbuf(stderr, NULL, _IOLBF, 0);
    double start = wall_seconds();
    G.width = width;
    G.height = height;
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
    if (!fo_assets_atlas(&G.sprites, "textures/sprites.atlas") ||
        !fo_assets_atlas_cells(&G.map, "maps/gts_gantown/gantown.atlas", 1) || !fo_font_load(&G.pixel6, "fonts/pixel6.fnt") ||
        !fo_font_load(&G.pixel12, "fonts/pixel12.fnt")) {
        fail("The APK does not hold the files of GANGFORT 2.1.");
        return 0;
    }
    FOSprite main = fo_assets_sprite(&G.map, "main");
    G.mapW = main.w * FO_ART;
    G.mapH = main.h * FO_ART;
    G.camX = 20;
    G.camY = 12;
    G.cloudX = G.mapW;
    G.cloudSpeed = 0.5f;
    fprintf(stderr, "start: %.3f s, textures %.1f MB\n", wall_seconds() - start,
            fo_assets_texture_bytes() / 1048576.0);
    return 1;
}

static float view_h(void) {
    return FO_VIEW_W * G.height / G.width;
}

static void clamp_camera(void) {
    float hw = FO_VIEW_W / 2, hh = view_h() / 2;
    if (G.camX < hw) G.camX = hw;
    if (G.camX > G.mapW - hw) G.camX = G.mapW - hw;
    if (G.camY < hh) G.camY = hh;
    if (G.camY > G.mapH - hh) G.camY = G.mapH - hh;
}

// The layers of GTS_Gantown (acp.java) from the back to the front, with the
// parallax of the background and the silos.
enum { LAYER_BG, LAYER_CLOUDS, LAYER_SILOS, LAYER_MAIN, LAYER_PLATFORMS, LAYER_FRONT, LAYER_COUNT };
static const char *kLayers[LAYER_COUNT] = {"bg", "clouds", "silos", "main", "platforms", "spawnbasesfront"};

static void draw_layers(int first, int last, float left, float bottom, float right, float top) {
    float xs[LAYER_COUNT] = {-G.camX * 0.03f, G.cloudX, -G.camX * 0.025f, 0, 0, 0};
    for (int pass = FO_PASS_OPAQUE; pass <= FO_PASS_BLEND; pass++) {
        for (int i = first; i <= last; i++) {
            if (G.layerMask & (1 << i)) continue;
            FOLayer layer = fo_assets_layer(&G.map, kLayers[i]);
            // The nearer layer has the smaller depth.
            fo_layer_draw(&layer, pass, xs[i], 0, FO_ART, 0.9f - i * 0.1f, left, bottom, right, top);
        }
    }
}

static void draw_world(void) {
    float hw = FO_VIEW_W / 2, hh = view_h() / 2;
    float left = G.camX - hw, bottom = G.camY - hh, right = G.camX + hw, top = G.camY + hh;
    fo_gfx_view(left, bottom, right, top);
    draw_layers(LAYER_BG, LAYER_PLATFORMS, left, bottom, right, top);
    // A row of the nine classes of each team, in front of the platforms.
    fo_gfx_depth(FO_DEPTH_OFF, 0);
    for (int team = 0; team < 2; team++) {
        for (int i = 0; i < 9; i++) {
            char name[64];
            snprintf(name, sizeof(name), "%s_%s_armsoverhead", team ? "red" : "blu", kClassNames[i]);
            FOSprite s = fo_assets_sprite(&G.sprites, name);
            float x = 14 + i * 2.2f + team * 22;
            fo_gfx_draw(&s, x, 9.6f, s.w * FO_ART, s.h * FO_ART, 0, 0, 0, team, FO_WHITE);
        }
    }
    draw_layers(LAYER_FRONT, LAYER_FRONT, left, bottom, right, top);
    fo_gfx_depth(FO_DEPTH_OFF, 0);
}

static void draw_hud(void) {
    fo_gfx_view(0, 0, (float)G.width, (float)G.height);
    // One UI pixel is 1/281 of the width of the screen, as in GANGFORT.
    float u = G.width / 281.0f;
    char text[128];
    snprintf(text, sizeof(text), "fortos 0.1\n%.0f fps  %.1f mb  %d calls", G.fps,
             fo_assets_texture_bytes() / 1048576.0, fo_gfx_draw_calls());
    fo_font_draw(&G.pixel6, text, 4 * u, G.height - 3 * u, u * 0.5f, fo_rgb(0xffb866), FO_ALIGN_LEFT);
    fo_font_draw(&G.pixel12, "drag to move the camera", G.width / 2.0f, 12 * u, u * 0.5f, FO_WHITE,
                 FO_ALIGN_CENTER);
}

void fo_game_frame(double dt) {
    fo_debug_poll(dt);
    G.time += (float)dt;
    G.fpsFrames++;
    G.fpsTime += (float)dt;
    if (G.fpsTime >= 1) {
        G.fps = G.fpsFrames / G.fpsTime;
        fprintf(stderr, "fps %.1f calls %d\n", G.fps, fo_gfx_draw_calls());
        G.fpsFrames = 0;
        G.fpsTime = 0;
    }
    // The clouds go to the left and start again at the right edge.
    G.cloudX -= G.cloudSpeed * (float)dt;
    FOSprite clouds = fo_assets_sprite(&G.map, "clouds");
    if (G.cloudX < -clouds.w * FO_ART) {
        G.cloudX = G.mapW;
        G.cloudSpeed = (1 + rand() % 10) * 0.1f;
    }
    clamp_camera();
    fo_gfx_begin(G.width, G.height, fo_rgb(0x26231f));
    fo_gfx_vignette(!G.noVignette);
    draw_world();
    draw_hud();
    fo_gfx_flush();
}

void fo_game_touch(int id, int phase, float x, float y) {
    (void)id;
    y = G.height - y;
    float metersPerPixel = FO_VIEW_W / G.width;
    if (phase == FO_TOUCH_BEGAN) {
        G.dragging = 1;
    } else if (phase == FO_TOUCH_MOVED && G.dragging) {
        G.camX -= (x - G.dragX) * metersPerPixel;
        G.camY -= (y - G.dragY) * metersPerPixel;
    } else if (phase == FO_TOUCH_ENDED) {
        G.dragging = 0;
    }
    G.dragX = x;
    G.dragY = y;
}

void fo_game_debug(const char *command) {
    float x, y;
    int n;
    if (sscanf(command, "vignette %d", &n) == 1) G.noVignette = !n;
    if (sscanf(command, "hide %d", &n) == 1) G.layerMask = n;
    if (sscanf(command, "camera %f %f", &x, &y) == 2) {
        G.camX = x;
        G.camY = y;
    }
}
