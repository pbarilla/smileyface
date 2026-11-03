/* SPDX-License-Identifier: GPL-3.0-only */
/* The FX rack: four slots after the bass. Each holds a unit of some kind (Delay, Drive, Crush), of some type (a digital, tape or dub delay...),
 * with an input (the bass, or a unit before it) and a switch to the output. The dub delay is inspired by Korg's monotron Delay.
 * Runs in the audio interrupt; integers only. Included by smileyface.c after voice.c. */
enum { FX_NONE, FX_DELAY, FX_DRIVE, FX_CRUSH, FX_KINDS };
enum { DL_DIGITAL, DL_TAPE, DL_DUB };                   /* the types of each kind (a unit's `sub`) */
enum { DR_FOLD };
enum { CR_BITS, CR_LOFI };
#define FXN 4u
#define FXU 2u                                         /* the slots in use: two, one after the other (the data has four) */
#define FX_VER 4u                                       /* a rack saved with other kinds is emptied */
typedef struct { uint8_t type, in, bus, sub; uint8_t p[8]; } fxu_t;        /* in: 0 the bass, n the unit in slot n - 1 (an earlier one); bus: 1 = it goes to the output */
typedef struct { uint8_t on, ver, pad[2]; fxu_t u[FXN]; } fxr_t;           /* 52 bytes */
static fxr_t rack;
static const uint8_t FX_SUBS[FX_KINDS] = {1, 3, 1, 2};

#define DL_N 24576u                                     /* the delay line: 1.1 s at 22.05 kHz */
static int16_t dl_buf[FXN][DL_N] __attribute__((section(".pool")));
typedef struct { int32_t s1, s2, lp, w0, w1, acc, dly, wp, amp, dcs; uint32_t tog, ph1, ph2; int32_t g, c1, d; } fxs_t;
static fxs_t fxs[FXN];
static int32_t fx_odc;                                  /* the rack's DC blocker: a howling loop into a fold can lean to one side */
static volatile uint32_t fx_rst;                        /* slots whose audio state the main loop wants cleared */

static const char *const FX_NAME[FX_KINDS] = {"", "DELAY", "DRIVE", "CRUSH"};
static const uint8_t FX_DEF[FX_KINDS][4][8] = {         /* each type's starting knobs */
    {{0}},
    {{12, 10, 22, 24}, {12, 12, 14, 8}, {12, 10, 22, 24}},     /* digital: TIME FEEDBK MIX TONE; tape: TIME FEEDBK MIX WOW; dub: TIME FEEDBK CUTOFF LEVEL */
    {{7, 24, 31, 22}},      /* DRIVE TONE MIX LEVEL */
    {{22, 12, 31, 24}, {10, 20, 31, 24}},                      /* bits: BITS RATE MIX LEVEL; lofi: BAND BITS MIX LEVEL */
};
static const uint8_t FX_DIV[8] = {1, 2, 3, 4, 6, 8, 12, 16};   /* the digital delay's TIME, in half steps: 1/32 1/16 dotted 1/16 1/8 ... 1/2 */

static int32_t mul14(int32_t a, int32_t c) { return (a >> 14) * c + (((a & 16383) * c) >> 14); }
static void svf_set(fxs_t *s, uint32_t idx, int32_t r2)         /* cutoff 0..31, damping 2R in Q14 */
{
    int32_t g = FXG[idx & 31u], den;
    s->g = g;
    s->c1 = r2 + g;
    den = 16384 + mul14(r2, g) + mul14(g, g);
    s->d = (int32_t)((1u << 28) / (uint32_t)den);
}
static int32_t svf_lp(fxs_t *s, int32_t x)                      /* a two pole low pass, zero delay feedback; the band state is squashed (the MS-20's diodes) */
{
    int32_t hp = mul14(x - mul14(s->s1, s->c1) - s->s2, s->d), gh = mul14(hp, s->g), bp = soft(gh + s->s1), gb = mul14(bp, s->g), lp = gb + s->s2;
    s->s1 = gh + bp;
    s->s2 = gb + lp;
    if (lp > 60000) lp = 60000;
    if (lp < -60000) lp = -60000;
    return lp;
}
/* Delay, as in the monotron: what comes in and the echo go through the filter, the filter's output is what you hear and what feeds the line, and
 * the line comes back in front of the filter. So every repeat is filtered again, FEEDBK is how much comes back (past half it howls), and with
 * none there is no echo. The line runs at half rate (a lo-fi echo); TIME is smoothed so a turn bends the repeats */
