// fo_mixer: mixes the sounds and the music into one stereo stream, as the
// Sound and Music objects of libGDX do for GANGFORT (GameSound, acw.java).
//
// A sound is a set of samples in memory, or a stream that the mixer
// decodes from Ogg Vorbis while it plays. Each play makes a voice with a
// volume and a pan. A pan of -1 is the left side and 1 is the right side,
// as Sound.play(volume, pitch, pan) of libGDX. One track of music plays
// in a loop.
//
// The game thread calls the play functions. The audio thread calls
// fo_mixer_render. A lock in the mixer keeps the two apart.

#ifndef FO_MIXER_H
#define FO_MIXER_H

#include <stddef.h>
#include <stdint.h>

typedef struct FOMixer FOMixer;

// A new mixer that renders stereo samples at rate samples per second.
FOMixer *fo_mixer_new(int rate);
void fo_mixer_free(FOMixer *mixer);

// Adds a sound with the name, from samples in memory. samples holds
// frames * channels values, with 1 or 2 channels. volume is the factor of
// the sound (GameSound.c, acw.java). The mixer keeps the buffer and frees
// it. Returns 0 when the mixer is full or has the name.
int fo_mixer_add_samples(FOMixer *mixer, const char *name, int16_t *samples, int frames, int channels, int rate,
                         float volume);

// Adds a sound from the bytes of an Ogg Vorbis file. The mixer keeps the
// bytes and frees them. With stream at 1, the mixer decodes the sound
// while it plays, and the sound has one voice at a time. With stream at 0,
// the mixer decodes all of the sound now. Returns 0 when the bytes are not
// Ogg Vorbis or the mixer is full; then the caller keeps the bytes.
int fo_mixer_add_vorbis(FOMixer *mixer, const char *name, unsigned char *ogg, size_t size, int stream, float volume);

// 1 when the mixer has the sound.
int fo_mixer_has(FOMixer *mixer, const char *name);

// The length of the sound in seconds, or 0.
float fo_mixer_length(FOMixer *mixer, const char *name);

// The volume factor of the sound, or 1.
float fo_mixer_volume(FOMixer *mixer, const char *name);

// Plays the sound with the volume and the pan, once or in a loop. Returns
// the id of the voice, or 0 when the mixer does not have the sound.
int fo_mixer_play(FOMixer *mixer, const char *name, float volume, float pan, int loop);

// Sets the volume and the pan of a voice (Sound.setPan).
void fo_mixer_set(FOMixer *mixer, int id, float volume, float pan);

// Stops a voice. 1 while a voice plays.
void fo_mixer_stop(FOMixer *mixer, int id);
int fo_mixer_playing(FOMixer *mixer, int id);

// Stops all the voices, not the music.
void fo_mixer_stop_all(FOMixer *mixer);

// The voices that play now.
int fo_mixer_voices(FOMixer *mixer);

// Plays the sound as the music, in a loop, with the volume. A new name
// starts the track from its start. A volume of 0 keeps the place of the
// track and makes no sound.
void fo_mixer_music(FOMixer *mixer, const char *name, float volume);

// Writes frames stereo frames, left and right, into out.
void fo_mixer_render(FOMixer *mixer, int16_t *out, int frames);

#endif
