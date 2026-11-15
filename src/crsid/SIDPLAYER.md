# cRSID in SID Player

cRSID by Hermit (Mihaly Horvath), 2022, integer-only C64/SID emulator written for Rockbox players.
Licence: WTFPL ("do what you want ... but please mention me as the original author").
Source: the copy benchmarked on 2026-11-15 (README.txt and ChangeLog.md are its own).

Used by `src/core/crsidengine.c`: the cRSID engine (engine 2), which also plays the RSID tunes
libsidplay 1 can't load, whichever engine is chosen.
The `host/` files are kept unchanged; their PC parts (SDL, file loading) are compiled out.

Changes for SID Player, both marked `sid-player` in the source:

- `C64/SID.c`: `combinedWF` was a function nested inside `cRSID_emulateWaves` (a GCC extension).
  Moved out unchanged, with the enclosing function's `SID`, `Channel` and `ChannelPtr` passed in.
- `C64/SID.c`: Settings > Filter > Off (`crsidFilterOff`, defined in crsidengine.c) routes every
  voice past the filter.

Not done: `C64/CPU.c` is also built from nested functions, so MSVC can't compile cRSID. The Palm
OS Simulator's DLL uses `src/core/crsidstub.c` instead (libsidplay plays when cRSID is chosen).

Settled in crsidengine.c rather than here: the tune's header is rewritten as a v2 header carrying
the chip and clock chosen in Settings (cRSID reads the v2 flags even from v1 headers), and an init
address of 0 becomes the load address (cRSID doesn't handle that case).
