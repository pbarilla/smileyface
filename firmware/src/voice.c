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
typedef struct { uint8_t cut, res, env, dec, acc, sld, drv, wave, adc, trk, atk, muf, tun, sub, swp, pol, fm, hir, swg, len; } knobs_t;   /* each 0..31 (wave 0 saw, 1 square; len 1..16); five pages of four */
#define KMAX 31

static step_t pat[STEPS];
static knobs_t kn = {14, 20, 18, 12, 16, 14, 4, 0, 4, 12, 2, 0, 16, 0, 12, 31, 0, 0, 0, 16};
static volatile uint32_t seq_run, seq_step = STEPS - 1u, seq_bpm = 126;
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
    int32_t xp, dcs, acc_cap, vca, s[4], G, G2, G3, G4, omG, R, k;   /* the ladder: states, gain powers and the loop's reciprocal, set at control rate */
    uint32_t tick;
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
    if (!slide_in || !vo.gate) vo.env_pos = 0;      /* a slid note keeps going: no new envelope */
    vo.acc = acc;
    vo.acc_cap += acc ? (32767 - vo.acc_cap) / 3 : 0;
    vo.gate = 1;
}

/* the sequencer: a step every 16th. bpm * 1 per sample against 44100 * 15 */
static volatile uint32_t sq_acc;
static volatile uint32_t ext_on, ext_in;                        /* an outside clock: 24 a quarter note, counted by the main loop (ext_in) */
static uint32_t ext_seen, ext_phase, ext_age;
static uint32_t sq_len, sq_age, sq_prev_sld;
static void seq_fire(void)
{
    const step_t s = pat[seq_step % STEPS];
    sq_age = 0;
    if (s.note) {
        gate_on(s.note, s.fl & F_ACC, sq_prev_sld);
        vo.tie = s.fl & F_SLD;
    } else {
        vo.tie = 0;
        vo.gate = 0;
    }
    sq_prev_sld = s.note ? (uint32_t)(s.fl & F_SLD) : 0u;
}
static void seq_play(void)
{
    seq_step = STEPS - 1u; sq_acc = 661500u; sq_prev_sld = 0; ext_phase = 5; ext_seen = ext_in; ext_age = 0; seq_run = 1;      /* (with an outside clock the next tick is the first step) */
}
static void ext_seen_reset(void) { ext_seen = ext_in; ext_phase = 5; ext_age = 0; }
static void seq_stop(void) { seq_run = 0; sq_prev_sld = 0; vo.gate = 0; vo.tie = 0; }

static int32_t core_step(void)                                  /* one step of the oscillator and the ladder; x out (Q19) */
{
    int32_t s, x, fb;
    uint32_t t, dt;
    t = vo.ph >> 16; dt = vo.incT >> (16 + OSH);
    vo.ph += vo.incT >> OSH;
    vo.ph2 += vo.incT >> (OSH + 1);
    if (kn.wave) {
        s = t < 32768u ? 22000 : -22000;
        s += mulq(blep(t, dt), 22000);
        s -= mulq(blep((t + 32768u) & 65535u, dt), 22000);
    } else {
        s = ((int32_t)t - 32768) * 22000 / 32768;
        s -= mulq(blep(t, dt), 22000);
        s = -s;
    }
    if (kn.sub) s += (vo.ph2 < 0x80000000u ? 9000 : -9000) * (int32_t)kn.sub / 31;       /* SUB: a square an octave down */
    {
        int32_t S, u, o;
        S = mulq(mulq(vo.s[0], vo.G3) + mulq(vo.s[1], vo.G2) + mulq(vo.s[2], vo.G) + vo.s[3], vo.omG);
        fb = (S >> 4) * vo.k >> 12;
        u = soft(mulq(s * (4 + (int32_t)kn.drv) / 6 - fb, vo.R)) * 16;          /* DRV: the oscillator pushed into the ladder */
        o = mulq(u, vo.G) + mulq(vo.s[0], vo.omG); vo.s[0] = 2 * o - vo.s[0];
        u = o; o = mulq(u, vo.G) + mulq(vo.s[1], vo.omG); vo.s[1] = 2 * o - vo.s[1];
        u = o; o = mulq(u, vo.G) + mulq(vo.s[2], vo.omG); vo.s[2] = 2 * o - vo.s[2];
        x = o;
        u = o; o = mulq(u, vo.G) + mulq(vo.s[3], vo.omG); vo.s[3] = 2 * o - vo.s[3];
        x = (x * (31 - (int32_t)kn.pol) + o * (int32_t)kn.pol) / 31;               /* POL: three poles to four */
    }
    return x;
}

