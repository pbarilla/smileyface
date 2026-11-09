/* SPDX-License-Identifier: GPL-3.0-only */
/* Tips: when nobody has touched the FM-1 for five minutes and nothing plays, a window shows a tip, and the next one every twenty seconds. Any touch closes it.
 * Switched off with Show Tips in the menu. The tips never explain the odd views: try it and find out. Included by smileyface.c after menu.c. */
typedef struct { const char *text; int8_t qr; } tip_t;           /* qr: an index into QR_ROW, or -1 */
static const tip_t TIPS[] = {
    {"Press GLO at any time to open the manual. Twist a knob first, and it explains that page.", -1},
    {"Press GLO and HOME together to open the menu.", -1},
    {"Switch these tips off in the menu: press GLO and HOME together.", -1},
    {"Read the manual:", QR_GUIDE},
    {"Hit LFO to roll a random pattern. Hit it again for another.", -1},
    {"Press ARP to mutate a few steps of this pattern.", -1},
    {"Tap SEQ in time to set the tempo.", -1},
    {"Press REC to undo your last change. Press it again to flip back.", -1},
    {"Press ENV to make a step shout. Tap SEL to glide into the next one.", -1},
    {"Hold SEL and turn SELECT to slide the whole pattern along.", -1},
    {"Turn ALGORITHM to find more knobs and switches.", -1},
    {"Turn the knobs while it plays. That is the fun part.", -1},
    {"Press PLAY in STEPS to loop one pattern. Press PLAY in the PLAY view to play your chain.", -1},
    {"In PLAY, pick a pattern and press OCT+ to add it to the chain. OCT- takes the last one off.", -1},
    {"In PLAY, hold SEL to copy a pattern. Hold EDIT and turn SELECT to move it.", -1},
    {"While your chain plays, press a white key to line up a pattern. It plays next.", -1},
    {"Turn PRESETS to pick another pattern.", -1},
    {"Hold HOME to open your projects.", -1},
    {"Hold SEL on a project to copy it into the first free spot. The demo copies too.", -1},
    {"Hold EDIT on the demo in the project list to delete it and use project 1 yourself. Restore Demos in the menu brings it back.", -1},
    {"Press FX, then hold a white key to punch in an effect.", -1},
    {"Open Brothers Link in the menu to play along with Chipbox and Drumlab.", -1},
    {"Press OCT- or OCT+ to move the keys down or up an octave.", -1},
    {"Share your projects:", QR_SHARE},
    {"Play it online:", QR_PLAY},
    {"Report a bug here:", QR_GIT},
};
#define TIPN (sizeof TIPS / sizeof TIPS[0])
#ifndef TIP_IDLE
#define TIP_IDLE 300000u                                        /* untouched this long */
#define TIP_EVERY 20000u                                        /* then the next tip every so often, until a touch */
#endif
static uint32_t tip_n, tip_on, tip_t0;
static void tip_open(void) { tip_on = 1; tip_t0 = fm1_ms ? fm1_ms : 1u; tip_n = (tip_n + 1u) % TIPN; }
static void scene_tip(void)                                     /* a window as tall as its words (a code adds its square) */
{
    const tip_t *t = &TIPS[(tip_n + TIPN - 1u) % TIPN];
    const int32_t x = 12, w = 216;
    uint32_t r, q, qr = t->qr >= 0;
    int32_t ty, h, y0;
    man_dry = 1; ty = man_lines(0, 0, t->text, w - 24); man_dry = 0;
    h = 30 + ty + 6 + (qr ? (QR_N + 8) * 4 + 4 : 0) + 28;
    y0 = (236 - h) / 2;
    cv_rect(x + 6, y0 + 6, w, h, C_K);                          /* the shadow */
    cv_rect(x, y0, w, h, C_Y);
    cv_rect(x + 3, y0 + 3, w - 6, h - 6, C_K);
    cv_rect(x + 3, y0 + 3, w - 6, 20, C_Y);
    text_at(x + 10, y0 + 5, "Tip", C_K);
    man_ink = C_W;
    man_lines(x + 12, y0 + 30, t->text, w - 24);
    man_ink = C_K;
    if (qr) {
        const int32_t qy = y0 + 30 + ty + 4, qx = x + (w - (QR_N + 8) * 4) / 2;
        cv_rect(qx, qy, (QR_N + 8) * 4, (QR_N + 8) * 4, C_W);
        for (r = 0; r < QR_N; r++) for (q = 0; q < QR_N; q++) if (QR_ROW[t->qr][r] >> (QR_N - 1u - q) & 1u) cv_rect(qx + 16 + (int32_t)q * 4, qy + 16 + (int32_t)r * 4, 4, 4, C_K);
    }
    cv_rect(x + 10, y0 + h - 27, w - 20, 1, C_RING);
    text_at(x + w - 10 - text_w(&FONT_S, "HOME  Close"), y0 + h - 21, "HOME  Close", C_Y);
}
