// fo_text: the texts of GANGFORT, from res/texts/gangforttexts.properties.
// It reads the keys and the values of a .properties file, and it fills the
// MessageFormat patterns that the texts use: {0}, {0,number,integer}, and
// {0,choice,1#one|1<many}.

#ifndef FO_TEXT_H
#define FO_TEXT_H

#include <stddef.h>

typedef struct FOText FOText;

// Reads the text of a .properties file. Returns NULL when it has no key.
FOText *fo_text_parse(const char *text);
void fo_text_free(FOText *text);

// Puts parent behind text, as the bundle of the more general locale of an
// I18NBundle: a key that text does not have comes from parent. text owns
// parent and frees it.
void fo_text_set_parent(FOText *text, FOText *parent);

// The value of the key, or the key itself when the file does not have it,
// as the I18NBundle of libGDX shows a missing key.
const char *fo_text_get(const FOText *text, const char *key);

// Fills pattern with the number n into out.
void fo_text_format(const char *pattern, int n, char *out, size_t size);

#endif
