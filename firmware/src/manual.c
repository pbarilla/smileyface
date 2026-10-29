/* SPDX-License-Identifier: GPL-3.0-only */
/* The manual: GLO opens the chapter for the view you are on. A page is the FM-1 with the controls to use ringed, a topic and a few lines.
 * SELECT turns pages, PRESETS jumps chapters, GLO again is the contents, HOME leaves. Included by smileyface.c after views.c. */
enum {
    M_MASTER = 1u << 0, M_SELECT = 1u << 1, M_PRESETS = 1u << 2, M_ALGO = 1u << 3, M_K1 = 1u << 4, M_K2 = 1u << 5, M_K3 = 1u << 6, M_K4 = 1u << 7,
    M_OCTD = 1u << 8, M_OCTU = 1u << 9, M_FX = 1u << 10, M_SEL = 1u << 11, M_ENV = 1u << 12, M_LFO = 1u << 13, M_EDIT = 1u << 14, M_GLO = 1u << 15,
    M_HOME = 1u << 16, M_SAVE = 1u << 17, M_ARP = 1u << 18, M_SEQ = 1u << 19, M_PLAY = 1u << 20, M_REC = 1u << 21, M_KEYS = 1u << 22,
    M_KNOBS = M_K1 | M_K2 | M_K3 | M_K4,
    M_SMALL = 1u << 24, M_NOPANEL = 1u << 25                                    /* the picture small (long text), no picture */
};
typedef struct { const char *topic; uint32_t ring; const char *text; } mpage_t;
typedef struct { const char *name; uint16_t col; const mpage_t *page; uint8_t n; } mchap_t;

