// SPDX-License-Identifier: GPL-3.0-only
// Projects over USB-MIDI (firmware/src/projio.c): the same SysEx frames as the updater (fm1ota.js), commands 0x50 info, 0x51 read, 0x52 write,
// 0x53 delete. After the Chipbox Projects page (pbarilla), which is after Felucca's updater (Leo Kuroshita, Hügelton Instruments).
import { Updater, pack7, unpack7 } from "./fm1ota.js";

const PRODUCT = "FM-1_303";
const CHUNK = 200;
const frame = (cmd, body) => {
  const m = [0, 0x59, cmd, body.length & 255, (body.length >> 8) & 255, 0, ...body];
  let s = 0;
  for (const b of body) s += b;
  m.push(~s & 255);
  return [0xF0, ...pack7(m), 0xF7];
};

export class Device {
  constructor(link) { this.link = link; }

  static async connect() {
    const midi = await navigator.requestMIDIAccess({ sysex: true });
    const up = new Updater(midi, () => {});
    const dev = await up.find((id) => id.text === PRODUCT);
    if (!dev) { const e = new Error("notfound"); e.code = "notfound"; throw e; }
    return new Device(dev.link);
  }

  async ask(cmd, body, ms = 2000) {
    for (let tries = 0; tries < 3; tries++) {
      this.link.drain();
      if (!this.link.send(frame(cmd, body))) { const e = new Error("lost"); e.code = "lost"; throw e; }
      const end = Date.now() + ms;
      while (Date.now() < end) {
        const p = await this.link.read(end - Date.now());
        if (!p) break;
        const u = unpack7(p.slice(1, -1));
        if (u.length >= 7 && u[0] === 0 && u[1] === 0x59 && u[2] === cmd) return u.slice(6, -1);
      }
    }
    const e = new Error("lost"); e.code = "lost"; throw e;
  }

  // { open, dirty, slots: [{ used }] }
  async info() {
    const r = await this.ask(0x50, [0]);
    const slots = [];
    for (let s = 0; s < r[1]; s++) slots.push({ used: r[4 + s] === 1 });
    return { open: r[2], dirty: r[3] === 1, slots };
  }

  // a project's bytes, or null when the slot is empty
  async read(slot, progress) {
    const out = [];
    let off = 0, total = 0;
    do {
      const r = await this.ask(0x51, [0, slot, off & 255, off >> 8]);
      if (r[0] === 7) return null;
      if (r[0] !== 0) { const e = new Error("read" + r[0]); e.code = "read"; throw e; }
      total = r[3] | (r[4] << 8);
      const part = r.slice(7);
      out.push(...part);
      off += part.length;
      if (progress) progress(off / total);
      if (!part.length) break;
    } while (off < total);
    return new Uint8Array(out);
  }

  // 1 when stored; throws with code "write" and .status otherwise
  async write(slot, bytes, progress) {
    let off = 0, last = 0;
    while (off < bytes.length) {
      const part = Array.from(bytes.slice(off, off + CHUNK));
      const r = await this.ask(0x52, [0, slot, bytes.length & 255, bytes.length >> 8, off & 255, off >> 8, ...part], 6000);
      last = r[0];
      if (last > 1) { const e = new Error("write" + last); e.code = "write"; e.status = last; throw e; }
      off += part.length;
      if (progress) progress(off / bytes.length);
    }
    return last;
  }

  async remove(slot) { return (await this.ask(0x53, [0, slot], 4000))[0]; }
}

// a file: {"smileyface": 1, "items": [{"name": "...", "project": "<base64>"}]}
const b64 = (u8) => { let s = ""; for (const b of u8) s += String.fromCharCode(b); return btoa(s); };
const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
export const toFile = (items) => JSON.stringify({ smileyface: 1, items: items.map((p) => ({ name: p.name, project: b64(p.project) })) }) + "\n";
export function fromFile(text) {
  const o = JSON.parse(text);
  if (!o || o.smileyface !== 1 || !Array.isArray(o.items) || !o.items.length) throw new Error("not a smileyface file");
  return o.items.map((p) => ({ name: String(p.name || ""), project: unb64(p.project) }));
}
