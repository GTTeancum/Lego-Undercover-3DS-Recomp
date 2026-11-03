/* The bottom screen's touch buttons from the controller, so they work
 * without reaching for the corner screen: LT (the console's ZL, which the
 * game never reads) opens the character switch, RT (ZR) scans. See
 * docs/bottom-screen.md.
 *
 * The DRC manager (0x00519D94) keeps its buttons as 0x2C-byte objects:
 * +0x28 Scan, +0x54 Exit, +0x80 Chars, +0xAC Audio, +0xD8 Red Bricks,
 * +0x104 Free Play, +0x130 Hint, +0x264 the area exit. Its map update
 * f_002D9F84 asks f_001EE730(button, touch) of each button shown whether
 * the stylus is on it (a circle around the sprite's middle), and acts on
 * a short touch let go. So a press of a bound controller button asks
 * Zakuro for a real tap on the button: "TAP!", x, y (bottom-screen pixels,
 * f32) and a count that changes with each request, at TAP_REQUEST
 * (Zakuro's title_hooks::tap_input holds the stylus there a few frames).
 * screen_frame_counters.c wraps f_001EE730 and calls map_button_pressed
 * after it, for each button the map tests.
 *
 * Zakuro leaves the buttons held in the shared page (HOST_STICKS +20, in
 * HID's bit order).
 */
#include <string.h>
#include "overrides.h"

#define HOST_STICKS  0x1FF81F00u
#define TAP_REQUEST  0x005A4F70u
#define DRC          0x00519D94u
#define PAD_ZL       (1u << 14)
#define PAD_ZR       (1u << 15)

static const struct { uint32_t button, pad; } bindings[] = {
    { DRC + 0x80, PAD_ZL },   /* Chars: the character switch */
    { DRC + 0x28, PAD_ZR },   /* Scan */
};
#define BINDINGS (sizeof bindings / sizeof bindings[0])
static int held[BINDINGS];
static uint32_t serial;

static float as_float(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static uint32_t bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

void map_button_pressed(Context *ctx, uint32_t button) {
    if (mem_read32(ctx, HOST_STICKS) != 0x53524B5Au /* "ZKRS" */) return;
    uint32_t pad = mem_read32(ctx, HOST_STICKS + 20);
    for (unsigned i = 0; i < BINDINGS; i++) {
        if (bindings[i].button != button) continue;
        int down = (pad & bindings[i].pad) != 0;
        int pressed = down && !held[i];
        held[i] = down;
        if (!pressed || (mem_read8(ctx, button + 0x28) & 1)) continue;   /* hidden */
        uint32_t sprite = mem_read32(ctx, button + 4);
        if (!sprite) continue;
        /* the sprite's place (+0xF0) and size (+0x54, +0x58): its middle */
        float x = as_float(mem_read32(ctx, sprite + 0xF0)) + 0.5f * as_float(mem_read32(ctx, sprite + 0x54));
        float y = as_float(mem_read32(ctx, sprite + 0xF4)) + 0.5f * as_float(mem_read32(ctx, sprite + 0x58));
        mem_write32(ctx, TAP_REQUEST, 0x21504154u);   /* "TAP!" */
        mem_write32(ctx, TAP_REQUEST + 4, bits(x));
        mem_write32(ctx, TAP_REQUEST + 8, bits(y));
        mem_write32(ctx, TAP_REQUEST + 12, ++serial);
    }
}
