# SID Player for Palm OS 5

A C64 SID music player for Palm OS 5 handhelds with an ARM processor (made on and for the Palm TX),
with SIDPLAY/w's "original" sound (libsidplay 1). Two versions: **SID Player** (the standard Palm OS
interface) and **SID Player DX** (a C64-style screen). GPL v2 or later (see `COPYING`), same as
libsidplay 1.

**Version 1.0** (2026-11-13). The apps are `SIDPlayer.prc` and `SIDPlayerDX.prc` (also attached to the
GitHub release).

## Using it

The full user manual is `MANUAL.md` (and the same as plain text for Notepad, `MANUAL.txt`; change both together). In short:

- **Needs:** a Palm OS 5 handheld with an ARM processor. Tested on a Palm TX, and in CloudpilotEmu as
  a Tungsten E2, the widescreen "E3" and rePalm; built to run on the older ARMv4T models too (Tungsten
  T/T2/E, Zire 21/71, Treo 600, Z22), but untested there. SID Player DX also
  needs a 16-bit colour screen 320 pixels wide; SID Player works on any of them. On a handheld that
  isn't Palm-branded, you will most likely need to install **MCA2** (Modern Clie Audio 2, made for
  Sony CLIÉ handhelds running Palm OS 5; <https://palmdb.net/app/mca2>) as well for the apps to work.
- **Install** `SIDPlayer.prc` and/or `SIDPlayerDX.prc` (in the project folder; they are separate apps and can sit side by side).
- **Tunes:** put PSID `.sid` files (as in the High Voltage SID Collection) in a folder called `SID` on
  the memory card.
- **Playing:** tap a tune to play it. Previous/next change the song within a file, and Pause resumes
  where it left off. On the 5-way: up/down move through the list, left/right change the song, centre
  plays.
- **Settings** (menu: Options > Settings): SID chip 6581 or 8580, filter on or off, and the clock (Auto =
  the tune's own, or force PAL or NTSC). OK restarts the current song with the new settings.
- **Speed Test** (Options menu): shows how much of the processor playing takes on your handheld.

Plan and decisions: `DESIGN.md`.

| Path | What |
|---|---|
| `SIDPlayer.prc`, `SIDPlayerDX.prc` | the two apps, ready to install |
| `src/palm_std/` | SID Player: the app with the standard Palm OS interface |
| `src/palm_c64/` | SID Player DX: the same player with the C64-style screen |
| `src/blob/` | the sound core as one relocatable ARM block (and, built from the same source, the simulator's `sidcore.dll`) |
| `src/core/` | our copy of libsidplay 1.36.60 (changes marked "sid-player") |
| `src/pc/` | `sid2wav.cpp` (renders a tune to a WAV file) and the PC config headers |
| `vendor/` | libsidplay 1.36.60's checked tarball and untouched source (the reference the tests compare against); `VENDOR.md` has the notes |
| `tools/` | the build scripts: `palm_build.sh` (both apps and the sound core), `pc_build.bat` (the PC builds, MSVC), and the tools they use |
| `tests/` | `TX_CHECKLIST.md` (the device test plan), `test_render.py` (renders tunes and checks them byte for byte against the unchanged libsidplay), `settings_check.c` (settings changes; `tools/settings_check.bat`), `levels_probe.c` (DX's level meters) |
| `run_sim.bat` | starts the Palm TX simulator with the apps (the simulator is PalmSource's and isn't included) |

Not in the repository, made on your own machine:
- `build/`: everything the build scripts produce (`tools/palm_build.sh` puts the apps in
  `build/palm/` and copies them to the top folder).
- `tests/tunes/user/`: tunes for `tests/test_render.py`, which aren't ours to publish. The test
  names twelve tunes from the High Voltage SID Collection (see `CASES` in the script; their reference
  checksums are in `tests/golden.json`) and skips any you don't have.

`sid2wav in.sid out.wav [-s N] [-t SEC] [-r HZ] [-6581] [-nofilter] [-pal|-ntsc] [-info]`
(defaults: start song, 120 s, 44100 Hz, 8580 waveforms, filter on, the tune's clock).

## How this was made

This project is a collaboration between its author and an AI: Claude (by Anthropic), working
through Claude Code. The AI wrote most of the code, tools and documentation under the author's
direction. The author set the goals and made the design decisions, including the choice of
libsidplay 1's sound. He also judged the sound by ear against SIDPLAY/w and tested every build
in the Palm OS Simulator and on the hardware.  Commits made with the AI's help carry a
`Co-Authored-By: Claude` line.

## Credits

- **libsidplay 1.36.60** by Michael Schwendt: the sound engine, the same one SIDPLAY/w used for its
  "original" emulation (GPL v2 or later). Our changes to it are marked "sid-player" in `src/core`.

### Tools

From the companies behind Palm OS, at the time:
- **PalmSource, Inc.**: the Palm OS 5 SDK (headers and documentation) and the Palm OS Garnet
  Simulator.
- **palmOne, Inc.** (later Palm, Inc.): the palmOne SDK (Palm TX headers and documentation) and the
  Palm TX Simulator. The simulators are used for testing only; they are not part of this project
  and are not redistributed.

From the community, who keep these tools working on modern systems:
- **Dmitry Grinberg**: GCC 9.1 for Palm OS (the m68k compiler for the app) and fixes to PilRC.
- **Wes Cherry** (original author) and **Aaron Ardiri** (later maintainer): PilRC, the Palm resource
  compiler.
- **palm2000.com**: the developer package that collects Dmitry's compiler, PilRC 3.3 and the
  adjusted SDK headers, now the place to get them.
- **Chuan Ji (jichu4n)**: the palm-os-sdk archive of the official SDKs.

Also: the xPack build of the Arm GNU Toolchain (GCC 15, for the ARM sound core), Microsoft Visual
Studio Build Tools (the PC test builds and the simulator's copy of the core), WSL with Ubuntu, QEMU
and GDB (testing the ARM core on the PC), and Python (build and test scripts).

Testing on other handhelds:
- **Christian Speckner**: CloudpilotEmu, the Palm emulator used to test the Tungsten E2 family and
  rePalm.
- **Dmitry Grinberg**: rePalm, and the widescreen "Tungsten E3" ROM.
- **PalmDB** (palmdb.net): its archive of device ROMs, and the people on its Discord who tried the
  first release, reported the Tungsten E2 crash and pointed out the ARMv4T problem.
