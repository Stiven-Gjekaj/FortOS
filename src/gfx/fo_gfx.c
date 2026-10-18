#include "fo_gfx.h"

#include <OpenGLES/ES2/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// One vertex: the position, the texture coordinate, and the tint with
// premultiplied alpha as four bytes.
typedef struct {
    float x, y, z;
    float u, v;
    unsigned char r, g, b, a;
} FOVertex;

#define FO_MAX_SPRITES 2048

typedef struct {
    GLuint program;
    GLint view;        // vec4: the scale and the offset of the view.
    GLint resolution;  // vec2: the size of the screen in pixels.
} FOProgram;

static FOVertex gVertices[FO_MAX_SPRITES * 4];
static int gSprites;
static FOProgram gPlain;
static FOTexture gVignette;
static FOProgram *gProgram;
static GLuint gIndexBuffer;
static GLuint gVertexBuffer;
static GLuint gCurrentTexture;
static FOTexture gWhite;
static int gDrawCalls;
static int gLastDrawCalls;
static float gView[4] = {1, 1, 0, 0};
static float gScreenW = 1, gScreenH = 1;
static float gDepth;
static int gDepthMode = FO_DEPTH_OFF;

const FOColor FO_WHITE = {1, 1, 1, 1};

static const char *kVertexShader =
    "uniform vec4 view;\n"
    "attribute vec3 position;\n"
    "attribute vec2 texcoord;\n"
    "attribute vec4 color;\n"
    "varying vec2 v_texcoord;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    v_texcoord = texcoord;\n"
    "    v_color = color;\n"
    "    gl_Position = vec4(position.xy * view.xy + view.zw, position.z, 1.0);\n"
    "}\n";

static const char *kPlainShader =
    "uniform sampler2D texture;\n"
    "varying mediump vec2 v_texcoord;\n"
    "varying lowp vec4 v_color;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(texture, v_texcoord) * v_color;\n"
    "}\n";

static GLuint compile(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "fo_gfx: shader: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int make_program(FOProgram *out, GLuint vertex, const char *fragmentSource) {
    GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) return 0;
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "texcoord");
    glBindAttribLocation(program, 2, "color");
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) return 0;
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "texture"), 0);
    out->program = program;
    out->view = glGetUniformLocation(program, "view");
    out->resolution = glGetUniformLocation(program, "resolution");
    return 1;
}

static void use_program(FOProgram *program) {
    gProgram = program;
    glUseProgram(program->program);
    glUniform4f(program->view, gView[0], gView[1], gView[2], gView[3]);
    if (program->resolution >= 0) glUniform2f(program->resolution, gScreenW, gScreenH);
}

int fo_gfx_init(void) {
    GLuint vertex = compile(GL_VERTEX_SHADER, kVertexShader);
    if (!make_program(&gPlain, vertex, kPlainShader)) return 0;

    // Each sprite is two triangles over four vertices.
    static GLushort indices[FO_MAX_SPRITES * 6];
    for (int i = 0; i < FO_MAX_SPRITES; i++) {
        GLushort base = (GLushort)(i * 4);
        GLushort quad[6] = {base, (GLushort)(base + 1), (GLushort)(base + 2),
                            (GLushort)(base + 2), (GLushort)(base + 1), (GLushort)(base + 3)};
        memcpy(&indices[i * 6], quad, sizeof(quad));
    }
    glGenBuffers(1, &gIndexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glGenBuffers(1, &gVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, gVertexBuffer);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FOVertex), (void *)offsetof(FOVertex, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FOVertex), (void *)offsetof(FOVertex, u));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FOVertex), (void *)offsetof(FOVertex, r));

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    static const unsigned char white[4 * 4 * 4] = {
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    gWhite = fo_gfx_texture(white, 4, 4);
    use_program(&gPlain);
    return 1;
}

void fo_gfx_begin(int width, int height, FOColor clear) {
    gLastDrawCalls = gDrawCalls;
    gDrawCalls = 0;
    gScreenW = (float)width;
    gScreenH = (float)height;
    glViewport(0, 0, width, height);
    glClearColor(clear.r, clear.g, clear.b, clear.a);
    glDepthMask(GL_TRUE);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gDepthMode = -1;
    fo_gfx_depth(FO_DEPTH_OFF, 0);
    use_program(&gPlain);
    fo_gfx_view(0, 0, gScreenW, gScreenH);
}

void fo_gfx_view(float left, float bottom, float right, float top) {
    fo_gfx_flush();
    gView[0] = 2 / (right - left);
    gView[1] = 2 / (top - bottom);
    gView[2] = -1 - left * gView[0];
    gView[3] = -1 - bottom * gView[1];
    glUniform4f(gProgram->view, gView[0], gView[1], gView[2], gView[3]);
}

void fo_gfx_depth(int mode, float z) {
    gDepth = z;
    if (mode == gDepthMode) return;
    fo_gfx_flush();
    gDepthMode = mode;
    if (mode == FO_DEPTH_OFF) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
    } else if (mode == FO_DEPTH_OPAQUE) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    } else {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
    }
}

