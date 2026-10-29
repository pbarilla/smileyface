/* SPDX-License-Identifier: GPL-3.0-only */
/* Smileyface: an acid bass line for the FM-1. One voice, 16 steps, knobs to twist.
 *
 *   keys             write a note on the cursor's step and move on (the note sounds); ENV or SEL held: an accent, a slide
 *   ENV  SEL  EDIT   accent on the cursor's step, slide on it, a rest (and move on)
 *   SELECT           the cursor along the steps          PLAY   start / stop
 *   KNOB 1..4        the sound, four pages: CUT RES ENV DEC / ACC SLD DRV WAV / ADC TRK ATK MUF / TUN SUB SWP POL
 *   ALGORITHM        the knobs' page                     PRESETS   tempo
 *   SEQ              tap tempo
 *   LFO              a random pattern                    ARP   nudge a few steps          REC   undo (one level)          OCT- OCT+   octave
 *   GEN              KNOBS = HIT TRN RNG SPC (hits spread over the 16 steps, turn, how far the melody wanders, accents and slides); SELECT rolls the dice
 *   HOME             tap: steps -> sound -> fly          hold: the 12 patterns (HOME opens, hold SEL copies one to a free slot, hold EDIT starts one over, GLO or OCT- leaves)
 *   SAVE             save the pattern (it also saves 2 s after an edit, when stopped)      MASTER   volume
 *   FLY              SELECT steers the ship up and down the scale, every key fires: the shot is a note at the ship's height,
 *                    a hit on a foe is an accent, shots on neighbouring steps that change pitch slide
 *   USB-MIDI         clock, start, stop and continue are followed (the tempo turns cyan); notes play the voice (velocity 100 and up = accent); CC 102 play / stop, 103 tempo / 2, 104 save, 105 random pattern,
 *                    106 open pattern 0..11, 74 75 71 72 = CUT ENV RES DEC. The device sends CC 100 = CPU % (worst block since boot),
 *                    CC 101 = view | 16 unsaved | 32 playing | 64 picker, CC 107 = the open pattern
 *
 * Integers only: no float, no 64-bit division. tests/host.c runs this file on a computer. */
#include "platform.c"
#ifdef SMILEY_HOST
#define FM1_TICKS_PER_US 24u
static uint32_t fm1_ticks(void) { return 0; }
#endif
#include "voice.c"
#include "bank.c"

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = fm1_ticks(), dt;
    voice_render(out, n);
    dt = (uint32_t)(fm1_ticks() - t0);
    if (dt > cpu_max) cpu_max = dt;
}

/* ---- the look: acid house. A yellow smiley, black, and the colours of a rave flyer */
#define C_Y RGB(255, 238, 0)
#define C_K RGB(0, 0, 0)
#define C_LIME RGB(160, 255, 0)
#define C_PINK RGB(255, 36, 170)
#define C_CY RGB(0, 226, 255)
#define C_OR RGB(255, 120, 0)
#define C_CHK RGB(22, 24, 0)                    /* the dark squares of the floor */
#define C_RING RGB(70, 66, 0)

enum { V_STEPS, V_SOUND, V_FLY, V_GEN, VIEWS };
#define Y_HDR 44u
#define Y_LOW 176u                              /* the knobs and the tabs start here */

static int32_t isqrt(int32_t n)
{
    int32_t r = 0;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}
