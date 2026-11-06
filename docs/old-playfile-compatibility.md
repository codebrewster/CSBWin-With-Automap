# Replaying playfiles from older CSBwin versions

A playfile (`playfile.log`) stores the player's inputs. With each input it also
stores a check on the random number generator: either the generator's current
value or the number of random numbers drawn since the previous input. During
playback the game recomputes everything from the inputs. If the game rules
differ from the version that made the recording, the random numbers drift
apart and playback stops with **"Sequence/timing error / Should we quit?"**.

CSBwin changed a number of rules over the years. Our build reads the version
from the first line of the playfile, for example

```
#CSB for Windows/Linux Version 9.7
```

and switches on the old rule for every change that came after that version.
Recordings from 14.4 and newer get none of the old rules, so they play exactly
as before.

## The switches

All switches are set in `PLAYFILE::Open` (`src/Recording.cpp`) by
`PlayfileVersionBefore()`. The variables are declared and documented in
`src/data.cpp`.

| Switch | Old rule used for recordings | What the old versions did | Code |
| --- | --- | --- | --- |
| `legacyMissileRange` | before 14.4 | A monster's missile range was based on its dexterity instead of its attack. | `MonsterAttacks`, `Monster.cpp` |
| `legacyAttributeDefense` | 9.8 up to 12.5 | Vitality, Anti-Fire and Anti-Magic were ignored when reducing poison, fire and spell damage, as in the Atari original. 9.7 and older used them, like 12.6 and newer do. | `TAG016426`, `Character.cpp` |
| `legacySmartDiscard` | before 11.052 | The older SmartDiscard (clears floor items when the item list fills up), with its own default table and random priorities. | `SmartDiscardLegacy.cpp` |
| `legacyQuietParamFetch` | before 11.049 | No "DSA &PARAM@ non-existent parameter" warning, and no "variable used without having been assigned a value" box after it. | `STKOP_ParamFetch`, `DSA.cpp` |
| `legacyUnsequencedTimers` | before 11.0 | Timers due on the same tick ran in the order of their slots in the timer table, not in the order they were created. | `TIMER::operator <`, `Timer.cpp` |
| `legacyPitViewportRedraw` | before 10.078 | The level's graphics were loaded and the view redrawn at every level the party fell through. Drawing clouds and explosions uses random numbers. The change came between 10.056 and 10.078; the exact version is not known. | `MoveObject`, `Code11f52.cpp` |
| `legacyMonsterAttackOrder` | before 9.9 | A monster's missile type and range were drawn only when it throws a missile, and the hero to hit only when it doesn't. A dead hero picked at random meant no attack. | `MonsterAttacks`, `Monster.cpp` |
| `legacyDoubleShooter` | before 9.9 | A wall shooter that fires two missiles launched the second one from its own cell instead of the target cell ("Repair two problems with double shooters" in the 9.9 notes). | `TriggerMissileLauncher`, `Timer.cpp` |
| `legacyPadAnyObject` | before 9.9 | A floor pad "operated by anything" was pressed by every object, clouds and missiles too ("Clouds and Missiles should not activate PressurePads" in the 9.9 notes). | `PressurePadOrAlcove`, `Code11f52.cpp` |

The three 9.9 changes were made between 9.8 and 9.95 (the sources in between
are not available), so their boundary is 9.9.

### File checksums

9.7 and older recorded a shift-and-add checksum of `graphics.dat` and of the
dungeon or save file, not the MD5 that newer versions record. Playback warned
**"Recorded with Different Graphics File"** and **"... dungeon File"** even
when the files were the right ones. `Signature()` in `src/data.cpp` now also
computes the old checksum over the whole file, and playback accepts either
value (`legacyGraphicSignature`, `legacyDungeonSignature`).

## Tested recordings

### CSBMinimumPickingAntman4 (CSBwin 11.022)

| What changed | Old rule used for recordings older than | Effect in this game |
| --- | --- | --- |
| Vitality, Anti-Fire and Anti-Magic now reduce poison, fire and spell damage; before, they were ignored, as in the Atari original | 12.6 (and not older than 9.8) | Poison strength differed at tick 0x105 (261) |
| The "DSA &PARAM@ non-existent parameter" warning (and the "variable used without a value" box after it) was added | 11.049 | The boxes at tick 520; they don't change gameplay |
| A monster's spell range is based on its attack instead of its dexterity | 14.4 | A monster's spell flew too far at tick 0x4ed (1261) |
| SmartDiscard was rewritten | 11.052 | Different random numbers from tick 0x1afb (6907) on |

The replay matches a trace from the original `CSBwin11_022.exe` on every
traced tick (up to 0x20cb) and plays to its last input (tick 0x392d) without a
message box.

### CSB_Record10373 (CSBwin 9.7)

| What changed | Old rule used for recordings older than | Effect in this game |
| --- | --- | --- |
| Old file checksum instead of MD5 | — | The two "Recorded with Different ... File" warnings at tick 19 |
| Timers due on the same tick run in creation order | 11.0 | Two monster timers swapped from tick 0x14 on; error at tick 0x32 (50) |
| Attributes ignored by defence (only 9.8 up to 12.5) | 9.8 | A wound roll while falling through pits; error at tick 0x523 (1315) |
| Monster attack draws missile type, range and target always | 9.9 | A Screamer attack drew two extra numbers; error at tick 0x567 (1383) |
| Second missile of a double shooter starts in the target cell | 9.9 | A missile one cell off from tick 0x74c; error at tick 0x792 (1938) |
| Clouds and missiles no longer press floor pads | 9.9 | Pads not pressed at tick 0xed5; error at the last input, tick 0xedb (3803) |

The replay matches traces from the original `CSBwin.exe` 9.7 on every tick up
to the end of the recording, and shows no message box.

## How the differences were found

1. Copy the game folder and add `#TimerTraceOn` (and, near a problem,
   `#AITraceOn`) after the first line of the playfile.
2. Replay it headless with our build:
   `SDL_VIDEODRIVER=dummy src/csb --directory . --play playfile.log --quick 100000`.
   It writes `TRACE000.txt`.
3. Replay the same file in the original exe under wine:
   `wine CSBwin.exe play=playfile.log quick=NNNN` (also for 9.7, which takes
   these options although they are not visible in the exe's strings). It writes
   `trace000.txt`. 9.7 hangs at the end of a replay and has to be killed.
4. Compare the two traces tick by tick; the last column is the random number.
   Where they split, compare the code of the old source with ours.

The old sources and executables are at <https://dianneandpaul.net/CSBwin/index>
(for example `CSBsrc97.rar`, `CSBsrc98.rar`, `CSBsrc995.rar`,
`CSBwin10_078src.zip`, `CSBwin11_0src.zip`, `CSBwin11_014Source.7z`).
