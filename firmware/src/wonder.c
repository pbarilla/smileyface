/* SPDX-License-Identifier: GPL-3.0-only */
/* The little wonders: a smiley that reacts when a knob finds something extreme, a flyer for
 * a splash, a moo when every knob has been turned, and Pierre's 1985 (a bare skin: knobs, LEDs and a tape). Included by smileyface.c after views.c. */
static uint32_t wd_kind, wd_ms, wd_prev;                        /* a knob found something extreme: which, when */
static uint32_t wd_touched, wd_need, wd_moo_ms, wd_mooed;      /* knobs turned (a bit each: page * 4 + knob), the ones that count, the moo */
static uint32_t pierre_on;
#define WD_MS 1800u
#define MOO_MS 3600u

static uint32_t wd_extreme(void)                                /* 0 nothing, 1 the filter sings alone, 2 the oscillator is off, 3 flat out */
{
    if (kn.hir && kn.hir != 15u && kn.res >= 26u) return 1u;
    if (kn.drv == 0u) return 2u;
    if (kn.drv >= 29u) return 3u;
    return 0;
}
static void wd_check(void)                                      /* when a knob moves into one of those: a moment of it */
{
    uint32_t k = wd_extreme();
    if (k && k != wd_prev) { wd_kind = k; wd_ms = fm1_ms ? fm1_ms : 1u; }
    wd_prev = k;
}
static void wd_touch(uint32_t pg, uint32_t k)
{
    uint32_t i, j;
    wd_touched |= 1u << (pg * 4u + k);
    if (!wd_need) for (i = 0; i < PAGES; i++) for (j = 0; j < 4u; j++) if (KP[i][j] != K_NONE) wd_need |= 1u << (i * 4u + j);
    if (!wd_mooed && (wd_touched & wd_need) == wd_need) { wd_mooed = 1; wd_moo_ms = fm1_ms ? fm1_ms : 1u; }
}
static uint32_t wd_moo_on(void) { return wd_moo_ms && fm1_ms - wd_moo_ms < MOO_MS; }
static uint32_t wd_on(void) { return (wd_kind && wd_ms && fm1_ms - wd_ms < WD_MS) || wd_moo_on(); }
static uint32_t wd_gag(int32_t *gp)                             /* the face for the header smiley, 0 for none */
{
    if (wd_moo_on()) { *gp = 100; return 1u; }                  /* tongue out */
    if (wd_kind && wd_ms && fm1_ms - wd_ms < WD_MS) { *gp = (int32_t)sina((fm1_ms - wd_ms) * 32u / WD_MS) * 100 / 127; return wd_kind == 1u ? 3u : wd_kind == 2u ? 4u : 2u; }
    return 0;
}
static void wd_rings(void)                                      /* rings spreading out from the header smiley (drawn before it) */
{
    static const uint16_t RC[4] = {0, C_CY, C_LIME, C_PINK};
    uint32_t age;
    int32_t i;
    if (!(wd_kind && wd_ms && fm1_ms - wd_ms < WD_MS)) return;
    age = fm1_ms - wd_ms;
    for (i = 0; i < 3; i++) {
        int32_t r = 19 + (int32_t)((age / 20u + (uint32_t)i * 9u) % 27u);
        if (r > 40) continue;
        disc(23, 20, r, RC[wd_kind & 3u]);
        disc(23, 20, r - 2, C_K);
    }
}

/* ---- the moo: a speech bubble, over everything */
static void wd_post(void)
{
    static const char M[] = "MOOOOOO";
    char t[2] = {0, 0};
    int32_t cx = 62, i;
    uint32_t shake;
    if (!wd_moo_on() || !cv_vis(48, 104)) return;
    cv_rect(40, 54, 188, 42, C_K);
    cv_rect(42, 56, 184, 38, C_W);
    for (i = 0; i < 7; i++) cv_line(32 + i, 44 + i, 52 + i / 2, 56, C_K);          /* the tail, up to the smiley */
    for (i = 0; i < 12; i++) cv_rect(36 + i * 2, 50 + i, 2, 6, C_W);
    shake = fm1_ms / 100u % 12u < 3u;
    for (i = 0; M[i]; i++) {
        int32_t dy = sina(fm1_ms / 45u + (uint32_t)i * 5u) * 6 / 127, dx = shake ? (int32_t)((fm1_ms / 40u * ((uint32_t)i + 3u) * 7u) % 5u) - 2 : 0;
        t[0] = M[i];
        cv_text(cx + dx, 62 + dy, &FONT_L, t, (fm1_ms / 250u + (uint32_t)i) % 5u == 0u ? C_PINK : C_K);
        cx += text_w(&FONT_L, t) + 1;
    }
}

