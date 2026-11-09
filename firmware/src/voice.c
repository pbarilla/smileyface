/* SPDX-License-Identifier: GPL-3.0-only */
/* The bass line: one 303-style voice (saw or square, a four pole ladder with a squashed feedback, an envelope
 * that decays, accent, slide) and the 16 step sequencer that plays it. Runs in the audio interrupt. Integers only. */
#include "acid_tables.h"
#ifndef OS
#define OS 2                                            /* the oscillator and the ladder run this many times a sample */
#endif
#if OS == 2
#define CUT_G CUT_G2
#define OSH 1
#else
#define CUT_G CUT_G1
#define OSH 0
#endif

#define STEPS 16u
enum { F_ACC = 1, F_SLD = 2 };
typedef struct { uint8_t note, fl; } step_t;        /* note 0 = rest, else a MIDI note; fl: F_ACC, F_SLD */
typedef struct { uint8_t cut, res, env, dec, acc, sld, drv, wave, adc, trk, atk, muf, tun, sub, swp, pol, fm, hir, swg; int8_t lvl; } knobs_t;   /* each 0..31 (wave: bit 0 square, bit 1 accents off; lvl: the output level, -24 .. +7 around 0) */
#define KMAX 31
#define PATS 16u                                    /* the patterns of a project */

static step_t pats[PATS][STEPS];
static uint8_t plen[PATS];                          /* each pattern's length, 1..16 */
static volatile uint32_t pat_idx;                   /* the pattern playing (and being written) */
static knobs_t kn = {14, 20, 18, 12, 16, 14, 4, 0, 4, 12, 2, 0, 16, 0, 12, 31, 0, 0, 0, 0};
#ifndef REL_RATE
#define REL_RATE 2200                                    /* how fast the volume closes at the end of a note, per control tick, two poles: 4500 is about 3 ms, 2000 about 12 ms */
#endif
static volatile uint8_t vol_dec = 31;               /* DECAY (the Devil Fish's volume decay): 0 shortest .. 15 three seconds, 16..31 the note settles lower and lower; 31 holds for the whole gate (how it always was) */
static volatile uint32_t seq_run, seq_step = STEPS - 1u, seq_bpm = 126;
/* patterns cued to play next, each when the one before ends (the main loop pushes, the audio pops) */
#define CUEQ 8u
static volatile uint8_t cue_q[CUEQ];
static volatile uint32_t cue_w, cue_r;
static void cue_push(uint32_t p) { if (cue_w - cue_r < CUEQ) { cue_q[cue_w % CUEQ] = (uint8_t)p; cue_w++; } }
static void cue_clear(void) { cue_r = cue_w; }
/* the CHAIN: up to 16 patterns, saved with the project, played in order and round again when PLAY is pressed in the PLAY view; PLAY anywhere else loops the one pattern.
 * The main loop edits it while stopped (an entry is written, then the length grows: the audio side never reads past chain_n); chain_pos is the entry playing. */
#define CHAIN_MAX 16u
static volatile uint8_t chain[CHAIN_MAX];
static volatile uint32_t chain_n, chain_pos;
static volatile uint8_t seq_wantchain;                          /* set by the main loop: PLAY would play the chain (the PLAY view is on) */
static volatile uint8_t seq_chain_on, seq_fresh;                /* this run follows the chain; the first tick of a run plays step 1 of the pattern it starts on */
/* the accent button, held from the main loop (ENV in PLAY) */
static volatile uint32_t fx_acc;
static volatile uint32_t cpu_max;
static volatile int32_t env_vis;                    /* the filter envelope as the screen sees it, Q15 */

/* events from the main loop (keys): 1 = note on (note, accent, slide), 2 = gate off */
#define EVQ 16u
static volatile uint32_t ev_q[EVQ], ev_w, ev_r;
static void ev_push(uint32_t v) { ev_q[ev_w % EVQ] = v; ev_w++; }
static void voice_on(uint32_t note, uint32_t acc, uint32_t slide) { ev_push(1u | note << 8 | acc << 16 | slide << 17); }
static void voice_off(void) { ev_push(2u); }

