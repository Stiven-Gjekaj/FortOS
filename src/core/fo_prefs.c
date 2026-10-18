#include "fo_prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FOPref *find(const FOPrefs *prefs, const char *key) {
    for (int i = 0; i < prefs->count; i++) {
        if (strcmp(prefs->items[i].key, key) == 0) return (FOPref *)&prefs->items[i];
    }
    return NULL;
}

// Removes the spaces at the two ends of s, in place.
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
    return s;
}

int fo_prefs_load(FOPrefs *prefs, const char *path) {
    memset(prefs, 0, sizeof(*prefs));
    if (strlen(path) >= sizeof(prefs->path)) return 0;
    strcpy(prefs->path, path);
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = 0;
        fo_prefs_set_string(prefs, trim(line), trim(eq + 1));
    }
    fclose(f);
    return 1;
}

int fo_prefs_save(const FOPrefs *prefs) {
    char tmp[sizeof(prefs->path) + 8];
    snprintf(tmp, sizeof(tmp), "%s.new", prefs->path);
    FILE *f = fopen(tmp, "w");
    if (!f) return 0;
    for (int i = 0; i < prefs->count; i++) fprintf(f, "%s=%s\n", prefs->items[i].key, prefs->items[i].value);
    if (fclose(f) != 0) return 0;
    return rename(tmp, prefs->path) == 0;
}

const char *fo_prefs_string(const FOPrefs *prefs, const char *key, const char *fallback) {
    const FOPref *p = find(prefs, key);
    return p ? p->value : fallback;
}

int fo_prefs_int(const FOPrefs *prefs, const char *key, int fallback) {
    const FOPref *p = find(prefs, key);
    if (!p) return fallback;
    char *end;
    long v = strtol(p->value, &end, 10);
    return end != p->value ? (int)v : fallback;
}

float fo_prefs_float(const FOPrefs *prefs, const char *key, float fallback) {
    const FOPref *p = find(prefs, key);
    if (!p) return fallback;
    char *end;
    float v = strtof(p->value, &end);
    return end != p->value ? v : fallback;
}

int fo_prefs_set_string(FOPrefs *prefs, const char *key, const char *value) {
    if (!key[0] || strlen(key) >= sizeof(prefs->items[0].key) || strlen(value) >= sizeof(prefs->items[0].value)) return 0;
    if (strchr(key, '=') || strchr(key, '\n') || strchr(value, '\n')) return 0;
    FOPref *p = find(prefs, key);
    if (!p) {
        if (prefs->count >= FO_PREFS_MAX) return 0;
        p = &prefs->items[prefs->count++];
        strcpy(p->key, key);
    }
    strcpy(p->value, value);
    return 1;
}

int fo_prefs_set_int(FOPrefs *prefs, const char *key, int value) {
    char s[32];
    snprintf(s, sizeof(s), "%d", value);
    return fo_prefs_set_string(prefs, key, s);
}

int fo_prefs_set_float(FOPrefs *prefs, const char *key, float value) {
    char s[32];
    snprintf(s, sizeof(s), "%.6g", value);
    return fo_prefs_set_string(prefs, key, s);
}
