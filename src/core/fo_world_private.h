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
// Spawns a projectile of the weapon def for the player owner.
FOBullet *fo_combat_spawn(FOWorld *world, FOPlayer *owner, const FOWeaponDef *def, float x, float y, float vx,
                          float vy);
// KillEventData.generateKillEventDataFromDamageHistory and the points:
// the kill of the player victim, or of its sentry when sentry is 1.
void fo_combat_credit(FOWorld *world, const FODamage *history, int victim, int sentry);

// fo_classes.c
void fo_classes_update(FOWorld *world, FOPlayer *player, float dt);
void fo_classes_after(FOWorld *world, FOPlayer *player, float dt, int inFridge);
void fo_classes_sentries(FOWorld *world, float dt);
void fo_classes_reset(FOWorld *world, FOPlayer *player, int keepSentry);
void fo_classes_death(FOWorld *world, FOPlayer *player);
void fo_player_set_cloak(FOWorld *world, FOPlayer *player, int on);
void fo_player_burn(FOWorld *world, FOPlayer *player, int attacker, int item, long long until);
void fo_sentry_damage(FOWorld *world, FOSentry *sentry, int attacker, int item, float damage);
void fo_sentry_remove(FOWorld *world, FOSentry *sentry);
void fo_sentry_land(FOWorld *world, FOSentry *sentry);
int fo_sentry_ready(const FOWorld *world, const FOSentry *sentry);

void fo_combat_weapon_update(FOPlayer *player, float dt);

// fo_modes.c
void fo_modes_contact(FOWorld *world, FOObj *a, FOObj *b, int begin);
void fo_modes_objects(FOWorld *world, float dt);
void fo_modes_update(FOWorld *world, float dt);
void fo_modes_round(FOWorld *world);

// 1 when the player holds the objective of the mode (MapGameMode.b(xn)).
int fo_world_holds_objective(const FOWorld *world, const FOPlayer *player);

// The game mode: 1 when the mode lets the player cloak or disguise
// (MapGameMode.a(xn) and c(xn)). The flag carrier of capture the flag
// cannot.
int fo_world_mode_allows(const FOWorld *world, const FOPlayer *player);

#endif
