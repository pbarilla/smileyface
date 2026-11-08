/* SPDX-License-Identifier: GPL-3.0-only */
/* THE BROTHERS' SHARED PUNCH-IN EFFECTS: pads 1-8 of the effects layer on Chipbox, Smileyface and Drumlab. The same sound code on the last two, a mono port of
 * Chipbox's master chain (fx.c); Chipbox runs its own. Bit n of the mask is pad n + 1, and these are the eight that travel over Bluetooth.
 *   1 STOP   the tape slows to a halt: the level fades and the voices' pitch dives (the host adds bfx_pitch to its voices)
 *   2 REPEAT the last sixteenth note, looped
 *   3 LOW-PASS and 4 HIGH-PASS   a filter that sweeps in while held and back out
 *   5 ECHO   the delay (a dotted-eighth-ish: two sixteenths), a tail that rings on
 *   6 REVERB four combs and two allpasses
 *   7 CRUSH  the samples held three at a time and the low bits dropped
 *   8 GATE   the sound chopped at the sixteenths
 * The host defines BFX_POOL (where the big buffers go) before including this, calls bfx_tick() at 60 Hz and bfx_run(out, n, step) with the stereo mix (it
 * reads the left side, writes both) while anything is on (bfx_busy()) and bfx_rec(out, n) otherwise (the repeat needs the last sixteenth), and sets bfx_mask from the pads. Integers only. */
#define BFX_STOP 1u
#define BFX_REP 2u
#define BFX_LPF 4u
#define BFX_HPF 8u
#define BFX_ECHO 16u
#define BFX_VERB 32u
#define BFX_CRUSH 64u
#define BFX_GATE 128u
#define BFX_EC 16384u                           /* the echo line: 0.37 s (a slow tempo cuts it short) */
#define BFX_RB 8192u                            /* the ring the repeat takes the last sixteenth from (a slow tempo cuts it short) */

static volatile uint32_t bfx_mask;
static volatile int32_t bfx_pitch;              /* the voices' pitch offset, 1/256 semitone (0 or below): the tape stop's dive */
static int32_t bfx_speed = 256;                 /* the tape: 256 running, 0 stopped */
static int16_t bfx_ec[BFX_EC] BFX_POOL;
static int16_t bfx_rb[BFX_RB] BFX_POOL;
static int16_t bfx_vb_comb[4][1617] BFX_POOL, bfx_vb_ap[2][556] BFX_POOL;
static const uint16_t BFX_VB_LEN[4] = {1116, 1277, 1422, 1617};
static const uint16_t BFX_AP_LEN[2] = {225, 556};
static const uint16_t BFX_LP_A[16] = {16384, 9754, 8393, 7118, 5961, 4942, 4062, 3316, 2692, 2176, 1752, 1406, 1127, 901, 719, 573};
static const uint16_t BFX_HP_A[16] = {0, 124, 165, 220, 293, 389, 517, 685, 908, 1199, 1579, 2071, 2702, 3500, 4493, 5699};
static uint32_t bfx_lp_i, bfx_hp_i, bfx_cnt, bfx_echo_tail, bfx_verb_tail, bfx_echo_clr = BFX_EC, bfx_pos, bfx_gp, bfx_rbw, bfx_rs, bfx_rl, bfx_ri, bfx_hold_n, bfx_vb_dirty;
static uint32_t bfx_vb_pos[4], bfx_ap_pos[2], bfx_prev;
static int32_t bfx_g = 256, bfx_held, bfx_lp1, bfx_lp2, bfx_vb_lp[4];

static void bfx_tick(void)                      /* 60 Hz: the ramps */
{
    uint32_t m = bfx_mask;
    int32_t tgt = m & BFX_STOP ? 0 : 256;
    if (bfx_speed > tgt) bfx_speed = tgt + (bfx_speed - tgt) * 13 / 16;
    else if (bfx_speed < tgt) bfx_speed += (tgt - bfx_speed) / 3 + 4;
    if (bfx_speed < 4 && tgt == 0) bfx_speed = 0;
    if (bfx_speed > 256) bfx_speed = 256;
    bfx_pitch = bfx_speed < 256 ? -(256 - bfx_speed) * 24 : 0;                  /* two octaves down at a halt */
    if (m & BFX_LPF) { if (++bfx_cnt % 3u == 0u && bfx_lp_i < 15u) bfx_lp_i++; }
    else if (bfx_lp_i) bfx_lp_i = bfx_lp_i > 2u ? bfx_lp_i - 2u : 0u;
    if (m & BFX_HPF) { if (++bfx_cnt % 3u == 0u && bfx_hp_i < 15u) bfx_hp_i++; }
    else if (bfx_hp_i) bfx_hp_i = bfx_hp_i > 2u ? bfx_hp_i - 2u : 0u;
    if (m & BFX_ECHO) bfx_echo_tail = 150u; else if (bfx_echo_tail) bfx_echo_tail--;       /* the echo rings on for 2.5 s */
    if (m & BFX_VERB) bfx_verb_tail = 200u; else if (bfx_verb_tail) bfx_verb_tail--;
}
static uint32_t bfx_busy(void) { return bfx_mask || bfx_lp_i || bfx_hp_i || bfx_echo_tail || bfx_verb_tail || bfx_speed < 256 || bfx_g != 256; }

