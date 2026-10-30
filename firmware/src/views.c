/* SPDX-License-Identifier: GPL-3.0-only */
/* The views: STEPS, SOUND, FLY, GEN, PLAY, the switches layer (FX), the projects picker, and the top and bottom bars. Included by smileyface.c. */
static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};      /* the 16 white keys, by key number */
enum { V_STEPS, V_FLY, V_WALK, V_PLAY, VIEWS };
#define Y_HDR 44u
#define Y_LOW 176u                              /* the knobs and the tabs start here */
#define PAT pats[pat_idx]

static struct { uint8_t slot, oct, view, magic; } cfg;          /* the settings sector: where you were */
#define SET_OK 0xA6u
static uint32_t slot_cur;
static uint32_t cur, page, view_now;
static int32_t env_view;
static uint32_t anim_t;                                         /* the floor and the smiley run on this */
static uint32_t pick_on, pick_sel;
static uint32_t shock_ms;                               /* the last touch; the last accent (the shockwave starts then) */
static uint32_t rnd_s;
static uint32_t rnd(void) { rnd_s = rnd_s * 1664525u + 1013904223u; return rnd_s >> 8; }

/* ---- the switches: the last pages of the ALGORITHM list, drawn as switches */
enum { P_WAVE, P_SUB, P_MUF, P_SWEEP, P_HIRES, P_POLES, P_ACCON, PADS_USED };
static uint32_t pad_level(uint32_t p)                           /* a switch's setting, 0..2 */
{
    switch (p) {
    case P_WAVE: return kn.wave & 1u;
    case P_SUB: return kn.sub ? 1u : 0u;
    case P_MUF: return kn.muf == 0u ? 0u : kn.muf < 16u ? 1u : 2u;
    case P_SWEEP: return kn.swp < 8u ? 0u : kn.swp < 20u ? 1u : 2u;
    case P_HIRES: return kn.hir ? 1u : 0u;
    case P_POLES: return kn.pol >= 16u ? 1u : 0u;
    case P_ACCON: return kn.wave >> 1 & 1u;                     /* 1 = accents off */
    default: return 0;
    }
}
static void pad_set(uint32_t p, uint32_t l)                     /* a switch to a given setting */
{
    static const uint8_t MUFV[3] = {0, 10, 22}, SWPV[3] = {3, 12, 28};
    switch (p) {
    case P_WAVE: kn.wave = (uint8_t)((kn.wave & ~1u) | (l & 1u)); break;
    case P_SUB: kn.sub = l ? 24u : 0u; break;
    case P_MUF: kn.muf = MUFV[l % 3u]; break;
    case P_SWEEP: kn.swp = SWPV[l % 3u]; break;
    case P_HIRES: kn.hir = l ? 31u : 0u; break;
    case P_POLES: kn.pol = l ? 31u : 0u; break;
    case P_ACCON: kn.wave = (uint8_t)((kn.wave & ~2u) | (l & 1u) << 1); break;
    default: break;
    }
}

/* ---- the knobs: five pages of four, in the order of the TD-3-MO's panel (the filter first, then what each note does, then the extras) */
#define PAGES 6u
#define OFS(f) ((uint8_t)__builtin_offsetof(knobs_t, f))
enum { K_TEMPO = 250, K_LEN = 251, K_LEVEL = 252, K_NONE = 255, K_SW = 200 };      /* K_SW + a switch pad: a stepped knob */
static const char *const KNAME[PAGES][4] = {
    {"CUTOFF", "RESON", "ENVMOD", "DECAY"}, {"TUNE", "ACCENT", "SLIDE", "TEMPO"}, {"DRIVE", "ACC DEC", "TRACK", "ATTACK"},
    {"FILT FM", "SWING", "LENGTH", "LEVEL"}, {"WAVE", "SUB OSC", "MUFFLE", "SWEEP"}, {"HI RES", "POLES", "ACCENT", ""}};
static const uint8_t KP[PAGES][4] = {
    {OFS(cut), OFS(res), OFS(env), OFS(dec)}, {OFS(tun), OFS(acc), OFS(sld), K_TEMPO}, {OFS(drv), OFS(adc), OFS(trk), OFS(atk)},
    {OFS(fm), OFS(swg), K_LEN, K_LEVEL},
    {K_SW + P_WAVE, K_SW + P_SUB, K_SW + P_MUF, K_SW + P_SWEEP}, {K_SW + P_HIRES, K_SW + P_POLES, K_SW + P_ACCON, K_NONE}};
