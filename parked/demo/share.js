// SPDX-License-Identifier: GPL-3.0-only
// The Share page: download a project (and its demo) as a file, upload one into a slot, back up and restore. After the Chipbox Projects page.
import { Device, toFile, fromFile } from "./fm1share.js";

const $ = (id) => document.getElementById(id);

const TEXT = {
  nomidi:
    "Whoopsiedaisy! This browser ain't able to connect to your FM-1! Try Chrome or Edge.",
  denied:
    "MIDI was blocked. Spoiler warning: We kind of need it! Allow it for this site and try again.",
  notfound: "Hey... did you actually plug the FM-1 in?",
  lost: "waitasecond, where'd it go?? Replug your FM-1 and connect again.",
  read: "Couldn't read it. Which is weird cause that's ALL i do!",
  w2: "What is this, some weird unknown thing? I cant read this!",
  w4: "Huh!? I cant write... whattheheck",
  w5: "Something went out of order. Try again.",
  w6: "No such slot. That's a silly warning, huh? What does that even mean? No such slot. yeah good luck lmao",
  badfile: "lmao that isn't a Smileyface file... what are you playing at here!",
};

const say = (t) => {
  $("status").textContent = t;
};
const prog = (f) => {
  const b = $("bar");
  b.style.display = f == null ? "none" : "block";
  if (f != null) b.value = Math.round(f * 100);
};
const fail = (e) => {
  prog(null);
  if (e && e.code === "write") say(TEXT["w" + e.status] || TEXT.w4);
  else if (e && TEXT[e.code]) say(TEXT[e.code]);
  else if (e && e.name === "SecurityError") say(TEXT.denied);
  else say(e && e.message ? e.message : String(e));
};

let busy = false;
const op = async (fn) => {
  // one thing at a time: the FM-1 takes one transfer at once
  if (busy) return;
  busy = true;
  document.body.classList.add("busy");
  try {
    await fn();
  } catch (e) {
    fail(e);
  } finally {
    busy = false;
    document.body.classList.remove("busy");
  }
};
let dev = null,
  info = null,
  pending = null; // pending: { slot, restore } for the file chooser

const save = (name, text) => {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([text], { type: "application/json" }));
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
};
const mk = (tag, cls, text) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text != null) e.textContent = text;
  return e;
};
const btn = (text, cls, fn) => {
  const b = mk("button", cls, text);
  b.addEventListener("click", fn);
  return b;
};

async function refresh() {
  info = await dev.info();
  $("mine").hidden = false;
  const box = $("slots");
  box.textContent = "";
  info.slots.forEach((s, i) => {
    const row = mk("div", "row" + (i === info.open ? " open" : ""));
    const what = mk("div", "what", "Project " + (i + 1));
    what.append(
      mk(
        "small",
        "",
        (s.used ? "has something" : "empty") +
          (s.demo ? ", has a demo" : "") +
          (i === info.open ? (info.dirty ? ", open, unsaved" : ", open") : "")
      )
    );
    row.append(mk("span", "n", String(i + 1)), what);
    const b = mk("div", "btns");
    if (s.used) b.append(btn("Download", "alt", () => download(i)));
    b.append(
      btn(s.used ? "Replace…" : "Upload…", "alt", () => chooseFor(i, b))
    );
    row.append(b);
    box.append(row);
  });
}

const download = (i) =>
  op(async () => {
    say("");
    const project = await dev.read(0, i, prog);
    const demo = info.slots[i].demo ? await dev.read(1, i, prog) : null;
    prog(null);
    save(
      `smileyface-project-${i + 1}.smileyface`,
      toFile([{ name: "Project " + (i + 1), project, demo }])
    );
  });

function chooseFor(slot, b) {
  const open = () => {
    pending = { slot };
    $("pick").value = "";
    $("pick").click();
  };
  if (!info.slots[slot].used) {
    open();
    return;
  }
  const was = [...b.childNodes];
  b.textContent = "";
  b.append(
    mk("span", "small", `Replaces project ${slot + 1}.`),
    btn("Pick a file", "warn", open),
    btn("Cancel", "alt", () => {
      b.textContent = "";
      b.append(...was);
    })
  );
}

$("pick").addEventListener("change", async (ev) => {
  const f = ev.target.files[0];
  if (!f || !pending) return;
  const { slot, restore } = pending;
  pending = null;
  let list;
  try {
    list = fromFile(await f.text());
  } catch (_) {
    fail({ code: "badfile" });
    return;
  }
  if (restore) {
    askRestore(list);
    return;
  }
  op(async () => {
    await put(slot, list[0]);
    await refresh();
  });
});

async function put(slot, item) {
  // the project, then its demo (or no demo: an old one of that slot goes)
  await dev.write(0, slot, item.project, prog);
  if (item.demo) await dev.write(1, slot, item.demo, prog);
  else if (info.slots[slot].demo) await dev.remove(1, slot);
  prog(null);
}
async function putAll(list) {
  const n = Math.min(list.length, info.slots.length);
  for (let i = 0; i < n; i++) await put(i, list[i]);
  await refresh();
}
function askRestore(list) {
  // the warning only when something would really be replaced
  const n = Math.min(list.length, info.slots.length);
  let used = 0;
  for (let i = 0; i < n; i++) if (info.slots[i].used) used++;
  const go = () => {
    $("confirm").hidden = true;
    op(() => putAll(list));
  };
  if (!used) {
    go();
    return;
  }
  const c = $("confirm");
  c.textContent = `This replaces ${
    n === 1 ? "project 1" : `projects 1 to ${n}`
  }. `;
  c.append(
    btn("Replace", "warn", go),
    " ",
    btn("Cancel", "alt", () => {
      c.hidden = true;
    })
  );
  c.hidden = false;
}

$("backup").addEventListener("click", () =>
  op(async () => {
    const out = [];
    for (let i = 0; i < info.slots.length; i++) {
      if (!info.slots[i].used) continue;
      out.push({
        name: "Project " + (i + 1),
        project: await dev.read(0, i, prog),
        demo: info.slots[i].demo ? await dev.read(1, i, prog) : null,
      });
    }
    prog(null);
    if (!out.length) {
      say("Nothing to back up.");
      return;
    }
    save(
      `smileyface-backup-${new Date().toISOString().slice(0, 10)}.smileyface`,
      toFile(out)
    );
  })
);

$("restore").addEventListener("click", () => {
  $("confirm").hidden = true;
  pending = { slot: 0, restore: true };
  $("pick").value = "";
  $("pick").click();
});

$("connect").addEventListener("click", () => {
  if (!navigator.requestMIDIAccess) {
    say(TEXT.nomidi);
    return;
  }
  op(async () => {
    say("");
    try {
      dev = await Device.connect();
      await refresh();
      $("connect").textContent = "Refresh";
    } catch (e) {
      dev = null;
      throw e;
    }
  });
});
