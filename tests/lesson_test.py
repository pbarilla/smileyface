#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Does every lesson in firmware/src/lessons.c in the host sim and checks that each step finishes (the host prints "LESSON i STEP j OK").
#   python3 tests/lesson_test.py BINARY [lesson numbers ...]      (BINARY: the host build of smileyface.c, see CLAUDE.md)
import os, re, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEL, ALG, K1, K2, K3, K4, PRE = 0, 1, 2, 3, 4, 5, 6
B = dict(OCTD=0, OCTU=1, FX=2, SEL=3, ENV=4, LFO=5, EDIT=6, GLO=7, HOME=8, SAVE=9, ARP=10, SEQ=11, PLAY=12, REC=13)
class R:
    def __init__(s): s.t = 300; s.L = []; s.held = 0
    def ev(s, x, dt=150): s.t += dt; s.L.append(f"{s.t} {x}")
    def wait(s, ms): s.t += ms
    def tap(s, b, hold=90):                                # HOME and SEL are read as levels (a tap is the release), the rest as edges
        if b in ("HOME", "SEL"):
            s.held |= 1 << B[b]; s.ev(f"held {hex(s.held)}", 100); s.t += hold; s.held &= ~(1 << B[b]); s.ev(f"held {hex(s.held)}", 0); s.wait(150)
        else: s.ev(f"btn {B[b]}", 100); s.wait(100)
    def enc(s, i, d=1):
        n = -(-4 // abs(d)) if 2 <= i <= 5 and abs(d) < 4 else 1                  # a knob step wants a few clicks
        for _ in range(n): s.ev(f"enc {i} {d}", 120)
    def encs(s, i, d, n): [s.enc(i, d) for _ in range(n)]
    def key(s, k, hold=150): s.ev(f"keys {hex(1 << k)}", 150); s.ev("keys 0", hold)
    def step(s): s.wait(1300)
def home(r, n=1):
    for _ in range(n): r.tap('HOME'); r.wait(200)
L = [
 lambda r: (home(r), r.step(), home(r), r.step(), home(r), r.step(), home(r)),
 lambda r: (r.key(0), r.step(), [r.key(k) for k in (2, 4, 5)], r.step(), r.enc(SEL, -1), r.step(), r.tap('EDIT'), r.step(), r.tap('PLAY'), r.step(), r.tap('PLAY')),
 lambda r: (r.tap('ENV'), r.step(), r.enc(SEL, 1), r.step(), r.tap('ENV'), r.step(), r.tap('PLAY'), r.step(), r.tap('PLAY')),
 lambda r: (r.tap('SEL'), r.step(), r.enc(SEL, 1), r.step(), r.tap('SEL'), r.step(), r.tap('PLAY')),
 lambda r: (r.tap('EDIT'), r.step(), r.encs(ALG, 1, 3), r.step(), r.enc(K3, -3), r.step(), r.tap('PLAY')),
 lambda r: (r.tap('PLAY'), r.step(), r.enc(K1, 3), r.step(), r.enc(K2, 3), r.step(), r.enc(K3, 3), r.step(), r.enc(K4, 3)),
 lambda r: (r.encs(ALG, 1, 1), r.step(), r.enc(K4, 5), r.step(), r.tap('PLAY'), r.step(), [(r.tap('SEQ'), r.wait(420)) for _ in range(5)]),
 lambda r: (r.tap('PLAY'), r.step(), r.encs(ALG, 1, 2), r.step(), r.enc(K1, 3), r.step(), r.enc(K2, 3), r.step(), r.enc(K3, 3)),
 lambda r: (r.tap('PLAY'), r.step(), r.encs(ALG, 1, 4), r.step(), r.enc(K1, 1), r.step(), r.enc(K2, 1), r.step(), r.enc(K3, 1)),
 lambda r: (r.enc(PRE, 1), r.step(), r.key(2), r.step(), r.enc(PRE, -1), r.step(), r.tap('PLAY')),
 lambda r: (home(r, 3), r.step(), r.tap('OCTU'), r.step(), r.enc(PRE, 1), r.step(), r.tap('OCTU'), r.step(), r.tap('PLAY')),
 lambda r: (r.tap('LFO'), r.step(), r.tap('REC'), r.step(), r.tap('ARP'), r.step(), r.tap('REC'), r.step(), r.tap('PLAY')),
 lambda r: (r.tap('PLAY'), r.step(), r.tap('FX'), r.step(), r.ev("keys 0x1", 100), r.wait(500), r.ev("keys 0", 0), r.step(), r.tap('FX')),
]
NAMES13 = ["Get Around", "First Notes", "Accents", "Slides", "Rests and Length", "Twist the Filter", "Tempo", "Devil Fish", "Switches", "Patterns", "Chain", "Dice and Undo", "Punch-in FX"]
sys.path.insert(0, os.path.join(ROOT, "tools"))
import masterclasses
def data_action(items):                          # a lesson made from data (tools/masterclasses.py)
    acts = masterclasses.actions(items)
    KN = [K1, K2, K3, K4]
    def run(r):
        for a in acts:
            if a[0] == "read": r.tap('OCTU')
            elif a[0] == "tap": r.tap(a[1])
            elif a[0] == "knob":
                for _ in range(abs(a[2])): r.ev(f"enc {KN[a[1] - 1]} {1 if a[2] > 0 else -1}", 120)
            elif a[0] == "alg":
                for _ in range(abs(a[1])): r.ev(f"enc {ALG} {1 if a[1] > 0 else -1}", 120)
            elif a[0] == "key": r.key(a[1])
            elif a[0] == "hold":
                bit = {"ENV": 0x10, "SEL": 0x08, "EDIT": 0x40}[a[1]]
                r.ev(f"held {hex(bit)}", 100); r.wait(a[2]); r.ev("held 0", 100)
            elif a[0] == "py": exec(a[1], dict(globals(), r=r))
            r.step()
    return run
ACTION_BY_NAME = dict(zip(NAMES13, L))
ACTION_BY_NAME.update({name: data_action(items) for _, name, items in masterclasses.MC})
ACTION_BY_NAME.update({name: data_action(items) for name, _, items in masterclasses.GUIDED})
def steps_of():
    src = open(os.path.join(ROOT, "firmware/src/lessons.c")).read()
    body = src[src.index("static const lesson_t LESSON[]"):src.index("#define LES_N")]
    body = re.sub(r'#include "(lessons_\w+\.h)"', lambda m: open(os.path.join(ROOT, "firmware/src", m.group(1))).read(), body)
    out = []
    for m in re.finditer(r'\{"([^"]+)", LS_\w+, (\d+), \(const lstep_t\[\]\)\{(.*?)\}(?:, \d+(?:, \d+)?)?\},\n', body, re.S):
        out.append((m.group(1), int(m.group(2)), re.findall(r'\bSTA?\("([^"]*)"', m.group(3))))
    return out
def run(binary, i):
    r = R()
    r.ev("held 0x180", 100); r.ev("held 0", 100); r.wait(1000)          # GLO + HOME: the menu, on its first line
    r.wait(300); r.tap('OCTU'); r.wait(600)                                      # the first line opens the list
    for _ in range(i): r.enc(SEL, 1)
    r.wait(300); r.tap('OCTU'); r.wait(1500)
    ACTION_BY_NAME[NAME_OF[i]](r)
    r.wait(2500)
    d = tempfile.mkdtemp()
    open(d + "/s.txt", "w").write("\n".join(r.L) + f"\n{r.t + 500} end\n")
    out = subprocess.run([binary, d + "/o", d + "/s.txt"], env=dict(os.environ, HOST_FLASH=d + "/f.bin"), capture_output=True, text=True, timeout=600).stdout
    for m in re.finditer(r"LESSON (\d+) STEP (\d+) INSTANT", out):
        if int(m.group(1)) == i: print(f"   INSTANT: lesson {i + 1} step {m.group(2)} is already true when it shows")
    return sorted(int(m.group(2)) for m in re.finditer(r"LESSON (\d+) STEP (\d+) OK", out) if int(m.group(1)) == i)
NAME_OF = []
def main():
    a = sys.argv[1:]; binary = a[0]; nums = [int(x) - 1 for x in a[1:] if x.isdigit()]
    ls = steps_of(); NAME_OF[:] = [n for n, _, _ in ls]
    missing = [n for n in NAME_OF if n not in ACTION_BY_NAME]; assert not missing, missing
    bad = 0
    for i, (name, n, texts) in enumerate(ls):
        long = [t for t in texts if len(t) > 27]
        if nums and i not in nums: continue
        got = run(binary, i); ok = got == list(range(n)) and not long; bad += not ok
        print(f"{i + 1:2} {name:18} {len(got)}/{n} steps" + ("" if got == list(range(n)) else f"   MISSING {sorted(set(range(n)) - set(got))}") + (f"   LONG {long}" if long else ""))
    print("all lessons finish" if not bad else f"{bad} lessons do not finish"); sys.exit(1 if bad else 0)
if __name__ == "__main__": main()
