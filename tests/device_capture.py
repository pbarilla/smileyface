#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Plays the open pattern over USB-MIDI (CC 102) and records the FM-1's USB audio (the "Felucca" audio device) with ffmpeg, then prints the CPU load the
firmware reports (control change 100). Needs the terminal to have macOS microphone permission (without it the recording is silent).
    python3 tests/device_capture.py OUT.wav [SECONDS]      needs mido, python-rtmidi, ffmpeg; FM-1 running Smileyface"""
import subprocess, sys, time
import mido

out_wav = sys.argv[1]
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0
port = [n for n in mido.get_output_names() if "FM" in n or "X0X" in n or "Felucca" in n][0]
out = mido.open_output(port)
inp = mido.open_input([n for n in mido.get_input_names() if port.split()[0] in n][0])
rec = subprocess.Popen(["ffmpeg", "-y", "-v", "error", "-f", "avfoundation", "-i", ":Felucca", "-t", str(secs + 2), "-ar", "44100", "-ac", "2", out_wav])
time.sleep(1.2)
out.send(mido.Message("control_change", control=102, value=127))
time.sleep(secs)
out.send(mido.Message("control_change", control=102, value=0))
cpu = [m.value for m in inp.iter_pending() if m.type == "control_change" and m.control == 100]
try:
    rec.wait(timeout=secs + 15)
except subprocess.TimeoutExpired:
    rec.kill()
    print("ffmpeg produced no audio in time: microphone permission, or the device is not streaming")
print("CPU % reports:", cpu)
