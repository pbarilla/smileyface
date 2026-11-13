#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The lessons that are made from data: the masterclasses (long lessons that teach WHY a sound behaves as it does: pages to read alternate with things to do) and the
# newer guided lessons, with the patches they start from. This file is the source. It writes firmware/src/lessons_mc_<id>.h (one masterclass each),
# lessons_guided.h (the guided lessons, placed after Punch-in FX) and lessons_mc_patches.h (the patch table); tests/lesson_test.py reads the same lists to do each lesson.
#   python3 tools/masterclasses.py
#
# PAGES   R(title, body, patch): title 20 characters at most; body wrapped to 26 characters a line, 11 lines a page (a long body becomes several pages: split at a sentence).
#         patch: the number of a patch (below) the chapter starts from, set when the page shows; 0 keeps the sound as it is.
# STEPS   D(text, pill, value, op, number, aux, action). text: 26 characters at most, verb first, names the control and what it does to a visible thing ('Turn KNOB 1 for CUTOFF').
#         value: an LV_ name without the prefix: VIEW NOTES ACC SLD CUR ONNOTE RUN BPM PAT PHASH PAGE CHAIN FX FXM LEN KALL CUT RES ENVM DEC KN PAD OCT PATS PFXB LVL KEYS (a key is down)
#           KN: aux = the knob's byte in knobs_t (KNB['cut'] ...) or 200 for the volume decay (DECAY). Read as unsigned.   PAD: aux = a switch (PAD_ORDER index): WAVE 0 saw 1 square, SUB 0..3,
#           MUF 0..2, SWEEP 0..2 (slow, mid, fast), HIRES 0 no sweep + hi res, 1 sweep + hi res, 2 sweep, POLES 0 = 2, 1 = 3, 2 = 4, ACCON 1 = accents OFF, BASS 0..2, DRIFT 0..2, DIODE 0..1.
#           PFXB: aux 0..6 = the bass's own FX pads 9..15 held. LVL: the LEVEL knob as it is drawn (0..31).
#         op: EQ GE (done when true; a step already true when it shows waits until it is undone), CHG UP DOWN (against the value when the previous step finished).
#         A knob step (pill 'KNOB n' with CHG / UP / DOWN) needs at least 4 clicks of a knob.
#         action: what the host test does: ("tap", 'PLAY') ("knob", n, clicks) exact clicks, sign = direction ("alg", page, 0) ALGORITHM to a page (relative, the generator counts) ("key", n) a key 0..26
#           ("hold", 'ENV'|'SEL'|'EDIT', ms) ("py", "code using r") with r.tap r.key r.ev r.enc r.wait r.step and SEL ALG K1..K4 PRE. A page needs none (OCT+ is pressed).
# PATCHES patch(mc, id, kn={...}, pads={...}, vol=31, pat='ONE' or a list of (note, 'a'/'s'/'as'/'') x16, bpm=120, length=16): what the voice and pattern 1 are. mc = the masterclass number; 99 = a
#         guided lesson's start. Unspecified knobs are the defaults (KN_DEF).
import os, re, textwrap

KN_ORDER = ["cut", "res", "env", "dec", "acc", "sld", "drv", "wave", "adc", "trk", "atk", "muf", "tun", "sub", "swp", "pol", "fm", "hir", "swg", "lvl"]
KNB = {n: i for i, n in enumerate(KN_ORDER)}
KN_DEF = dict(cut=14, res=20, env=18, dec=12, acc=16, sld=14, drv=4, wave=0, adc=4, trk=12, atk=2, muf=0, tun=16, sub=0, swp=12, pol=31, fm=0, hir=0, swg=0, lvl=0)
PAD_ORDER = ["wave", "sub", "muf", "sweep", "hires", "poles", "accon", "bass", "drift", "diode"]
PADN = {n: i for i, n in enumerate(PAD_ORDER)}
VDEC = 200

def _p(spec):
    out = []
    for n, f in spec: out.append((n, f))
    return out
PATS = {
    "ONE": [(36, "")] * 16,
    "MEL": _p([(36, ""), (0, ""), (38, ""), (0, ""), (39, ""), (0, ""), (43, ""), (0, ""), (41, ""), (0, ""), (39, ""), (0, ""), (38, ""), (0, ""), (36, ""), (0, "")]),
    "ACC4": [(36, "a" if i % 4 == 0 else "") for i in range(16)],
    "PILE": [(36, "a" if i % 8 < 4 else "") for i in range(16)],
    "JUMP": _p([(36, "s"), (48, ""), (0, ""), (0, "")] * 4),
}

class R:
    def __init__(s, title, body, patch=0): s.title, s.body, s.patch = title, " ".join(body.split()), patch