/* nothing is on: still keep the dry mix, so a REPEAT pressed now has the last sixteenth to loop */
static void bfx_rec(const int32_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t v = out[2u * i];
        bfx_rb[bfx_rbw++ & (BFX_RB - 1u)] = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
    }
}

/* out: n stereo frames (left read, both written); step: samples in a sixteenth note */
static void bfx_run(int32_t *out, uint32_t n, uint32_t step)
{
    uint32_t m = bfx_mask, i;
    uint32_t hold = (m & BFX_CRUSH) ? 3u : 1u, echo_d = step * 2u;
    int32_t a_lp = BFX_LP_A[bfx_lp_i], a_hp = BFX_HP_A[bfx_hp_i], tg = bfx_speed >= 256 ? 256 : bfx_speed * bfx_speed >> 8;
    if (echo_d > BFX_EC) echo_d = BFX_EC;
    if (echo_d < 64u) echo_d = 64u;
    if (step > BFX_RB - 2u) step = BFX_RB - 2u;
    if ((m ^ bfx_prev) & m & BFX_REP) { bfx_rl = step; bfx_rs = bfx_rbw - step; bfx_ri = 0; }       /* the repeat has just gone on: the last sixteenth */
    bfx_prev = m;
    if (bfx_echo_tail) bfx_echo_clr = 0;
    else if (bfx_echo_clr < BFX_EC) {                                 /* the delay line is wiped a piece at a time once its tail is over */
        uint32_t w = BFX_EC - bfx_echo_clr < 2048u ? BFX_EC - bfx_echo_clr : 2048u;
        memset(&bfx_ec[bfx_echo_clr], 0, w * sizeof bfx_ec[0]);
        bfx_echo_clr += w;
    }
    if (!bfx_verb_tail && bfx_vb_dirty) { memset(bfx_vb_comb, 0, sizeof bfx_vb_comb); memset(bfx_vb_ap, 0, sizeof bfx_vb_ap); memset(bfx_vb_lp, 0, sizeof bfx_vb_lp); bfx_vb_dirty = 0; }
    for (i = 0; i < n; i++) {
        int32_t v = out[2u * i], target = 256;
        bfx_rb[bfx_rbw++ & (BFX_RB - 1u)] = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);       /* the dry mix, always: what the repeat loops */
        if (!bfx_busy()) continue;
        if (m & BFX_REP) { v = bfx_rb[(bfx_rs + bfx_ri) & (BFX_RB - 1u)]; if (++bfx_ri >= bfx_rl) bfx_ri = 0; }
        if (m & BFX_GATE) { if (++bfx_gp >= step) bfx_gp = 0; if (bfx_gp >= step / 2u) target = 0; }
        bfx_g += target > bfx_g ? (target - bfx_g > 4 ? 4 : target - bfx_g) : (bfx_g - target > 4 ? -4 : target - bfx_g);     /* a short ramp: no clicks */
        v = v * bfx_g >> 8;
        if (hold > 1u) { if (++bfx_hold_n >= hold) { bfx_hold_n = 0; bfx_held = v; } v = (bfx_held >> 9) * 512; }
        if (a_hp) { bfx_lp1 += ((v - bfx_lp1) * a_hp) >> 14; v -= bfx_lp1; }
        if (a_lp < 16384) { bfx_lp2 += ((v - bfx_lp2) * a_lp) >> 14; v = bfx_lp2; }
        if (bfx_echo_tail) {                                         /* the delay: what comes back, and what goes in while the pad is held */
            int32_t d = bfx_ec[bfx_pos], w = (m & BFX_ECHO) ? v : 0, nb = w + (d * 140 >> 8);
            bfx_ec[bfx_pos] = (int16_t)(nb > 32767 ? 32767 : nb < -32767 ? -32767 : nb);
            v += d * 150 >> 8;
            if (++bfx_pos >= echo_d) bfx_pos = 0;
        }
        v = v * tg >> 8;
        if (bfx_verb_tail) {                                         /* the reverb: four combs and two allpasses */
            int32_t in = (m & BFX_VERB) ? v >> 2 : 0, sum = 0, k, w;
            bfx_vb_dirty = 1;
            for (k = 0; k < 4; k++) {
                int32_t d = bfx_vb_comb[k][bfx_vb_pos[k]];
                bfx_vb_lp[k] += (d - bfx_vb_lp[k]) >> 2;             /* the highs die first */
                w = in + (bfx_vb_lp[k] * 222 >> 8);
                bfx_vb_comb[k][bfx_vb_pos[k]] = (int16_t)(w > 32767 ? 32767 : w < -32767 ? -32767 : w);
                if (++bfx_vb_pos[k] >= BFX_VB_LEN[k]) bfx_vb_pos[k] = 0;
                sum += d;
            }
            for (k = 0; k < 2; k++) {
                int32_t d = bfx_vb_ap[k][bfx_ap_pos[k]];
                w = sum + (d >> 1);
                bfx_vb_ap[k][bfx_ap_pos[k]] = (int16_t)(w > 32767 ? 32767 : w < -32767 ? -32767 : w);
                sum = d - (w >> 1);
                if (++bfx_ap_pos[k] >= BFX_AP_LEN[k]) bfx_ap_pos[k] = 0;
            }
            v += sum * 3 >> 1;
        }
        out[2u * i] = out[2u * i + 1u] = v;
    }
}
