# SID Player for Palm OS 5 (design notes)

**Status 2026-11-13: VERSION 1.0 RELEASED**, https://github.com/cyberpunkgothic/palm-sid-player
(public; release v1.0 has both .prc files).  Tested on the real Palm TX (tests/TX_CHECKLIST.md: all
passed; Speed Test under 20% with the filter on, under 10% off), the TX simulator, and CloudpilotEmu
(Tungsten E2, E3v2, deNVFS E2, rePalm; an OS 4 m515 refuses politely).  The device fixes on the way
are under "Device compatibility" below.  The repository leaves out the user's tunes (tests/tunes/user,
including the extra local render cases in local_cases.json) and anything about the Casio (CASIO.md,
excluded through .git/info/exclude).  The milestone notes below are the history.

Status: milestone 1 built 2026-11-07: tools\pc_build.bat (MSVC, 32-bit) builds the unchanged
libsidplay 1.36.60 + src\pc\sid2wav.cpp -> build\pc\sid2wav.exe; test WAVs in build\wav
(44.1 kHz 16-bit mono, 8580 waveforms, filter on, tune's clock: the user's SIDPLAY/w settings).
Renders are repeatable byte for byte.  User compared with SIDPLAY/w (tunes from the user's collection,
including Elvira - The Arcade Game track 1): "sounds identical".  MILESTONE 1 DONE.

Milestone 3 (Palm app) BUILT 2026-11-07, works in the TX simulator (real TX: see the status above):
- TWO APPS, kept separately (decided 2026-11-08), for people who don't want the skin or whose device
  can't show it (it needs 16-bit colour and the 320 px screen):
  - src/palm_std: "SID Player", creator 'SdPl', the standard Palm OS interface (list, buttons, info
    text) -> build/palm/SIDPlayer.prc.  Started from the first standard-UI build (2026-11-07).
  - src/palm_c64: "SID Player DX", creator 'SdPX', the C64-style skin -> build/palm/SIDPlayerDX.prc.
  Each has main.c (68k app), sidplayer.rcp, resource.h, Makefile (WSL, Dmitry's GCC 9.1 + PilRC 3.3),
  linker.lkr; intermediates in build/palm_std and build/palm_c64.  Both use the same sound core
  (build/palm/core.rcp).  tools/palm_build.sh builds everything.  The code below the UI (core loading,
  files, playback, the simulator ring) is the same in both main.c files: fix it in both.  Fixes and
  features that aren't about the skin go into the standard app too.
  - Ported to the standard app 2026-11-08: Pause label (pausing keeps the place); Play with no tune
    loaded plays the selected one (or the first); page keys move a page, the 5-way up/down one line;
    the list shows names without ".sid" and with '_' as spaces (truncated with an ellipsis).
    Not ported (skin only): level meters, block counts, the C64 screen.
  - Checked by the user in the TX simulator (2026-11-08): both apps install side by side, launch and
    play.
  - Settings (2026-11-08, both apps, Options > Settings...): SID chip 6581/8580 (default 8580), filter
    on/off (on), clock Auto/PAL/NTSC (Auto = the tune's own, PAL if it doesn't say).  Saved with
    PrefSetAppPreferences (each app keeps its own).  OK restarts the current song: libsidplay's setConfig
    resets the SID clock to the config's default when the chip or filter changes (an NTSC tune would
    change speed mid-song).  tests/settings_check.c (built by tools/settings_check.bat): changing settings with a
    tune loaded then restarting is bit-identical to a fresh start, all 12 combinations.  Note: in
    libsidplay 1 the chip setting only picks the combined-waveform tables (the filter is the same), so
    most tunes sound identical either way: of the 16 test tunes only Chevy Chase differs.  Checked by the user
    in the simulator, 2026-11-08: works in both apps.
  run_sim.bat starts the TX simulator; test tunes in <simulator>\Card1\SID.
