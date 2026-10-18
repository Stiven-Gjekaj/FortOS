// fo_prefs: the saved choices of the player, as the Preferences of libGDX
// (akb.java). The file has one "key=value" line for each choice.

#ifndef FO_PREFS_H
#define FO_PREFS_H

#define FO_PREFS_MAX 256

typedef struct {
    char key[48];
    char value[80];
} FOPref;

typedef struct {
    char path[512];
    FOPref items[FO_PREFS_MAX];
    int count;
} FOPrefs;

// Reads the file at path. A file that is not there gives no choices.
// Returns 0 only when the path is too long.
int fo_prefs_load(FOPrefs *prefs, const char *path);

// Writes all the choices to the path of fo_prefs_load. It writes a second
// file and then moves it on the first, so a stop in the middle keeps the
// old file. Returns 0 when it cannot write.
int fo_prefs_save(const FOPrefs *prefs);

// The value of key, or fallback when the key is not there.
const char *fo_prefs_string(const FOPrefs *prefs, const char *key, const char *fallback);
int fo_prefs_int(const FOPrefs *prefs, const char *key, int fallback);
float fo_prefs_float(const FOPrefs *prefs, const char *key, float fallback);

// Sets the value of key. Returns 0 when the key or the value is too long,
// or when the list is full.
int fo_prefs_set_string(FOPrefs *prefs, const char *key, const char *value);
int fo_prefs_set_int(FOPrefs *prefs, const char *key, int value);
int fo_prefs_set_float(FOPrefs *prefs, const char *key, float value);

#endif
