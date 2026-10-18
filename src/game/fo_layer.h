// fo_layer: draws the big images of a map in two passes. The opaque pass
// draws the opaque cells with no blending, into the depth buffer. The
// blend pass draws the mixed cells. Neither pass draws an empty cell.
// Reason: the A5 cannot blend six layers over the full screen at 60
// frames per second.

#ifndef FO_LAYER_H
#define FO_LAYER_H

#include "fo_assets.h"

enum {
    FO_PASS_OPAQUE = 0,
    FO_PASS_BLEND = 1,
};

// Draws the cells of layer that the view shows. The bottom left of the
// layer is at (x, y), and one art pixel is art meters. z is the depth of
// the layer. The view is the rectangle of fo_gfx_view.
void fo_layer_draw(const FOLayer *layer, int pass, float x, float y, float art, float z,
                   float left, float bottom, float right, float top);

#endif