- Both apps: list .sid files in /SID on the card, play the tapped one, < > change song, Play/Pause,
  shows title/author/copyright/song/time; 5-way keys; tall TX layout via the PINS/DIA API (untested on
  a device).  The ARM core comes from 'armc' resources; the 68k side copies it into a MemChunkNew block
  (flag 0x1200 = non-movable + allow >64 KB: confirmed to give the full size) and fixes its addresses.
  On the TX the core feeds the Sound Manager stream directly (native callback, 44.1 kHz).
- Simulator: the core is build\pc\sidcore.dll; PceNativeCall("sidcore.dll\0sidcoreEntry").  The
  simulator unloads the DLL after each call (the core pins itself) and rejects native stream callbacks,
  and its 68k sound thread cannot make native calls, so there the main thread fills a 1 s ring buffer and
  a 68k callback copies from it.  Plays without lag.
- Bugs found on the way (all would have hit the TX): libgcc's 32-bit divide/multiply call each other at
  absolute addresses (main.c has its own); libc's memset was built with 32-bit int but the app uses
  -mshort, so GCC's hidden memset calls wiped the stack.  The Makefile now fails if any absolute jump or
  any library code is linked in.  (-fcall-used-d2 kept as a precaution: traps may change d2.)
- Debug aids in main.c: DEBUG_LOG (writes /sidlog.txt on the card), SIM_SOUND (0 = silent simulator).
  The simulator's own error log: <simulator>\ErrorLog.txt.  MemHeapCheck crashes this simulator.
- Checked by the user in the simulator (2026-11-07): playback, < >, Play/Stop, landscape, About, leaving
  to the launcher (the early "Invalid chunk ptr" on exit is gone: it was the memset bug).  Song info is
  centred between the list and the buttons.
- Icon: tools/gen_icon.py (C= logo in the Palm OS 5 launcher style: skewed + extruded, black outline,
  flat blue base with dithered shadow).  Large icon is 32x22 / 64x44 (measured from the TX simulator's
  stock icons: 56 x 44 px artwork, 56 x 22 px base); small icons 15x9 / 30x18, plain logo.
- C64-style UI, SID Player DX (2026-11-08, from the "hybrid panels" mock-up drawn by tools/mock_c64.py): the app draws the whole screen at native resolution in C64 colours
  with its own 8x8 font (src/palm_c64/font8.h from tools/gen_font.py + tools/mock_c64.py), through a 16-bit
  320x30 strip bitmap.  Level meters: SC_LEVELS gives each voice's peak output (mixer) and note starts
  (6581_.cpp: gate on, or a legato pitch change held 2 frames); a new note kicks the bar to full, which
  falls back towards the voice's loudness.  tests/levels_probe.c prints the meter data for a tune.
