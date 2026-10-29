/* SPDX-License-Identifier: GPL-3.0-only */
/* Twelve pattern slots in flash: a pattern, the knobs and the tempo each. Two sectors take turns (a save writes the other one,
 * the header last, so a cut leaves the old bank standing). An empty slot shows a factory pattern, or nothing. */
#define SLOTS 12u
typedef struct { step_t pat[STEPS]; knobs_t kn; uint8_t bpm, used, pad[2]; } slot_t;      /* 48 bytes */
typedef struct { uint32_t magic, seq, crc, n; slot_t s[SLOTS]; } bank_t;
#define BANK_MAGIC 0x53464B31u
#define BANK_AT PROJ_AT
static bank_t bank;
static uint32_t bank_on;                                        /* which sector holds the bank now: 0 or 1 */

#define N(n, f) {n, f}
#define A F_ACC
#define S F_SLD
static const slot_t FACTORY[6] = {
    {{N(36, A), N(36, 0), N(48, S), N(39, 0), N(0, 0), N(36, 0), N(43, A | S), N(41, 0), N(36, A), N(0, 0), N(36, 0), N(48, S), N(51, 0), N(0, 0), N(46, A), N(43, S)},
     {14, 20, 18, 12, 16, 14, 4, 0, 4, 12, 2, 0, 16, 0, 12, 31, 0, 0, 0, 16}, 126, 1, {0}},
    {{N(36, 0), N(36, 0), N(0, 0), N(48, A), N(36, 0), N(0, 0), N(39, 0), N(0, 0), N(36, 0), N(36, A), N(51, 0), N(0, 0), N(48, S), N(0, 0), N(43, 0), N(46, A)},
     {10, 26, 24, 8, 22, 12, 6, 1, 3, 14, 0, 4, 16, 0, 12, 31, 0, 0, 0, 16}, 132, 1, {0}},
    {{N(36, 0), N(38, 0), N(39, A), N(41, 0), N(43, S), N(41, 0), N(39, 0), N(38, A), N(36, 0), N(0, 0), N(36, A), N(48, 0), N(0, 0), N(46, S), N(0, 0), N(43, 0)},
     {18, 14, 14, 16, 10, 16, 2, 0, 6, 10, 4, 0, 16, 0, 12, 31, 0, 0, 0, 16}, 120, 1, {0}},
    {{N(36, A), N(48, 0), N(36, 0), N(48, A), N(36, 0), N(48, 0), N(36, A), N(48, 0), N(36, 0), N(48, A), N(36, 0), N(48, 0), N(39, A), N(51, 0), N(41, 0), N(53, A)},
     {12, 22, 20, 6, 20, 6, 10, 0, 2, 8, 0, 8, 16, 0, 12, 31, 0, 0, 0, 16}, 138, 1, {0}},
    {{N(36, S), N(43, S), N(48, 0), N(0, 0), N(46, S), N(43, S), N(39, A), N(0, 0), N(36, S), N(48, S), N(51, A | S), N(48, S), N(43, 0), N(0, 0), N(41, A), N(0, 0)},
     {16, 24, 20, 14, 18, 22, 4, 0, 5, 14, 3, 2, 16, 0, 12, 31, 0, 0, 0, 16}, 124, 1, {0}},
    {{N(36, A), N(0, 0), N(0, 0), N(0, 0), N(36, 0), N(0, 0), N(0, 0), N(0, 0), N(36, A), N(0, 0), N(0, 0), N(36, 0), N(0, 0), N(0, 0), N(39, A), N(0, 0)},
     {8, 28, 26, 18, 24, 10, 2, 0, 8, 16, 6, 0, 16, 0, 12, 31, 0, 0, 0, 16}, 118, 1, {0}},
};
#undef N
#undef A
#undef S

#ifdef SMILEY_HOST
static uint32_t crc32(const uint8_t *p, uint32_t n)             /* (platform.c has it on the device) */
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) { c ^= *p++; for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1; }
    return ~c;
}
#endif
static uint32_t bank_crc(void) { return crc32((const uint8_t *)bank.s, sizeof bank.s); }
static int bank_try(uint32_t at, bank_t *b)
{
    if (pf_read(at, b, sizeof *b) != 0) return 0;
    if (b->magic != BANK_MAGIC || b->n != SLOTS) return 0;
    return crc32((const uint8_t *)b->s, sizeof b->s) == b->crc;
}
static void bank_load(void)                                     /* the newer of the two sectors, or an empty bank */
{
    static bank_t other;
    uint32_t a = bank_try(BANK_AT, &bank), b = bank_try(BANK_AT + 0x1000u, &other);
    if (a && b) { if (other.seq > bank.seq) { bank = other; bank_on = 1; } else bank_on = 0; }
    else if (b) { bank = other; bank_on = 1; }
    else if (a) bank_on = 0;
    else { memset(&bank, 0, sizeof bank); bank.magic = BANK_MAGIC; bank.n = SLOTS; bank_on = 1; }
}
static int bank_flush(void)                                     /* the other sector, header last */
{
    uint32_t at = BANK_AT + (bank_on ? 0u : 0x1000u);
    bank.seq++;
    bank.crc = bank_crc();
    if (pf_erase(at) != 0) return -1;
    if (pf_write(at + 16u, (const uint8_t *)bank.s, sizeof bank.s) != 0) return -1;
    if (pf_write(at, (const uint8_t *)&bank, 16u) != 0) return -1;
    bank_on ^= 1u;
    return 0;
}
static const slot_t *slot_get(uint32_t i)
{
    static const slot_t BLANK = {{{0, 0}}, {14, 20, 18, 12, 16, 14, 4, 0, 4, 12, 2, 0, 16, 0, 12, 31, 0, 0, 0, 16}, 126, 1, {0}};
    if (bank.s[i].used) return &bank.s[i];
    return i < 6u ? &FACTORY[i] : &BLANK;
}
