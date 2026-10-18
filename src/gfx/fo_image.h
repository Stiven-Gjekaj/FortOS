// fo_image: makes a texture from the bytes of a PNG file.

#ifndef FO_IMAGE_H
#define FO_IMAGE_H

#include <stddef.h>
#include "fo_gfx.h"

// Decodes the PNG and makes a texture. With a scale other than 1, the
// image is resized first with a high quality filter, so that the GPU can
// draw it at the size of the screen with no blur. Returns a texture with
// id 0 when the bytes are not an image.
FOTexture fo_image_texture(const unsigned char *png, size_t size, float scale);

// Decodes and resizes the PNG as fo_image_texture does, and gives the
// premultiplied RGBA pixels, which the caller frees. Returns NULL when the
// bytes are not an image.
unsigned char *fo_image_decode(const unsigned char *png, size_t size, float scale, int *width, int *height);

#endif
