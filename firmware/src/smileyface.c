/* SPDX-License-Identifier: GPL-3.0-only */
/* Smileyface: an acid bass line for the FM-1. One voice, 16 steps, 16 patterns a project, knobs to twist.
 *
 *   GLO              the manual, at the chapter for where you are (GLO again: the contents, HOME leaves)
 *   GLO + HOME       the menu (either first): Factory Reset, Roll The Dice (every pattern random; hold OCT+ to confirm)
 *   HOME             tap: STEPS -> FLY -> WALK -> PLAY          hold: the 12 projects (HOME opens, hold SEL copies, hold EDIT starts over, GLO: help, OCT- leaves)
 *   keys             STEPS: write a note on the cursor's step and move on (ENV or SEL held: an accent, a slide)
 *                    PLAY: a white key cues a pattern (several make a chain)     FLY, WALK: every key shoots
 *   ENV  SEL  EDIT   accent on the cursor's step, slide on it (tap), a rest (and move on)       PLAY: ENV held = accent, EDIT clears the chain
 *   SEL held         SELECT wraps the pattern along (the steps slide forwards or backwards, round the end)
 *   SELECT           the cursor along the steps (FLY: steers the ship, WALK: steps between lanes)
 *   PRESETS          the pattern (16 a project); while it plays the pattern is cued for the end of the loop
 *   KNOB 1..4        the sound, six pages (ALGORITHM turns them): CUTOFF RESON ENVMOD DECAY / TUNE ACCENT SLIDE TEMPO / DRIVE ACC DEC TRACK ATTACK / FILT FM SWING LENGTH LEVEL /
 *                    WAVE SUB OSC MUFFLE SWEEP / HI RES POLES ACCENT (the last two pages are the switches, drawn as switches)
 *   PLAY             start / stop      SEQ   tap tempo      LFO   a random pattern      ARP   nudge a few steps      REC   undo (one level)
 *   OCT- OCT+        octave      SAVE   save (it also saves 2 s after an edit, when stopped)      MASTER   volume
 *   USB-MIDI         clock, start, stop and continue are followed (the tempo turns cyan); notes play the voice (velocity 100 and up = accent);
 *                    CC 102 play / stop, 103 tempo / 2, 104 save, 105 random pattern, 106 open project 0..11, 110 factory reset, 111 roll the dice, 112 a weird thing (test), 108 pattern 0..15 (cued while it plays),
 *                    74 75 71 72 = CUT ENV RES DEC. The device sends CC 100 = CPU % (worst block since boot), CC 101 = view | 16 unsaved | 32 playing | 64 projects,
 *                    CC 107 = the open project, CC 109 = the pattern
 *
 * Integers only: no float, no 64-bit division. tests/host.c runs this file on a computer. */
#include "platform.c"
#ifdef SMILEY_HOST
#define FM1_TICKS_PER_US 24u
static uint32_t fm1_ticks(void) { return 0; }
#endif
#include "voice.c"
#include "bank.c"

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = fm1_ticks(), dt;
    voice_render(out, n);
    dt = (uint32_t)(fm1_ticks() - t0);
    if (dt > cpu_max) cpu_max = dt;
}

/* knobs count as being touched */
static uint32_t act_ms;
static int32_t enc_act(uint32_t e) { int32_t v = fm1_enc_take(e); if (v) act_ms = fm1_ms; return v; }
#define fm1_enc_take enc_act
#include "look.c"
#include "views.c"
#include "manual.c"
#include "menu.c"