static int32_t fx_dub(fxs_t *s, int16_t *buf, int32_t x, int32_t tgt, int32_t fbg, uint32_t cut)
{
    int32_t wet = s->tog == 0u ? (s->w0 + s->w1) >> 1 : s->w1, y = soft(svf_lp(s, x + mulq(wet, fbg)));
    if (s->tog == 0u) s->acc = y, s->tog = 1u;
    else {
        int32_t in22 = (s->acc + y) >> 1, dd = (tgt - s->dly) >> 11, di, fr, i0, i1, a, b, w;
        s->tog = 0u;
        if (dd == 0) dd = (tgt > s->dly) - (tgt < s->dly);
        s->dly += dd;
        di = s->dly >> 8; fr = s->dly & 255;
        i0 = s->wp - di; if (i0 < 0) i0 += (int32_t)DL_N;
        i1 = i0 - 1; if (i1 < 0) i1 += (int32_t)DL_N;
        a = buf[i0]; b = buf[i1];
        w = a + (((b - a) * fr) >> 8);
        s->lp += mulq(w - s->lp, FXA[cut]);                     /* the chip's own roll off: the line is dull */
        s->dcs += (in22 * 256 - s->dcs) >> 8;                  /* a DC blocker (Q8, so it settles to nothing): the clipping in a runaway loop is lopsided */
        buf[s->wp] = (int16_t)(in22 - (s->dcs >> 8));
        if (++s->wp >= (int32_t)DL_N) s->wp = 0;
        s->w0 = s->w1; s->w1 = s->lp;
    }
    return y;
}

/* An echo with the dry bass left alone and the echoes added (digital: clean, clipped hard; tape: soft clipped, dull, and the speed drifts) */
static int32_t fx_echo(fxs_t *s, int16_t *buf, int32_t x, int32_t tgt, int32_t fbg, int32_t wetg, uint32_t tone, int32_t wow, uint32_t tape)
{
    int32_t wet = s->tog == 0u ? (s->w0 + s->w1) >> 1 : s->w1;
    if (wow) { s->ph1 += 107000u; s->ph2 += 681000u; }
    if (s->tog == 0u) { s->acc = x; s->tog = 1u; }
    else {
        int32_t in22 = (s->acc + x) >> 1, dd, di, fr, i0, i1, a, b, w, f, t2 = tgt;
        s->tog = 0u;
        if (wow) t2 += (SIN64[s->ph1 >> 26] * 3 + SIN64[s->ph2 >> 26]) * wow / 508;
        if (t2 < 256) t2 = 256;
        dd = (t2 - s->dly) >> 11;
        if (dd == 0) dd = (t2 > s->dly) - (t2 < s->dly);
        s->dly += dd;
        di = s->dly >> 8; fr = s->dly & 255;
        i0 = s->wp - di; if (i0 < 0) i0 += (int32_t)DL_N;
        i1 = i0 - 1; if (i1 < 0) i1 += (int32_t)DL_N;
        a = buf[i0]; b = buf[i1];
        w = a + (((b - a) * fr) >> 8);
        s->lp += mulq(w - s->lp, FXA[tape ? tone & 31u : 8u + (tone & 31u) * 23u / 31u]);
        f = in22 + mulq(s->lp, fbg);
        f = tape ? soft(f) : f > 32000 ? 32000 : f < -32000 ? -32000 : f;
        s->dcs += (f * 256 - s->dcs) >> 8;
        buf[s->wp] = (int16_t)(f - (s->dcs >> 8));
        if (++s->wp >= (int32_t)DL_N) s->wp = 0;
        s->w0 = s->w1; s->w1 = s->lp;
    }
    return x + mulq(wet, wetg);
}

