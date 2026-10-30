/* SPDX-License-Identifier: GPL-3.0-only */
/* The menu: GLO and HOME together (either first). Factory Reset, and Roll The Dice (every pattern of every project, random; it asks you to hold OCT+).
 * Included by smileyface.c after manual.c. */
static uint32_t menu_on, menu_sel, dice_t0, dice_prog, dice_armed;      /* menu_on: 1 the list, 2 Factory Reset?, 3 Roll The Dice? */
#define DICE_MS 1500u

static void rnd_pattern_into(step_t *p)                         /* a random pattern, some sparse, some busy, in a root of its own */
{
    static const uint8_t SC[10] = {0, 0, 3, 5, 7, 7, 10, 12, 12, 15}, ROOT[5] = {36, 36, 38, 41, 43};
    uint32_t i, root = ROOT[rnd() % 5u], rest = 2u + rnd() % 4u;
    for (i = 0; i < STEPS; i++) {
        p[i].note = rnd() % rest ? (uint8_t)(root + SC[rnd() % 10u] + (rnd() % 5u == 0u ? 12u : 0u)) : 0;
        p[i].fl = (uint8_t)(p[i].note ? ((rnd() % 4u == 0u ? F_ACC : 0) | (rnd() % 4u == 0u ? F_SLD : 0)) : 0);
    }
}
static void roll_dice(void)
{
    uint32_t i, a;
    seq_stop();
    for (i = 0; i < SLOTS; i++) {
        slot_t *sl = &bank.s[i];
        if (!sl->used) *sl = *slot_get(i);
        for (a = 0; a < PATS; a++) { rnd_pattern_into(sl->pats[a]); sl->plen[a] = STEPS; }
        sl->used = 1; sl->pcur = 0;
    }
    bank_flush();
    slot_load(slot_cur);
    undo_has = 0;
}
static void factory_reset(void)                                 /* every project goes back to the start */
{
    seq_stop();
    memset(bank.s, 0, sizeof bank.s);
    bank_flush();
    cfg.slot = 0; cfg.oct = 1; cfg.view = 0; cfg.magic = SET_OK;
    settings_save(&cfg, sizeof cfg);
    slot_load(0);
    view_now = 0; undo_has = 0; pick_on = 0;
}

static void menu_win(const char *title, uint16_t col, int32_t h)
{
    const int32_t x = 6, y = 36, w = 228;
    cv_rect(x + 6, y + 6, w, h, C_K);                           /* the shadow */
    cv_rect(x, y, w, h, col);
    cv_rect(x + 3, y + 3, w - 6, h - 6, C_K);
    cv_rect(x + 3, y + 3, w - 6, 20, col);
    text_at(x + 10, y + 5, title, C_K);
    cv_rect(x + 10, y + h - 27, w - 20, 1, C_RING);
}
static void scene_menu(void)
{
    const int32_t x = 6, y = 36, w = 228;
    uint32_t i;
    if (menu_on == 1u) {
        static const char *const IT[2] = {"Factory Reset", "Roll The Dice"};
        const int32_t h = 128;
        menu_win("Menu", C_Y, h);
        for (i = 0; i < 2u; i++) {
            int32_t ly = y + 32 + (int32_t)i * 30;
            if (i == menu_sel) { cv_rect(x + 8, ly - 4, w - 16, 26, RGB(60, 0, 40)); cv_rect(x + 8, ly - 4, 4, 26, C_PINK); }
            text_at(x + 22, ly, IT[i], i == menu_sel ? C_W : C_RING);
        }
        text_at(x + 10, y + h - 21, "OCT+  Select", C_Y);
        text_at(x + w - 10 - text_w(&FONT_S, "HOME  Close"), y + h - 21, "HOME  Close", C_Y);
    } else if (menu_on == 2u) {
        const int32_t h = 112;
        menu_win("Confirm", C_PINK, h);
        text_mid(x + w / 2, y + 44, "Factory Reset?", C_W);
        text_at(x + 10, y + h - 21, "OCT+  Yes", C_Y);
        text_at(x + w - 10 - text_w(&FONT_S, "OCT-  No"), y + h - 21, "OCT-  No", C_Y);
    } else if (menu_on == 3u) {
        const int32_t h = 168;
        menu_win("Roll The Dice?", C_PINK, h);
        man_lines(x + 10, y + 30, "This will trash all your patterns.", w - 20);
        man_lines(x + 10, y + 64, "Are you sure? It'll get weird.", w - 20);
        cv_rect(x + 10, y + 104, w - 20, 14, C_RING);                           /* the hold: it fills while OCT+ is down */
        cv_rect(x + 12, y + 106, w - 24, 10, C_K);
        cv_rect(x + 12, y + 106, (w - 24) * (int32_t)dice_prog / 100, 10, C_PINK);
        text_at(x + 10, y + h - 21, "Hold OCT+  Yes", C_Y);
        text_at(x + w - 10 - text_w(&FONT_S, "OCT-  No"), y + h - 21, "OCT-  No", C_Y);
    }
}