static void paint(uint32_t y0, uint32_t y1)                     /* rows y0 .. y1 of the screen */
{
    while (y0 < y1) {
        uint32_t h = y1 - y0 > 64u ? 64u : y1 - y0;
        cv_begin(240, h, C_K);
        cv_oy = -(int32_t)y0;
        if (man_on) scene_man();
        else if (pick_on) scene_pick();
        else if (menu_on) { if (y0 < Y_HDR) scene_head(); if (y0 + h > Y_HDR && y0 < Y_LOW - 4u) { if (view_now == V_STEPS) scene_steps(); else if (view_now == V_FLY) scene_fly(); else if (view_now == V_WALK) scene_walk(); else scene_play(); } if (y0 + h > Y_LOW - 4u) scene_low(); scene_menu(); }
        else {
            if (y0 < Y_HDR) scene_head();
            if (y0 + h > Y_HDR && y0 < Y_LOW - 4u) {
                if (view_now == V_STEPS) scene_steps();
                else if (view_now == V_FLY) scene_fly();
                else if (view_now == V_WALK) scene_walk();
                else scene_play();
            }
            if (y0 + h > Y_LOW - 4u) scene_low();
        }
        cv_oy = 0;
        if (!man_on && !menu_on) fx_post(y0, h);
        cv_blit(0, y0);
        y0 += h;
    }
}

/* ---- power on: a big smiley, grinning, looking about */
static void splash(void)
{
    uint32_t f, y0;
    for (f = 0; f < 22u; f++) {
        uint32_t t0 = fm1_ms;
        for (y0 = 0; y0 < 240u; y0 += 60u) {
            cv_begin(240, 60, C_K);
            cv_oy = -(int32_t)y0;
            floor_rows(0, 240, 2 + (int32_t)f, f * 6u);
            smiley(120, 92, 56, f < 14u ? (int32_t)f : 14 - (int32_t)(f - 14u), (int32_t)sina(f * 3u) / 20);
            cv_oy = 0;
            cv_blit(0, y0);
        }
        while (fm1_ms - t0 < 50u) platform_poll();
    }
}

static void pat_pick(uint32_t p)                                /* a pattern: now if it is stopped, at the end of the loop if it plays */
{
    if (p >= PATS) return;
    if (!seq_run) { pat_idx = p; cue_clear(); cur = 0; }
    else { cue_clear(); cue_push(p); }
}

