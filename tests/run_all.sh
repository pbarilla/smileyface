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
cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -DSMILEY_HOST -DPOOL= '-D__attribute__(x)=' -DAPP_VER='"dev"' \
   -DQUICK_DEMO -Ifirmware/src -Ifirmware/gen -o build/host/smileyface_san firmware/src/smileyface.c -lm 2>/dev/null || { echo "FAIL build"; exit 1; }
for t in steps views switches wrap fly walk picker len clock tap manual monkey; do
    rm -f build/host/t_$t.bin
    HOST_FLASH=build/host/t_$t.bin build/host/smileyface_san build/host/r_$t tests/$t.txt > build/host/r_$t.log 2>&1 || { echo "FAIL $t"; tail -3 build/host/r_$t.log; fail=1; continue; }
    grep -q "runtime error\|AddressSanitizer" build/host/r_$t.log && { echo "FAIL $t (sanitizer)"; fail=1; } || echo "ok   $t"
done
exit $fail
