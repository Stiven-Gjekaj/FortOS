// fo_hud: the HUD of a match: the bars of WeaponInfoHUD (ajt.java), the
// kill feed of DeathList (aic.java), and DeathRespawnMessage (aie.java).
// The units are pixels of the screen with y up. One UI unit is 1/281 of
// the width of the screen.

#ifndef FO_HUD_H
#define FO_HUD_H

#include "fo_assets.h"
#include "fo_font.h"
#include "fo_text.h"
#include "fo_world.h"

void fo_hud_init(const FOAtlasTextures *sprites, const FOFont *smallest, const FOText *texts, int width, int height);

// Reads the events of the last frame of the world: the kills.
void fo_hud_events(const FOWorld *world, int localId, float dt);

// Draws the HUD of the local player. now is the world clock in seconds.
void fo_hud_draw(const FOWorld *world, const FOPlayer *local, float dt);

// SniperAimingHUD (ajp.java): the power of the shot and the aim line.
// (x, y) is the point 1 m behind the scope, on the screen. angle is in
// radians, and charge goes from 0 to 100.
void fo_hud_sniper(float x, float y, float angle, float charge);

#endif
