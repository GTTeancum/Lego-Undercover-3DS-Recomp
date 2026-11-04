/* The bottom screen's touch buttons from the controller (see
 * docs/bottom-screen.md, "Controller layout"):
 *   LB tapped        the character switch (disguises)   } Zakuro, through
 *   LB held          a wheel of every button shown,     } the wheel request
 *                    picked with the right stick        }
 *   RB               Scan
 *   L3               the area exit or, without one, Exit
 * On foot, where the map shows, Zakuro keeps L and R from the game (they
 * no longer turn the camera; the right stick does) and hands them over as
 * host bits. The D-pad is the game's (disguise choice, codes, whistle,
 * steering), Up excepted while the map shows: Zakuro's map mode, during
 * which the buttons are hidden and none of this acts. The triggers are
 * left free.
 *
 * The DRC manager (0x00519D94) keeps its buttons as 0x2C-byte objects:
 * +0x28 Scan, +0x54 Exit, +0x80 Chars, +0xAC Audio, +0xD8 Red Bricks,
 * +0x104 Free Play, +0x130 Hint, +0x264 the area exit (wheel indices 0-7).
 * Its map update f_002D9F84 asks f_001EE730(button, touch) of each button
 * shown whether the stylus is on it (a circle around the sprite's middle),
 * and acts on a short touch let go; screen_frame_counters.c wraps
 * f_001EE730 and calls map_button_pressed after it, for each button tested
 * (Exit, always tested, comes once a frame). A button is pressed by asking
 * Zakuro for a real tap on it: "TAP!", x, y (bottom-screen pixels, f32) and
 * a count that changes with each request, at TAP_REQUEST.
 *
 * To Zakuro, at BUTTONS_SHOWN: "BTN!" and a mask of the buttons the map
 * tested lately (by wheel index). From Zakuro, at WHEEL_REQUEST: "WHL!",
 * a wheel index and a count that changes with each choice.
 * Zakuro leaves the buttons held in the shared page (HOST_STICKS +20, HID's
 * bit order, its own above bit 16).
 */
#include <string.h>
#include "overrides.h"

#define HOST_STICKS    0x1FF81F00u
#define TAP_REQUEST    0x005A4F70u
#define BUTTONS_SHOWN  0x005A4F90u
#define WHEEL_REQUEST  0x005A4FA0u
#define DRC            0x00519D94u
#define HOST_L3        (1u << 16)
#define HOST_R         (1u << 18)              /* R held, kept from the game */
#define HOST_MAP       (1u << 31)              /* Zakuro's map mode is open */

/* the buttons by wheel index */
static const uint32_t buttons[8] = { 0x28, 0x54, 0x80, 0xAC, 0xD8, 0x104, 0x130, 0x264 };
#define SCAN 0
#define EXIT 1
#define AREA_EXIT 7

static const struct { int index; uint32_t pad; } bindings[] = {
    { SCAN, HOST_R },
    { AREA_EXIT, HOST_L3 },
    { EXIT, HOST_L3 },          /* when there is no area exit */
};
#define BINDINGS (sizeof bindings / sizeof bindings[0])
static int held[BINDINGS];

/* hidden while map mode is open (+0x28 bit 0): the buttons and the six
 * character portraits from +0x15C */
static const uint32_t hide[] = { 0x28, 0x54, 0x80, 0xAC, 0xD8, 0x104, 0x130, 0x264,
    0x15C, 0x188, 0x1B4, 0x1E0, 0x20C, 0x238 };
#define HIDE (sizeof hide / sizeof hide[0])
static uint8_t hidden_before[HIDE];
static int map_open;

static uint32_t frame, seen[8], serial, wheel_serial;

void nav_point_frame(Context *ctx);   /* nav_point.c */

