/* SPDX-License-Identifier: GPL-3.0-only */
/* The punch-in effects layer (FX): sixteen pads on the white keys, the same on Chipbox and Drumlab for pads 1-8 (bfx.c) and the ones that travel over Bluetooth.
 * Pads 9-15 only exist on Smileyface, and work on the bass itself: accent everything, slide everything, an octave up, an octave down, resonance to the top, a cutoff
 * sweep, a short decay. Pad 16 is the LATCH: with it on, a tap turns a pad on and the next tap turns it off. Included by smileyface.c after rack.c.
 * (The old FX rack, delay / drive / crush units, is switched off: its code is still here, see fx.c, and the projects still keep its settings.) */
static uint32_t pfx_on, pfx_held, pfx_tog, pfx_lat;               /* the layer is open; the pads held; the pads latched on; the latch */
static volatile uint32_t pfx_mask;                                /* what is on now (held | latched): bit n = pad n + 1 */
static const char *const PFX_NAME[16] = {"STOP", "REPEAT", "LPF", "HPF", "ECHO", "REVERB", "CRUSH", "GATE", "ACCENT", "SLIDE", "OCT UP", "OCT DN", "RESMAX", "SWEEP", "SHORT", "LATCH"};

static void pfx_set(uint32_t m) { pfx_mask = m; }
static void pfx_close(void) { pfx_on = 0; pfx_held = pfx_tog = pfx_lat = 0; pfx_set(0); }
static void pfx_input(uint32_t down, uint32_t up)               /* the white keys, as pads */
{
    uint32_t p;
    for (p = 0; p < 16u; p++) {
        uint32_t bit = 1u << p;
        if (down >> WHITE_KEY[p] & 1u) {
            if (p == 15u) { pfx_lat ^= 1u; if (!pfx_lat) pfx_tog = 0; }
            else if (pfx_lat) pfx_tog ^= bit;
            else pfx_held |= bit;
        }
        if (up >> WHITE_KEY[p] & 1u) pfx_held &= ~bit;
    }
    pfx_set((pfx_held | pfx_tog | (pfx_lat ? 1u << 15 : 0u)) & 0x7FFFu);
}
static void scene_pfx(void)
{
    uint32_t p;
    floor_rows((int32_t)Y_HDR, (int32_t)Y_LOW - 4, 4 + shock_bump(), anim_t);
    if (cv_vis(0, (int32_t)Y_HDR)) scene_head();
    if (cv_vis((int32_t)Y_LOW - 4, 240)) cv_rect(0, (int32_t)Y_LOW - 4, 240, 64, C_K);
    for (p = 0; p < 16u; p++) {
        int32_t x = 3 + (int32_t)(p % 4u) * 59, y = 52 + (int32_t)(p / 4u) * 46;
        uint32_t on = p == 15u ? pfx_lat : (pfx_mask >> p & 1u);
        uint16_t c = p < 8u ? C_CY : p == 15u ? C_LIME : C_PINK;
        cv_rect(x, y, 57, 42, on ? c : C_K);
        cv_rect(x + 2, y + 2, 53, 38, on ? C_K : RGB(22, 22, 28));
        cv_rect(x + 4, y + 4, 49, 34, on ? c : RGB(30, 30, 38));
        text_mid(x + 28, y + 17, PFX_NAME[p], on ? C_K : c);
    }
}
