/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer's chain and the saved chain records, on their own: the first tick of a run plays step 1 of the pattern you are on (a run no longer
 * skips ahead), PLAY in the PLAY view follows the chain, anywhere else one pattern loops, a cued pattern goes in between and the chain goes on,
 * a Brothers Link follower is left to the leader; a chain record survives a round trip and an old (erased or damaged) record reads as no chain.
 *   cc -O2 -DSMILEY_HOST -Ifirmware/src -Ifirmware/gen -o build/chain_test tests/chain_test.c -lm && build/chain_test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static void midi_send(uint32_t a, uint32_t b, uint32_t c) { (void)a; (void)b; (void)c; }
static void midi_isr_send(uint32_t a, uint32_t b, uint32_t c) { (void)a; (void)b; (void)c; }
static int32_t bfx_pitch;
#include "../firmware/src/voice.c"
#define __attribute__(x)
#include "../firmware/src/fx.c"
#undef __attribute__
#define PROJ_AT 0u
static int pf_read(uint32_t o, void *d, uint32_t n) { (void)o; (void)d; (void)n; return -1; }
static int pf_erase(uint32_t o) { (void)o; return 0; }
static int pf_write(uint32_t o, const void *p, uint32_t n) { (void)o; (void)p; (void)n; return 0; }
#define __attribute__(x)
#include "../firmware/src/bank.c"
#undef __attribute__

static int fail;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fail = 1; } } while (0)
static int32_t buf[2 * 256];
static uint32_t log_pat[4096], log_step[4096], logn;
static void run_blocks(uint32_t n)                              /* render n chunks of 8 samples, and note every step change (pattern, step) */
{
    static uint32_t last = 999;
    uint32_t b;
    for (b = 0; b < n; b++) {
        uint32_t s;
        memset(buf, 0, sizeof buf);
        voice_render(buf, 8);
        s = (uint32_t)(pat_idx * 100u + seq_step);
        if (s != last && seq_run && logn < 4096u) { log_pat[logn] = pat_idx; log_step[logn] = seq_step; logn++; last = s; }
    }
}
static void start(uint32_t chain_view)
{
    logn = 0; seq_run = 0; seq_stop(); seq_bpm = 6000;          /* fast: a step is about a tenth of a block */
    seq_wantchain = (uint8_t)chain_view; seq_play();
}
static uint32_t patterns_seen(uint32_t *out)                    /* the patterns in order of play (each pattern once per run of steps) */
{
    uint32_t i, n = 0;
    for (i = 0; i < logn; i++) if (log_step[i] == 0u) out[n++] = log_pat[i];
    return n;
}
int main(void)
{
    uint32_t seen[512], n, i;
    uint8_t d[8] = {3, 1, 3, 4}, rec[CHAIN_REC];
    memset(plen, 16, sizeof plen);
    /* 1. a run starts on the pattern you are on, at step 1 (the old start skipped to the next one when one was cued or the demo ring was on) */
    pat_idx = 5; chain_n = 0; cue_clear();
    start(0); run_blocks(600);
    CHECK(logn && log_pat[0] == 5u && log_step[0] == 0u, "first tick is pattern %u step %u, want 5 and 0", log_pat[0], log_step[0]);
    n = patterns_seen(seen);
    for (i = 0; i < n; i++) CHECK(seen[i] == 5u, "a single pattern loops: saw %u", seen[i]);
    /* 2. a cue while a single pattern loops: it plays next, then it is the one that loops */
    pat_idx = 2; cue_clear();
    start(0); run_blocks(100); cue_push(7); run_blocks(900);
    n = patterns_seen(seen);
    CHECK(n >= 3u && seen[0] == 2u && seen[1] == 7u && seen[2] == 7u, "cue: %u %u %u, want 2 7 7", seen[0], seen[1], seen[2]);
    /* 3. the chain, started from the PLAY view: its first entry, in order, round again */
    chain[0] = 3; chain[1] = 1; chain[2] = 3; chain[3] = 4; chain_n = 4; pat_idx = 9; cue_clear();
    start(1); run_blocks(2400);
    n = patterns_seen(seen);
    CHECK(n >= 9u, "chain: only %u patterns", n);
    for (i = 0; i < n && i < 9u; i++) CHECK(seen[i] == d[i % 4u], "chain slot %u plays %u, want %u", i, seen[i], d[i % 4u]);
    /* 4. the same chain, PLAY pressed anywhere else: only the pattern you are on */
    pat_idx = 6;
    start(0); run_blocks(900);
    n = patterns_seen(seen);
    for (i = 0; i < n; i++) CHECK(seen[i] == 6u, "single mode played %u", seen[i]);
    /* 5. a cue while the chain plays: it goes in between, the chain carries on where it was */
    pat_idx = 0; cue_clear();
    start(1); run_blocks(100);
    cue_push(12);
    run_blocks(1400);
    n = patterns_seen(seen);
    CHECK(n >= 5u && seen[0] == 3u && seen[1] == 12u && seen[2] == 1u && seen[3] == 3u, "cue in a chain: %u %u %u %u, want 3 12 1 3", seen[0], seen[1], seen[2], seen[3]);
    /* 6. an empty chain started from PLAY loops the pattern */
    chain_n = 0; pat_idx = 8; cue_clear();
    start(1); run_blocks(700);
    n = patterns_seen(seen);
    for (i = 0; i < n; i++) CHECK(seen[i] == 8u, "empty chain played %u", seen[i]);
    /* 7. a follower that follows the leader's pattern does not run its own chain */
    chain[0] = 1; chain[1] = 2; chain_n = 2; pat_idx = 1; brothers_mode = 2; bt_patsync = 1;
    start(1); run_blocks(700);
    n = patterns_seen(seen);
    for (i = 0; i < n; i++) CHECK(seen[i] == 1u, "follower ran its chain: %u", seen[i]);
    brothers_mode = 0; bt_patsync = 0; seq_stop();
    /* 8. the saved record */
    chain_rec_put(rec, d, 4u);
    CHECK(chain_rec_n(rec) == 4u && rec[1] == 3u && rec[4] == 4u, "a record round trip");
    memset(rec, 0xFF, sizeof rec); CHECK(chain_rec_n(rec) == 0u, "erased bytes (an older bank) are no chain");
    memset(rec, 0, sizeof rec); CHECK(chain_rec_n(rec) == 0u, "zeros are no chain");
    chain_rec_put(rec, d, 4u); rec[2] ^= 1u; CHECK(chain_rec_n(rec) == 0u, "a damaged record is no chain");
    chain_rec_put(rec, d, 4u); rec[1] = 99u; rec[CHAIN_MAX + 1u] = chain_sum(rec); CHECK(chain_rec_n(rec) == 0u, "a pattern number past 16 is no chain");
    { uint8_t big[20]; for (i = 0; i < 20u; i++) big[i] = (uint8_t)(i % 16u); chain_rec_put(rec, big, 20u); CHECK(chain_rec_n(rec) == CHAIN_MAX, "a long chain is cut at 16"); }
    chain_rec_put(rec, d, 0u); CHECK(chain_rec_n(rec) == 0u, "an empty chain is no chain");
    printf(fail ? "FAIL chain_test\n" : "chain_test ok\n");
    return fail;
}
