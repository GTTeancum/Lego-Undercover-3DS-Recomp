/* Map mode's navigation point (see docs/bottom-screen.md, "Map mode").
 *
 * Zakuro's map mode (D-pad Up while the minimap shows) moves a cursor over
 * the bottom screen; A there asks for a navigation point: "NAV!", x, y
 * (bottom-screen pixels, f32) and a count that changes with each request,
 * at NAV_REQUEST. The game has no tap-to-waypoint, so this makes one with
 * its own map markers: f_001FCE38(&position, type, flag) (to
 * f_001F886C(drc, ...), as AddFerryMapMarker does) puts a marker
 * in one of the map's 16 slots ([DRC +0x24] +0xE0, 0x28-byte objects that
 * keep a pointer to the position, so moving the point moves the marker);
 * type 0 is WayPoint.tga in the map's icon table (0x003588A0).
 *
 * The cursor is turned into a world position by running the map's own
 * drawing backwards. The map draws a world point p at
 *     rotate(map(p) - map(centre), angle) + screen size / 2
 * (f_002DBB04), with map() = f_001EE9B8(drc, &p, &out), the angle
 * f_001EEA58(drc) and the screen size at 0x00529DA0 +0x5C/+0x60. The
 * centre is the map's dummy-location object (DRC +0x428), a fixed point
 * (DRC +0x42C set, position at DRC +0x418) or the player. map() is linear
 * across the ground, so it is measured at the player and 100 units along x
 * and z and inverted; the height is the player's.
 *
 * The marker is SubObjectivePointGreen (type 15), apart from the game's
 * own mission markers; past EDGE pixels from the map's middle the game
 * pins it at that distance with an arrow pointing on to it. A on the point
 * as shown clears it, as does arriving there (within ARRIVED units across
 * the ground).
 */
#include <math.h>
#include <string.h>
#include "overrides.h"

#define NAV_REQUEST   0x005A4F80u
#define NAV_POSITION  0x005A4FD0u   /* the point, 3 x f32, the marker's */
#define DRC           0x00519D94u
#define PLAYER        0x00377228u
#define SCREEN_SIZE   (0x00529DA0u + 0x5Cu)
#define WAYPOINT      15           /* SubObjectivePointGreen.tga */
#define ARRIVED       6.0f
#define SAME_POINT    12.0f        /* pixels */
#define EDGE          100.0f       /* farther out the map pins a point at this
                                      distance from its middle, with an arrow */

static uint32_t nav_serial, marker;
static int active;

static float as_float(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static uint32_t bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }
static float rd(Context *ctx, uint32_t a) { return as_float(mem_read32(ctx, a)); }
static void wr(Context *ctx, uint32_t a, float v) { mem_write32(ctx, a, bits(v)); }

/* as tip_text.c: calls a game function the way generated code does */
#define RETURN_ADDRESS 0x00245F9Cu
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

/* scratch below the caller's stack: buffers, and the stack for calls */
static uint32_t scratch;

/* the map's coordinates of a world point */
static int map_point(Context *ctx, const float p[3], float out[2]) {
    uint32_t in = scratch, res = scratch + 0x10;
    for (int i = 0; i < 3; i++) wr(ctx, in + 4 * i, p[i]);
    ctx->r[0] = DRC; ctx->r[1] = in; ctx->r[2] = res;
    if (!game_call(ctx, f_001EE9B8, 0x001EE9B8u)) return 0;
    out[0] = rd(ctx, res); out[1] = rd(ctx, res + 4);
    return 1;
}

static int map_angle(Context *ctx, float *angle) {
    ctx->r[0] = DRC;
    if (!game_call(ctx, f_001EEA58, 0x001EEA58u)) return 0;
    *angle = as_float(ctx->vfp[0]);
    return 1;
}

static void world_of(Context *ctx, uint32_t object, float p[3]) {
    uint32_t at = mem_read32(ctx, object + 0x3C) + 0x8C;
    for (int i = 0; i < 3; i++) p[i] = rd(ctx, at + 4 * i);
}

static void map_centre(Context *ctx, float p[3]) {
    uint32_t dummy = mem_read32(ctx, DRC + 0x428);
    if (dummy) { world_of(ctx, dummy, p); return; }
    if (mem_read32(ctx, DRC + 0x42C)) {
        for (int i = 0; i < 3; i++) p[i] = rd(ctx, DRC + 0x418 + 4 * i);
        return;
    }
    world_of(ctx, mem_read32(ctx, PLAYER), p);
}

static uint32_t slots(Context *ctx) { return mem_read32(ctx, DRC + 0x24) + 0xE0; }

static int marker_slot(Context *ctx) {
    if (!marker) return -1;
    for (int i = 0; i < 16; i++) if (mem_read32(ctx, slots(ctx) + 4 * i) == marker) return i;
    return -1;
}

static void clear(Context *ctx) {
    int i = marker_slot(ctx);
    if (i >= 0) mem_write32(ctx, slots(ctx) + 4 * i, 0);
    marker = 0;
    active = 0;
}

static void add(Context *ctx) {
    ctx->r[0] = NAV_POSITION; ctx->r[1] = WAYPOINT; ctx->r[2] = 0;
    if (!game_call(ctx, f_001FCE38, 0x001FCE38u)) return;
    int32_t i = (int32_t)ctx->r[0];
    marker = (i >= 0 && i < 16) ? mem_read32(ctx, slots(ctx) + 4 * i) : 0;
}

