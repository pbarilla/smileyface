/* SPDX-License-Identifier: GPL-3.0-only */
/* The 303 voice on its own: every cutoff x resonance x wave x note for stability and level, the filter's slope,
 * the envelope's shape. cc -O2 -Ifirmware/src -Ifirmware/gen -o build/voice_test tests/voice_test.c -lm && build/voice_test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../firmware/src/voice.c"
#define __attribute__(x)                                /* (the pool is the device's) */
#include "../firmware/src/fx.c"
#undef __attribute__

static int32_t buf[2 * 256];
static void reset(void) { memset(&vo, 0, sizeof vo); ev_r = ev_w = 0; seq_run = 0; }
/* a held note: n blocks, returns the peak after the first `skip` blocks; fills `outv` with the mono output */
static int32_t hold(uint32_t note, uint32_t acc, uint32_t blocks, uint32_t skip, int32_t *outv, int32_t *rms)
{
    uint32_t b, i;
    int32_t pk = 0;
    double s2 = 0; uint32_t cnt = 0;
    reset();
    voice_on(note, acc, 0);
    for (b = 0; b < blocks; b++) {
        memset(buf, 0, sizeof buf);
        voice_render(buf, 256);
        for (i = 0; i < 256; i++) {
            int32_t v = buf[2 * i];
            if (outv) outv[b * 256 + i] = v;
            if (b >= skip) { if (abs(v) > pk) pk = abs(v); s2 += (double)v * v; cnt++; }
        }
    }
    if (rms) *rms = cnt ? (int32_t)sqrt(s2 / cnt) : 0;
    return pk;
}

