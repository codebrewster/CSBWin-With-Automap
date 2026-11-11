# Recording and replaying a game

CSB can record every input of a game into a file and play it back later. The replay runs the
same game again, tick by tick: the same fights, the same deaths, the same win. While it runs,
the map and the Statistics window fill up exactly as they did when you played, so a replay is
also a way to get the map and the statistics of a game you played with the recording on.

## Recording

**File → Record** turns recording on or off. A tick mark means it is on.

- At the prison door (before **Enter** or **Resume**) recording starts right away.
- Anywhere else, Record is remembered and the recording starts at the next prison door: after
  **File → Reset**, or when you load a saved game with **File → Load saved game**.
- While a recording is being written, a red **REC** shows on the menu bar.
- Choosing **Record** again stops the recording; the file is complete up to that point.

Each recording goes into a new file `logs/RecordNNNNN.log` in the game folder (or in the game
folder itself when there is no `logs` folder). Hover **Record** in the menu for what it will do
right now.

A replay only works with the same `dungeon.dat` and `graphics.dat` it was recorded with.
Record cannot be switched during a replay.

Saving with **File → Save game** is recorded too: the recording gets a `2223` line, which CSBwin
also knows, so that a replay does to the game what the save did.

## One recording over several sessions

A whole game rarely fits into one sitting. **File → Continue recording...** carries a recording
on into the next session, so that the game ends up in one file:

1. Play with **Record** on. Stop whenever you like: save or not, then quit.
2. Next time, at the prison door (before **Enter** or **Resume**), choose
   **File → Continue recording...** and pick yesterday's `logs/RecordNNNNN.log`. **Record**
   must be off for that (choose it again if a new recording has just started).
3. The recording is replayed at full speed to its end. The replay bar shows how far it is;
   a long game takes a minute or two.
4. At its end the menu bar says **The recording continues in RecordNNNNN.log**, **REC**
   shows, and you play on. What you do is added to the same file.

You go on where the recording ends, which is where you stopped playing, whether or not you
saved there; you do not load a saved game for it. The game you continue is the one any replay
of the file reaches, so the file stays one plain stream of inputs and replays from start to end
like any other recording, in CSBwin 17.6 as well when it was made by its rules. It keeps the
rules it was recorded with (see below), whatever the settings say now.

If the last session ended with **Save and Quit** from the inventory's disk icon, a replay would
quit there too. Before the replay that save is turned into a `2223` line, as **File → Save
game** writes it, and the file as it was is kept as `RecordNNNNN.log.orig`.

The recording is not continued, and the menu bar says so, when the replay is stopped before its
end or goes out of step.

## Recordings that replay in CSBwin 17.6

A few rules of this port are not in Paul Stevens' CSBwin 17.6:

- clicking a fountain with an empty hand gives the leader water (17.6 touches the wall);
- a weapon left in the Altar of Vi gets its charges back;
- SHOOT gives 20 experience where `graphics.dat` says 9 (Dungeon Master 1.0);
- **Monsters vulnerable on attack**: a monster that is attacking has half its defence;
- **Launchers power** above 1: a missile shot with a bow, crossbow or sling hits that many
  times as hard.

**Misc → Nostalgia Mode** switches all of them off: the game then plays by the rules of 17.6,
and a recording made that way replays in 17.6 as well. Without it a recording replays only in
this port.

When a recording starts with any of these rules on, a box lists them and asks whether to switch
to the 17.6 rules. **Yes** turns Nostalgia Mode on; **No** keeps the settings, and the box does
not come back for the same settings until CSB starts again. The map's testing aids
(`map_debug = 1`) are listed too: what they do is not recorded at all, so no replay can follow
them; **Yes** turns them off until CSB starts again.

A recording names its rules in `#` lines at its start (`#NoFountainDrink`,
`#ShotDamageFactor1`, ...), which CSBwin skips. A replay plays by those rules, whatever the
settings are, and while a recording is written or a replay runs the three settings cannot be
changed. Recordings from before these lines replay as they were made: the launcher bonus at 4,
the fountain and the SHOOT experience on.

## Replaying

At the prison door, **File → Playback..** picks a `.log` file and starts the replay. You can
also start the game with `--play file.log` on the command line.