static void app_main(void)
{
    uint32_t keys, prev = 0, btn_was = 0, hb_ms = 0, weird_next = 16000u, redraw = 7, last_draw = 0, dirty = 0, t_change = 0, held_note = 0, k, cfg_dirty = 0;
    uint32_t ext_ms = 0, tap_last = 0, tap_n = 0, tap_k = 0, tap_iv[4] = {0}, sel_was = 0, sel_t0 = 0, sel_used = 0;
    uint32_t home_was = 0, home_t0 = 0, home_used = 0, edit_was = 0, edit_t0 = 0, edit_used = 0, sel_was2 = 0, sel_rot = 0, sel_keyed = 0;
    int32_t e;
    rnd_s = fm1_ticks() ^ 0x1234567u;
#if !defined(SMILEY_HOST) || defined(SPLASH_DEMO)
    splash();
#endif
    bank_load();
    if (settings_load(&cfg, sizeof cfg) != 0 || cfg.magic != SET_OK) {                  /* a new device, or a new firmware */ cfg.slot = 0; cfg.oct = 1; cfg.view = 0; cfg.magic = SET_OK; }
    if (cfg.slot >= SLOTS) cfg.slot = 0;
    if (cfg.oct > 3u) cfg.oct = 1;
    view_now = cfg.view < VIEWS ? cfg.view : 0u;
    slot_load(cfg.slot);
    for (;;) {
        uint32_t b, rel, down, up, home_now, edit_now, sel_now, env_now, glo_now;
        platform_poll();
        b = fm1_input_edges(&rel);
        {
            uint8_t st, d1, d2;
            while (midi_get(&st, &d1, &d2)) {
                uint32_t ty = st & 0xF0u;
                if (st == 0xF8u) { ext_ms = fm1_ms; ext_in++; if (!ext_on) { ext_on = 1; ext_in = 1; ext_seen_reset(); } }          /* MIDI clock */
                else if (st == 0xFAu) { seq_play(); redraw |= 7; }                                                                  /* start */
                else if (st == 0xFBu) { seq_run = 1; redraw |= 7; }                                                                 /* continue */
                else if (st == 0xFCu) { seq_stop(); redraw |= 7; }                                                                  /* stop */
                else if (ty == 0x90u && d2) voice_on(d1, d2 >= 100u, 0);
                else if (ty == 0x80u || ty == 0x90u) { if (!seq_run) voice_off(); }
                else if (ty == 0xB0u) {
                    if (d1 == 102u) { if (d2 >= 64u && !seq_run) seq_play(); else if (d2 < 64u && seq_run) seq_stop(); redraw |= 7; }
                    else if (d1 == 103u && d2 >= 30u) { seq_bpm = d2 * 2u; dirty = 1; t_change = fm1_ms; redraw |= 1; }
                    else if (d1 == 104u) { slot_save(); dirty = 0; }
                    else if (d1 == 105u) { undo_save(); random_pattern(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
                    else if (d1 == 106u && d2 < SLOTS) { if (dirty) slot_save(); dirty = 0; slot_load(d2); cfg.slot = (uint8_t)d2; cfg_dirty = 1; t_change = fm1_ms; redraw |= 7; }
                    else if (d1 == 112u) {                                                                                     /* test: one weird thing on demand */
                        if (d2 < 2u) bg_style = d2;
                        else if (d2 >= 10u && d2 <= 13u) { gag_kind = d2 - 9u; gag_ms = fm1_ms; }
                        else if (d2 == 20u) glitch_ms = fm1_ms;
                        else if (d2 == 21u || d2 == 22u) { swap_kind = d2 - 20u; swap_ms = fm1_ms; }
                        else if (d2 == 30u) { crit_kind = 1u; crit_ms = fm1_ms; }
                        redraw |= 7;
                    }
                    else if (d1 == 110u) { factory_reset(); dirty = cfg_dirty = 0; redraw |= 7; }
                    else if (d1 == 111u) { roll_dice(); dirty = 0; redraw |= 7; }
                    else if (d1 == 108u && d2 < PATS) { pat_pick(d2); redraw |= 7; }
                    else if (d1 == 74u) { kn.cut = (uint8_t)(d2 >> 2); redraw |= 6; }
                    else if (d1 == 75u) { kn.env = (uint8_t)(d2 >> 2); redraw |= 6; }
                    else if (d1 == 71u) { kn.res = (uint8_t)(d2 >> 2); redraw |= 6; }
                    else if (d1 == 72u) { kn.dec = (uint8_t)(d2 >> 2); redraw |= 6; }
                }
            }
            if (fm1_ms - hb_ms >= 1000u) {                  /* heartbeat for tests */
                hb_ms = fm1_ms;
                midi_send(0xB0u, 100u, cpu_max / 1393u > 127u ? 127u : cpu_max / 1393u);
                midi_send(0xB0u, 101u, view_now | (dirty ? 16u : 0u) | (seq_run ? 32u : 0u) | (pick_on ? 64u : 0u));
                midi_send(0xB0u, 107u, slot_cur);
                midi_send(0xB0u, 109u, pat_idx);
            }
        }
        if (ext_on && fm1_ms - ext_ms > 400u) { ext_on = 0; redraw |= 1; }         /* the clock stopped: back to our own */
        keys = fm1_in.notes;
        if (keys != prev || fm1_in.buttons != btn_was) act_ms = fm1_ms;
        btn_was = fm1_in.buttons;
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        home_now = fm1_in.buttons >> BTN_HOME & 1u;
        edit_now = fm1_in.buttons >> BTN_EDIT & 1u;
        glo_now = fm1_in.buttons >> BTN_GLO & 1u;
        sel_now = fm1_in.buttons >> BTN_SEL & 1u;
        env_now = fm1_in.buttons >> BTN_ENV & 1u;
        if (home_now && !home_was) {
            home_t0 = fm1_ms; home_used = 0;
            if (glo_now && !menu_on) { menu_on = 1; menu_sel = 0; man_on = 0; man_lock = 0; pick_on = 0; home_used = 1; redraw |= 7; }          /* GLO + HOME: the menu */
        }
        if (home_now && !home_used && fm1_ms - home_t0 >= 600u && !pick_on && !man_on && !menu_on) { home_used = 1; pick_on = 1; pick_sel = slot_cur; redraw |= 7; }     /* hold HOME: the projects */
        if (!home_now && home_was && !home_used) {                                                        /* tap HOME: leave what is open, open the project in the frame, or the next view */
            if (menu_on) { menu_on = 0; dice_t0 = 0; dice_prog = 0; }
            else if (man_on) { man_on = 0; man_lock = 0; }
            else if (pick_on) {
                if (dirty) slot_save();
                dirty = 0; slot_load(pick_sel); cfg.slot = (uint8_t)pick_sel; cfg_dirty = 1; t_change = fm1_ms; pick_on = 0;
            }
            else { view_now = (view_now + 1u) % VIEWS; cfg.view = (uint8_t)view_now; cfg_dirty = 1; t_change = fm1_ms; }
            redraw |= 7;
        }
        home_was = home_now;
        if (b & (1u << BTN_GLO) && home_now && !menu_on) { menu_on = 1; menu_sel = 0; man_on = 0; man_lock = 0; pick_on = 0; home_used = 1; redraw |= 7; b &= ~(1u << BTN_GLO); }       /* HOME first, then GLO: the menu */
        if (b & (1u << BTN_GLO) && !man_on && !menu_on) {                                                              /* the manual, at the chapter for where you are */
            man_open(pick_on ? MC_PROJ : view_now == V_STEPS ? MC_STEPS : view_now == V_FLY ? MC_FLY : view_now == V_WALK ? MC_WALK : MC_PLAY, 0u);
            redraw |= 7;
            b &= ~(1u << BTN_GLO);
        }
        if (menu_on) {
            uint32_t oct_up = fm1_in.buttons >> BTN_OCT_UP & 1u;
            e = fm1_enc_take(ENC_SELECT) + fm1_enc_take(ENC_PRESETS);
            for (k = 0; k < 4u; k++) fm1_enc_take(ENC_KNOB1 + k);
            fm1_enc_take(ENC_ALGORITHM);
            if (menu_on == 1u) {
                if (e) { menu_sel = e > 0 ? 1u : 0u; redraw |= 7; }
                if (b & (1u << BTN_OCT_UP)) { menu_on = 2u + menu_sel; dice_t0 = dice_prog = dice_armed = 0; redraw |= 7; }
                if (b & (1u << BTN_OCT_DOWN)) { menu_on = 0; redraw |= 7; }
            } else if (menu_on == 2u) {
                if (b & (1u << BTN_OCT_UP)) { factory_reset(); dirty = cfg_dirty = 0; menu_on = 0; redraw |= 7; }
                else if (b & (1u << BTN_OCT_DOWN)) { menu_on = 1; redraw |= 7; }
            } else {                                                                                  /* Roll The Dice: hold OCT+ (let go of it first: it was pressed to get here) */
                if (b & (1u << BTN_OCT_DOWN)) { menu_on = 1; dice_t0 = dice_prog = 0; redraw |= 7; }
                else if (!oct_up) { dice_armed = 1; dice_t0 = 0; if (dice_prog) { dice_prog = 0; redraw |= 7; } }
                else if (dice_armed) {
                    if (!dice_t0) dice_t0 = fm1_ms ? fm1_ms : 1u;
                    dice_prog = (fm1_ms - dice_t0) * 100u / DICE_MS;
                    redraw |= 7;
                    if (dice_prog >= 100u) { roll_dice(); dirty = 0; menu_on = 0; dice_t0 = dice_prog = 0; }
                }
            }
            if (b & (1u << BTN_PLAY)) { if (seq_run) seq_stop(); else seq_play(); }
            edit_was = edit_now; sel_was = sel_now;
            fm1_led_key(BTN_PLAY, seq_run);
            fm1_led_key(BTN_OCT_UP, menu_on != 0u); fm1_led_key(BTN_OCT_DOWN, menu_on != 0u);
            if (redraw && fm1_ms - last_draw >= 30u) { paint(0, 240); last_draw = fm1_ms; redraw = 0; }
            continue;
        }
        if (man_on) {
            man_input(b);
            for (k = 0; k < 4u; k++) fm1_enc_take(ENC_KNOB1 + k);
            fm1_enc_take(ENC_ALGORITHM);
            if (b & (1u << BTN_PLAY)) { if (seq_run) seq_stop(); else seq_play(); }
            edit_was = edit_now; sel_was = sel_now;
            fm1_led_key(BTN_PLAY, seq_run);
            if (fm1_ms - last_draw >= 40u) { paint(0, 240); last_draw = fm1_ms; }
            continue;
        }
        if (edit_now && !edit_was) { edit_t0 = fm1_ms; edit_used = 0; }
        if (pick_on && edit_now && !edit_used && fm1_ms - edit_t0 >= 600u) {                              /* hold EDIT on a project: it goes back to the start */
            edit_used = 1; if (pick_sel == slot_cur) undo_save(); bank.s[pick_sel].used = 0; bank_flush();
            if (pick_sel == slot_cur) slot_load(pick_sel);
            redraw |= 7;
        }
        if (sel_now && !sel_was) { sel_t0 = fm1_ms; sel_used = 0; }
        if (pick_on && sel_now && !sel_used && fm1_ms - sel_t0 >= 600u) {                                 /* hold SEL on a project: a copy in the first free spot */
            uint32_t f;
            sel_used = 1;
            for (f = 1; f < SLOTS; f++) if (!bank.s[f].used) break;
            if (f < SLOTS) { bank.s[f] = *slot_get(pick_sel); bank.s[f].used = 1; bank_flush(); pick_sel = f; redraw |= 7; }
        }
        if (pick_on) {
            if (b & (1u << BTN_OCT_DOWN)) { pick_on = 0; redraw |= 7; }
            e = fm1_enc_take(ENC_SELECT) + fm1_enc_take(ENC_PRESETS);
            if (e) { int32_t v = (int32_t)pick_sel + e; pick_sel = (uint32_t)(v < 0 ? 0 : v >= (int32_t)SLOTS ? (int32_t)SLOTS - 1 : v); redraw |= 7; }
            e = fm1_enc_take(ENC_ALGORITHM);
            if (e) { int32_t v = (int32_t)pick_sel + (e < 0 ? -3 : 3); pick_sel = (uint32_t)(v < 0 ? 0 : v >= (int32_t)SLOTS ? (int32_t)SLOTS - 1 : v); redraw |= 7; }
            for (k = 0; k < 4u; k++) fm1_enc_take(ENC_KNOB1 + k);
            if (b & (1u << BTN_PLAY)) { if (seq_run) seq_stop(); else seq_play(); }
            edit_was = edit_now; sel_was = sel_now;
            fm1_led_key(BTN_PLAY, seq_run);
            if (redraw && fm1_ms - last_draw >= 20u) { paint(0, 240); last_draw = fm1_ms; redraw = 0; }
            continue;
        }
        edit_was = edit_now; sel_was = sel_now;
        for (k = 0; k < 27u; k++) {
            uint32_t nt = 41u + 12u * cfg.oct + k - 12u, wi = 99u, w;
            for (w = 0; w < 16u; w++) if (WHITE_KEY[w] == k) wi = w;
            if ((down >> k & 1u) && view_now == V_PLAY) {
                if (wi < PATS) { pat_pick(wi); redraw |= 7; }
            }
            else if ((down >> k & 1u) && view_now == V_FLY) { fly_fire(env_now, sel_now); dirty = 1; t_change = fm1_ms; redraw |= 7; }
            else if ((down >> k & 1u) && view_now == V_WALK) { fly_deg = walk_lane * 2; fly_fire(env_now, sel_now); dirty = 1; t_change = fm1_ms; redraw |= 7; }
            else if (down >> k & 1u) {
                PAT[cur].note = (uint8_t)nt;
                PAT[cur].fl = (uint8_t)((env_now ? F_ACC : 0) | (sel_now ? F_SLD : 0));                    /* ENV or SEL held: an accent, a slide */
                voice_on(nt, PAT[cur].fl & F_ACC, 0);
                held_note = nt;
                if (sel_now) sel_keyed = 1;
                cur = (cur + 1u) % STEPS;
                dirty = 1; t_change = fm1_ms; redraw |= 7;
            }
            if ((up >> k & 1u) && !seq_run && view_now != V_PLAY && (view_now == V_FLY || view_now == V_WALK || held_note == nt)) { voice_off(); held_note = 0; }
        }
        fx_apply(env_now, view_now == V_PLAY);
        if (b & (1u << BTN_ENV) && (view_now == V_STEPS)) { PAT[cur].fl ^= F_ACC; dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (!sel_now && sel_was2 && !sel_rot && !sel_keyed && (view_now == V_STEPS)) { PAT[cur].fl ^= F_SLD; dirty = 1; t_change = fm1_ms; redraw |= 7; }      /* a tap on SEL (not a hold): a slide */
        if (sel_now && !sel_was2) { sel_rot = 0; sel_keyed = 0; }
        sel_was2 = sel_now;
        if (b & (1u << BTN_EDIT)) {
            if (view_now == V_PLAY) { cue_clear(); redraw |= 7; }
            else if (view_now == V_STEPS) { PAT[cur].note = 0; PAT[cur].fl = 0; cur = (cur + 1u) % STEPS; dirty = 1; t_change = fm1_ms; redraw |= 7; }
        }
        if (b & (1u << BTN_SEQ)) {                                          /* tap tempo: the average of the last four taps' gaps */
            uint32_t dt = fm1_ms - tap_last;
            if (dt > 2000u || !tap_last) tap_n = 0;
            else if (!ext_on) {
                uint32_t i, sum = 0, avg, bpm;
                tap_iv[tap_k++ & 3u] = dt;
                if (tap_n < 4u) tap_n++;
                for (i = 0; i < tap_n; i++) sum += tap_iv[i];
                avg = sum / tap_n;
                bpm = avg ? 60000u / avg : 0u;
                if (bpm >= 60u && bpm <= 200u) { seq_bpm = bpm; dirty = 1; t_change = fm1_ms; redraw |= 1; }
            }
            tap_last = fm1_ms;
        }
        if (b & (1u << BTN_REC) && undo_has) { undo_swap(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_ARP)) { undo_save(); mutate(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_LFO)) { undo_save(); random_pattern(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_PLAY)) { if (seq_run) { seq_stop(); } else seq_play(); redraw |= 7; }
        if (b & (1u << BTN_OCT_DOWN) && cfg.oct > 0u) { cfg.oct--; cfg_dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && cfg.oct < 3u) { cfg.oct++; cfg_dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_SAVE)) { if (!seq_run) { slot_save(); if (cfg_dirty) settings_save(&cfg, sizeof cfg); dirty = cfg_dirty = 0; } }
        e = fm1_enc_take(ENC_SELECT);
        if (e && view_now == V_FLY) { fly_deg += e; fly_deg = fly_deg < 0 ? 0 : fly_deg > 14 ? 14 : fly_deg; redraw |= 2; }
        else if (e && sel_now && (view_now == V_STEPS)) {                            /* SEL held: wrap the pattern along */
            if (!sel_rot) undo_save();
            sel_rot = 1; pat_rotate(e > 0 ? 1 : -1); dirty = 1; t_change = fm1_ms; redraw |= 7;
        }
        else if (e && view_now == V_WALK) { walk_lane += e > 0 ? 1 : -1; walk_lane = walk_lane < 0 ? 0 : walk_lane > 7 ? 7 : walk_lane; redraw |= 2; }
        else if (e && view_now != V_PLAY) { cur = (cur + STEPS + (uint32_t)(e % 16)) % STEPS; redraw |= 7; }
        e = fm1_enc_take(ENC_ALGORITHM);
        if (e) { int32_t v = (int32_t)page + (e < 0 ? -1 : 1); page = (uint32_t)(v < 0 ? 0 : v >= (int32_t)PAGES ? (int32_t)PAGES - 1 : v); redraw |= 4; }
        e = fm1_enc_take(ENC_PRESETS);
        if (e) { int32_t v = (int32_t)pat_target() + e; pat_pick((uint32_t)(v < 0 ? 0 : v >= (int32_t)PATS ? (int32_t)PATS - 1 : v)); redraw |= 7; }
        for (k = 0; k < 4u; k++) {
            e = fm1_enc_take(ENC_KNOB1 + k);
            if (e) { knob_turn(page, k, e); dirty = 1; t_change = fm1_ms; redraw |= 6; }
        }
        if (view_now == V_WALK) {
            walk_x += ((walk_lane << 4) - walk_x) / 3;
            if (seq_step != fly_last_step) { if (seq_step == 0u || fly_last_step == 99u) foes_spawn(); fly_last_step = seq_step; }
        }
        if (view_now == V_FLY) {
            int32_t want = deg_y(fly_deg) << 4;
            ship_y += (want - ship_y) / 3;
            if (seq_step != fly_last_step) { if (seq_step == 0u || fly_last_step == 99u) foes_spawn(); fly_last_step = seq_step; }
        }
        if ((dirty || cfg_dirty) && !seq_run && fm1_ms - t_change > 2000u) {
            if (dirty) slot_save();
            if (cfg_dirty) settings_save(&cfg, sizeof cfg);
            dirty = cfg_dirty = 0;
        }
        if (seq_run) {
            static uint32_t sd, sp;
            if (sd != seq_step || sp != pat_idx) {
                sd = seq_step; sp = pat_idx; redraw |= 7;
                if (PAT[seq_step % STEPS].note && (PAT[seq_step % STEPS].fl & F_ACC)) shock_ms = fm1_ms ? fm1_ms : 1u;       /* an accent: a shockwave */
            }
            if (fm1_ms - last_draw > 45u) redraw |= 3;
        } else if (fm1_ms - act_ms > 8000u && fm1_ms - last_draw > 80u && !menu_on) redraw |= 3;                            /* left alone: it keeps moving */
        fm1_led_key(BTN_PLAY, seq_run);
        fm1_led_key(BTN_ENV, view_now != V_PLAY && (PAT[cur].fl & F_ACC));
        fm1_led_key(BTN_SEL, view_now != V_PLAY && (PAT[cur].fl & F_SLD));
        fm1_led_glow(BTN_SAVE, dirty);
        fm1_led_glow(BTN_REC, undo_has);
        fm1_led_key(BTN_OCT_DOWN, cfg.oct < 1u);
        fm1_led_key(BTN_OCT_UP, cfg.oct > 1u);
        {                                                       /* the keys: the cursor's note glows, the playing note is lit; in PLAY the patterns, in FX the switches */
            int32_t kc = PAT[cur].note ? (int32_t)PAT[cur].note - 29 - 12 * (int32_t)cfg.oct : -1;
            int32_t kp = seq_run && PAT[seq_step % STEPS].note ? (int32_t)PAT[seq_step % STEPS].note - 29 - 12 * (int32_t)cfg.oct : -1;
            for (k = 0; k < 27u; k++) {
                uint32_t wi = 99u, w, glow = (int32_t)k == kc, lit = (int32_t)k == kp;
                for (w = 0; w < 16u; w++) if (WHITE_KEY[w] == k) wi = w;
                if (view_now == V_PLAY) { glow = wi < PATS; lit = wi == pat_idx; }
                fm1_led_glow(KEY_LED(k), glow); fm1_led_key(KEY_LED(k), lit);
            }
        }
        if (fm1_ms >= weird_next) { weird_pick(); weird_next = fm1_ms + 10000u + rnd() % 18000u; }      /* something random every 10 to 28 s: little treats */
        if (weird_active() && fm1_ms - last_draw > 45u) redraw |= 3;
        if (redraw && fm1_ms - last_draw >= 20u) {
            uint32_t ms = fm1_ms;
            anim_t = ms / 24u;
            env_view = env_vis;
            if (redraw & 4u) paint(0, 240); else paint(0, Y_LOW - 4u);
            last_draw = ms;
            redraw = 0;
        }
    }
}
