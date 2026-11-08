/* SPDX-License-Identifier: GPL-3.0-only */
/* Twelve projects in flash: each has 16 patterns (and each pattern its length), the sound (the knobs), the tempo and the FX rack. Two copies of the bank
 * take turns (a save writes the other one, the header last, so a cut leaves the old bank standing); a copy is two 4 KB sectors.
 * An empty project shows the factory one (the first) or a blank. */
#define SLOTS 12u
typedef struct { step_t pats[PATS][STEPS]; uint8_t plen[PATS]; knobs_t kn; uint8_t bpm, used, pcur, pad; fxr_t fx; } slot_t;      /* 604 bytes (the first 552 were a whole project before the rack) */
#define SLOT_V1 552u
typedef struct { uint32_t magic, seq, crc, n; slot_t s[SLOTS]; } bank_t;
#define BANK_MAGIC 0x53464B33u
#define BANK_MAGIC_V1 0x53464B32u
#define BANK_AT PROJ_AT
#define BANK_SPAN 0x2000u                                       /* one copy: two sectors */
static bank_t bank;
static uint32_t bank_on;                                        /* which copy holds the bank now: 0 or 1 */

#define N(n, f) {n, f}
#define A F_ACC
#define S F_SLD
#define R0 {N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0), N(0, 0)}
static const slot_t FACTORY = {
    {
        {N(36, A), N(36, 0), N(48, S), N(39, 0), N(0, 0), N(36, 0), N(43, A | S), N(41, 0), N(36, A), N(0, 0), N(36, 0), N(48, S), N(51, 0), N(0, 0), N(46, A), N(43, S)},
        {N(36, 0), N(36, 0), N(0, 0), N(48, A), N(36, 0), N(0, 0), N(39, 0), N(0, 0), N(36, 0), N(36, A), N(51, 0), N(0, 0), N(48, S), N(0, 0), N(43, 0), N(46, A)},
        {N(36, 0), N(38, 0), N(39, A), N(41, 0), N(43, S), N(41, 0), N(39, 0), N(38, A), N(36, 0), N(0, 0), N(36, A), N(48, 0), N(0, 0), N(46, S), N(0, 0), N(43, 0)},
        {N(36, A), N(48, 0), N(36, 0), N(48, A), N(36, 0), N(48, 0), N(36, A), N(48, 0), N(36, 0), N(48, A), N(36, 0), N(48, 0), N(39, A), N(51, 0), N(41, 0), N(53, A)},
        {N(36, S), N(43, S), N(48, 0), N(0, 0), N(46, S), N(43, S), N(39, A), N(0, 0), N(36, S), N(48, S), N(51, A | S), N(48, S), N(43, 0), N(0, 0), N(41, A), N(0, 0)},
        {N(36, A), N(0, 0), N(0, 0), N(0, 0), N(36, 0), N(0, 0), N(0, 0), N(0, 0), N(36, A), N(0, 0), N(0, 0), N(36, 0), N(0, 0), N(0, 0), N(39, A), N(0, 0)},
        R0, R0, R0, R0, R0, R0, R0, R0, R0, R0,
    },
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {14, 20, 18, 12, 16, 14, 4, 0, 4, 12, 2, 0, 16, 0, 12, 31, 0, 0, 0, 0}, 126, 1, 0, 0,
    {0, FX_VER, {0, 0}, {{0}}},
};
static const slot_t BLANK = {
    {R0, R0, R0, R0, R0, R0, R0, R0, R0, R0, R0, R0, R0, R0, R0, R0},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {14, 20, 18, 12, 16, 14, 4, 0, 4, 12, 2, 0, 16, 0, 12, 31, 0, 0, 0, 0}, 126, 1, 0, 0,
    {0, FX_VER, {0, 0}, {{0}}},
};
#undef R0
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
static int bank_try_v1(uint32_t at, bank_t *b)                  /* a bank saved before the rack: 552 byte projects, they get an empty rack. It stays in flash until the first save. */
{
    static uint8_t raw[16u + SLOTS * SLOT_V1] __attribute__((section(".pool")));
    uint32_t i, magic, sq, crc, n;
    if (pf_read(at, raw, sizeof raw) != 0) return 0;
    memcpy(&magic, raw, 4); memcpy(&sq, raw + 4, 4); memcpy(&crc, raw + 8, 4); memcpy(&n, raw + 12, 4);
    if (magic != BANK_MAGIC_V1 || n != SLOTS || crc32(raw + 16, SLOTS * SLOT_V1) != crc) return 0;
    memset(b, 0, sizeof *b);
    b->magic = BANK_MAGIC; b->n = SLOTS; b->seq = sq;
    for (i = 0; i < SLOTS; i++) { memcpy(&b->s[i], raw + 16u + i * SLOT_V1, SLOT_V1); b->s[i].fx = BLANK.fx; }
    return 1;
}
static void bank_load(void)                                     /* the newer of the two copies, or an empty bank */
{
    static bank_t other;
    uint32_t a = bank_try(BANK_AT, &bank), b = bank_try(BANK_AT + BANK_SPAN, &other);
    if (a && b) { if (other.seq > bank.seq) { bank = other; bank_on = 1; } else bank_on = 0; }
    else if (b) { bank = other; bank_on = 1; }
    else if (a) bank_on = 0;
    else {
        uint32_t v0 = (uint32_t)bank_try_v1(BANK_AT, &bank), v1 = (uint32_t)bank_try_v1(BANK_AT + BANK_SPAN, &other);
        if (v1 && (!v0 || other.seq > bank.seq)) bank = other;
        else if (!v0) { memset(&bank, 0, sizeof bank); bank.magic = BANK_MAGIC; bank.n = SLOTS; }
        bank_on = 1;
    }
}
static int bank_flush(void)                                     /* the other copy, header last */
{
    uint32_t at = BANK_AT + (bank_on ? 0u : BANK_SPAN);
    bank.seq++;
    bank.crc = bank_crc();
    if (pf_erase(at) != 0 || pf_erase(at + 0x1000u) != 0) return -1;
    if (pf_write(at + 16u, (const uint8_t *)bank.s, sizeof bank.s) != 0) return -1;
    if (pf_write(at, (const uint8_t *)&bank, 16u) != 0) return -1;
    bank_on ^= 1u;
    return 0;
}
/* KOROBEINIKI, project 1: an acid line under Chipbox's demo 1 (the same tune, A minor, 144 BPM, one bar a pattern: E A E A D C E A). Built when asked, never stored; the player's own project 1 moves to a free slot at start-up. */
#define KX 255u                                                 /* a rest */
#define KT 100u                                                 /* the chord's third */
static const slot_t *korob(void)
{
    static slot_t k;
    static uint8_t built;
    static const uint8_t ROOT[8] = {40, 45, 40, 45, 38, 36, 40, 45}, THIRD[8] = {4, 3, 4, 3, 3, 4, 4, 3}, KIND[8] = {0, 0, 0, 1, 2, 2, 0, 1};
    static const uint8_t IV[3][16] = {{0, 0, 12, 7, KX, 0, 12, KX, 0, KX, 7, 12, KX, 0, KT, 7},                    /* the verse */
                                      {0, KX, 0, KX, 0, KX, 12, 12, 7, KX, KT, 7, 12, KX, 12, 7},                  /* the fill */
                                      {0, 0, 12, 0, 0, 0, 12, 7, 0, 0, KT, 7, 0, 12, KT, 7}};                    /* the lift */
    static const uint8_t FL[3][16] = {{F_ACC, 0, F_SLD, 0, 0, 0, F_ACC, 0, F_ACC, 0, F_SLD, 0, 0, 0, F_ACC, 0},
                                      {F_ACC, 0, 0, 0, F_ACC, 0, 0, F_SLD, F_ACC, 0, 0, F_SLD, F_ACC, 0, F_SLD, 0},
                                      {F_ACC, 0, 0, 0, F_ACC, 0, F_SLD, 0, F_ACC, 0, 0, 0, F_ACC, 0, F_SLD, 0}};
    uint32_t p, i;
    if (built) return &k;
    k = FACTORY;
    for (p = 0; p < PATS; p++) {
        k.plen[p] = 16;
        for (i = 0; i < STEPS; i++) {
            uint32_t iv = p < 8u ? IV[KIND[p]][i] : KX;
            k.pats[p][i].note = iv == KX ? 0u : (uint8_t)(ROOT[p] + (iv == KT ? THIRD[p] : iv));
            k.pats[p][i].fl = iv == KX ? 0u : FL[KIND[p]][i];
        }
    }
    k.kn.cut = 13; k.kn.res = 22; k.bpm = 144; k.used = 1; k.pcur = 0;
    built = 1;
    return &k;
}
static const slot_t *slot_get(uint32_t i)
{
    if (i == 0u) return korob();
    if (bank.s[i].used) return &bank.s[i];
    return i == 1u ? &FACTORY : &BLANK;                          /* (the factory acid pattern used to be project 1) */
}
static void bank_demo_room(void)                                /* the player's own project 1 (an older firmware) moves to the last free slot */
{
    uint32_t f;
    if (!bank.s[0].used) return;
    for (f = SLOTS - 1u; f > 0u && bank.s[f].used; f--) {}
    if (f == 0u) return;                                        /* no room: it stays where it is, hidden, and moves when there is */
    bank.s[f] = bank.s[0]; bank.s[0].used = 0;
    bank_flush();
}
