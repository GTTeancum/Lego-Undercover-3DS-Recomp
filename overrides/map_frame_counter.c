/* Counts the frames the bottom-screen map runs, for Zakuro to know when the
 * map is showing (and draw it round as a minimap). See docs/bottom-screen.md.
 *
 * f_001EE730 is the map's per-frame function: in traced runs it ran in every
 * window where the map showed and in none where tips, the pause menu or the
 * cutscene wipe covered it. After it runs, this adds one to a counter at
 * MAP_FRAMES, in the unused end of the page that holds the end of the
 * game's .bss (which ends at 0x005A45D8). Zakuro's title_hooks reads it.
 */
#include "overrides.h"

#define MAP_FRAMES 0x005A4F00u

RECOMP_OVERRIDE(0x001EE730) {
    CALL(RECOMP_ORIGINAL(0x001EE730));
    mem_write32(ctx, MAP_FRAMES, mem_read32(ctx, MAP_FRAMES) + 1);
}
