/* Right stick camera, console-LEGO style. See docs/camera.md.
 *
 * Left/right: f_00200444 runs per character each frame; for the player it
 * adds the frame's camera input into the camera object at 0x005899E0
 * (+0x140 yaw: 2 * ch8 (L) - 2 * ch10 (R) and the stylus drag's x), which
 * the camera consumes later in the frame. After the original runs, this
 * adds the stick's x there too.
 *
 * Up/down: the game's own elevation input (+0x144) raises the camera only
 * to about 22 degrees and pulls it back as it goes (15 to 21 units, which
 * reads as a zoom), so it is left alone. Instead the stick moves an
 * elevation of our own, and the view is tilted to it around the frame's
 * look-at point (0x0058D39C, the player) at the camera's distance:
 * from 5 degrees to straight down (88). The game hands the frame's camera
 * matrix to f_0022479C, which copies it to the view (0x0052AB80: right, up,
 * forward rows and the eye, row-major 4x4); this rewrites both, only when
 * the eye is the frame's camera's and within 2 units of this camera's
 * (+0x004; it trails a little while the player moves), so cutscene
 * cameras are left be.
 *
 * Zakuro leaves the stick in the shared page (memory::config::
 * HOST_RIGHT_STICK): "ZKRS", x, y as f32, -1..1, up +.
 */
#include <math.h>
#include <string.h>
#include "overrides.h"

#define PLAYER          0x00377228u /* the player character, a pointer */
#define CAMERA          0x005899E0u
#define CAMERA_YAW      (CAMERA + 0x140u)
#define CAMERA_EYE      (CAMERA + 0x004u)
#define LOOK_AT         0x0058D39Cu /* the frame's look-at, the eye at +0x24 */
#define VIEW            0x0052AB80u
#define WIPE_SHUT       (0x00519D94u + 0x485u) /* cutscene bottom screen */
#define HOST_STICK      0x1FF81F00u

/* full stick: yaw as fast as holding L or R; elevation in degrees */
#define YAW_PER_FRAME   2.0f
#define TILT_PER_FRAME  3.0f
#define LOWEST          5.0f
#define HIGHEST         88.0f
#define DEADZONE        0.15f

#define DEG (3.14159265f / 180.0f)

/* the elevation the stick has set, degrees; below 0 until first used */
static float elevation = -1.0f;
static float stick_y;

static float as_float(uint32_t bits) { float f; memcpy(&f, &bits, 4); return f; }
static uint32_t as_bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

/* outside the deadzone, rescaled so the edge of it is 0 */
static float shaped(float v) {
    float a = v < 0 ? -v : v;
    if (a <= DEADZONE) return 0.0f;
    if (a > 1.0f) a = 1.0f;
    a = (a - DEADZONE) / (1.0f - DEADZONE);
    return v < 0 ? -a : a;
}

RECOMP_OVERRIDE(0x00200444) {
    uint32_t character = ctx->r[0];
    CALL(RECOMP_ORIGINAL(0x00200444));
    if (character != mem_read32(ctx, PLAYER)) return;
    if (mem_read8(ctx, WIPE_SHUT)) return;
    if (mem_read32(ctx, HOST_STICK) != 0x53524B5Au /* "ZKRS" */) return;
    float x = shaped(as_float(mem_read32(ctx, HOST_STICK + 4)));
    float y = shaped(as_float(mem_read32(ctx, HOST_STICK + 8)));
    if (x != 0.0f) {
        /* stick right turns the view right */
        float yaw = as_float(mem_read32(ctx, CAMERA_YAW)) + x * YAW_PER_FRAME;
        mem_write32(ctx, CAMERA_YAW, as_bits(yaw));
    }
    stick_y = y;
}

static float rd(Context *ctx, uint32_t a) { return as_float(mem_read32(ctx, a)); }
static void wr(Context *ctx, uint32_t a, float v) { mem_write32(ctx, a, as_bits(v)); }

RECOMP_OVERRIDE(0x0022479C) {
    uint32_t source = ctx->r[0];
    CALL(RECOMP_ORIGINAL(0x0022479C));
    float y = stick_y;
    stick_y = 0.0f;
    if (mem_read8(ctx, WIPE_SHUT)) return;
    /* this camera's view? */
    /* (the view's eye is the frame's camera's, LOOK_AT +0x24; it trails the
       gameplay camera's own place by a little while the player moves, and
       a cutscene's camera is nowhere near it) */
    float near = 0.0f;
    for (int i = 0; i < 3; i++) {
        float eye = rd(ctx, VIEW + 0x30 + 4 * i);
        if (fabsf(eye - rd(ctx, LOOK_AT + 0x24 + 4 * i)) > 0.01f) return;
        float d = eye - rd(ctx, CAMERA_EYE + 4 * i);
        near += d * d;
    }
    if (near > 2.0f * 2.0f) return;
    float eye[3], at[3], f[3];
    for (int i = 0; i < 3; i++) {
        eye[i] = rd(ctx, VIEW + 0x30 + 4 * i);
        at[i] = rd(ctx, LOOK_AT + 4 * i);
        f[i] = rd(ctx, VIEW + 0x20 + 4 * i);
    }
    float dx = eye[0] - at[0], dy = eye[1] - at[1], dz = eye[2] - at[2];
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);
    float flat = sqrtf(f[0] * f[0] + f[2] * f[2]);
    if (distance < 0.5f || flat < 0.01f) return;
    float hx = f[0] / flat, hz = f[2] / flat;
    if (elevation < 0.0f) {
        if (y == 0.0f) return;              /* the game's own until first used */
        elevation = asinf(dy / distance) / DEG;
    }
    elevation += y * TILT_PER_FRAME;
    if (elevation < LOWEST) elevation = LOWEST;
    if (elevation > HIGHEST) elevation = HIGHEST;
    float c = cosf(elevation * DEG), s = sinf(elevation * DEG);
    float nf[3] = { hx * c, -s, hz * c };                 /* forward */
    float r[3] = { hz, 0.0f, -hx };                       /* right, level */
    float u[3] = { nf[1] * r[2] - nf[2] * r[1],           /* up = forward x right */
                   nf[2] * r[0] - nf[0] * r[2],
                   nf[0] * r[1] - nf[1] * r[0] };
    for (int i = 0; i < 3; i++) {
        wr(ctx, VIEW + 0x00 + 4 * i, r[i]);
        wr(ctx, VIEW + 0x10 + 4 * i, u[i]);
        wr(ctx, VIEW + 0x20 + 4 * i, nf[i]);
        wr(ctx, VIEW + 0x30 + 4 * i, at[i] - distance * nf[i]);
    }
    for (uint32_t o = 0; o < 0x40; o += 4) mem_write32(ctx, source + o, mem_read32(ctx, VIEW + o));
}
