#include "fo_mixer.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// The decoder is third party code. Its warnings are not ours to fix.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#include "../third_party/stb_vorbis.c"
#pragma clang diagnostic pop

#define FO_MIXER_SOUNDS 64
#define FO_MIXER_VOICES 64
#define FO_STREAM_FRAMES 4096

// A sound that the mixer decodes while it plays. buffer holds the frames
// from first to first + count, and the decoder is at first + count.
typedef struct {
    stb_vorbis *vorbis;
    unsigned char *ogg;
    int first, count;
    int16_t buffer[FO_STREAM_FRAMES * 2];
} FOStream;

typedef struct {
    char name[40];
    int16_t *samples;
    int channels;
    int frames;
    int rate;
    float volume;
    FOStream *stream;
} FOSound;

typedef struct {
    int id;          // 0 when the voice is free.
    int sound;
    double pos;
    float volume, pan;
    int loop;
} FOVoice;

struct FOMixer {
    pthread_mutex_t lock;
    int rate;
    FOSound sounds[FO_MIXER_SOUNDS];
    int soundCount;
    FOVoice voices[FO_MIXER_VOICES];
    int nextId;
    int music;       // The sound of the music, or -1.
    double musicPos;
    float musicVolume;
    float *mix;
    int mixFrames;
};

FOMixer *fo_mixer_new(int rate) {
    FOMixer *m = calloc(1, sizeof(FOMixer));
    if (!m) return NULL;
    pthread_mutex_init(&m->lock, NULL);
    m->rate = rate;
    m->nextId = 1;
    m->music = -1;
    return m;
}

