// SPDX-License-Identifier: GPL-3.0-only
// Drives the browser build (site/play/smileyface.js) from Node: boots it, presses things, checks the screen and the sound.
//   node tests/web_play.mjs
import { createRequire } from "module";
const require = createRequire(import.meta.url);
const Smileyface = require("../site/play/smileyface.js");
let fails = 0;
const ok = (c, m) => { console.log((c ? "ok   " : "FAIL ") + m); if (!c) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const m = await Smileyface();
const BTN = { OCT_DOWN: 0, OCT_UP: 1, FX: 2, SEL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, SAVE: 9, ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };
const press = async (id, ms = 60) => { m._web_button(id, 1); await sleep(ms); m._web_button(id, 0); await sleep(40); };
const key = async (k) => { m._web_keys(1 << k); await sleep(60); m._web_keys(0); await sleep(40); };
const fb = () => new Uint16Array(m.HEAPU8.buffer, m._web_fb(), 240 * 240);
const lit = () => { const f = fb(); let n = 0; for (let i = 0; i < f.length; i++) if (f[i]) n++; return n; };
const block = () => { const p = m._web_audio() >> 2; return m.HEAP32.subarray(p, p + 512); };
const peak = (n = 40) => { let pk = 0; for (let b = 0; b < n; b++) for (const v of block()) pk = Math.max(pk, Math.abs(v)); return pk; };
m._web_start();
await sleep(600);
ok(lit() > 20000, "the screen draws on boot");
ok(peak() === 0, "silent before anything is pressed");
m._web_keys(1 << 5);
await sleep(80);
ok(peak(30) > 1000, "a key sounds");
m._web_keys(0);
await sleep(200);
peak(100);
await press(BTN.PLAY);
await sleep(700);
let pk = peak(80);
ok(pk > 2000 && pk < 30000, `PLAY plays the pattern (peak ${pk})`);
const before = Array.from(fb().slice(100 * 240, 101 * 240)).join();
await sleep(400);
const after = Array.from(fb().slice(100 * 240, 101 * 240)).join();
ok(before !== after, "the screen moves while it plays");
for (const [name, pressHome] of [["sound", 1], ["fly", 1], ["gen", 1], ["steps", 1]]) {
  await press(BTN.HOME);
  await sleep(250);
  ok(lit() > 20000, `HOME -> ${name} draws`);
}
m._web_button(BTN.HOME, 1);
await sleep(750);
const pl = lit();
ok(pl > 2000 && pl < 20000, `hold HOME opens the picker (${pl} lit)`);
m._web_button(BTN.HOME, 0);
await sleep(200);
await press(BTN.OCT_DOWN);
await sleep(200);
ok(lit() > 20000, "OCT- leaves the picker");
await press(BTN.PLAY);
await sleep(200);
for (let i = 0; i < 40; i++) { m._web_enc(2, 1); await sleep(20); m._web_enc(3, 1); await sleep(20); m._web_enc(4, -1); await sleep(20); }
ok(lit() > 20000, "twisting the knobs keeps drawing");
await press(BTN.LFO); await press(BTN.ARP); await press(BTN.REC); await press(BTN.REC);
for (let i = 0; i < 16; i++) { await key(i % 27); await press(BTN.ENV, 30); await press(BTN.SEL, 30); }
await press(BTN.SAVE);
ok(lit() > 20000, "keys, accent, slide, random, mutate, undo, save all survive");
console.log(fails ? `${fails} failed` : "all ok");
process.exit(fails ? 1 : 0);
