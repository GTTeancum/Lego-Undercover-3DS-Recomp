/* Debug: opens a shop for testing its screens, when LEGO_OPEN_SHOP=<type>
 * is set (0 hub/extras shop, 4 red brick shop; the script commands
 * HubShop / RedBrickShop call f_002133C4(type, 0)). It opens once,
 * LEGO_OPEN_SHOP_AFTER (default 900) frames of the touch camera's update
 * (f_002F77D4, gameplay only) after the first. Silent otherwise. */
#include <stdio.h>
#include <stdlib.h>
#include "overrides.h"

#define RETURN_ADDRESS 0x002F77D4u

static int game_call(Context *ctx, void (*fn)(Context *), uint32_t addr) {
    int32_t budget = ctx->budget;
    ctx->budget = 1 << 28;
    ctx->r[14] = RETURN_ADDRESS;
    ctx->r[15] = addr;
    ctx->depth++; fn(ctx); ctx->depth--;
    while (!ctx->exit && (ctx->r[15] != RETURN_ADDRESS || ctx->thumb)) {
        ctx->depth++; recomp_call(ctx); ctx->depth--;
    }
    ctx->budget = budget;
    return !ctx->exit;
}

RECOMP_OVERRIDE(0x002F77D4) {
    CALL(RECOMP_ORIGINAL(0x002F77D4));
    static int type = -2, after, frames;
    if (type == -2) {
        const char *t = getenv("LEGO_OPEN_SHOP"), *a = getenv("LEGO_OPEN_SHOP_AFTER");
        type = t ? atoi(t) : -1;
        after = a ? atoi(a) : 900;
    }
    if (type < 0 || ++frames != after) return;
    uint32_t saved[16];
    for (int i = 0; i < 16; i++) saved[i] = ctx->r[i];
    ctx->r[0] = (uint32_t)type; ctx->r[1] = 0;
    int ok = game_call(ctx, f_002133C4, 0x002133C4u);
    fprintf(stderr, "debug_open_shop: opened shop %d (%s)\n", type, ok ? "ok" : "stopped early");
    ctx->exit = 0;
    for (int i = 0; i < 16; i++) ctx->r[i] = saved[i];
}
