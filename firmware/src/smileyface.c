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
 *   KNOB 1..4        the sound, six pages (ALGORITHM turns them): CUTOFF RESON ENVMOD NRM DEC / TUNE ACCENT SLIDE TEMPO / DRIVE ACC DEC TRACK ATTACK / FILT FM DECAY LENGTH LEVEL /
 *                    WAVE SUB OSC MUFFLE SWEEP / HI RES POLES ACCENT (the last two pages are the switches, drawn as switches)
 *   PLAY             start / stop      SEQ   tap tempo      LFO   a random pattern      ARP   nudge a few steps      REC   undo (one level)
 *   FX               an effect after the bass (ALGORITHM: none, then each in a row; HOME: back)
 *   OCT- OCT+        octave      SAVE   save (it also saves 2 s after an edit, when stopped)      MASTER   volume
 *   USB-MIDI         clock, start, stop and continue are followed (the tempo turns cyan); notes play the voice (velocity 100 and up = accent);
 *                    CC 102 play / stop, 103 tempo / 2, 104 save, 105 random pattern, 106 open project 0..11, 110 factory reset, 111 roll the dice, 112 a weird thing (test), 114 a button press (test), 117 rack on / off (test), 118 a unit in a slot (test), 108 pattern 0..15 (cued while it plays),
 *                    74 75 71 72 = CUT ENV RES DEC. The device sends CC 100 = CPU % (worst block since boot), CC 119 / 120 / 121 = slowest screen / drawing / main loop turn in ms, CC 101 = view | 16 unsaved | 32 playing | 64 projects,
 *                    CC 107 = the open project, CC 109 = the pattern, CC 116 = the switch bits (1 square, 2 accents off, 4 and 8 BASS, 16 and 32 DRIFT, 64 DIODE). CC 127 sets those bits (test)
 *
 * Integers only: no float, no 64-bit division. tests/host.c runs this file on a computer. */
#ifndef SMILEY_HOST
#define OTA_PROJ_IO
#endif
#include "platform.c"
#ifdef SMILEY_HOST
#define FM1_TICKS_PER_US 24u
static uint32_t fm1_ticks(void) { return 0; }
#endif
#define BFX_POOL __attribute__((section(".pool")))            /* the shared punch-in effects (bfx.c): the same code on Drumlab */
#include "bfx.c"
#include "voice.c"
#include "fx.c"
#include "bank.c"

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = fm1_ticks(), dt;
    voice_render(out, n);
    /* fx_process(out, n);        the old FX rack (delay / drive / crush units) is switched off: the FX button is the punch-in layer now (pfx.c) */
    { static uint32_t t60; t60 += n; if (t60 >= 735u) { t60 -= 735u; bfx_tick(); } }
    if (bfx_busy() || bfx_mask) bfx_run(out, n, 661500u / (seq_bpm ? seq_bpm : 126u));
    dt = (uint32_t)(fm1_ticks() - t0);
    if (dt > cpu_max) cpu_max = dt;
}

/* knobs count as being touched */
static uint32_t act_ms;
static int32_t enc_act(uint32_t e) { int32_t v = fm1_enc_take(e); if (v) act_ms = fm1_ms; return v; }
#define fm1_enc_take enc_act
#include "look.c"
#include "views.c"
#include "wonder.c"
#include "rack.c"
#include "pfx.c"
#include "manual.c"
#include "menu.c"

static uint32_t paint_max, draw_max, loop_max;                   /* the slowest screen, the slowest drawing of it, and the slowest turn of the main loop, in ms (heartbeat CC 119, 120, 121) */
static void paint(uint32_t y0, uint32_t y1)                     /* rows y0 .. y1 of the screen */
{
    uint32_t t0 = fm1_ms, td = 0;
    while (y0 < y1) {
        uint32_t h = y1 - y0 > 64u ? 64u : y1 - y0, ts = fm1_ms;
        cv_begin(240, h, C_K);
        cv_oy = -(int32_t)y0;
        if (man_on) scene_man();
        else if (pierre_on && !pick_on && !fx_on && !menu_on) scene_pierre();
        else if (pick_on) scene_pick();
        else if (pfx_on && !menu_on) scene_pfx();
        /* else if (fx_on && !menu_on) scene_fx();      the old rack screen */
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
        if (!man_on && !menu_on && !pierre_on && !pick_on && !fx_on) wd_post();
        cv_oy = 0;
        if (!man_on && !menu_on) fx_post(y0, h);
        td += fm1_ms - ts;
        cv_blit(0, y0);
        y0 += h;
    }
    if (fm1_ms - t0 > paint_max) paint_max = fm1_ms - t0;
    if (td > draw_max) draw_max = td;
}