def D(text, pill, v, op, n, a, act): return (text, pill, v, op, n, a, act)
def KEY(text="Press a key to hear it", n=0): return D(text, "KEYS", "KEYS", "EQ", 1, 0, ("key", n))
def PLAY(text="Press PLAY to hear it"): return D(text, "PLAY", "RUN", "EQ", 1, 0, ("tap", "PLAY"))
def STOP(text="Press PLAY to stop"): return D(text, "PLAY", "RUN", "EQ", 0, 0, ("tap", "PLAY"))
def KN(n, text, v, op, num, a, clicks): return D(text, "KNOB %d" % n, v, op, num, a, ("knob", n, clicks))
def ALG(page, text): return D(text, "ALGORITHM", "PAGE", "EQ", page, 0, ("alg", page))

MC = []                                          # (id, name, items)
GUIDED = []                                      # (name, start patch or 0, items)
PATCHES = []                                     # (mc, id, kn, pads, vol, pat, bpm, length)
def masterclass(mid, name, *items): MC.append((mid, name, list(items)))
def guided(name, start, *items): GUIDED.append((name, start, list(items)))
def patch(mc, pid, kn=None, pads=None, vol=31, pat="ONE", bpm=120, length=16): PATCHES.append((mc, pid, dict(kn or {}), dict(pads or {}), vol, pat, bpm, length))

# ===== the content goes below this line =====

# ---- helpers (content only) ----
def KNV(n, text, name, op, num, clicks): return D(text, "KNOB %d" % n, "KN", op, num, KNB[name], ("knob", n, clicks))
def SW(n, text, name, op, num, clicks): return D(text, "KNOB %d" % n, "PAD", op, num, PADN[name], ("knob", n, clicks))
def LVLV(n, text, op, num, clicks): return D(text, "KNOB %d" % n, "LVL", op, num, 0, ("knob", n, clicks))
def PG(page): return ALG(page, "")
def HOLDKEY(text, pill, mask, key, v, op, n): return D(text, pill, v, op, n, 0, ("py", "r.ev('held %s',100); r.key(%d); r.ev('held 0',100)" % (mask, key)))
def PADHOLD(text, key, a): return D(text, "KEYS", "PFXB", "EQ", 1, a, ("py", "r.ev('keys %s',100); r.wait(500); r.ev('keys 0',100)" % hex(1 << key)))
def fix(items):                                   # ALGORITHM step texts are made from the page they go to
    out, cur = [], 0
    for it in items:
        if isinstance(it, R):
            if it.patch: cur = 0
        elif it[6][0] == "alg":
            d = it[6][1] - cur; cur = it[6][1]
            t = ("Turn ALGORITHM one page on" if d == 1 else "Turn ALGORITHM %d pages on" % d) if d > 0 else ("Turn ALGORITHM one back" if d == -1 else "Turn ALGORITHM back %d" % -d)
            it = (t,) + tuple(it[1:])
        out.append(it)
    return out
def MCX(mid, name, *items): masterclass(mid, name, *fix(items))
def GDX(name, start, *items): guided(name, start, *fix(items))
def mk(notes, acc=(), sld=()): return [(n, ("a" if i in acc else "") + ("s" if i in sld else "")) for i, n in enumerate(notes)]
RIFFN = [36, 36, 48, 36, 39, 0, 36, 43, 36, 36, 46, 0, 36, 48, 39, 41]
PATS["RIFF"] = mk(RIFFN)
PATS["RIFFA"] = mk(RIFFN, acc=(0, 7, 12))
PATS["RIFFAS"] = mk(RIFFN, acc=(0, 7, 12), sld=(3, 9, 14))
M8 = [36, 38, 39, 43, 41, 39, 38, 36] + [0] * 8
PATS["MEL8"] = mk(M8)
PATS["MEL8A"] = mk(M8, acc=(0, 3, 5))
PATS["MEL8S"] = mk(M8, sld=(0, 3, 5))
PATS["JUMPA"] = mk([36, 48, 0, 0] * 4, acc=(1, 5, 9, 13), sld=(0, 4, 8, 12))
PATS["JUMPS"] = mk([36, 48, 0, 0] * 4, sld=(0, 4, 8, 12))
def cl(**k):
    d = dict(trk=0, res=0, env=0); d.update(k); return d

