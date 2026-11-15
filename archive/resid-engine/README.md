# The reSID engine (archived 2026-11-15)

Taken out of the Palm app, where cRSID took its place (it sounds almost the same on the TX), and
kept here in case it is wanted again (it costs about half as much as cRSID). Not built.

What it is: reSID 0.16 (from VICE 1.16), tuned to sound like reSIDfp, making the sound for
libsidplay 1, which still runs the tune. On the Palm TX it cost 15% of the processor (Elvira) against
libsidplay's 10%, and 27% for cRSID.

- `resid/` - reSID 0.16 with sid-player's changes: reSIDfp's measured cut-off curves (`curves.h`, made
  by `gen_resid_curves.py` from `tools/filter_measurements`), 2-cycle filter steps in fast mode (cut-off
  up to 16 kHz), stronger resonance (Q = 0.707 + 1.5 * res / 15), the combined-waveform tables as bytes
  (128 KB smaller), reSID 0.16-p2's `mute()`, and `resid_envelope.cc` (envelope.cc under another name:
  libsidplay 1 has an envelope.cpp too). The tuning's research: `research/resid` (not in git).
- `residengine.cpp/.h` - the engine.

To put it back (see git history for the exact lines, if it has been committed; otherwise
`.tmp/pre-cleanup-2026-11-15.tar` holds the tree from before it was taken out):

1. **Every SID write, in order.** libsidplay 1 still has the hooks: `sidWriteHook` in
   `src/core/6510_.cpp` (called with each SID register write) and `sidEmuFillOverride` in
   `src/core/6581_.cpp` (makes the samples instead of libsidplay's voices). `residEnable(true)` sets
   both. Snapshots of the registers per player call are not enough: Elvira's drum restarts its
   envelope (gate off/on) within one call. Writes go to reSID 4 cycles apart.
2. **Song start:** `residEnable(on)` before `sidEmuInitializeSong` (so reSID gets the init routine's
   writes), then `residSongStart(mos8580, filter, bright, sampleRate)`.
3. **Old PlaySID-sample rips** (stores to $D41D in the data): reSID can't play those samples, so play
   them with libsidplay (`usesPlaySIDSamples` in `src/blob/sidcore.cpp`).
4. Builds: the reSID sources and `residengine.cpp` in `tools/blob_build.sh` (with `-Isrc/resid`) and,
   with `/DSIDPLAYER_RESID`, in `tools/pc_build.bat` (`sid2wav -resid` for comparisons).

Tests then: ARM core against the PC renderer, 128/128 identical (all user tunes, both chips, both
curves, 44.1 and 22.05 kHz); the settings test with both engines.

