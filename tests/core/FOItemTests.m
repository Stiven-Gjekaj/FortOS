// Tests of fo_items: the table that tools/items.py writes, and its lookups.

#import "FOTest.h"
#include "fo_items.h"

FO_TEST(testItemsHave34HatsForAllClasses) {
    int hats = 0;
    for (int i = 0; i < FO_ITEM_COUNT; i++) {
        if (kFOItems[i].kind != FO_ITEM_HAT) continue;
        hats++;
        for (int c = 1; c <= 9; c++) FO_EXPECT(fo_item_allowed(kFOItems[i].id, c));
        FO_EXPECT(kFOItems[i].region != NULL || kFOItems[i].frameCount > 0);
        FO_EXPECT(kFOItems[i].polygonCount >= 1);
    }
    FO_EQUAL(@(hats), @34);
}

FO_TEST(testItemsKeepThePlaceOfTheGoldenHat) {
    // GoldenHat.a(int) (abn.java): (4, 1.5) art pixels for the rusher.
    const FOItem *h = fo_item(1);
    FO_EXPECT(h != NULL);
    FO_EQUAL(@(h->offsets[1][0]), @0.4f);
    FO_EQUAL(@(h->offsets[1][1]), @0.15f);
    FO_EQUAL(@(h->offsets[8][0]), @0.65f);
    FO_EQUAL(@(h->effect), @(FO_HAT_GOLDEN));
}

FO_TEST(testItemsGiveTheWeaponsOfEachClass) {
    int ids[8];
    int n = fo_items_for_class(5, FO_ITEM_WEAPON, ids, 8);
    FO_EQUAL(@(n), @2);
    FO_EQUAL(@(ids[0]), @35);
    FO_EQUAL(@(ids[1]), @46);
    FO_EXPECT(!fo_item_allowed(46, 1));
    FO_EQUAL(@(fo_items_for_class(9, FO_ITEM_MELEE, ids, 8)), @1);
    FO_EXPECT(fo_item(40) == NULL);
    FO_EXPECT(fo_item(999) == NULL);
}