# ============================================================================== 1  303 STORY
patch(1, 1, cl(cut=8), pat="RIFF")
patch(1, 3, cl(cut=8, res=22, env=20, dec=10), pat="RIFF")
patch(1, 4, cl(cut=8, res=22, env=20, dec=10), pat="RIFFA")
patch(1, 5, cl(cut=8, res=22, env=20, dec=10), pat="RIFFAS")
MCX(1, "Masterclass: 303 Story",
  R("Made for guitarists", "The TB-303 is a Roland box from the early 1980s. TB means Transistor Bass. Roland says it was made to play bass for solo guitarists. It has a 16-step sequencer, one oscillator and one filter.", 1),
  R("Not a bass guitar", "Pair it with the TR-606 drums and a guitarist needs no bass player. That was the plan. It did not sound much like a bass guitar. Press PLAY and judge."),
  PLAY(),
  R("A flop", "About 10,000 were made. It cost $395 and flopped: hard to program, strange sounding, no English manual in the US. Roland stopped making it in 1984 and shops dumped the stock."),
  R("Chicago, 1987", "Chicago kids bought them for as little as $50. Phuture (DJ Pierre, Spanky and Herb J) made Acid Tracks, out on Trax Records in 1987. Spanky had bought a used 303."),
  R("Twist with the beat", "DJ Pierre tells it: he started twisting the knobs, and they fell in love with how he twisted them with the beat. A 303 has no LFO: every move is a hand. Press PLAY and be Pierre.", 1),
  PLAY(),
  KNV(1, "Turn KNOB 1 right: open up", "cut", "UP", 0, 14),
  R("Squelch", "RESON feeds the filter back into itself. It boosts the sound right at the cutoff. That is the squelch. Turn it up, then sweep CUTOFF."),
  KNV(2, "Turn KNOB 2 right: RESON", "res", "UP", 0, 12),
  KNV(1, "Turn KNOB 1 for CUTOFF", "cut", "CHG", 0, 10),
  R("Envelope", "Each note opens the filter and it closes again. ENVMOD is how far it opens. NRM DEC is how fast it closes. Together they make the wow."),
  KNV(3, "Turn KNOB 3 right: ENVMOD", "env", "UP", 0, 12),
  KNV(4, "Turn KNOB 4 for NRM DEC", "dec", "CHG", 0, 8),
  R("Accent", "Accent makes a note louder and brighter, and sweeps the filter. Some steps of this riff are accented now. Listen, then turn ACCENT.", 4),
  PLAY(),
  PG(1),
  KNV(2, "Turn KNOB 2 for ACCENT", "acc", "CHG", 0, 10),
  R("Slide", "A slide glides to the next note without a new attack. A tie holds one pitch, a slide changes it. Some steps slide now.", 5),
  PLAY(),
  PG(1),
  KNV(3, "Turn KNOB 3 for SLIDE", "sld", "CHG", 0, 10),
  R("Four poles or three", "Roland says the filter has four poles and 24 dB. Many guides say three poles and 18 dB, and Roland says that is often misquoted. POLES lets you pick 2, 3 or 4."),
  PG(5),
  SW(3, "Turn KNOB 3 left: 3 poles", "poles", "EQ", 1, -1),
  SW(3, "Turn KNOB 3 right: 4 poles", "poles", "EQ", 2, 1),
  R("The word acid", "Where the word acid comes from is disputed. Pierre says he meant a harsh, biting sound. Acid house then crossed to London: the Shoom club opened in November 1987 and the smiley face became its symbol."),
  R("Blade, 1998", "The opening rave of the film Blade plays Pump Panel's remix of New Order's Confusion. Red Bull Music Academy hears an overdriven 303 lead in it. Try DRIVE."),
  PG(2),
  KNV(1, "Turn KNOB 1 right: DRIVE", "drv", "GE", 20, 18),
  R("Blade Runner", "Blade Runner (1982) is a different thing. Its sound is Vangelis on a Yamaha CS-80, a polyphonic synth. It is not a 303. A common mix-up."),
  R("Later life", "Robin Whittle's Devil Fish mod widened the 303's filter range. ReBirth put two 303s on a computer in 1996. Roland made the TB-03 in 2017. Behringer's TD-3 clone came in 2019."),
  R("Our take", "This page is OUR TAKE, not history. A loop repeats exactly, on a strict grid. It never tires and never drifts. That cold, strict feel is the machine.", 5),
  R("Our take: the hand", "OUR TAKE, part two. Now a hand moves a dial, never the same way twice. The filter opens and closes like a voice. Cold grid, warm hand: that can sound like the future. Twist with the beat."),
  PLAY(),
  KNV(1, "Turn KNOB 1 for CUTOFF", "cut", "CHG", 0, 10),
  KNV(2, "Turn KNOB 2 for RESON", "res", "CHG", 0, 10),
  KNV(3, "Turn KNOB 3 for ENVMOD", "env", "CHG", 0, 10),
  KNV(4, "Turn KNOB 4 for NRM DEC", "dec", "CHG", 0, 10),
)

