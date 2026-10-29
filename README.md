# Smileyface

An acid bass line for the M-VAVE FM-1. One 303-style voice, 16 steps, knobs to twist.

- **Steps**: press a key to write a note, `ENV` for an accent, `SEL` for a slide, `EDIT` for a rest. 16 patterns a project.
- **Play**: the white keys cue patterns and chain them. The switches (wave, sub osc, muffler, sweep, hi-res, poles, accent on or off) are the last knob pages, drawn as switches.
- **Knobs**: six pages: cutoff, resonance, env mod, decay; accent, slide, drive, wave; accent decay, tracking, soft attack, muffler; tune, sub, sweep, poles; filter FM, hi resonance, swing, length.
- **Wrap**: hold `SEL` and turn `SELECT` to wrap the pattern round.
- **Fly** and **Walk**: shoot to write. Steer a ship up and down the scale, or step between the lanes of a corridor. Every key shoots: a shot is a note, no shot is a rest, a hit is an accent.

<p><img src="site/img/steps.png" width="19%"> <img src="site/img/fly.png" width="19%"> <img src="site/img/walk.png" width="19%"> <img src="site/img/play.png" width="19%"> <img src="site/img/projects.png" width="19%"></p>

- **Patterns**: hold `HOME` for 12 slots. `LFO` is a random pattern (it might rule, it might not), `ARP` nudges a few steps, `REC` undoes.
- Press `GLO` for the manual, on the FM-1, at the page for where you are. Online: the [guide](site/guide/index.html) and the cheat sheet (`python3 tools/make_cheatsheet.py`).
- `SEQ` taps the tempo. MIDI clock, start and stop are followed.

Forked from [Chipbox](https://github.com/pbarilla/chipbox), which is built on Felucca, SLOOP and Dinghy (GPL-3.0-only). See `LICENSING.md`.

## Build

`./build.sh` (Docker running; the JieLi toolchain in `~/.jieli/toolchain`, the SDK in `~/fw-AC79_AIoT_SDK`).
`python3 tools/fm1_install.py build/smileyface.fwsc --yes` flashes the FM-1.

On a computer: `cc -O2 -DSMILEY_HOST -DPOOL= '-D__attribute__(x)=' -DAPP_VER='"dev"' -Ifirmware/src -Ifirmware/gen -o build/host/smileyface firmware/src/smileyface.c -lm`,
then `build/host/smileyface build/host/s tests/steps.txt` (screens as PPM, the sound as WAV).
