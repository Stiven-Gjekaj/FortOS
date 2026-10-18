// fo_world_private: the calls between the files of the world. Only the
// files of the world include it.

#ifndef FO_WORLD_PRIVATE_H
#define FO_WORLD_PRIVATE_H

#include "fo_world.h"

// The object of the body of a fixture.
FOObj *fo_world_fixture_obj(FOPhysFixture *fixture);

// Player.i(float) (xn.java:726-740): sets the vertical speed, as a jump.
void fo_player_launch(FOWorld *world, FOPlayer *player, float speed);

// fo_combat.c
void fo_combat_contact(FOWorld *world, FOObj *a, FOObj *b, float px, float py);
void fo_combat_update(FOWorld *world, float dt);
void fo_combat_kill(FOWorld *world, FOPlayer *victim);
void fo_combat_reset(FOPlayer *player);
void fo_combat_drop(FOWorld *world, FOPlayer *player);

#endif