void fo_mixer_free(FOMixer *m) {
    if (!m) return;
    for (int i = 0; i < m->soundCount; i++) {
        FOSound *s = &m->sounds[i];
        free(s->samples);
        if (s->stream) {
            stb_vorbis_close(s->stream->vorbis);
            free(s->stream->ogg);
            free(s->stream);
        }
    }
    free(m->mix);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

static int find_sound(const FOMixer *m, const char *name) {
    for (int i = 0; i < m->soundCount; i++) {
        if (strcmp(m->sounds[i].name, name) == 0) return i;
    }
    return -1;
}

static int add_sound(FOMixer *m, const FOSound *sound) {
    pthread_mutex_lock(&m->lock);
    int ok = m->soundCount < FO_MIXER_SOUNDS && find_sound(m, sound->name) < 0;
    if (ok) m->sounds[m->soundCount++] = *sound;
    pthread_mutex_unlock(&m->lock);
    return ok;
}

int fo_mixer_add_samples(FOMixer *m, const char *name, int16_t *samples, int frames, int channels, int rate,
                         float volume) {
    FOSound s;
    memset(&s, 0, sizeof(s));
    strncpy(s.name, name, sizeof(s.name) - 1);
    s.samples = samples;
    s.frames = frames;
    s.channels = channels == 1 ? 1 : 2;
    s.rate = rate;
    s.volume = volume;
    return add_sound(m, &s);
}

int fo_mixer_add_vorbis(FOMixer *m, const char *name, unsigned char *ogg, size_t size, int stream, float volume) {
    if (!stream) {
        int channels = 0, rate = 0;
        short *out = NULL;
        int frames = stb_vorbis_decode_memory(ogg, (int)size, &channels, &rate, &out);
        if (frames <= 0 || !out) return 0;
        if (!fo_mixer_add_samples(m, name, out, frames, channels, rate, volume)) {
            free(out);
            return 0;
        }
        free(ogg);
        return 1;
    }
    int error = 0;
    stb_vorbis *v = stb_vorbis_open_memory(ogg, (int)size, &error, NULL);
    if (!v) return 0;
    stb_vorbis_info info = stb_vorbis_get_info(v);
    FOStream *st = calloc(1, sizeof(FOStream));
    if (!st) {
        stb_vorbis_close(v);
        return 0;
    }
    st->vorbis = v;
    st->ogg = ogg;
    FOSound s;
    memset(&s, 0, sizeof(s));
    strncpy(s.name, name, sizeof(s.name) - 1);
    s.channels = info.channels == 1 ? 1 : 2;
    s.rate = (int)info.sample_rate;
    s.frames = (int)stb_vorbis_stream_length_in_samples(v);
    s.volume = volume;
    s.stream = st;
    if (!add_sound(m, &s)) {
        stb_vorbis_close(v);
        free(st);
        return 0;
    }
    return 1;
}

int fo_mixer_has(FOMixer *m, const char *name) {
    pthread_mutex_lock(&m->lock);
    int has = find_sound(m, name) >= 0;
    pthread_mutex_unlock(&m->lock);
    return has;
}

float fo_mixer_length(FOMixer *m, const char *name) {
    pthread_mutex_lock(&m->lock);
    int i = find_sound(m, name);
    float len = i >= 0 && m->sounds[i].rate > 0 ? (float)m->sounds[i].frames / (float)m->sounds[i].rate : 0;
    pthread_mutex_unlock(&m->lock);
    return len;
}

float fo_mixer_volume(FOMixer *m, const char *name) {
    pthread_mutex_lock(&m->lock);
    int i = find_sound(m, name);
    float v = i >= 0 ? m->sounds[i].volume : 1.0f;
    pthread_mutex_unlock(&m->lock);
    return v;
}

int fo_mixer_play(FOMixer *m, const char *name, float volume, float pan, int loop) {
    pthread_mutex_lock(&m->lock);
    int s = find_sound(m, name);
    int id = 0;
    if (s >= 0) {
        // A stream has one voice at a time.
        if (m->sounds[s].stream) {
            for (int i = 0; i < FO_MIXER_VOICES; i++) {
                if (m->voices[i].id && m->voices[i].sound == s) m->voices[i].id = 0;
            }
        }
        for (int i = 0; i < FO_MIXER_VOICES; i++) {
            if (m->voices[i].id) continue;
            FOVoice *v = &m->voices[i];
            v->id = id = m->nextId++;
            if (m->nextId <= 0) m->nextId = 1;
            v->sound = s;
            v->pos = 0;
            v->volume = volume;
            v->pan = pan;
            v->loop = loop;
            break;
        }
    }
    pthread_mutex_unlock(&m->lock);
    return id;
}

static FOVoice *find_voice(FOMixer *m, int id) {
    if (id <= 0) return NULL;
    for (int i = 0; i < FO_MIXER_VOICES; i++) {
        if (m->voices[i].id == id) return &m->voices[i];
    }
    return NULL;
}

void fo_mixer_set(FOMixer *m, int id, float volume, float pan) {
    pthread_mutex_lock(&m->lock);
    FOVoice *v = find_voice(m, id);
    if (v) {
        v->volume = volume;
        v->pan = pan;
    }
    pthread_mutex_unlock(&m->lock);
}

void fo_mixer_stop(FOMixer *m, int id) {
    pthread_mutex_lock(&m->lock);
    FOVoice *v = find_voice(m, id);
    if (v) v->id = 0;
    pthread_mutex_unlock(&m->lock);
}

int fo_mixer_playing(FOMixer *m, int id) {
    pthread_mutex_lock(&m->lock);
    int playing = find_voice(m, id) != NULL;
    pthread_mutex_unlock(&m->lock);
    return playing;
}

void fo_mixer_stop_all(FOMixer *m) {
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < FO_MIXER_VOICES; i++) m->voices[i].id = 0;
    pthread_mutex_unlock(&m->lock);
}

int fo_mixer_voices(FOMixer *m) {
    pthread_mutex_lock(&m->lock);
    int n = 0;
    for (int i = 0; i < FO_MIXER_VOICES; i++) n += m->voices[i].id != 0;
    pthread_mutex_unlock(&m->lock);
    return n;
}

