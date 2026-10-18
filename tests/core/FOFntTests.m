// Tests of fo_fnt. Each test writes its own font text.

#import "FOTest.h"
#include "fo_fnt.h"

static const char *kFont =
    "info face=\"\" size=12 bold=0 italic=0 padding=1,0,1,1 spacing=0,0\n"
    "common lineHeight=16 base=14 scaleW=512 scaleH=64 pages=1 packed=0\n"
    "page id=0 file=\"pixel12.png\"\n"
    "chars count=3\n"
    "char id=32    x=0    y=0    width=0    height=0    xoffset=-1   yoffset=-1   xadvance=5    page=0\n"
    "char id=33    x=110  y=1    width=4    height=13   xoffset=-1   yoffset=1    xadvance=3    page=0\n"
    "char id=65    x=20   y=2    width=8    height=12   xoffset=0    yoffset=2    xadvance=7    page=0\n"
    "kernings count=0\n";

FO_TEST(testFntReadsTheFont) {
    FOFnt font;
    FO_EXPECT(fo_fnt_parse(&font, kFont));
    FO_EQUAL_INT(font.line_height, 16);
    FO_EQUAL_INT(font.base, 14);
    FO_EQUAL_INT(font.page_w, 512);
    FO_EQUAL(@(font.page_file), @"pixel12.png");
    FO_EQUAL_INT(font.pad_top, 1);
    FO_EQUAL_INT(font.pad_left, 1);
    FO_EXPECT(font.glyphs['!'].present);
    FO_EQUAL_INT(font.glyphs['!'].x, 110);
    FO_EQUAL_INT(font.glyphs['!'].xoff, -1);
    FO_EQUAL_INT(font.glyphs['A'].yoff, 2);
    FO_EXPECT(!font.glyphs['B'].present);
}

FO_TEST(testFntMeasuresText) {
    FOFnt font;
    fo_fnt_parse(&font, kFont);
    FO_EQUAL_INT(fo_fnt_width(&font, "A! A"), 7 + 3 + 5 + 7);
    FO_EQUAL_INT(fo_fnt_width(&font, "AB"), 7);
    FO_EQUAL_INT(fo_fnt_width(&font, "!\nAA"), 14);
}

FO_TEST(testFntRefusesOtherText) {
    FOFnt font;
    FO_EXPECT(!fo_fnt_parse(&font, "hello"));
}