/* Drive: gain into a fold, a tone filter, and a mix with the dry */
static int32_t fx_shape(uint32_t sub, int32_t v)
{
    {
        uint32_t m = (uint32_t)(v + 32768) & 131071u;
        return (m < 65536u ? (int32_t)m - 32768 : 98304 - (int32_t)m) * 5 / 8;
    }
}
static int32_t fx_drive(fxs_t *s, int32_t x, uint32_t sub, int32_t gq, uint32_t tone, int32_t mixg)
{
    int32_t y = fx_shape(sub, x * gq / 16);
    s->lp += mulq(y - s->lp, FXA[4u + (tone & 31u) * 27u / 31u]);
    return mulq(x, 32768 - mixg) + mulq(s->lp, mixg);
}

/* Crush: fewer bits, a slower sample rate; lofi squeezes the band first (high pass and low pass) */
static int32_t fx_crush(fxs_t *s, int32_t x, uint32_t sub, uint32_t band, uint32_t bits, uint32_t rate, int32_t mixg)
{
    int32_t v = x, sh = 1 + (int32_t)bits * 13 / 31, y;
    if (sub == CR_LOFI) {
        s->s1 += (v - s->s1) >> (9u - band * 5u / 31u);          /* the high pass: 14 Hz up to 440 Hz */
        s->lp += mulq(v - s->s1 - s->lp, FXA[28u - band * 10u / 31u]);   /* the low pass: 5 kHz down to 1 kHz */
        v = s->lp;
    } else {
        if (s->tog == 0u) { s->acc = v; s->tog = 1u + rate * rate / 24u; }
        s->tog--;
        v = s->acc;
    }
    y = ((v + (1 << (sh - 1))) >> sh) * (1 << sh);
    y = y > 32000 ? 32000 : y < -32000 ? -32000 : y;
    return mulq(x, 32768 - mixg) + mulq(y, mixg);
}

typedef struct { int32_t tgt, fbg, wetg, gl, a, b; } fxp_t;

