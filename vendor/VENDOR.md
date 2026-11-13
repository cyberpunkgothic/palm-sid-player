# libsidplay 1 sources

Michael Schwendt's libsidplay 1, the engine behind SIDPLAY/w's "original" mode.
GPL **version 2 or later** (the file headers say "either version 2 of the License, or
(at your option) any later version"; `COPYING` is GPL v2).

| Version | Tarball | From | Check |
|---|---|---|---|
| 1.36.60 | `libsidplay_1.36.60.orig.tar.bz2` | Debian snapshot (bullseye to forky) | SHA-1 in `tarballs/SHA1SUMS` |

Checked 2026-11-07. Unpacked in `src/`, and kept untouched: `tools/pc_build.bat` builds it as the
reference player the tests compare against. Our changed copy is the project's `src/core`.

Versions 1.36.47 (Debian, woody), .57 (Fedora) and .59 (Debian) were also fetched and compared
before choosing .60 (below); they aren't part of the project.

## What changed after 1.36.57 (the user's SIDPLAY/w version)

The emulation files (`6510_`, `6581_`, `envelope`, `mixing`, `samples`, `wave6581.h`,
`wave8580.h`, `fixpoint.h`, `emucfg.h`) are **identical from .57 to .60**, so .60 sounds the same.

- **.59:** `mus_.cpp` (Sidplayer .mus files), `smart.h` (`this->` for newer GCC), build files.
- **.60:** `eeconfig.cpp` sets `isReady` before `reset()`; `sidtune.cpp` initialises
  `isSlashedFileName`; build files.

**Decision: base the player on 1.36.60.**

## Notes for porting

- **Fixed point is already built in.** `mytypes.h`: with `SID_FPUFILTER` defined (the default in
  `libcfg.h.in`) the filter uses `float`; without it, `fixpoint.h`'s 16.16 `fixed` class. The PC
  reference build uses float (as SIDPLAY/w almost certainly did); the handheld builds use fixed (later decision: the Palm uses soft-float, see DESIGN.md).
  Milestone 2 compares the two outputs.
- `DIRECT_FIXPOINT` (configure option) switches the oscillators to another fixed-point form; check it.
- Other floats are only in table set-up (`eeconfig.cpp` filter tables, `envelope.cpp` rates): fine.
- **Default filter:** `SIDEMU_DEFAULTFILTERFS = 400.0`, `FM = 60.0`, `FT = 0.05` (`emucfg.h`).
- **No oversampling** anywhere in libsidplay 1: SIDPLAY/w's "Oversampling 16" setting must be its own
  front-end feature. Leave it out unless the WAVs sound different without it.
- PSID v1 and v2 headers are read (clock and SID model flags too); v3+ is refused.
- State is in `static` globals across the files: needs gathering into one block for Palm ARM code.
- About 8,800 lines of C++ in total.