static void pat_pick(uint32_t p)                                /* a pattern: now if it is stopped, at the end of the loop if it plays */
{
    if (p >= PATS) return;
    if (!seq_run) { pat_idx = p; cue_clear(); cur = 0; }
    else { cue_clear(); cue_push(p); }
}
static void pat_chain(uint32_t p)                               /* a key in PLAY: it joins the chain after the ones already cued */
{
    if (p >= PATS) return;
    if (!seq_run) pat_pick(p); else cue_push(p);
}
#include "projio.c"

static void app_main(void)
{
    uint32_t fake_b = 0, keys, prev = 0, btn_was = 0, hb_ms = 0, weird_next = 16000u, redraw = 7, last_draw = 0, dirty = 0, t_change = 0, held_note = 0, k, cfg_dirty = 0;
#define bt_pack() do { cfg.pad[0] = (uint8_t)((brothers_mode & 3u) | bt_side << 2 | bt_patsync << 3 | bt_fxsync << 4 | bt_key << 5); cfg_dirty = 1; t_change = fm1_ms; } while (0)       /* the Bluetooth settings, kept with the others */
    uint32_t ext_ms = 0, tap_last = 0, tap_n = 0, tap_k = 0, tap_iv[4] = {0}, sel_was = 0, sel_t0 = 0, sel_used = 0;
    uint32_t home_was = 0, home_t0 = 0, home_used = 0, edit_was = 0, edit_t0 = 0, edit_used = 0, sel_was2 = 0, sel_rot = 0, sel_keyed = 0;
    int32_t e;
#ifndef SMILEY_HOST
    uint32_t fr_was = 0;
#endif
    uint32_t loop_t = 0, pt_y = 0, pt_end = 0, pt_t = 0;                   /* the effect screen goes out a strip a turn, so the loop keeps listening */
    rnd_s = fm1_ticks() ^ 0x1234567u;
#if !defined(SMILEY_HOST) || defined(SPLASH_DEMO)
    splash_flyer();
#endif
    bank_load();
    if (settings_load(&cfg, sizeof cfg) != 0 || cfg.magic != SET_OK) {                  /* a new device, or a new firmware */
        struct { uint8_t slot, oct, view, magic; } old;                                  /* the settings from before the MIDI clock: four bytes */
        if (settings_load(&old, sizeof old) == 0 && old.magic == SET_OK) { cfg.slot = old.slot; cfg.oct = old.oct; cfg.view = old.view; }
        else { cfg.slot = 0; cfg.oct = 1; cfg.view = 0; }
        cfg.magic = SET_OK; cfg.clk = CLK_DEF;
    }
    if (cfg.clk > 2u) cfg.clk = CLK_DEF;
    clk_send = cfg.clk == 1u;
    bt_side = cfg.pad[0] >> 2 & 1u; bt_patsync = cfg.pad[0] >> 3 & 1u; bt_fxsync = cfg.pad[0] >> 4 & 1u; bt_key = cfg.pad[0] >> 5 & 1u;                   /* cfg.pad[0]: the Bluetooth sync: bits 0-1 the mode, 2 the sidechain, 3 follow patterns */
    if (cfg.slot >= SLOTS) cfg.slot = 0;
    if (cfg.oct > 3u) cfg.oct = 1;
    view_now = cfg.view < VIEWS ? cfg.view : 0u;
    slot_load(cfg.slot);
    for (;;) {
        uint32_t b, rel, down, up, home_now, edit_now, sel_now, env_now, glo_now, lg = fm1_ms - loop_t;
        loop_t = fm1_ms;
        if (lg > loop_max) loop_max = lg;
        platform_poll();
        b = fm1_input_edges(&rel);
        b |= fake_b; fake_b = 0;
        g_dirty = dirty;
        if (px_open_req) {                                              /* a project came in over USB: open it */
            uint32_t sl = px_open_req - 1u;
            px_open_req = 0;
            slot_load(sl); cfg.slot = (uint8_t)sl; cfg_dirty = 1; t_change = fm1_ms; dirty = 0; pick_on = 0; redraw |= 7;
        }
        {
            uint8_t st, d1, d2;
            { static uint32_t paused;                                                                                                        /* the installer is talking: the Bluetooth radio goes quiet (it makes the FM-1 miss requests), and comes back 20 s after */
          if (ota_hit_ms && brothers_mode && !paused) { paused = brothers_mode; brothers_set(0); }
          if (paused && fm1_ms - ota_hit_ms > 20000u) { brothers_set(paused); paused = 0; ota_hit_ms = 0; } }
            if (ota_back) { ota_back = 0; redraw |= 7; pt_y = pt_end = 0; }                                                     /* the installer gave up and left its message on the screen: paint it all again */
            { static uint8_t bt_booted; if (!bt_booted && fm1_ms > 1500u) { bt_booted = 1; if ((cfg.pad[0] & 3u) && (cfg.pad[0] & 3u) <= 2u) brothers_set(cfg.pad[0] & 3u); } }      /* the mode it was left in */
            brothers_poll();                                                                                                   /* a follower: the leader's packets into clock ticks */
            if (brothers_mode == 1u) {                                                                                           /* the leader: the pattern playing (no drums: no kicks, no punch-ins), its key and swing */
                static uint32_t sc_ms, sc_val;
                if (fm1_ms - sc_ms > 300u) { uint8_t nt[STEPS]; uint32_t i2; sc_ms = fm1_ms; for (i2 = 0; i2 < STEPS; i2++) nt[i2] = pats[pat_idx % PATS][i2].note; sc_val = brothers_detect(nt, STEPS); }
                brothers_lead_set(pat_idx, 0, pfx_mask & 0xFFu, sc_val, kn.swg);
            }
            else if (brothers_mode == 2u && bt_patsync && seq_run) {                                                            /* a follower: the leader's pattern, at the end of this one */
                uint32_t p = brothers_pat();
                if (p != 0xFFu && p != bt_last) { bt_last = (uint8_t)p; if (p < PATS && p != pat_idx) pat_pick(p); }
            } else if (brothers_mode != 2u) bt_last = 0xFFu;
            bt_scale = (uint8_t)(brothers_mode == 2u && bt_key ? brothers_scale() : 0u);                                         /* a follower: the leader's key, punch-in effects and swing */
            bfx_mask = (pfx_mask & 0xFFu) | (brothers_mode == 2u && bt_fxsync ? brothers_fx() & 0xFFu : 0u);                       /* the shared pads: ours, and the leader's */
            ax_acc = pfx_mask >> 8 & 1u; ax_sld = pfx_mask >> 9 & 1u; ax_up = pfx_mask >> 10 & 1u; ax_dn = pfx_mask >> 11 & 1u; ax_res = pfx_mask >> 12 & 1u; ax_swp = pfx_mask >> 13 & 1u; ax_short = pfx_mask >> 14 & 1u;
            bt_swg_on = (uint8_t)(brothers_mode == 2u && bt_patsync && brothers_pat() != 0xFFu); bt_swg = (uint8_t)(brothers_swing() > 40u ? 40u : brothers_swing());
            while (midi_get(&st, &d1, &d2) || bt_get(&st, &d1, &d2)) {
                uint32_t ty = st & 0xF0u;
                if (st >= 0xF8u && (d1 == 0x55u ? brothers_mode != 2u : cfg.clk != 2u)) continue;                                                                          /* the clock and transport: only when MIDI Clock is Follow */
                if (st == 0xF8u) { if (d1 == 0x55u) { uint32_t nb = brothers_bpm(); if (nb >= 30u && nb <= 255u) seq_bpm = nb; } ext_ms = fm1_ms; ext_in++; if (!ext_on) { ext_on = 1; ext_in = 1; ext_seen_reset(); } }          /* MIDI clock */
                else if (st == 0xFAu) { seq_play(); redraw |= 7; }                                                                  /* start */
                else if (st == 0xFBu) { seq_run = 1; redraw |= 7; }                                                                 /* continue */
                else if (st == 0xFCu) { seq_stop(); redraw |= 7; }                                                                  /* stop */
                else if (ty == 0x90u && d2) voice_on(d1, d2 >= 100u, 0);
                else if (ty == 0x80u || ty == 0x90u) { if (!seq_run) voice_off(); }
                else if (ty == 0xB0u) {
                    if (d1 == 102u) { if (d2 >= 64u && !seq_run) seq_play(); else if (d2 < 64u && seq_run) seq_stop(); redraw |= 7; }
                    else if (d1 == 103u && d2 >= 30u) { seq_bpm = d2 * 2u; dirty = 1; t_change = fm1_ms; redraw |= 1; }
                    else if (d1 == 104u) { slot_save(); dirty = 0; }
                    else if (d1 == 119u) { bt_demo_load(); dirty = 0; redraw |= 7; }                                                    /* test: the Bluetooth demo */
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
                    else if (d1 == 124u) { wd_mooed = 1; wd_moo_ms = fm1_ms ? fm1_ms : 1u; redraw |= 3; }                         /* test: the moo */
                    else if (d1 == 127u) { kn.wave = (uint8_t)((kn.wave & 0x80u) | (d2 & 0x7Fu)); redraw |= 7; }                   /* test: the switch bits */
                    else if (d1 == 125u) { wd_kind = d2; wd_ms = fm1_ms ? fm1_ms : 1u; redraw |= 3; }                              /* test: a knob finds something */
                    else if (d1 == 126u) { pierre_on = d2 ? 1u : 0u; redraw |= 7; }                                              /* test: Pierre, 1985 */
                    else if (d1 == 117u) { rack.on = d2 ? 1u : 0u; dirty = 1; t_change = fm1_ms; redraw |= 7; }                    /* test: the rack on / off */
                    else if (d1 == 118u && (d2 >> 2 & 3u) < FXN && (d2 & 3u) < FX_KINDS) { fx_set(d2 >> 2 & 3u, d2 & 3u, d2 >> 4 & 3u); dirty = 1; t_change = fm1_ms; redraw |= 7; }     /* test: put a unit in a slot (type 0 none, 1 delay, 2 drive, 3 crush; slot * 4; type + 16 * flavour) */
                    else if (d1 == 114u) fake_b |= 1u << (d2 & 15u);                                                           /* test: a button press */
                    else if (d1 == 110u) { factory_reset(); dirty = cfg_dirty = 0; redraw |= 7; }
                    else if (d1 == 111u) { roll_dice(); dirty = 0; redraw |= 7; }
                    else if (d1 == 108u && d2 < PATS) { pat_chain(d2); redraw |= 7; }
                    else if (d1 == 74u || d1 == 75u || d1 == 71u || d1 == 72u) {                                              /* CUT ENV RES DEC from a controller (recorded as knob turns) */
                        uint8_t *kp = d1 == 74u ? &kn.cut : d1 == 75u ? &kn.env : d1 == 71u ? &kn.res : &kn.dec;
                        int32_t nv = d2 >> 2, dl = nv - (int32_t)*kp;
                        if (dl) { *kp = (uint8_t)nv; }
                        redraw |= 6;
                    }
                }
            }
            if (fm1_ms - hb_ms >= 1000u) {                  /* heartbeat for tests */
                hb_ms = fm1_ms;
                midi_send(0xB0u, 100u, cpu_max / 1393u > 127u ? 127u : cpu_max / 1393u);
                midi_send(0xB0u, 101u, view_now | (dirty ? 16u : 0u) | (seq_run ? 32u : 0u) | (pick_on ? 64u : 0u));
                midi_send(0xB0u, 107u, slot_cur);
                midi_send(0xB0u, 109u, pat_idx);
                midi_send(0xB0u, 116u, kn.wave & 127u);                                                                     /* the switch bits: 1 square, 2 accents off, 4-8 BASS, 16-32 DRIFT, 64 DIODE (a check) */
                midi_send(0xB0u, 119u, paint_max > 127u ? 127u : paint_max);
                midi_send(0xB0u, 120u, draw_max > 127u ? 127u : draw_max);
                midi_send(0xB0u, 121u, loop_max > 127u ? 127u : loop_max);
#ifndef SMILEY_HOST
                { uint32_t fr = fm1_in.frames; midi_send(0xB0u, 122u, ((fr - fr_was) >> 3) > 127u ? 127u : (fr - fr_was) >> 3); fr_was = fr; }               /* panel scans a second / 8: about 113 */
#endif
                paint_max = draw_max = loop_max = 0;
            }
        }
        if (ext_on && fm1_ms - ext_ms > 400u) { ext_on = 0; redraw |= 1; }         /* the clock stopped: back to our own */
        keys = fm1_in.notes;
        if (keys != prev || fm1_in.buttons != btn_was) act_ms = fm1_ms;
        {   static uint32_t fp_was;                             /* the glitch is drawn onto whatever strip is painted while it is on, and the knobs are only repainted when you touch them: so when it ends, paint everything once. It can never stay on. */
            uint32_t fp = fx_post_on();
            if (fp_was && !fp) redraw |= 7;
            fp_was = fp;
        }
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
            if (menu_on == 4u) menu_on = 1;                                                           /* back from the Bluetooth page */
            else if (menu_on) { menu_on = 0; dice_t0 = 0; dice_prog = 0; }
            else if (man_on) { man_on = 0; man_lock = 0; }
            else if (pick_on) {
                if (dirty) slot_save();
                dirty = 0; slot_load(pick_sel); cfg.slot = (uint8_t)pick_sel; cfg_dirty = 1; t_change = fm1_ms; pick_on = 0;
            }
            else if (pfx_on) pfx_close();
            else if (fx_on) fx_on = 0;
            else { view_now = (view_now + 1u) % VIEWS; cfg.view = (uint8_t)view_now; cfg_dirty = 1; t_change = fm1_ms; }
            redraw |= 7;
        }
        home_was = home_now;
        if (b & (1u << BTN_GLO) && home_now && !menu_on) { menu_on = 1; menu_sel = 0; man_on = 0; man_lock = 0; pick_on = 0; home_used = 1; redraw |= 7; b &= ~(1u << BTN_GLO); }       /* HOME first, then GLO: the menu */
        if (b & (1u << BTN_GLO) && !man_on && !menu_on) {                                                              /* the manual, at the chapter for where you are */
            man_open(pick_on ? MC_PROJ : pfx_on ? MC_FX : view_now == V_STEPS ? MC_STEPS : view_now == V_PLAY ? MC_PLAY : MC_QUICK, 0u);          /* (the odd views are not explained: try it and find out) */
            redraw |= 7;
            b &= ~(1u << BTN_GLO);
        }
        if (menu_on) {
            uint32_t oct_up = fm1_in.buttons >> BTN_OCT_UP & 1u;
            e = fm1_enc_take(ENC_SELECT) + fm1_enc_take(ENC_PRESETS);
            for (k = 0; k < 4u; k++) fm1_enc_take(ENC_KNOB1 + k);
            fm1_enc_take(ENC_ALGORITHM);
            if (menu_on == 1u) {
                { static uint32_t bt_t; if (fm1_ms - bt_t > 300u) { bt_t = fm1_ms; redraw |= 7; } }                                   /* the Bluetooth line moves by itself */
                if (e) { int32_t v = (int32_t)menu_sel + (e > 0 ? 1 : -1); menu_sel = (uint32_t)(v < 0 ? 0 : v > 4 ? 4 : v); redraw |= 7; }
                if (b & (1u << BTN_OCT_UP)) { if (menu_sel == 4u) { pierre_on ^= 1u; menu_on = 0; }
                    else if (menu_sel == 3u) { menu_on = 4u; bt_sel = 0; }                                                                       /* the Bluetooth page */
                    else if (menu_sel == 2u) { cfg.clk = (uint8_t)((cfg.clk + 1u) % 3u); clk_send = cfg.clk == 1u; if (cfg.clk != 2u) ext_on = 0; cfg_dirty = 1; t_change = fm1_ms; }          /* MIDI Clock: off, send, follow */
                    else { menu_on = 2u + menu_sel; dice_t0 = dice_prog = dice_armed = 0; } redraw |= 7; }
                if (b & (1u << BTN_OCT_DOWN)) { menu_on = 0; redraw |= 7; }
            } else if (menu_on == 4u) {                                                                       /* Bluetooth: Sync, (Status), Sidechain, Patterns */
                { static uint32_t bt_t; if (fm1_ms - bt_t > 300u) { bt_t = fm1_ms; redraw |= 7; } }
                if (e) { int32_t v = (int32_t)bt_sel + (e > 0 ? 1 : -1); bt_sel = (uint32_t)(v < 0 ? 0 : v > 5 ? 5 : v); redraw |= 7; }
                if (b & (1u << BTN_OCT_UP)) {
                    if (bt_sel == 0u) brothers_set(((uint32_t)brothers_mode + 1u) % 3u);
                    else if (bt_sel == 1u) bt_side ^= 1u;
                    else if (bt_sel == 2u) bt_patsync ^= 1u;
                    else if (bt_sel == 3u) bt_fxsync ^= 1u;
                    else if (bt_sel == 4u) bt_key ^= 1u;
                    else { bt_demo_load(); menu_on = 0; dirty = 0; }                                                                 /* Demo: the locked project */
                    if (bt_sel != 5u) bt_pack();
                    redraw |= 7;
                }
                if (b & (1u << BTN_OCT_DOWN)) { menu_on = 1; redraw |= 7; }
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
        if (b & (1u << BTN_FX) && !pick_on) { if (pfx_on) pfx_close(); else pfx_on = 1; redraw |= 7; }                          /* FX: the punch-in effects (the old rack: fx_on ^= 1u) */
        if (pfx_on && !menu_on) {                                                                                         /* the layer: the white keys are pads */
            pfx_input(down, up);
            if (b & (1u << BTN_PLAY)) { if (seq_run) seq_stop(); else seq_play(); }
            edit_was = edit_now; sel_was = sel_now; sel_was2 = sel_now;
            fm1_led_key(BTN_PLAY, seq_run); fm1_led_key(BTN_FX, 1);
            if (fm1_ms - last_draw >= 40u) { anim_t = fm1_ms / 24u; paint(0, 240); last_draw = fm1_ms; }
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
            if (b & (1u << BTN_PLAY)) {
                if (seq_run) seq_stop(); else seq_play();
            }
            edit_was = edit_now; sel_was = sel_now;
            fm1_led_key(BTN_PLAY, seq_run);
            if (redraw && fm1_ms - last_draw >= 20u) { paint(0, 240); last_draw = fm1_ms; redraw = 0; }
            continue;
        }
        if (fx_on) {                                                                                      /* the effect: ALGORITHM walks through them all (none first), HOME goes back */
            uint32_t ty = rack.u[fx_sel].type;
            e = fm1_enc_take(ENC_SELECT);                                                                 /* SELECT: slot 1 or slot 2 */
            if (e) { fx_sel = e > 0 ? 1u : 0u; ty = rack.u[fx_sel].type; redraw |= 7; }
            e = fm1_enc_take(ENC_ALGORITHM);
            if (e) {
                int32_t w = (int32_t)fx_pos(fx_sel) + e;
                w = w < 0 ? 0 : w > (int32_t)FXL_N - 1 ? (int32_t)FXL_N - 1 : w;
                if (w != (int32_t)fx_pos(fx_sel)) { fx_pick(fx_sel, (uint32_t)w); rack.on = fx_any(); ty = rack.u[fx_sel].type; dirty = 1; t_change = fm1_ms; }
                redraw |= 7;
            }
            fm1_enc_take(ENC_PRESETS);
            for (k = 0; k < 4u; k++) {
                e = fm1_enc_take(ENC_KNOB1 + k);
                if (!e || !ty) continue;
                fx_turn(fx_sel, k, e);
                dirty = 1; t_change = fm1_ms; redraw |= 7;
            }
            for (k = 0; k < 27u; k++) {                                                                   /* the keys play the bass, to hear what you are doing */
                uint32_t nt = 41u + 12u * cfg.oct + k - 12u;
                if (down >> k & 1u) { voice_on(nt, env_now, 0); held_note = nt; }
                if ((up >> k & 1u) && held_note == nt && !seq_run) { voice_off(); held_note = 0; }
            }
            if (b & (1u << BTN_PLAY)) { if (seq_run) seq_stop(); else seq_play(); redraw |= 7; }
            if (b & (1u << BTN_OCT_DOWN) && cfg.oct > 0u) { cfg.oct--; cfg_dirty = 1; t_change = fm1_ms; }
            if (b & (1u << BTN_OCT_UP) && cfg.oct < 3u) { cfg.oct++; cfg_dirty = 1; t_change = fm1_ms; }
            if (b & (1u << BTN_SAVE)) { if (!seq_run) { slot_save(); if (cfg_dirty) settings_save(&cfg, sizeof cfg); dirty = cfg_dirty = 0; } }
                                                                                                      /* (no autosave in here: a save freezes the knobs for 100 ms. Leave, and it saves; or press SAVE) */
            edit_was = edit_now; sel_was = sel_now; sel_was2 = sel_now;
            fm1_led_key(BTN_PLAY, seq_run); fm1_led_key(BTN_FX, 1); fm1_led_glow(BTN_SAVE, dirty); fm1_led_glow(BTN_REC, undo_has);
            if (pt_y >= pt_end && fm1_ms - last_draw > 45u) redraw |= 3;                                  /* (the lower rows go to the screen only when they change: the screen is the slow part) */
            if (redraw && pt_y >= pt_end && fm1_ms - last_draw >= 20u) { anim_t = fm1_ms / 24u; pt_y = 0; pt_end = redraw & 4u ? 240u : rack.u[fx_sel].type ? Y_LOW - 4u : 214u; last_draw = fm1_ms; redraw = 0; }
            if (pt_y < pt_end && fm1_ms - pt_t >= 13u) { uint32_t y1 = pt_y + 64u > pt_end ? pt_end : pt_y + 64u; paint(pt_y, y1); pt_y = y1; pt_t = fm1_ms; }          /* one strip, then back to the knobs while it travels to the screen (about 12 ms) */
            continue;
        }
        edit_was = edit_now; sel_was = sel_now;
        for (k = 0; k < 27u; k++) {
            uint32_t nt = 41u + 12u * cfg.oct + k - 12u, wi = 99u, w;
            for (w = 0; w < 16u; w++) if (WHITE_KEY[w] == k) wi = w;
            if ((down >> k & 1u) && view_now == V_PLAY) {
                if (wi < PATS) { pat_chain(wi); redraw |= 7; }
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
        if (b & (1u << BTN_REC) && undo_has) { undo_swap(); dirty = 1; t_change = fm1_ms; redraw |= 7; }                     /* REC: undo */
        if (b & (1u << BTN_ARP)) { undo_save(); mutate(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_LFO)) { undo_save(); random_pattern(); dirty = 1; t_change = fm1_ms; redraw |= 7; }
        if (b & (1u << BTN_PLAY)) {
            if (seq_run) seq_stop(); else seq_play();
            redraw |= 7;
        }
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
            if (e) { knob_turn(page, k, e); wd_touch(page, k); wd_check(); dirty = 1; t_change = fm1_ms; redraw |= 6; }
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
        } else if (fm1_ms - act_ms > 8000u && fm1_ms - last_draw > 80u && !menu_on) redraw |= 3;
        if ((wd_on() || pierre_on) && !menu_on && fm1_ms - last_draw > 45u) redraw |= 3;
        fm1_led_key(BTN_PLAY, seq_run);
        fm1_led_key(BTN_ENV, view_now != V_PLAY && (PAT[cur].fl & F_ACC));
        fm1_led_key(BTN_SEL, view_now != V_PLAY && (PAT[cur].fl & F_SLD));
        fm1_led_glow(BTN_SAVE, dirty);
        fm1_led_glow(BTN_REC, undo_has); fm1_led_glow(BTN_SEL, 0); fm1_led_glow(BTN_EDIT, 0);
        fm1_led_key(BTN_FX, pfx_on); fm1_led_glow(BTN_FX, pfx_mask != 0u);
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
