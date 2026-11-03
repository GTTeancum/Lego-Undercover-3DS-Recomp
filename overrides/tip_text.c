/* Tutorial tips and loading-screen tips as text on the top screen, bottom
 * middle, drawn by the game's own text printer in its font (button tags as
 * button icons), in place of the bottom screen's boxes; Zakuro hides the
 * bottom screen while one is up. See docs/bottom-screen.md.
 *
 * Where: the gameplay renderer f_002E4858(this, phase) draws one screen a
 * frame (0x00529DA0 +0x3C8: 0 top, 1 bottom; it flips every frame in
 * gameplay). Its last phase sets up 2D drawing and runs the panel
 * dispatcher f_00245F9C(list) (from f_001EEBB4): list 0x00518188 holds the
 * top screen's panels, 0x00518198 the bottom's (the tip box is one of
 * those). The tip is printed just before a top-screen dispatch, in that 2D
 * state.
 *
 * When: a notice is up (0x00517E68 +0x10) that a tutorial tip showed (the
 * ShowTipTextOnce / ShowTipTextTimed calls, return addresses 0x002638D0 and
 * 0x00270068, recorded at 0x005A4F44 by screen_frame_counters.c). Its text
 * is at +0x30.
 *
 * How, as the tip box prints (f_0023C780) but without its box:
 *   f_00255244(font)                 the UI font ([0x003757A0]) made current
 *   f_001A0FF8(text, 1, 0)           text box and flags
 *   f_002551CC(font, 4, 5)           blending
 *   f_00235CB0(font, ALIGN)          alignment, 0 centred
 *   f_002551C4 / f_00229120          colour and alpha
 *   f_00254D80(x, y)                 position, s0 and s1, top-screen pixels
 *   f_002244D8(buffer)               a copy of the text with its tags
 *                                    (<l>, <r>...) turned into button glyphs
 *   f_00195408(font, 0, buffer)      draw it
 * (the printf f_00235CC4 does the last two after formatting). The 2D draw
 * alpha and layer (0x00376300, 0x003763EC), the font's settings, the
 * current font, the text position and the clip rectangles are put back
 * afterwards, so the game's own drawing is unchanged.
 */
#include <string.h>
#include "overrides.h"

#define SCREEN        (0x00529DA0u + 0x3C8u)
#define NOTICE_UP     (0x00517E68u + 0x10u)
#define NOTICE_TEXT   (0x00517E68u + 0x30u)
#define NOTICE_FROM   0x005A4F44u
#define UI_FONT       0x003757A0u            /* the tip box's font */
#define CURRENT_FONT  (0x00376618u + 0x14u)  /* f_00255234 reads, f_00255244 sets */
#define TEXT_POS      0x00376624u            /* f_00254D80 sets x, y */
#define DRAW_ALPHA    0x00376300u            /* 0x80 opaque */
#define DRAW_LAYER    0x003763ECu

/* 2D drawing, text included, is clipped to a rectangle kept at CLIP:
 * x0, y0, x1, y1, width, height, within the screen's bounds at BOUNDS
 * (x0, y0, x1, y1). Text also re-clips to the text box at TEXT_BOX
 * (x0, y0, x1, y1) unless a caller set its own clip. The tip opens both to
 * the whole screen while it prints. */
#define CLIP          (0x00529DA0u + 0xE4u)
#define BOUNDS        (0x00529DA0u + 0xFCu)
#define TEXT_BOX      0x0052EC1Cu

#define ALIGN   0                            /* centred */
#define COLOUR  0xFFFFFFFFu                  /* 0xAABBGGRR */
#define TIP_X   200.0f                       /* top screen, 400 x 240 */
#define TIP_Y   196.0f

static int printing;

static uint32_t bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }
static float as_float(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }

/* Calls a game function the way generated code does. A call can come back
 * with r15 somewhere other than the return address (an indirect jump left
 * to its caller, as the text code's helpers do); the caller then keeps
 * dispatching from r15 until the return address is reached. The time
 * slice is lifted for the call, so the thread is not handed back to the
 * host halfway through the print. */
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

