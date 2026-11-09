#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Everything that runs on a computer: the voice test, every host script (under the address and undefined-behaviour sanitizers),
# the monkey. The browser test (node tests/web_play.mjs) needs web/build.sh first. Prints FAIL lines; exit 1 if any.
cd "$(dirname "$0")/.." || exit 1
mkdir -p build/host
fail=0
cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/voice_test tests/voice_test.c -lm || exit 1
build/voice_test > build/voice_test.log || { echo "FAIL voice_test"; fail=1; }
tail -3 build/voice_test.log
cc -O2 -DSMILEY_HOST -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/chain_test tests/chain_test.c -lm && build/chain_test || { echo "FAIL chain_test"; fail=1; }
cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -DSMILEY_HOST -DPOOL= '-D__attribute__(x)=' -DAPP_VER='"dev"' \
   -Ifirmware/src -Ifirmware/gen -o build/host/smileyface_san firmware/src/smileyface.c -lm 2>/dev/null || { echo "FAIL build"; exit 1; }
for t in steps views switches switches2 wrap fly walk menu silly weird picker pat chain len clock tap manual fx fx2 fx5 wonder monkey; do
    rm -f build/host/t_$t.bin
    HOST_FLASH=build/host/t_$t.bin build/host/smileyface_san build/host/r_$t tests/$t.txt > build/host/r_$t.log 2>&1 || { echo "FAIL $t"; tail -3 build/host/r_$t.log; fail=1; continue; }
    grep -q "runtime error\|AddressSanitizer" build/host/r_$t.log && { echo "FAIL $t (sanitizer)"; fail=1; } || echo "ok   $t"
done
for v in quick:-DSMILEY_QS tips:-DTIP_IDLE=1500u:-DTIP_EVERY=1500u; do                          # the quick start (a new device) and the idle tips, each in a build of its own
    t=${v%%:*}; fl=$(echo "${v#*:}" | tr ':' ' ')
    cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -DSMILEY_HOST $fl -DPOOL= '-D__attribute__(x)=' -DAPP_VER='"dev"' -Ifirmware/src -Ifirmware/gen -o build/host/smileyface_$t firmware/src/smileyface.c -lm 2>/dev/null || { echo "FAIL build $t"; fail=1; continue; }
    rm -f build/host/t_$t.bin
    HOST_FLASH=build/host/t_$t.bin build/host/smileyface_$t build/host/r_$t tests/$t.txt > build/host/r_$t.log 2>&1 || { echo "FAIL $t"; fail=1; continue; }
    grep -q "runtime error\|AddressSanitizer" build/host/r_$t.log && { echo "FAIL $t (sanitizer)"; fail=1; } || echo "ok   $t"
done
rm -f build/host/t_trash.bin                                                                   # the demo deleted, a restart (it stays gone), Restore Demos, a restart (it is back): three runs on one flash
for n in 1 2 3; do
    HOST_FLASH=build/host/t_trash.bin build/host/smileyface_san build/host/r_trash$n tests/trash$n.txt > build/host/r_trash$n.log 2>&1 || { echo "FAIL trash$n"; fail=1; continue; }
    grep -q "runtime error\|AddressSanitizer" build/host/r_trash$n.log && { echo "FAIL trash$n (sanitizer)"; fail=1; } || echo "ok   trash$n"
done
rm -rf build/host/r_glitch; mkdir -p build/host/r_glitch; rm -f build/host/t_glitch.bin
HOST_FLASH=build/host/t_glitch.bin build/host/smileyface_san build/host/r_glitch/s tests/glitch.txt > build/host/r_glitch.log 2>&1 || { echo "FAIL glitch"; fail=1; }
python3 tests/glitch_check.py build/host/r_glitch/s-600.ppm build/host/r_glitch/s-2000.ppm || fail=1
exit $fail
