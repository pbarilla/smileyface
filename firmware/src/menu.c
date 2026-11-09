/* SPDX-License-Identifier: GPL-3.0-only */
/* The menu: GLO and HOME together (either first). Factory Reset, and Roll The Dice (every pattern of every project, random; it asks you to hold OCT+).
 * Included by smileyface.c after manual.c. */
static uint32_t bt_sel;                                         /* the Bluetooth page: which of Sync, Sidechain, Patterns */
static uint32_t menu_on, menu_sel, dice_t0, dice_prog, dice_armed;      /* menu_on: 1 the list, 2 Factory Reset?, 3 Roll The Dice? */
#define DICE_MS 1500u
static uint32_t menu_n(void) { return demo_off ? MENU_ROWS : MENU_ROWS - 1u; }          /* the lines shown: Restore Demos only once the demo was deleted */
static uint32_t menu_id(uint32_t i) { return !demo_off && i >= 6u ? i + 1u : i; }      /* a line's row: 0 Quick Start Always .. 5 Roll The Dice, 6 Restore Demos, 7 Factory Reset, 8 Pierre */
#define MENU_ROWS 9u                                            /* (all of them; the Restore Demos row shows only while the demo is deleted: `menu_n()` rows, `menu_id()` maps a line to its row) */
#define MENU_ROWS_X                                            /* Quick Start Always, Show Tips, Projects At Start, MIDI Clock, Brothers Link, Roll The Dice, Restore Demos, Factory Reset, Pierre */

/* THE BLUETOOTH DEMO: a locked acid line to play with all three brothers (D minor, 124 BPM): pattern 1 the groove, pattern 2 the lift */
static void bt_demo_load(void)
{
    static const uint8_t N[2][16] = {{38, 0, 38, 41, 0, 38, 43, 0, 38, 0, 41, 38, 0, 45, 43, 0}, {38, 38, 0, 41, 0, 43, 0, 45, 46, 0, 45, 43, 0, 41, 0, 38}};
    static const uint8_t F[2][16] = {{0, 0, 0, F_ACC, 0, 0, F_SLD, 0, 0, 0, F_ACC, 0, 0, F_SLD, 0, 0}, {F_ACC, 0, 0, 0, 0, F_SLD, 0, F_ACC, 0, 0, F_SLD, 0, 0, 0, 0, F_ACC}};
    uint32_t p, i;
    seq_stop();
    for (p = 0; p < 2u && p < PATS; p++) {
        for (i = 0; i < STEPS; i++) { pats[p][i].note = N[p][i]; pats[p][i].fl = N[p][i] ? F[p][i] : 0; }
        plen[p] = STEPS;
    }
    pat_idx = 0; cue_clear(); seq_bpm = 124; undo_has = 0;
    bt_locked = 1; chain_n = 0;
}
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
        if (i == 0u && !demo_off) continue;                     /* the demo stays */
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
    memset(bank.s, 0, sizeof bank.s); memset(bank.chain, 0, sizeof bank.chain);
    bank_flush();
    cfg.slot = 0; cfg.oct = 1; cfg.view = 0; cfg.magic = SET_OK; cfg.pad[1] = QS_OK; cfg.pad[2] = DEMO_OK; demo_off = 0;       /* (the demo comes back) */
    man_open(MC_QUICK, 0); man_lock = 1;                         /* like a new device: the quick start again */
    settings_save(&cfg, sizeof cfg);
    slot_load(0);
    view_now = 0; undo_has = 0; pick_on = 0;
}

