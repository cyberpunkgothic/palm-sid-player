# SID Player for Palm OS 5 — User Manual

*Version 1.01*

SID Player plays Commodore 64 music (SID tunes) on a Palm OS 5 handheld. It uses libsidplay 1, the engine behind SIDPLAY/w's "original" sound, so tunes sound the way they did in SIDPLAY/w rather than the way modern reSID-based players render them.

It comes as two separate apps that play exactly the same way:

| App | Looks like | Needs |
|---|---|---|
| **SID Player** | A standard Palm OS app: a list, buttons and song details | Any Palm OS 5 handheld with an ARM processor |
| **SID Player DX** | A Commodore 64 screen: blue panels, the C64 font, level meters | The same, plus a 16-bit colour screen 320 pixels wide |

You can install either one or both: they don't share anything and can sit side by side in the launcher. Each keeps its own settings.

> SID Player was developed on and for the **Palm TX**. It needs Palm OS 5 with an ARM processor: it will not run on Palm OS 4 or earlier handhelds (it says so and closes).
>
> **Tested on:** a Palm TX, the Palm TX simulator, and in the CloudpilotEmu emulator as a Tungsten E2, the widescreen "Tungsten E3", and rePalm. It is built to run on every Palm OS 5 processor, including the older ARMv4T ones (Tungsten T, T2 and E, Zire 21 and 71, Treo 600, Z22), but hasn't been tried on those yet: reports are welcome.
>
> **Not a Palm-branded handheld?** You will most likely need to install **MCA2** (Modern Clie Audio 2) for the apps to work. It's made for Sony CLIÉ handhelds running Palm OS 5: [palmdb.net/app/mca2](https://palmdb.net/app/mca2) (see section 1).

---

## 1. Installing

Download the apps from the releases page: <https://github.com/cyberpunkgothic/palm-sid-player/releases>.

1. Install `SIDPlayer.prc`, `SIDPlayerDX.prc`, or both:
   - **with HotSync:** open the file with the Palm Desktop's Install Tool, then HotSync; or
   - **from a memory card:** copy the `.prc` files to the card's `PALM\Launcher` folder. They then appear in the launcher's card category and run from the card. The launcher's *Copy* command can copy them into the handheld if you prefer.
2. Both apps appear in the launcher with the Commodore logo.

**On a handheld that isn't Palm-branded,** such as a Sony CLIÉ with Palm OS 5, install **MCA2** (Modern Clie Audio 2) as well, before starting SID Player: without it, the apps will most likely not work there. MCA2 is available from PalmDB: <https://palmdb.net/app/mca2>

To remove them, use the launcher's *Delete* command as with any other app.

## 2. Adding tunes

1. On the memory card (SD or MMC), make a folder called **`SID`** at the top level, next to the `PALM` folder.
2. Put your `.sid` files in it. You can copy them with a card reader, or with whatever file transfer your handheld supports.

If your handheld has more than one card or drive, the player uses the first one that has a `SID` folder.

| What | Details |
|---|---|
| **Format** | **PSID** `.sid` files: the format of most of the High Voltage SID Collection (HVSC) |
| **File names** | Ending in `.sid`, under 64 characters. Underscores show as spaces in the list, and the `.sid` ending is left off. |
| **How many** | Up to 256 tunes; the list is sorted by name |
| **Size** | Up to 128 KB per file (SID tunes are far smaller) |
| **Folders** | Only the `SID` folder itself is read, not folders inside it |

The list is read when the app starts, and again whenever a card goes in or comes out while it's open: you can swap cards without leaving the app. If you take the card out while a tune is playing, the tune carries on (it's held in memory) until you choose another. If you add or remove tunes some other way (with the card still in), leave the app and open it again.

## 3. Playing music

### SID Player (the standard interface)

