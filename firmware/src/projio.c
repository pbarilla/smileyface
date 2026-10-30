/* SPDX-License-Identifier: GPL-3.0-only */
/* Projects over USB-MIDI, for the website's Share page: read a slot's bytes, write a slot. The same SysEx frames as the updater
 * (F0 pack7(00 59 cmd len24 body sum) F7, ota.c), commands 0x50..0x53.
 *   0x50 info    (one byte, ignored) -> 0x50: version 1, slots, open project, 1 if the open one is unsaved, then per slot: 1 if used
 *   0x51 read    kind (0, a project), slot, offset (2) -> 0x51: status, kind, slot, total (2), offset (2), up to 200 bytes
 *                (offset 0 takes a snapshot; the open project is read as it is now, unsaved edits included)
 *   0x52 write   kind, slot, total (2), offset (2), bytes -> 0x52: status, kind, slot, bytes so far (2). In order, from offset 0; the last chunk
 *                checks the whole thing and stores it. A written project is opened.
 *   0x53 delete  kind, slot -> 0x53: status
 * A project is its slot_t (bank.c).
 * status: 0 ok, 1 stored, 2 not valid, 4 flash failed, 5 out of order, 6 no such slot, 7 empty slot
 * Included by smileyface.c. The sound stops while a transfer writes (flash stalls it). */
#define PX_CHUNK 200u
#define PX_MAX ((uint32_t)sizeof(slot_t))
static uint8_t px[PX_MAX] __attribute__((section(".pool")));    /* the thing in transit */
static uint32_t px_len, px_got, px_kind, px_slot, px_open_req;  /* px_open_req: slot + 1 to open in the main loop */
static uint32_t g_dirty;                                        /* the main loop's unsaved flag, for info */

static void clean_slot(slot_t *s)                               /* whatever came in, made safe to play */
{
    uint32_t a, b;
    uint8_t *k = (uint8_t *)&s->kn;
    for (a = 0; a < PATS; a++) {
        for (b = 0; b < STEPS; b++) { if (s->pats[a][b].note > 127u) s->pats[a][b].note = 0; s->pats[a][b].fl &= 3u; if (!s->pats[a][b].note) s->pats[a][b].fl = 0; }
        if (s->plen[a] < 1u || s->plen[a] > STEPS) s->plen[a] = STEPS;
    }
    for (a = 0; a < 19u; a++) if (k[a] > KMAX) k[a] = KMAX;
    if (s->kn.wave > 3u) s->kn.wave &= 3u;
    if (s->kn.lvl < -24 || s->kn.lvl > 7) s->kn.lvl = 0;
    if (s->bpm < 60u || s->bpm > 200u) s->bpm = 126;
    if (s->pcur >= PATS) s->pcur = 0;
    s->used = 1; s->pad = 0;
}
static uint32_t px_snapshot(uint32_t slot)       /* into px: the length, 0 when empty */
{
    if (slot == slot_cur) { slot_t t; memset(&t, 0, sizeof t); slot_pack(&t); memcpy(px, &t, sizeof t); return (uint32_t)sizeof t; }
    if (!bank.s[slot].used) return 0;
    memcpy(px, &bank.s[slot], sizeof(slot_t));
    return (uint32_t)sizeof(slot_t);
}
static uint32_t px_store(uint32_t slot, uint32_t len)           /* the whole thing is in px: a status */
{
    if (len != (uint32_t)sizeof(slot_t)) return 2u;
    seq_stop();
    memcpy(&bank.s[slot], px, sizeof(slot_t));
    clean_slot(&bank.s[slot]);
    if (bank_flush() != 0) return 4u;
    px_open_req = slot + 1u;
    return 1u;
}

