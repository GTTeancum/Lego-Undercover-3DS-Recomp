/* Counts the frames certain screens run, for Zakuro to know what the bottom
 * screen shows (crates/zakuro/src/title_hooks.rs). See docs/bottom-screen.md.
 *
 * Each override runs the game's function, then adds one to its counter in
 * the unused end of the page that holds the end of the game's .bss (which
 * ends at 0x005A45D8):
 *   0x005A4F00  f_001EE730  the map's per-frame function
 *   0x005A4F04  f_0014FFBC  video playback (cutscene .moflex), every frame
 *                           a video plays, never otherwise
 *   0x005A4F08  f_00303E78  the loading screen (spinning badge, tip text)
 *   0x005A4F0C  f_00235CC4  text drawing; during a video only subtitles draw
 *   0x005A4F10  f_002C5988  the save slot screen
 *   0x005A4F14  f_001DC234  the mission name card (on the wipe; it also
 *                           runs on the loading screen)
 *   0x005A4F18  f_002D8768  the shop mode's renderer (red bricks, extras; class
 *                           vtable 0x00361CC0, it draws UI_Shop_Topscreen)
 * and at 0x005A4F40 whether the notice shown last (f_00235F30) came from a
 * tutorial tip (the ShowTipTextOnce / ShowTipTextTimed script commands,
 * f_00263850 and f_0026FFEC), 1, or from anything else, 0.
 */
#include "overrides.h"

#define COUNTERS 0x005A4F00u

static void count(Context *ctx, uint32_t slot) {
    uint32_t at = COUNTERS + 4 * slot;
    mem_write32(ctx, at, mem_read32(ctx, at) + 1);
}

void map_button_pressed(Context *ctx, uint32_t button);  /* map_buttons.c */

RECOMP_OVERRIDE(0x001EE730) {
    uint32_t button = ctx->r[0];
    CALL(RECOMP_ORIGINAL(0x001EE730));
    map_button_pressed(ctx, button);
    count(ctx, 0);
}

RECOMP_OVERRIDE(0x0014FFBC) {
    CALL(RECOMP_ORIGINAL(0x0014FFBC));
    count(ctx, 1);
}

void top_text_seen(Context *ctx, uint32_t return_address, uint32_t text);  /* tip_text.c */

RECOMP_OVERRIDE(0x00303E78) {
    CALL(RECOMP_ORIGINAL(0x00303E78));
    count(ctx, 2);
}

RECOMP_OVERRIDE(0x00235CC4) {
    top_text_seen(ctx, ctx->r[14], ctx->r[0]);
    CALL(RECOMP_ORIGINAL(0x00235CC4));
    count(ctx, 3);
}

void top_text_save_slots(void);  /* tip_text.c */

RECOMP_OVERRIDE(0x002C5988) {
    CALL(RECOMP_ORIGINAL(0x002C5988));
    count(ctx, 4);
    top_text_save_slots();
}

RECOMP_OVERRIDE(0x001DC234) {
    CALL(RECOMP_ORIGINAL(0x001DC234));
    count(ctx, 5);
}

RECOMP_OVERRIDE(0x002D8768) {
    CALL(RECOMP_ORIGINAL(0x002D8768));
    count(ctx, 6);
}

#define NOTICE_IS_TIP 0x005A4F40u
static int in_tip;

RECOMP_OVERRIDE(0x00263850) {
    in_tip = 1;
    CALL(RECOMP_ORIGINAL(0x00263850));
    in_tip = 0;
}

RECOMP_OVERRIDE(0x0026FFEC) {
    in_tip = 1;
    CALL(RECOMP_ORIGINAL(0x0026FFEC));
    in_tip = 0;
}

/* and at 0x005A4F44 the return address of the call that showed it, to tell
 * where a notice comes from */
RECOMP_OVERRIDE(0x00235F30) {
    int tip = in_tip;
    uint32_t from = ctx->r[14];
    CALL(RECOMP_ORIGINAL(0x00235F30));
    mem_write32(ctx, NOTICE_IS_TIP, (uint32_t)tip);
    mem_write32(ctx, NOTICE_IS_TIP + 4, from);
}
