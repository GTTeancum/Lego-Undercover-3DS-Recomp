/* Tutorial tips as text on the top screen, drawn by the game's own text
 * printer, in place of the bottom screen's tip box (Zakuro hides the bottom
 * screen while one is up). See docs/bottom-screen.md.
 *
 * After the top screen's HUD is drawn (see below), while a notice is up (0x00517E68 +0x10) that a tutorial tip showed (the
 * ShowTipTextOnce / ShowTipTextTimed calls, return addresses 0x002638D0 and
 * 0x00270068, recorded at 0x005A4F44 by screen_frame_counters.c), this
 * prints the notice's text (+0x30) at the bottom middle, the way the HUD
 * prints its own text (f_00286A54):
 *   font = f_00220674(12)            a font by size
 *   f_00255244(font)                 make it the current font
 *   f_00235CB0(font, ALIGN)          alignment
 *   f_002551F8(font, 0, 1, 0, 0, 0)  scale and flags
 *   f_00229120(font, 0xFF)           alpha
 *   f_002551C4(font, COLOUR)         colour, 0xAABBGGRR
 *   f_00254D80(x, y)                 position, s0 and s1, top-screen pixels
 *   f_00235CC4("%s", text)           print
 * The font's fields (+0x9C colour and alpha, +0xAA..+0xB0 alignment,
 * scale, flags), the current font and the text position are put back
 * afterwards, so the game's own text is unchanged.
 */
#include <string.h>
#include "overrides.h"

#define NOTICE_UP     (0x00517E68u + 0x10u)
#define NOTICE_TEXT   (0x00517E68u + 0x30u)
#define NOTICE_FROM   0x005A4F44u
#define FORMAT_S      0x0017C410u          /* "%s" in the game's data */
#define CURRENT_FONT  (0x00376618u + 0x14u)  /* f_00255234 reads, f_00255244 sets */
#define TEXT_POS      0x00376624u            /* f_00254D80 sets x, y */
#define FONT_SIZE     12                     /* f_00220674 picks a font by size */

#define ALIGN   0                          /* 0 centred, as the HUD's -1 is left */
#define COLOUR  0xFFFFFFFFu
#define TIP_X   200.0f
#define TIP_Y   206.0f

static int printing;

static uint32_t bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

/* calls a game function with up to four arguments and two more on the stack */
#define GAME_CALL(fn, a0, a1, a2, a3) do { \
    ctx->r[0] = (a0); ctx->r[1] = (a1); ctx->r[2] = (a2); ctx->r[3] = (a3); \
    ctx->r[14] = 0x002E4858u; \
    CALL(fn); \
} while (0)

static uint32_t font(Context *ctx) {
    ctx->r[14] = 0x002E4858u;
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

/* The gameplay renderer f_002E4858(this, phase) draws one screen a frame
 * (0x00529DA0 +0x3C8: 0 top, 1 bottom; it flips every frame in gameplay).
 * Phase 6 of a top frame draws the HUD (f_001D9614), so the tip is printed
 * right after it. */
#define SCREEN    (0x00529DA0u + 0x3C8u)
#define HUD_PHASE 6

RECOMP_OVERRIDE(0x002E4858) {
    uint32_t phase = ctx->r[1];
    CALL(RECOMP_ORIGINAL(0x002E4858));
    if (phase != HUD_PHASE || mem_read32(ctx, SCREEN) != 0) return;
    mem_write32(ctx, DEBUG_CALLS, mem_read32(ctx, DEBUG_CALLS) + 1);
    if (!printing && tip_up(ctx)) print_tip(ctx);
}

static void print_tip(Context *ctx) {
    mem_write32(ctx, DEBUG_PRINTS, mem_read32(ctx, DEBUG_PRINTS) + 1);
    uint32_t text = mem_read32(ctx, NOTICE_TEXT);
    if (!text) return;
    /* the caller's registers, the game's current font and text position,
       as they were */
    uint32_t saved[16];
    memcpy(saved, ctx->r, sizeof saved);
    uint32_t s0 = ctx->vfp[0], s1 = ctx->vfp[1];
    uint32_t current = mem_read32(ctx, CURRENT_FONT);
    uint32_t pos_x = mem_read32(ctx, TEXT_POS), pos_y = mem_read32(ctx, TEXT_POS + 4);
    printing = 1;

    GAME_CALL(f_00220674, FONT_SIZE, 0, 0, 0);      /* a font by size */
    uint32_t f = ctx->r[0];
    if (f) {
        uint32_t fields[6];                          /* +0x9C..+0xB3 */
        for (int i = 0; i < 6; i++) fields[i] = mem_read32(ctx, f + 0x9C + 4 * i);
        GAME_CALL(f_00255244, f, 0, 0, 0);           /* make it current */
        GAME_CALL(f_00235CB0, f, (uint32_t)ALIGN, 0, 0);
        ctx->r[13] -= 8;
        mem_write32(ctx, ctx->r[13], 0);
        mem_write32(ctx, ctx->r[13] + 4, 0);
        GAME_CALL(f_002551F8, f, 0, 1, 0);
        ctx->r[13] += 8;
        GAME_CALL(f_002551C4, f, COLOUR, 0, 0);
        GAME_CALL(f_00229120, f, 0xFF, 0, 0);
        ctx->vfp[0] = bits(TIP_X);
        ctx->vfp[1] = bits(TIP_Y);
        GAME_CALL(f_00254D80, 0, 0, 0, 0);
        GAME_CALL(f_00235CC4, FORMAT_S, text, 0, 0);
        for (int i = 0; i < 6; i++) mem_write32(ctx, f + 0x9C + 4 * i, fields[i]);
    }
    mem_write32(ctx, CURRENT_FONT, current);
    mem_write32(ctx, TEXT_POS, pos_x);
    mem_write32(ctx, TEXT_POS + 4, pos_y);

    printing = 0;
    ctx->vfp[0] = s0;
    ctx->vfp[1] = s1;
    memcpy(ctx->r, saved, sizeof saved);
}