static int32_t mulq(int32_t a, int32_t g)           /* a * g >> 15, g <= 32768, no 64 bit */
{
    return (a >> 15) * g + (((a & 32767) * g) >> 15);
}
static int32_t soft(int32_t x)                      /* a cubic knee: x - x^3 / 3, flat past 1.0 (Q15) */
{
    if (x > 32767) x = 32767;
    if (x < -32767) x = -32767;
    return x - mulq(mulq(x, x), x) / 3;
}

typedef struct {
    uint32_t ph, ph2, incT;                         /* oscillator phases, the step now */
    int32_t p, tp;                                  /* pitch now and wanted, in 1/256 semitones: the glide runs on this, as the 303's does (an RC on the pitch voltage) */                          /* oscillator phase, step now, step wanted (slide) */
    uint32_t gate, acc, tie, env_pos, note;
    int32_t drv, xp, dcs, acc_cap, vca, s[4], G, G2, G3, G4, omG, R, k, bl, bl2, bl3, va, dc, ci, v1, vs, idxc, fmk, fmy, nd0, nd1, dr0, dr1, dg0, dg1;   /* the ladder: states, gain powers and the loop's reciprocal, set at control rate */
    uint32_t tick, rs, nz;
} voice_t;
static voice_t vo;

static int32_t blep(uint32_t t, uint32_t dt)        /* polyBLEP residue, Q15; t, dt are Q16 */
{
    int32_t x;
    if (!dt) return 0;
    if (t < dt) { x = (int32_t)((t << 15) / dt); return 2 * x - mulq(x, x) - 32768; }
    if (t > 65536u - dt) { x = (int32_t)(((int32_t)t - 65536) * 32768 / (int32_t)dt); return mulq(x, x) + 2 * x + 32768; }
    return 0;
}

static void gate_on(uint32_t note, uint32_t acc, uint32_t slide_in)
{
    vo.tp = (int32_t)(note < 127u ? note : 126u) << 8;
    vo.note = note;
    if (!slide_in || !vo.gate || !vo.p) vo.p = vo.tp;
    if (!slide_in || !vo.gate) {
        vo.env_pos = 0; vo.va = 1 << 24;
        vo.rs = vo.rs * 1664525u + 1013904223u; vo.nd0 = (int32_t)(vo.rs >> 16) - 32768;       /* DRIFT: this note is a little off, in pitch and in brightness */
        vo.rs = vo.rs * 1664525u + 1013904223u; vo.nd1 = (int32_t)(vo.rs >> 16) - 32768;
    } else vo.va = 1 << 24;                                  /* a slid note keeps its filter sweep, but its volume is plucked again: VOL DEC cannot make a slide vanish */      /* a slid note keeps going: no new envelope */
    vo.acc = acc;
    vo.acc_cap += acc ? ((kn.swp < 8u ? 65534 : 32767) - vo.acc_cap) / 3 : 0;        /* SWEEP slow: it piles up twice as high */
    vo.gate = 1;
}

