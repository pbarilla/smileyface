#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The bank loader on damaged flash: take a flash image (a bank that has chains), flip random bytes in the bank and its chain records, and start the app
(under the sanitizers) on every one: it must come up, open projects, play the PLAY view's chain and leave no sanitizer message.
   python3 tests/bank_fuzz.py FLASH.bin [runs]   (needs build/host/smileyface_san from tests/run_all.sh)"""
import os, random, struct, subprocess, sys, tempfile
src, runs = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 60
data = bytearray(open(src, "rb").read())
here = os.path.dirname(os.path.abspath(__file__))
exe = os.path.join(here, "..", "build", "host", "smileyface_san")
banks = [i for i in range(len(data) - 16) if data[i:i + 4] == b"3KFS" and struct.unpack_from("<I", data, i + 12)[0] == 12]
script = "\n".join(["100 midi 0xB0 106 5", "300 held 0x100", "360 held 0", "500 held 0x100", "560 held 0", "700 held 0x100", "760 held 0",
                    "900 btn 12", "2500 btn 12", "2700 midi 0xB0 106 3", "3000 held 0x100", "3060 held 0", "3300 end"]) + "\n"
tmp = tempfile.mkdtemp()
open(os.path.join(tmp, "s.txt"), "w").write(script)
random.seed(7)
bad = 0
for n in range(runs):
    d = bytearray(data)
    for _ in range(random.choice([1, 4, 30, 200])):
        b = random.choice(banks)
        lo = b + 16 + 12 * 604 if random.random() < 0.6 else b                      # mostly the chain records
        hi = b + 16 + 12 * 604 + 216 if lo != b else b + 16 + 12 * 604 + 216
        d[random.randrange(lo, hi)] = random.randrange(256)
    open(os.path.join(tmp, "f.bin"), "wb").write(d)
    r = subprocess.run([exe, os.path.join(tmp, "o"), os.path.join(tmp, "s.txt")], env=dict(os.environ, HOST_FLASH=os.path.join(tmp, "f.bin")), capture_output=True, text=True, timeout=120)
    out = r.stdout + r.stderr
    if r.returncode != 0 or "runtime error" in out or "AddressSanitizer" in out:
        bad += 1; print("FAIL run", n, r.returncode, out[-300:])
print("bank_fuzz:", runs, "damaged banks,", bad, "failures")
sys.exit(1 if bad else 0)