/* ---- the flyer, the launch screen: a photocopy on yellow paper, a different tagline each time, sometimes with waves. */
static uint32_t fl_pick;                                         /* tagline = fl_pick % 4, waves = / 4 & 1 */
static const char *const FL_TAG[4] = {"FM-1_303  ASK AROUND", "IN YOUR MIND", "BRING A FRIEND", "SECRET LOCATION"};
static void wave_band(int32_t y, int32_t amp, uint16_t col, uint32_t t)   /* a wavy line across the screen, like the sixties posters */
{
    int32_t x;
    for (x = 0; x < 240; x += 2) cv_rect(x, y + sina((uint32_t)(x / 5) + t) * amp / 127, 2, 3, col);
}
static void flyer_paint(uint32_t f, int32_t off)               /* one frame, into the strip being drawn (cv_begin and cv_oy are set) */
{
    int32_t i;
    uint32_t seed;
    static const char NM[] = "SMILEYFACE";
    char t[2] = {0, 0};
    int32_t cx = 22;
    cv_rect(0, 0, 240, 240, C_Y);
    if ((fl_pick / 4u) & 1u) { wave_band(34, 8, C_PINK, f); wave_band(40, 8, C_K, f + 3u); wave_band(150, 8, C_PINK, f + 7u); }
    disc(120 + 5, 90 + 4, 74, C_PINK);                       /* the second colour, out of register */
    smiley(120 + off / 2, 90, 70, f < 14u ? (int32_t)f / 2 : 7 - (int32_t)(f - 14u) / 2, (int32_t)sina(f * 3u) / 24);
    cv_rect(0, 0, 240, 5, C_K);                              /* the copier's edge */
    cv_rect(24 + (int32_t)(f % 3u), 0, 2, 240, RGB(200, 180, 0));
    cv_rect(0, 234, 240, 6, C_K);
    for (i = 0; i < 20; i++) {                               /* a torn edge */
        seed = (uint32_t)i * 2654435761u + f / 3u * 40503u;
        cv_rect(i * 12, 228 + (int32_t)(seed >> 13) % 7, 12, 8, C_K);
    }
    for (i = 0; NM[i]; i++) {
        t[0] = NM[i];
        cv_text(cx, 168 + ((i * 7 + (int32_t)(f / 4u)) % 5) - 2, &FONT_L, t, C_K);
        cx += text_w(&FONT_L, t) + 1;
    }
    text_mid(120, 210, FL_TAG[fl_pick % 4u], C_K);
    for (i = 0; i < 260; i++) {                              /* specks, new ones every few frames */
        seed = ((uint32_t)i * 2246822519u) ^ (f / 3u * 3266489917u);
        seed ^= seed >> 15; seed *= 2246822519u; seed ^= seed >> 13;
        cv_rect((int32_t)(seed % 240u), (int32_t)(seed >> 8) % 240, (seed >> 20 & 3u) == 0u ? 2 : 1, 1, C_K);
    }
}
static void splash_flyer(void)
{
    uint32_t f, y0;
    fl_pick = rnd();                                         /* (a tagline of its own) */
    for (f = 0; f < 40u; f++) {
        uint32_t t0 = fm1_ms;
        for (y0 = 0; y0 < 240u; y0 += 60u) {
            cv_begin(240, 60, C_Y);
            cv_oy = -(int32_t)y0;
            flyer_paint(f % 22u, f < 6u ? (int32_t)(6u - f) : 0);
            cv_oy = 0;
            cv_blit(0, y0);
        }
        while (fm1_ms - t0 < 50u) platform_poll();
    }
}

