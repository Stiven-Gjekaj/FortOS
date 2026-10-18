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
    float scopeX, scopeY;  // SniperRifle.g(): the scope, while the rifle aims.
    float scopeArm;        // The angle of the arm while the rifle aims.
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

// Draws a standing character of the class with its middle at the screen
// point (cx, cy), for a menu. unit is the screen size of one art pixel.
void fo_look_preview(const FOWorld *world, int cls, short team, float cx, float cy, float unit, float time, float width,
                     float height);

// Draws the bomb and its gates on the bomb map.
void fo_look_bomb(const FOWorld *world);

// Draws the sign of the point at (x, y) on the hill map.
void fo_look_koth(const FOWorld *world, float x, float y);

// Draws the flags on the flag map.
void fo_look_flags(const FOWorld *world, float now);

#endif
