// fo_items: the items of GANGFORT (ItemDataHelper, aao.java): the 34
// hats, the default weapons, and the 5 weapons of the store. In FortOS the
// player owns all of them. tools/items.py writes the table in fo_items.c.

#ifndef FO_ITEMS_H
#define FO_ITEMS_H

#define FO_ITEM_COUNT 49
#define FO_HAT_POLYGONS 4
#define FO_HAT_VERTICES 8

typedef enum {
    FO_ITEM_HAT = 1,
    FO_ITEM_WEAPON = 2,   // The primary weapon.
    FO_ITEM_MELEE = 3,    // The knife of the spy.
} FOItemKind;

// The hats that play their frames at times. The others play them once
// when the player gets the hat.
typedef enum {
    FO_HAT_STILL = 0,
    FO_HAT_GOLDEN,        // GoldenHat: each 3 to 7 s, and golden drops at each point.
    FO_HAT_SCORE,         // ConfettiHat: at each point of the player.
    FO_HAT_SPARKLE,       // DiamondHat: each 1 to 3 s.
} FOHatEffect;

typedef struct {
    int id;
    FOItemKind kind;
    unsigned int classes;   // Bit c is set when class c can use the item.
    const char *name;       // The name of the class of the original.
    const char *title;      // The keys of the texts.
    const char *description;
    const char *icon;       // The picture in ui.atlas, or NULL for a default weapon.
    // The hats only. All sizes are meters, one art pixel is 0.1 m.
    const char *region;     // The still region, or NULL.
    const char *const *frames;
    int frameCount;
    float frameTime;
    int loop;
    FOHatEffect effect;
    float w, h;             // The size of the first region.
    float offsets[10][2];   // a(int): the middle of the hat from the left of the head, by class.
    int polygonCount;
    float polygons[FO_HAT_POLYGONS][FO_HAT_VERTICES * 2];  // From the bottom left of the region.
    int polygonSizes[FO_HAT_POLYGONS];
} FOItem;

extern const FOItem kFOItems[FO_ITEM_COUNT];

// The item with the id, or NULL.
const FOItem *fo_item(int id);

// The class can use the item (aao.b(int, int)).
int fo_item_allowed(int id, int cls);

// The items of the kind that the class can use, in the order of the
// original list. Returns the count.
int fo_items_for_class(int cls, FOItemKind kind, int *ids, int max);

#endif
