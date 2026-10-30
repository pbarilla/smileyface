// SPDX-License-Identifier: GPL-3.0-only
// The Share page: download a project as a file, upload one into a slot, back up and restore. After the Chipbox Projects page.
import { Device, toFile, fromFile } from "./fm1share.js";

const $ = (id) => document.getElementById(id);
const TEXT = {
  nomidi: "This browser can't talk to the FM-1. Try Chrome or Edge.",
  denied: "MIDI was blocked. Allow it for this site and try again.",
  notfound: "No FM-1 found. Check the USB cable and close other MIDI apps. It needs the beta that can share.",
  lost: "Lost the FM-1. Replug it and connect again.",
  read: "Couldn't read it.",
  w2: "That isn't something this firmware can read.",
  w4: "Couldn't write it. Try again.",
  w5: "Something went out of order. Try again.",
  w6: "No such slot.",
  badfile: "That file isn't a Smileyface file.",
};
const say = (t) => { $("status").textContent = t; };
const prog = (f) => { const b = $("bar"); b.style.display = f == null ? "none" : "block"; if (f != null) b.value = Math.round(f * 100); };
const fail = (e) => {
  prog(null);
  if (e && e.code === "write") say(TEXT["w" + e.status] || TEXT.w4);
  else if (e && TEXT[e.code]) say(TEXT[e.code]);
  else if (e && e.name === "SecurityError") say(TEXT.denied);
  else say(e && e.message ? e.message : String(e));
};

let busy = false;
const op = async (fn) => {                      // one thing at a time: the FM-1 takes one transfer at once
  if (busy) return;
  busy = true; document.body.classList.add("busy");
  try { await fn(); } catch (e) { fail(e); } finally { busy = false; document.body.classList.remove("busy"); }
};
let dev = null, info = null, pending = null;     // pending: { slot, restore } for the file chooser

const save = (name, text) => {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([text], { type: "application/json" }));
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
};
const mk = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
const btn = (text, cls, fn) => { const b = mk("button", cls, text); b.addEventListener("click", fn); return b; };

async function refresh() {
  info = await dev.info();
  $("mine").hidden = false;
  const box = $("slots");
  box.textContent = "";
  info.slots.forEach((s, i) => {
    const row = mk("div", "row" + (i === info.open ? " open" : ""));
    const what = mk("div", "what", "Project " + (i + 1));
    what.append(mk("small", "", (s.used ? "has something" : "empty") + (i === info.open ? (info.dirty ? ", open, unsaved" : ", open") : "")));
    row.append(mk("span", "n", String(i + 1)), what);
    const b = mk("div", "btns");
    if (s.used) b.append(btn("Download", "alt", () => download(i)));
    b.append(btn(s.used ? "Replace…" : "Upload…", "alt", () => chooseFor(i, b)));
    row.append(b);
    box.append(row);
  });
}

const download = (i) => op(async () => {
  say("");
  const project = await dev.read(i, prog);
  prog(null);
  save(`smileyface-project-${i + 1}.smileyface`, toFile([{ name: "Project " + (i + 1), project }]));
});

function chooseFor(slot, b) {
  const open = () => { pending = { slot }; $("pick").value = ""; $("pick").click(); };
  if (!info.slots[slot].used) { open(); return; }
  const was = [...b.childNodes];
  b.textContent = "";
  b.append(mk("span", "small", `Replaces project ${slot + 1}.`), btn("Pick a file", "warn", open), btn("Cancel", "alt", () => { b.textContent = ""; b.append(...was); }));
}

$("pick").addEventListener("change", async (ev) => {
  const f = ev.target.files[0];
  if (!f || !pending) return;
  const { slot, restore } = pending;
  pending = null;
  let list;
  try { list = fromFile(await f.text()); } catch (_) { fail({ code: "badfile" }); return; }
  if (restore) { askRestore(list); return; }
  op(async () => { await put(slot, list[0]); await refresh(); });
});

async function put(slot, item) {
  await dev.write(slot, item.project, prog);
  prog(null);
}
async function putAll(list) {
  const n = Math.min(list.length, info.slots.length);
  for (let i = 0; i < n; i++) await put(i, list[i]);
  await refresh();
}
function askRestore(list) {                     // the warning only when something would really be replaced
  const n = Math.min(list.length, info.slots.length);
  let used = 0;
  for (let i = 0; i < n; i++) if (info.slots[i].used) used++;
  const go = () => { $("confirm").hidden = true; op(() => putAll(list)); };
  if (!used) { go(); return; }
  const c = $("confirm");
  c.textContent = `This replaces ${n === 1 ? "project 1" : `projects 1 to ${n}`}. `;
  c.append(btn("Replace", "warn", go), " ", btn("Cancel", "alt", () => { c.hidden = true; }));
  c.hidden = false;
}

$("backup").addEventListener("click", () => op(async () => {
  const out = [];
  for (let i = 0; i < info.slots.length; i++) {
    if (!info.slots[i].used) continue;
    out.push({ name: "Project " + (i + 1), project: await dev.read(i, prog) });
  }
  prog(null);
  if (!out.length) { say("Nothing to back up."); return; }
  save(`smileyface-backup-${new Date().toISOString().slice(0, 10)}.smileyface`, toFile(out));
}));

$("restore").addEventListener("click", () => {
  $("confirm").hidden = true;
  pending = { slot: 0, restore: true };
  $("pick").value = "";
  $("pick").click();
});

$("connect").addEventListener("click", () => {
  if (!navigator.requestMIDIAccess) { say(TEXT.nomidi); return; }
  op(async () => {
    say("");
    try {
      dev = await Device.connect();
      await refresh();
      $("connect").textContent = "Refresh";
    } catch (e) { dev = null; throw e; }
  });
});
