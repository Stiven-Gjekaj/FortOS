// fo_hud: the HUD of a match: the bars of WeaponInfoHUD (ajt.java), the
// kill feed of DeathList (aic.java), DeathRespawnMessage (aie.java), the
// objective HUDs (ahk.java, aiy.java, ahy.java), and RoundOverDialog
// (ajl.java).
// The units are pixels of the screen with y up. One UI unit is 1/281 of
// the width of the screen.

#ifndef FO_HUD_H
#define FO_HUD_H

#include "fo_assets.h"
#include "fo_font.h"
#include "fo_text.h"
#include "fo_world.h"

// The fonts of the HUD: pixel4 is the smallest, pixel21 the biggest.
typedef struct {
    const FOFont *pixel4, *pixel6, *pixel12, *pixel21;
} FOHudFonts;

void fo_hud_init(const FOAtlasTextures *sprites, const FOHudFonts *fonts, const FOText *texts, int width, int height);

// The name of the map, for the round-over dialog.
void fo_hud_map(const char *name);

// The kill cam: the name of the killer, or NULL when it ends.
void fo_hud_killcam(const char *killer);

// The part of the world on the screen, in meters.
void fo_hud_view(float left, float bottom, float width, float height);

// Reads the events of the last frame of the world: the kills.
void fo_hud_events(const FOWorld *world, int localId, float dt);

// Draws the HUD of the local player. now is the world clock in seconds.
void fo_hud_draw(const FOWorld *world, const FOPlayer *local, float dt);

// SniperAimingHUD (ajp.java): the power of the shot and the aim line.
// (x, y) is the point 1 m behind the scope, on the screen. angle is in
// radians, and charge goes from 0 to 100.
void fo_hud_sniper(float x, float y, float angle, float charge);

// The hint of the bomb under it. Call it with the view of the world.
void fo_hud_bomb_hint(const FOWorld *world, float dt);

// DeathCamHUD (aib.java): the bars of the kill cam and the name of the
// killer.
void fo_hud_death_cam(const char *killer);

// RoundOverDialog (ajl.java): draws the dialog when the round is over, and
// returns 1 when it shows.
int fo_hud_round_over(const FOWorld *world, const FOPlayer *local, float dt);

#endif
