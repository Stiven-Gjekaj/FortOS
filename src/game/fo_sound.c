#include "fo_sound.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "fo_assets.h"

static FOMixer *gMixer;

// The sounds of res/sounds_ogg, but the music theme1 and the sounds of
// the menus that come later.
static const char *const kSounds[] = {
    "applelauncher_shoot", "backstab",          "cartpush",          "cartpush_explosion",
    "colorfulflamethrower_shoot", "cp_owner_changed", "death1",       "doublebarrelpistol_shoot",
    "explosion",           "flag_secured",      "flag_stolen",       "flamethrower_shoot",
    "game_over_lost",      "game_over_victory", "grenade_launcher_shoot", "health_fridge",
    "item_gained",         "jump",              "justicerevolver_shoot", "killzoom",
    "lethalsniper_shoot",  "medic_healing",     "medic_uber_begin",  "medic_uber_end",
    "menu_click1",         "minigun_shoot",     "needle_squirter_shoot", "paperairplanelauncher_shoot",
    "pumpactionshotgun_shoot", "respawn",       "revolver_shoot",    "rocket_launcher_shoot",
    "sentry_alerted",      "sentry_built",      "shotgun_shoot",     "sniper_rifle_shoot",
    "spawndoors_close",    "spawndoors_open",   "splash_hit",        "uddergun_shoot"};

#define FO_SOUND_COUNT ((int)(sizeof(kSounds) / sizeof(kSounds[0])))

typedef struct {
    unsigned char *bytes;
    size_t size;
} FOSoundFile;

static FOSoundFile gFiles[FO_SOUND_COUNT];

// GameSound volume factors (akz.java:128-129).
static float sound_volume(const char *name) {
    return strcmp(name, "medic_healing") == 0 || strcmp(name, "uddergun_shoot") == 0 ? 0.5f : 1.0f;
}

// The second thread: decodes each sound into the mixer.
static double seconds(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec / 1e6;
}

static void *decode_all(void *unused) {
    (void)unused;
    double start = seconds();
    for (int i = 0; i < FO_SOUND_COUNT; i++) {
        if (!gFiles[i].bytes) continue;
        if (!fo_mixer_add_vorbis(gMixer, kSounds[i], gFiles[i].bytes, gFiles[i].size, 0, sound_volume(kSounds[i]))) {
            fprintf(stderr, "sound: %s does not decode\n", kSounds[i]);
            free(gFiles[i].bytes);
        }
        gFiles[i].bytes = NULL;
    }
    fprintf(stderr, "sound: all sounds are ready in %.2f s\n", seconds() - start);
    return NULL;
}

void fo_sound_init(FOMixer *mixer) {
    gMixer = mixer;
    size_t size = 0;
    unsigned char *theme = fo_assets_read("sounds_ogg/theme1.ogg", &size);
    if (theme && !fo_mixer_add_vorbis(mixer, "theme1", theme, size, 1, 1.0f)) free(theme);
    // The reads of the APK stay on this thread. Only the decode moves.
    for (int i = 0; i < FO_SOUND_COUNT; i++) {
        char path[96];
        snprintf(path, sizeof(path), "sounds_ogg/%s.ogg", kSounds[i]);
        gFiles[i].bytes = fo_assets_read(path, &gFiles[i].size);
    }
    pthread_t thread;
    if (pthread_create(&thread, NULL, decode_all, NULL) == 0) pthread_detach(thread);
}

// The sounds of GameSound.b() with no place. The core sends them as
// sounds at (0, 0).
static int is_global(const char *name) {
    return strcmp(name, "cp_owner_changed") == 0 || strcmp(name, "flag_secured") == 0 ||
           strcmp(name, "flag_stolen") == 0 || strcmp(name, "cartpush_explosion") == 0 ||
           strcmp(name, "game_over_victory") == 0 || strcmp(name, "game_over_lost") == 0 ||
           strcmp(name, "killzoom") == 0 || strcmp(name, "menu_click1") == 0 || strcmp(name, "item_gained") == 0;
}

void fo_sound_global(const char *name) {
    if (gMixer) fo_mixer_play(gMixer, name, 1.0f, 0.0f, 0);
}

void fo_sound_music(float volume) {
    if (gMixer) fo_mixer_music(gMixer, "theme1", volume);
}

// --- SpatialAudioManager (ala.java) ---------------------------------------

#define FO_SLOTS 50

typedef struct {
    int active;              // a
    int voice;               // b: the voice of the mixer.
    char name[40];
    float x, y;              // d
    int attach;              // f: the id of the player, or 0.
    int loop;                // g
    float start;             // i
    float length;
} FOSlot;

static FOSlot gSlots[FO_SLOTS];
static float gListenerX, gListenerY, gRange = 1;
// ala.a: the volume of all the sounds, and its fade (ala.java:162-169).
static float gMaster = 1, gFadeFrom = 1, gFadeTo = 1, gFadeStart, gFadeTime, gNow;

// The volume and the pan of a slot (ala.a.a(Vector2, float)). The pan
// compares the angle of the sound with the angle of a zero vector, and the
// cross product with it is always 0. So the pan goes to the left for a
// sound above or under the ear, and to the middle for a sound at its side.
static void slot_mix(const FOSlot *s, float *volume, float *pan) {
    float dx = s->x - gListenerX, dy = s->y - gListenerY;
    float len = sqrtf(dx * dx + dy * dy);
    float angle = len > 0 ? atan2f(dy / len, dx / len) : 0;
    float a = fabsf(0.0f - angle);
    if (a > 1.5707964f) {
        *pan = -1.0f + (0.0f - -1.0f) * ((a - 1.5707964f) / 1.5707964f);
    } else {
        *pan = 0.0f + (-1.0f - 0.0f) * (a / 1.5707964f);
    }
    float v = 1.0f - len / gRange;
    *volume = (v < 0 ? 0 : v > 1 ? 1 : v) * gMaster;
}