# ============================================================================== 2  FILTER
patch(2, 1, cl(cut=31))
patch(2, 2, cl(cut=14))
patch(2, 3, cl(cut=12))
patch(2, 4, cl(cut=12, res=28))
patch(2, 5, cl(cut=12, res=20))
patch(2, 6, cl(cut=8, res=20, env=24, dec=12))
patch(2, 7, cl(cut=6, res=24, env=22, dec=12))
patch(2, 8, cl(cut=10, res=22, env=8), pat="MEL")
MCX(2, "Masterclass: Filter",
  R("A plain saw", "A saw wave holds every harmonic, so it sounds bright and buzzy. The CUTOFF is almost wide open. Listen to it first.", 1),
  PLAY(),
  R("What a filter does", "A low pass filter lets low tones through and cuts the high ones. What is left is the colour of the sound. CUTOFF sets where the cut starts."),
  KNV(1, "Turn KNOB 1 left: darker", "cut", "DOWN", 0, -12),
  R("Cutoff in hertz", "CUTOFF 0 is 28 Hz. 8 is 106 Hz. 14 is 288 Hz. 20 is 780 Hz. The bass note here is 65 Hz. One click is about a quarter of an octave."),
  KNV(1, "Turn KNOB 1 full left", "cut", "EQ", 0, -31),
  KNV(1, "Turn KNOB 1 full right", "cut", "GE", 31, 31),
  R("Poles", "A filter's slope is its poles. With more poles the highs fall away faster above the cutoff. POLES picks 2, 3 or 4. Hear the 2-pole filter, then the 4-pole.", 2),
  PLAY(),
  PG(5),
  SW(3, "Turn KNOB 3 to 2 poles", "poles", "EQ", 0, -2),
  SW(3, "Turn KNOB 3 to 4 poles", "poles", "EQ", 2, 2),
  R("Resonance", "RESON feeds the filter's output back to its input. That boosts the sound right at the cutoff. At the top it is just short of ringing by itself.", 3),
  PLAY(),
  KNV(2, "Turn KNOB 2 full right", "res", "GE", 28, 31),
  KNV(1, "Turn KNOB 1 for CUTOFF", "cut", "CHG", 0, 12),
  R("The thin bass", "More RESON eats the low end: the bass gets thin. BASS adds the oscillator's own low end back, under the filter, once or twice.", 4),
  PLAY(),
  PG(6),
  SW(1, "Turn KNOB 1 right: BASS", "bass", "GE", 1, 1),
  SW(1, "Turn KNOB 1 right again", "bass", "EQ", 2, 1),
  R("Drive", "DRIVE multiplies the oscillator before a soft clip. 0 is silent. The default is 1.33 times, 16 is 3.3, 24 is 19, 31 is 88.8. The clip sits inside the loop, so it tames the resonance too.", 5),
  PLAY(),
  PG(2),
  KNV(1, "Turn KNOB 1 right: DRIVE", "drv", "GE", 20, 16),
  KNV(1, "Turn KNOB 1 full left", "drv", "EQ", 0, -31),
  R("The sweep", "Every note starts with the cutoff pushed up by ENVMOD, then it falls back. NRM DEC is how fast. At the default, ENVMOD lifts the cutoff 2.7 octaves. No attack: it starts at the top.", 6),
  PLAY(),
  KNV(3, "Turn KNOB 3 for ENVMOD", "env", "CHG", 0, 8),
  KNV(4, "Turn KNOB 4 right: slower", "dec", "UP", 0, 14),
  KNV(4, "Turn KNOB 4 left: faster", "dec", "DOWN", 0, -20),
  R("Why it talks", "A voice makes vowels by moving a resonant peak. Here the resonant peak moves with every note. A fast fall says yow. A slow fall says wow.", 7),
  PLAY(),
  KNV(4, "Turn KNOB 4 left: yow", "dec", "DOWN", 0, -8),
  KNV(4, "Turn KNOB 4 right: wow", "dec", "UP", 0, 24),
  R("Track the note", "On a plain filter every note gets the same cutoff. TRACK lets higher notes open it more. Zero is note 37. At 12 each semitone adds 87 Hz, at 31 it adds 225 Hz.", 8),
  PLAY(),
  PG(2),
  KNV(3, "Turn KNOB 3 right: TRACK", "trk", "UP", 0, 16),
  R("The whole path", "Saw, then DRIVE, then four filter stages in a row (CUTOFF, RESON, ENVMOD), then the volume. Every sweep you hear is those few knobs."),
)

