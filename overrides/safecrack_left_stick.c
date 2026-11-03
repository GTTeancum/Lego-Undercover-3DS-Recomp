/* Safe cracking with the left stick: point the stick and the dial turns
 * there, the way the stylus turns it. See docs/bottom-screen.md.
 *
 * The safe crack update f_002FD52C (class vtable 0x003633F8, +0x48) reads
 * the stylus with f_0025255C, takes the vector from the dial's centre to
 * it, and if its length is between the tuning's inner and outer radius
 * turns it into a dial angle, atan2(x, -y) * 65536 / 2pi, 0 straight up,
 * kept at +0xFA of its state ([this+0x24]); within about 22 degrees of the
 * knob's angle (+0xF8) it grabs the knob, and the knob then follows.
 *
 * While the left stick is pushed past STICK_ON and the stylus is up, this
 * gives that one call a stylus point on the dial: first on the knob, so it
 * grabs, then turning toward the stick's direction at up to STEP a frame.
 * Zakuro leaves the sticks in the shared page (HOST_STICKS): "ZKRS", right
 * x, y, left x, y as f32, -1..1, up positive.
 */
#include <math.h>
#include <string.h>
#include "overrides.h"

#define HOST_STICKS   0x1FF81F00u
#define TOUCH_TABLE   0x0052A448u        /* entry 0; +0x1D phase, 0 = up */
#define TUNING        0x003758B8u        /* +0 centre x, +4 centre y (fractions), +0x3C outer, +0x40 inner radius */
#define ANGLE_UNITS   10430.378f         /* 65536 / 2pi */
#define STICK_ON      0.5f
#define STEP          0.14f              /* radians a frame, about 8 degrees */

static int virtual_touch;
static float touch_x, touch_y;
static int turning;
static float dial;                        /* radians, 0 up, clockwise */

static float as_float(uint32_t bits) { float f; memcpy(&f, &bits, 4); return f; }
static uint32_t as_bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

static float wrap(float a) {
    while (a > (float)M_PI) a -= 2.0f * (float)M_PI;
    while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}

RECOMP_OVERRIDE(0x002FD52C) {
    uint32_t self = ctx->r[0];
    virtual_touch = 0;
    if (mem_read32(ctx, HOST_STICKS) == 0x53524B5Au /* "ZKRS" */ && !mem_read8(ctx, TOUCH_TABLE + 0x1D)) {
        float x = as_float(mem_read32(ctx, HOST_STICKS + 12));
        float y = as_float(mem_read32(ctx, HOST_STICKS + 16));
        uint32_t state = mem_read32(ctx, self + 0x24);
        if (state && x * x + y * y > STICK_ON * STICK_ON) {
            float target = atan2f(x, y);
            if (!turning) {
                /* start on the knob, so the game grabs it */
                dial = (float)mem_read16(ctx, state + 0xF8) / ANGLE_UNITS;
                turning = 1;
            } else {
                float d = wrap(target - dial);
                if (d > STEP) d = STEP;
                if (d < -STEP) d = -STEP;
                dial = wrap(dial + d);
            }
            float cx = 320.0f * as_float(mem_read32(ctx, TUNING + 0x00));
            float cy = 240.0f * as_float(mem_read32(ctx, TUNING + 0x04));
            float r = 0.5f * (as_float(mem_read32(ctx, TUNING + 0x3C)) + as_float(mem_read32(ctx, TUNING + 0x40)));
            touch_x = cx + r * sinf(dial);
            touch_y = cy - r * cosf(dial);
            virtual_touch = 1;
        } else {
            turning = 0;
        }
    } else {
        turning = 0;
    }
    CALL(RECOMP_ORIGINAL(0x002FD52C));
    virtual_touch = 0;
}

/* f_0025255C(out, entry) copies a touch entry; during the call above it
 * hands back the stick's point instead, held. */
RECOMP_OVERRIDE(0x0025255C) {
    uint32_t out = ctx->r[0];
    CALL(RECOMP_ORIGINAL(0x0025255C));
    if (!virtual_touch) return;
    mem_write32(ctx, out + 0x00, as_bits(touch_x));
    mem_write32(ctx, out + 0x04, as_bits(touch_y));
    mem_write32(ctx, out + 0x08, as_bits(touch_x));
    mem_write32(ctx, out + 0x0C, as_bits(touch_y));
    mem_write8(ctx, out + 0x1D, 2);
}
