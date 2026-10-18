// Tests of fo_mixer: the voices, the volume, the pan, the loop, and the
// music. Each test builds its sounds from samples in memory.

#import "FOTest.h"
#include <stdlib.h>
#include "fo_mixer.h"

// A mono sound of frames samples, each of the value.
static int16_t *constant(int frames, int16_t value) {
    int16_t *s = malloc(sizeof(int16_t) * (size_t)frames);
    for (int i = 0; i < frames; i++) s[i] = value;
    return s;
}

static FOMixer *mixerWith(const char *name, int frames, int16_t value, float volume) {
    FOMixer *m = fo_mixer_new(44100);
    FO_EXPECT(fo_mixer_add_samples(m, name, constant(frames, value), frames, 1, 44100, volume));
    return m;
}

FO_TEST(testMixerPlaysASoundToItsEnd) {
    FOMixer *m = mixerWith("beep", 100, 16384, 1.0f);
    int id = fo_mixer_play(m, "beep", 1.0f, 0.0f, 0);
    FO_EXPECT(id > 0);
    FO_EXPECT(fo_mixer_playing(m, id));
    int16_t out[2 * 150];
    fo_mixer_render(m, out, 150);
    FO_EXPECT(abs(out[0] - 16383) <= 1 && abs(out[1] - 16383) <= 1);
    FO_EXPECT(abs(out[2 * 99] - 16383) <= 1);
    FO_EQUAL_INT(out[2 * 100], 0);
    FO_EXPECT(!fo_mixer_playing(m, id));
    FO_EQUAL_INT(fo_mixer_voices(m), 0);
    FO_EXPECT(fabsf(fo_mixer_length(m, "beep") - 100 / 44100.0f) < 1e-6f);
    fo_mixer_free(m);
}

FO_TEST(testMixerPanLowersTheOtherSide) {
    FOMixer *m = mixerWith("beep", 100, 16384, 1.0f);
    int16_t out[2 * 10];
    int id = fo_mixer_play(m, "beep", 1.0f, -1.0f, 0);
    fo_mixer_render(m, out, 10);
    FO_EXPECT(abs(out[0] - 16383) <= 1);
    FO_EQUAL_INT(out[1], 0);
    // A pan of 0.5 to the right halves the left side.
    fo_mixer_set(m, id, 1.0f, 0.5f);
    fo_mixer_render(m, out, 10);
    FO_EXPECT(abs(out[0] - 8191) <= 1);
    FO_EXPECT(abs(out[1] - 16383) <= 1);
    fo_mixer_free(m);
}

FO_TEST(testMixerVolumeOfTheSoundAndOfTheVoice) {
    FOMixer *m = mixerWith("heal", 100, 16384, 0.5f);
    FO_EXPECT(fo_mixer_volume(m, "heal") == 0.5f);
    int16_t out[2 * 4];
    fo_mixer_play(m, "heal", 0.5f, 0.0f, 0);
    fo_mixer_render(m, out, 4);
    FO_EXPECT(abs(out[0] - 4095) <= 1);
    fo_mixer_free(m);
}

FO_TEST(testMixerSumsVoicesAndClamps) {
    FOMixer *m = mixerWith("beep", 100, 20000, 1.0f);
    int16_t out[2 * 4];
    fo_mixer_play(m, "beep", 1.0f, 0.0f, 0);
    fo_mixer_play(m, "beep", 1.0f, 0.0f, 0);
    FO_EQUAL_INT(fo_mixer_voices(m), 2);
    fo_mixer_render(m, out, 4);
    FO_EQUAL_INT(out[0], 32767);
    fo_mixer_free(m);
}

FO_TEST(testMixerLoopAndStop) {
    FOMixer *m = mixerWith("flame", 10, 8000, 1.0f);
    int id = fo_mixer_play(m, "flame", 1.0f, 0.0f, 1);
    int16_t out[2 * 35];
    fo_mixer_render(m, out, 35);
    FO_EXPECT(out[2 * 34] > 7000);
    FO_EXPECT(fo_mixer_playing(m, id));
    fo_mixer_stop(m, id);
    FO_EXPECT(!fo_mixer_playing(m, id));
    fo_mixer_render(m, out, 4);
    FO_EQUAL_INT(out[0], 0);
    fo_mixer_free(m);
}

FO_TEST(testMixerUnknownSoundHasNoVoice) {
    FOMixer *m = mixerWith("beep", 10, 100, 1.0f);
    FO_EQUAL_INT(fo_mixer_play(m, "nothing", 1.0f, 0.0f, 0), 0);
    FO_EXPECT(!fo_mixer_has(m, "nothing"));
    FO_EXPECT(fo_mixer_has(m, "beep"));
    // The same name twice is refused.
    int16_t *again = constant(10, 1);
    FO_EXPECT(!fo_mixer_add_samples(m, "beep", again, 10, 1, 44100, 1.0f));
    free(again);
    fo_mixer_free(m);
}

FO_TEST(testMixerMusicLoopsWithItsVolume) {
    FOMixer *m = mixerWith("theme1", 20, 10000, 1.0f);
    fo_mixer_music(m, "theme1", 0.4f);
    int16_t out[2 * 50];
    fo_mixer_render(m, out, 50);
    FO_EXPECT(abs(out[0] - 4000) <= 2);
    FO_EXPECT(abs(out[2 * 45] - 4000) <= 2);
    // A volume of 0 makes no sound.
    fo_mixer_music(m, "theme1", 0.0f);
    fo_mixer_render(m, out, 4);
    FO_EQUAL_INT(out[0], 0);
    fo_mixer_free(m);
}

FO_TEST(testMixerRefusesBytesThatAreNotVorbis) {
    FOMixer *m = fo_mixer_new(44100);
    unsigned char junk[64] = {1, 2, 3};
    FO_EXPECT(!fo_mixer_add_vorbis(m, "junk", junk, sizeof(junk), 0, 1.0f));
    FO_EXPECT(!fo_mixer_add_vorbis(m, "junk", junk, sizeof(junk), 1, 1.0f));
    fo_mixer_free(m);
}