static const mpage_t MP_QUICK[] = {
    {"", M_GLO | M_SELECT | M_HOME | M_SMALL, "Press GLO at any time to open this manual. Turn SELECT to flip through the pages. Press HOME to close it."},
    {"", M_KEYS | M_PLAY | M_SMALL, "Press a key to write a note. Press PLAY to hear your pattern. You're making acid already."},
    {"", M_KNOBS | M_ALGO | M_SMALL, "Turn KNOB 1 to 4 to twist the sound while it plays. Turn ALGORITHM to reach more knobs."},
    {"", M_LFO | M_REC | M_SMALL, "Press LFO for a random pattern. It might rule, it might not. Press REC to undo."},
    {"", M_HOME | M_SMALL, "Tap HOME to hop between STEPS, FLY, WALK and PLAY. Hold HOME to open your projects."},
    {"", M_NOPANEL, "Press GLO any time you get lost. Have fun, and make something rad!"},
};
static const mpage_t MP_STEPS[] = {
    {"Write", M_KEYS, "Press a key to write a note on the cursor's step. The cursor hops on to the next step."},
    {"Accent", M_ENV | M_KEYS, "Press ENV to add an accent to the step. Hold ENV while you press a key to write it with an accent."},
    {"Slide", M_SEL | M_KEYS, "Tap SEL to slide the step into the next one. Hold SEL while you press a key to write it with a slide."},
    {"Wrap", M_SEL | M_SELECT | M_SMALL, "Hold SEL and turn SELECT to wrap the pattern along. The steps slide forwards or backwards, and the ones that fall off one end come back at the other."},
    {"Rests", M_EDIT | M_SELECT, "Press EDIT to make a rest. Turn SELECT to move the cursor without writing."},
    {"Play", M_PLAY | M_PRESETS, "Press PLAY to start and stop. Turn PRESETS to pick another of your 16 patterns."},
    {"Octave", M_OCTD | M_OCTU, "Press OCT- or OCT+ to play lower or higher. The lights show how far you've gone."},
    {"Lucky dip", M_LFO | M_ARP | M_REC, "Press LFO for a random pattern. Press ARP to nudge a few steps. Press REC to undo."},
};
static const mpage_t MP_KNOBS[] = {
    {"Live", M_KNOBS | M_SMALL, "Turn KNOB 1 to 4 while it plays, in any view. The dials at the bottom show where they are."},
    {"More knobs", M_ALGO, "Turn ALGORITHM to reach the next page of knobs. The dots at the right show which page you're on."},
    {"Filter", M_KNOBS | M_SMALL, "CUTOFF opens the filter. RESON makes it sing. ENVMOD sets how far each note sweeps it. DECAY sets how long the sweep takes."},
    {"Tune and accent", M_KNOBS | M_SMALL, "TUNE nudges the pitch. ACCENT is how hard accents hit. SLIDE is how long slides take. TEMPO sets the speed."},
    {"Devil Fish", M_KNOBS | M_SMALL, "DRIVE pushes the oscillator into the filter. ACC DEC is how long an accent rings. TRACK makes the filter follow the pitch. ATTACK softens each note's start."},
    {"Extras", M_KNOBS | M_SMALL, "FILT FM lets the oscillator shake the filter. SWING swings the odd steps late. LENGTH sets how many steps play. LEVEL sets how loud it is."},
    {"Switches", M_KNOBS | M_ALGO | M_SMALL, "Turn ALGORITHM on to pages 5 and 6 for the switches. Turn a knob right to flip a switch up, left to flip it down."},
    {"Tempo", M_SEQ | M_ALGO, "Tap SEQ in time to set the tempo, or use KNOB 4 (TEMPO) on page 2. Send MIDI clock and the FM-1 follows."},
};
static const mpage_t MP_FLY[] = {
    {"Steer", M_SELECT, "Turn SELECT to fly the ship up and down the scale. Higher is a higher note."},
    {"Shoot", M_KEYS, "Press any key to fire. A shot writes a note at the ship's height. No shot leaves a rest."},
    {"Accents", M_ENV, "Hit a foe to make an accent. Or hold ENV while you shoot."},
    {"Slides", M_SEL, "Shoot twice in a row at different heights and the notes slide. Hold SEL to stop that."},
    {"Go", M_PLAY, "Press PLAY first. The steps fly past, and each shot lands on the nearest one."},
};
static const mpage_t MP_WALK[] = {
    {"Steer", M_SELECT, "Turn SELECT to step left and right along the corridor. Each lane is a note, low on the left and high on the right."},
    {"Shoot", M_KEYS, "Press any key to shoot. A shot writes a note in your lane, on the step that's passing. No shot leaves a rest."},
    {"Accents", M_ENV, "Hit a monster to make an accent. Or hold ENV while you shoot."},
    {"Slides", M_SEL, "Shoot twice in a row in different lanes and the notes slide. Hold SEL to stop that."},
    {"Go", M_PLAY, "Press PLAY first. The steps come at you down the corridor."},
};
static const mpage_t MP_PLAY[] = {
    {"Patterns", M_PRESETS, "Every project holds 16 patterns. Turn PRESETS to pick the one you're writing."},
    {"Cue", M_KEYS, "Press a white key in PLAY to cue a pattern. It starts when the current one ends."},
    {"Chain", M_KEYS | M_EDIT, "Press several keys to chain patterns in that order. Press EDIT to clear the chain."},
    {"Accent", M_ENV, "Hold ENV to accent everything that plays."},
    {"Length", M_ALGO | M_K3, "Turn ALGORITHM to page 4, then turn KNOB 3 (LENGTH) to set how many steps this pattern plays."},
};
static const mpage_t MP_PROJ[] = {
    {"Open", M_HOME | M_SELECT, "Hold HOME to open your projects. Turn SELECT to pick one, then press HOME to open it."},
    {"Copy and clear", M_SEL | M_EDIT, "Hold SEL on a project to copy it into a free spot. Hold EDIT to start it over."},
    {"Save", M_SAVE, "Press SAVE to save. Stop touching it for two seconds and it saves by itself."},
};
#define MC(n, c, p) {n, c, p, sizeof p / sizeof p[0]}
static const mchap_t MCH[] = {
    MC("Quick Start", RGB(160, 255, 0), MP_QUICK), MC("Steps", RGB(255, 238, 0), MP_STEPS), MC("Knobs", RGB(255, 36, 170), MP_KNOBS),
    MC("Fly", RGB(160, 255, 0), MP_FLY), MC("Walk", RGB(255, 120, 0), MP_WALK), MC("Play", RGB(0, 226, 255), MP_PLAY), MC("Projects", RGB(255, 238, 0), MP_PROJ),
};
#define MCHN (sizeof MCH / sizeof MCH[0])
enum { MC_QUICK, MC_STEPS, MC_KNOBS, MC_FLY, MC_WALK, MC_PLAY, MC_PROJ };

/* the FM-1 as the Play online page draws it (1040 x 664, cut off under the buttons): knob and button centres */
static const int16_t MKN[8][2] = {{139, 187}, {253, 185}, {139, 295}, {253, 295}, {665, 185}, {772, 185}, {880, 185}, {988, 185}};
static const int16_t MBT[14][3] = {{147, 380, 84}, {249, 380, 84}, {663, 287, 58}, {729, 287, 58}, {794, 287, 58}, {860, 287, 58}, {925, 287, 58}, {990, 287, 58},
                                   {663, 352, 58}, {729, 352, 58}, {794, 352, 58}, {860, 352, 58}, {925, 352, 58}, {990, 352, 58}};