/* ---- Pierre, 1985: no floor, no smileys. Sixteen red lights, big silver knobs, a tape. Hiss. */
static void scene_pierre(void)
{
    uint32_t k, i, seed;
    int32_t x, ph = (int32_t)(fm1_ms / 40u);
    cv_rect(0, 0, 240, 240, RGB(14, 14, 12));
    cv_rect(0, 78, 240, 98, RGB(120, 120, 112));                    /* the panel */
    cv_rect(0, 78, 240, 3, RGB(190, 190, 180)); cv_rect(0, 173, 240, 3, RGB(70, 70, 64));
    for (i = 0; i < STEPS; i++) {                                    /* the steps: dim red where a note is, bright on the one playing */
        const step_t s = PAT[i];
        uint32_t on = seq_run && seq_step == i, len = plen[pat_idx] ? plen[pat_idx] : STEPS;
        uint16_t c = i >= len ? RGB(30, 30, 28) : on ? RGB(255, 40, 20) : s.note ? RGB(130, 0, 0) : RGB(46, 20, 18);
        disc(12 + (int32_t)i * 14, 30, on ? 5 : 4, c);
        if (s.note && (s.fl & F_ACC) && i < len) cv_rect(8 + (int32_t)i * 14, 42, 9, 2, RGB(255, 120, 0));
        if (s.note && (s.fl & F_SLD) && i < len) cv_rect(8 + (int32_t)i * 14, 46, 9, 2, RGB(0, 200, 230));
    }
    cv_rect((int32_t)(cur % STEPS) * 14 + 6, 56, 13, 2, C_W);
    for (k = 0; k < 4u; k++) {                                       /* the knobs: a silver cap, a ridged rim, a line that points */
        int32_t cx = 30 + (int32_t)k * 60, cy = 122, a, px, py;
        uint32_t vmax, v = knob_val(page, k, &vmax);
        if (KP[page][k] == K_NONE) continue;
        for (a = 0; a < 32; a++) cv_rect(cx + sina((uint32_t)a * 2u) * 26 / 127 - 1, cy - cosa((uint32_t)a * 2u) * 26 / 127 - 1, 3, 3, RGB(50, 50, 46));    /* rim ridges */
        disc(cx, cy, 24, RGB(60, 60, 56));
        disc(cx, cy, 21, RGB(205, 205, 196));
        disc(cx, cy, 14, RGB(170, 170, 160));
        a = -24 + (int32_t)(vmax ? 48u * v / vmax : 0u);
        px = cx + sina((uint32_t)(a + 64)) * 21 / 127; py = cy - cosa((uint32_t)(a + 64)) * 21 / 127;
        for (i = 0; i < 4u; i++) cv_line(cx + (px - cx) * (int32_t)i / 8 + (int32_t)i / 2, cy + (py - cy) * (int32_t)i / 8, px, py, RGB(20, 20, 18));
        cv_line(cx, cy, px, py, RGB(20, 20, 18)); cv_line(cx + 1, cy, px + 1, py, RGB(20, 20, 18));
        text_mid(cx, 154, KNAME[page][k], RGB(20, 20, 18));
    }
    cv_rect(56, 180, 128, 48, RGB(46, 44, 40));                      /* a cassette, a label in biro: Ron Hardy's, from the story */
    cv_rect(64, 182, 112, 17, RGB(225, 220, 200)); cv_rect(64, 197, 112, 1, RGB(150, 140, 120));
    { static const char LB[] = "ACID TRACKS"; char tt[2] = {0, 0}; int32_t lx = 72; for (i = 0; LB[i]; i++) { tt[0] = LB[i]; text_at(lx, 182 + (int32_t)((i * 5u) % 3u) - 1, tt, RGB(20, 40, 170)); lx += text_w(&FONT_S, tt); } }
    cv_rect(78, 201, 84, 22, RGB(14, 14, 12));
    for (i = 0; i < 2u; i++) {                                       /* two reels, and the tape goes round them while it plays */
        int32_t cx = 98 + (int32_t)i * 44, cy = 212, j;
        disc(cx, cy, 9, RGB(60, 58, 52)); disc(cx, cy, 3, RGB(190, 190, 180));
        for (j = 0; j < 3; j++) { int32_t an = (seq_run ? ph : 0) * (i ? 1 : -1) + j * 21; cv_line(cx, cy, cx + cosa((uint32_t)an & 63u) * 8 / 127, cy + sina((uint32_t)an & 63u) * 8 / 127, RGB(190, 190, 180)); }
    }
    for (x = 0; x < 240; x += 3) cv_rect(x, 233 + sina((uint32_t)(x / 6) + (uint32_t)ph) * 2 / 127, 3, 2, RGB(70, 70, 64));                        /* tape hiss, a wobbly line */
    for (i = 0; i < 320u; i++) {                                     /* the hiss: specks that never sit still */
        seed = (uint32_t)i * 2246822519u ^ (fm1_ms / 50u) * 3266489917u;
        seed ^= seed >> 15; seed *= 2246822519u; seed ^= seed >> 13;
        cv_rect((int32_t)(seed % 240u), (int32_t)(seed >> 8) % 240, 1, 1, (seed >> 20 & 1u) ? RGB(90, 90, 84) : RGB(0, 0, 0));
    }
    for (k = 0; k < PAGES; k++) cv_rect(236, 84 + (int32_t)k * 5, 4, 3, k == page ? RGB(255, 40, 20) : RGB(70, 70, 64));
}

/* ---- the vortex: left alone for 45 seconds the floor turns into a slow spiral (op-art, dim, it moves about twice a second) */
static uint8_t vt_a[16][30], vt_r[16][30], vt_ok;
static int32_t atan64(int32_t y, int32_t x)                     /* an angle round a circle in 64ths: near enough to draw with */
{
    int32_t ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax, a;
    if (!mx) return 0;
    a = mn * 8 / mx;
    if (ay > ax) a = 16 - a;
    if (x < 0) a = 32 - a;
    if (y < 0) a = 64 - a;
    return a & 63;
}
static void vortex_floor(void)
{
    int32_t i, j;
    uint32_t tt = fm1_ms / 700u;
    uint16_t c = FLOOR[fm1_ms / 4000u % 3u];
    if (!vt_ok) {
        for (j = 0; j < 16; j++) for (i = 0; i < 30; i++) {
            int32_t dx = i * 8 + 4 - 120, dy = 44 + j * 8 + 4 - 108;
            vt_a[j][i] = (uint8_t)atan64(dy, dx);
            vt_r[j][i] = (uint8_t)(isqrt(dx * dx + dy * dy) / 8);
        }
        vt_ok = 1;
    }
    for (j = 0; j < 16; j++) {
        int32_t y = 44 + j * 8;
        if (!cv_vis(y, y + 8)) continue;
        for (i = 0; i < 30; i++) if (((vt_a[j][i] >> 2) + vt_r[j][i] + tt) & 1u) cv_rect(i * 8, y, 8, 8, c);
    }
}