static uint32_t knob_val(uint32_t pg, uint32_t k, uint32_t *vmax)
{
    uint8_t o = KP[pg][k];
    *vmax = KMAX;
    if (o == K_TEMPO) { *vmax = 140; return seq_bpm - 60u; }
    if (o == K_LEN) { *vmax = 16; return plen[pat_idx] ? plen[pat_idx] : STEPS; }
    if (o >= K_SW && o < K_SW + PADS_USED) { *vmax = o == K_SW + P_MUF || o == K_SW + P_SWEEP ? 2u : 1u; return o == K_SW + P_ACCON ? 1u - pad_level(P_ACCON) : pad_level(o - K_SW); }      /* (the accent switch: up is on) */
    if (o == K_LEVEL) { *vmax = 31; return (uint32_t)(24 + kn.lvl); }
    if (o == K_NONE) return 0;
    return ((uint8_t *)&kn)[o];
}
static void knob_turn(uint32_t pg, uint32_t k, int32_t e)
{
    uint8_t o = KP[pg][k];
    if (o == K_TEMPO) { if (!ext_on) { int32_t v = (int32_t)seq_bpm + e; seq_bpm = (uint32_t)(v < 60 ? 60 : v > 200 ? 200 : v); } }
    else if (o >= K_SW && o < K_SW + PADS_USED) { uint32_t p = o - K_SW, mx = p == P_MUF || p == P_SWEEP ? 2u : 1u, l = p == P_ACCON ? 1u - pad_level(p) : pad_level(p); int32_t v = (int32_t)l + (e < 0 ? -1 : 1); v = v < 0 ? 0 : v > (int32_t)mx ? (int32_t)mx : v; pad_set(p, p == P_ACCON ? 1u - (uint32_t)v : (uint32_t)v); }
    else if (o == K_LEVEL) { int32_t v = 24 + (int32_t)kn.lvl + e; kn.lvl = (int8_t)((v < 0 ? 0 : v > 31 ? 31 : v) - 24); }
    else if (o == K_NONE) { }
    else if (o == K_LEN) { int32_t v = (int32_t)(plen[pat_idx] ? plen[pat_idx] : STEPS) + e; plen[pat_idx] = (uint8_t)(v < 1 ? 1 : v > 16 ? 16 : v); }
    else { uint8_t *p = &((uint8_t *)&kn)[o]; int32_t v = (int32_t)*p + e; *p = (uint8_t)(v < 0 ? 0 : v > KMAX ? KMAX : v); }
}

static void fx_apply(uint32_t env_held, uint32_t play_view)     /* ENV held in PLAY is the accent button */
{
    fx_acc = play_view && env_held;
}

/* ---- the random weirdness: now and then (the main loop picks) the background changes, the header smiley pulls a face, the picture glitches or swaps
 * its colours, something crosses the screen */
