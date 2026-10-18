#include "fo_items.h"

#include <stddef.h>

const FOItem *fo_item(int id) {
    for (int i = 0; i < FO_ITEM_COUNT; i++) {
        if (kFOItems[i].id == id) return &kFOItems[i];
    }
    return NULL;
}

int fo_item_allowed(int id, int cls) {
    const FOItem *item = fo_item(id);
    return item && cls >= 0 && cls < 32 && (item->classes >> cls & 1);
}

int fo_items_for_class(int cls, FOItemKind kind, int *ids, int max) {
    int n = 0;
    for (int i = 0; i < FO_ITEM_COUNT && n < max; i++) {
        if (kFOItems[i].kind == kind && fo_item_allowed(kFOItems[i].id, cls)) ids[n++] = kFOItems[i].id;
    }
    return n;
}