static void slot_stop(FOSlot *s) {
    if (s->voice) fo_mixer_stop(gMixer, s->voice);
    memset(s, 0, sizeof(*s));
}

// ala.a(String, wr, x, y, loop, delay): a sound at a place, or on a player.
static FOSlot *play_at(const char *name, float x, float y, int attach, int loop, float now) {
    if (!gMixer || !fo_mixer_has(gMixer, name)) return NULL;
    FOSlot *s = NULL;
    for (int i = 0; i < FO_SLOTS; i++) {
        if (!gSlots[i].active) s = &gSlots[i];
    }
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    s->active = 1;
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->x = x;
    s->y = y;
    s->attach = attach;
    s->loop = loop;
    s->start = now;
    s->length = fo_mixer_length(gMixer, name);
    float volume, pan;
    slot_mix(s, &volume, &pan);
    s->voice = fo_mixer_play(gMixer, name, volume, pan, loop);
    return s;
}

// --- The loops of the heal beam and the flamethrower ------------------------

static FOSlot *gHeal[FO_MAX_PLAYERS];
static FOSlot *gFlame[FO_MAX_PLAYERS];
static int gHealTarget[FO_MAX_PLAYERS];

static int is_flame_sound(const char *name) {
    return strcmp(name, "flamethrower_shoot") == 0 || strcmp(name, "colorfulflamethrower_shoot") == 0;
}

static void loops(const FOWorld *world, float now) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *p = &world->players[i];
        int heal = p->used && p->alive && p->cls == FO_CLASS_MEDIC ? p->healTarget : 0;
        // Player.c(int) (xn.java:1165-1178): the loop starts with a new
        // target and stops with none.
        if (heal != gHealTarget[i]) {
            if (heal == 0 && gHeal[i]) {
                slot_stop(gHeal[i]);
                gHeal[i] = NULL;
            } else if (heal != 0 && !gHeal[i]) {
                float x, y;
                fo_player_position(p, &x, &y);
                gHeal[i] = play_at("medic_healing", x, y, p->id, 1, now);
            }
            gHealTarget[i] = heal;
        }
        // Flamethrower.a(float) (alk.java:52-61): the loop stops 4 intervals
        // after the last shot.
        const FOWeaponDef *def = p->weapon.def;
        int flaming = p->used && p->alive && def && def->kind == FO_SHOT_FLAME && p->weapon.lastShot != 0 &&
                      (float)(world->now - p->weapon.lastShot) <= def->interval * 4.0f;
        if (flaming && !gFlame[i]) {
            float x, y;
            fo_player_position(p, &x, &y);
            gFlame[i] = play_at(def->sound, x, y, p->id, 1, now);
        } else if (!flaming && gFlame[i]) {
            slot_stop(gFlame[i]);
            gFlame[i] = NULL;
        }
    }
}

void fo_sound_update(const FOWorld *world, int localId, float listenerX, float listenerY, float range, float now) {
    (void)localId;
    if (!gMixer) return;
    gListenerX = listenerX;
    gListenerY = listenerY;
    gNow = now;
    if (gFadeTime <= 0) {
        gMaster = gFadeTo;
    } else {
        float t = (now - gFadeStart) / gFadeTime;
        gMaster = (t <= 1.0f ? t : 1.0f) * (gFadeTo - gFadeFrom) + gFadeFrom;
    }
    gRange = range > 0 ? range : 1;
    for (int i = 0; i < world->event_count; i++) {
        const FOEvent *e = &world->events[i];
        // SpriteEffectsHandler.a (alb.java:307-309): each explosion effect
        // plays the explosion sound.
        if (e->kind == FO_EVENT_EFFECT && strcmp(e->name, "explosion") == 0) {
            play_at("explosion", e->x, e->y, 0, 0, now);
            continue;
        }
        if (e->kind != FO_EVENT_SOUND) continue;
        if (is_flame_sound(e->name)) continue;
        if (is_global(e->name)) fo_sound_global(e->name);
        else play_at(e->name, e->x, e->y, e->a, 0, now);
    }
    loops(world, now);
    // ala.a(float, float): each slot follows its player, and stops at its end.
    for (int i = 0; i < FO_SLOTS; i++) {
        FOSlot *s = &gSlots[i];
        if (!s->active) continue;
        const FOPlayer *p = s->attach ? fo_world_player((FOWorld *)world, s->attach) : NULL;
        if (s->attach && !p) {
            for (int k = 0; k < FO_MAX_PLAYERS; k++) {
                if (gHeal[k] == s) gHeal[k] = NULL;
                if (gFlame[k] == s) gFlame[k] = NULL;
            }
            slot_stop(s);
            continue;
        }
        if (!s->loop && (now - s->start >= s->length || !fo_mixer_playing(gMixer, s->voice))) {
            slot_stop(s);
            continue;
        }
        if (p) fo_player_position(p, &s->x, &s->y);
        float volume, pan;
        slot_mix(s, &volume, &pan);
        fo_mixer_set(gMixer, s->voice, volume, pan);
    }
}

void fo_sound_stop_all(void) {
    for (int i = 0; i < FO_SLOTS; i++) {
        if (gSlots[i].active) slot_stop(&gSlots[i]);
    }
    memset(gHeal, 0, sizeof(gHeal));
    memset(gFlame, 0, sizeof(gFlame));
    memset(gHealTarget, 0, sizeof(gHealTarget));
}

void fo_sound_fade(float volume, float seconds) {
    gFadeFrom = gMaster;
    gFadeTo = volume;
    gFadeStart = gNow;
    gFadeTime = seconds;
}