// res/shaders/vignette.frag of GANGFORT gives color * mix(1, v, 0.45), with
// v = smoothstep(0.65, 0.45, the distance from the middle of the screen in
// screen units). This texture holds 0.55 + 0.45 * v, and a multiply blend
// puts it over the frame in one pass. Reason: the shader of the original
// is too slow on the A5 for each pixel of each layer.
static void make_vignette(void) {
    enum { size = 64 };
    static unsigned char pixels[size * size * 4];
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x + 0.5f) / size - 0.5f, dy = (y + 0.5f) / size - 0.5f;
            float len = sqrtf(dx * dx + dy * dy);
            float t = (len - 0.65f) / (0.45f - 0.65f);
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            float v = t * t * (3 - 2 * t);
            unsigned char m = (unsigned char)((0.55f + 0.45f * v) * 255 + 0.5f);
            unsigned char *p = &pixels[(y * size + x) * 4];
            p[0] = p[1] = p[2] = m;
            p[3] = 255;
        }
    }
    gVignette = fo_gfx_texture(pixels, size, size);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void fo_gfx_vignette(void) {
    if (!gVignette.id) make_vignette();
    fo_gfx_flush();
    fo_gfx_depth(FO_DEPTH_OFF, 0);
    fo_gfx_view(0, 0, 1, 1);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    FOSprite s = fo_gfx_whole(&gVignette);
    fo_gfx_draw(&s, 0, 0, 1, 1, 0, 0, 0, 0, FO_WHITE);
    fo_gfx_flush();
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void fo_gfx_flush(void) {
    if (gSprites == 0) return;
    // A new buffer each time lets the GPU keep the old one while it draws.
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(FOVertex) * 4 * gSprites), NULL, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(FOVertex) * 4 * gSprites), gVertices);
    glDrawElements(GL_TRIANGLES, gSprites * 6, GL_UNSIGNED_SHORT, 0);
    gSprites = 0;
    gDrawCalls++;
}

static unsigned char byte(float value) {
    if (value <= 0) return 0;
    if (value >= 1) return 255;
    return (unsigned char)(value * 255.0f + 0.5f);
}

// Adds one quad. The corners are in the order bottom left, bottom right,
// top left, top right, and each corner has its texture coordinate.
static void push(GLuint texture, const float corners[4][4], FOColor tint) {
    if (texture != gCurrentTexture) {
        fo_gfx_flush();
        glBindTexture(GL_TEXTURE_2D, texture);
        gCurrentTexture = texture;
    }
    if (gSprites == FO_MAX_SPRITES) fo_gfx_flush();
    unsigned char r = byte(tint.r * tint.a), g = byte(tint.g * tint.a), b = byte(tint.b * tint.a), a = byte(tint.a);
    FOVertex *out = &gVertices[gSprites * 4];
    for (int i = 0; i < 4; i++) {
        out[i].x = corners[i][0];
        out[i].y = corners[i][1];
        out[i].z = gDepth;
        out[i].u = corners[i][2];
        out[i].v = corners[i][3];
        out[i].r = r;
        out[i].g = g;
        out[i].b = b;
        out[i].a = a;
    }
    gSprites++;
}

void fo_gfx_draw(const FOSprite *sprite, float x, float y, float w, float h, float ox, float oy, float angle,
                 int flip, FOColor tint) {
    if (!sprite || !sprite->texture || !sprite->texture->id) return;
    float u0 = sprite->u0, u1 = sprite->u1;
    if (flip) {
        u0 = sprite->u1;
        u1 = sprite->u0;
    }
    // The top of the sprite is v0, because the atlas rows go down.
    float corners[4][4] = {{0, 0, u0, sprite->v1}, {w, 0, u1, sprite->v1}, {0, h, u0, sprite->v0}, {w, h, u1, sprite->v0}};
    float c = 1, s = 0;
    if (angle != 0) {
        c = cosf(angle);
        s = sinf(angle);
    }
    for (int i = 0; i < 4; i++) {
        float dx = corners[i][0] - ox, dy = corners[i][1] - oy;
        corners[i][0] = x + ox + dx * c - dy * s;
        corners[i][1] = y + oy + dx * s + dy * c;
    }
    push(sprite->texture->id, corners, tint);
}

void fo_gfx_rect(float x, float y, float w, float h, FOColor color) {
    const float corners[4][4] = {{x, y, 0.5f, 0.5f}, {x + w, y, 0.5f, 0.5f}, {x, y + h, 0.5f, 0.5f},
                                 {x + w, y + h, 0.5f, 0.5f}};
    push(gWhite.id, corners, color);
}

void fo_gfx_line(float x1, float y1, float x2, float y2, float thickness, FOColor color) {
    float dx = x2 - x1, dy = y2 - y1;
    float length = sqrtf(dx * dx + dy * dy);
    if (length <= 0) return;
    float nx = -dy / length * thickness / 2, ny = dx / length * thickness / 2;
    const float corners[4][4] = {{x1 - nx, y1 - ny, 0.5f, 0.5f}, {x2 - nx, y2 - ny, 0.5f, 0.5f},
                                 {x1 + nx, y1 + ny, 0.5f, 0.5f}, {x2 + nx, y2 + ny, 0.5f, 0.5f}};
    push(gWhite.id, corners, color);
}

int fo_gfx_draw_calls(void) {
    return gLastDrawCalls;
}

FOTexture fo_gfx_texture(const unsigned char *rgba, int width, int height) {
    FOTexture texture = {0, width, height};
    fo_gfx_flush();
    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    gCurrentTexture = texture.id;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return texture;
}

void fo_gfx_texture_free(FOTexture *texture) {
    if (!texture || !texture->id) return;
    fo_gfx_flush();
    if (gCurrentTexture == texture->id) gCurrentTexture = 0;
    glDeleteTextures(1, &texture->id);
    texture->id = 0;
}

FOSprite fo_gfx_whole(const FOTexture *texture) {
    FOSprite sprite = {texture, 0, 0, 1, 1, texture->width, texture->height};
    return sprite;
}

FOColor fo_rgb(unsigned int hex) {
    return fo_rgba(hex, 1);
}

FOColor fo_rgba(unsigned int hex, float alpha) {
    FOColor color = {((hex >> 16) & 0xff) / 255.0f, ((hex >> 8) & 0xff) / 255.0f, (hex & 0xff) / 255.0f, alpha};
    return color;
}
