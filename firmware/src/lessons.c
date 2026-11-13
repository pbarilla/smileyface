/* SPDX-License-Identifier: GPL-3.0-only */
/* LESSONS: the first line of the menu (GLO + HOME). A lesson works on a scratch project; your own comes back when it ends and nothing a lesson does is saved.
 * A card over the knobs gives one instruction at a time with the control to use in a pill; a step finishes by itself when what it asks for has happened,
 * the card goes green, the next step shows. GLO hides the card and brings it back. Opening the menu or the projects ends the lesson. Included by smileyface.c after menu.c. */
enum { LV_VIEW, LV_NOTES, LV_ACC, LV_SLD, LV_CUR, LV_ONNOTE, LV_RUN, LV_BPM, LV_PAT, LV_PHASH, LV_PAGE, LV_CHAIN, LV_FX, LV_FXM, LV_LEN, LV_KALL, LV_CUT, LV_RES, LV_ENVM, LV_DEC,
       LV_READ, LV_KN, LV_PAD, LV_OCT, LV_PATS, LV_PFXB, LV_LVL, LV_KEYS };      /* LV_READ: a page to read (OCT+ goes on); LV_KN: a knob's byte (a: its offset in knobs_t, 200 the volume decay); LV_PAD: a switch's setting (a: the pad) */
