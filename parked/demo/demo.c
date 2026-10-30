/* SPDX-License-Identifier: GPL-3.0-only */
/* Demos, as in Doom: a performance is what you did, and when. REC, then PLAY, then play; REC again keeps it, with the project. In the project list,
 * PLAY on a project plays its demo back through the same code: the start (the project as it was) and a list of what changed at each tick (a knob
 * turned, a step written, a pattern cued, a whole pattern replaced, the tempo). Time is the music's: 1/8 of a step. Included by smileyface.c
 * after pat_pick. */
#define DEMO_AT (BANK_AT + 0x4000u)                             /* after the bank's two copies */
#define DEMO_SPAN 0x4000u                                       /* 16 KB a project: four sectors */
#define DEMO_MAGIC 0x53464431u                                  /* "SFD1" */
#define DEMO_EV_MAX (DEMO_SPAN - 16u - (uint32_t)sizeof(slot_t))
typedef struct { uint32_t magic, len, end_t, crc; } demo_hdr_t;

static struct { slot_t snap; uint8_t ev[DEMO_EV_MAX]; } demo_img __attribute__((section(".pool")));      /* the project at the start, then the events */
static uint32_t demo_n, demo_pos, demo_end, demo_pending, demo_req, demo_act0, demo_view0;
enum { E_KNOB = 1, E_STEP, E_PAT, E_CUEKILL, E_BULK, E_VIEW, E_BPM };
static uint32_t ev_len(uint32_t type)                           /* bytes in an event after its type and time */
{
    switch (type) {
    case E_KNOB: return 2;
    case E_STEP: return 3;
    case E_PAT: return 1;
    case E_CUEKILL: return 0;
    case E_BULK: return 2u + 32u;
    case E_VIEW: return 1;
    case E_BPM: return 1;
    default: return 99;
    }
}
static uint32_t demo_t8(void)                                   /* the music's clock: 1/8 of a step since PLAY */
{
    uint32_t fr = ext_on ? ext_phase * 8u / 6u : sq_acc * 8u / 661500u;
    return seq_total * 8u + (fr > 7u ? 7u : fr);
}
static uint32_t demo_hash(void)                                 /* what plays, as one number (for the tests) */
{
    slot_t t;
    memset(&t, 0, sizeof t);
    slot_pack(&t);
    t.used = 0;
    return crc32((const uint8_t *)&t, sizeof t);
}
static void demo_stop_rec(void)
{
    if (demo_state != DS_REC && demo_state != DS_ARMED) return;
    demo_end = demo_t8();
#ifdef DEMO_DEBUG
    printf("demo rec: %u bytes, ends at %u: state hash %08x\n", demo_n, demo_end, demo_hash());
#endif
    demo_pending = demo_state == DS_REC && demo_n > 0u;         /* saved when it is quiet (flash stalls the sound) */
    demo_state = DS_IDLE;
}
static void demo_put(uint32_t type, const uint8_t *p, uint32_t n)
{
    uint32_t t;
    if (demo_state != DS_REC || demo_replay) return;
    if (demo_n + 3u + n > DEMO_EV_MAX) { demo_stop_rec(); return; }                 /* full: keep what there is */
    t = demo_t8();
    demo_img.ev[demo_n++] = (uint8_t)type;
    demo_img.ev[demo_n++] = (uint8_t)t;
    demo_img.ev[demo_n++] = (uint8_t)(t >> 8);
    memcpy(demo_img.ev + demo_n, p, n);
    demo_n += n;
}
static void rec_knob(uint32_t page, uint32_t k, int32_t e) { uint8_t p[2] = {(uint8_t)(page * 4u + k), (uint8_t)(int8_t)(e < -127 ? -127 : e > 127 ? 127 : e)}; demo_put(E_KNOB, p, 2); }
static void rec_step(uint32_t step, uint32_t aud) { uint8_t p[3] = {(uint8_t)step, PAT[step % STEPS].note, (uint8_t)(PAT[step % STEPS].fl | (aud ? 0x80u : 0u))}; demo_put(E_STEP, p, 3); }
static void rec_pat(uint32_t pt) { uint8_t p[1] = {(uint8_t)pt}; demo_put(E_PAT, p, 1); }
static void rec_cuekill(void) { demo_put(E_CUEKILL, 0, 0); }
static void rec_bulk(void) { uint8_t p[34]; p[0] = (uint8_t)pat_idx; p[1] = plen[pat_idx]; memcpy(p + 2, PAT, 32); demo_put(E_BULK, p, 34); }
static void rec_view(uint32_t v) { uint8_t p[1] = {(uint8_t)v}; demo_put(E_VIEW, p, 1); }
static void rec_bpm(void) { uint8_t p[1] = {(uint8_t)seq_bpm}; demo_put(E_BPM, p, 1); }

