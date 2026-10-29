#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run firmware/src/smileyface.c on this computer (tests/host.c): build/host/smileyface.wav and the screens as PNG.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
cc -O2 -Wall -Wno-unused-function -DSMILEY_HOST $HOST_CFLAGS -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/src -Ifirmware/gen -o build/host/smileyface firmware/src/smileyface.c
build/host/smileyface build/host/smileyface ${1:+"$1"} ${2:+"$2"}
python3 - <<'PY'
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob("smileyface-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