/* the sequencer: a step every 16th. bpm * 1 per sample against 44100 * 15 */
static volatile uint32_t sq_acc;
static volatile uint32_t ext_on, ext_in;                        /* an outside clock: 24 a quarter note, counted by the main loop (ext_in) */
static uint32_t ext_seen, ext_phase, ext_age;
static uint32_t sq_len, sq_age, sq_prev_sld;
#ifdef FELUCCA_ID                                            /* firmware/ble/ble.c: the brothers' Bluetooth link, its own unit */
extern volatile uint8_t brothers_mode;                      /* 0 off, 1 lead, 2 follow */
extern void brothers_set(uint32_t mode);
extern void brothers_poll(void);
extern uint32_t brothers_link(void);
extern void brothers_lead_tick(uint32_t off, uint32_t per);
extern uint32_t brothers_bpm(void);
extern void brothers_lead_set(uint32_t pat, uint32_t kick, uint32_t fx, uint32_t scale, uint32_t swing);
extern uint32_t brothers_pat(void);
extern uint32_t brothers_fx(void);
extern uint32_t brothers_scale(void);
extern uint32_t brothers_swing(void);
extern uint32_t brothers_detect(const uint8_t *notes, uint32_t n);
extern uint32_t brothers_snap(uint32_t note, uint32_t scale);
extern uint32_t brothers_duck(uint32_t depth);
extern void brothers_lead_run(uint32_t run);
#else
static uint8_t brothers_mode;
static void brothers_set(uint32_t mode) { brothers_mode = (uint8_t)mode; }
static void brothers_poll(void) {}
static uint32_t brothers_link(void) { return 0; }
static void brothers_lead_tick(uint32_t off, uint32_t per) { (void)off; (void)per; }
static uint32_t brothers_bpm(void) { return 0; }
static void brothers_lead_set(uint32_t pat, uint32_t kick, uint32_t fx, uint32_t scale, uint32_t swing) { (void)pat; (void)kick; (void)fx; (void)scale; (void)swing; }
static uint32_t brothers_pat(void) { return 0xFFu; }
static uint32_t brothers_fx(void) { return 0u; }
static uint32_t brothers_scale(void) { return 0u; }
static uint32_t brothers_swing(void) { return 0u; }
static uint32_t brothers_detect(const uint8_t *notes, uint32_t n) { (void)notes; (void)n; return 0u; }
static uint32_t brothers_snap(uint32_t note, uint32_t scale) { (void)scale; return note; }
static uint32_t brothers_duck(uint32_t depth) { (void)depth; return 256u; }
static void brothers_lead_run(uint32_t run) { (void)run; }
#endif
static uint8_t bt_side, bt_patsync, bt_fxsync, bt_key, bt_last = 0xFFu;
static volatile uint8_t bt_scale, bt_swg_on, bt_swg;            /* as a follower: the leader's key, its swing */
static volatile uint8_t ax_acc, ax_sld, ax_up, ax_dn, ax_res, ax_swp, ax_short;      /* the bass's own punch-in pads (pfx.c, pads 9-15): accent all, slide all, octave up and down, resonance to the top, a cutoff sweep, a short decay */           /* Bluetooth sync, as a follower: the sidechain; following the leader's pattern (menu > Bluetooth) */
static uint8_t btq[32];                                      /* the brothers' link's clock / start / stop, in order, for the main loop (as USB MIDI's) */
static volatile uint8_t btq_w, btq_r;
void bt_rt(uint32_t st) { if ((uint8_t)(btq_w - btq_r) < 32u) btq[btq_w++ & 31u] = (uint8_t)st; }
static int bt_get(uint8_t *st, uint8_t *d1, uint8_t *d2)
{
    if (btq_r == btq_w) return 0;
    *st = btq[btq_r++ & 31u]; *d1 = 0x55u; *d2 = 0;
    return 1;
}
static volatile uint8_t clk_send;                               /* MIDI Clock: Send. Our own clock sends 24 a quarter note (six a step), play and stop send start and stop */
static uint32_t clk_acc;
static void seq_fire(void)
{
    step_t s = pats[pat_idx % PATS][seq_step % STEPS];
    sq_age = 0;
    if (s.note) {
        gate_on(bt_scale ? brothers_snap(s.note, bt_scale) : s.note, ((s.fl & F_ACC) || ax_acc) && !(kn.wave & 2u), sq_prev_sld);          /* (accents off: the switch) */
        vo.tie = (s.fl | (ax_sld ? F_SLD : 0)) & F_SLD;
    } else {
        vo.tie = 0;
        vo.gate = 0;
    }
    sq_prev_sld = s.note ? (uint32_t)((s.fl | (ax_sld ? F_SLD : 0)) & F_SLD) : 0u;
}
static void seq_next(void)                                      /* the next step; at the end of the pattern, the next one cued */
{
    uint32_t len = plen[pat_idx % PATS];
    if (len < 1u || len > STEPS) len = STEPS;
    if (++seq_step >= len) {
        seq_step = 0;
        if (seq_fresh) seq_fresh = 0;                           /* (a run starts one step "before" step 1: that wrap is not the end of a pattern) */
        else if (cue_w != cue_r) { pat_idx = cue_q[cue_r % CUEQ] % PATS; cue_r++; }                    /* a cued pattern first, then the chain goes on where it was */
        else if (seq_chain_on && chain_n && !(brothers_mode == 2u && bt_patsync)) {
            uint32_t n = chain_n;
            chain_pos = (chain_pos + 1u) % n;
            pat_idx = chain[chain_pos] % PATS;
        }
    }
}
static void seq_play(void)
{
    seq_step = STEPS - 1u; sq_acc = 661500u; clk_acc = 661500u; sq_prev_sld = 0; ext_phase = 5; ext_seen = ext_in; ext_age = 0;
    seq_chain_on = seq_wantchain && chain_n ? 1u : 0u; chain_pos = 0; seq_fresh = 1;
    if (seq_chain_on) pat_idx = chain[0] % PATS;                /* the chain starts at its first entry; anywhere else the pattern you are on loops */
    if (clk_send) midi_send(0xFAu, 0, 0);
    if (brothers_mode == 1u) brothers_lead_run(1);
    seq_run = 1;      /* (with an outside clock the next tick is the first step) */
}
static void ext_seen_reset(void) { ext_seen = ext_in; ext_phase = 5; ext_age = 0; }
static void seq_stop(void) { uint32_t was = seq_run; cue_clear(); seq_run = 0; sq_prev_sld = 0; vo.gate = 0; vo.tie = 0; if (was && clk_send) midi_send(0xFCu, 0, 0); if (was && brothers_mode == 1u) brothers_lead_run(0); }

