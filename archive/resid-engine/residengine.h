// residengine: reSID 0.16 (src/resid, with sid-player's filter changes) making the sound for
// libsidplay 1, which still runs the tune (6510, timing, song selection).  GPL v2 or later.
//
// libsidplay calls the tune's player routine once per frame; every SID register write the 6510
// makes is queued (sidWriteHook) and handed to reSID in order, 4 cycles apart, before the next
// samples are made (sidEmuFillOverride).  Register snapshots per call would lose notes that are
// retriggered inside one call (a gate off/on), so the writes are passed on one by one.
//
// Use (with the stream stopped):
//   residEnable(true)                 before sidEmuInitializeSong, every song start
//   residSongStart(...)               right after sidEmuInitializeSong
//   residEnable(false)                to go back to libsidplay's own SID emulation
#ifndef SIDPLAYER_RESIDENGINE_H
#define SIDPLAYER_RESIDENGINE_H

// Install (or remove) the hooks, create reSID on first use, reset it and empty the write queue.
// False if reSID could not be created (out of memory).
bool residEnable(bool on);

// Set reSID up for the song just initialised: the chip, filter on/off, the cut-off curve
// (bright = reSIDfp's measured curve, else reSID 0.16's own), the sample rate and the C64 clock
// (libsidplay's, for the song).  The init routine's writes are already queued.
void residSongStart(bool mos8580, bool filter, bool bright, unsigned long sampleRate);

bool residActive();

#endif