# ============================================================================== 3  ACCENT
patch(3, 1, cl(cut=10, res=20, env=18, dec=12), pat="ACC4")
patch(3, 2, cl(cut=10, res=22, env=14, dec=12), pat="PILE")
patch(3, 3, cl(cut=12, res=12, env=10, dec=10), pat="JUMP")
patch(3, 4, cl(cut=12, res=20, env=14, dec=10), pat="JUMPA")
patch(3, 5, cl(cut=10, res=20, env=18, dec=12))
MCX(3, "Masterclass: Accent",
  R("Three kinds of step", "A step is a rest, a plain note, or a note with ACCENT and or SLIDE. A plain note lasts 5/8 of a step. Every fourth step is accented here.", 1),
  PLAY(),
  R("What accent does", "An accented note gets a shorter filter decay (ACC DEC), a bigger sweep, a little more volume and a fast attack. ACCENT sets how much. Only the accented steps change."),
  PG(1),
  KNV(2, "Turn KNOB 2 right: ACCENT", "acc", "UP", 0, 12),
  R("Two decays", "Accented notes have their own filter decay, ACC DEC. Plain notes use NRM DEC. By default 54 ms against 178 ms: the accent is shorter and sharper."),
  PG(2),
  KNV(2, "Turn KNOB 2 right: ACC DEC", "adc", "UP", 0, 12),
  R("Accents pile up", "On a 303 the accent circuit does not fully discharge between accents, so a run of accents climbs higher. Ours does too. SWEEP sets how long the pile lasts.", 2),
  PLAY(),
  PG(4),
  SW(4, "Turn KNOB 4 left: slow", "sweep", "EQ", 0, -1),
  SW(4, "Turn KNOB 4 right: fast", "sweep", "EQ", 2, 2),
  R("Accent sweep switch", "ACC SWP has three spots. Right: the accent sweeps. Middle: sweeps, with HI RES. Left: no sweep, no extra volume, HI RES. HI RES doubles the resonance."),
  PG(5),
  SW(1, "Turn KNOB 1 full left", "hires", "EQ", 0, -2),
  SW(1, "Turn KNOB 1 full right", "hires", "EQ", 2, 2),
  R("Slide", "Slide moves the pitch in semitones, so the glide is even. SLIDE sets the time: about 22 ms at the left end, 132 ms at the right. A slide flag ties its note to the next.", 3),
  PLAY(),
  PG(1),
  KNV(3, "Turn KNOB 3 right: slow", "sld", "GE", 28, 16),
  KNV(3, "Turn KNOB 3 left: quick", "sld", "DOWN", 0, -20),
  R("No new sweep", "A slid note does not restart the filter sweep: the pitch glides, the filter carries on. Its volume is plucked again, so the glide never fades out."),
  PG(0),
  KNV(3, "Turn KNOB 3 right: ENVMOD", "env", "UP", 0, 10),
  R("Slide into accent", "When a slide lands on an accented note, the sweep carries on and the accent boost lands on top, so the cutoff jumps. Listen for it.", 4),
  PLAY(),
  PG(1),
  KNV(2, "Turn KNOB 2 right: ACCENT", "acc", "UP", 0, 12),
  R("Write your own", "Hold ENV while you press a key: that note is accented. Hold SEL: it slides. A key written over a step clears its flags. LFO writes a random riff.", 5),
  HOLDKEY("Hold ENV, press a key", "ENV", "0x10", 0, "ACC", "UP", 0),
  HOLDKEY("Hold SEL, press a key", "SEL", "0x8", 2, "SLD", "UP", 0),
  D("Press LFO: a random riff", "LFO", "PHASH", "CHG", 0, 0, ("tap", "LFO")),
  PLAY(),
  STOP(),
)

# ============================================================================== 4  DEVIL FISH
patch(4, 1, cl(cut=14, res=20, env=18), pat="MEL")
patch(4, 2, cl(cut=10, res=20, env=18), pat="ACC4")
patch(4, 3, cl(cut=10, res=14, env=14, dec=8), pat="ACC4")
patch(4, 4, cl(cut=12, res=14, env=12), pat="JUMP")
patch(4, 5, cl(cut=10, res=20, env=10), pat="MEL")
patch(4, 6, cl(cut=12, res=22, env=12), pat="MEL")
patch(4, 7, cl(cut=10, res=22, env=8), pat="MEL")
MCX(4, "Masterclass: Devil Fish",
  R("What it adds", "Robin Whittle's Devil Fish mod gave the 303 extras: a wider cutoff range, switchable self-resonance, variable tracking, longer slides. He called it the most dynamic, pulsating, wailing musical instrument he knew.", 1),
  R("Where they live", "Smileyface has these ideas, with its own numbers. Two pages on: DRIVE, ACC DEC, TRACK, ATTACK. Three on: FILT FM, DECAY. Four and five on: SWEEP, ACC SWP."),
  R("Overdrive", "DRIVE pushes the saw into a soft clip before the filter. The default is 1.33 times. The top is 66.6 times that, the Devil Fish figure. 0 is silence."),
  PLAY(),
  PG(2),
  KNV(1, "Turn KNOB 1 right: DRIVE", "drv", "GE", 16, 12),
  KNV(1, "Turn KNOB 1 further right", "drv", "GE", 24, 8),
  KNV(1, "Turn KNOB 1 full right", "drv", "GE", 31, 8),
  KNV(1, "Turn KNOB 1 full left", "drv", "EQ", 0, -31),
  R("Accent decay", "ACC DEC is the filter decay of accented notes. NRM DEC does the rest. Make the accents longer and they turn into wows.", 2),
  PLAY(),
  PG(2),
  KNV(2, "Turn KNOB 2 right: ACC DEC", "adc", "UP", 0, 14),
  KNV(2, "Turn KNOB 2 left: ACC DEC", "adc", "DOWN", 0, -20),
  R("Soft attack", "ATTACK fades plain notes in, from about 0.6 to 9 ms. A fast accent ignores it and stabs through. Turn it up: plain notes soften, accents still hit.", 3),
  PLAY(),
  PG(2),
  KNV(4, "Turn KNOB 4 right: ATTACK", "atk", "GE", 24, 22),
  R("Volume decay", "DECAY is the volume decay, not the filter. At the right end the note holds. Left of that it falls to a floor, then it plucks: 3 s at 15, 261 ms at 8, 16 ms at 0.", 4),
  PLAY(),
  PG(3),
  D("Turn KNOB 2 left: DECAY", "KNOB 2", "KN", "DOWN", 0, 200, ("knob", 2, -8)),
  D("Turn KNOB 2 full left", "KNOB 2", "KN", "EQ", 0, 200, ("knob", 2, -31)),
  R("Hi res", "ACC SWP in the middle or at the left turns on HI RES: the resonance doubles. From RESON 20 up, the loop passes 4 and the filter rings by itself.", 5),
  PLAY(),
  PG(5),
  SW(1, "Turn KNOB 1 left: HI RES", "hires", "EQ", 1, -1),
  SW(1, "Turn KNOB 1 full left", "hires", "EQ", 0, -1),
  R("Filter FM", "FILT FM feeds the sound back into the cutoff, per sample. Little happens until well up, then it goes wild: about 2 octaves of shake at the top.", 6),
  PLAY(),
  PG(3),
  KNV(1, "Turn KNOB 1 right: FILT FM", "fm", "GE", 20, 20),
  KNV(1, "Turn KNOB 1 full right", "fm", "GE", 31, 12),
  KNV(1, "Turn KNOB 1 full left", "fm", "EQ", 0, -31),
  R("Track", "TRACK lets higher notes open the filter more. Zero is note 37. At 12 each semitone adds 87 Hz, at 31 it adds 225 Hz.", 7),
  PLAY(),
  PG(2),
  KNV(3, "Turn KNOB 3 right: TRACK", "trk", "UP", 0, 16),
  R("Sweep pad", "FX holds a SWEEP pad too: the cutoff sweeps the whole range about every 1.5 seconds. Hold it while the riff plays."),
  D("Press FX to show the pads", "FX", "FX", "EQ", 1, 0, ("tap", "FX")),
  PADHOLD("Hold the 14th key: SWEEP", 23, 5),
  D("Press FX to leave", "FX", "FX", "EQ", 0, 0, ("tap", "FX")),
  R("The row", "That is the Devil Fish row: DRIVE, ACC DEC, ATTACK, DECAY, HI RES, TRACK, and a filter that can shake itself."),
)