static void demo_apply(uint32_t type, const uint8_t *p)         /* an event happens again */
{
    demo_replay = 1;
    switch (type) {
    case E_KNOB: knob_turn(p[0] / 4u % PAGES, p[0] % 4u, (int8_t)p[1]); break;
    case E_STEP: {
        uint32_t st = p[0] % STEPS;
        PAT[st].note = p[1]; PAT[st].fl = p[2] & 3u;
        if (p[2] & 0x80u) { if (p[1]) voice_on(p[1], p[2] & F_ACC, 0); cur = (st + 1u) % STEPS; }
        break;
    }
    case E_PAT: pat_pick(p[0]); break;
    case E_CUEKILL: cue_clear(); break;
    case E_BULK: if (p[0] < PATS) { uint32_t i; for (i = 0; i < STEPS; i++) { pats[p[0]][i].note = p[2u + i * 2u]; pats[p[0]][i].fl = p[3u + i * 2u] & 3u; } plen[p[0]] = p[1] >= 1u && p[1] <= STEPS ? p[1] : STEPS; } break;
    case E_VIEW: view_now = p[0] % VIEWS; break;
    case E_BPM: seq_bpm = p[0] < 60u ? 60u : p[0] > 200u ? 200u : p[0]; break;
    default: break;
    }
    demo_replay = 0;
}

static uint32_t demo_addr(uint32_t slot) { return DEMO_AT + slot * DEMO_SPAN; }
static void demo_scan(void)                                     /* which projects have a demo */
{
    uint32_t s;
    demo_has = 0;
    for (s = 0; s < SLOTS; s++) {
        demo_hdr_t h;
        if (pf_read(demo_addr(s), &h, sizeof h) == 0 && h.magic == DEMO_MAGIC && h.len <= DEMO_EV_MAX) demo_has |= 1u << s;
    }
}
static int demo_validate(const uint8_t *ev, uint32_t n)         /* the events are whole and make sense */
{
    uint32_t i = 0;
    while (i < n) {
        uint32_t l = ev_len(ev[i]);
        if (l == 99u || i + 3u + l > n) return 0;
        if (ev[i] == E_KNOB && (ev[i + 3u] / 4u >= PAGES)) return 0;
        i += 3u + l;
    }
    return 1;
}
static int demo_write_flash(uint32_t slot, const uint8_t *img, uint32_t len, uint32_t end_t)       /* img: the project at the start, then len bytes of events; header last */
{
    uint32_t at = demo_addr(slot), i;
    demo_hdr_t h;
    h.magic = DEMO_MAGIC; h.len = len; h.end_t = end_t;
    h.crc = crc32(img, (uint32_t)sizeof(slot_t) + len);
    for (i = 0; i < 4u; i++) if (pf_erase(at + i * 0x1000u) != 0) return -1;
    if (pf_write(at + 16u, img, (uint32_t)sizeof(slot_t) + len) != 0) return -1;
    if (pf_write(at, (const uint8_t *)&h, sizeof h) != 0) return -1;
    demo_has |= 1u << slot;
    return 0;
}
static int demo_save(uint32_t slot) { return demo_write_flash(slot, (const uint8_t *)&demo_img, demo_n, demo_end); }
static void demo_erase(uint32_t slot) { pf_erase(demo_addr(slot)); demo_has &= ~(1u << slot); }
static int demo_load(uint32_t slot)                             /* from flash into demo_img: 1 if it is whole */
{
    demo_hdr_t h;
    if (pf_read(demo_addr(slot), &h, sizeof h) != 0 || h.magic != DEMO_MAGIC || h.len > DEMO_EV_MAX) return 0;
    if (pf_read(demo_addr(slot) + 16u, &demo_img, (uint32_t)sizeof(slot_t) + h.len) != 0) return 0;
    if (crc32((const uint8_t *)&demo_img, (uint32_t)sizeof(slot_t) + h.len) != h.crc || !demo_validate(demo_img.ev, h.len)) return 0;
    demo_n = h.len; demo_end = h.end_t;
    return 1;
}
static void demo_snapshot(void) { memset(&demo_img.snap, 0, sizeof demo_img.snap); slot_pack(&demo_img.snap); demo_n = 0; }
static void demo_start(uint32_t slot)
{
    if (!demo_load(slot)) return;
    seq_stop();
    demo_view0 = view_now;
    demo_replay = 1;
    slot_apply(&demo_img.snap);                                 /* (slot_cur stays: it is the user's project that comes back) */
    demo_replay = 0;
    demo_pos = 0;
    demo_state = DS_PLAY;
    seq_play();
}
static void demo_stop_play(void)                                /* the end, or a touch: the user's own project comes back */
{
    if (demo_state != DS_PLAY) return;
#ifdef DEMO_DEBUG
    printf("demo end: state hash %08x\n", demo_hash());
#endif
    demo_state = DS_IDLE;
    seq_stop();
    slot_apply(slot_get(slot_cur));
    view_now = demo_view0;
}
static void demo_run(void)                                      /* every pass of the main loop while a demo plays */
{
    uint32_t t = demo_t8();
    while (demo_pos < demo_n) {
        uint32_t type = demo_img.ev[demo_pos], tev = (uint32_t)demo_img.ev[demo_pos + 1u] | (uint32_t)demo_img.ev[demo_pos + 2u] << 8;
        if (tev > t) break;
        demo_apply(type, demo_img.ev + demo_pos + 3u);
        demo_pos += 3u + ev_len(type);
    }
    if (t >= demo_end) demo_stop_play();
}