#define GAME_CALL(addr, a0, a1, a2, a3) do { \
    ctx->r[0] = (a0); ctx->r[1] = (a1); ctx->r[2] = (a2); ctx->r[3] = (a3); \
    if (!game_call(ctx, f_##addr, 0x##addr##u)) return; \
} while (0)

static int tip_up(Context *ctx) {
    if (!mem_read32(ctx, NOTICE_UP) || !mem_read32(ctx, NOTICE_TEXT)) return 0;
    uint32_t from = mem_read32(ctx, NOTICE_FROM);
    return from == 0x002638D0u || from == 0x00270068u;
}

/* the game state the print changes, kept and put back */
typedef struct {
    uint32_t r[16], s0, s1, thumb;
    uint32_t current, pos[2], alpha, layer, clip[6], box[4];
    uint32_t font, fields[6];              /* the font's +0x9C..+0xB3 */
} Saved;

static void save(Context *ctx, Saved *k, uint32_t font) {
    memcpy(k->r, ctx->r, sizeof k->r);
    k->s0 = ctx->vfp[0]; k->s1 = ctx->vfp[1]; k->thumb = ctx->thumb;
    k->current = mem_read32(ctx, CURRENT_FONT);
    k->pos[0] = mem_read32(ctx, TEXT_POS); k->pos[1] = mem_read32(ctx, TEXT_POS + 4);
    k->alpha = mem_read32(ctx, DRAW_ALPHA); k->layer = mem_read32(ctx, DRAW_LAYER);
    for (int i = 0; i < 6; i++) k->clip[i] = mem_read32(ctx, CLIP + 4 * i);
    for (int i = 0; i < 4; i++) k->box[i] = mem_read32(ctx, TEXT_BOX + 4 * i);
    k->font = font;
    for (int i = 0; i < 6; i++) k->fields[i] = mem_read32(ctx, font + 0x9C + 4 * i);
}

static void restore(Context *ctx, const Saved *k) {
    for (int i = 0; i < 6; i++) mem_write32(ctx, k->font + 0x9C + 4 * i, k->fields[i]);
    for (int i = 0; i < 4; i++) mem_write32(ctx, TEXT_BOX + 4 * i, k->box[i]);
    for (int i = 0; i < 6; i++) mem_write32(ctx, CLIP + 4 * i, k->clip[i]);
    mem_write32(ctx, DRAW_ALPHA, k->alpha); mem_write32(ctx, DRAW_LAYER, k->layer);
    mem_write32(ctx, TEXT_POS, k->pos[0]); mem_write32(ctx, TEXT_POS + 4, k->pos[1]);
    mem_write32(ctx, CURRENT_FONT, k->current);
    ctx->vfp[0] = k->s0; ctx->vfp[1] = k->s1; ctx->thumb = (uint8_t)k->thumb;
    memcpy(ctx->r, k->r, sizeof k->r);
}

static void whole_screen(Context *ctx) {
    uint32_t b[4];
    for (int i = 0; i < 4; i++) b[i] = mem_read32(ctx, BOUNDS + 4 * i);
    for (int i = 0; i < 4; i++) mem_write32(ctx, CLIP + 4 * i, b[i]);
    mem_write32(ctx, CLIP + 16, bits(as_float(b[2]) - as_float(b[0])));
    mem_write32(ctx, CLIP + 20, bits(as_float(b[3]) - as_float(b[1])));
    for (int i = 0; i < 4; i++) mem_write32(ctx, TEXT_BOX + 4 * i, b[i]);
}

/* prints text (a host string) on the screen being drawn, centred at
 * (x, y), in the given font, the way the tip box prints */
static void draw(Context *ctx, uint32_t font, const char *text, float x, float y, uint32_t stack) {
    /* the text in a buffer below the stack, for its tags to become glyphs
       in place, as the printf formats into its own */
    uint32_t buffer = (stack - 0x800u) & ~7u;
    uint32_t n = 0;
    for (; n < 0x7FF && text[n]; n++) mem_write8(ctx, buffer + n, (uint8_t)text[n]);
    mem_write8(ctx, buffer + n, 0);
    ctx->r[13] = buffer - 8;
    mem_write32(ctx, DRAW_ALPHA, 0x80);
    mem_write32(ctx, DRAW_LAYER, 0x3D);
    GAME_CALL(00255244, font, 0, 0, 0);
    ctx->vfp[0] = bits(0.0f);
    GAME_CALL(001A0FF8, buffer, 1, 0, 0);
    GAME_CALL(002551CC, font, 4, 5, 0);
    GAME_CALL(00235CB0, font, (uint32_t)ALIGN, 0, 0);
    GAME_CALL(002551C4, font, COLOUR, 0, 0);
    GAME_CALL(00229120, font, 0xFF, 0, 0);
    ctx->vfp[0] = bits(x);
    ctx->vfp[1] = bits(y);
    GAME_CALL(00254D80, 0, 0, 0, 0);
    whole_screen(ctx);
    GAME_CALL(002244D8, buffer, 0, 0, 0);
    GAME_CALL(00195408, font, 0, buffer, 0);
}

/* prints text on the screen being drawn, leaving the game's state as it
 * was */
static void print_text(Context *ctx, uint32_t font, const char *text, float x, float y) {
    if (!font || !text[0]) return;
    Saved k;
    save(ctx, &k, font);
    printing = 1;
    draw(ctx, font, text, x, y, k.r[13]);
    printing = 0;
    /* back to where the caller was; a call that stopped early (it should
       not, with the time slice lifted) leaves the print short */
    ctx->exit = 0;
    restore(ctx, &k);
}

static void read_string(Context *ctx, uint32_t from, char *to, unsigned size) {
    unsigned i = 0;
    for (; i + 1 < size; i++) {
        uint8_t c = mem_read8(ctx, from + i);
        if (!c) break;
        to[i] = (char)c;
    }
    to[i] = 0;
}

RECOMP_OVERRIDE(0x00245F9C) {
    if (!printing && mem_read32(ctx, SCREEN) == 0 && tip_up(ctx)) {
        char text[0x800];
        read_string(ctx, mem_read32(ctx, NOTICE_TEXT), text, sizeof text);
        print_text(ctx, mem_read32(ctx, UI_FONT), text, TIP_X, TIP_Y);
    }
    CALL(RECOMP_ORIGINAL(0x00245F9C));
}

/* Text the bottom screen prints that belongs on the top screen: the
 * loading screen's tip (its renderer f_00303E78 prints "%s %s" of "Tip:"
 * and the tip, returning to 0x003041C0). (A cutscene video's subtitles,
 * printed returning to 0x002E18F4, cannot go this way: the video's frame
 * is copied over the top screen after everything drawn there, so Zakuro
 * moves their text as a piece instead.) screen_frame_counters.c, which wraps
 * the printf, hands each print here (top_text_seen) with the font it uses;
 * the render loop f_0025BE10(phase, modes, count), which draws every
 * active mode for one phase, prints the text last seen at the bottom middle
 * of the top screen after a top-screen frame's last phase (6). A print
 * not repeated within a few frames has gone. */
#define LOADING_PRINT   0x003041C0u
#define LAST_PHASE      6
#define BOTTOM_TEXT_Y   208.0f

typedef struct { uint32_t from, font; unsigned age; char text[0x400]; } Moved;
#define MOVED 1
static Moved moved[MOVED] = { { LOADING_PRINT, 0, 1000, "" } };

void top_text_seen(Context *ctx, uint32_t return_address, uint32_t text) {
    if (printing) return;
    for (int i = 0; i < MOVED; i++) {
        if (moved[i].from != return_address) continue;
        read_string(ctx, text, moved[i].text, sizeof moved[i].text);
        moved[i].font = mem_read32(ctx, CURRENT_FONT);
        moved[i].age = 0;
    }
}

RECOMP_OVERRIDE(0x0025BE10) {
    uint32_t phase = ctx->r[0];
    CALL(RECOMP_ORIGINAL(0x0025BE10));
    if (phase != LAST_PHASE || printing || mem_read32(ctx, SCREEN) != 0) return;
    for (int i = 0; i < MOVED; i++) {
        if (moved[i].age++ > 8) continue;
        print_text(ctx, moved[i].font, moved[i].text, TIP_X, BOTTOM_TEXT_Y);
    }
}