# ============================================================================== 5  TONE
patch(5, 1, cl(cut=24))
patch(5, 2, cl(cut=12, res=28))
patch(5, 3, cl(cut=20, res=6, env=10), pat="MEL")
patch(5, 4, cl(cut=14, res=24, env=14))
patch(5, 5, cl(cut=14, res=18, env=14))
patch(5, 6, cl(cut=20, res=10, env=8))
MCX(5, "Masterclass: Tone",
  R("Saw or square", "A saw holds every harmonic: bright and buzzy. A square holds only the odd ones: hollow, like a clarinet. Same note, different colour.", 1),
  PLAY(),
  PG(4),
  SW(1, "Turn KNOB 1 right: square", "wave", "EQ", 1, 1),
  SW(1, "Turn KNOB 1 left: saw", "wave", "EQ", 0, -1),
  R("Sub oscillator", "SUB OSC adds a square one octave down. Low, mid and high are 16, 32 and 41 percent of the saw's level."),
  SW(2, "Turn KNOB 2 right: SUB OSC", "sub", "GE", 1, 1),
  SW(2, "Turn KNOB 2 full right", "sub", "GE", 3, 2),
  R("Bass under it", "A resonant filter eats the low end. The real 303 loses quite a lot of bass between its filter and its amp. BASS adds the oscillator's low end back, once or twice.", 2),
  PLAY(),
  PG(6),
  SW(1, "Turn KNOB 1 right: BASS", "bass", "GE", 1, 1),
  SW(1, "Turn KNOB 1 right again", "bass", "EQ", 2, 1),
  R("Muffle", "MUFFLE is a soft clip after the volume: 1.5, 6.5 or 12.5 times. It squashes the loud peaks of each note.", 3),
  PLAY(),
  PG(4),
  SW(3, "Turn KNOB 3 right: MUFFLE", "muf", "GE", 1, 1),
  SW(3, "Turn KNOB 3 right again", "muf", "EQ", 2, 1),
  R("Diode", "The 303's filter is a diode ladder. DIODE clips every stage on one side, feeds the oscillator in 2.5 times harder and takes a quarter off the resonance.", 4),
  PLAY(),
  PG(6),
  SW(3, "Turn KNOB 3 right: DIODE", "diode", "EQ", 1, 1),
  SW(3, "Turn KNOB 3 left: off", "diode", "EQ", 0, -1),
  R("Drift", "DRIFT makes each note a little off. At 1 the pitch is off by up to 28 cents and the cutoff by up to 0.8 octave. At 2: 66 cents and 2.2 octaves. A slow wander too.", 5),
  PLAY(),
  PG(6),
  SW(2, "Turn KNOB 2 right: DRIFT", "drift", "EQ", 1, 1),
  SW(2, "Turn KNOB 2 right again", "drift", "EQ", 2, 1),
  R("Stack the grit", "Clips add up. DRIVE clips before the filter, DIODE inside it, MUFFLE after the volume. Switch them on one by one.", 6),
  PLAY(),
  PG(2),
  KNV(1, "Turn KNOB 1 right: DRIVE", "drv", "GE", 24, 20),
  PG(4),
  SW(3, "Turn KNOB 3 full right", "muf", "EQ", 2, 2),
  PG(6),
  SW(3, "Turn KNOB 3 right: DIODE", "diode", "EQ", 1, 1),
  R("No clicks", "A note ends in about 10 ms, not at once, and the DC blocker fades out with the volume. So the end of a note never ticks."),
)