While the replay runs, the menu bar has a **Replay** menu and its controls beside it:

| Control | What it does |
|---|---|
| **Pause** / **Resume**, or **Space** | Stops the replay where it is, and goes on |
| **-** and **+** buttons, or the **-** and **+** keys | Slower and faster: 0.5x, 1x, 2x, 4x, 8x, 16x, 32x, 64x, max |
| **Replay → 0.5x … max** | Picks a speed directly |
| **Replay → Stop replay, play on from here** | Ends the replay; you play on from that point |

- The speed is relative to **Speed** in the menu: 2x at Normal is twice as fast as Normal.
- **tick N** shows how far the replay is; paused, it says so in yellow.
- From 4x on, max included, the sounds are thinned out and the bar says **(fewer sounds)**: a
  sound plays, but the same sound does not start again until 100 milliseconds of real time have
  passed. A fight at 64x is then a quick rattle of hits instead of a wall of noise or a backlog
  that plays on long after. Other sounds are not held up by it. This covers the game's own
  sounds, a dungeon's custom sounds and the extra ones from the `sounds` folder (monster steps,
  war cry, horn, critical hits). In `csb.cfg`, `replay_sound_gap` sets the milliseconds, and
  `replay_sounds = 0` turns the sounds of a fast replay off altogether, as before (the bar then
  says **(no sounds)**).
- **max** runs the replay as fast as the computer can, with no clock at all. The game window
  then shows about 30 pictures a second, so the time goes to the game rather than to drawing;
  Space and **-** still answer at once. A recording that asks for quick play itself
  (`#NoSpeedLimit`), or `--quick` on the command line, shows as max too.
- If the computer cannot keep up at a high speed, the replay simply runs as fast as it can; it
  never skips anything. How fast a replay runs never changes what happens in it.
- A new replay starts at 1x.

The Map and Statistics windows work as always during a replay, paused or not: look around,
switch topics, and use **EXPORT** and **Export as HTML** whenever you like.

## Recordings from CSBwin 17.6

Playfiles made with Paul Stevens' CSBwin 17.6 (often named `playfile.log`) replay here as
well: pick them with **File → Playback..** like your own. They replay by the rules of 17.6,
whatever Nostalgia Mode and the other settings say. Antman's Dungeon Master run, for
example, plays to its end on the same tick as in 17.6. Put the `dungeon.dat` and
`graphics.dat` it was recorded with into the game folder first; a replay with other game data
goes out of step.

If a 17.6 recording stops early with "Sequence/timing error", this port and 17.6 disagree
somewhere. Please report it with the playfile and the `dungeon.dat` it needs.

## When the replay ends

When the file runs out, the menu bar says **Replay ended at tick N: you play on from here**
for a few seconds, and the game is yours from that point. Nothing else stops: no message box
to answer, and the Map and Statistics windows, with their exports, stay open and usable.

A replay that reaches the win shows the end as the game did, and the Statistics window opens
by itself, as after a win you played; both exports work there too.

## For developers

The rules where the port and 17.6 differ, and which ones are in force, live in
`src/GameRules.{h,cpp}` (no engine, no SDL; tested in `src/automap_tests/GameRulesTest.cpp`).
Continue recording is in `src/Recording.cpp` (`ContinueRecording_*`, `RECORDFILE::Append`); the
preparation of a Save and Quit ending in `src/ContinueLog.{h,cpp}`, tested in
`src/automap_tests/ContinueLogTest.cpp`. `--continue-check DAY1:DAY2` tests it with a recording
cut in two at a tick: it continues `DAY1`, plays the inputs of `DAY2` (the header of the
recording and its lines from the cut on) into the continued game, and prints the fingerprints
of `--rewind-check` at the end. A replay of the continued `DAY1` must then end like the uncut
recording.
The pace lives in `src/PlaybackControl.{h,cpp}` (no engine, no SDL; tested in
`src/automap_tests/PlaybackControlTest.cpp`). `checkVBL` in `src/system.cpp` hands out the
vertical blanks from a `PlaybackClock` when a replay is paused or not at 1x; at 1x the game's
own clock runs as before. A replay takes its input from the file by game tick, so the number of
blanks a second only changes how fast it goes. The menu and keys are in `src/CSBlinux.cpp`.