/* screen <-> world through the map, as it is drawn this frame */
typedef struct { float centre[2], angle, half[2], origin[3], ax[2], az[2], at0[2]; } MapView;

static int map_view(Context *ctx, MapView *v) {
    float c[3];
    map_centre(ctx, c);
    if (!map_point(ctx, c, v->centre) || !map_angle(ctx, &v->angle)) return 0;
    v->half[0] = 0.5f * rd(ctx, SCREEN_SIZE);
    v->half[1] = 0.5f * rd(ctx, SCREEN_SIZE + 4);
    world_of(ctx, mem_read32(ctx, PLAYER), v->origin);
    float px[3] = { v->origin[0] + 100.0f, v->origin[1], v->origin[2] };
    float pz[3] = { v->origin[0], v->origin[1], v->origin[2] + 100.0f };
    float mx[2], mz[2];
    if (!map_point(ctx, v->origin, v->at0) || !map_point(ctx, px, mx) || !map_point(ctx, pz, mz)) return 0;
    for (int i = 0; i < 2; i++) { v->ax[i] = (mx[i] - v->at0[i]) / 100.0f; v->az[i] = (mz[i] - v->at0[i]) / 100.0f; }
    return 1;
}

static int to_world(const MapView *v, float sx, float sy, float p[3]) {
    float x = sx - v->half[0], z = sy - v->half[1];
    float s = sinf(v->angle), c = cosf(v->angle);
    float dx = c * x + s * z, dz = -s * x + c * z;          /* rotated back */
    float m0 = v->centre[0] + dx - v->at0[0], m1 = v->centre[1] - dz - v->at0[1];
    float det = v->ax[0] * v->az[1] - v->az[0] * v->ax[1];
    if (fabsf(det) < 1e-9f) return 0;
    float u = (m0 * v->az[1] - v->az[0] * m1) / det;
    float w = (v->ax[0] * m1 - m0 * v->ax[1]) / det;
    p[0] = v->origin[0] + u; p[1] = v->origin[1]; p[2] = v->origin[2] + w;
    return 1;
}

static int to_screen(Context *ctx, const MapView *v, const float p[3], float *sx, float *sy) {
    float m[2];
    if (!map_point(ctx, p, m)) return 0;
    float dx = m[0] - v->centre[0], dz = v->centre[1] - m[1];
    float s = sinf(v->angle), c = cosf(v->angle);
    *sx = c * dx - s * dz + v->half[0];
    *sy = c * dz + s * dx + v->half[1];
    return 1;
}

static void frame(Context *ctx) {
    uint32_t player = mem_read32(ctx, PLAYER);
    if (!player || !mem_read32(ctx, DRC + 0x24)) return;
    if (mem_read32(ctx, NAV_REQUEST) == 0x2156414Eu /* "NAV!" */) {
        uint32_t n = mem_read32(ctx, NAV_REQUEST + 12);
        if (n != nav_serial) {
            nav_serial = n;
            float sx = rd(ctx, NAV_REQUEST + 4), sy = rd(ctx, NAV_REQUEST + 8);
            MapView v;
            float p[3], ox, oy, old[3];
            if (map_view(ctx, &v) && to_world(&v, sx, sy, p)) {
                for (int i = 0; i < 3; i++) old[i] = rd(ctx, NAV_POSITION + 4 * i);
                int same = 0;
                if (active && to_screen(ctx, &v, old, &ox, &oy)) {
                    float ex = ox - v.half[0], ey = oy - v.half[1], r = hypotf(ex, ey);
                    if (hypotf(ox - sx, oy - sy) < SAME_POINT) same = 1;
                    if (r > EDGE && hypotf(v.half[0] + ex * EDGE / r - sx, v.half[1] + ey * EDGE / r - sy) < SAME_POINT) same = 1;
                }
                if (same) {
                    clear(ctx);
                } else {
                    for (int i = 0; i < 3; i++) wr(ctx, NAV_POSITION + 4 * i, p[i]);
                    active = 1;
                }
            }
        }
    }
    if (!active) return;
    float me[3];
    world_of(ctx, player, me);
    if (hypotf(me[0] - rd(ctx, NAV_POSITION), me[2] - rd(ctx, NAV_POSITION + 8)) < ARRIVED) {
        clear(ctx);
        return;
    }
    /* the marker went (a new map, a level loaded): put it back */
    if (marker_slot(ctx) < 0) add(ctx);
}

/* once a frame, from map_buttons.c, while the map runs */
void nav_point_frame(Context *ctx) {
    uint32_t r[16], vfp[32], fpscr = *ctx->fpscr;
    uint8_t n = ctx->n, z = ctx->z, c = ctx->c, v = ctx->v;
    memcpy(r, ctx->r, sizeof r);
    memcpy(vfp, ctx->vfp, sizeof vfp);
    scratch = (ctx->r[13] - 0x400u) & ~0xFu;
    ctx->r[13] = scratch - 0x10u;
    frame(ctx);
    memcpy(ctx->r, r, sizeof r);
    memcpy(ctx->vfp, vfp, sizeof vfp);
    *ctx->fpscr = fpscr;
    ctx->n = n; ctx->z = z; ctx->c = c; ctx->v = v;
}