static void fx_process(int32_t *out, uint32_t n)
{
    uint32_t i, k;
    fxp_t fp[FXN];
    if (fx_rst) {
        for (k = 0; k < FXN; k++) if (fx_rst >> k & 1u) memset(&fxs[k], 0, sizeof fxs[k]);
        fx_rst = 0;
    }
    if (!rack.on) return;
    memset(fp, 0, sizeof fp);
    for (k = 0; k < FXN; k++) {
        const fxu_t *u = &rack.u[k];
        fxs_t *s = &fxs[k];
        fxp_t *f = &fp[k];
        int32_t p0 = (int32_t)(u->p[0] & 31u), p1 = (int32_t)(u->p[1] & 31u), p2 = (int32_t)(u->p[2] & 31u), p3 = (int32_t)(u->p[3] & 31u);
        f->gl = 32768;
        if (u->type == FX_DELAY) {
            int32_t ms = 30 + p0 * p0 * 970 / 961;              /* tape, dub: free */
            if (u->sub == DL_DIGITAL) { uint32_t bpm = seq_bpm < 40u ? 40u : seq_bpm; ms = (int32_t)(7500u * FX_DIV[p0 >> 2] / bpm); if (ms > 1090) ms = 1090; if (ms < 8) ms = 8; }
            f->tgt = ms * 5645;                                 /* samples at 22.05 kHz, Q8 */
            f->wetg = p2 * 1057;
            f->fbg = p1 * (u->sub == DL_TAPE ? 1200 : 1000);
            f->a = u->sub == DL_TAPE ? p3 * 270 : 0;           /* the wow */
            if (u->sub == DL_DUB) { f->wetg = 0; f->fbg = p1 * 2150; f->gl = p3 * 1365; svf_set(s, (uint32_t)p2, 17000); }
            if (s->dly == 0) s->dly = f->tgt;
        } else if (u->type == FX_DRIVE) {
            f->a = 16 + (p0 * p0 * 3 >> 2);
            f->wetg = p2 * 1057;
            f->gl = p3 * 1365;
        } else if (u->type == FX_CRUSH) {
            f->wetg = p2 * 1057;
            f->gl = p3 * 1365;
        }
    }
    for (i = 0; i < n; i++) {
        int32_t sig[FXN + 1u], bus = 0, y, x = out[2u * i];
        uint32_t any = 0;
        sig[0] = x;
        for (k = 0; k < FXN; k++) {
            const fxu_t *u = &rack.u[k];
            const fxp_t *f = &fp[k];
            uint32_t src = u->in <= k && (u->in == 0u || rack.u[u->in - 1u].type != FX_NONE) ? u->in : 0u;      /* an empty or later slot: the bass */
            int32_t v = sig[src];
            if (u->type == FX_DELAY) {
                if (u->sub == DL_DUB) y = fx_dub(&fxs[k], dl_buf[k], v, f->tgt, f->fbg, 27u);
                else y = fx_echo(&fxs[k], dl_buf[k], v, f->tgt, f->fbg, f->wetg, u->sub == DL_TAPE ? 26u : u->p[3], f->a, u->sub == DL_TAPE);
            }
            else if (u->type == FX_DRIVE) y = fx_drive(&fxs[k], v, u->sub, f->a, u->p[1], f->wetg);
            else if (u->type == FX_CRUSH) y = fx_crush(&fxs[k], v, u->sub, u->p[0] & 31u, u->sub == CR_LOFI ? (uint32_t)(u->p[1] & 31u) : (uint32_t)(u->p[0] & 31u), u->p[1] & 31u, f->wetg);
            else { sig[k + 1u] = 0; continue; }
            y = mulq(y, f->gl);
            sig[k + 1u] = y;
            if (u->bus) { bus += y; any = 1; }
        }
        if (any) { fx_odc += (bus * 256 - fx_odc) >> 12; y = bus - (fx_odc >> 8); }
        else y = x;
        if (y > 32000) y = 32000;
        if (y < -32000) y = -32000;
        out[2u * i] = y;
        out[2u * i + 1u] = y;
    }
}

