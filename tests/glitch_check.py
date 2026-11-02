#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tests/glitch.txt left two screenshots: the knobs and the tabs (the rows from 172 down) must be the same in both."""
import sys
def rows(path):
    d = open(path, "rb").read()
    head, px = d.split(b"\n", 1)                                # "P6 240 240 255" and the pixels
    w, h = map(int, head.split()[1:3])
    return [px[y * w * 3:(y + 1) * w * 3] for y in range(h)]
a, b = rows(sys.argv[1]), rows(sys.argv[2])
bad = [y for y in range(172, 240) if a[y] != b[y]]
if bad:
    print("FAIL glitch stuck on: %d rows of the knobs / tabs differ (first %d)" % (len(bad), bad[0]))
    sys.exit(1)
print("ok   glitch gone from the knobs and tabs")
