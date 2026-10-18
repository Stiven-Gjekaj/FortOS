// Tests of fo_prefs. Each test writes its own file in a new directory.

#import "FOTest.h"
#include "fo_prefs.h"

static NSString *temp_file(NSString *name) {
    NSString *dir = [NSTemporaryDirectory() stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:NULL];
    return [dir stringByAppendingPathComponent:name];
}

FO_TEST(testPrefsGiveTheFallbackWithNoFile) {
    FOPrefs p;
    FO_EXPECT(fo_prefs_load(&p, temp_file(@"none.prefs").UTF8String));
    FO_EQUAL(@(p.count), @0);
    FO_EQUAL(@(fo_prefs_int(&p, "killcam_enabled", 1)), @1);
    FO_EQUAL(@(fo_prefs_string(&p, "hat", "none")), @"none");
}

FO_TEST(testPrefsKeepTheValuesAfterASave) {
    NSString *path = temp_file(@"a.prefs");
    FOPrefs p;
    fo_prefs_load(&p, path.UTF8String);
    FO_EXPECT(fo_prefs_set_int(&p, "killcam_enabled", 0));
    FO_EXPECT(fo_prefs_set_float(&p, "stick_x", 0.25f));
    FO_EXPECT(fo_prefs_set_string(&p, "hat_1", "hat_cap"));
    FO_EXPECT(fo_prefs_set_int(&p, "killcam_enabled", 1));
    FO_EQUAL(@(p.count), @3);
    FO_EXPECT(fo_prefs_save(&p));
    FOPrefs q;
    fo_prefs_load(&q, path.UTF8String);
    FO_EQUAL(@(q.count), @3);
    FO_EQUAL(@(fo_prefs_int(&q, "killcam_enabled", 7)), @1);
    FO_EQUAL(@(fo_prefs_float(&q, "stick_x", 0)), @0.25f);
    FO_EQUAL(@(fo_prefs_string(&q, "hat_1", "")), @"hat_cap");
    FO_EXPECT(![[NSFileManager defaultManager] fileExistsAtPath:[path stringByAppendingString:@".new"]]);
}

FO_TEST(testPrefsReadLinesWithSpacesAndSkipBadLines) {
    NSString *path = temp_file(@"b.prefs");
    [@"# a comment\n  music  =  0 \r\nno equals sign\nvignette=1\n=5\nbad_number=abc\n" writeToFile:path
                                                                                       atomically:YES
                                                                                         encoding:NSUTF8StringEncoding
                                                                                            error:NULL];
    FOPrefs p;
    fo_prefs_load(&p, path.UTF8String);
    FO_EQUAL(@(p.count), @3);
    FO_EQUAL(@(fo_prefs_int(&p, "music", 1)), @0);
    FO_EQUAL(@(fo_prefs_int(&p, "vignette", 0)), @1);
    FO_EQUAL(@(fo_prefs_int(&p, "bad_number", 9)), @9);
}

FO_TEST(testPrefsRefuseKeysThatBreakTheFile) {
    FOPrefs p;
    fo_prefs_load(&p, temp_file(@"c.prefs").UTF8String);
    FO_EXPECT(!fo_prefs_set_string(&p, "a=b", "1"));
    FO_EXPECT(!fo_prefs_set_string(&p, "", "1"));
    FO_EXPECT(!fo_prefs_set_string(&p, "key", "two\nlines"));
    FO_EQUAL(@(p.count), @0);
}