static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t w = isqrt(r * r - dy * dy);
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}
static void ellipse(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c)
{
    int32_t dy;
    for (dy = -ry; dy <= ry; dy++) {
        int32_t w = isqrt(rx * rx * (ry * ry - dy * dy) / (ry * ry));
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}
static int32_t cosa(uint32_t a) { return SIN64[(a + 16u) & 63u]; }
static int32_t sina(uint32_t a) { return SIN64[a & 63u]; }

/* the smiley: eyes slide with the beat, the mouth grins wider on an accent, and it melts a little when the sound opens */
static void smiley(int32_t cx, int32_t cy, int32_t r, int32_t melt, uint32_t ph, int32_t grin, int32_t look)
{
    int32_t j, a;
    for (j = 0; j < 3; j++) {                                   /* drips */
        int32_t len = (sina(ph + (uint32_t)j * 21u) + 128) * melt / 256;
        int32_t x = cx - r / 2 + j * (r / 2);
        int32_t dw = r / 8 + 3;                                 /* the drip is thicker on a bigger face */
        if (len > 1) { cv_rect(x - dw / 2, cy + r - 3, dw, len + 3, C_Y); disc(x, cy + r + len - 1, dw / 2 + 1, C_Y); }
    }
    disc(cx, cy, r + 2, C_K);
    disc(cx, cy, r, C_Y);
    if (r < 14) {                                               /* small: eyes as bars, the mouth as a few dots */
        int32_t ey = cy - r / 3 - 1;
        cv_rect(cx - r / 3 - 1 + look, ey, 2, r / 2 + 1, C_K);
        cv_rect(cx + r / 3 - 1 + look, ey, 2, r / 2 + 1, C_K);
        cv_rect(cx - r / 2, cy + r / 6, 2, 2, C_K);
        cv_rect(cx - r / 4 - 1, cy + r / 2 - 1 + grin / 4, 2, 2, C_K);
        cv_rect(cx - 1, cy + r / 2 + grin / 4, 3, 2, C_K);
        cv_rect(cx + r / 4, cy + r / 2 - 1 + grin / 4, 2, 2, C_K);
        cv_rect(cx + r / 2 - 1, cy + r / 6, 2, 2, C_K);
        return;
    }
    ellipse(cx - r / 3 + look, cy - r / 4, r / 7 + 1, r / 3, C_K);
    ellipse(cx + r / 3 + look, cy - r / 4, r / 7 + 1, r / 3, C_K);
    for (a = 4; a <= 28; a++) {                                 /* the mouth: an arc, the corners curled up */
        int32_t px = cx + cosa((uint32_t)a + 0u) * (r * 3 / 5 + grin) / 127;
        int32_t py = cy + r / 8 + sina((uint32_t)a) * (r / 2 + grin / 2) / 127 - (a < 8 || a > 24 ? 2 : 0);
        cv_rect(px - r / 24 - 1, py - r / 24 - 1, r / 12 + 3, r / 12 + 3, C_K);
    }
}

static void utoa3(char *s, uint32_t v)
{
    uint32_t i = 0;
    if (v >= 100u) s[i++] = (char)('0' + v / 100u % 10u);
    if (v >= 10u) s[i++] = (char)('0' + v / 10u % 10u);
    s[i++] = (char)('0' + v % 10u);
    s[i] = 0;
}
static void note_name(char *s, uint32_t note)
{
    static const char *const NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const char *p;
    uint32_t i = 0;
    s[0] = 0;
    if (!note || note >= 128u) return;
    for (p = NAME[note % 12u]; *p; p++) s[i++] = *p;
    s[i++] = (char)('0' + note / 12u - 1u);
    s[i] = 0;
}

/* ---- state */
static struct {                                                 /* what is being played: a slot's pattern, knobs and tempo */
    step_t pat[STEPS];
    knobs_t kn;
    uint8_t bpm, oct, view, magic;
} set;
static struct { uint8_t slot, oct, view, magic; } cfg;          /* the settings sector: where you were */
#define SET_OK 0xA5u
static uint32_t slot_cur;
static uint32_t cur, page, view_now;
static int32_t env_view;
static uint32_t anim_t;                                         /* the floor and the smiley run on this */

/* the floor: a checkerboard that waves, more when the filter is open, and slowly changes colour */
static const uint16_t FLOOR[3] = {RGB(44, 50, 0), RGB(58, 0, 46), RGB(0, 44, 58)};
static void floor_rows(int32_t y0, int32_t y1, int32_t amp, uint32_t t)
{
    int32_t y;
    uint32_t ci = t / 300u, fi = ci / 1u % 3u, fj = (ci + 1u) % 3u, mix = t % 300u;
    uint16_t c = (uint16_t)((((FLOOR[fi] >> 11) * (300u - mix) + (FLOOR[fj] >> 11) * mix) / 300u) << 11 |
                            ((((FLOOR[fi] >> 5) & 63u) * (300u - mix) + ((FLOOR[fj] >> 5) & 63u) * mix) / 300u) << 5 |
                            ((FLOOR[fi] & 31u) * (300u - mix) + (FLOOR[fj] & 31u) * mix) / 300u);
    for (y = y0; y < y1; y++) {
        int32_t ox = sina((uint32_t)y * 3u / 2u + t) * amp / 127 + (int32_t)(t / 3u % 44u), cy = (y + sina(t / 2u + (uint32_t)(y >> 4)) * amp / 254 + 400) / 22, x;
        for (x = -ox - 44; x < 240; x += 22) {
            int32_t cxi = (x + ox + 440) / 22;
            if ((cxi + cy) & 1) cv_rect(x + ox, y, 22, 1, c);
        }
    }
}

static void dial(int32_t cx, int32_t cy, int32_t r, uint32_t v, uint32_t vmax, uint16_t col)
{
    uint32_t idx = 24u + v * 48u / vmax, j;
    for (j = 24u; j <= 72u; j += 2u) {                          /* the track, lit up to the value */
        int32_t px = cx + cosa(j) * r / 127, py = cy + sina(j) * r / 127;
        cv_rect(px - 1, py - 1, 3, 3, j <= idx ? col : C_RING);
    }
    cv_line(cx, cy, cx + cosa(idx) * (r - 5) / 127, cy + sina(idx) * (r - 5) / 127, C_Y);
    cv_line(cx + 1, cy, cx + 1 + cosa(idx) * (r - 5) / 127, cy + sina(idx) * (r - 5) / 127, C_Y);
    disc(cx, cy, 2, C_Y);
}
static void wave_icon(int32_t cx, int32_t cy, int32_t r, uint32_t sq, uint16_t col)
{
    int32_t k, x0 = cx - r, w = r;                              /* two teeth of a saw, or two pulses */
    for (k = 0; k < 2; k++) {
        int32_t x = x0 + k * w, lo = cy + r / 2, hi = cy - r / 2;
        if (sq) { cv_rect(x, hi, 2, r, col); cv_rect(x, hi, w / 2, 2, col); cv_rect(x + w / 2, hi, 2, r, col); cv_rect(x + w / 2, lo, w / 2, 2, col); }
        else { cv_line(x, lo, x + w, hi, col); cv_line(x, lo + 1, x + w, hi + 1, col); cv_rect(x + w, hi, 2, r, col); }
    }
}

#define PAGES 5u
static const char *const KNAME[PAGES][4] = {{"CUT", "RES", "ENV", "DEC"}, {"ACC", "SLD", "DRV", "WAV"}, {"ADC", "TRK", "ATK", "MUF"}, {"TUN", "SUB", "SWP", "POL"}, {"FM", "HIR", "SWG", "LEN"}};
static uint8_t *knob_p(uint32_t pg, uint32_t k) { uint8_t *a = &set.kn.cut; return &a[pg * 4u + k]; }

static uint8_t gen_hits, gen_rot, gen_rng, gen_spc;
static void scene_low(void)
{
    uint32_t k;
    int32_t y;
    cv_rect(0, (int32_t)Y_LOW - 4, 240, 64, C_K);
    for (k = 0; k < 4u; k++) {
        static const char *const GN[4] = {"HIT", "TRN", "RNG", "SPC"};
        int32_t cx = 30 + (int32_t)k * 60;
        uint16_t col = k == 0u ? C_LIME : k == 1u ? C_PINK : k == 2u ? C_CY : C_OR;
        if (view_now == V_GEN) {
            uint32_t gv[4] = {gen_hits, gen_rot, gen_rng, gen_spc}, gm[4] = {16, 15, 14, 8};
            dial(cx, (int32_t)Y_LOW + 12, 14, gv[k], gm[k], col);
            cv_text(cx - text_w(&FONT_S, GN[k]) / 2, (int32_t)Y_LOW + 25, &FONT_S, GN[k], C_Y);
            continue;
        } else {
            uint32_t v = *knob_p(page, k);
            if (page == 1u && k == 3u) wave_icon(cx, (int32_t)Y_LOW + 12, 14, v, C_LIME);
            else dial(cx, (int32_t)Y_LOW + 12, 14, v, page == 4u && k == 3u ? 16u : KMAX, col);
            cv_text(cx - text_w(&FONT_S, KNAME[page][k]) / 2, (int32_t)Y_LOW + 25, &FONT_S, KNAME[page][k], C_Y);
        }
    }
    if (view_now != V_GEN) for (k = 0; k < PAGES; k++) cv_rect(232, (int32_t)Y_LOW + 2 + (int32_t)k * 7, 6, 4, k == page ? C_Y : C_RING);
    for (y = 0; y < (int32_t)VIEWS; y++) {                      /* tabs */
        static const char *const NM[VIEWS] = {"STEPS", "SOUND", "FLY", "GEN"};
        static const uint16_t TC[VIEWS] = {C_Y, C_PINK, C_LIME, C_OR};
        int32_t x = y * 60;
        uint16_t bg = (uint32_t)y == view_now ? TC[y] : C_K, fg = (uint32_t)y == view_now ? C_K : TC[y];
        cv_rect(x, 218, 60, 22, bg);
        cv_rect(x, 218, 60, 2, TC[y]);
        cv_text(x + 30 - text_w(&FONT_S, NM[y]) / 2, 221, &FONT_S, NM[y], fg);
    }
}

static void scene_head(void)
{
    char s[8];
    uint32_t now = seq_run ? seq_step : cur, nt = pat[now % STEPS].note, ac = nt && (pat[now % STEPS].fl & F_ACC);
    cv_rect(0, 0, 240, (int32_t)Y_HDR, C_K);
    smiley(23, 20, 16, seq_run ? 4 + env_view / 2200 : 0, anim_t / 2u, ac ? 5 : 0, seq_run ? (int32_t)sina(seq_step * 4u) / 40 : 0);
    note_name(s, nt);
    if (s[0]) cv_text(86, 6, &FONT_L, s, ac ? C_PINK : C_Y);
    utoa3(s, seq_bpm);
    cv_text(236 - text_w(&FONT_L, s), 6, &FONT_L, s, ext_on ? C_CY : C_Y);
    cv_rect(0, (int32_t)Y_HDR - 3, 240, 3, C_Y);
}

/* a step as a tiny smiley: asleep for a rest, grinning on an accent, big when it is playing */
static void mini(int32_t cx, int32_t cy, uint32_t mode)
{
    uint16_t face = mode == 0u ? C_RING : mode == 1u ? C_LIME : mode == 2u ? C_PINK : C_Y;
    int32_t r = mode == 3u ? 6 : 5;
    disc(cx, cy, r, face);
    if (mode == 0u) { cv_rect(cx - 3, cy - 1, 2, 1, C_K); cv_rect(cx + 1, cy - 1, 2, 1, C_K); cv_rect(cx - 2, cy + 2, 4, 1, C_K); return; }
    cv_rect(cx - 3, cy - 2, 2, 3, C_K); cv_rect(cx + 1, cy - 2, 2, 3, C_K);
    cv_rect(cx - 3, cy + 2, 1, 1, C_K); cv_rect(cx - 2, cy + 3, 4, 1, C_K); cv_rect(cx + 2, cy + 2, 1, 1, C_K);
    if (mode >= 2u) cv_rect(cx - 2, cy + 2, 4, 2, C_K);
}

static void scene_steps(void)
{
    uint32_t i;
    floor_rows((int32_t)Y_HDR, (int32_t)Y_LOW - 4, 6 + env_view / 1800, anim_t);
    for (i = 0; i < STEPS; i++) {
        int32_t x = 4 + (int32_t)i * 29 / 2, base = 150, h;
        const step_t s = pat[i];
        uint32_t on = seq_run && seq_step == i, ac = s.fl & F_ACC, beyond = i >= (uint32_t)set.kn.len;
        uint16_t c = on ? C_Y : ac ? C_PINK : C_LIME;
        if (beyond) { cv_rect(x + 4, base - 3, 4, 3, C_RING); mini(x + 6, 162, 0u); continue; }
        if (s.note) {
            h = ((int32_t)s.note - 33) * 3;
            h = h < 8 ? 8 : h > 100 ? 100 : h;
            cv_rect(x, base - h, 12, h, c);
            cv_rect(x + 2, base - h + 2, 3, h - 4, on ? C_K : ac ? RGB(255, 140, 210) : RGB(210, 255, 120));
            if (ac) cv_rect(x - 1, base - h - 5, 14, 3, C_Y);
            if (s.fl & F_SLD) {                                 /* a slide: a bridge to the next note's top */
                const step_t n = pat[(i + 1u) % STEPS];
                if (n.note) {
                    int32_t h2 = ((int32_t)n.note - 33) * 3, d;
                    h2 = h2 < 8 ? 8 : h2 > 100 ? 100 : h2;
                    for (d = 0; d < 3; d++) cv_line(x + 12, base - h + d, x + 14, base - h2 + d, C_CY);
                }
            }
        } else cv_rect(x + 4, base - 3, 4, 3, C_RING);
        mini(x + 6, 162, on ? 3u : !s.note ? 0u : ac ? 2u : 1u);
    }
    {
        int32_t x = 4 + (int32_t)cur * 29 / 2;
        cv_rect(x - 1, 170, 14, 2, C_Y);
        cv_rect(x - 1, 168, 2, 3, C_Y); cv_rect(x + 11, 168, 2, 3, C_Y);
    }
}

/* the sound: where the cutoff sits (cyan), where the envelope throws it (pink), the resonance as a bump */
static void scene_sound(void)
{
    int32_t x, c0 = set.kn.cut * 7, c1 = c0 + ENV_AMT[set.kn.env], bump = (int32_t)set.kn.res * set.kn.res * 46 / (31 * 31);
    int32_t last0 = 0, last1 = 0;
    floor_rows((int32_t)Y_HDR, (int32_t)Y_LOW - 4, 6 + env_view / 1800, anim_t);
    for (x = 8; x < 232; x++) {
        int32_t i = (x - 8) * 255 / 224, d0 = i - c0, d1 = i - c1, y0, y1, ay;
        y0 = 62 + (d0 > 0 ? d0 * 3 / 2 : 0) - (d0 > -14 && d0 < 14 ? bump * (14 - (d0 < 0 ? -d0 : d0)) / 14 : 0);
        y1 = 62 + (d1 > 0 ? d1 * 3 / 2 : 0) - (d1 > -14 && d1 < 14 ? bump * (14 - (d1 < 0 ? -d1 : d1)) / 14 : 0);
        y0 = y0 < 48 ? 48 : y0 > 166 ? 166 : y0;
        y1 = y1 < 48 ? 48 : y1 > 166 ? 166 : y1;
        for (ay = y1; ay < y0; ay++) if (((ay + x / 2) & 3) < 2) cv_rect(x, ay, 1, 1, C_PINK);         /* where the envelope throws it, striped */
        for (ay = y0 + 3; ay < 168; ay += 4) cv_rect(x, ay, 1, 1, RGB(0, 90, 100));
        if (x > 8) { int32_t d; for (d = 0; d < 3; d++) { cv_line(x - 1, last0 + d, x, y0 + d, C_CY); cv_line(x - 1, last1 + d, x, y1 + d, C_Y); } }
        last0 = y0; last1 = y1;
    }
}

/* ---- a random pattern (the TD-3-mo's trick: it might rule, it might not) */
static uint32_t rnd_s;
static uint32_t rnd(void) { rnd_s = rnd_s * 1664525u + 1013904223u; return rnd_s >> 8; }
static void random_pattern(void)
{
    static const uint8_t SC[10] = {0, 0, 3, 5, 7, 7, 10, 12, 12, 15};
    uint32_t i;
    for (i = 0; i < STEPS; i++) {
        step_t *s = &set.pat[i];
        s->note = rnd() % 4u ? (uint8_t)(36u + SC[rnd() % 10u] + (rnd() % 5u == 0u ? 12u : 0u)) : 0;
        s->fl = (uint8_t)(s->note ? ((rnd() % 4u == 0u ? F_ACC : 0) | (rnd() % 4u == 0u ? F_SLD : 0)) : 0);
    }
}

static void mutate(void)                                        /* nudge a few steps: a rest wakes up, a note moves an octave or goes quiet, an accent or a slide flips */
{
    static const uint8_t SC[5] = {0, 3, 7, 10, 12};
    uint32_t n;
    for (n = 0; n < 3u; n++) {
        step_t *s = &set.pat[rnd() % STEPS];
        uint32_t r = rnd() % 5u;
        if (!s->note) { if (r < 3u) { s->note = (uint8_t)(36u + SC[rnd() % 5u]); s->fl = 0; } }
        else if (r == 0u) { s->note = 0; s->fl = 0; }
        else if (r == 1u) s->note = (uint8_t)(s->note >= 48u ? s->note - 12u : s->note + 12u);
        else if (r == 2u) s->fl ^= F_ACC;
        else if (r == 3u) s->fl ^= F_SLD;
        else s->note = (uint8_t)(s->note + (rnd() & 1u ? 2u : -2u));
    }
}
static void pat_copy(void) { uint32_t i; for (i = 0; i < STEPS; i++) pat[i] = set.pat[i]; kn = set.kn; }

#define FLY_ROOT 36
/* ---- undo: one level (REC); random, mutate, GEN and a cleared slot save the pattern first, REC again puts it back */
static step_t undo_pat[STEPS];
static uint32_t undo_has;
static void undo_save(void) { uint32_t i; for (i = 0; i < STEPS; i++) undo_pat[i] = set.pat[i]; undo_has = 1; }
static void undo_swap(void) { uint32_t i; for (i = 0; i < STEPS; i++) { step_t t = set.pat[i]; set.pat[i] = undo_pat[i]; undo_pat[i] = t; } }

/* ---- GEN: hits spread evenly over the 16 steps, a melody that wanders. The knobs reshape it, SELECT rolls the dice. */
static const uint8_t LADDER[15] = {0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24, 27, 29, 31, 34};     /* a minor pentatonic, three octaves */
static uint8_t gen_hits = 7, gen_rot, gen_rng = 6, gen_spc = 3;      /* (declared early for the dials) */
static uint32_t gen_seed = 7;
static void gen_apply(void)
{
    uint32_t i, r = gen_seed * 2654435761u + 12345u, deg = 0, first = 1;
    for (i = 0; i < STEPS; i++) {
        uint32_t j = (i + STEPS - gen_rot) % STEPS;
        step_t *s = &set.pat[i];
        if ((j + 1u) * gen_hits / STEPS == j * gen_hits / STEPS) { s->note = 0; s->fl = 0; continue; }
        r = r * 1664525u + 1013904223u;
        deg = (uint32_t)((int32_t)deg + (int32_t)((r >> 16) % 5u) - 2);
        if ((int32_t)deg < 0) deg = 0;
        if (deg > gen_rng) deg = gen_rng;
        s->note = (uint8_t)(FLY_ROOT + LADDER[deg]);
        s->fl = (uint8_t)((first || ((r >> 8) & 7u) < gen_spc / 2u ? F_ACC : 0) | (((r >> 24) & 7u) < gen_spc / 3u ? F_SLD : 0));
        first = 0;
    }
}
static void scene_gen(void)
{
    int32_t cx = 120, cy = 108, i, j;
    uint32_t pos = seq_run ? seq_step : 99u;
    floor_rows((int32_t)Y_HDR, (int32_t)Y_LOW - 4, 3 + env_view / 2500, anim_t);
    for (j = 0; j < 3; j++) {                                   /* rings that breathe with the filter */
        int32_t r = 22 + j * 18 + env_view / 3000, a;
        for (a = 0; a < 64; a++) cv_rect(cx + cosa((uint32_t)a) * r / 127, cy + sina((uint32_t)a) * r / 127, 1, 1, j == 1 ? C_CY : C_RING);
    }
    for (i = 0; i < (int32_t)STEPS; i++) {
        uint32_t a = 48u + (uint32_t)i * 4u;
        const step_t st = pat[i];
        int32_t h, k, x, y;
        uint16_t c = (uint32_t)i == pos ? C_Y : st.fl & F_ACC ? C_PINK : C_LIME;
        if (!st.note) { x = cx + cosa(a) * 18 / 127; y = cy + sina(a) * 18 / 127; cv_rect(x - 1, y - 1, 3, 3, C_RING); continue; }
        h = ((int32_t)st.note - 33) * 38 / 34 + 4;
        h = h < 4 ? 4 : h > 42 ? 42 : h;
        for (k = 14; k < 14 + h; k += 2) disc(cx + cosa(a) * k / 127, cy + sina(a) * k / 127, 2, c);
        x = cx + cosa(a) * (14 + h) / 127; y = cy + sina(a) * (14 + h) / 127;
        disc(x, y, 4, c);
        if (st.fl & F_SLD) { const step_t n = pat[(i + 1) % (int32_t)STEPS]; if (n.note) { uint32_t a2 = 48u + (uint32_t)((i + 1) % (int32_t)STEPS) * 4u; int32_t h2 = ((int32_t)n.note - 33) * 38 / 34 + 4, d; h2 = h2 < 4 ? 4 : h2 > 42 ? 42 : h2; for (d = 0; d < 2; d++) cv_line(x + d, y, cx + cosa(a2) * (14 + h2) / 127 + d, cy + sina(a2) * (14 + h2) / 127, C_CY); } }
    }
    smiley(cx, cy, 11, 0, 0, seq_run && pos < STEPS && (pat[pos].fl & F_ACC) ? 4 : 0, 0);
}

/* ---- FLY: the ship's height is the pitch, a shot is a note, no shot is a rest. SELECT steers, every key fires. */
#define SHIP_X 52
#define COL_W 22
#define FOES 4u
static int32_t fly_deg = 4, ship_y = 120 << 4;
static uint8_t foe_step[FOES], foe_deg[FOES], foe_live;
static uint32_t fly_bolt_ms, fly_boom_ms, fly_boom_step, fly_boom_deg, fired_ms[STEPS], fly_last_step = 99;
static int32_t deg_y(int32_t d) { return 160 - d * 104 / 14; }
static int32_t note_y(int32_t n) { n = n < 36 ? 36 : n > 70 ? 70 : n; return 160 - (n - 36) * 104 / 34; }
static void foes_spawn(void)
{
    uint32_t i;
    foe_live = 0;
    for (i = 0; i < FOES; i++) {
        foe_step[i] = (uint8_t)(i * 4u + 1u + rnd() % 3u);
        foe_deg[i] = (uint8_t)(rnd() % 15u);
        foe_live |= 1u << i;
    }
}
static void fly_fire(uint32_t acc_held, uint32_t slide_held)
{
    uint32_t nt = FLY_ROOT + LADDER[fly_deg], st, a, b, i, prev, acc = acc_held, step_ms = 15000u / seq_bpm;
    if (seq_run) { do { a = seq_step; b = sq_acc; } while (a != seq_step); st = b >= 330750u ? (a + 1u) % STEPS : a; }
    else { st = cur; a = st; }
    for (i = 0; i < FOES; i++)
        if ((foe_live >> i & 1u) && foe_step[i] == st && (uint32_t)(fly_deg - (int32_t)foe_deg[i] + 1) <= 2u) {      /* a hit: an accent */
            foe_live &= ~(1u << i); acc = 1; fly_boom_ms = fm1_ms; fly_boom_step = st; fly_boom_deg = foe_deg[i];
        }
    prev = (st + STEPS - 1u) % STEPS;
    if (!slide_held && fm1_ms - fired_ms[prev] < 2u * step_ms && set.pat[prev].note && set.pat[prev].note != nt && !(fired_ms[prev] == 0u))
        set.pat[prev].fl |= F_SLD;                                  /* neighbours that change pitch slide */
    set.pat[st].note = (uint8_t)nt;
    set.pat[st].fl = (uint8_t)(acc ? F_ACC : 0);
    fired_ms[st] = fm1_ms ? fm1_ms : 1u;
    fly_bolt_ms = fm1_ms;
    pat_copy();
#ifdef FLY_DEBUG
    printf("%6u ms fire step %u (head %u) note %u acc %u slide(prev) %u\n", fm1_ms, st, a, nt, acc, set.pat[prev].fl & F_SLD);
#endif
    if (!seq_run || st == a) voice_on(nt, acc, 0);
    if (!seq_run) cur = (cur + 1u) % STEPS;
}
static void scene_fly(void)
{
    int32_t k, i, pos = seq_run ? (int32_t)seq_step : (int32_t)cur, fr = 0;
    uint32_t t = anim_t;
    if (seq_run) fr = (int32_t)(sq_acc * COL_W / 661500u);
    cv_rect(0, (int32_t)Y_HDR, 240, (int32_t)Y_LOW - 4 - (int32_t)Y_HDR, RGB(10, 0, 18));
    for (i = 0; i < 40; i++) {                                      /* stars, three depths */
        uint32_t h = (uint32_t)i * 2654435761u, sp = 1u + h % 3u;
        int32_t x = (int32_t)((h >> 8) % 240u) - (int32_t)(t * sp / 2u) % 240, y = 48 + (int32_t)((h >> 20) % 118u);
        if (x < 0) x += 240;
        cv_rect(x, y, sp, 1 + (int32_t)sp / 2, sp == 3u ? C_Y : sp == 2u ? C_CY : C_RING);
    }
    for (k = 0; k < 15; k += 1) {                                   /* the ladder: faint lines at each scale degree */
        int32_t y = deg_y(k), x;
        for (x = 0; x < 240; x += 8) cv_rect(x, y, 2, 1, k % 5 == 0 ? RGB(80, 60, 110) : RGB(36, 26, 56));
    }
    for (k = -3; k <= 9; k++) {                                     /* the steps going by */
        int32_t x = SHIP_X + k * COL_W - fr, si = (pos + k + 32) % (int32_t)STEPS, y;
        const step_t s = pat[si];
        cv_rect(x - COL_W / 2, 50, 1, 118, si % 4 == 0 ? RGB(90, 70, 130) : RGB(40, 30, 60));
        if (s.note) {
            y = note_y(s.note);
            disc(x, y, 5, s.fl & F_ACC ? C_PINK : C_LIME);
            if (s.fl & F_SLD) { const step_t n = pat[(si + 1) % (int32_t)STEPS]; if (n.note) { int32_t d; for (d = 0; d < 3; d++) cv_line(x + 5, y + d - 1, x + COL_W - 5, note_y(n.note) + d - 1, C_CY); } }
        }
    }
    for (i = 0; i < (int32_t)FOES; i++) {                           /* foes: grumpy little faces */
        int32_t dk = ((int32_t)foe_step[i] - pos + 32 + 8) % 16 - 8, x = SHIP_X + dk * COL_W - fr, y = deg_y(foe_deg[i]);
        if (!(foe_live >> i & 1u) || dk < -3 || dk > 9) continue;
        disc(x, y, 7, C_OR);
        cv_rect(x - 4, y - 3, 3, 3, C_K); cv_rect(x + 2, y - 3, 3, 3, C_K);
        cv_line(x - 4, y - 5, x - 1, y - 3, C_K); cv_line(x + 4, y - 5, x + 1, y - 3, C_K);
        cv_rect(x - 3, y + 3, 7, 2, C_K);
    }
    if (fm1_ms - fly_bolt_ms < 140u) {                              /* the shot */
        int32_t y = ship_y >> 4;
        cv_rect(SHIP_X + 10, y - 1, 240 - SHIP_X, 3, C_LIME);
        cv_rect(SHIP_X + 10, y, 240 - SHIP_X, 1, C_Y);
    }
    if (fm1_ms - fly_boom_ms < 300u) {
        int32_t dk = ((int32_t)fly_boom_step - pos + 32 + 8) % 16 - 8, x = SHIP_X + dk * COL_W - fr, y = deg_y((int32_t)fly_boom_deg), r = (int32_t)(fm1_ms - fly_boom_ms) / 12 + 4, j;
        for (j = 0; j < 16; j++) cv_rect(x + cosa((uint32_t)j * 4u) * r / 127 - 1, y + sina((uint32_t)j * 4u) * r / 127 - 1, 3, 3, j & 1 ? C_PINK : C_Y);
    }
    smiley(SHIP_X, ship_y >> 4, 10, 0, 0, fm1_ms - fly_bolt_ms < 140u ? 4 : 0, 2);
}

/* ---- the pattern slots */
static uint32_t pick_on, pick_sel;
static void slot_load(uint32_t i)
{
    const slot_t *sl = slot_get(i);
    uint32_t k;
    for (k = 0; k < STEPS; k++) set.pat[k] = sl->pat[k];
    set.kn = sl->kn; set.bpm = sl->bpm; seq_bpm = set.bpm; slot_cur = i; cur = 0;
    pat_copy();
}
static void slot_save(void)
{
    slot_t *sl = &bank.s[slot_cur];
    uint32_t k;
    for (k = 0; k < STEPS; k++) sl->pat[k] = set.pat[k];
    sl->kn = set.kn; sl->bpm = set.bpm; sl->used = 1;
    bank_flush();
}
static void scene_pick(void)
{
    uint32_t i, k;
    char nm[4];
    cv_rect(0, 0, 240, 240, C_K);
    for (i = 0; i < SLOTS; i++) {
        int32_t x = 6 + (int32_t)(i % 3u) * 79, y = 6 + (int32_t)(i / 3u) * 52;
        const slot_t *sl = slot_get(i);
        uint16_t fr = i == pick_sel ? C_Y : C_RING;
        cv_rect(x, y, 74, 46, fr);
        cv_rect(x + 2, y + 2, 70, 42, i == pick_sel ? RGB(30, 28, 0) : C_K);
        if (i == pick_sel) cv_rect(x + 2, y + 2, 70, 3, C_Y);
        for (k = 0; k < STEPS; k++) {
            const step_t st = sl->pat[k];
            int32_t h = ((int32_t)st.note - 33) * 26 / 34;
            if (!st.note) continue;
            h = h < 3 ? 3 : h > 26 ? 26 : h;
            cv_rect(x + 5 + (int32_t)k * 4, y + 42 - h, 3, h, st.fl & F_ACC ? C_PINK : C_LIME);
        }
        utoa3(nm, i + 1u);
        cv_text(x + 5, y + 6, &FONT_S, nm, i == pick_sel ? C_Y : C_RING);
        if (!sl->used) cv_rect(x + 62, y + 8, 4, 4, C_RING);
        if (i == slot_cur) mini(x + 61, y + 16, 3u);
    }
    cv_text(236 - text_w(&FONT_S, "HOME  Open"), 222, &FONT_S, "HOME  Open", C_Y);
}

static void paint(uint32_t y0, uint32_t y1)                     /* rows y0 .. y1 of the screen */
{
    while (y0 < y1) {
        uint32_t h = y1 - y0 > 64u ? 64u : y1 - y0;
        cv_begin(240, h, C_K);
        cv_oy = -(int32_t)y0;
        if (pick_on) { scene_pick(); cv_oy = 0; cv_blit(0, y0); y0 += h; continue; }
        if (y0 < Y_HDR) scene_head();
        if (y0 + h > Y_HDR && y0 < Y_LOW - 4u) { if (view_now == V_STEPS) scene_steps(); else if (view_now == V_SOUND) scene_sound(); else if (view_now == V_FLY) scene_fly(); else scene_gen(); }
        if (y0 + h > Y_LOW - 4u) scene_low();
        cv_oy = 0;
        cv_blit(0, y0);
        y0 += h;
    }
}


/* ---- power on: a big smiley, grinning, then melting */
static void splash(void)
{
    uint32_t f, y0;
    for (f = 0; f < 22u; f++) {
        uint32_t t0 = fm1_ms;
        for (y0 = 0; y0 < 240u; y0 += 60u) {
            cv_begin(240, 60, C_K);
            cv_oy = -(int32_t)y0;
            floor_rows(0, 240, 2 + (int32_t)f, f * 6u);
            smiley(120, 92, 56, f < 6u ? 0 : (int32_t)(f - 6u) * 5, f * 3u, f < 6u ? (int32_t)f * 2 : 8, (int32_t)sina(f * 3u) / 40);
            cv_oy = 0;
            cv_blit(0, y0);
        }
        while (fm1_ms - t0 < 50u) platform_poll();
    }
}

static void app_main(void)
{
    uint32_t keys, prev = 0, hb_ms = 0, redraw = 7, last_draw = 0, dirty = 0, t_change = 0, held_note = 0, k, cfg_dirty = 0;
    uint32_t ext_ms = 0, tap_last = 0, tap_n = 0, tap_k = 0, tap_iv[4] = {0}, sel_was = 0, sel_t0 = 0, sel_used = 0;
    uint32_t home_was = 0, home_t0 = 0, home_used = 0, edit_was = 0, edit_t0 = 0, edit_used = 0;
    int32_t e;
    rnd_s = fm1_ticks() ^ 0x1234567u;
#if !defined(SMILEY_HOST) || defined(SPLASH_DEMO)
    splash();
#endif
    bank_load();
    if (settings_load(&cfg, sizeof cfg) != 0 || cfg.magic != SET_OK) { cfg.slot = 0; cfg.oct = 1; cfg.view = 0; cfg.magic = SET_OK; }
    if (cfg.slot >= SLOTS) cfg.slot = 0;
    if (cfg.oct > 3u) cfg.oct = 1;
    set.oct = cfg.oct;
    view_now = cfg.view < VIEWS ? cfg.view : 0u;
    slot_load(cfg.slot);
    for (;;) {
        uint32_t b, rel, down, up, home_now, edit_now;
        platform_poll();
        b = fm1_input_edges(&rel);
        {
            uint8_t st, d1, d2;
            while (midi_get(&st, &d1, &d2)) {
                uint32_t ty = st & 0xF0u;
                if (st == 0xF8u) { ext_ms = fm1_ms; ext_in++; if (!ext_on) { ext_on = 1; ext_in = 1; ext_seen_reset(); } }          /* MIDI clock */
                else if (st == 0xFAu) { seq_play(); redraw |= 7; }                                                                  /* start */
                else if (st == 0xFBu) { seq_run = 1; redraw |= 7; }                                                                 /* continue */
                else if (st == 0xFCu) { seq_stop(); redraw |= 7; }                                                                  /* stop */
                else if (ty == 0x90u && d2) voice_on(d1, d2 >= 100u, 0);
                else if (ty == 0x80u || ty == 0x90u) { if (!seq_run) voice_off(); }
                else if (ty == 0xB0u) {
                    if (d1 == 102u) { if (d2 >= 64u && !seq_run) seq_play(); else if (d2 < 64u && seq_run) seq_stop(); redraw |= 7; }
                    else if (d1 == 103u && d2 >= 30u) { seq_bpm = d2 * 2u; set.bpm = (uint8_t)seq_bpm; dirty = 1; t_change = fm1_ms; redraw |= 1; }
                    else if (d1 == 104u) { slot_save(); dirty = 0; }
                    else if (d1 == 105u) { random_pattern(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
                    else if (d1 == 106u && d2 < SLOTS) { if (dirty) slot_save(); dirty = 0; slot_load(d2); cfg.slot = (uint8_t)d2; cfg_dirty = 1; t_change = fm1_ms; redraw |= 7; }
                    else if (d1 == 74u) { set.kn.cut = (uint8_t)(d2 >> 2); redraw |= 6; }
                    else if (d1 == 75u) { set.kn.env = (uint8_t)(d2 >> 2); redraw |= 6; }
                    else if (d1 == 71u) { set.kn.res = (uint8_t)(d2 >> 2); redraw |= 6; }
                    else if (d1 == 72u) { set.kn.dec = (uint8_t)(d2 >> 2); redraw |= 6; }
                }
            }
            if (fm1_ms - hb_ms >= 1000u) {                  /* heartbeat for tests */
                hb_ms = fm1_ms;
                midi_send(0xB0u, 100u, cpu_max / 1393u > 127u ? 127u : cpu_max / 1393u);
                midi_send(0xB0u, 101u, view_now | (dirty ? 16u : 0u) | (seq_run ? 32u : 0u) | (pick_on ? 64u : 0u));
                midi_send(0xB0u, 107u, slot_cur);
            }
        }
        if (ext_on && fm1_ms - ext_ms > 400u) { ext_on = 0; redraw |= 1; }         /* the clock stopped: back to our own */
        keys = fm1_in.notes;
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        home_now = fm1_in.buttons >> BTN_HOME & 1u;
        edit_now = fm1_in.buttons >> BTN_EDIT & 1u;
        if (home_now && !home_was) { home_t0 = fm1_ms; home_used = 0; }
        if (home_now && !home_used && fm1_ms - home_t0 >= 600u && !pick_on) { home_used = 1; pick_on = 1; pick_sel = slot_cur; redraw |= 7; }      /* hold HOME: the patterns */
        if (!home_now && home_was && !home_used) {                                                         /* tap HOME: the next view, or open the pattern in the frame */
            if (pick_on) {
                if (dirty) slot_save();
                dirty = 0; slot_load(pick_sel); cfg.slot = (uint8_t)pick_sel; cfg_dirty = 1; t_change = fm1_ms; pick_on = 0;
            } else { view_now = (view_now + 1u) % VIEWS; cfg.view = (uint8_t)view_now; cfg_dirty = 1; t_change = fm1_ms; if (view_now == V_GEN) undo_save(); }
            redraw |= 7;
        }
        home_was = home_now;
        if (edit_now && !edit_was) { edit_t0 = fm1_ms; edit_used = 0; }
        if (pick_on && edit_now && !edit_used && fm1_ms - edit_t0 >= 600u) {                              /* hold EDIT on a pattern: it goes back to the start */
            edit_used = 1; if (pick_sel == slot_cur) undo_save(); bank.s[pick_sel].used = 0; bank_flush();
            if (pick_sel == slot_cur) slot_load(pick_sel);
            redraw |= 7;
        }
        {
            uint32_t sel_now = fm1_in.buttons >> BTN_SEL & 1u;
            if (sel_now && !sel_was) { sel_t0 = fm1_ms; sel_used = 0; }
            if (pick_on && sel_now && !sel_used && fm1_ms - sel_t0 >= 600u) {                          /* hold SEL on a pattern: a copy in the first free slot */
                uint32_t f;
                sel_used = 1;
                for (f = 6; f < SLOTS; f++) if (!bank.s[f].used) break;                                  /* (the first six start out as the factory patterns) */
                if (f < SLOTS) { bank.s[f] = *slot_get(pick_sel); bank.s[f].used = 1; bank_flush(); pick_sel = f; redraw |= 7; }
            }
            sel_was = sel_now;
        }
        if (pick_on) {
            if (b & ((1u << BTN_GLO) | (1u << BTN_OCT_DOWN))) { pick_on = 0; redraw |= 7; }
            e = fm1_enc_take(ENC_SELECT) + fm1_enc_take(ENC_PRESETS);
            if (e) { int32_t v = (int32_t)pick_sel + e; pick_sel = (uint32_t)(v < 0 ? 0 : v >= (int32_t)SLOTS ? (int32_t)SLOTS - 1 : v); redraw |= 7; }
            e = fm1_enc_take(ENC_ALGORITHM);
            if (e) { int32_t v = (int32_t)pick_sel + (e < 0 ? -3 : 3); pick_sel = (uint32_t)(v < 0 ? 0 : v >= (int32_t)SLOTS ? (int32_t)SLOTS - 1 : v); redraw |= 7; }
            for (k = 0; k < 4u; k++) fm1_enc_take(ENC_KNOB1 + k);
            if (b & (1u << BTN_PLAY)) { if (seq_run) seq_stop(); else seq_play(); }
            edit_was = edit_now;
            fm1_led_key(BTN_PLAY, seq_run);
            if (redraw && fm1_ms - last_draw >= 20u) { paint(0, 240); last_draw = fm1_ms; redraw = 0; }
            continue;
        }
        edit_was = edit_now;
        for (k = 0; k < 27u; k++) {
            uint32_t nt = 41u + 12u * set.oct + k - 12u;
            if ((down >> k & 1u) && view_now == V_FLY) { fly_fire(fm1_in.buttons >> BTN_ENV & 1u, fm1_in.buttons >> BTN_SEL & 1u); dirty = 1; t_change = fm1_ms; redraw |= 7; }
            else if ((down >> k & 1u) && view_now == V_GEN) { voice_on(nt, 0, 0); held_note = nt; }
            else if (down >> k & 1u) {
                set.pat[cur].note = (uint8_t)nt;
                set.pat[cur].fl = (uint8_t)((fm1_in.buttons >> BTN_ENV & 1u ? F_ACC : 0) | (fm1_in.buttons >> BTN_SEL & 1u ? F_SLD : 0));      /* ENV or SEL held: an accent, a slide */
                voice_on(nt, set.pat[cur].fl & F_ACC, 0);
                held_note = nt;
                cur = (cur + 1u) % STEPS;
                dirty = 1; t_change = fm1_ms; redraw |= 7;
            }
            if ((up >> k & 1u) && !seq_run && (view_now == V_FLY || view_now == V_GEN || held_note == nt)) { voice_off(); held_note = 0; }
        }
        if (b & (1u << BTN_ENV)) { set.pat[cur].fl ^= F_ACC; dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_SEL)) { set.pat[cur].fl ^= F_SLD; dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_EDIT)) { set.pat[cur].note = 0; set.pat[cur].fl = 0; cur = (cur + 1u) % STEPS; dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_SEQ)) {                                          /* tap tempo: the average of the last four taps' gaps */
            uint32_t dt = fm1_ms - tap_last;
            if (dt > 2000u || !tap_last) tap_n = 0;
            else if (!ext_on) {
                uint32_t i, sum = 0, avg, bpm;
                tap_iv[tap_k++ & 3u] = dt;
                if (tap_n < 4u) tap_n++;
                for (i = 0; i < tap_n; i++) sum += tap_iv[i];
                avg = sum / tap_n;
                bpm = avg ? 60000u / avg : 0u;
                if (bpm >= 60u && bpm <= 200u) { seq_bpm = bpm; set.bpm = (uint8_t)bpm; dirty = 1; t_change = fm1_ms; redraw |= 1; }
            }
            tap_last = fm1_ms;
        }
        if (b & (1u << BTN_REC) && undo_has) { undo_swap(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_ARP)) { undo_save(); mutate(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_LFO)) { undo_save(); random_pattern(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_PLAY)) { if (seq_run) { seq_stop(); } else seq_play(); redraw |= 7; }
        if (b & (1u << BTN_OCT_DOWN) && set.oct > 0u) { set.oct--; cfg.oct = set.oct; cfg_dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && set.oct < 3u) { set.oct++; cfg.oct = set.oct; cfg_dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_SAVE)) { if (!seq_run) { slot_save(); if (cfg_dirty) settings_save(&cfg, sizeof cfg); dirty = cfg_dirty = 0; } }
        e = fm1_enc_take(ENC_SELECT);
        if (e && view_now == V_FLY) { fly_deg += e; fly_deg = fly_deg < 0 ? 0 : fly_deg > 14 ? 14 : fly_deg; redraw |= 2; }
        else if (e && view_now == V_GEN) { gen_seed += (uint32_t)e; gen_apply(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        else if (e) { cur = (cur + STEPS + (uint32_t)(e % 16)) % STEPS; redraw |= 7; }
        e = fm1_enc_take(ENC_ALGORITHM);
        if (e) { int32_t v = (int32_t)page + (e < 0 ? -1 : 1); page = (uint32_t)(v < 0 ? 0 : v >= (int32_t)PAGES ? (int32_t)PAGES - 1 : v); redraw |= 4; }
        e = fm1_enc_take(ENC_PRESETS);
        if (e) {
            int32_t v = (int32_t)seq_bpm + e;
            seq_bpm = (uint32_t)(v < 60 ? 60 : v > 200 ? 200 : v);
            set.bpm = (uint8_t)seq_bpm; dirty = 1; t_change = fm1_ms; redraw |= 1;
        }
        for (k = 0; k < 4u; k++) {
            e = fm1_enc_take(ENC_KNOB1 + k);
            if (e && view_now == V_GEN) {
                static uint8_t *const GP[4] = {&gen_hits, &gen_rot, &gen_rng, &gen_spc};
                static const uint8_t GLO[4] = {1, 0, 1, 0}, GHI[4] = {16, 15, 14, 8};
                int32_t v = (int32_t)*GP[k] + e;
                *GP[k] = (uint8_t)(v < GLO[k] ? GLO[k] : v > GHI[k] ? GHI[k] : v);
                gen_apply(); dirty = 1; t_change = fm1_ms; redraw |= 7;
            } else if (e) {
                uint8_t *p = knob_p(page, k);
                int32_t v = (int32_t)*p + e;
                if (page == 1u && k == 3u) v = e > 0 ? 1 : 0;
                if (page == 4u && k == 3u) v = v < 1 ? 1 : v > 16 ? 16 : v;                 /* LEN: 1 to 16 steps */
                *p = (uint8_t)(v < 0 ? 0 : v > KMAX ? KMAX : v);
                dirty = 1; t_change = fm1_ms; redraw |= 6;
            }
        }
        pat_copy();
        if (view_now == V_FLY) {
            int32_t want = deg_y(fly_deg) << 4;
            ship_y += (want - ship_y) / 3;
            if (seq_step != fly_last_step) { if (seq_step == 0u || fly_last_step == 99u) foes_spawn(); fly_last_step = seq_step; }
        }
        if ((dirty || cfg_dirty) && !seq_run && fm1_ms - t_change > 2000u) {
            if (dirty) slot_save();
            if (cfg_dirty) settings_save(&cfg, sizeof cfg);
            dirty = cfg_dirty = 0;
        }
        if (seq_run) { static uint32_t sd; if (sd != seq_step) { sd = seq_step; redraw |= 7; } if (fm1_ms - last_draw > 45u) redraw |= 3; }
        fm1_led_key(BTN_PLAY, seq_run);
        fm1_led_key(BTN_ENV, set.pat[cur].fl & F_ACC);
        fm1_led_key(BTN_SEL, set.pat[cur].fl & F_SLD);
        fm1_led_glow(BTN_SAVE, dirty);
        fm1_led_glow(BTN_REC, undo_has);
        fm1_led_key(BTN_OCT_DOWN, set.oct < 1u);
        fm1_led_key(BTN_OCT_UP, set.oct > 1u);
        {                                                       /* the keys: the cursor's note glows, the playing note is lit */
            int32_t kc = set.pat[cur].note ? (int32_t)set.pat[cur].note - 29 - 12 * (int32_t)set.oct : -1;
            int32_t kp = seq_run && pat[seq_step % STEPS].note ? (int32_t)pat[seq_step % STEPS].note - 29 - 12 * (int32_t)set.oct : -1;
            for (k = 0; k < 27u; k++) { fm1_led_glow(KEY_LED(k), (int32_t)k == kc); fm1_led_key(KEY_LED(k), (int32_t)k == kp); }
        }
        if (redraw && fm1_ms - last_draw >= 20u) {
            uint32_t ms = fm1_ms;
            anim_t = ms / 24u;
            env_view = env_vis;
            if (redraw & 4u) paint(0, 240); else paint(0, Y_LOW - 4u);
            last_draw = ms;
            redraw = 0;
        }
    }
}
