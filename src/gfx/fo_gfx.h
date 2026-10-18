// fo_gfx: a sprite batch for OpenGL ES 2.0.
// All coordinates are y-up, as in libGDX. fo_gfx_view sets the rectangle
// of the world that the screen shows. The HUD uses a view in pixels with
// (0, 0) at the bottom left.

#ifndef FO_GFX_H
#define FO_GFX_H

#include <stddef.h>

typedef struct {
    unsigned int id;  // The name of the GL texture.
    int width;        // The size of the texture in texels.
    int height;
} FOTexture;

typedef struct {
    float r, g, b, a;
} FOColor;

// A rectangle of a texture. u0, v0 is the top left texel, as in the
// atlas files. w and h are the size in texels.
typedef struct {
    const FOTexture *texture;
    float u0, v0, u1, v1;
    int w, h;
} FOSprite;

// Makes the shaders and the buffers. Call it with a current GL context.
// Returns 0 when a shader does not compile.
int fo_gfx_init(void);

// Starts a frame with the size of the drawable in pixels, and clears it.
void fo_gfx_begin(int width, int height, FOColor clear);

// The screen shows the rectangle from (left, bottom) to (right, top).
// It flushes the sprites that wait.
void fo_gfx_view(float left, float bottom, float right, float top);

// The depth modes. FO_DEPTH_OPAQUE draws with no blending and writes the
// depth, so that the GPU skips the pixels that a nearer opaque sprite
// hides. FO_DEPTH_TEST blends and draws only in front of the depth.
enum {
    FO_DEPTH_OFF = 0,
    FO_DEPTH_OPAQUE = 1,
    FO_DEPTH_TEST = 2,
};

// Sets the depth mode and the depth z of the next sprites, from -1 (near)
// to 1 (far).
void fo_gfx_depth(int mode, float z);

// The pass of the scope of the sniper rifle: the next sprites take their
// alpha from the red channel of mask. The mask covers the world rectangle
// (x, y, w, h) and turns by angle radians around its middle.
void fo_gfx_scope_begin(const FOTexture *mask, float x, float y, float w, float h, float angle);
void fo_gfx_scope_end(void);

// Darkens the edges of the frame, as the vignette of GANGFORT. Call it
// after the frame is drawn.
void fo_gfx_vignette(void);

// Draws sprite with its bottom left corner at (x, y) and the size (w, h).
// The sprite turns by angle radians around the point (x + ox, y + oy).
// With flip, the texels go from right to left.
void fo_gfx_draw(const FOSprite *sprite, float x, float y, float w, float h, float ox, float oy, float angle,
                 int flip, FOColor tint);

// Fills a rectangle with one colour.
void fo_gfx_rect(float x, float y, float w, float h, FOColor color);

// Fills a turned rectangle: a line of thickness from (x1, y1) to (x2, y2).
void fo_gfx_line(float x1, float y1, float x2, float y2, float thickness, FOColor color);

// Sends the sprites that wait in the batch to the GPU.
void fo_gfx_flush(void);

// The number of draw calls in the last frame, for the speed log.
int fo_gfx_draw_calls(void);

// Makes a texture from RGBA pixels with premultiplied alpha. The texture
// uses nearest filtering, as all atlases of GANGFORT do.
FOTexture fo_gfx_texture(const unsigned char *rgba, int width, int height);
void fo_gfx_texture_free(FOTexture *texture);

// A sprite for the whole texture.
FOSprite fo_gfx_whole(const FOTexture *texture);

FOColor fo_rgb(unsigned int hex);
FOColor fo_rgba(unsigned int hex, float alpha);

extern const FOColor FO_WHITE;

#endif