static void voice_render(int32_t *out, uint32_t n)
{
    uint32_t i;
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
                        if (ext_age > 2000u && ext_age < 90000u) { sq_len = (sq_len * 3u + ext_age) / 4u; seq_bpm = 661500u / sq_len; }
                        ext_age = 0;
                        seq_step = (seq_step + 1u) % (kn.len < 1u ? 1u : kn.len > STEPS ? STEPS : kn.len);
                        seq_fire();
                    }
                }
            } else {
                uint32_t thr = (seq_step & 1u) ? 661500u - 661500u * kn.swg / 96u : 661500u + 661500u * kn.swg / 96u;      /* SWG: the odd steps come late */
                sq_acc += seq_bpm;
                if (sq_acc >= thr) {
                    sq_acc -= thr;
                    sq_len = thr / seq_bpm;
                    seq_step = (seq_step + 1u) % (kn.len < 1u ? 1u : kn.len > STEPS ? STEPS : kn.len);
                    seq_fire();
                }
            }
            sq_age++;
            if (vo.gate && !vo.tie && sq_age > sq_len * 5u / 8u) vo.gate = 0;
        }
        if ((vo.tick++ & 7u) == 0u) {                       /* control rate: 5.5 kHz */
            int32_t tg = vo.gate ? (vo.acc ? 32767 : 24000) : 0, e, idx, amt;
            uint32_t ep;
            vo.p += mulq(vo.tp - vo.p, GLIDE_G[kn.sld]);
            {
                uint32_t n = (uint32_t)vo.p >> 8, a = NOTE_INC[n], b = NOTE_INC[n + 1u];
                vo.incT = (uint32_t)mulq((int32_t)(a + (((b - a) >> 8) * ((uint32_t)vo.p & 255u))), TUNE_R[kn.tun]);          /* TUN: a little either way */
            }
            vo.vca += mulq(tg - vo.vca, vo.gate ? 12000 - (int32_t)kn.atk * 370 : 2500);        /* ATK: a soft attack */
            ep = vo.env_pos >> 16;
            e = ENV_T[ep < 767u ? ep : 767u];
            env_vis = vo.gate ? e : env_vis - (env_vis >> 3);
            vo.env_pos += DEC_STEP[vo.acc ? kn.adc : kn.dec];            /* accent has its own decay (ADC) */
            vo.acc_cap -= vo.acc_cap >> (12 - (int32_t)kn.swp / 6);        /* SWP: how long the accents pile up */
            amt = ENV_AMT[kn.env];
            if (vo.acc) amt += mulq(amt + 30, 8192 + (vo.acc_cap >> 1) + (int32_t)kn.acc * 1000);
            idx = (int32_t)kn.cut * 7 + mulq(e, amt) + ((int32_t)vo.note - 45) * (int32_t)kn.trk * 24 / 310;     /* TRK: the cutoff follows the note */
            if (idx > 255) idx = 255;
            if (idx < 0) idx = 0;
            k = RES_K[kn.res];
            k += (k * (int32_t)kn.hir) >> 9;                        /* HIR: past the edge of singing */
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
        s = mulq(x >> 4, vo.vca);
        s = s * (4096 + (vo.k >> 2)) >> 12;                 /* a quarter of what the resonance takes from the bass comes back */
        s = soft(s * (6 + (int32_t)kn.muf * 2) / 4) * 7 / 8;          /* MUF: soft clip after the VCA */
        vo.dcs += (s * 16 - vo.dcs) >> 9;                       /* a DC blocker: the clipping is lopsided on a resonant saw */
        s -= vo.dcs >> 4;
        if (s > 32000) s = 32000;
        if (s < -32000) s = -32000;
        out[2u * i] += s;
        out[2u * i + 1u] += s;
    }
}