enum { LO_EQ, LO_GE, LO_CHG, LO_UP, LO_DOWN };
enum { LS_EMPTY, LS_MELODY, LS_TWO, LS_MC, LS_PATCH };      /* LS_MC: a masterclass (empty; its pages set the sound up); LS_PATCH: a lesson that starts from a patch (lesson_t.patch) */
typedef struct { const char *text, *pill; uint8_t v, op, thr, a; } lstep_t;      /* a read page: text is the page, pill its title, thr the patch to set up when it shows */
typedef struct { const char *name; uint8_t setup, n; const lstep_t *s; uint8_t mc, patch; } lesson_t;      /* mc: a masterclass (1..); patch: the start of an LS_PATCH lesson */
#define ST(t, p, v, o, n) {t, p, LV_##v, LO_##o, n, 0}
#define STA(t, p, v, o, n, a) {t, p, LV_##v, LO_##o, n, a}
#define RD(title, text, patch) {text, title, LV_READ, LO_EQ, patch, 0}
static const lesson_t LESSON[] = {
    {"Get Around", LS_EMPTY, 4, (const lstep_t[]){ST("Tap HOME for another view", "HOME", VIEW, CHG, 0), ST("Tap HOME for another view", "HOME", VIEW, CHG, 0), ST("Tap HOME for another view", "HOME", VIEW, CHG, 0), ST("Tap HOME until STEPS shows", "HOME", VIEW, EQ, 0)}},
    {"First Notes", LS_EMPTY, 6, (const lstep_t[]){ST("Press a key to add a note", "KEYS", NOTES, GE, 1), ST("Press 3 more keys", "KEYS", NOTES, GE, 4), ST("Turn SELECT left to a note", "SELECT", ONNOTE, EQ, 1),
        ST("Press EDIT to make a rest", "EDIT", NOTES, DOWN, 0), ST("Press PLAY to hear it", "PLAY", RUN, EQ, 1), ST("Press PLAY to stop", "PLAY", RUN, EQ, 0)}},
    {"Accents", LS_MELODY, 5, (const lstep_t[]){ST("Press ENV to accent a step", "ENV", ACC, UP, 0), ST("Turn SELECT to move cursor", "SELECT", CUR, CHG, 0), ST("Press ENV to accent again", "ENV", ACC, UP, 0),
        ST("Press PLAY to hear them", "PLAY", RUN, EQ, 1), ST("Press PLAY to stop", "PLAY", RUN, EQ, 0)}},
    {"Slides", LS_MELODY, 4, (const lstep_t[]){ST("Tap SEL to slide a step", "SEL", SLD, UP, 0), ST("Turn SELECT to move cursor", "SELECT", CUR, CHG, 0), ST("Tap SEL to slide again", "SEL", SLD, UP, 0),
        ST("Press PLAY to hear slides", "PLAY", RUN, EQ, 1)}},
#include "lessons_mc_3.h"
    {"Rests and Length", LS_MELODY, 4, (const lstep_t[]){ST("Press EDIT to make a rest", "EDIT", NOTES, DOWN, 0), ST("Turn ALGORITHM 3 pages on", "ALGORITHM", PAGE, EQ, 3), ST("Turn KNOB 3 for LENGTH", "KNOB 3", LEN, CHG, 0),
        ST("Press PLAY to hear it", "PLAY", RUN, EQ, 1)}},
#include "lessons_mc_1.h"
    {"Twist the Filter", LS_MELODY, 5, (const lstep_t[]){ST("Press PLAY to start", "PLAY", RUN, EQ, 1), ST("Turn KNOB 1 for CUTOFF", "KNOB 1", CUT, CHG, 0), ST("Turn KNOB 2 for RESON", "KNOB 2", RES, CHG, 0),
        ST("Turn KNOB 3 for ENVMOD", "KNOB 3", ENVM, CHG, 0), ST("Turn KNOB 4 for NRM DEC", "KNOB 4", DEC, CHG, 0)}},
#include "lessons_mc_2.h"
    {"Tempo", LS_MELODY, 4, (const lstep_t[]){ST("Turn ALGORITHM one page on", "ALGORITHM", PAGE, EQ, 1), ST("Turn KNOB 4 for TEMPO", "KNOB 4", BPM, CHG, 0), ST("Press PLAY to start", "PLAY", RUN, EQ, 1),
        ST("Tap SEQ 4 times in rhythm", "SEQ", BPM, CHG, 0)}},
    {"Devil Fish", LS_MELODY, 5, (const lstep_t[]){ST("Press PLAY to start", "PLAY", RUN, EQ, 1), ST("Turn ALGORITHM 2 pages on", "ALGORITHM", PAGE, EQ, 2), ST("Turn KNOB 1 for DRIVE", "KNOB 1", KALL, CHG, 0),
        ST("Turn KNOB 2 for ACC DEC", "KNOB 2", KALL, CHG, 0), ST("Turn KNOB 3 for TRACK", "KNOB 3", KALL, CHG, 0)}},
#include "lessons_mc_4.h"
    {"Switches", LS_MELODY, 5, (const lstep_t[]){ST("Press PLAY to start", "PLAY", RUN, EQ, 1), ST("Turn ALGORITHM 4 pages on", "ALGORITHM", PAGE, EQ, 4), ST("Turn KNOB 1 right: WAVE", "KNOB 1", KALL, CHG, 0),
        ST("Turn KNOB 2 right: SUB OSC", "KNOB 2", KALL, CHG, 0), ST("Turn KNOB 3 right: MUFFLE", "KNOB 3", KALL, CHG, 0)}},
#include "lessons_mc_5.h"
    {"Patterns", LS_TWO, 4, (const lstep_t[]){ST("Turn PRESETS to pattern 2", "PRESETS", PAT, CHG, 0), ST("Press a key to add a note", "KEYS", PHASH, CHG, 0), ST("Turn PRESETS to pattern 1", "PRESETS", PAT, EQ, 0),
        ST("Press PLAY to hear it", "PLAY", RUN, EQ, 1)}},
    {"Chain", LS_TWO, 5, (const lstep_t[]){ST("Tap HOME until PLAY shows", "HOME", VIEW, EQ, 3), ST("Press OCT+ to add to chain", "OCT+", CHAIN, UP, 0), ST("Turn PRESETS to pattern 2", "PRESETS", PAT, CHG, 0),
        ST("Press OCT+ to add it too", "OCT+", CHAIN, UP, 0), ST("Press PLAY to hear chain", "PLAY", RUN, EQ, 1)}},
    {"Dice and Undo", LS_MELODY, 5, (const lstep_t[]){ST("Press LFO to randomize", "LFO", PHASH, CHG, 0), ST("Press REC to undo", "REC", PHASH, CHG, 0), ST("Press ARP to mutate", "ARP", PHASH, CHG, 0),
        ST("Press REC to undo again", "REC", PHASH, CHG, 0), ST("Press PLAY to hear it", "PLAY", RUN, EQ, 1)}},
    {"Punch-in FX", LS_MELODY, 4, (const lstep_t[]){ST("Press PLAY to start", "PLAY", RUN, EQ, 1), ST("Press FX to show the pads", "FX", FX, EQ, 1), ST("Hold a white key for an FX", "KEYS", FXM, EQ, 1),
        ST("Press FX to leave", "FX", FX, EQ, 0)}},
#include "lessons_guided.h"
};
#define LES_N ((uint32_t)(sizeof LESSON / sizeof LESSON[0]))
static slot_t les_slot;                                         /* the player's project, while a lesson runs */
static uint8_t les_chain[CHAIN_MAX];
static uint32_t les_on, les_i, les_step, les_ok, les_ok_t, les_fin, les_hide, les_pick, les_snap, les_snap_step = ~0u, les_kbase, les_chn, les_dirty, les_view, les_redraw;
static char les_txt[40];
static uint8_t les_dn[4], les_oct;                                       /* a bit a lesson: finished (kept in the settings) */
static uint32_t les_saved, les_top;
static uint32_t les_ndone(void) { uint32_t i, n = 0; for (i = 0; i < LES_N; i++) n += les_dn[i >> 3] >> (i & 7u) & 1u; return n; }
static uint32_t les_u(char *s, uint32_t v) { uint32_t n = v >= 10u ? 2u : 1u; if (v >= 10u) s[0] = (char)('0' + v / 10u % 10u); s[n - 1u] = (char)('0' + v % 10u); return n; }
static const char *const LES_PRAISE[9] = {"Nice!", "Well done!", "Way to go!", "Good!", "Got it!", "Yes!", "Sweet!", "Nailed it!", "Easy!"};