#ifndef SMILEY_HOST
static uint8_t px_msg[8u + 8u + PX_CHUNK + 8u], px_wire[2u + 8u * (8u + 8u + PX_CHUNK + 8u) / 7u + 4u];
static void px_reply(uint32_t cmd, const uint8_t *body, uint32_t n)
{
    uint32_t i, s = 0, w;
    px_msg[0] = 0; px_msg[1] = 0x59; px_msg[2] = (uint8_t)cmd;
    px_msg[3] = (uint8_t)n; px_msg[4] = (uint8_t)(n >> 8); px_msg[5] = 0;
    for (i = 0; i < n; i++) { px_msg[6u + i] = body[i]; s += body[i]; }
    px_msg[6u + n] = (uint8_t)~s;
    px_wire[0] = 0xF0;
    w = 1u + ota_pack7(px_msg, 7u + n, px_wire + 1);
    px_wire[w++] = 0xF7;
    ota_wire_send(px_wire, w);
}
static void proj_io(uint32_t cmd, const uint8_t *b, uint32_t n)
{
    uint8_t r[8u + PX_CHUNK];
    uint32_t s, i;
    if (cmd == 0x50u) {
        r[0] = 1; r[1] = (uint8_t)SLOTS; r[2] = (uint8_t)slot_cur; r[3] = g_dirty ? 1u : 0u;
        for (s = 0; s < SLOTS; s++) {
            r[4u + s] = (uint8_t)(bank.s[s].used || s == slot_cur);
        }
        px_reply(0x50u, r, 4u + SLOTS);
    } else if (cmd == 0x51u && n == 4u) {
        uint32_t kind = b[0], slot = b[1], off = (uint32_t)b[2] | (uint32_t)b[3] << 8, k;
        r[1] = (uint8_t)kind; r[2] = (uint8_t)slot; r[3] = r[4] = r[5] = r[6] = 0;
        if (slot >= SLOTS || kind > 0u) { r[0] = 6; px_reply(0x51u, r, 7u); return; }
        if (off == 0u) { px_len = px_snapshot(slot); px_slot = slot; px_kind = kind; }
        if (!px_len || slot != px_slot || kind != px_kind || off > px_len) { r[0] = px_len ? 5u : 7u; px_reply(0x51u, r, 7u); return; }
        k = px_len - off < PX_CHUNK ? px_len - off : PX_CHUNK;
        r[0] = 0; r[3] = (uint8_t)px_len; r[4] = (uint8_t)(px_len >> 8); r[5] = (uint8_t)off; r[6] = (uint8_t)(off >> 8);
        for (i = 0; i < k; i++) r[7u + i] = px[off + i];
        px_reply(0x51u, r, 7u + k);
    } else if (cmd == 0x52u && n >= 7u) {
        uint32_t kind = b[0], slot = b[1], total = (uint32_t)b[2] | (uint32_t)b[3] << 8, off = (uint32_t)b[4] | (uint32_t)b[5] << 8, dl = n - 6u, st = 0;
        if (slot >= SLOTS || kind > 0u) st = 6u;
        else if (off == 0u) {
            if (!total || total > PX_MAX) st = 2u;
            else { px_len = total; px_got = 0; px_slot = slot; px_kind = kind; }
        }
        if (!st && (slot != px_slot || kind != px_kind || off != px_got || off + dl > px_len || !px_len)) st = 5u;
        if (!st) {
            for (i = 0; i < dl; i++) px[off + i] = b[6u + i];
            px_got += dl;
            if (px_got == px_len) { st = px_store(slot, px_len); px_len = 0; }
        }
        r[0] = (uint8_t)st; r[1] = (uint8_t)kind; r[2] = (uint8_t)slot; r[3] = (uint8_t)px_got; r[4] = (uint8_t)(px_got >> 8);
        if (st >= 2u) px_len = 0;
        px_reply(0x52u, r, 5u);
    } else if (cmd == 0x53u && n == 2u) {
        uint32_t kind = b[0], slot = b[1];
        r[0] = slot >= SLOTS || kind > 0u ? 6u : 1u;
        if (r[0] == 1u) {
            seq_stop();
            bank.s[slot].used = 0;
            if (bank_flush() != 0) r[0] = 4u; else if (slot == slot_cur) px_open_req = slot + 1u;
        }
        r[1] = (uint8_t)kind; r[2] = (uint8_t)slot;
        px_reply(0x53u, r, 3u);
    }
}
static void app_install_mark(void) { }                          /* (ota.c tells the app an install starts; nothing to note here) */
#endif