/* ---- the main loop's side: set a slot, load and keep a rack */
static void fx_clear_line(uint32_t k) { memset(dl_buf[k], 0, sizeof dl_buf[k]); }
static void fx_defaults(fxu_t *u, uint32_t type, uint32_t sub)
{
    uint32_t j;
    for (j = 0; j < 8u; j++) u->p[j] = FX_DEF[type % FX_KINDS][sub % 4u][j];
    u->sub = (uint8_t)sub;
}
static void fx_chain(fxr_t *r)                                  /* a plain list: each unit hears the one before it, the last one is heard */
{
    uint32_t k, prev = 0, last = FXN;
    for (k = 0; k < FXN; k++) {
        fxu_t *u = &r->u[k];
        u->in = (uint8_t)prev; u->bus = 0;
        if (u->type != FX_NONE) { prev = k + 1u; last = k; }
    }
    if (last < FXN) r->u[last].bus = 1;
}
static void fx_set(uint32_t k, uint32_t type, uint32_t sub)    /* put a unit (or none) in a slot, with its starting knobs; the audio never sees it half made */
{
    fxu_t *u = &rack.u[k];
    u->type = FX_NONE;
    fx_clear_line(k);
    fx_rst |= 1u << k;
    if (type >= FX_KINDS) type = FX_NONE;
    if (sub >= FX_SUBS[type]) sub = 0;
    fx_defaults(u, type, sub);
    u->type = (uint8_t)type;
    fx_chain(&rack);
}
static uint8_t fx_keep[FXN][FX_KINDS][4][8], fx_kept[FXN][FX_KINDS][4];      /* knobs of units turned out, so turning back finds them */
static void fx_stash(uint32_t k)
{
    const fxu_t *u = &rack.u[k];
    uint32_t j;
    if (u->type == FX_NONE) return;
    for (j = 0; j < 8u; j++) fx_keep[k][u->type][u->sub][j] = u->p[j];
    fx_kept[k][u->type][u->sub] = 1;
}
static void fx_recall(uint32_t k)
{
    fxu_t *u = &rack.u[k];
    uint32_t j;
    if (u->type == FX_NONE || !fx_kept[k][u->type][u->sub]) return;
    for (j = 0; j < 8u; j++) u->p[j] = fx_keep[k][u->type][u->sub][j];
}
static void fx_morph(uint32_t k, uint32_t type, uint32_t sub) /* another unit in the same place: same wires, its own knobs */
{
    fxu_t *u = &rack.u[k];
    u->type = FX_NONE;
    fx_clear_line(k);
    fx_rst |= 1u << k;
    fx_defaults(u, type, sub);
    u->type = (uint8_t)type;
}
static void fx_unplug(uint32_t k) { fx_set(k, FX_NONE, 0); }                           /* empty a slot; the list closes up */
#define FXL_N 7u                                               /* the effects ALGORITHM walks, none first */
static const uint8_t FXL_K[FXL_N] = {FX_NONE, FX_DELAY, FX_DELAY, FX_DELAY, FX_CRUSH, FX_CRUSH, FX_DRIVE};      /* ALGORITHM walks them all in a row */
static const uint8_t FXL_S[FXL_N] = {0, DL_DIGITAL, DL_TAPE, DL_DUB, CR_BITS, CR_LOFI, DR_FOLD};
static uint32_t fx_pos(uint32_t k)                              /* where the unit in a slot is in that row */
{
    uint32_t i;
    for (i = 0; i < FXL_N; i++) if (FXL_K[i] == rack.u[k].type && (i == 0u || FXL_S[i] == rack.u[k].sub)) return i;
    return 0;
}
static void fx_pick(uint32_t k, uint32_t i)                     /* put the i-th of the row in a slot (0 takes it out); its knobs come back if it was there before */
{
    uint32_t old = rack.u[k].type;
    fx_stash(k);
    if (FXL_K[i] == FX_NONE) fx_unplug(k);
    else if (old == FX_NONE) fx_set(k, FXL_K[i], FXL_S[i]);
    else fx_morph(k, FXL_K[i], FXL_S[i]);
    fx_recall(k);
}
static void fx_sanitize(fxr_t *r)
{
    uint32_t k, j;
    r->on = r->on ? 1u : 0u;
    for (k = 0; k < FXN; k++) {
        fxu_t *u = &r->u[k];
        if (r->ver != FX_VER || k >= FXU) memset(u, 0, sizeof *u);      /* an older rack: other kinds, emptied; slots past two stay empty */
        if (u->type >= FX_KINDS) u->type = FX_NONE;
        if (u->sub >= FX_SUBS[u->type]) u->sub = 0;
        for (j = 0; j < 8u; j++) if (u->p[j] > 31u) u->p[j] = 31u;
    }
    r->ver = FX_VER;
    fx_chain(r);
}
static void fx_load(const fxr_t *r)                             /* a project's rack becomes what plays (the lines start empty) */
{
    uint32_t k;
    rack.on = 0;
    memset(fx_kept, 0, sizeof fx_kept);
    for (k = 0; k < FXN; k++) { rack.u[k].type = FX_NONE; fx_clear_line(k); }
    rack = *r;
    rack.on = 0;
    fx_sanitize(&rack);
    fx_rst = (1u << FXN) - 1u;
    rack.on = rack.u[0].type != FX_NONE || rack.u[1].type != FX_NONE ? 1u : 0u;
}
static uint32_t fx_any(void) { return rack.u[0].type != FX_NONE || rack.u[1].type != FX_NONE; }