# ============================================================================== GUIDED
patch(99, 1, pat="MEL8A")
patch(99, 2, pat="MEL8S")
patch(99, 3, pat="MEL8")
GDX("Write Live", 0,
  HOLDKEY("Hold ENV, press a key", "ENV", "0x10", 0, "ACC", "UP", 0),
  HOLDKEY("Hold SEL, press a key", "SEL", "0x8", 2, "SLD", "UP", 0),
  D("Press OCT+ or OCT-", "OCT", "OCT", "CHG", 0, 0, ("tap", "OCTU")),
  KEY("Press a key: new octave", 4),
  PLAY("Press PLAY to hear it"),
  STOP(),
)
GDX("Hit Harder", 1,
  PLAY("Press PLAY to start"),
  PG(1),
  KNV(2, "Turn KNOB 2 for ACCENT", "acc", "CHG", 0, 8),
  PG(2),
  KNV(2, "Turn KNOB 2 for ACC DEC", "adc", "CHG", 0, 8),
  PG(4),
  SW(4, "Turn KNOB 4 for SWEEP", "sweep", "CHG", 0, -4),
)
GDX("Glide", 2,
  PLAY("Press PLAY to start"),
  PG(1),
  KNV(3, "Turn KNOB 3 for SLIDE", "sld", "CHG", 0, 6),
  KNV(3, "Turn KNOB 3 right: slow", "sld", "GE", 28, 24),
  KNV(3, "Turn KNOB 3 left: quick", "sld", "DOWN", 0, -26),
  STOP(),
)
GDX("Soft and Short", 3,
  PLAY("Press PLAY to start"),
  PG(2),
  KNV(4, "Turn KNOB 4 for ATTACK", "atk", "CHG", 0, 8),
  PG(3),
  D("Turn KNOB 2 left: DECAY", "KNOB 2", "KN", "DOWN", 0, 200, ("knob", 2, -14)),
  LVLV(4, "Turn KNOB 4 for LEVEL", "CHG", 0, -4),
  STOP(),
)
GDX("Make It Sing", 3,
  PLAY("Press PLAY to start"),
  PG(5),
  SW(1, "Turn KNOB 1 left: HI RES", "hires", "EQ", 1, -1),
  PG(2),
  KNV(1, "Turn KNOB 1 full left", "drv", "EQ", 0, -6),
  PG(0),
  KNV(2, "Turn KNOB 2 right: RESON", "res", "GE", 26, 12),
)
GDX("Colour", 3,
  PLAY("Press PLAY to start"),
  PG(5),
  SW(3, "Turn KNOB 3 left: 2 poles", "poles", "EQ", 0, -3),
  PG(6),
  SW(3, "Turn KNOB 3 right: DIODE", "diode", "EQ", 1, 1),
  SW(1, "Turn KNOB 1 right: BASS", "bass", "GE", 1, 1),
  SW(2, "Turn KNOB 2 right: DRIFT", "drift", "GE", 1, 1),
)
GDX("Groove", 3,
  PLAY("Press PLAY to start"),
  PG(5),
  KNV(2, "Turn KNOB 2 for SWING", "swg", "CHG", 0, 8),
  PG(3),
  D("Turn KNOB 3 left: LENGTH", "KNOB 3", "LEN", "DOWN", 0, 0, ("knob", 3, -4)),
  D("Hold SEL, turn SELECT", "SEL", "PHASH", "CHG", 0, 0, ("py", "r.ev('held 0x8',100); r.enc(SEL,1); r.ev('held 0',100)")),
  STOP(),
)
GDX("Live Cue", 3,
  D("Tap HOME until PLAY shows", "HOME", "VIEW", "EQ", 3, 0, ("py", "home(r, 3)")),
  D("Hold SEL to copy it", "SEL", "PATS", "UP", 0, 0, ("py", "r.tap('SEL', hold=700)")),
  PLAY("Press PLAY to start"),
  D("Press the 1st key: cue it", "KEYS", "KEYS", "EQ", 1, 0, ("key", 0)),
  D("Wait for the loop to end", "PLAY", "PAT", "CHG", 0, 0, ("py", "r.wait(2500)")),
  STOP(),
)
GDX("Pattern Tools", 3,
  D("Tap HOME until PLAY shows", "HOME", "VIEW", "EQ", 3, 0, ("py", "home(r, 3)")),
  D("Hold SEL to copy it", "SEL", "PATS", "UP", 0, 0, ("py", "r.tap('SEL', hold=700)")),
  D("Hold EDIT, turn SELECT", "EDIT", "PAT", "CHG", 0, 0, ("py", "r.ev('held 0x40',100); r.enc(SEL,1); r.ev('held 0',100)")),
  D("Press OCT+ to add to chain", "OCT+", "CHAIN", "UP", 0, 0, ("tap", "OCTU")),
  D("Press OCT+ to add it again", "OCT+", "CHAIN", "GE", 2, 0, ("tap", "OCTU")),
  D("Press OCT- to take one off", "OCT-", "CHAIN", "DOWN", 0, 0, ("tap", "OCTD")),
  D("Tap EDIT to clear chain", "EDIT", "CHAIN", "EQ", 0, 0, ("py", "r.ev('held 0x40',100); r.ev('held 0',100)")),
)
GDX("Acid Pads", 3,
  PLAY("Press PLAY to start"),
  D("Press FX to show the pads", "FX", "FX", "EQ", 1, 0, ("tap", "FX")),
  PADHOLD("Hold the 9th key: ACCENT", 14, 0),
  PADHOLD("Hold the 13th: RESMAX", 21, 4),
  PADHOLD("Hold the 14th: SWEEP", 23, 5),
  PADHOLD("Hold the 15th: SHORT", 24, 6),
  D("Press FX to leave", "FX", "FX", "EQ", 0, 0, ("tap", "FX")),
)

