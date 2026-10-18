// fo_effects: the sprite effects of a match, as SpriteEffectsHandler
// (alb.java): the explosions, the hits on walls, the blood, the poofs of a
// landing, the smoke of the rockets, the sentries that break, the big
// blast of the bomb, and the effects of the maps (PeriodicalMapSpriteEffect,
// acz.java).

#ifndef FO_EFFECTS_H
#define FO_EFFECTS_H

#include "fo_assets.h"
#include "fo_maps.h"
#include "fo_world.h"

// Finds the frames of each effect. Call it after the sprite atlas loads.
void fo_effects_init(const FOAtlasTextures *sprites);

// Clears the effects for a new map, and starts the effects of that map.
// The big blast of the bomb map loads textures/explosion.atlas.
void fo_effects_reset(FOMapId map, float now);

// Starts the effects of the events of the last frame of the world, the
// smoke of each rocket, and the effects of the map. now is the clock of
// the app in seconds.
void fo_effects_update(const FOWorld *world, float now);

// The layers of wz.java: particlesBehindPlatforms, sentry, and
// particlesTopMost. Call each with the view of the world.
void fo_effects_draw_behind(const FOWorld *world, float now);
void fo_effects_draw_sentry(const FOWorld *world, float now);
void fo_effects_draw_top(const FOWorld *world, float now);

#endif