/* kn.wave: bit 0 square, bit 1 accents off, 2-3 BASS (0..2), 4-5 DRIFT (0..2), 6 DIODE, 7 two poles. Old banks have bits 0-1 only. */
#define W_BASS ((uint32_t)kn.wave >> 2 & 3u)
#define W_DRIFT ((uint32_t)kn.wave >> 4 & 3u)
#define W_DIODE ((uint32_t)kn.wave >> 6 & 1u)
#define W_TWO ((uint32_t)kn.wave >> 7 & 1u)
static int32_t dsoft(int32_t o)                     /* a diode stage: lopsided soft clip (the lopsidedness is the grit), Q19 */
{
    return (soft(((o >> 4) * 6) + 6000) - soft(6000)) * 16 / 6;
}

static int32_t core_step(void)                                  /* one step of the oscillator and the ladder; x out (Q19) */
{
    int32_t s, x, x2, fb;
    uint32_t t, dt, dio = W_DIODE;
    if (vo.fmk) {                                       /* FILT FM: what comes out of the amp shakes the cutoff, a sample at a time (the Devil Fish's knob). Little until well up, then wild. */
        int32_t m = ((vo.fmy >> 6) * vo.fmk) >> 9, ii = vo.idxc * 16 + m, g, fr;          /* in sixteenths of a table step: it glides, it does not jump a whole step (that was a tick) */
        if (ii < 0) ii = 0;
        if (ii > 255 * 16) ii = 255 * 16;
        fr = ii & 15; ii >>= 4;
        g = CUT_G[ii] + (((int32_t)CUT_G[ii < 255 ? ii + 1 : 255] - (int32_t)CUT_G[ii]) * fr >> 4); vo.G = g;         vo.G2 = mulq(g, g); vo.G3 = mulq(vo.G2, g); vo.G4 = mulq(vo.G3, g); vo.omG = 32768 - g;
        vo.R = (int32_t)((4096u << 15) / (uint32_t)(4096 + (vo.k * vo.G4 >> 15)));
    }
    t = vo.ph >> 16; dt = vo.incT >> (16 + OSH);
    vo.ph += vo.incT >> OSH;
    vo.ph2 += vo.incT >> (OSH + 1);
    if (kn.wave & 1u) {
        s = t < 32768u ? 22000 : -22000;
        s += mulq(blep(t, dt), 22000);
        s -= mulq(blep((t + 32768u) & 65535u, dt), 22000);
    } else {
        s = ((int32_t)t - 32768) * 22000 / 32768;
        s -= mulq(blep(t, dt), 22000);
        s = -s;
    }
    vo.bl += mulq(s - vo.bl, 330); vo.bl2 += mulq(vo.bl - vo.bl2, 330); vo.bl3 += mulq(vo.bl2 - vo.bl3, 330);      /* BASS: the oscillator's own low end (three poles, 140 Hz) under the filter, so a high resonance cannot thin it out */
    if (kn.sub) s += (vo.ph2 < 0x80000000u ? 9000 : -9000) * (int32_t)kn.sub / 31;       /* SUB: a square an octave down */
    {
        int32_t S, u, o;
        S = mulq(mulq(vo.s[0], vo.G3) + mulq(vo.s[1], vo.G2) + mulq(vo.s[2], vo.G) + vo.s[3], vo.omG);
        fb = (S >> 4) * vo.k >> 12;
        if (vo.drv < 3) { vo.nz = vo.nz * 1664525u + 1013904223u; fb -= (int32_t)(vo.nz >> 27) - 16; }       /* (a whisper, so the filter can start ringing on its own when the oscillator is turned down) */
        u = soft(mulq(((s >> 2) * (int32_t)(dio ? DRV_G[vo.drv & 31] * 5u / 2u : DRV_G[vo.drv & 31]) >> 6) - fb, vo.R)) * 16;          /* DRV: the oscillator pushed into the ladder: silent to 66.6x the default (s >> 2 keeps the product inside 32 bits) */
        o = mulq(u, vo.G) + mulq(vo.s[0], vo.omG); vo.s[0] = 2 * o - vo.s[0];
        u = dio ? dsoft(o) : o; o = mulq(u, vo.G) + mulq(vo.s[1], vo.omG); vo.s[1] = 2 * o - vo.s[1];
        x2 = o;
        u = dio ? dsoft(o) : o; o = mulq(u, vo.G) + mulq(vo.s[2], vo.omG); vo.s[2] = 2 * o - vo.s[2];
        x = o;
        u = dio ? dsoft(o) : o; o = mulq(u, vo.G) + mulq(vo.s[3], vo.omG); vo.s[3] = 2 * o - vo.s[3];
        x = (x * (31 - (int32_t)kn.pol) + o * (int32_t)kn.pol) / 31;               /* POL: three poles to four */
        if (W_TWO) x = x2;                                                          /* two poles: the tap after the second stage */
        if (W_BASS) { int32_t lo = vo.bl3 * 16; x += mulq(lo, 32768); if (W_BASS == 2u) x += mulq(lo, 32768); }       /* BASS: the oscillator's low end, once (1) or twice (2) */
        if (dio) x = dsoft(x) * 2;                                                   /* DIODE: and the last stage's output is saturated too, whichever tap it is (and made up, it squashes) */
    }
    return x;
}

