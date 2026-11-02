/* SPDX-License-Identifier: GPL-3.0-only */
/* The FX rack screen (FX opens it): the bass, four slots and the output as boxes joined by cables, a picture of what the selected unit does,
 * and its knobs. A unit has its own colour all over the screen, so you know whose knobs you are holding. Included by smileyface.c after views.c. */
static uint32_t fx_on, fx_sel;                                   /* the FX view is open; the slot (0 or 1) whose knobs you hold */
static const uint16_t FX_COL[FX_KINDS] = {C_PINK, C_CY, C_OR, C_LIME};
static const char *const FXT_NAME[FX_KINDS][4] = {{""}, {"Robot on pills", "Tapes of wrath", "Take the dub"}, {"thats why i love...", "Laundry Day", "Crunch", "Fuzz"}, {"Bitcrush (hey,|not everything is a pun)", "Dead Radio", ""}};      /* what each is called */
static const char *const FXK_NAME[FX_KINDS][4][4] = {
    {{"", "", "", ""}},
    {{"TIME", "FEEDBK", "MIX", "TONE"}, {"TIME", "FEEDBK", "MIX", "WOW"}, {"TIME", "FEEDBK", "CUTOFF", "LEVEL"}},
    {{"DRIVE", "TONE", "MIX", "LEVEL"}, {"DRIVE", "TONE", "MIX", "LEVEL"}, {"DRIVE", "TONE", "MIX", "LEVEL"}, {"DRIVE", "TONE", "MIX", "LEVEL"}},
    {{"BITS", "RATE", "MIX", "LEVEL"}, {"BAND", "BITS", "MIX", "LEVEL"}, {"", "", "", ""}},
};

