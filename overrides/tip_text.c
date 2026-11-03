/* Tutorial tips as text on the top screen, drawn by the game's own text
 * printer, in place of the bottom screen's tip box (Zakuro hides the bottom
 * screen while one is up). See docs/bottom-screen.md.
 *
 * f_0023C780 draws text of the top-screen HUD every game frame. After it,
 * while a notice is up (0x00517E68 +0x10) that a tutorial tip showed (the
 * ShowTipTextOnce / ShowTipTextTimed calls, return addresses 0x002638D0 and
 * 0x00270068, recorded at 0x005A4F44 by screen_frame_counters.c), this
 * prints the notice's text (+0x30) at the bottom middle, the way the HUD
 * prints its own text (f_00286A54):
 *   f_00220674(12)                   text style
 *   f_00255244()
 *   f_00235CB0(font, ALIGN)          alignment
 *   f_002551F8(font, 0, 1, 0, 0, 0)  scale and flags
 *   f_00229120(font, 0xFF)           alpha
 *   f_002551C4(font, COLOUR)         colour, 0xAABBGGRR
 *   f_00254D80(x, y)                 position, s0 and s1, top-screen pixels
 *   f_00235CC4("%s", text)           print
 * where font is f_00255234().
 */
#include <string.h>
#include "overrides.h"

#define NOTICE_UP     (0x00517E68u + 0x10u)
#define NOTICE_TEXT   (0x00517E68u + 0x30u)
#define NOTICE_FROM   0x005A4F44u
#define FORMAT_S      0x0017C410u          /* "%s" in the game's data */

#define ALIGN   0                          /* 0 centred, as the HUD's -1 is left */
#define COLOUR  0xFFFFFFFFu
#define TIP_X   200.0f
#define TIP_Y   206.0f

static int printing;

static uint32_t bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

/* calls a game function with up to four arguments and two more on the stack */
#define GAME_CALL(fn, a0, a1, a2, a3) do { \
    ctx->r[0] = (a0); ctx->r[1] = (a1); ctx->r[2] = (a2); ctx->r[3] = (a3); \
    ctx->r[14] = 0x0023C780u; \
    CALL(fn); \
} while (0)

static uint32_t font(Context *ctx) {
    ctx->r[14] = 0x0023C780u;
    f_00255234(ctx);
    return ctx->r[0];
}

static int tip_up(Context *ctx) {
    if (!mem_read32(ctx, NOTICE_UP)) return 0;
    uint32_t from = mem_read32(ctx, NOTICE_FROM);
    return from == 0x002638D0u || from == 0x00270068u;
}

#define DEBUG_CALLS  0x005A4F48u
#define DEBUG_PRINTS 0x005A4F4Cu

static void print_tip(Context *ctx);

/* NOT WORKING YET: the prints run (DEBUG counters) but nothing shows on
 * either screen, and printing before the drawer disturbed the game's own
 * notice. Kept disabled until the text system's batching and screen
 * selection are understood; Zakuro hides the bottom screen for tips. */
#define TIP_TEXT_ENABLED 0

RECOMP_OVERRIDE(0x0023C780) {
    CALL(RECOMP_ORIGINAL(0x0023C780));
    mem_write32(ctx, DEBUG_CALLS, mem_read32(ctx, DEBUG_CALLS) + 1);
    if (TIP_TEXT_ENABLED && !printing && tip_up(ctx)) print_tip(ctx);
}

static void print_tip(Context *ctx) {
    mem_write32(ctx, DEBUG_PRINTS, mem_read32(ctx, DEBUG_PRINTS) + 1);
    uint32_t text = mem_read32(ctx, NOTICE_TEXT);
    if (!text) return;
    /* the caller's registers and stack, as they were */
    uint32_t saved[16];
    memcpy(saved, ctx->r, sizeof saved);
    uint32_t s0 = ctx->vfp[0], s1 = ctx->vfp[1];
    printing = 1;

    GAME_CALL(f_00220674, 12, 0, 0, 0);
    GAME_CALL(f_00255244, 0, 0, 0, 0);
    uint32_t f = font(ctx);
    GAME_CALL(f_00235CB0, f, (uint32_t)ALIGN, 0, 0);
    ctx->r[13] -= 8;
    mem_write32(ctx, ctx->r[13], 0);
    mem_write32(ctx, ctx->r[13] + 4, 0);
    GAME_CALL(f_002551F8, f, 0, 1, 0);
    ctx->r[13] += 8;
    GAME_CALL(f_00229120, f, 0xFF, 0, 0);
    GAME_CALL(f_002551C4, f, COLOUR, 0, 0);
    ctx->vfp[0] = bits(TIP_X);
    ctx->vfp[1] = bits(TIP_Y);
    GAME_CALL(f_00254D80, 0, 0, 0, 0);
    GAME_CALL(f_00235CC4, FORMAT_S, text, 0, 0);

    printing = 0;
    ctx->vfp[0] = s0;
    ctx->vfp[1] = s1;
    memcpy(ctx->r, saved, sizeof saved);
}
