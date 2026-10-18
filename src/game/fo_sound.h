// fo_sound: the sounds and the music of a match, as SpatialAudioManager
// (ala.java) and the sound calls of GANGFORT. A sound in the world gets a
// volume and a pan from its place and the listener. The music theme1
// plays in a loop.

#ifndef FO_SOUND_H
#define FO_SOUND_H

#include "fo_mixer.h"
#include "fo_world.h"

// Reads the sounds of the APK into the mixer. The music streams at once.
// The other sounds decode on a second thread, and each one plays when it
// is ready.
void fo_sound_init(FOMixer *mixer);

// Plays a sound with no place, as GameSound.b() (acw.java).
void fo_sound_global(const char *name);

// The volume of the music: 1 in the menus, 0.4 in a match, and 0 in the
// kill cam and at the end of a round (aeb.java, ahh.java).
void fo_sound_music(float volume);

// Plays the sounds of the events of the last frame of the world, and
// keeps the loops of the heal beams and the flamethrowers. listener is
// the place of the ear and range the distance at which a sound is gone
// (wt.java:79-89). now is the clock of the app in seconds.
void fo_sound_update(const FOWorld *world, int localId, float listenerX, float listenerY, float range, float now);

// Stops all the sounds of the world, as at the end of a match.
void fo_sound_stop_all(void);

#endif
