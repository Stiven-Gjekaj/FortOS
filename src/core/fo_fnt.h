// fo_fnt: reads the text form of an AngelCode BMFont file, such as
// res/fonts/pixel12.fnt. The glyph positions are in the page, with y down.

#ifndef FO_FNT_H
#define FO_FNT_H

typedef struct {
    int x, y, w, h;   // The glyph in the page.
    int xoff, yoff;   // The place of the glyph from the pen, y down from the top of the line.
    int advance;      // The move of the pen after the glyph.
    int present;      // 1 when the font has the glyph.
} FOGlyph;

#define FO_FNT_GLYPHS 256

typedef struct {
    int line_height;
    int base;
    int page_w, page_h;
    char page_file[64];
    int pad_top, pad_right, pad_bottom, pad_left;
    FOGlyph glyphs[FO_FNT_GLYPHS];  // The glyphs with the codes 0 to 255.
} FOFnt;

// Reads the text of a .fnt file. Returns 0 when the text is not a font.
int fo_fnt_parse(FOFnt *font, const char *text);

// The width of text in font pixels.
int fo_fnt_width(const FOFnt *font, const char *text);

#endif
