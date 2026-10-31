# Releasing

Tags are `vX.Y.Z` or `vX.Y.Z-suffix`, one digit each (`v1.0.0`). The tag's text after the `v` is the package's version.

1. Check: `./tests/run_all.sh`, `./web/build.sh && node tests/web_play.mjs`, `./build.sh --release 1.0.0`, `node tests/web_installer.mjs build/smileyface-1.0.0.fwsc`, then flash it and play.
2. Commit, push `main`.
3. `git tag v1.0.0 && git push origin v1.0.0`. The `release` workflow builds, attests and attaches the `.fwsc`.
4. The `pages` workflow then runs: it puts the release's firmware on the site (the installer fetches it), builds the browser version and the cheat sheet, deploys. In the repo's Settings > Pages the source must be **GitHub Actions**.
5. The Firmware Hub reads `fmhub.json` from `main` and looks for new releases every few hours. The identity `FM-1_303` must be unused (thefirmwarehub.com/kb/#ids).
