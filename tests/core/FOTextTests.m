// Tests of fo_text. Each test writes its own text.

#import "FOTest.h"
#include "fo_text.h"

FO_TEST(testTextReadsKeysAndEscapes) {
    FOText *t = fo_text_parse("# comment\nyou=you\r\nitem_a_description=line one\\nline two\n"
                              "  spaced = value \nname=\\u00fcber\n");
    FO_EXPECT(t != NULL);
    FO_EQUAL(@(fo_text_get(t, "you")), @"you");
    FO_EQUAL(@(fo_text_get(t, "item_a_description")), @"line one\nline two");
    FO_EQUAL(@(fo_text_get(t, "spaced")), @"value ");
    FO_EQUAL(@(fo_text_get(t, "name")), @"\u00fcber");
    FO_EQUAL(@(fo_text_get(t, "missing")), @"missing");
    fo_text_free(t);
}

FO_TEST(testTextFormatsChoices) {
    const char *p = "you will respawn in {0,choice,1#1 second|1<{0,number,integer} seconds}";
    char out[128];
    fo_text_format(p, 1, out, sizeof(out));
    FO_EQUAL(@(out), @"you will respawn in 1 second");
    fo_text_format(p, 7, out, sizeof(out));
    FO_EQUAL(@(out), @"you will respawn in 7 seconds");
    fo_text_format("{0} kills", 3, out, sizeof(out));
    FO_EQUAL(@(out), @"3 kills");
}

FO_TEST(testTextRefusesEmptyText) {
    FO_EXPECT(fo_text_parse("") == NULL);
    FO_EXPECT(fo_text_parse("# only a comment\n") == NULL);
}

FO_TEST(testTextFallsBackToTheParentBundle) {
    // gangforttexts_es_ES has fewer keys than gangforttexts_es, and that one
    // fewer than gangforttexts. A missing key comes from the next one.
    FOText *base = fo_text_parse("play=play\nquit=quit\nyou=you\n");
    FOText *es = fo_text_parse("play=jugar\nquit=salir\n");
    FOText *esES = fo_text_parse("play=jugar ya\n");
    fo_text_set_parent(es, base);
    fo_text_set_parent(esES, es);
    FO_EQUAL(@(fo_text_get(esES, "play")), @"jugar ya");
    FO_EQUAL(@(fo_text_get(esES, "quit")), @"salir");
    FO_EQUAL(@(fo_text_get(esES, "you")), @"you");
    FO_EQUAL(@(fo_text_get(esES, "missing")), @"missing");
    fo_text_free(esES);
}
