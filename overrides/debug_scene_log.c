/* Logs every UI scene the game loads, for mapping bottom-screen states
 * (docs/bottom-screen.md). Silent unless LEGO_SCENE_LOG is set in the
 * environment; then each load prints
 *   scene <object address> <owner r1> <path>
 * to stderr. f_00217430(path, owner) loads a ".bxaml" blend scene and
 * returns the scene object.
 */
#include <stdio.h>
#include <stdlib.h>
#include "overrides.h"

static int scene_log_enabled(void) {
    static int state = -1;
    if (state < 0) state = getenv("LEGO_SCENE_LOG") != NULL;
    return state;
}

RECOMP_OVERRIDE(0x00217430) {
    uint32_t path = ctx->r[0];
    uint32_t owner = ctx->r[1];
    CALL(RECOMP_ORIGINAL(0x00217430));
    if (!scene_log_enabled()) return;
    char name[128];
    unsigned i = 0;
    for (; i < sizeof name - 1; i++) {
        uint8_t c = mem_read8(ctx, path + i);
        if (!c) break;
        name[i] = (char)c;
    }
    name[i] = 0;
    fprintf(stderr, "scene %08X %08X %s\n", ctx->r[0], owner, name);
}