static const char *les_row(void)                                /* the menu's first line */
{
    const char *p = "Lessons: ";
    uint32_t i = 0;
    while (*p) les_txt[i++] = *p++;
    i += les_u(les_txt + i, les_ndone()); les_txt[i++] = '/'; i += les_u(les_txt + i, LES_N);
    for (p = " done"; *p; p++) les_txt[i++] = *p;
    les_txt[i] = 0;
    return les_txt;
}
static void les_list_open(void)                                 /* it opens on the first lesson you have not done */
{
    uint32_t i = 0;
    while (i < LES_N && (les_dn[i >> 3] >> (i & 7u) & 1u)) i++;
    les_pick = i < LES_N ? i : 0u;
}
static void scene_lessons(void)                                 /* the list: a tick and green text on the ones you finished */
{
    const int32_t x = 6, w = 228, y = 18, rows = 9;
    uint32_t i, top = les_top;
    char t[24];
    uint32_t k = 0;
    if (les_pick < top) top = les_pick; else if (les_pick >= top + (uint32_t)rows) top = les_pick + 1u - (uint32_t)rows;
    les_top = top;
    for (const char *p = "Lessons  "; *p; p++) t[k++] = *p;
    k += les_u(t + k, les_ndone()); t[k++] = '/'; k += les_u(t + k, LES_N); t[k] = 0;
    menu_win(t, C_Y, 212, y);
    for (i = 0; i < (uint32_t)rows && top + i < LES_N; i++) {
        const uint32_t n = top + i, dn = les_dn[n >> 3] >> (n & 7u) & 1u;
        const int32_t ly = y + 28 + (int32_t)i * 16;
        if (n == les_pick) { cv_rect(x + 8, ly - 3, w - 16, 16, RGB(60, 0, 40)); cv_rect(x + 8, ly - 3, 4, 16, C_PINK); }
        if (dn) { cv_rect(x + 18, ly + 6, 2, 2, C_LIME); cv_rect(x + 20, ly + 8, 2, 2, C_LIME); cv_rect(x + 22, ly + 6, 2, 2, C_LIME); cv_rect(x + 24, ly + 4, 2, 2, C_LIME); cv_rect(x + 26, ly + 2, 2, 2, C_LIME); }
        else { cv_rect(x + 17, ly + 1, 11, 1, C_RING); cv_rect(x + 17, ly + 11, 11, 1, C_RING); cv_rect(x + 17, ly + 1, 1, 11, C_RING); cv_rect(x + 27, ly + 1, 1, 11, C_RING); }
        text_at(x + 36, ly, LESSON[n].name, n == les_pick ? C_W : dn ? C_LIME : LESSON[n].mc ? C_Y : C_RING);       /* (a masterclass in gold) */
    }
    {   const int32_t th = rows * 16 - 4, len = th * rows / (int32_t)LES_N, at = (th - len) * (int32_t)top / (int32_t)(LES_N - (uint32_t)rows);
        cv_rect(x + w - 10, y + 26, 2, th, C_RING); cv_rect(x + w - 10, y + 26 + at, 2, len, C_Y);
    }
    text_at(x + 10, y + 212 - 21, "OCT+  Start", C_Y);
    text_at(x + w - 10 - text_w(&FONT_S, "HOME  Close"), y + 212 - 21, "HOME  Close", C_Y);
}
static void les_patch(uint32_t mc, uint32_t id);
static void les_setup(uint32_t kind, uint32_t patch)            /* the scratch project */
{
    static const uint8_t MEL[8] = {36, 38, 39, 43, 41, 39, 38, 36};
    uint32_t i, p;
    slot_apply(&BLANK);
    chain_n = 0; chain_pos = 0; seq_bpm = 120;
    if (kind != LS_EMPTY && kind != LS_MC && kind != LS_PATCH)
        for (p = 0; p < (kind == LS_TWO ? 2u : 1u); p++) for (i = 0; i < 8u; i++) { pats[p][i].note = (uint8_t)(MEL[i] + (p ? 5u : 0u)); pats[p][i].fl = 0; }
    pat_idx = 0; cur = 0; page = 0; undo_has = 0; view_now = V_STEPS;
    if (kind == LS_PATCH) les_patch(99u, patch);
}
static uint32_t les_val(uint32_t v, uint32_t a)
{
    uint32_t i, n = 0, h = 7u;
    switch (v) {
    case LV_VIEW: return view_now;
    case LV_NOTES: for (i = 0; i < STEPS; i++) n += PAT[i].note != 0u; return n;
    case LV_ACC: for (i = 0; i < STEPS; i++) n += PAT[i].note != 0u && (PAT[i].fl & F_ACC) != 0u; return n;
    case LV_SLD: for (i = 0; i < STEPS; i++) n += PAT[i].note != 0u && (PAT[i].fl & F_SLD) != 0u; return n;
    case LV_CUR: return cur;
    case LV_ONNOTE: return PAT[cur % STEPS].note != 0u;
    case LV_RUN: return seq_run;
    case LV_BPM: return seq_bpm;
    case LV_PAT: return pat_idx;
    case LV_PHASH: for (i = 0; i < STEPS; i++) h = h * 31u + PAT[i].note * 3u + PAT[i].fl; return h;
    case LV_PAGE: return page;
    case LV_CHAIN: return chain_n;
    case LV_FX: return pfx_on != 0u;
    case LV_FXM: return pfx_mask != 0u;
    case LV_LEN: return plen[pat_idx % PATS];
    case LV_KALL: for (i = 0; i < sizeof kn; i++) h = h * 31u + ((const uint8_t *)&kn)[i]; return h * 31u + vol_dec;
    case LV_CUT: return kn.cut;
    case LV_RES: return kn.res;
    case LV_ENVM: return kn.env;
    case LV_DEC: return kn.dec;
    case LV_KN: return a == 200u ? vol_dec : a < sizeof kn ? (uint32_t)((const uint8_t *)&kn)[a] : 0u;
    case LV_PAD: return pad_level(a);
    case LV_OCT: return cfg.oct;                                               /* the keys' octave */
    case LV_PATS: for (i = 0; i < PATS; i++) n += pat_has(i); return n;         /* patterns that hold a note */
    case LV_PFXB: return pfx_mask >> (8u + a) & 1u;                             /* a: the bass's own pads 9..15 (0..6) */
    case LV_KEYS: return fm1_in.notes != 0u;                                    /* a key is down */
    case LV_LVL: return (uint32_t)(24 + (int32_t)kn.lvl);                       /* the output level as the knob shows it (0..31) */
    }
    return 0;
}
typedef struct { uint8_t mc, id; int8_t kn[20]; uint8_t pad[10], vol, bpm, len, note[16], fl[16]; } mcpatch_t;      /* made by tools/masterclasses.py */
static const mcpatch_t MCP[] = {
#include "lessons_mc_patches.h"
    {0, 0, {0}, {0}, 0, 0, 0, {0}, {0}}
};
static void les_patch(uint32_t mc, uint32_t id)                 /* the sound and the pattern a chapter (mc 99: a lesson's start) begins with */
{
    uint32_t i;
    for (i = 0; i + 1u < sizeof MCP / sizeof MCP[0]; i++) {
        const mcpatch_t *p = &MCP[i];
        uint32_t k;
        if (p->mc != mc || p->id != id) continue;
        seq_stop();
        memcpy(&kn, p->kn, sizeof kn); vol_dec = p->vol; seq_bpm = p->bpm;
        for (k = 0; k < PADS_USED; k++) if (p->pad[k] != 255u) pad_set(k, p->pad[k]);
        for (k = 0; k < STEPS; k++) { pats[0][k].note = p->note[k]; pats[0][k].fl = p->fl[k]; }
        plen[0] = p->len; pat_idx = 0; cur = 0; undo_has = 0;
        break;
    }
    page = 0;
}
static void les_enter_step(void)                                /* a step became the current one: a read page sets its sound up */
{
    const lstep_t *s = &LESSON[les_i].s[les_step];
    if (s->v == LV_READ && s->thr) les_patch(LESSON[les_i].mc, s->thr);
}
static void les_enter(uint32_t i, uint32_t dirty_now)                               /* a lesson on a scratch project (the player's is kept) */
{
    uint32_t k;
    if (!les_on) {
        slot_pack(&les_slot);
        for (k = 0; k < CHAIN_MAX; k++) les_chain[k] = chain[k];
        les_chn = chain_n; les_dirty = dirty_now; les_view = view_now; les_oct = cfg.oct;
    }
    seq_stop();
    if (pfx_on) pfx_close();
    les_setup(LESSON[i].setup, LESSON[i].patch);
    les_on = 1; les_i = i; les_step = 0; les_ok = 0; les_fin = 0; les_hide = 0; les_snap_step = ~0u;
    les_enter_step();
}
static uint32_t les_exit(void)                                      /* the player's project comes back */
{
    uint32_t k;
    seq_stop();
    if (pfx_on) pfx_close();
    slot_apply(&les_slot);
    for (k = 0; k < CHAIN_MAX; k++) chain[k] = les_chain[k];
    chain_n = les_chn; chain_pos = 0; undo_has = 0; view_now = les_view; cfg.view = (uint8_t)les_view; page = 0; cfg.oct = les_oct;
    les_on = 0;
    return les_dirty;
}
static uint32_t les_arm = 1;
static uint32_t les_true(const lstep_t *s, uint32_t v) { return (s->op == LO_EQ && v == s->thr) || (s->op == LO_GE && v >= s->thr); }
static uint32_t les_tick(void)                                  /* every loop: has the step been done; 1 when the card must be painted again */
{
    const lesson_t *l = &LESSON[les_i];
    const lstep_t *s;
    uint32_t v, done = 0;
    if (!les_on || les_fin) return 0;
    if (les_ok) {
        if (fm1_ms - les_ok_t > 900u) { les_ok = 0; if (++les_step >= l->n) { les_fin = 1; les_dn[les_i >> 3] |= (uint8_t)(1u << (les_i & 7u)); les_saved = 1; } else les_enter_step(); return 1; }
        return 0;
    }
    s = &l->s[les_step];
    if (s->v == LV_READ) return 0;                                                  /* a page to read: OCT+ goes on (les_read_next) */
    v = les_val(s->v, s->a);
    if (les_snap_step != les_step) { les_snap_step = les_step; les_snap = v; les_kbase = les_kclk; les_arm = !les_true(s, v);
#ifdef SMILEY_HOST
        if (!les_arm) { printf("LESSON %u STEP %u INSTANT\n", (unsigned)les_i, (unsigned)les_step); fflush(stdout); }
#endif
    }
    if (!les_arm && !les_true(s, v)) les_arm = 1;                                    /* a step that was already true when it showed waits until it has been undone */
    switch (s->op) {
    case LO_EQ: case LO_GE: done = les_arm && les_true(s, v); break;
    case LO_CHG: done = v != les_snap; break;
    case LO_UP: done = v > les_snap; break;
    default: done = v < les_snap; break;
    }
    if (done && s->op >= LO_CHG && s->pill[0] == 'K' && s->pill[1] == 'N' && les_kclk - les_kbase < 4u) done = 0;       /* a knob step wants a few clicks of a knob, not one */
    if (done) {
        les_ok = 1; les_ok_t = fm1_ms;
        if (les_step + 1u < l->n) { les_snap = les_val(l->s[les_step + 1u].v, l->s[les_step + 1u].a); les_snap_step = les_step + 1u; les_kbase = les_kclk; les_arm = !les_true(&l->s[les_step + 1u], les_snap);
#ifdef SMILEY_HOST
            if (!les_arm && l->s[les_step + 1u].v != LV_READ) { printf("LESSON %u STEP %u INSTANT\n", (unsigned)les_i, (unsigned)les_step + 1u); fflush(stdout); }
#endif
        }        /* (what you do while the card is green counts for the next step) */
#ifdef SMILEY_HOST
        printf("LESSON %u STEP %u OK\n", (unsigned)les_i, (unsigned)les_step); fflush(stdout);
#endif
        return 1;
    }
    return 0;
}
static void les_read_next(void)                                 /* OCT+ on a read page */
{
    const lesson_t *l = &LESSON[les_i];
#ifdef SMILEY_HOST
    printf("LESSON %u STEP %u OK\n", (unsigned)les_i, (unsigned)les_step); fflush(stdout);
#endif
    if (++les_step >= l->n) { les_fin = 1; les_dn[les_i >> 3] |= (uint8_t)(1u << (les_i & 7u)); les_saved = 1; return; }
    les_enter_step();
    les_snap = les_val(l->s[les_step].v, l->s[les_step].a); les_snap_step = les_step; les_kbase = les_kclk; les_arm = !les_true(&l->s[les_step], les_snap);
}
static uint32_t les_reading(void)                               /* a page to read is up (HOME or GLO puts it away: les_hide) */
{
    return les_on && !les_hide && !les_fin && !les_ok && !man_on && !menu_on && !pick_on && LESSON[les_i].s[les_step].v == LV_READ;
}
static uint32_t les_card_on(void) { return les_on && !les_hide && !man_on && !menu_on && !pick_on && !tip_on && !les_reading(); }
static void scene_lcard(void)                                   /* the card, over the knobs */
{
    const lesson_t *l = &LESSON[les_i];
    const lstep_t *s = &l->s[les_step < l->n ? les_step : l->n - 1u];
    const int32_t y = 0;
    uint16_t edge = les_ok || les_fin ? C_LIME : C_Y;
    char t[16];
    uint32_t k = les_u(t, les_fin ? l->n : les_step + 1u);
    if (text_w(&FONT_S, l->name) + 8 * (int32_t)(k + (l->n >= 10u ? 2u : 1u) + 1u) <= 208) { t[k++] = '/'; k += les_u(t + k, l->n); }       /* (a long name leaves room for the step only) */
    t[k] = 0;
    cv_rect(6, y, 228, 62, edge); cv_rect(8, y + 2, 224, 58, C_K);
    text_at(14, y + 5, l->name, C_Y); text_at(226 - text_w(&FONT_S, t), y + 5, t, C_RING);
    if (les_fin) text_at(14, y + 23, "That's it!", C_LIME);
    else text_at(14, y + 23, les_ok ? LES_PRAISE[(les_i * 5u + les_step * 3u) % 9u] : s->text, les_ok ? C_LIME : C_W);
    {   const char *pill = les_fin ? "OCT+" : s->pill;
        const int32_t pw = text_w(&FONT_S, pill) + 14;
        cv_rect(14, y + 40, pw, 19, C_PINK); cv_rect(15, y + 41, pw - 2, 17, C_K);
        text_at(21, y + 43, pill, C_PINK);
    }
    if (les_fin) { text_at(66, y + 43, "Next", C_LIME); text_at(226 - text_w(&FONT_S, "OCT-  Done"), y + 43, "OCT-  Done", RGB(150, 146, 90)); }
    else text_at(226 - text_w(&FONT_S, "GLO  hide"), y + 43, "GLO  hide", RGB(150, 146, 90));
}
static void scene_lread(void)                                   /* a page to read: a window over the scene, the title, the text, OCT+ Next */
{
    const lesson_t *l = &LESSON[les_i];
    const lstep_t *s = &l->s[les_step];
    const int32_t x = 6, w = 228, y = 18, h = 212;
    char ct[16], ln[34];
    const char *p = s->text;
    int32_t ly = y + 28;
    uint32_t k = les_u(ct, les_step + 1u);
    ct[k++] = '/'; k += les_u(ct + k, l->n); ct[k] = 0;
    menu_win(s->pill, C_Y, h, y);
    text_at(x + w - 14 - text_w(&FONT_S, ct), y + 5, ct, C_K);
    while (*p && ly < y + h - 30) {
        uint32_t n = 0;
        while (*p && *p != '\n' && n < 33u) ln[n++] = *p++;
        ln[n] = 0;
        if (*p == '\n') p++;
        text_at(x + 10, ly, ln, C_W);
        ly += 13;
    }
    text_at(x + 10, y + h - 21, "OCT+  Next", C_Y);
    text_at(x + w - 10 - text_w(&FONT_S, "HOME  Hide"), y + h - 21, "HOME  Hide", C_Y);
}