static uint32_t gag_kind, gag_ms, glitch_ms, swap_ms, swap_kind, crit_kind, crit_ms;
#define GAG_MS 1500u
#define GLITCH_MS 240u
#define CRIT_MS 4200u
static uint32_t weird_active(void)
{
    return (gag_kind && fm1_ms - gag_ms < GAG_MS) || fm1_ms - glitch_ms < GLITCH_MS || fm1_ms - swap_ms < (swap_kind == 2u ? 260u : 2600u) || (crit_kind && fm1_ms - crit_ms < CRIT_MS);
}
static void weird_pick(void)                                    /* one random thing */
{
    uint32_t r = rnd() % 100u;
    if (r < 46u) bg_style = bg_style ? 0u : 1u;                  /* the checkerboard, or the smiley wallpaper */
    else if (r < 66u) { gag_kind = 1u + rnd() % 4u; gag_ms = fm1_ms; }
    else if (r < 78u) glitch_ms = fm1_ms;
    else if (r < 86u) { swap_kind = 1u + (rnd() % 4u == 0u); swap_ms = fm1_ms; }
    else { crit_kind = 1u; crit_ms = fm1_ms; }
}
static void critter(void)                                       /* a saucer crossing the screen */
{
    uint32_t age = fm1_ms - crit_ms;
    int32_t x = 250 - (int32_t)age * 300 / (int32_t)CRIT_MS, y = 70 + sina(age / 45u) * 6 / 127;
    if (!crit_kind || age >= CRIT_MS) return;
    ellipse(x, y, 22, 6, C_CY);
    disc(x, y - 5, 8, C_LIME);
    cv_rect(x - 14, y - 1, 3, 3, C_PINK); cv_rect(x - 2, y - 1, 3, 3, C_Y); cv_rect(x + 10, y - 1, 3, 3, C_PINK);
    cv_rect(x - 4, y - 8, 2, 3, C_K); cv_rect(x + 2, y - 8, 2, 3, C_K);
}
static void fx_post(uint32_t y0, uint32_t h)                    /* what happens to the picture after it is drawn: a glitch, a colour swap */
{
    uint32_t i, y, n = 240u * h;
    if (fm1_ms - glitch_ms < GLITCH_MS) {                       /* bands of rows slide sideways */
        uint32_t seed = (fm1_ms / 50u) * 2654435761u;
        uint16_t tmp[240];
        for (y = 0; y + 4u <= h; y += 4u) {
            uint32_t hh = (seed ^ ((y0 + y) * 40503u)) * 2246822519u, rr;
            int32_t off;
            if ((hh >> 8) % 4u) continue;
            off = (int32_t)((hh >> 16) % 61u) - 30;
            for (rr = 0; rr < 4u; rr++) {
                uint16_t *row = cv_px + (y + rr) * 240u;
                for (i = 0; i < 240u; i++) tmp[(i + 240u + (uint32_t)off) % 240u] = row[i];
                for (i = 0; i < 240u; i++) row[i] = tmp[i];
            }
        }
    }
    if (swap_kind == 1u && fm1_ms - swap_ms < 2600u) {          /* red and blue trade places */
        for (i = 0; i < n; i++) { uint32_t c = swap16(cv_px[i]); cv_px[i] = swap16(((c & 31u) << 11) | (c & 0x7E0u) | (c >> 11)); }
    } else if (swap_kind == 2u && fm1_ms - swap_ms < 260u) {    /* a flash of the negative */
        for (i = 0; i < n; i++) cv_px[i] = (uint16_t)~cv_px[i];
    }
}

/* ---- the silly bits */
static uint32_t idle_ms(void) { return seq_run ? 0u : fm1_ms - act_ms; }           /* how long nobody has touched it (while it plays it is not idle) */
static uint32_t pat_target(void)                                /* the pattern PRESETS is heading for: the last one cued, else the one playing */
{
    return cue_w != cue_r ? cue_q[(cue_w - 1u) % CUEQ] : pat_idx;
}
static int32_t shock_bump(void) { uint32_t age = fm1_ms - shock_ms; return age < 500u ? (int32_t)(500u - age) / 25 : 0; }       /* an accent: the floor jolts */
static int32_t idle_bump(void) { uint32_t i = idle_ms(); return i > 8000u ? (int32_t)((i - 8000u) / 400u > 30u ? 30u : (i - 8000u) / 400u) : 0; }     /* left alone: the floor goes wild */
static void saucer(void)                                        /* left alone for a long time: a saucer goes by */
{
    uint32_t i = idle_ms();
    int32_t x, y;
    if (i < 25000u) return;
    x = (int32_t)((i - 25000u) / 14u % 420u) - 90;
    y = 70 + sina(i / 40u) * 10 / 127;
    ellipse(x, y, 22, 6, C_CY);
    disc(x, y - 5, 8, C_LIME);
    cv_rect(x - 14, y - 1, 3, 3, C_PINK); cv_rect(x - 2, y - 1, 3, 3, C_Y); cv_rect(x + 10, y - 1, 3, 3, C_PINK);
    cv_rect(x - 4, y - 8, 2, 3, C_K); cv_rect(x + 2, y - 8, 2, 3, C_K);
}