static void voice_render(int32_t *out, uint32_t n)
{
    uint32_t i, duck = brothers_mode == 2u && bt_side ? brothers_duck(190u) : 256u;
    while (ev_r != ev_w) {
        uint32_t v = ev_q[ev_r % EVQ];
        ev_r++;
        if ((v & 255u) == 1u) { gate_on(v >> 8 & 255u, v >> 16 & 1u, v >> 17 & 1u); vo.tie = 0; }
        else vo.gate = 0;
    }
    for (i = 0; i < n; i++) {
        int32_t s, x, k;
        if (seq_run) {
            if (ext_on) {                                       /* an outside clock: six ticks a step */
                ext_age++;
                while (ext_seen != ext_in) {
                    ext_seen++;
                    if (++ext_phase >= 6u) {
                        ext_phase = 0;
                        if (brothers_mode == 2u) sq_len = 661500u / seq_bpm;                                          /* the brothers' link says the tempo exactly (the main loop sets seq_bpm) */
                        else if (ext_age > 2000u && ext_age < 90000u) { sq_len = (sq_len * 3u + ext_age) / 4u; seq_bpm = 661500u / sq_len; }
                        ext_age = 0;
                        seq_next();
                        seq_fire();
                    }
                }
            } else {
                uint32_t sg = bt_swg_on ? bt_swg : kn.swg, thr = (seq_step & 1u) ? 661500u - 661500u * sg / 96u : 661500u + 661500u * sg / 96u;      /* SWG: the odd steps come late */
                sq_acc += seq_bpm;
                if (clk_send || brothers_mode == 1u) { clk_acc += seq_bpm * 6u; if (clk_acc >= 661500u) { clk_acc -= 661500u; if (clk_send) midi_isr_send(0xF8u, 0, 0); if (brothers_mode == 1u) brothers_lead_tick(i, 60000000u / seq_bpm); } }
                if (sq_acc >= thr) {
                    sq_acc -= thr;
                    sq_len = thr / seq_bpm;
                    seq_next();
                    seq_fire();
                }
            }
            sq_age++;
            if (vo.gate && !vo.tie && sq_age > sq_len * 5u / 8u) vo.gate = 0;
        }
        if ((vo.tick++ & 7u) == 0u) {                       /* control rate: 5.5 kHz */
            const uint32_t accx = vo.acc || fx_acc;
            const uint32_t swp_on = kn.hir != 15u;                     /* ACC SWP at 0: no accent sweep */
            int32_t tg = vo.gate ? (accx && swp_on ? 32767 : 24000) : 0, e, idx, qi, amt, pp;
            uint32_t ep;
            vo.p += mulq(vo.tp - vo.p, GLIDE_G[kn.sld]);
            if ((++vo.dc & 4095) == 0) {                       /* DRIFT: two slow wanderers, one for the pitch and one for the cutoff */
                vo.rs = vo.rs * 1664525u + 1013904223u; vo.dg0 = (int32_t)(vo.rs >> 16) - 32768;
                vo.rs = vo.rs * 1664525u + 1013904223u; vo.dg1 = (int32_t)(vo.rs >> 16) - 32768;
            }
            vo.dr0 += (vo.dg0 - vo.dr0) >> 11; vo.dr1 += (vo.dg1 - vo.dr1) >> 11;
            pp = vo.p + bfx_pitch + (ax_up ? 3072 : 0) - (ax_dn ? 3072 : 0) + (W_DRIFT ? mulq(vo.dr0 + vo.nd0, W_DRIFT == 1u ? 72 : 170) : 0);
            pp = pp < 0 ? 0 : pp > 126 * 256 ? 126 * 256 : pp;
            {
                uint32_t n = (uint32_t)pp >> 8, a = NOTE_INC[n], b = NOTE_INC[n + 1u];
                vo.incT = (uint32_t)mulq((int32_t)(a + (((b - a) >> 8) * ((uint32_t)pp & 255u))), TUNE_R[kn.tun]);          /* TUN: a little either way */
            }
            if (vol_dec < 31u || ax_short) {                                    /* VOL DEC: the volume falls, to nothing or to a floor */
                uint32_t vdv = ax_short ? 4u : vol_dec;
                int32_t fl = vdv < 16u ? 0 : (int32_t)(vdv - 15u) << 20;
                vo.va += mulq(fl - vo.va, VD_G[vdv < 15u ? vdv : 15u]);
                tg = (tg * (vo.va >> 9)) >> 15;
            } else vo.va = 1 << 24;
            { int32_t vr = vo.gate ? (accx ? 9000 : ATK_G[kn.atk]) : REL_RATE;          /* two poles one after the other: the volume starts and ends with no corner, so no tick */
              vo.v1 += mulq(tg - vo.v1, vr); vo.vca += mulq(vo.v1 - vo.vca, vr); }        /* ATK: a soft attack (2 to 30 ms), on the notes without an accent */
            ep = vo.env_pos >> 16;
            e = ENV_T[ep < 767u ? ep : 767u];
            env_vis = vo.gate ? e : env_vis - (env_vis >> 3);
            vo.env_pos += DEC_STEP[accx ? kn.adc : kn.dec];            /* NRM DEC and ACC DEC: the filter envelope's two decays */
            vo.acc_cap -= vo.acc_cap >> (12 - (int32_t)kn.swp / 6);        /* SWP: how long the accents pile up */
            amt = ENV_AMT[kn.env];
            if (accx && swp_on) amt += mulq(amt + 30, 8192 + (vo.acc_cap >> 1) + (int32_t)kn.acc * 1000);
            idx = (int32_t)kn.cut * 7 + mulq(e, amt);
            if (W_DRIFT) idx += mulq(vo.dr1 + vo.nd1, W_DRIFT == 1u ? 24 : 64);
            if (ax_swp) { int32_t t = (int32_t)(vo.tick >> 8 & 255u); t = t < 128 ? t : 255 - t; idx += (t - 64) * 2; }          /* SWEEP: the cutoff goes up and down */
            if (idx > 255) idx = 255;
            if (idx < 0) idx = 0;
            qi = idx << 8;
            if (kn.trk) {                                           /* TRK: the cutoff follows the note, in Hz, like the Devil Fish: 2.7 kHz a volt (an octave) at the top, none at C2 (65 Hz, 2 V) */
                int32_t hz = (int32_t)CUT_HZ[idx] + ((vo.p - 37 * 256) * (int32_t)kn.trk * 29) / 1024, lo = 0, hi = 255;
                if (hz <= (int32_t)CUT_HZ[0]) qi = 0;
                else if (hz >= (int32_t)CUT_HZ[255]) qi = 255 << 8;
                else {
                    while (hi - lo > 1) { int32_t m = (lo + hi) >> 1; if ((int32_t)CUT_HZ[m] <= hz) lo = m; else hi = m; }
                    qi = (lo << 8) + (hz - (int32_t)CUT_HZ[lo]) * 256 / ((int32_t)CUT_HZ[lo + 1] - (int32_t)CUT_HZ[lo]);
                }
            }
            vo.ci += (qi - vo.ci) / 3;                       /* the cutoff does not jump when a note starts: about half a millisecond to get there (no tick) */
            idx = vo.ci >> 8;
            vo.idxc = idx; vo.fmk = (int32_t)kn.fm * (int32_t)kn.fm;
            k = RES_K[ax_res ? 31u : kn.res];                      /* RES MAX: the resonance to the top */
            if (W_DIODE) k -= k >> 2;                              /* a diode ladder sings a little less */
            vo.drv = kn.drv;
            if (kn.hir) k *= 2;                                     /* HI RES: twice the feedback, so it rings by itself */
            vo.G = CUT_G[idx];
            vo.G2 = mulq(vo.G, vo.G); vo.G3 = mulq(vo.G2, vo.G); vo.G4 = mulq(vo.G3, vo.G);
            vo.omG = 32768 - vo.G;
            vo.k = k;
            vo.R = (int32_t)((4096u << 15) / (uint32_t)(4096 + (k * vo.G4 >> 15)));     /* 1 / (1 + k G^4): the feedback solved, no delay in the loop */
        }
        {
            int32_t xa = core_step(), xb = OS == 2 ? core_step() : xa;
            x = OS == 2 ? (vo.xp + 2 * xa + xb) >> 2 : xa;      /* [1 2 1] down to 44.1 kHz */
            vo.xp = xb;
        }
        vo.vs += (vo.vca - vo.vs) >> 3;                         /* (and between control ticks the gain glides, it does not step) */
        s = mulq(x >> 4, vo.vs);
        s = s * (4096 + (vo.k >> 2)) >> 12;                 /* a quarter of what the resonance takes from the bass comes back */
        s = soft(s * (6 + (int32_t)kn.muf * 2) / 4) * 7 / 8;          /* MUF: soft clip after the VCA */
        vo.dcs += (s * 16 - vo.dcs) >> 9;                       /* a DC blocker: the clipping is lopsided on a resonant saw */
        s -= mulq(vo.dcs >> 4, vo.vs * 4 > 32768 ? 32768 : vo.vs * 4);          /* (the DC it takes off fades with the volume: it used to stay, and when a note ended it was left on its own, a small step and a bump: the tick) */
        s = s * (24 + (int32_t)kn.lvl) / 24;                    /* LEVEL */
        s = (s * (int32_t)duck) >> 8;                           /* the sidechain: the leader's kicks pull the bass down and let go */
        vo.fmy = s;
        if (s > 32000) s = 32000;
        if (s < -32000) s = -32000;
        out[2u * i] += s;
        out[2u * i + 1u] += s;
    }
}
