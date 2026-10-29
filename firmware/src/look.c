/* SPDX-License-Identifier: GPL-3.0-only */
/* The look: acid house. A yellow smiley, black, and the colours of a rave flyer. Drawing helpers for the views (included by smileyface.c). */
#define C_Y RGB(255, 238, 0)
#define C_K RGB(0, 0, 0)
#define C_LIME RGB(160, 255, 0)
#define C_PINK RGB(255, 36, 170)
#define C_CY RGB(0, 226, 255)
#define C_OR RGB(255, 120, 0)
#define C_RING RGB(70, 66, 0)
#define C_W RGB(235, 235, 225)

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
        int32_t w = isqrt(r * r + r - dy * dy);                 /* (r * r + r: no one pixel nubs at the poles) */
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}
static void ellipse(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c)
{
    int32_t dy;
    for (dy = -ry; dy <= ry; dy++) {
        int32_t w = isqrt(rx * rx * (ry * ry + ry - dy * dy) / (ry * ry));
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}
static int32_t cosa(uint32_t a) { return SIN64[(a + 16u) & 63u]; }
static int32_t sina(uint32_t a) { return SIN64[a & 63u]; }

/* the smiley: eyes slide with the beat, the mouth grins wider on an accent, and it melts a little when the sound opens.
 * The mouth is a parabola drawn a column at a time, so it stays smooth at any size. */
static void smiley(int32_t cx, int32_t cy, int32_t r, int32_t melt, uint32_t ph, int32_t grin, int32_t look)
{
    int32_t j, x, w = r * 3 / 5, dep = r / 3 + 1 + grin / 2, y0 = cy + r / 5 - grin / 4, t = r / 10 + 2;
    for (j = 0; j < 3; j++) {                                   /* drips */
        int32_t len = (sina(ph + (uint32_t)j * 21u) + 128) * melt / 256;
        int32_t dx = cx - r / 2 + j * (r / 2);
        int32_t dw = r / 8 + 3;                                 /* the drip is thicker on a bigger face */
        if (len > 1) { cv_rect(dx - dw / 2, cy + r - 3, dw, len + 3, C_Y); disc(dx, cy + r + len - 1, dw / 2 + 1, C_Y); }
    }
    disc(cx, cy, r + 2, C_K);
    disc(cx, cy, r, C_Y);
    ellipse(cx - r / 3 + look, cy - r / 4 - 1, r / 7 + 1, r / 3, C_K);
    ellipse(cx + r / 3 + look, cy - r / 4 - 1, r / 7 + 1, r / 3, C_K);
    for (x = -w; x <= w; x++) {                                 /* the mouth */
        int32_t y = y0 + (dep * (w * w - x * x) + w * w / 2) / (w * w);       /* (rounded: no bump at the middle) */
        cv_rect(cx + x, y, 1, t, C_K);
    }
    cv_rect(cx - w - 1, y0 - 1, 2, t, C_K);                     /* the corners curl up a little */
    cv_rect(cx + w, y0 - 1, 2, t, C_K);
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

/* the floor: a checkerboard that waves, more when the filter is open, and slowly changes colour */
static const uint16_t FLOOR[3] = {RGB(44, 50, 0), RGB(58, 0, 46), RGB(0, 44, 58)};
static void floor_rows(int32_t y0, int32_t y1, int32_t amp, uint32_t t)
{
    int32_t y;
    uint32_t ci = t / 300u, fi = ci % 3u, fj = (ci + 1u) % 3u, mix = t % 300u;
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

static void toggle(int32_t cx, int32_t cy, uint32_t level, uint32_t n, uint16_t col)       /* a switch: a slot and a lever that sits at one of n places, lowest at the bottom */
{
    int32_t top = cy - 9, bot = cy + 9, y = n > 1u ? bot - (bot - top) * (int32_t)level / (int32_t)(n - 1u) : bot, i;
    cv_rect(cx - 4, cy - 14, 9, 28, C_RING);
    cv_rect(cx - 2, cy - 12, 5, 24, C_K);
    for (i = 0; i < (int32_t)n; i++) cv_rect(cx + 8, bot - (bot - top) * i / (int32_t)(n - 1u), 4, 2, i == (int32_t)level ? col : C_RING);       /* the places */
    cv_rect(cx - 2, y < cy ? y : cy, 5, y < cy ? cy - y : y - cy, col);                    /* the lever's arm up from the middle ... */
    disc(cx, y, 5, col);
    disc(cx, y, 2, C_K);
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

static void text_at(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text(x, y, &FONT_S, s, c); }
static void text_mid(int32_t cx, int32_t y, const char *s, uint16_t c) { cv_text(cx - text_w(&FONT_S, s) / 2, y, &FONT_S, s, c); }
