// fo_look: draws the players, their weapons, and the projectiles, as
// PlayerRenderData (ada.java), Weapon.a(...) (alz.java), and Bullet.a(alh)
// (xg.java). The rules are in reference/notes/player-render.md.

#ifndef FO_LOOK_H
#define FO_LOOK_H

#include "fo_assets.h"
#include "fo_world.h"

// The animation state of one player on the screen (fields of ada.java).
typedef struct {
    float stateTime;   // st: grows by dt each frame (wr.java:68-70).
    int S;             // The last run frame, for the bob.
    float D;           // The time of the last idle sample, for idl().
    float smokeTime;   // Lsmoke: the last cigarette smoke of the spy.
} FOLook;

// Finds the regions of each class and each team. Call it after the
// sprite atlas is loaded.
void fo_look_init(const FOAtlasTextures *sprites);

// Draws a living player. now is the world clock in seconds. t is the part
// of a step that the physics did not do yet (wt.a), for a smooth place.
// viewer is the local player, for the cloak and the disguise.
void fo_look_player(const FOWorld *world, const FOPlayer *p, FOLook *look, float now, float t,
                    const FOPlayer *viewer);

// Draws the heal beam of each medic.
void fo_look_beams(const FOWorld *world);

// Draws the health bars of the teammates when the viewer is a medic.
void fo_look_health_bars(const FOWorld *world, const FOPlayer *viewer);

// Draws the sentries.
void fo_look_sentries(const FOWorld *world, float now);

// Draws the death animation of a dead player at the place of its death.
void fo_look_dead(const FOWorld *world, const FOPlayer *p, float now);

// Draws the projectiles in flight.
void fo_look_bullets(const FOWorld *world, float now);

// Draws the weapons that the dead players dropped.
void fo_look_pickups(const FOWorld *world);

#endif