/* ---- the bars */
static void scene_low(void)
{
    uint32_t k;
    int32_t y;
    cv_rect(0, (int32_t)Y_LOW - 4, 240, 64, C_K);
    for (k = 0; k < 4u; k++) {
        int32_t cx = 30 + (int32_t)k * 60;
        uint16_t col = k == 0u ? C_LIME : k == 1u ? C_PINK : k == 2u ? C_CY : C_OR;
        uint32_t vmax, v = knob_val(page, k, &vmax);
        if (KP[page][k] == K_NONE) continue;
        if (KP[page][k] == K_SW + P_WAVE) wave_icon(cx, (int32_t)Y_LOW + 12, 14, kn.wave & 1u, col);       /* the wave itself */
        else if (KP[page][k] >= K_SW && KP[page][k] < K_SW + PADS_USED) toggle(cx, (int32_t)Y_LOW + 12, v, vmax + 1u, col);       /* a switch */
        else dial(cx, (int32_t)Y_LOW + 12, 14, v, vmax, col);
        text_mid(cx, (int32_t)Y_LOW + 25, KNAME[page][k], C_Y);
    }
    for (k = 0; k < PAGES; k++) cv_rect(236, (int32_t)Y_LOW + 2 + (int32_t)k * 5, 4, 3, k == page ? C_Y : C_RING);
    for (y = 0; y < (int32_t)VIEWS; y++) {                      /* tabs */
        static const char *const NM[VIEWS] = {"STEPS", "FLY", "WALK", "PLAY"};
        static const uint16_t TC[VIEWS] = {C_Y, C_LIME, C_OR, C_CY};
        int32_t x = y * 60;
        int32_t on = (uint32_t)y == view_now;
        cv_rect(x, 218, 60, 22, on ? TC[y] : C_K);
        cv_rect(x, 218, 60, 2, TC[y]);
        text_mid(x + 30, 221, NM[y], on ? C_K : TC[y]);
    }
    if (seq_run) cv_rect((int32_t)(seq_step % 16u) * 15, 218, 30, 2, C_W);                       /* a light chases along the tabs with the steps */
}

static void scene_head(void)
{
    char s[8];
    uint32_t now = seq_run ? seq_step : cur, nt = PAT[now % STEPS].note, ac = nt && (PAT[now % STEPS].fl & F_ACC);
    cv_rect(0, 0, 240, (int32_t)Y_HDR, C_K);
    smiley_g(23, 20, 16, ac ? 5 : 0, seq_run ? (int32_t)sina(seq_step * 4u) / 40 : 0, gag_kind && fm1_ms - gag_ms < GAG_MS ? gag_kind : 0u, (int32_t)sina((fm1_ms - gag_ms) * 32u / GAG_MS) * 100 / 127);
    utoa3(s, pat_target() + 1u);                                /* the pattern, in a tag; pink and the number of the one you are heading for while another is cued */
    cv_rect(50, 9, 30, 24, cue_w != cue_r ? C_PINK : C_CY);
    cv_rect(52, 11, 26, 20, C_K);
    text_mid(65, 13, s, cue_w != cue_r ? C_PINK : C_CY);
    note_name(s, nt);
    if (s[0]) cv_text(92, 6, &FONT_L, s, ac ? C_PINK : C_Y);
    utoa3(s, seq_bpm);
    cv_text(236 - text_w(&FONT_L, s), 6, &FONT_L, s, ext_on ? C_CY : C_Y);
    cv_rect(0, (int32_t)Y_HDR - 3, 240, 3, C_Y);
}

