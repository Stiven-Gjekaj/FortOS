// fo_zip: reads the files in a zip archive, such as Balatro.love.
// It reads stored and deflated entries. It does not write archives.

#ifndef FO_ZIP_H
#define FO_ZIP_H

#include <stddef.h>

typedef struct FOZip FOZip;

// Opens the archive at path and reads its central directory.
// Returns NULL when the file is not a zip archive.
FOZip *fo_zip_open(const char *path);
void fo_zip_close(FOZip *zip);

// The number of entries, and the name of the entry at index.
int fo_zip_count(const FOZip *zip);
const char *fo_zip_name(const FOZip *zip, int index);

// Reads the entry with the full name, for example
// "resources/textures/2x/Jokers.png". Returns a buffer that the caller
// frees with free(), or NULL when the entry is not there or is damaged.
// The buffer has one extra zero byte after the data, so that text can be
// read as a C string. size gets the size of the data without that byte.
unsigned char *fo_zip_read(FOZip *zip, const char *name, size_t *size);

#endif
