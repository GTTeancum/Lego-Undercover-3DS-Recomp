/* The bottom screen's touch buttons from the controller, so they work
 * without reaching for the corner screen: LT (the console's ZL, which the
 * game never reads) opens the character switch, RT (ZR) scans, D-pad Left
 * asks for a hint, D-pad Right opens Red Bricks, L3 (the left stick
 * pressed in) takes the area exit or, without one, Exit (the game reads
 * the D-pad only in menus, and Down for one character's ability). While
 * Zakuro's map mode is open the buttons are hidden and none of this acts.
 * See docs/bottom-screen.md.
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
#define PAD_RIGHT    (1u << 4)
#define PAD_LEFT     (1u << 5)
#define PAD_ZL       (1u << 14)
#define PAD_ZR       (1u << 15)
#define HOST_L3      (1u << 16)
#define HOST_MAP     (1u << 31)               /* Zakuro's map mode is open */

static const struct { uint32_t button, pad; } bindings[] = {
    { DRC + 0x80,  PAD_ZL },    /* Chars: the character switch */
    { DRC + 0x28,  PAD_ZR },    /* Scan */
    { DRC + 0x130, PAD_LEFT },  /* Hint */
    { DRC + 0xD8,  PAD_RIGHT }, /* Red Bricks */
    { DRC + 0x264, HOST_L3 },   /* the area exit */
    { DRC + 0x54,  HOST_L3 },   /* Exit, when there is no area exit */
};
/* every button, hidden while map mode is open (+0x28 bit 0 hides one) */
static const uint32_t all_buttons[] = { 0x28, 0x54, 0x80, 0xAC, 0xD8, 0x104, 0x130, 0x264,
    /* the character portraits, six from +0x15C */
    0x15C, 0x188, 0x1B4, 0x1E0, 0x20C, 0x238 };
#define ALL (sizeof all_buttons / sizeof all_buttons[0])
static uint8_t hidden_before[ALL];
static int map_open;
#define BINDINGS (sizeof bindings / sizeof bindings[0])
static int held[BINDINGS];
static uint32_t serial;

static float as_float(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static uint32_t bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

static void map_mode(Context *ctx, int open) {
    if (open == map_open) return;
    map_open = open;
    for (unsigned i = 0; i < ALL; i++) {
        uint32_t flags = DRC + all_buttons[i] + 0x28;
        uint8_t v = mem_read8(ctx, flags);
        if (open) { hidden_before[i] = v & 1; mem_write8(ctx, flags, v | 1); }
        else mem_write8(ctx, flags, (uint8_t)((v & ~1u) | hidden_before[i]));
    }
}

void map_button_pressed(Context *ctx, uint32_t button) {
    if (mem_read32(ctx, HOST_STICKS) != 0x53524B5Au /* "ZKRS" */) return;
    uint32_t pad = mem_read32(ctx, HOST_STICKS + 20);
    map_mode(ctx, (pad & HOST_MAP) != 0);
    if (map_open) return;
    int area_exit = !(mem_read8(ctx, DRC + 0x264 + 0x28) & 1) && mem_read8(ctx, DRC + 0x49B);
    for (unsigned i = 0; i < BINDINGS; i++) {
        if (bindings[i].button != button) continue;
        if (button == DRC + 0x54 && area_exit) continue;   /* L3 is the area exit's */
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
