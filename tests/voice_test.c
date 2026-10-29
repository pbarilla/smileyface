/* SPDX-License-Identifier: GPL-3.0-only */
/* The 303 voice on its own: every cutoff x resonance x wave x note for stability and level, the filter's slope,
 * the envelope's shape. cc -O2 -Ifirmware/src -Ifirmware/gen -o build/voice_test tests/voice_test.c -lm && build/voice_test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../firmware/src/voice.c"

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
    memset(&kn, 0, sizeof kn);
    kn.dec = 31; kn.sld = 0;
    /* 1: every cutoff / resonance / wave / note stays below the rails */
    for (w = 0; w < 2; w++)
        for (r = 0; r < 32; r += 3)
            for (c = 0; c < 32; c += 2)
                for (n = 24; n < 100; n += 12) {
                    int32_t pk;
                    kn.cut = (uint8_t)c; kn.res = (uint8_t)r; kn.wave = (uint8_t)w; kn.env = 0; kn.drv = 0;
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
    /* 4: fuzz: random knobs, patterns and tempos while it plays; nothing may rail, stick or go quiet for good */
    {
        uint32_t seed = 12345, run, bl, i, rails = 0, dcbad = 0;
        for (run = 0; run < 300; run++) {
            uint8_t *kp = &kn.cut;
            double sum = 0; uint32_t cnt = 0;
            int32_t pk = 0;
            reset();
            for (i = 0; i < 20; i++) { seed = seed * 1664525u + 1013904223u; kp[i] = (uint8_t)((seed >> 12) % 32u); }
            kn.wave = (uint8_t)(kn.wave & 1);
            kn.len = (uint8_t)(1 + kn.len % 16);
            for (i = 0; i < STEPS; i++) { seed = seed * 1664525u + 1013904223u; pat[i].note = (seed >> 9) % 4u ? (uint8_t)(24 + (seed >> 14) % 60u) : 0; pat[i].fl = (uint8_t)((seed >> 20) & 3u); }
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
    return 0;
}