static void scene_steps(void)
{
    uint32_t i, len = plen[pat_idx] ? plen[pat_idx] : STEPS;
    floor_rows((int32_t)Y_HDR, (int32_t)Y_LOW - 4, 6 + env_view / 1800 + shock_bump() + idle_bump(), anim_t + (idle_ms() > 8000u ? anim_t * 2u : 0u));
    saucer();
    critter();
    for (i = 0; i < STEPS; i++) {
        int32_t x = 4 + (int32_t)i * 29 / 2, base = 150, h, hop = 0;
        const step_t s = PAT[i];
        uint32_t on = seq_run && seq_step == i, ac = s.fl & F_ACC;
        uint16_t c = on ? C_Y : ac ? C_PINK : C_LIME;
        if (i >= len) { cv_rect(x + 4, base - 3, 4, 3, C_RING); mini(x + 6, 162, 0u); continue; }
        if (s.note) {
            h = ((int32_t)s.note - 33) * 3;
            h = h < 8 ? 8 : h > 100 ? 100 : h;
            cv_rect(x, base - h, 12, h, c);
            cv_rect(x + 2, base - h + 2, 3, h - 4, on ? C_K : ac ? RGB(255, 140, 210) : RGB(210, 255, 120));
            if (ac) cv_rect(x - 1, base - h - 5, 14, 3, C_Y);
            if (s.fl & F_SLD) {                                 /* a slide: a bridge to the next note's top */
                const step_t n = PAT[(i + 1u) % STEPS];
                if (n.note) {
                    int32_t h2 = ((int32_t)n.note - 33) * 3, d;
                    h2 = h2 < 8 ? 8 : h2 > 100 ? 100 : h2;
                    for (d = 0; d < 3; d++) cv_line(x + 12, base - h + d, x + 14, base - h2 + d, C_CY);
                }
            }
        } else cv_rect(x + 4, base - 3, 4, 3, C_RING);
        if (on) hop = sina((uint32_t)(sq_acc * 32u / 661500u)) * 6 / 127;                                    /* the playing step hops */
        else if (idle_ms() > 10000u) hop = sina(anim_t * 2u + i * 4u) * 3 / 127;                              /* left alone: the smileys do a wave */
        mini(x + 6, 162 - hop, on ? 3u : !s.note ? 0u : ac ? 2u : 1u);
    }
    {
        int32_t x = 4 + (int32_t)cur * 29 / 2;
        cv_rect(x - 1, 170, 14, 2, C_Y);
        cv_rect(x - 1, 168, 2, 3, C_Y); cv_rect(x + 11, 168, 2, 3, C_Y);
    }
}

/* ---- a random pattern (the TD-3-mo's trick: it might rule, it might not), a nudge, one level of undo */
static void random_pattern(void)
{
    static const uint8_t SC[10] = {0, 0, 3, 5, 7, 7, 10, 12, 12, 15};
    uint32_t i;
    for (i = 0; i < STEPS; i++) {
        step_t *s = &PAT[i];
        s->note = rnd() % 4u ? (uint8_t)(36u + SC[rnd() % 10u] + (rnd() % 5u == 0u ? 12u : 0u)) : 0;
        s->fl = (uint8_t)(s->note ? ((rnd() % 4u == 0u ? F_ACC : 0) | (rnd() % 4u == 0u ? F_SLD : 0)) : 0);
    }
}
static void mutate(void)                                        /* nudge a few steps: a rest wakes up, a note moves an octave or goes quiet, an accent or a slide flips */
{
    static const uint8_t SC[5] = {0, 3, 7, 10, 12};
    uint32_t n;
    for (n = 0; n < 3u; n++) {
        step_t *s = &PAT[rnd() % STEPS];
        uint32_t r = rnd() % 5u;
        if (!s->note) { if (r < 3u) { s->note = (uint8_t)(36u + SC[rnd() % 5u]); s->fl = 0; } }
        else if (r == 0u) { s->note = 0; s->fl = 0; }
        else if (r == 1u) s->note = (uint8_t)(s->note >= 48u ? s->note - 12u : s->note + 12u);
        else if (r == 2u) s->fl ^= F_ACC;
        else if (r == 3u) s->fl ^= F_SLD;
        else s->note = (uint8_t)(s->note + (rnd() & 1u ? 2u : -2u));
    }
}
#define FLY_ROOT 36
static step_t undo_pat[STEPS];
static uint32_t undo_has, undo_len;
static void undo_save(void) { uint32_t i; for (i = 0; i < STEPS; i++) undo_pat[i] = PAT[i]; undo_len = plen[pat_idx]; undo_has = 1; }
static void undo_swap(void)
{
    uint32_t i, l = plen[pat_idx];
    for (i = 0; i < STEPS; i++) { step_t t = PAT[i]; PAT[i] = undo_pat[i]; undo_pat[i] = t; }
    plen[pat_idx] = (uint8_t)undo_len; undo_len = l;
}

static const uint8_t LADDER[15] = {0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24, 27, 29, 31, 34};     /* a minor pentatonic, three octaves */