void fo_mixer_music(FOMixer *m, const char *name, float volume) {
    pthread_mutex_lock(&m->lock);
    int s = name ? find_sound(m, name) : -1;
    if (s != m->music) {
        m->music = s;
        m->musicPos = 0;
        if (s >= 0 && m->sounds[s].stream) {
            stb_vorbis_seek_start(m->sounds[s].stream->vorbis);
            m->sounds[s].stream->first = 0;
            m->sounds[s].stream->count = 0;
        }
    }
    m->musicVolume = volume;
    pthread_mutex_unlock(&m->lock);
}

// The frame at the place of a sound, left and right. A stream decodes the
// next part when the place passes its buffer. Returns 0 at the end.
static int frame_at(FOSound *s, int at, float *left, float *right) {
    if (at < 0 || at >= s->frames) return 0;
    const int16_t *p;
    if (s->stream) {
        FOStream *st = s->stream;
        if (at < st->first) {
            stb_vorbis_seek_start(st->vorbis);
            st->first = 0;
            st->count = 0;
        }
        while (at >= st->first + st->count) {
            st->first += st->count;
            st->count = stb_vorbis_get_samples_short_interleaved(st->vorbis, s->channels, st->buffer,
                                                                 FO_STREAM_FRAMES * s->channels);
            if (st->count <= 0) {
                st->count = 0;
                return 0;
            }
        }
        p = &st->buffer[(at - st->first) * s->channels];
    } else {
        p = &s->samples[at * s->channels];
    }
    *left = p[0] / 32768.0f;
    *right = s->channels == 2 ? p[1] / 32768.0f : *left;
    return 1;
}

// The volumes of the two sides for a pan, as AndroidSound of libGDX: a pan
// to one side lowers the other side.
static void sides(float volume, float pan, float *left, float *right) {
    *left = *right = volume;
    if (pan < 0) *right *= 1.0f - fabsf(pan);
    else if (pan > 0) *left *= 1.0f - fabsf(pan);
}

void fo_mixer_render(FOMixer *m, int16_t *out, int frames) {
    pthread_mutex_lock(&m->lock);
    if (m->mixFrames < frames) {
        free(m->mix);
        m->mix = malloc(sizeof(float) * 2 * (size_t)frames);
        m->mixFrames = m->mix ? frames : 0;
    }
    if (!m->mix) {
        memset(out, 0, sizeof(int16_t) * 2 * (size_t)frames);
        pthread_mutex_unlock(&m->lock);
        return;
    }
    memset(m->mix, 0, sizeof(float) * 2 * (size_t)frames);
    for (int i = 0; i < FO_MIXER_VOICES; i++) {
        FOVoice *v = &m->voices[i];
        if (!v->id) continue;
        FOSound *s = &m->sounds[v->sound];
        double step = (double)s->rate / (double)m->rate;
        float lv, rv;
        sides(v->volume * s->volume, v->pan, &lv, &rv);
        for (int f = 0; f < frames; f++) {
            float l, r;
            if (!frame_at(s, (int)v->pos, &l, &r)) {
                if (v->loop && s->frames > 0) {
                    v->pos = 0;
                    if (!frame_at(s, 0, &l, &r)) break;
                } else {
                    v->id = 0;
                    break;
                }
            }
            m->mix[f * 2] += l * lv;
            m->mix[f * 2 + 1] += r * rv;
            v->pos += step;
        }
    }
    if (m->music >= 0 && m->musicVolume > 0) {
        FOSound *s = &m->sounds[m->music];
        double step = (double)s->rate / (double)m->rate;
        float vol = m->musicVolume * s->volume;
        for (int f = 0; f < frames; f++) {
            float l, r;
            if (!frame_at(s, (int)m->musicPos, &l, &r)) {
                m->musicPos = 0;
                if (!frame_at(s, 0, &l, &r)) break;
            }
            m->mix[f * 2] += l * vol;
            m->mix[f * 2 + 1] += r * vol;
            m->musicPos += step;
        }
    }
    for (int i = 0; i < frames * 2; i++) {
        float x = m->mix[i];
        if (x > 1.0f) x = 1.0f;
        if (x < -1.0f) x = -1.0f;
        out[i] = (int16_t)(x * 32767.0f);
    }
    pthread_mutex_unlock(&m->lock);
}
