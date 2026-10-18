#include "fo_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *key;
    char *value;
} FOTextEntry;

struct FOText {
    FOTextEntry *entries;
    int count;
};

// Adds the UTF-8 bytes of the code point c to out.
static size_t put_utf8(char *out, unsigned int c) {
    if (c < 0x80) {
        out[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        out[0] = (char)(0xc0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3f));
        return 2;
    }
    out[0] = (char)(0xe0 | (c >> 12));
    out[1] = (char)(0x80 | ((c >> 6) & 0x3f));
    out[2] = (char)(0x80 | (c & 0x3f));
    return 3;
}

// Copies the characters from start to end into a new string, with the
// escapes of a .properties file: \n, \t, \\, \uXXXX, and a backslash before
// another character.
static char *unescape(const char *start, const char *end) {
    char *out = malloc((size_t)(end - start) * 3 + 1);
    if (!out) return NULL;
    size_t n = 0;
    for (const char *p = start; p < end; p++) {
        if (*p != '\\' || p + 1 >= end) {
            out[n++] = *p;
            continue;
        }
        p++;
        if (*p == 'n') {
            out[n++] = '\n';
        } else if (*p == 't') {
            out[n++] = '\t';
        } else if (*p == 'u' && p + 4 < end) {
            char hex[5] = {p[1], p[2], p[3], p[4], 0};
            n += put_utf8(out + n, (unsigned int)strtoul(hex, NULL, 16));
            p += 4;
        } else {
            out[n++] = *p;
        }
    }
    out[n] = 0;
    return out;
}

FOText *fo_text_parse(const char *text) {
    FOText *t = calloc(1, sizeof(FOText));
    if (!t) return NULL;
    int capacity = 0;
    const char *p = text;
    while (*p) {
        const char *line = p;
        while (*p && *p != '\n') p++;
        const char *end = p;
        if (*p) p++;
        if (end > line && end[-1] == '\r') end--;
        while (line < end && (*line == ' ' || *line == '\t')) line++;
        if (line == end || *line == '#' || *line == '!') continue;
        const char *eq = line;
        while (eq < end && *eq != '=' && *eq != ':') eq++;
        if (eq == end) continue;
        const char *keyEnd = eq;
        while (keyEnd > line && (keyEnd[-1] == ' ' || keyEnd[-1] == '\t')) keyEnd--;
        const char *value = eq + 1;
        while (value < end && (*value == ' ' || *value == '\t')) value++;
        if (t->count == capacity) {
            capacity = capacity ? capacity * 2 : 256;
            FOTextEntry *bigger = realloc(t->entries, sizeof(FOTextEntry) * (size_t)capacity);
            if (!bigger) break;
            t->entries = bigger;
        }
        t->entries[t->count].key = unescape(line, keyEnd);
        t->entries[t->count].value = unescape(value, end);
        t->count++;
    }
    if (t->count == 0) {
        fo_text_free(t);
        return NULL;
    }
    return t;
}

void fo_text_free(FOText *t) {
    if (!t) return;
    for (int i = 0; i < t->count; i++) {
        free(t->entries[i].key);
        free(t->entries[i].value);
    }
    free(t->entries);
    free(t);
}

const char *fo_text_get(const FOText *t, const char *key) {
    if (t) {
        for (int i = 0; i < t->count; i++) {
            if (strcmp(t->entries[i].key, key) == 0) return t->entries[i].value;
        }
    }
    return key;
}

// Fills one {...} element, from after '{' to before its '}'.
static void element(const char *start, const char *end, int n, char *out, size_t size) {
    const char *choice = strstr(start, ",choice,");
    if (!choice || choice > end) {
        snprintf(out, size, "%d", n);
        return;
    }
    // A choice: "1#one|1<many". Each limit "a#" means n >= a, and "a<"
    // means n > a. The last limit that n passes gives the text.
    const char *p = choice + 8;
    const char *bestStart = NULL, *bestEnd = NULL;
    while (p < end) {
        char *mark;
        double limit = strtod(p, &mark);
        char kind = *mark;
        const char *text = mark + 1;
        // The text ends at a '|' outside braces.
        const char *q = text;
        int depth = 0;
        while (q < end && (depth > 0 || *q != '|')) {
            if (*q == '{') depth++;
            if (*q == '}') depth--;
            q++;
        }
        int pass = kind == '<' ? n > limit : n >= limit;
        if (pass || !bestStart) {
            bestStart = text;
            bestEnd = q;
        }
        p = q + 1;
    }
    char sub[256];
    snprintf(sub, sizeof(sub), "%.*s", (int)(bestEnd - bestStart), bestStart);
    fo_text_format(sub, n, out, size);
}

void fo_text_format(const char *pattern, int n, char *out, size_t size) {
    size_t used = 0;
    if (size == 0) return;
    for (const char *p = pattern; *p && used + 1 < size;) {
        if (*p != '{') {
            out[used++] = *p++;
            continue;
        }
        // Find the closing brace of this element.
        const char *q = p + 1;
        int depth = 1;
        while (*q && depth > 0) {
            if (*q == '{') depth++;
            if (*q == '}') depth--;
            if (depth > 0) q++;
        }
        char part[256];
        element(p + 1, q, n, part, sizeof(part));
        for (const char *c = part; *c && used + 1 < size; c++) out[used++] = *c;
        p = *q ? q + 1 : q;
    }
    out[used] = 0;
}