/* the pattern wraps: every step moves along (forwards d = 1: the last one comes round to the first), within the pattern's length */
static void pat_rotate(int32_t d)
{
    uint32_t len = plen[pat_idx] ? plen[pat_idx] : STEPS, i;
    step_t t[STEPS];
    for (i = 0; i < len; i++) t[(i + len + (uint32_t)(d % (int32_t)len)) % len] = PAT[i];
    for (i = 0; i < len; i++) PAT[i] = t[i];
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
        foe_deg[i] = (uint8_t)(rnd() % 8u * 2u);                 /* (even degrees: they stand in a lane in WALK too) */
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
    if (!slide_held && fm1_ms - fired_ms[prev] < 2u * step_ms && PAT[prev].note && PAT[prev].note != nt && !(fired_ms[prev] == 0u))
        PAT[prev].fl |= F_SLD;                                  /* neighbours that change pitch slide */
    PAT[st].note = (uint8_t)nt;
    PAT[st].fl = (uint8_t)(acc ? F_ACC : 0);
    fired_ms[st] = fm1_ms ? fm1_ms : 1u;
    fly_bolt_ms = fm1_ms;
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
        const step_t s = PAT[si];
        cv_rect(x - COL_W / 2, 50, 1, 118, si % 4 == 0 ? RGB(90, 70, 130) : RGB(40, 30, 60));
        if (s.note) {
            y = note_y(s.note);
            disc(x, y, 5, s.fl & F_ACC ? C_PINK : C_LIME);
            if (s.fl & F_SLD) { const step_t n = PAT[(si + 1) % (int32_t)STEPS]; if (n.note) { int32_t d; for (d = 0; d < 3; d++) cv_line(x + 5, y + d - 1, x + COL_W - 5, note_y(n.note) + d - 1, C_CY); } }
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
    smiley(SHIP_X, ship_y >> 4, 10, fm1_ms - fly_bolt_ms < 140u ? 4 : 0, 2);
}


/* ---- WALK: a first person corridor of 8 lanes, a note each, low on the left. The steps come at you down it; SELECT steps between lanes, a key shoots. */
static int32_t walk_lane = 3, walk_x = 3 << 4;
static uint32_t note_lane(uint32_t n)                           /* the lane (the even scale degrees) nearest a note */
{
    uint32_t i, best = 0, bd = 99, d;
    for (i = 0; i < 8u; i++) { d = (uint32_t)((int32_t)LADDER[i * 2u] + FLY_ROOT > (int32_t)n ? (int32_t)LADDER[i * 2u] + FLY_ROOT - (int32_t)n : (int32_t)n - FLY_ROOT - (int32_t)LADDER[i * 2u]); if (d < bd) { bd = d; best = i; } }
    return best;
}
#define WHZ 86                                                  /* the horizon, and the camera's height and focal length */
#define WCAM 40
#define WF 150
static void walk_orb(int32_t lane, int32_t dz, int32_t px, uint16_t col, int32_t *ox, int32_t *oy)
{
    int32_t r = 9 * WF / dz;
    r = r < 2 ? 2 : r > 18 ? 18 : r;
    *ox = 120 + (lane * 20 - 70 - px) * WF / dz;
    *oy = WHZ + (WCAM - 14) * WF / dz;
    disc(*ox, *oy, r, col);
}
static void scene_walk(void)
{
    int32_t y, k, i, len = plen[pat_idx] ? plen[pat_idx] : (int32_t)STEPS, pos = seq_run ? (int32_t)seq_step : (int32_t)cur, fr = seq_run ? (int32_t)(sq_acc * 256u / 661500u) : 0;
    int32_t px = walk_x * 20 / 16 - 70 + 70 - 0, travelled = pos * 40 + fr * 40 / 256, sx, sy;
    static const uint16_t FA[2] = {RGB(40, 44, 0), RGB(18, 20, 0)}, WA[2] = {RGB(56, 0, 46), RGB(26, 0, 22)}, CA[2] = {RGB(0, 40, 54), RGB(0, 18, 26)};
    px = walk_x * 20 / 16 - 70;                                 /* the camera's world x: the middle of the lane you stand in */
    for (y = (int32_t)Y_HDR; y < (int32_t)Y_LOW - 4; y++) {
        int32_t dy = y - WHZ, dz, par, l, xl, xr;
        if (dy == 0) continue;
        dz = (dy > 0 ? WCAM : 90 - WCAM) * WF / (dy > 0 ? dy : -dy);
        if (dz > 900) continue;
        par = ((dz + travelled) / 40) & 1;
        xl = 120 + (-80 - px) * WF / dz; xr = 120 + (80 - px) * WF / dz;
        cv_rect(0, y, xl > 0 ? xl : 0, 1, WA[par]);                                 /* the walls */
        if (xr < 240) cv_rect(xr, y, 240 - xr, 1, WA[par ^ 1]);
        for (l = 0; l < 8; l++) {
            int32_t x0 = 120 + (l * 20 - 80 - px) * WF / dz, x1 = 120 + (l * 20 - 60 - px) * WF / dz;
            uint16_t c = dy > 0 ? FA[(par + l) & 1] : CA[(par + l) & 1];
            if (dy > 0 && l == walk_lane) c = (par & 1) ? RGB(90, 84, 0) : RGB(56, 52, 0);                  /* your lane glows */
            cv_rect(x0, y, x1 - x0 + 1, 1, c);
        }
    }
    for (k = 9; k >= 0; k--) {                                  /* the steps coming, far ones first */
        int32_t si = (pos + k) % len, dz = k * 40 + 40 - fr * 40 / 256 + 10, lane;
        const step_t s = PAT[si];
        if (!s.note) continue;
        lane = (int32_t)note_lane(s.note);
        walk_orb(lane, dz, px, s.fl & F_ACC ? C_PINK : C_LIME, &sx, &sy);
        if (s.fl & F_SLD) { const step_t n = PAT[(si + 1) % len]; if (n.note) { int32_t nx, ny, r0 = 0; (void)r0; nx = 120 + ((int32_t)note_lane(n.note) * 20 - 70 - px) * WF / (dz + 40); ny = WHZ + (WCAM - 14) * WF / (dz + 40); cv_line(sx, sy, nx, ny, C_CY); cv_line(sx, sy + 1, nx, ny + 1, C_CY); } }
    }
    for (i = 0; i < (int32_t)FOES; i++) {                       /* monsters, in their lanes */
        int32_t dk = ((int32_t)foe_step[i] - pos + 16) % 16, dz = dk * 40 + 50 - fr * 40 / 256, x, yy, r;
        if (!(foe_live >> i & 1u) || dk > 9) continue;
        walk_orb((int32_t)foe_deg[i] / 2, dz, px, C_OR, &x, &yy);
        r = 10 * WF / dz; r = r < 3 ? 3 : r > 18 ? 18 : r;
        cv_rect(x - r / 2 - 1, yy - r / 3, r / 3 + 1, r / 3 + 1, C_K); cv_rect(x + r / 3, yy - r / 3, r / 3 + 1, r / 3 + 1, C_K);
        cv_rect(x - r / 2, yy + r / 3, r + 1, r / 4 + 1, C_K);
    }
    if (fm1_ms - fly_bolt_ms < 140u) { cv_line(120, 166, 120, WHZ, C_LIME); cv_line(119, 166, 119, WHZ, C_Y); cv_line(121, 166, 121, WHZ, C_Y); }
    if (fm1_ms - fly_boom_ms < 300u) {
        int32_t r = (int32_t)(fm1_ms - fly_boom_ms) / 14 + 6, j;
        for (j = 0; j < 12; j++) cv_rect(120 + cosa((uint32_t)j * 5u) * r / 127 - 1, 120 + sina((uint32_t)j * 5u) * r / 127 - 1, 3, 3, j & 1 ? C_PINK : C_Y);
    }
    smiley(120, 160, 9, fm1_ms - fly_bolt_ms < 140u ? 4 : 0, 0);           /* you */
}

/* ---- PLAY: the 16 patterns as pads, two rows of eight like the keys under them. A key cues a pattern; the chain plays in the order you pressed. */
static void tile_bars(int32_t x, int32_t y, int32_t w, int32_t h, const step_t *p, uint16_t c, uint16_t ca)
{
    uint32_t k;
    for (k = 0; k < STEPS; k++) {
        int32_t bh = ((int32_t)p[k].note - 33) * h / 34;
        if (!p[k].note) continue;
        bh = bh < 2 ? 2 : bh > h ? h : bh;
        cv_rect(x + (int32_t)k * w / 16, y + h - bh, w / 16 > 1 ? w / 16 - 1 : 1, bh, p[k].fl & F_ACC ? ca : c);
    }
}
static void scene_play(void)
{
    uint32_t i, q;
    char nm[4];
    floor_rows((int32_t)Y_HDR, (int32_t)Y_LOW - 4, 3 + env_view / 2500 + shock_bump() / 2 + idle_bump(), anim_t);
    critter();
    for (i = 0; i < PATS; i++) {
        int32_t x = 4 + (int32_t)(i % 8u) * 29, y = 50 + (int32_t)(i / 8u) * 60;
        uint32_t queued = 0, pos = 0, len = plen[i] ? plen[i] : STEPS, any = 0, k;
        uint16_t fr = i == pat_idx ? C_Y : C_RING;
        for (k = 0; k < STEPS; k++) if (pats[i][k].note) any = 1;
        for (q = cue_r; q != cue_w; q++) if (cue_q[q % CUEQ] == i && !queued) queued = q - cue_r + 1u;
        if (queued) fr = C_PINK;
        cv_rect(x, y, 28, 56, fr);
        cv_rect(x + 2, y + 2, 24, 52, i == pat_idx ? RGB(34, 30, 0) : C_K);
        utoa3(nm, i + 1u);
        text_at(x + 3, y + 3, nm, i == pat_idx ? C_Y : any ? C_W : C_RING);
        if (queued) { nm[0] = (char)('0' + queued); nm[1] = 0; cv_rect(x + 17, y + 3, 9, 15, C_PINK); text_at(x + 18, y + 3, nm, C_K); }
        if (any) tile_bars(x + 3, y + 20, 24, 32, pats[i], i == pat_idx ? C_Y : C_LIME, C_PINK);
        if (i == pat_idx && seq_run) { pos = seq_step < len ? seq_step : 0u; cv_rect(x + 3 + (int32_t)pos * 24 / 16, y + 20, 1, 32, C_W); }
        if (i == pat_idx) cv_rect(x + 3, y + 53, 24 * (int32_t)len / 16, 1, C_Y);
    }
}

/* ---- the projects */
static void slot_load(uint32_t i)
{
    const slot_t *sl = slot_get(i);
    uint32_t a, b;
    for (a = 0; a < PATS; a++) { for (b = 0; b < STEPS; b++) pats[a][b] = sl->pats[a][b]; plen[a] = sl->plen[a] ? sl->plen[a] : STEPS; }
    kn = sl->kn; seq_bpm = sl->bpm < 60u || sl->bpm > 200u ? 126u : sl->bpm;
    pat_idx = sl->pcur < PATS ? sl->pcur : 0u; cue_clear(); slot_cur = i; cur = 0;
}
static void slot_save(void)
{
    slot_t *sl = &bank.s[slot_cur];
    uint32_t a, b;
    for (a = 0; a < PATS; a++) { for (b = 0; b < STEPS; b++) sl->pats[a][b] = pats[a][b]; sl->plen[a] = plen[a]; }
    sl->kn = kn; sl->bpm = (uint8_t)seq_bpm; sl->pcur = (uint8_t)pat_idx; sl->used = 1;
    bank_flush();
}
static void scene_pick(void)
{
    uint32_t i;
    char nm[4];
    cv_rect(0, 0, 240, 240, C_K);
    for (i = 0; i < SLOTS; i++) {
        int32_t x = 6 + (int32_t)(i % 3u) * 79, y = 6 + (int32_t)(i / 3u) * 52;
        const slot_t *sl = slot_get(i);
        uint16_t fr = i == pick_sel ? C_Y : C_RING;
        uint32_t n = 0, a;
        cv_rect(x, y, 74, 46, fr);
        cv_rect(x + 2, y + 2, 70, 42, i == pick_sel ? RGB(30, 28, 0) : C_K);
        if (i == pick_sel) cv_rect(x + 2, y + 2, 70, 3, C_Y);
        tile_bars(x + 5, y + 16, 64, 26, sl->pats[sl->pcur < PATS ? sl->pcur : 0], C_LIME, C_PINK);
        for (a = 0; a < PATS; a++) { uint32_t k; for (k = 0; k < STEPS; k++) if (sl->pats[a][k].note) { n++; break; } }
        utoa3(nm, i + 1u);
        text_at(x + 5, y + 4, nm, i == pick_sel ? C_Y : C_RING);
        for (a = 0; a < n && a < 16u; a++) cv_rect(x + 30 + (int32_t)(a % 8u) * 4, y + 7 + (int32_t)(a / 8u) * 4, 3, 3, C_CY);      /* how many patterns have something in them */
        if (!sl->used) cv_rect(x + 62, y + 8, 4, 4, C_RING);
        if (i == slot_cur) mini(x + 63, y + 16, 3u);
    }
    text_at(236 - text_w(&FONT_S, "HOME  Open"), 222, "HOME  Open", C_Y);
}
