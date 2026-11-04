/* Right stick camera: analog orbit (yaw) and tilt (pitch), console-LEGO
 * style, through the game's own camera inputs. See docs/camera.md.
 *
 * f_00200444 runs per character each frame; for the player it adds the
 * frame's camera input into the camera object at 0x005899E0:
 *   +0x140  yaw:   2 * ch8 (L) - 2 * ch10 (R), and the stylus drag's x
 *   +0x144  pitch: the stylus drag's y (TouchCameraSystem +0x38)
 * which the camera consumes and clears later in the same frame, applying
 * its own limits and smoothing. After the original runs, this adds the
 * right stick there too. Zakuro leaves the stick in the shared page
 * (memory::config::HOST_RIGHT_STICK): "ZKRS", x, y as f32, -1..1, up +.
 */
#include <stdlib.h>
#include <string.h>
#include "overrides.h"

#define PLAYER          0x00377228u /* the player character, a pointer */
#define CAMERA          0x005899E0u
#define CAMERA_YAW      (CAMERA + 0x140u)
#define CAMERA_PITCH    (CAMERA + 0x144u)
#define WIPE_SHUT       (0x00519D94u + 0x485u) /* cutscene bottom screen */
#define HOST_STICK      0x1FF81F00u

/* full stick: yaw as fast as holding L or R, pitch as a firm stylus drag */
#define YAW_PER_FRAME   2.0f
#define PITCH_PER_FRAME 0.08f
#define DEADZONE        0.15f

static float as_float(uint32_t bits) { float f; memcpy(&f, &bits, 4); return f; }
static uint32_t as_bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

/* outside the deadzone, rescaled so the edge of it is 0 */
static float shaped(float v) {
    float a = v < 0 ? -v : v;
    if (a <= DEADZONE) return 0.0f;
    if (a > 1.0f) a = 1.0f;
    a = (a - DEADZONE) / (1.0f - DEADZONE);
    return v < 0 ? -a : a;
}

RECOMP_OVERRIDE(0x00200444) {
    uint32_t character = ctx->r[0];
    CALL(RECOMP_ORIGINAL(0x00200444));
    if (character != mem_read32(ctx, PLAYER)) return;
    if (mem_read8(ctx, WIPE_SHUT)) return;
    if (mem_read32(ctx, HOST_STICK) != 0x53524B5Au /* "ZKRS" */) return;
    float x = shaped(as_float(mem_read32(ctx, HOST_STICK + 4)));
    float y = shaped(as_float(mem_read32(ctx, HOST_STICK + 8)));
    if (x != 0.0f) {
        /* stick right turns the view right */
        float yaw = as_float(mem_read32(ctx, CAMERA_YAW)) + x * YAW_PER_FRAME;
        mem_write32(ctx, CAMERA_YAW, as_bits(yaw));
    }
    if (y != 0.0f) {
        /* DEBUG: which input component, how strong */
        static uint32_t off = 0; static float k = 0;
        if (!off) { const char *e = getenv("CAM_OFF"); off = e ? (uint32_t)strtoul(e, 0, 16) : 0x144; e = getenv("CAM_K"); k = e ? (float)atof(e) : PITCH_PER_FRAME; }
        float pitch = as_float(mem_read32(ctx, CAMERA + off)) + y * k;
        mem_write32(ctx, CAMERA + off, as_bits(pitch));
    }
}
