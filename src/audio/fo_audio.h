// fo_audio: the audio output of the phone. A RemoteIO unit asks the mixer
// for stereo frames of 16 bits.

#ifndef FO_AUDIO_H
#define FO_AUDIO_H

#include "fo_mixer.h"

#define FO_AUDIO_RATE 44100

// Starts the output with the mixer. Returns 0 when it does not start.
int fo_audio_start(FOMixer *mixer);

// Stops the output while the app is in the background, and starts it again.
void fo_audio_set_active(int active);

#endif
