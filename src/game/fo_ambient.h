// fo_ambient: the flies and the birds of the maps, as Flie (xk.java) and
// Bird (xc.java). They only change the look of a match.

#ifndef FO_AMBIENT_H
#define FO_AMBIENT_H

#include "fo_assets.h"
#include "fo_world.h"

// Finds the frames of the flies and the birds.
void fo_ambient_init(const FOAtlasTextures *sprites);

// Puts the flies in the fly zones and the birds in the bird zones of the
// map of the world.
void fo_ambient_reset(const FOWorld *world);

// Moves the flies and the birds. (viewX, viewY, viewW, viewH) is the part
// of the world on the screen. now is the clock of the app in seconds.
void fo_ambient_update(const FOWorld *world, float viewX, float viewY, float viewW, float viewH, float dt, float now);

// Draws them behind the platforms, with the view of the world.
void fo_ambient_draw(float now);

#endif