static void menu_win(const char *title, uint16_t col, int32_t h, int32_t y)
{
    const int32_t x = 6, w = 228;
    cv_rect(x + 6, y + 6, w, h, C_K);                           /* the shadow */
    cv_rect(x, y, w, h, col);
    cv_rect(x + 3, y + 3, w - 6, h - 6, C_K);
    cv_rect(x + 3, y + 3, w - 6, 20, col);
    text_at(x + 10, y + 5, title, C_K);
    cv_rect(x + 10, y + h - 27, w - 20, 1, C_RING);
}
static void scene_menu(void)
{
    const int32_t x = 6, w = 228, y = menu_on == 1u ? 18 : 36;
    uint32_t i;
    if (menu_on == 1u) {
        const char *const IT[MENU_ROWS] = {cfg.pad[1] & QS_ALWAYS ? "Quick Start Always: Yes" : "Quick Start Always: No", cfg.pad[1] & QS_NOTIPS ? "Show Tips: No" : "Show Tips: Yes",
                                           cfg.pad[1] & QS_PICK ? "Projects At Start: Yes" : "Projects At Start: No",
                                           cfg.clk == 1u ? "MIDI Clock: Send" : cfg.clk == 2u ? "MIDI Clock: Follow" : "MIDI Clock: Off", "Brothers Link", "Roll The Dice", slot0_busy ? "Restore: Slot 1 In Use" : "Restore Demos", "Factory Reset", "Pierre, 1985"};
        const int32_t h = 212;
        menu_win("Menu", C_Y, h, y);
        for (i = 0; i < menu_n(); i++) {
            int32_t ly = y + 28 + (int32_t)i * 17;
            if (i == menu_sel) { cv_rect(x + 8, ly - 3, w - 16, 17, RGB(60, 0, 40)); cv_rect(x + 8, ly - 3, 4, 17, C_PINK); }
            text_at(x + 22, ly, IT[menu_id(i)], i == menu_sel ? C_W : C_RING);
        }
        text_at(x + 10, y + h - 21, "OCT+  Select", C_Y);
        text_at(x + w - 10 - text_w(&FONT_S, "HOME  Close"), y + h - 21, "HOME  Close", C_Y);
    } else if (menu_on == 4u) {                                     /* Bluetooth: the same words on all three brothers */
        static const char *const MODE[3] = {"Sync: Off", "Sync: Lead", "Sync: Follow"};
        static const char *const STAT[3][3] = {{"Status: Off", "Status: Off", "Status: Off"}, {"Status: Ready", "Status: Ready", "Status: Leading"}, {"Status: Searching", "Status: Linked", "Status: In sync"}};
        const char *const IT[7] = {MODE[brothers_mode % 3u], STAT[brothers_mode % 3u][brothers_link() % 3u], bt_side ? "Sidechain: On" : "Sidechain: Off", bt_patsync ? "Patterns: On" : "Patterns: Off",
                                   bt_fxsync ? "Punch-in FX: On" : "Punch-in FX: Off", bt_key ? "Key: On" : "Key: Off", bt_locked ? "Demo: Locked" : "Demo: Load"};
        const int32_t h = 204;
        menu_win("Brothers Link", C_Y, h, y);
        for (i = 0; i < 7u; i++) {
            int32_t ly = y + 30 + (int32_t)i * 20;
            uint32_t row = i == 0u ? 0u : i == 1u ? 9u : i - 1u;                       /* the status line is only shown */
            if (row == bt_sel) { cv_rect(x + 8, ly - 4, w - 16, 19, RGB(60, 0, 40)); cv_rect(x + 8, ly - 4, 4, 19, C_PINK); }
            text_at(x + 22, ly, IT[i], row == bt_sel ? C_W : C_RING);
        }
        text_at(x + 10, y + h - 21, "OCT+  Change", C_Y);
        text_at(x + w - 10 - text_w(&FONT_S, "HOME  Back"), y + h - 21, "HOME  Back", C_Y);
    } else if (menu_on == 2u) {
        const int32_t h = 112;
        menu_win("Confirm", C_PINK, h, y);
        text_mid(x + w / 2, y + 44, "Factory Reset?", C_W);
        text_at(x + 10, y + h - 21, "OCT+  Yes", C_Y);
        text_at(x + w - 10 - text_w(&FONT_S, "OCT-  No"), y + h - 21, "OCT-  No", C_Y);
    } else if (menu_on == 3u) {
        const int32_t h = 168;
        menu_win("Roll The Dice?", C_PINK, h, y);
        man_lines(x + 10, y + 30, "This will trash all your patterns.", w - 20);
        man_lines(x + 10, y + 64, "Are you sure? It'll get weird.", w - 20);
        cv_rect(x + 10, y + 104, w - 20, 14, C_RING);                           /* the hold: it fills while OCT+ is down */
        cv_rect(x + 12, y + 106, w - 24, 10, C_K);
        cv_rect(x + 12, y + 106, (w - 24) * (int32_t)dice_prog / 100, 10, C_PINK);
        text_at(x + 10, y + h - 21, "Hold OCT+  Yes", C_Y);
        text_at(x + w - 10 - text_w(&FONT_S, "OCT-  No"), y + h - 21, "OCT-  No", C_Y);
    }
}
