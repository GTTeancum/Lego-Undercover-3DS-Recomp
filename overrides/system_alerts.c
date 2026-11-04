/* The game's alert box (system messages on the bottom screen at boot:
 * StreetPass, the HOME Button notice, autosave), told to Zakuro, which
 * answers the first two by itself and shows the autosave notice filling
 * the window. See docs/bottom-screen.md.
 *
 * f_0020ACA4(line, format, ...) sets a line of the alert's text (the
 * alert object at [0x00376C84]); the first-boot flow f_001E5868 shows the
 * StreetPass question (its call returns to 0x001E59B0) and the HOME notice
 * (0x001E5B9C), f_00319A84 the autosave notice (0x00319B54). Each call is
 * left for Zakuro at ALERT: "ALR!", the return address, and a count.
 *
 * The first boot's language picker (f_003074E4, a method of the page class
 * at 0x00363CE8; it reads the language names table at 0x003660FC,
 * which lists English, Francais, Espanol) is counted at LANGUAGE_PICKER,
 * for Zakuro to choose English (A, the first, already highlighted); a
 * language option belongs in the menus later.
 */
#include "overrides.h"

#define ALERT           0x005A4FB0u
#define LANGUAGE_PICKER 0x005A4FBCu

static uint32_t serial;

RECOMP_OVERRIDE(0x0020ACA4) {
    uint32_t from = ctx->r[14];
    CALL(RECOMP_ORIGINAL(0x0020ACA4));
    mem_write32(ctx, ALERT, 0x21524C41u);   /* "ALR!" */
    mem_write32(ctx, ALERT + 4, from);
    mem_write32(ctx, ALERT + 8, ++serial);
}

RECOMP_OVERRIDE(0x003074E4) {
    CALL(RECOMP_ORIGINAL(0x003074E4));
    mem_write32(ctx, LANGUAGE_PICKER, mem_read32(ctx, LANGUAGE_PICKER) + 1);
}