static void fx_turn(uint32_t k, uint32_t knob, int32_t e)
{
    fxu_t *u = &rack.u[k];
    int32_t v;
    if (u->type == FX_NONE || knob > 3u) return;
    v = (int32_t)u->p[knob] + e;
    u->p[knob] = (uint8_t)(v < 0 ? 0 : v > 31 ? 31 : v);
}
#define P_TOP 50                                                 /* the picture lives between the header and the knobs */
#define P_BOT 168
static void fx_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    if (y < P_TOP) { h -= P_TOP - y; y = P_TOP; }
    if (y + h > P_BOT) h = P_BOT - y;
    if (h > 0) cv_rect(x, y, w, h, c);
}
static void fx_bar(int32_t x, int32_t base, int32_t h, int32_t w, uint16_t c, uint16_t hi)       /* a fat bar like the steps: a stripe of light down it */
{
    if (h < 3) h = 3;
    fx_rect(x, base - h, w, h, c);
    if (h > 6) fx_rect(x + 2, base - h + 2, 3, h - 4, hi);
}
static void fx_picture(const fxu_t *u)                          /* what the unit does, drawn as the steps are drawn: fat bars on the floor, no words. Every knob shows, and too much */
{
    uint16_t col = FX_COL[u->type], hi = u->type == FX_DELAY ? RGB(150, 245, 255) : u->type == FX_DRIVE ? RGB(255, 190, 120) : RGB(210, 255, 120);
    int32_t x, i, p0 = (int32_t)u->p[0], p1 = (int32_t)u->p[1], p2 = (int32_t)u->p[2], p3 = (int32_t)u->p[3];
    uint32_t sub = u->sub;
    if (u->type == FX_DELAY && sub == DL_TAPE) {                /* two reels, and the tape runs down past a head and back up: TIME spreads the reels, WOW makes it loose, FEEDBK leaves ghosts, MIX is the tape's thickness */
        int32_t r, wow = p3, a = (int32_t)(fm1_ms / 45u) + sina(fm1_ms / 90u) * wow / 500, half = 46 + p0 * 34 / 31, th = 2 + p2 / 5, hy = 150, lx, rx, g, k;
        lx = 120 - half; rx = 120 + half;
        for (r = 0; r < 2; r++) {
            int32_t cx = 120 + (r ? half : -half) + sina(fm1_ms / 60u + (uint32_t)r * 32u) * wow / 800, cy = 96 + sina(fm1_ms / 75u + (uint32_t)r * 20u) * wow / 500, j, dy;
            for (dy = -39; dy <= 39; dy++) {                    /* a ring, not a disc over a disc */
                int32_t wo, wi;
                if (cy + dy < -cv_oy || cy + dy >= -cv_oy + (int32_t)cv_h) continue;
                wo = isqrt(39 * 39 + 39 - dy * dy);
                wi = dy > -31 && dy < 31 ? isqrt(31 * 31 + 31 - dy * dy) : -1;
                if (wi < 0) fx_rect(cx - wo, cy + dy, 2 * wo + 1, 1, col);
                else { fx_rect(cx - wo, cy + dy, wo - wi, 1, col); fx_rect(cx + wi + 1, cy + dy, wo - wi, 1, col); }
            }
            disc(cx, cy, 8, col);
            for (j = 0; j < 3; j++) { int32_t an = a * (r ? 1 : -1) + j * 21; cv_line(cx, cy, cx + cosa((uint32_t)an & 63u) * 27 / 127, cy + sina((uint32_t)an & 63u) * 27 / 127, col); }
        }
        for (g = 0; g <= p1 / 8; g++) {                         /* the tape: down from the left reel, across the head, up to the right one (ghosts below, dimmer) */
            int32_t wob = sina(fm1_ms / 50u + (uint32_t)g * 9u) * wow / 508 * (g ? 2 : 1), y0 = hy + g * 6 + wob;
            if (y0 > 164) y0 = 164;
            if (y0 < 136) y0 = 136;
            uint16_t c = g ? RGB(0, 90, 110) : col;
            for (k = 0; k < (g ? 2 : th); k++) {
                cv_line(lx + 4, 132 + k, 106, y0 + k, c);
                cv_line(106, y0 + k, 134, y0 + k, c);
                cv_line(134, y0 + k, rx - 4, 132 + k, c);
            }
        }
        fx_rect(106, hy - 9, 28, 7, C_W); fx_rect(116, hy - 9, 8, 7, C_K); fx_rect(110, hy - 14, 20, 5, RGB(150, 150, 140));        /* the head */
    } else if (u->type == FX_DELAY) {                           /* the echoes: TIME spaces them, FEEDBK shrinks them slowly or not, and (digi) MIX is how loud, TONE how fat and dull; (dub) CUTOFF fat and dull, LEVEL tall */
        int32_t dub = sub == DL_DUB, sp = !dub ? 22 + (int32_t)FX_DIV[p0 >> 2] * 8 : 20 + p0 * 6, r = 20 + p1 * 275 / 100, n = 0, w = 6 + (31 - (dub ? p2 : p3)) * 12 / 31, h, top = 100;
        int32_t lv = dub ? p3 * 100 / 24 : 100;
        h = (dub ? 100 : 20 + p2 * 80 / 31) * lv / 100;
        x = 8;
        while (x < 226 && h > 4) {
            int32_t hh = h > 100 ? 100 : h, d = n == 0 ? top * lv / 100 : hh;
            if (d > 100) d = 100;
            fx_bar(x, 150, d, w, n == 0 ? C_W : r > 100 ? C_PINK : col, n == 0 ? C_W : r > 100 ? RGB(255, 140, 210) : hi);
            if (cv_vis(155, 170)) mini(x + w / 2, 162, n == 0 ? 3u : 1u);
            x += sp > w + 4 ? sp : w + 4; h = h * r / 100; n++;
        }
    } else if (u->type == FX_DRIVE) {                           /* the wave in (faint) and out: DRIVE bends it, TONE rounds it, MIX shows how much, LEVEL how tall */
        int32_t gq = 16 + (p0 * p0 * 3 >> 2), m = p2, lv = p3 * 256 / 24, gt = 30 + p1 * 226 / 31, lp = 0, py = 100;
        for (x = 6; x < 234; x++) {
            int32_t sn = SIN64[(uint32_t)((x - 6) * 2 / 3) & 63u], dry = sn * 40 / 127, wet = fx_shape(sub, sn * 256 * gq / 16) * 44 / 32768, o, y, lo, hi2;
            lp += (wet - lp) * gt >> 8;
            o = (dry * (31 - m) + lp * m) / 31 * lv >> 8;
            if (o > 48) o = 48;
            if (o < -48) o = -48;
            y = 100 - o;
            lo = y < py ? y : py; hi2 = y < py ? py : y;
            fx_rect(x, 100 - dry, 1, 1, C_RING);
            fx_rect(x, lo, 3, hi2 - lo + 3, col);
            py = y;
        }
        if (cv_vis(155, 170)) mini(120, 162, 3u);
    } else if (u->type == FX_CRUSH && sub == CR_BITS) {         /* a sine in blocks, like a tiny screen: BITS makes the blocks tall, RATE wide, MIX blends it with the clean wave, LEVEL scales it */
        int32_t bh = 3 + p0 * p0 * 36 / 961, bw = 6 + p1 * p1 * 40 / 961, lv = p3 * 256 / 24;
        for (x = 6; x < 234; x++) fx_rect(x, 100 - SIN64[(uint32_t)((x - 6) * 64 / 114) & 63u] * 40 / 127, 1, 1, C_RING);
        for (x = 6; x < 234; x += bw) {
            int32_t v = SIN64[(uint32_t)((x + bw / 2 - 6) * 64 / 114) & 63u] * 40 / 127 + 300, q = (v + bh / 2) / bh * bh - 300, o = (v - 300) + (q - (v - 300)) * p2 / 31;
            o = o * lv >> 8;
            if (o > 46 - bh / 2) o = 46 - bh / 2;
            if (o < bh / 2 - 46) o = bh / 2 - 46;
            fx_rect(x, 100 - o - bh / 2, bw - 1 > 0 ? bw - 1 : 1, bh, col);
        }
    } else if (u->type == FX_CRUSH) {                           /* the band that is left: BAND squeezes it, BITS makes the bars blocky, MIX blends it with the clean, LEVEL scales it */
        int32_t hp = p0 * 14 / 31, lp = 28 - p0 * 12 / 31, qs = 3 + p1 * p1 * 36 / 961, lv = p3 * 100 / 24;
        for (i = 0; i < 16; i++) {
            int32_t b = i * 2, h = 100;
            if (b < hp) h = 4 + 96 * b / (hp ? hp : 1); else if (b > lp) h = 100 - (b - lp) * 14;
            if (h < 4) h = 4;
            h = 100 + (h - 100) * p2 / 31;
            h = h * lv / 100;
            if (h > 97) h = 97;
            fx_bar(4 + i * 29 / 2, 150, h / qs * qs + 3, 12, col, hi);
            if (cv_vis(155, 170)) mini(4 + i * 29 / 2 + 6, 162, 1u);
        }
    }
}
static void fx_weird(int32_t x, int32_t y, const char *w, uint16_t c)       /* a word whose letters will not sit still */
{
    char t[2] = {0, 0};
    int32_t cx = x;
    uint32_t i, shake = fm1_ms / 100u % 24u < 4u;
    for (i = 0; w[i]; i++) {
        int32_t dy = sina(fm1_ms / 55u + i * 6u) * 4 / 127, dx = shake ? (int32_t)((fm1_ms / 40u * (i + 3u) * 7u) % 5u) - 2 : 0;
        t[0] = w[i];
        cv_text(cx + dx, y + dy, &FONT_L, t, (fm1_ms / 250u + i) % 7u == 0u ? C_PINK : c);
        cx += text_w(&FONT_L, t);
    }
}
static void scene_fx(void)
{
    uint32_t k, t = rack.u[fx_sel].type, pos = fx_pos(fx_sel);
    uint16_t col = FX_COL[t];
    floor_rows((int32_t)Y_HDR, (int32_t)Y_LOW - 4, 4 + shock_bump(), anim_t);
    if (cv_vis(0, (int32_t)Y_HDR)) {                            /* (the screen is drawn in strips, and each strip runs all of this: only what shows) */
                                                                /* the header: the smiley, and what is on (the strip starts black: no need to paint it so) */
    smiley_g(23, 20, 16, 0, seq_run ? (int32_t)sina(seq_step * 4u) / 40 : 0, gag_kind && fm1_ms - gag_ms < GAG_MS ? gag_kind : 0u, (int32_t)sina((fm1_ms - gag_ms) * 32u / GAG_MS) * 100 / 127);
    if (t) {                                                    /* its name, right on top (twice, a pixel apart, to be fat); a | breaks the line */
        const char *nm = FXT_NAME[t][rack.u[fx_sel].sub];
        char l1[24], l2[28];
        uint32_t n = 0, m = 0;
        while (nm[n] && nm[n] != '|' && n < 23u) { l1[n] = nm[n]; n++; }
        l1[n] = 0; l2[0] = 0;
        if (nm[n] == '|') { n++; while (nm[n] && m < 27u) l2[m++] = nm[n++]; l2[m] = 0; }
        if (m) {
            cv_text(235 - text_w(&FONT_S, l1), 4, &FONT_S, l1, col); cv_text(236 - text_w(&FONT_S, l1), 4, &FONT_S, l1, col);
            cv_text(235 - text_w(&FONT_S, l2), 22, &FONT_S, l2, col); cv_text(236 - text_w(&FONT_S, l2), 22, &FONT_S, l2, col);
        } else { cv_text(235 - text_w(&FONT_S, l1), 12, &FONT_S, l1, col); cv_text(236 - text_w(&FONT_S, l1), 12, &FONT_S, l1, col); }
    }
    for (k = 0; k < FXU; k++) {                                 /* the bass goes into 1, then 2: two boxes and an arrow, a bracket under yours */
        int32_t x = 48 + (int32_t)k * 36, tp = rack.u[k].type;
        char dg[2] = {(char)('1' + k), 0};
        if (tp) cv_rect(x, 8, 22, 22, FX_COL[tp]);
        else { cv_rect(x, 8, 22, 2, C_RING); cv_rect(x, 28, 22, 2, C_RING); cv_rect(x, 8, 2, 22, C_RING); cv_rect(x + 20, 8, 2, 22, C_RING); }
        text_mid(x + 11, 14, dg, tp ? C_K : C_RING);
        if (k == fx_sel) { cv_rect(x, 33, 22, 2, C_Y); cv_rect(x, 31, 2, 3, C_Y); cv_rect(x + 20, 31, 2, 3, C_Y); }
        if (k + 1u < FXU) { cv_line(x + 24, 19, x + 33, 19, C_W); cv_line(x + 29, 15, x + 33, 19, C_W); cv_line(x + 29, 23, x + 33, 19, C_W); }
    }
    cv_rect(0, (int32_t)Y_HDR - 3, 240, 3, C_Y);
    }
    if (t) {
        const fxu_t *u = &rack.u[fx_sel];
        static const uint16_t KC[4] = {C_LIME, C_PINK, C_CY, C_OR};
        if (cv_vis((int32_t)Y_HDR, (int32_t)Y_LOW - 4)) fx_picture(u);
        if (cv_vis((int32_t)Y_LOW, 240)) for (k = 0; k < 4u; k++) {
            int32_t cx = 30 + (int32_t)k * 60;
            dial(cx, (int32_t)Y_LOW + 12, 14, u->p[k], 31, KC[k]);
            text_mid(cx, (int32_t)Y_LOW + 25, FXK_NAME[t][u->sub][k], C_Y);
        }
    } else {                                                    /* nothing on: the smiley looks at the knob to twist, and a goofy arrow points at it */
        int32_t bob = sina(fm1_ms / 40u) * 5 / 127, x, i;
        if (cv_vis(160, 195)) for (x = 22; x < 68; x += 3) cv_rect(x + bob, 173 + sina((uint32_t)(x / 5) + fm1_ms / 50u) * 4 / 127, 3, 7, C_PINK);        /* a wobbly shaft */
        if (cv_vis(160, 195)) for (i = 0; i < 20; i++) cv_rect(2 + i + bob, 177 - i * 15 / 20 - 1, 1, i * 30 / 20 + 3, C_PINK);                                 /* a fat head, tip at the edge */
        if (cv_vis(158, 210)) { cv_rect(76, 158, 164, 52, C_K); fx_weird(82, 162, "TWIST", C_Y); fx_weird(82, 184, "THIS", C_Y); }
    }
    if (cv_vis(218, 240)) for (k = 0; k < FXL_N; k++) {          /* the row ALGORITHM walks: a smiley for each, a bracket on yours */
        int32_t x = 13 + (int32_t)k * 24;
        mini(x, 228, k == pos ? 3u : k == 0u ? 0u : 1u);
        if (k == pos) { cv_rect(x - 9, 237, 19, 2, C_Y); cv_rect(x - 9, 234, 2, 4, C_Y); cv_rect(x + 8, 234, 2, 4, C_Y); }
    }
}