| On screen | What it does |
|---|---|
| **The list** | All the tunes in the `SID` folder. Tap one to play it. |
| **Song details** (below the list) | The tune's title, author and copyright (release year and publisher), then **Song N of M** and the time played. |
| **Play / Pause** | Pauses the music, and continues it from the same place. With no tune loaded, it plays the selected tune (or the first one). |
| **<** and **>** | The previous and next song in the same file. Many SID files hold several songs (a game's title music, in-game tunes, jingles); this steps through them and wraps around at either end. |

### SID Player DX (the C64 screen)

The DX screen is laid out like a C64 directory listing:

| Panel | What it shows |
|---|---|
| **Title line** | `*** SID PLAYER DX V1.01 ***` and the number of tunes on the card |
| **NOW PLAYING** | Title, author and copyright; **SONG N/M** and the time played. Before you choose anything it says *TAP A TUNE BELOW TO PLAY IT*. |
| **Buttons** | Play/pause, **<<** and **>>** (previous and next song in the file) |
| **VOICES** | One level bar for each of the SID chip's three voices. A bar jumps to full when its voice starts a note, then settles back to the voice's loudness. Shown only on the tall screen (see section 5). |
| **TUNES ON CARD** | The tunes with their size in disk blocks, as a C64 disk directory lists them. The tune playing is shown in white and the cursor as a highlighted bar. |

**In the tune list:**
- Tap a tune to play it.
- Tap the top edge of the TUNES ON CARD box to go up a page, or the bottom edge to go down a page.
- Arrows at the top and bottom right of the box show there are more tunes above or below.

### Keys (both apps)

| Key | Does |
|---|---|
| 5-way **up / down** | Move through the list one tune at a time |
| **Page up / down buttons** (older handhelds without a 5-way) | Move a page at a time |
| 5-way **left / right** | Previous / next song in the file |
| 5-way **centre** | Play the selected tune |

### Things worth knowing
- **Songs don't end by themselves.** Most C64 music loops forever, as it did in the games, and the SID format doesn't record song lengths. The time just keeps counting: change the song or the tune when you've heard enough.
- **The music keeps playing** while the About and Settings boxes are open.
- **The handheld won't switch itself off while music is playing.** The auto-off timer is held off during playback. When the music is paused, the handheld switches off after its usual delay.
- **Pressing the power button pauses the music.** When you switch the handheld on again, the app shows it as paused; tap Play to carry on from the same place.
- **Leaving the app** (the Home button, or opening another app) stops the music. The next time you open it, choose a tune again.

## 4. Settings

**Options menu > Settings…** (tap the menu icon, or the app's title bar on handhelds that open menus that way).

| Setting | Choices | What it does |
|---|---|---|
| **SID chip** | **6581** or **8580** (default 8580) | Which version of the C64's sound chip to imitate. In libsidplay 1 this only changes how two waveforms played together sound, so most tunes sound the same either way. A few, which use that trick, differ. |
| **Filter** | On (default) or off | Whether the SID chip's filter is imitated. Tunes are written for it, so leave it on unless you're short of speed (see section 6). Off makes many tunes brighter or harsher. |
| **Clock** | **Auto** (default), **PAL** or **NTSC** | The speed of the C64 the tune was written for. *Auto* uses the speed the tune asks for, or PAL if it doesn't say. PAL machines (Europe) and NTSC machines (America) ran at slightly different speeds and pitches, and tunes were written for one or the other: forcing the wrong one makes a tune play faster or slower. |
| **Volume** | A slider, **0%** to **200%** in steps of 5% (default 100%) | How loud the music plays. It changes as you move the slider, so you can set it by ear. The long marks under the slider are 0, 100 and 200%; the short ones are every 25%. 100% is libsidplay's own level, which leaves room for four voices: on earbuds you'll probably want less, on the speaker more. Most tunes stay clean up to 200%. The handheld's own volume settings in Preferences don't affect SID Player. |

Tap **OK** to keep the changes or **Cancel** to leave them as they were (Cancel also puts the volume back). If you changed the chip, the filter or the clock, **OK restarts the current song from the beginning** with the new settings; a new volume alone carries on without a restart. The settings are saved and used next time; SID Player and SID Player DX each keep their own.

## 5. Screen sizes

- **The tall screen (Palm TX and similar):** close the input area, and the app grows to fill the whole screen. Open it again and the app shrinks back. On the tall screen, SID Player shows more of the list. SID Player DX adds the VOICES panel with its level meters.
- **Landscape:** if your handheld can turn its screen sideways, both apps redraw themselves to fit. SID Player DX keeps its C64 screen at full width and fills the space at the sides with the border colour.
- **Square 320 x 320 screens:** both apps work. SID Player DX leaves out the VOICES panel to make room for the list.

## 6. Speed Test

**Options menu > Speed Test.** Choose a tune first: the test uses the current song.

The app makes 10 seconds of that song's music as fast as it can, then reports how long that took, as a share of the processor:

| Result | Meaning |
|---|---|
| Under 50% | *Plenty of headroom.* |
| 50–79% | *Should play smoothly.* |
| 80–99% | *Borderline: it may stutter.* |
| 100% or more | *Too slow to play in real time.* |

On a Palm TX, every tune tried used under 20% with the filter on, and under 10% with it off.

Busy tunes cost more than quiet ones, so try one of your busiest. If a tune stutters, turning the filter off in Settings makes playing cheaper; run the test again to see by how much. After the test the song starts again from the beginning; if it was paused, it stays paused.

(In the Palm OS Simulator on a PC, the test measures the PC, and says so.)

## 7. Messages

| Message | What to do |
|---|---|
| **No tunes in /SID on the card** (DX: *NO TUNES*) | There's no card, no folder called `SID` at its top level, or no `.sid` files in it. Check the card: the list updates when you put it back in. |
| **This player needs a Palm with an ARM processor (Palm OS 5).** / **This player needs Palm OS 5.** | The handheld is too old for SID Player. |
| **Not enough memory for the sound core / the player / the sound buffer** | Free some memory (delete apps or data you don't need), or reset the handheld, then try again. The player needs about 1 MB while it runs. |
| **The sound core is missing or damaged.** | Install the `.prc` file again. |
| **Could not open the sound output.** | The handheld's sound couldn't be started: reset the handheld and try again. |
| ***name* is not a PSID tune.** / ***name*: not a playable tune** | The file isn't a PSID tune, or it's damaged. Some newer HVSC files are in a format libsidplay 1 can't play. |
| **Could not open *name*** | The file was moved or renamed (with the card still in): reopen the app to read the list again. |
| **Choose a tune first: the test uses the current song.** | The Speed Test needs a tune playing or paused. |

## 8. About

**Options menu > About** shows the version and credits.

SID Player is free software under the **GNU General Public License, version 2 or later**, as is libsidplay 1, whose sound engine it uses. The source code is at <https://github.com/cyberpunkgothic/palm-sid-player>.

**Credits:**
- **Michael Schwendt:** libsidplay 1, the engine of SIDPLAY's original sound.
- **PalmSource and palmOne:** the Palm OS SDKs and simulators.
- **The community members who keep the old Palm development tools working:** Dmitry Grinberg (GCC for Palm OS), Wes Cherry and Aaron Ardiri (PilRC), palm2000.com and Chuan Ji.
- **Testing on other handhelds:** Christian Speckner (CloudpilotEmu, and finding a crash on rePalm), Dmitry Grinberg (uARM, rePalm, and the suggestion that lets the player run on older processors), and PalmDB and the people on its Discord who tried it out.

The README has the full list. SID Player was made as a collaboration between its author and an AI (Claude, by Anthropic); the README describes how.