- Device readiness (2026-11-08, both apps): auto-off held off while playing (EvtResetAutoOffTimer
  every 5 s from every event loop; About/Settings wake once a second for it while playing); pause on
  sleep (sysNotifySleepNotifyEvent callback, registered while the app runs; the screen is refreshed
  through frmUpdateEvent); Options > Speed Test (renders 10 s of the current song with SC_FILL and
  reports the share of real time; the song restarts afterwards); after loading the core, 64 KB of
  other memory is read to push the relocated code out of the XScale's write-back data cache (Palm OS
  has no public cache call; the instruction cache can't be cleared from an app, but could only hold
  stale lines if other code ran at those addresses recently).  Test plan: tests/TX_CHECKLIST.md.
  Checked by the user in the simulator (2026-11-08): both apps fine; Speed Test 0.00-0.03 s per 10 s
  there (the PC's speed: the PC build makes 100 s in ~0.17 s; the Palm timer counts in 0.01 s).
- ON HOLD (2026-11-09) until the TX arrives: next step is tests/TX_CHECKLIST.md on the device.
- To check on the TX: sound (native callback, soft-float speed), the tall layout on the device, and which
  keys the 5-way sends (both apps page on vchrPageUp/Down and step on vchrRockerUp/Down).

Milestone 2 DONE (2026-11-07):
- Our copy of libsidplay is src\core (vendor\ stays untouched).  tools\pc_build.bat builds
  sid2wav_ref (vendor), sid2wav (src\core) and sid2wav_fixed (src\core, fixed-point filter).
- tests\test_render.py: 19 cases (tunes from the user's collection in tests\tunes\user, not in the
  repository; 60 s each) checked byte for byte against tests\golden.json (made from sid2wav_ref); --compare A B measures two builds.
- Step 1, fixed-point filter: libsidplay's old `fixed` class no longer compiles, so the filter has a
  plain 16.16 integer version (6581_.cpp waveCalcFilter, tables in eeconfig.cpp, FILTER_BITS in
  mytypes.h).  Against the float reference: 9 of 19 identical, the rest differ by single filter-output
  steps, 45-86 dB down.  For scale: the same unchanged code built with x87 floats (as 2002 compilers
  did) differs from the reference by 36-52 dB on every tune.  20 bits barely helps; 24 overflows.
  User (blind test): slight harshness on sharp sounds with the fixed filter.  Cause found with a
  side-by-side check (FILTER_CHECK build): the filter settles towards whole numbers and libsidplay
  truncates, so tiny rounding differences flip output steps (12% of low-pass samples in one test tune).
  More bits cannot fix that.
- DECISION 2026-11-07: keep the original float filter on the Palm, using ARM software floating
  point (GCC follows IEEE single precision like the PC).  The fixed filter stays in the code
  (build with SID_FIXED_FILTER) as a fallback if soft-float turns out too slow on the TX.
- Bit-exact ARM: tools/arm_check.sh (WSL: arm-linux-gnueabi-g++ ARMv5TE soft-float + qemu-arm);
  `test_render.py --exe sid2wav_core_arm` -> 19/19 identical to the PC reference.  Three fixes needed:
  1. filtertables.h: the low-pass table from the PC reference (exp()/log() differ between C libraries;
     tools/gen_filter_tables.py, 44.1 and 22.05 kHz, default filter only).
  2. mixing.cpp fill16bitMono: voices called in a fixed order (sample, 3, 2, 1, as MSVC does); C++
     leaves the order open and ring modulation/sync make it matter (Elvira 1, Freestyler).  The other
     mixers (8-bit, stereo) still have the open order: fix them if they are ever used.
  3. eeconfig.cpp: negative float -> uword via a signed int (undefined in C; MSVC wraps).
  Also: plain char is unsigned on ARM; build with -fsigned-char to be safe.
- Step 2 DONE 2026-11-07, by a different route than planned: instead of rewriting libsidplay's globals
  into one context block, the core is built as ONE RELOCATABLE BLOCK (the PEAL idea) and the host fixes
  up its addresses when loading it.  libsidplay barely changes, so the sound stays bit-exact.
  - src/blob: sidcore_api.h (the interface: SC_INIT/CONFIG/LOAD/START/FILL/SHUTDOWN in a SidCoreCall
    block, PceNativeCall-style entry, file layout), sidcore.cpp (entry, own arena allocator, runtime
    bits), sidcore.ld, config/ (no exceptions, SID_NO_FILES).
  - tools/blob_build.sh (Git Bash; xPack GCC 15, ARMv4T soft-float (was ARMv5TE until 2026-11-12, see
    below), linked at 0 with --emit-relocs)
    -> tools/blob_pack.py -> build/blob/sidcore.bin: 163 KB image + 48 KB bss, ~2,070 addresses to fix.
    The packer rejects any relocation type it does not understand and any linker veneer (newlib's
    strlen is Thumb: the core has its own).
  - SID_NO_FILES in src/core: load from memory only; file loading/saving, the SID+INFO two-file formats
    and PowerPacker unpacking are left out of the core (the PC build keeps them).
  - Memory: the host gives SC_INIT one arena; peak use 397-440 KB on the test tunes; SC_INIT refuses
    under 512 KB (libsidplay doesn't check its start-up allocations); 640 KB is ample.
  - Test: tests/blob_harness.c loads it exactly as the Palm will, under qemu-arm (tools/blob_check.sh);
    `test_render.py --exe sidcore` -> 19/19 identical to the PC reference; 22.05 kHz identical too.
    tools/blob_debug.sh runs it under gdb-multiarch for crashes.
  - For the Palm app (milestone 3): byte-swap SidCoreCall fields on the 68k side (the core reads them
    little-endian); the 68k side does the relocation (also swapping); check whether the XScale needs a
    cache flush after copying code; decide where the ~210 KB block + 640 KB arena live (dynamic heap or
    feature memory); speed of soft-float unknown until the TX (fixed filter is the fallback).
    [Answered 2026-11-13 on the real TX: under 20% of the processor, float filter on; see below.]
  - Device compatibility (2026-11-12, after the apps went out on the PalmDB Discord: fine on a real TX,
    "Fatal Exception" at start on a Tungsten E2).  The TX tolerated all of these; other devices don't:
    1. Alignment of what the 68k side hands the core.  The SidCoreCall block was a local in coreCall,
       and the 68k compiler only aligns the stack to 2 bytes (-mno-align-int), so it was 4-byte
       aligned or not depending on the caller; the core's first word load from it faulted on the E2.
       coreCall now aligns the block itself (both apps).  Rule: anything the core reads with word
       loads must be 4-byte aligned on the 68k side.
    2. Stack size.  The stack Palm OS gives native code is too small for SC_INIT on the E2.  Both ways
       into the core (sidcoreEntry, and sidcoreStream on the Sound Manager's thread, which can run at
       the same time) switch to 16 KB stacks of the core's own (sidcore.cpp, CORE_STACK_SWITCH; 32 KB
       more bss).
    3. Alignment of the core itself.  It is linked for an 8-byte aligned base, but MemChunkNew only
       promises 4: a block 4 bytes off misaligns the core's stacks and 64-bit data (rePalm: "FAULT:
       DABT" on playback, a strd).  loadArmCore allocates 8 more and rounds the base up (coreChunk is
       what gets freed).  (MemPtrNew, suggested on the Discord, can't allocate over ~64 KB on OS 5
       and promises no 8-byte alignment either.)
    4. Instruction set (from the Discord).  The core was built for ARMv5TE, but some Palm OS 5
       handhelds have ARMv4T processors (TI OMAP: Tungsten T/T2/E, Zire 21/71, Treo 600; Samsung
       S3C2410: Z22), where ARMv5 instructions are undefined.  It had 202 blx, 189 clz, 3 smulbb and
       2 strd + 1 ldrd.  Now -march=armv4t, still -mtune=xscale (ARMv4T instructions, scheduled for
       the XScale); the libraries were already the ARMv4T ones; tools/blob_build.sh fails if any
       ARMv5-only instruction appears; the stack switch returns with bx lr (a pop into pc can't return
       to Thumb on ARMv4T).  19/19 tunes still identical.  Speed on the real TX (2026-11-13, user's
       Speed Test, the ARMv4T build with the float filter): under 20% of the processor on every tune
       tried, so the fixed-filter / 22.05 kHz fallbacks aren't needed.  Disassemble with objdump -m armv5te, or v5 instructions show as UNDEFINED.
    5. Card volumes.  The apps used the first VFS volume only; they now use the first one with a
       /SID folder (the deNVFS E2 ROM has a built-in card in the first slot).
    6. Card changes while the app is open (real TX, 2026-11-13).  Both apps register for
       sysNotifyVolumeMountedEvent/UnmountedEvent with the sleep notification (watchSystem, undone on
       exit).  On a mount they set vfsHandledUIAppSwitch in the notification's handled field, so the
       system doesn't switch to the launcher; either event sets cardChanged, and the event loop sends
       the main form a CARD_UPDATE_CODE frmUpdateEvent that reads the list again (rescanCard).  The
       playing tune carries on (it's in the core) and is found again by name (loadedName), or marked
       LOADED_GONE.  The 5-way's up/down (page up/down with navigator bits in keyCode) now move one
       line; real page buttons still page.
    How it was tested: CloudpilotEmu (web; the only Palm OS 5 devices it emulates are the Tungsten E2
    family and rePalm), with ROMs from PalmDB's "Palm OS Device ROMs" (palm-roms-complete): E2 NOR +
    NAND, CloudPilot-Tungsten-E3v2 (320x480), CloudPilot-E2-denvfs, repalm-paravirtualized (logs data
    aborts with registers and the bytes at the PC to the browser console: the quickest way to a fault
    address), and an m515 (OS 4.1), where both apps refuse politely.  Passed on all of them and in the
    TX simulator.  On the E2 there is no fault report: a diagnostic build with the dlog lines drawn at
    the top of the screen (make EXTRA=-DDEBUG_SCREEN=1; they stay visible above a Fatal Alert) and
    variant builds bisected it.  Ruled out on the way: r9 use (-ffixed-r9 changed nothing),
    interrupts/timing, the instruction cache, code being overwritten, heap damage.  An app cannot use
    CP15 cache instructions on the E2 (they crash it).
Config headers for the PC: src\pc\config (generated from libsidplay's .in templates; SID_FPUFILTER on,
DIRECT_FIXPOINT off as configure's default).

## Findings from preparation (2026-11-07; details in vendor/VENDOR.md)
- All four versions fetched and checked (.57 from Fedora's archive, the rest from Debian's).
- The emulation code is identical from 1.36.57 to .60: use **1.36.60**.
- Licence: **GPL v2 or later**.
- libsidplay 1 already has a fixed-point filter (build without `SID_FPUFILTER`), so the
  "no FPU" work is mostly done; floats are left only in table set-up.
- Default filter: Fs 400, Fm 60, Ft 0.05.  No oversampling in libsidplay 1 (a SIDPLAY/w feature).
- Test tunes: tests/tunes/user (the user's own, kept out of the repository).
- Palm tools (2026-11-07): in %LOCALAPPDATA%\Programs\PalmDev (README.md there).  ARM core: GCC 15.2
  arm-none-eabi, native Windows, works for XScale.  68k app: Dmitry Grinberg's GCC 9.1 + PilRC 3.3
  (palm2000 dev package v3) in WSL Ubuntu 24.04: hello world built and ran in the TX simulator.  SDKs 1.0-5r4 + PalmOne SDK unpacked.

## Goal
Play C64 SID tunes (PSID files, as in the HVSC collection) on:
- **Palm TX**: Palm OS 5.4.9 (with the Enterprise Security Update), 312 MHz XScale ARM, 320x480.

## The sound: SIDPLAY/w "original" mode, not reSID
The user has listened with SIDPLAY/w (the last original version, 7 Nov 2002) for twenty years.
He prefers its original emulation (libsidplay 1.x, Michael Schwendt) to reSID, and later
SIDPLAY versions can't switch reSID off.  So the target is that sound:
- waveforms computed per output sample from idealised shapes (not cycle by cycle);
- a clean digital filter with an adjustable curve, as in SIDPLAY/w's settings;
- few of the real chip's analogue flaws: bright and crisp.
It is also much cheaper than reSID, which suits a handheld.

## Plan
- **One portable C core:** a 6502 that runs the tune's init and play routines, a SID emulation,
  a PSID loader (subtunes; song lengths later), and a fixed-point mixer (the TX's ARM has no FPU).
- **A PC build of the core** that writes WAV files.  The user compares them with SIDPLAY/w on
  tunes he knows; the filter will need the most tuning by ear.
- **Palm front end:** a 68k app with the core as native ARM code, sound through the Sound Manager's streaming calls, files from the SD card
  (VFS Manager), and a song list.  The desktop Palm OS 5 simulator can't run ARM code, so the sound
  is checked on the real TX.
- **Later:** RSID tunes (these need CIA timers and raster interrupts), and a filter-curve setting.

## Licence: GPL (decided 2026-11-07)
GPL, so the player can use libsidplay 1's own engine and sound like SIDPLAY/w's original mode,
not an imitation.  Match libsidplay 1.36's GPL version and keep its notices.

## Test tunes
Tunes from the user's own collection, in tests/tunes/user: they aren't ours to publish, so they
stay out of the repository.  tests/test_render.py skips any that are missing, and reads extra local
cases from tests/tunes/user/local_cases.json if it's there.