static int32_t man_k = 13, man_x0 = 16, man_y0 = 34;      /* the picture's scale in 64ths of a unit (13 = 0.2, 7 = small) and where it sits */
#define MPX(v) (man_x0 + (((v) - 30) * man_k) / 64)
#define MPY(v) (man_y0 + (((v) - 82) * man_k) / 64)
#define MZ(n) (((n) * man_k + 6) / 13)

static uint32_t man_on, man_ch, man_pg, man_toc, man_lock;        /* man_lock: the quick start at a first power-on: its pages only */
static void man_ring(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    cv_rect(x1, y1, x2 - x1, 2, C_PINK); cv_rect(x1, y2 - 2, x2 - x1, 2, C_PINK); cv_rect(x1, y1, 2, y2 - y1, C_PINK); cv_rect(x2 - 2, y1, 2, y2 - y1, C_PINK);
}
static void man_panel(uint32_t ring)
{
    uint32_t i;
    cv_rect(MPX(32), MPY(84), MZ(207), MZ(68 + 14), RGB(24, 24, 32));
    cv_rect(MPX(342), MPY(127), MZ(49), MZ(49), C_K);
    for (i = 0; i < 8u; i++) { cv_rect(MPX(MKN[i][0]) - MZ(5), MPY(MKN[i][1]) - MZ(5), MZ(11), MZ(11), RGB(14, 14, 18)); cv_rect(MPX(MKN[i][0]) - 1, MPY(MKN[i][1]) - MZ(5), 2, MZ(5), C_W); }
    for (i = 0; i < 14u; i++) cv_rect(MPX(MBT[i][0]) - MZ(MBT[i][2] / 10), MPY(MBT[i][1]) - MZ(4), MZ(MBT[i][2] / 5), MZ(8), RGB(60, 60, 78));
    for (i = 0; i < 16u; i++) cv_rect(MPX(80 + (int32_t)i * 587 / 10), MPY(424) + MZ(4), MZ(9), MZ(7), RGB(74, 74, 90));
    for (i = 0; i < 8u; i++)
        if (ring >> i & 1u) man_ring(MPX(MKN[i][0]) - MZ(9), MPY(MKN[i][1]) - MZ(9), MPX(MKN[i][0]) + MZ(10), MPY(MKN[i][1]) + MZ(10));
    for (i = 0; i < 14u; i++)
        if (ring >> (i + 8u) & 1u) man_ring(MPX(MBT[i][0]) - MZ(MBT[i][2] / 10 + 4), MPY(MBT[i][1]) - MZ(8), MPX(MBT[i][0]) + MZ(MBT[i][2] / 10 + 4), MPY(MBT[i][1]) + MZ(9));
    if (ring & M_KEYS) man_ring(MPX(60), MPY(424) + MZ(1), MPX(1040), MPY(424) + MZ(14));
}
static int32_t man_lines(int32_t x, int32_t y, const char *s, int32_t maxw)       /* wrapped on spaces, 17 px a line; returns the next y */
{
    const char *ls = s, *le = s, *p = s;
    char t[64];
    for (;;) {
        const char *we;
        uint32_t n;
        while (*p == ' ') p++;
        we = p;
        while (*we && *we != ' ') we++;
        if (we == p) break;
        n = (uint32_t)(we - ls);
        if (n > 63u) n = 63u;
        memcpy(t, ls, n); t[n] = 0;
        if (le != ls && text_w(&FONT_S, t) > maxw) {
            n = (uint32_t)(le - ls); memcpy(t, ls, n); t[n] = 0;
            text_at(x, y, t, C_W); y += 17;
            ls = p;
        }
        le = we; p = we;
    }
    if (le != ls) { uint32_t n = (uint32_t)(le - ls); memcpy(t, ls, n); t[n] = 0; text_at(x, y, t, C_W); y += 17; }
    return y;
}
static void man_num(char *d, uint32_t a, uint32_t b)         /* "a/b", both under 100 */
{
    uint32_t n = 0;
    if (a >= 10u) d[n++] = (char)('0' + a / 10u);
    d[n++] = (char)('0' + a % 10u); d[n++] = '/';
    if (b >= 10u) d[n++] = (char)('0' + b / 10u);
    d[n++] = (char)('0' + b % 10u); d[n] = 0;
}
static void scene_man(void)
{
    uint32_t i;
    char nb[8];
    const mchap_t *c = &MCH[man_ch];
    const mpage_t *p = &c->page[man_pg];
    cv_rect(0, 0, 240, 240, C_K);
    if (man_toc) {
        man_num(nb, man_ch + 1u, (uint32_t)MCHN);
        cv_rect(0, 0, 240, 26, C_Y);
        text_at(8, 4, "Manual", C_K);
        for (i = 0; i < MCHN; i++) {
            int32_t y = 30 + (int32_t)i * 22;
            char cn[4];
            if (i == man_ch) cv_rect(0, y, 240, 21, RGB(40, 36, 0));
            cv_rect(8, y + 3, 5, 15, MCH[i].col);
            text_at(22, y + 2, MCH[i].name, i == man_ch ? C_Y : C_W);
            cn[0] = (char)('0' + MCH[i].n / 10u); cn[1] = (char)('0' + MCH[i].n % 10u); cn[2] = 0;
            text_at(214, y + 2, MCH[i].n >= 10u ? cn : cn + 1, C_RING);
        }
        cv_rect(8, 215, 224, 1, C_RING);
        text_at(8, 220, "OCT+  Open", C_Y);
        text_at(232 - text_w(&FONT_S, "HOME  Close"), 220, "HOME  Close", C_Y);
        return;
    }
    man_num(nb, man_pg + 1u, c->n);
    cv_rect(0, 0, 240, 26, c->col);
    text_at(8, 4, c->name, C_K);
    text_at(232 - text_w(&FONT_S, nb), 4, nb, C_K);
    if (p->ring & M_NOPANEL) man_lines(8, 70, p->text, 224);
    else {
        int32_t ty = 142, small = (p->ring & M_SMALL) != 0;
        man_k = small ? 7 : 13; man_x0 = small ? 64 : 16; man_y0 = 34;
        man_panel(p->ring);
        if (small) { ty = 88; if (p->topic[0]) { text_at(8, 84, p->topic, C_PINK); ty = 104; } }
        else if (p->topic[0]) text_at(8, 120, p->topic, C_PINK);
        else ty = 120;
        man_lines(8, ty, p->text, 224);
        man_k = 13; man_x0 = 16;
    }
    {   int32_t d0 = man_lock ? 120 - (int32_t)c->n * 5 : 224 - (int32_t)c->n * 10;          /* the dots: centred, or at the right with GLO Menu at the left */
        for (i = 0; i < c->n; i++) cv_rect(d0 + (int32_t)i * 10, 226, 6, 4, i <= man_pg ? c->col : RGB(58, 58, 74));
        if (!man_lock) text_at(8, 218, "GLO Menu", RGB(138, 138, 154));
        if (man_pg > 0u || (!man_lock && man_ch > 0u)) text_at(man_lock ? 4 : d0 - 14, 216, "<", RGB(138, 138, 154));
        if (man_pg + 1u < c->n || (!man_lock && man_ch + 1u < MCHN)) text_at(228, 216, ">", RGB(138, 138, 154));
    }
}
static void man_open(uint32_t ch, uint32_t pg) { man_ch = ch; man_pg = pg; man_toc = 0; man_lock = 0; man_on = 1; }
static void man_input(uint32_t btn)                      /* SELECT turns pages (on into the next chapter), PRESETS jumps chapters, OCT+ opens a chapter from the contents */
{
    int32_t e = fm1_enc_take(ENC_SELECT), f = fm1_enc_take(ENC_PRESETS);
    if (btn & (1u << BTN_GLO)) { if (!man_lock) man_toc ^= 1u; }
    if (man_lock) {                                         /* the quick start alone: SELECT turns its pages, nothing else */
        if (e > 0 && man_pg + 1u < MCH[man_ch].n) man_pg++;
        else if (e < 0 && man_pg > 0u) man_pg--;
        return;
    }
    if (e && !man_toc) {
        if (e > 0) { if (man_pg + 1u < MCH[man_ch].n) man_pg++; else if (man_ch + 1u < MCHN) { man_ch++; man_pg = 0; } }
        else { if (man_pg > 0u) man_pg--; else if (man_ch > 0u) { man_ch--; man_pg = MCH[man_ch].n - 1u; } }
    } else if (e) f = e;
    if (f) {
        if (f > 0 && man_ch + 1u < MCHN) man_ch++;
        else if (f < 0 && man_ch > 0u) man_ch--;
        man_pg = 0;
    }
    if (man_toc && (btn & (1u << BTN_OCT_UP))) { man_toc = 0; man_pg = 0; }
}
