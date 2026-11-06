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
- From 4x on the game's built-in sounds stay off, so they do not pile up, and the bar says **(fewer sounds)**. The extra sounds from the `sounds` folder (monster steps, war cry, horn, critical hits) still play.
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
well: pick them with **File → Playback..** like your own. Antman's Dungeon Master run, for
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

The pace lives in `src/PlaybackControl.{h,cpp}` (no engine, no SDL; tested in
`src/automap_tests/PlaybackControlTest.cpp`). `checkVBL` in `src/system.cpp` hands out the
vertical blanks from a `PlaybackClock` when a replay is paused or not at 1x; at 1x the game's
own clock runs as before. A replay takes its input from the file by game tick, so the number of
blanks a second only changes how fast it goes. The menu and keys are in `src/CSBlinux.cpp`.