# ===== the content goes above this line =====

MAXL, WIDTH = 11, 26
def expand(items):                                # a long page becomes two or three: split at sentences, 11 lines each
    out = []
    for it in items:
        if not isinstance(it, R): out.append(it); continue
        sents = re.split(r"(?<=[.!?:])\s+", it.body)
        chunks, cur = [], []
        for sn in sents:
            if cur and len(textwrap.wrap(" ".join(cur + [sn]), WIDTH)) > MAXL: chunks.append(" ".join(cur)); cur = []
            cur.append(sn)
        chunks.append(" ".join(cur))
        for k, ch in enumerate(chunks):
            out.append(R(it.title if k == 0 else ("%s %d" % (it.title[:17], k + 1)), ch, it.patch if k == 0 else 0))
    return out
def wrap(body):
    out = []
    for para in body.split("\n"): out += textwrap.wrap(para, WIDTH) or [""]
    return out
def c_str(s): return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'
def steps_c(name, items):
    items = expand(items)
    parts = []
    for it in items:
        if isinstance(it, R):
            lines = wrap(it.body)
            assert len(lines) <= MAXL and len(it.title) <= 20, (name, it.title)
            parts.append('RD(%s, %s, %d)' % (c_str(it.title), c_str("\n".join(lines)), it.patch))
        else:
            t, p, v, op, n, a, _ = it
            assert len(t) <= 26, (name, t, len(t))
            parts.append('STA(%s, %s, %s, %s, %d, %d)' % (c_str(t), c_str(p), v, op, n, a))
    return len(items), ", ".join(parts)
def actions(items):
    out, cur = [], 0
    for it in expand(items):
        if isinstance(it, R):
            if it.patch: cur = 0
            out.append(("read",)); continue
        act = it[6]
        if act[0] == "alg":
            out.append(("alg", act[1] - cur)); cur = act[1]
        else: out.append(act)
    return out
def pat_rows(pat, length):
    rows = PATS[pat] if isinstance(pat, str) else list(pat)
    rows = list(rows) + [(0, "")] * (16 - len(rows))
    return rows[:16]
def patch_c(mc, pid, kn, pads, vol, pat, bpm, length):
    k = dict(KN_DEF); k.update(kn)
    for n in kn: assert n in KNB, n
    for n in pads: assert n in PADN, n
    rows = pat_rows(pat, length)
    fl = lambda f: " | ".join(x for x, c in (("F_ACC", "a"), ("F_SLD", "s")) if c in f) or "0"
    return "    {%d, %d, {%s}, {%s}, %d, %d, %d, {%s}, {%s}},\n" % (mc, pid, ", ".join(str(k[n]) for n in KN_ORDER), ", ".join(str(pads.get(n, 255)) for n in PAD_ORDER), vol, bpm, length,
                                                              ", ".join(str(n) for n, _ in rows), ", ".join(fl(f) for _, f in rows))
def emit(root):
    src = os.path.join(root, "firmware/src")
    for mid, name, items in MC:
        n, st = steps_c(name, items)
        with open(os.path.join(src, "lessons_mc_%d.h" % mid), "w") as f:
            f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* A masterclass, made by tools/masterclasses.py (edit that, not this). */\n    {%s, LS_MC, %d, (const lstep_t[]){%s}, %d, 0},\n" % (c_str(name), n, st, mid))
    with open(os.path.join(src, "lessons_guided.h"), "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* The guided lessons made from data, by tools/masterclasses.py (edit that, not this). */\n")
        for name, start, items in GUIDED:
            n, st = steps_c(name, items)
            f.write("    {%s, %s, %d, (const lstep_t[]){%s}, 0, %d},\n" % (c_str(name), "LS_PATCH" if start else "LS_EMPTY", n, st, start))
    with open(os.path.join(src, "lessons_mc_patches.h"), "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* The patches of the masterclasses, made by tools/masterclasses.py (edit that, not this). */\n")
        for p in PATCHES: f.write(patch_c(*p))
if __name__ == "__main__":
    emit(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    print(len(MC), "masterclasses,", len(GUIDED), "guided lessons,", len(PATCHES), "patches")