static float as_float(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static uint32_t bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

static void map_mode(Context *ctx, int open) {
    if (open == map_open) return;
    map_open = open;
    for (unsigned i = 0; i < HIDE; i++) {
        uint32_t flags = DRC + hide[i] + 0x28;
        uint8_t v = mem_read8(ctx, flags);
        if (open) { hidden_before[i] = v & 1; mem_write8(ctx, flags, v | 1); }
        else mem_write8(ctx, flags, (uint8_t)((v & ~1u) | hidden_before[i]));
    }
}

/* the middle of a button's sprite (place +0xF0, size +0x54, +0x58) */
static int middle(Context *ctx, int index, float *x, float *y) {
    uint32_t sprite = mem_read32(ctx, DRC + buttons[index] + 4);
    if (!sprite) return 0;
    *x = as_float(mem_read32(ctx, sprite + 0xF0)) + 0.5f * as_float(mem_read32(ctx, sprite + 0x54));
    *y = as_float(mem_read32(ctx, sprite + 0xF4)) + 0.5f * as_float(mem_read32(ctx, sprite + 0x58));
    return 1;
}

/* tested by the map lately, not hidden, and on the screen (a button put
   away is animated off it) */
static int shown(Context *ctx, int index) {
    float x, y;
    if (frame - seen[index] > 1 || (mem_read8(ctx, DRC + buttons[index] + 0x28) & 1)) return 0;
    if (!middle(ctx, index, &x, &y)) return 0;
    return x >= 0.0f && x < 320.0f && y >= 0.0f && y < 240.0f;
}

/* asks Zakuro to tap a button, in its middle */
static void tap(Context *ctx, int index) {
    float x, y;
    if (!shown(ctx, index) || !middle(ctx, index, &x, &y)) return;
    mem_write32(ctx, TAP_REQUEST, 0x21504154u);   /* "TAP!" */
    mem_write32(ctx, TAP_REQUEST + 4, bits(x));
    mem_write32(ctx, TAP_REQUEST + 8, bits(y));
    mem_write32(ctx, TAP_REQUEST + 12, ++serial);
}

void map_button_pressed(Context *ctx, uint32_t button) {
    if (mem_read32(ctx, HOST_STICKS) != 0x53524B5Au /* "ZKRS" */) return;
    uint32_t pad = mem_read32(ctx, HOST_STICKS + 20);
    map_mode(ctx, (pad & HOST_MAP) != 0);
    int index = -1;
    for (int i = 0; i < 8; i++) if (button == DRC + buttons[i]) index = i;
    if (index < 0) return;
    if (index == EXIT) {
        /* once a frame: what is shown, for the wheel, and its choice */
        frame++;
        nav_point_frame(ctx);
        uint32_t mask = 0;
        for (int i = 0; i < 8; i++) if (shown(ctx, i)) mask |= 1u << i;
        mem_write32(ctx, BUTTONS_SHOWN, 0x214E5442u);   /* "BTN!" */
        mem_write32(ctx, BUTTONS_SHOWN + 4, map_open ? 0 : mask);
        if (mem_read32(ctx, WHEEL_REQUEST) == 0x214C4857u /* "WHL!" */) {
            uint32_t n = mem_read32(ctx, WHEEL_REQUEST + 8);
            if (n != wheel_serial) {
                wheel_serial = n;
                uint32_t choice = mem_read32(ctx, WHEEL_REQUEST + 4);
                if (choice < 8 && !map_open) tap(ctx, (int)choice);
            }
        }
    }
    seen[index] = frame;
    if (map_open) return;
    int area_exit = shown(ctx, AREA_EXIT) && mem_read8(ctx, DRC + 0x49B);
    for (unsigned i = 0; i < BINDINGS; i++) {
        if (bindings[i].index != index) continue;
        if (index == EXIT && area_exit) continue;   /* L3 is the area exit's */
        int down = (pad & bindings[i].pad) != 0;
        int pressed = down && !held[i];
        held[i] = down;
        if (pressed) tap(ctx, index);
    }
}
