/* Draw distance, doubled (DRAW_DISTANCE_SCALE). See docs/camera.md.
 *
 * Far clip: a level's camera settings give ZFarClip (default 70, capped at
 * 600 by f_002F67FC), which goes to the top screen's camera through
 * f_0023F168(camera, near s0, far s1) and to the gameplay camera through
 * f_002E08DC(camera, near s0, far s1); the far plane is scaled in both
 * (after the cap, so the cap doubles too). The gameplay camera hands its
 * far plane on to f_0023F168 every frame; that is not scaled again.
 * Fog: the level's FogStart / FogEnd (and the 3DS's DS_FogStart /
 * DS_FogEnd) are read with the settings getter f_00241280(settings, name,
 * default) -> s0 (f_001A293C); those four are scaled as they come back,
 * so the extra distance is not fogged away.
 * Detail: each object's LOD switch distances (LOD_Distance1, 2, or the
 * defaults at 0x00376A18 +0x1C) go to f_002282B8(object, distances[2]);
 * both are scaled, so models keep their detail as far again.
 * LOG_DRAW_DISTANCE=1 logs what is scaled.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "overrides.h"

#define DRAW_DISTANCE_SCALE 2.0f

static float as_float(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static uint32_t as_bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

static int logging(void) {
    static int on = -1;
    if (on < 0) on = getenv("LOG_DRAW_DISTANCE") != NULL;
    return on;
}

static void scale_far(Context *ctx, const char *who) {
    float far = as_float(ctx->vfp[1]);
    if (logging()) fprintf(stderr, "DRAW %s near %.3f far %.1f -> %.1f\n", who, as_float(ctx->vfp[0]), far, far * DRAW_DISTANCE_SCALE);
    ctx->vfp[1] = as_bits(far * DRAW_DISTANCE_SCALE);
}

/* only the call from the level's camera setup (f_002F67FC, returning to
 * 0x002F7120): the gameplay camera passes its far plane, already scaled, on
 * to this setter each frame, and other cameras (the map's, the 2D ones)
 * keep theirs */
#define LEVEL_CAMERA_SETUP 0x002F7120u
RECOMP_OVERRIDE(0x0023F168) {
    if (ctx->r[14] == LEVEL_CAMERA_SETUP) scale_far(ctx, "top screen camera");
    CALL(RECOMP_ORIGINAL(0x0023F168));
}

RECOMP_OVERRIDE(0x002E08DC) {
    scale_far(ctx, "gameplay camera");
    CALL(RECOMP_ORIGINAL(0x002E08DC));
}

/* whether the setting asked for is one of the fog distances */
static int fog_setting(Context *ctx, uint32_t name) {
    char text[16];
    for (unsigned i = 0; i < sizeof text; i++) {
        text[i] = (char)mem_read8(ctx, name + i);
        if (!text[i]) break;
    }
    text[sizeof text - 1] = 0;
    return !strcmp(text, "FogStart") || !strcmp(text, "FogEnd") || !strcmp(text, "DS_FogStart") || !strcmp(text, "DS_FogEnd");
}

RECOMP_OVERRIDE(0x00241280) {
    uint32_t name = ctx->r[1];
    int fog = name && fog_setting(ctx, name);
    CALL(RECOMP_ORIGINAL(0x00241280));
    if (!fog) return;
    float value = as_float(ctx->vfp[0]);
    if (value <= 0.0f) return;   /* 0 and below: no fog, or a marker */
    if (logging()) fprintf(stderr, "DRAW fog %.1f -> %.1f\n", value, value * DRAW_DISTANCE_SCALE);
    ctx->vfp[0] = as_bits(value * DRAW_DISTANCE_SCALE);
}

RECOMP_OVERRIDE(0x002282B8) {
    uint32_t distances = ctx->r[1];
    if (distances) {
        for (int i = 0; i < 2; i++) {
            float d = as_float(mem_read32(ctx, distances + 4 * i));
            mem_write32(ctx, distances + 4 * i, as_bits(d * DRAW_DISTANCE_SCALE));
        }
    }
    CALL(RECOMP_ORIGINAL(0x002282B8));
    /* the caller's array is on its stack and goes when it returns */
}
