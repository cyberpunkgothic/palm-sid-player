# Palm TX test checklist

For the first test on the real handheld. Write down anything odd (what you did, what happened);
a photo of any error message helps.

## Setup
- [ ] Copy `SIDPlayer.prc` and `SIDPlayerDX.prc` (in the project folder) to the TX: with HotSync, or by
      putting them in the `PALM\Launcher` folder on the SD card (they then show in the launcher's card
      category and run from the card; the launcher's Copy command can move them into the TX).
- [ ] Put some `.sid` tunes in a folder called `SID` on the SD card: include Elvira, Rainbow Islands
      and Chevy Chase.
- [ ] Both apps appear in the launcher with the C= icon, and their names fit.

## Sound (the most important part: the simulator couldn't test this)
- [ ] A tune plays, and sounds the same as on the PC / SIDPLAY/w.
- [ ] No stutters, clicks or slowdowns over a minute or two of a busy tune (Rainbow Islands).
- [ ] **Speed test** (Options > Speed Test, with a tune chosen): write down the number for each app.
      Under 50% is comfortable; 80% or more means the fallbacks are needed (see DESIGN.md).
- [ ] Speed test again with the filter off (Options > Settings), to see what the filter costs.
- [ ] Settings: switching 6581/8580 changes Chevy Chase; filter off sounds brighter; NTSC is faster.

## Controls and screen
- [ ] Tap a tune, Pause, Play (resumes where it left off), previous/next song.
- [ ] 5-way: up/down move one line, left/right change the song, centre plays. Does up/down move one
      line or a whole page? (If a page, tell me: it's a one-line fix.)
- [ ] Close the input area (the tall screen): the app fills it. Open it again.
- [ ] Landscape (rotate button): the app redraws properly.
- [ ] About box and Settings box: the music keeps playing while they are open.

## Device behaviour
- [ ] Leave a tune playing without touching anything for 5 minutes: it must keep playing (the auto-off
      fix). After pausing, the TX should switch itself off as normal.
- [ ] Press the power button while playing: it goes to sleep. Press it again: the app shows Paused
      (SID Player: the button says Play), and Play continues the song.
- [ ] Leave the app while playing (Home button) and come back: no crash; the list is still there.
- [ ] Take the SD card out and put it back, then reopen the app.
- [ ] A soft reset afterwards: no problems (the apps leave nothing behind).

## Results
| Check | SID Player | SID Player DX |
|---|---|---|
| Speed test, filter on | under 20% on every tune tried (both apps) | |
| Speed test, filter off | under 10% | |
| Notes | | |

**2026-11-13, first test on the real TX (the ARMv4T build):**
- Both apps start and play.  Speed Test under 20% on every tune tried.
- Sleep (power button while playing): as intended.
- Landscape: as intended.
- Card out while playing: the current song keeps playing (it is in memory); choosing another says it
  can't be found.  Card back in: Palm OS switches to the launcher's card view, as it does for any app.
- Speed Test with the filter off: under 10%.  The 6581/8580 switch: as intended.
- 5-way: up/down jumped a whole page (the TX's 5-way sends page up/down); left/right change the song
  as intended.  Fixed 2026-11-13: up/down with the 5-way's navigator bits move one line; real page
  buttons on older handhelds still page.  Verified on the TX.
- Home and back: the app starts fresh (the list is there; the song stops), as Palm apps do.
- 5 minutes untouched while playing: kept playing (auto-off hold works).
- Filter off sounds brighter, NTSC plays faster: as intended.
- Soft reset afterwards: no problems.
- Card handling improved 2026-11-13 (verified on the TX, with a soft reset afterwards): with the app open, taking the card out
  keeps the current tune playing and empties the list; putting it back refreshes the list (the
  playing tune highlighted again) without switching to the launcher.