int main(void)
{
    uint32_t c, r, w, n, bad = 0, clip = 0;
    int32_t worst = 0, rms;
    memset(&kn, 0, sizeof kn); kn.drv = 4;
    kn.dec = 31; kn.sld = 0;
    /* 1: every cutoff / resonance / wave / note stays below the rails */
    for (w = 0; w < 2; w++)
        for (r = 0; r < 32; r += 3)
            for (c = 0; c < 32; c += 2)
                for (n = 24; n < 100; n += 12) {
                    int32_t pk;
                    kn.cut = (uint8_t)c; kn.res = (uint8_t)r; kn.wave = (uint8_t)w; kn.env = 0; kn.drv = 4;
                    pk = hold(n, 0, 24, 2, 0, &rms);
                    if (pk > worst) worst = pk;
                    if (pk > 32000) { clip++; if (clip < 6) printf("loud: wave %u res %u cut %u note %u peak %d\n", w, r, c, n, pk); }
                    if (pk == 0 && c > 4) { bad++; printf("silent: wave %u res %u cut %u note %u\n", w, r, c, n); }
                }
    printf("worst peak %d, loud %u, silent %u\n", worst, clip, bad);
    /* 2: level vs resonance at a middle cutoff, note A1 */
    kn.cut = 14; kn.wave = 0;
    for (r = 0; r < 32; r += 4) { kn.res = (uint8_t)r; hold(33, 0, 40, 4, 0, &rms); printf("res %2u  rms %5d\n", r, rms); }
    /* 3: level vs cutoff, no resonance */
    kn.res = 0;
    for (c = 0; c < 32; c += 4) { kn.cut = (uint8_t)c; hold(33, 0, 40, 4, 0, &rms); printf("cut %2u  rms %5d\n", c, rms); }
    /* 3b: the switches: level (rms) of A1 at a high resonance, each setting alone; BASS must lift it, the others must not break it */
    {
        static const struct { const char *name; uint8_t wave, pol; } M[] = {{"plain", 0, 31}, {"BASS 1", 4, 31}, {"BASS 2", 8, 31}, {"DIODE", 64, 31}, {"3 poles", 0, 0}, {"2 poles", 128, 0}, {"DRIFT 2", 32, 31}};
        uint32_t m; int32_t base = 0, bass2 = 0;
        kn.cut = 10; kn.res = 26; kn.env = 0;
        for (m = 0; m < sizeof M / sizeof M[0]; m++) {
            kn.wave = M[m].wave; kn.pol = M[m].pol;
            hold(33, 0, 60, 6, 0, &rms);
            printf("%-8s rms %5d\n", M[m].name, rms);
            if (m == 0) base = rms;
            if (m == 2) bass2 = rms;
            if (rms < 200) { printf("FAIL: %s is silent\n", M[m].name); return 1; }
        }
        if (bass2 <= base) { printf("FAIL: BASS does not bring the low end back\n"); return 1; }
        kn.wave = 0; kn.pol = 31;
    }
    /* 3c: the Devil Fish bits */
    {
        int32_t r1, r2, r3;
        static int32_t o[256 * 120];
        uint32_t i;
        kn.cut = 14; kn.res = 10; kn.env = 0; kn.wave = 0; kn.pol = 31; kn.hir = 0; kn.drv = 4;
        /* DECAY (the volume): a held note must fall away when it is short, and stay when it is 31 */
        for (i = 0; i < 3; i++) {
            static const uint8_t V[3] = {31, 6, 0};
            double e1 = 0, e2 = 0; uint32_t j;
            vol_dec = V[i];
            hold(45, 0, 120, 0, o, &r1);
            for (j = 300; j < 900; j++) e1 += (double)o[j] * o[j];
            for (j = 24000; j < 27000; j++) e2 += (double)o[j] * o[j];
            printf("DECAY %2u: first 20 ms rms %5d, at 0.55 s rms %5d\n", V[i], (int32_t)sqrt(e1 / 600), (int32_t)sqrt(e2 / 3000));
            if (i == 0 && e2 < e1 / 4) { printf("FAIL: DECAY 31 should hold the note\n"); return 1; }
            if (i == 2 && e2 > e1 / 100) { printf("FAIL: DECAY 0 should die away\n"); return 1; }
        }
        vol_dec = 31;
        /* FILT FM: turned up, it must change the sound (a knob that does nothing is a bug) */
        {
            static int32_t o2[256 * 120];
            double d2 = 0, e2 = 0; uint32_t j;
            kn.fm = 0; hold(45, 1, 120, 0, o, &r1);
            kn.fm = 31; hold(45, 1, 120, 0, o2, &r2);
            for (j = 2000; j < 30000; j++) { d2 += ((double)o[j] - o2[j]) * ((double)o[j] - o2[j]); e2 += (double)o[j] * o[j]; }
            printf("FILT FM 0 -> 31: the sound changes by %.2f (rms of the difference over the rms)\n", sqrt(d2 / e2));
            if (sqrt(d2 / e2) < 0.05) { printf("FAIL: FILT FM does nothing\n"); return 1; }
            kn.fm = 0;
        }
        /* DRIVE 0 is silence; with HI RES and a high resonance the filter rings alone */
        kn.drv = 0; kn.res = 31; kn.hir = 0;
        hold(45, 0, 120, 60, 0, &r1);
        kn.hir = 31;
        hold(45, 0, 120, 60, 0, &r2);
        kn.drv = 31; kn.hir = 0; kn.res = 10;
        hold(45, 0, 120, 60, 0, &r3);
        printf("DRIVE 0 res 31: rms %d, with HI RES: rms %d; DRIVE 31: rms %d\n", r1, r2, r3);
        if (r2 < 1500) { printf("FAIL: the filter does not ring alone with HI RES\n"); return 1; }
        if (r1 > r2 / 2) { printf("FAIL: HI RES makes no difference\n"); return 1; }
        kn.drv = 4; kn.hir = 0; kn.res = 20;
    }
    /* 3d: every knob and switch must do something: a pattern with accents, slides and a run of accents, each control moved from low to high */
    {
        static const struct { const char *name; uint32_t off; uint32_t lo, hi; } C[] = {
            {"CUTOFF", 0, 0, 31}, {"RESON", 1, 0, 31}, {"ENVMOD", 2, 0, 31}, {"NRM DEC", 3, 0, 31}, {"ACCENT", 4, 0, 31}, {"SLIDE", 5, 0, 31}, {"DRIVE", 6, 4, 31},
            {"ACC DEC", 8, 0, 31}, {"TRACK", 9, 0, 31}, {"ATTACK", 10, 0, 31}, {"MUFFLE", 11, 0, 31}, {"TUNE", 12, 0, 31}, {"SUB OSC", 13, 0, 31}, {"SWEEP", 14, 0, 31},
            {"POLES", 15, 0, 31}, {"FILT FM", 16, 0, 31}, {"ACC SWP: hi res", 17, 0, 31}, {"ACC SWP: no sweep", 17, 0, 15}, {"SWING", 18, 0, 31}, {"LEVEL", 19, 0, 31},
            {"WAVE", 100, 0, 1}, {"ACCENTS off", 101, 0, 1}, {"BASS", 102, 0, 2}, {"DRIFT", 103, 0, 2}, {"DIODE", 104, 0, 1}, {"TWO POLES", 105, 0, 1}, {"DECAY", 106, 12, 31},
        };
        static int32_t pa[256 * 300], pb[256 * 300];
        static const uint8_t N[16] = {36, 48, 39, 0, 43, 55, 36, 36, 60, 41, 0, 46, 36, 48, 52, 39}, FL[16] = {1, 0, 2, 0, 1, 1, 1, 0, 3, 0, 0, 2, 1, 0, 0, 0};
        uint32_t c, w, bad3 = 0;
        for (c = 0; c < sizeof C / sizeof C[0]; c++) {
            int32_t *dst[2] = {pa, pb};
            double d2 = 0, e2 = 0;
            for (w = 0; w < 2; w++) {
                uint32_t bl, ii, v = w ? C[c].hi : C[c].lo;
                reset(); memset(&kn, 0, sizeof kn);
                kn.cut = 14; kn.res = 20; kn.env = 18; kn.dec = 12; kn.acc = 16; kn.sld = 14; kn.drv = 8; kn.adc = 4; kn.trk = 12; kn.atk = 2; kn.tun = 16; kn.swp = 12; kn.pol = 31;
                vol_dec = 31;
                if (C[c].off < 20u) ((uint8_t *)&kn)[C[c].off] = (uint8_t)v;
                else if (C[c].off == 100u) kn.wave = (uint8_t)((kn.wave & ~1u) | v);
                else if (C[c].off == 101u) kn.wave = (uint8_t)((kn.wave & ~2u) | v << 1);
                else if (C[c].off == 102u) kn.wave = (uint8_t)((kn.wave & ~0x0Cu) | v << 2);
                else if (C[c].off == 103u) kn.wave = (uint8_t)((kn.wave & ~0x30u) | v << 4);
                else if (C[c].off == 104u) kn.wave = (uint8_t)((kn.wave & ~0x40u) | v << 6);
                else if (C[c].off == 105u) kn.wave = (uint8_t)((kn.wave & ~0x80u) | v << 7);
                else vol_dec = (uint8_t)v;
                for (ii = 0; ii < STEPS; ii++) { pats[0][ii].note = N[ii]; pats[0][ii].fl = FL[ii]; }
                plen[0] = 16; pat_idx = 0; seq_bpm = 126; seq_play();
                for (bl = 0; bl < 300u; bl++) { memset(buf, 0, sizeof buf); voice_render(buf, 256); for (ii = 0; ii < 256u; ii++) dst[w][bl * 256u + ii] = buf[2u * ii]; }
            }
            for (w = 0; w < 256u * 300u; w++) { d2 += ((double)pa[w] - pb[w]) * ((double)pa[w] - pb[w]); e2 += (double)pa[w] * pa[w]; }
            if (e2 < 1.0 || sqrt(d2 / e2) < 0.002) { bad3++; printf("FAIL: %s does nothing (%.4f)\n", C[c].name, e2 < 1.0 ? 0.0 : sqrt(d2 / e2)); }
        }
        printf("every control does something: %u of %u do nothing\n", bad3, (uint32_t)(sizeof C / sizeof C[0]));
        if (bad3) return 1;
        seq_stop(); vol_dec = 31;
    }
    /* 4: fuzz: random knobs, patterns and tempos while it plays; nothing may rail, stick or go quiet for good */
    {
        uint32_t seed = 12345, run, bl, i, rails = 0, dcbad = 0;
        for (run = 0; run < 300; run++) {
            uint8_t *kp = &kn.cut;
            double sum = 0; uint32_t cnt = 0;
            int32_t pk = 0;
            reset();
            for (i = 0; i < 19; i++) { seed = seed * 1664525u + 1013904223u; kp[i] = (uint8_t)((seed >> 12) % 32u); }
            vol_dec = (uint8_t)((seed >> 7) % 32u);
            kn.wave = (uint8_t)((seed >> 3) & 0xFDu);                      /* square, BASS, DRIFT, DIODE, two poles at random (3 = 1 to 2) */
            if ((kn.wave >> 2 & 3u) == 3u) kn.wave &= (uint8_t)~0x04u;
            if ((kn.wave >> 4 & 3u) == 3u) kn.wave &= (uint8_t)~0x10u;
            plen[0] = (uint8_t)(1 + (seed >> 5) % 16u); pat_idx = 0;
            for (i = 0; i < STEPS; i++) { seed = seed * 1664525u + 1013904223u; pats[0][i].note = (seed >> 9) % 4u ? (uint8_t)(24 + (seed >> 14) % 60u) : 0; pats[0][i].fl = (uint8_t)((seed >> 20) & 3u); }
            seed = seed * 1664525u + 1013904223u; seq_bpm = 60 + (seed >> 10) % 141u;
            seq_play();
            for (bl = 0; bl < 400; bl++) {
                memset(buf, 0, sizeof buf);
                voice_render(buf, 256);
                for (i = 0; i < 256; i++) { int32_t v = buf[2 * i]; if (abs(v) > pk) pk = abs(v); sum += v; cnt++; }
            }
            if (pk > 32000) { rails++; printf("run %u: peak %d (cut %u res %u drv %u muf %u)\n", run, pk, kn.cut, kn.res, kn.drv, kn.muf); }
            if (fabs(sum / cnt) > 2500) { dcbad++; printf("run %u: dc %.0f\n", run, sum / cnt); }
        }
        printf("fuzz: %u rails, %u dc offsets\n", rails, dcbad);
    }
    /* 5: cueing: a pattern cued while another plays starts when that one ends; a chain plays in order; stopping forgets the chain */
    {
        uint32_t bl, trace[64], n = 0, last = 99, ok = 1;
        reset();
        memset(pats, 0, sizeof pats);
        plen[0] = 4; plen[1] = 3; plen[2] = 2; pat_idx = 0;
        for (n = 0; n < 4; n++) { pats[0][n].note = 40; pats[1][n].note = 50; pats[2][n].note = 60; }
        seq_bpm = 120;
        cue_clear();
        seq_play();
        n = 0;
        for (bl = 0; bl < 700; bl++) {
            memset(buf, 0, sizeof buf);
            if (bl == 40) { cue_push(2); cue_push(1); cue_push(1); }      /* during the first loop of pattern 0 */
            voice_render(buf, 256);
            if (pat_idx != last && n < 64u) { trace[n++] = pat_idx; last = pat_idx; }
        }
        printf("cue chain: patterns played in order:");
        for (bl = 0; bl < n; bl++) printf(" %u", trace[bl]);
        printf("\n");
        if (n != 3u || cue_w != cue_r || trace[0] != 0u || trace[1] != 2u || trace[2] != 1u || ok == 0u) { printf("FAIL: expected 0 2 1 and an empty chain\n"); return 1; }
        seq_stop();
        if (cue_w != cue_r) { printf("FAIL: stop kept the chain\n"); return 1; }
        printf("cue ok\n");
    }
    /* 6: the FX rack: a delay's echo comes when TIME says, and no mix of units, knobs and notes rails or sticks */
    {
        uint32_t bl, k, i, t, runs, rails = 0, stuck = 0, dcbad = 0;
        static int32_t y[256 * 400];
        memset(&rack, 0, sizeof rack);
        rack.on = 1;
        for (t = 0; t < 32u; t += 15u) {                        /* TIME 0, 15, 30 */
            int32_t pk1 = 0, at = 0;
            uint32_t want, ms = 30u + t * t * 970u / 961u;
            memset(fxs, 0, sizeof fxs); fx_clear_line(0);
            fx_set(0, FX_DELAY, DL_DUB); fx_rst = 0;
            rack.u[0].p[0] = (uint8_t)t; rack.u[0].p[1] = 16; rack.u[0].p[2] = 31; rack.u[0].p[3] = 24;
            for (bl = 0; bl < 200u; bl++) {
                memset(buf, 0, sizeof buf);
                if (bl == 0) buf[0] = buf[1] = 20000;
                fx_process(buf, 256);
                for (i = 0; i < 256u; i++) y[bl * 256u + i] = buf[2u * i];
            }
            for (i = 100; i < 200u * 256u; i++) if (abs(y[i]) > pk1 && (i > ms * 441u / 10u / 2u)) { pk1 = abs(y[i]); at = (int32_t)i; if (i > ms * 441u / 10u * 3u / 2u) break; }
            want = ms * 441u / 10u;
            printf("delay TIME %u: %u ms, first echo at sample %d, wanted %u (peak %d)\n", t, ms, at, want, pk1);
            if (at < (int32_t)want * 9 / 10 || at > (int32_t)want * 11 / 10 + 80) { printf("FAIL: the echo is not where TIME says\n"); return 1; }
        }
        {                                                       /* the digital delay lands on the beat: 1/8 at 120 bpm is 250 ms */
            int32_t pk = 0, at = 0;
            seq_bpm = 120;
            memset(fxs, 0, sizeof fxs); fx_clear_line(0);
            fx_set(0, FX_DELAY, DL_DIGITAL); fx_rst = 0;
            rack.u[0].p[0] = 12; rack.u[0].p[1] = 8; rack.u[0].p[2] = 31; rack.u[0].p[3] = 31;
            for (bl = 0; bl < 100u; bl++) {
                memset(buf, 0, sizeof buf);
                if (bl == 0) buf[0] = buf[1] = 20000;
                fx_process(buf, 256);
                for (i = 0; i < 256u; i++) { int32_t v = abs(buf[2u * i]), at0 = (int32_t)(bl * 256u + i); if (at0 > 2000 && v > pk) { pk = v; at = at0; } }
            }
            printf("digital 1/8 at 120 bpm: echo at sample %d, wanted 11025 (peak %d)\n", at, pk);
            if (at < 10900 || at > 11200 || pk < 5000) { printf("FAIL: the digital delay is off the beat\n"); return 1; }
        }
        for (runs = 0; runs < 120u; runs++) {                   /* random racks over a playing bass */
            uint32_t sat = 0, tot = 0;
            double mean = 0;
            reset(); memset(&kn, 0, sizeof kn); kn.drv = 4; kn.cut = 14; kn.res = 18; kn.env = 18; kn.dec = 12; kn.tun = 16; kn.pol = 31; kn.dec = 8;
            memset(fxs, 0, sizeof fxs); fx_rst = 0;
            rack.on = 1;
            for (k = 0; k < FXN; k++) {
                fx_set(k, rand() % FX_KINDS, rand() % 4u);
                if (rack.u[k].type) { for (i = 0; i < 8u; i++) rack.u[k].p[i] = (uint8_t)(rand() % 32u); if (rack.u[k].type != FX_DELAY && rack.u[k].p[3] > 24u) rack.u[k].p[3] = 24; if (rand() % 3u == 0u) rack.u[k].bus = 1; if (k && rand() % 2u) rack.u[k].in = (uint8_t)(rand() % (k + 1u)); }
            }
            fx_sanitize(&rack);
            for (i = 0; i < STEPS; i++) { pats[0][i].note = (uint8_t)(rand() % 4u ? 36 + rand() % 24 : 0); pats[0][i].fl = (uint8_t)(rand() % 4); plen[0] = 16; }
            pat_idx = 0; seq_bpm = 90 + rand() % 90; seq_play();
            for (bl = 0; bl < 400u; bl++) {
                memset(buf, 0, sizeof buf);
                voice_render(buf, 256);
                fx_process(buf, 256);
                for (i = 0; i < 256u; i++) { int32_t v = buf[2u * i]; if (bl >= 100u) { if (abs(v) >= 32000) sat++; mean += v; tot++; } }
            }
            if (sat > tot / 3u) { stuck++; printf("rails:"); for (k = 0; k < FXN; k++) { printf(" [%u/%u in%u bus%u", rack.u[k].type, rack.u[k].sub, rack.u[k].in, rack.u[k].bus); for (i = 0; i < 4u; i++) printf(" %u", rack.u[k].p[i]); printf("]"); } printf("\n"); }
            if (fabs(mean / tot) > 4000.0) { dcbad++; printf("dc %.0f:", mean / tot); for (k = 0; k < FXN; k++) { printf(" [%u/%u in%u bus%u", rack.u[k].type, rack.u[k].sub, rack.u[k].in, rack.u[k].bus); for (i = 0; i < 8u; i++) printf(" %u", rack.u[k].p[i]); printf("]"); } printf("\n"); }
            for (i = 0; i < 256u; i++) if (abs(buf[2u * i]) > 32000) rails++;
        }
        printf("fx fuzz: %u runs mostly on the rails, %u dc offsets, %u samples past 32000\n", stuck, dcbad, rails);
        if (stuck || dcbad || rails) { printf("FAIL: the rack\n"); return 1; }
        printf("fx ok\n");
    }
    return 0;
}
