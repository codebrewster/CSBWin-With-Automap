# Automap visibility: how the map decides what the party has seen

The map should show what a player would have noticed in the game's 3D view, no more and no less.
This page explains how it does that for floors, walls, wall decorations, doors, floor items,
monsters, missiles and clouds, and where each rule and threshold lives, so that you can change
them when the map and the 3D view disagree.

`docs/automap.md` is the full reference, with the history of each measurement. This page is the
map of the moving parts. Line numbers are as of 2026-10-24; search for the name if they drift.

## The pipeline in one picture

```
 engine draws a 3D frame
        │
        ├─ 1. Geometry: which of the 21 view cells could be on screen?      AutoMapSight
        │     (paint order + screen columns, ray to the centre, light reach)
        │
        ├─ 2. Measurement: how much of each drawn thing is left on screen,  AutoMapMeasure
        │     and does it stand out from what is around it?                 AutoMapSight scores
        │
        ├─ 3. Recording: explored cells, seen faces, items, cell looks,     AutoMapRecorder
        │     monsters seen now (only what the frame really showed)         (stores, saved in .map)
        │
        └─ 4. Drawing: vision mode × spoiler level filter the live level    AutoMapScene_BuildDrawList
              against the stores into the draw list the window paints
```

Steps 1 to 3 decide what the party *has seen*. They run every frame in the game and do not
depend on any map setting, except that the spoiler level and vision mode never feed back
into them. Step 4 decides what the map *shows* from that record. When the map shows too much
or too little, first work out which step is wrong:

| Symptom | Usually step |
|---|---|
| A cell, wall or floor appears that the 3D view never showed, or one it showed is missing | 1 (geometry, light reach) |
| A decoration, item, monster or door counts as seen though barely visible, or the reverse | 2 (thresholds) |
| Something seen is forgotten, or something gone is still remembered | 3 (recording rules) |
| The record is right but the map draws it wrongly in one vision mode or spoiler level | 4 (filter) |

## Building blocks

### The 21 view cells

The 3D view draws at most three tiles ahead and one tile to each side. The engine numbers the
cells like this (`AutoMapSight.h`):

```
               2 left  1 left  ahead  1 right  2 right
  4 ahead        0       4      6       5        1     <- never seen
  3 ahead       (2)     [7]     [9]     [8]      (3)    <- 2, 3 only as stone at the screen edge
  2 ahead       10      [12]    [14]    [13]     11     <- 10, 11 never seen
  1 ahead              [15]    [17]    [16]
  party row            [18]    [20]    [19]
```

Each frame the recorder gets these 21 cells (`AutoMapRecorder::RecordViewport`,
`src/AutoMapRecorder.cpp:113`). For each one it decides whether it blocks the view:

- stone and the rock outside the level block;
- a shut door blocks unless its picture is see-through (below);
- everything else (floor, open door, pit, teleporter, stairs) lets the view through.

### Rule A: is a cell on screen at all? (explored)

`AutoMapSight_FindExploredCells` (`src/AutoMapSight.cpp:196`) replays how the 3D view paints:
far rows first, and in each row the sides before the middle. Every wall covers its picture's
screen columns from floor to ceiling. A cell counts as on screen when at least
`AUTOMAP_SEEN_SCREEN_COLUMNS` (4) of its columns are left uncovered by blocking cells painted
after it. That is how the strip of floor beside a wall straight ahead counts. A shut door covers
only the columns its door is painted in: straight ahead its frame fills the doorway, but beside
the way the strip of stone three ahead at the screen's edge shows past it, and beside the party
the 3D view draws no door at all.

An explored cell is one the party has seen *some part of*. Floors and walls appear on the map
once explored, and so do items lying in them, because an item can lie anywhere in a cell.

### Rule B: is the centre of a cell in sight? (visible now)

`AutoMapSight_FindVisibleCells` (`src/AutoMapSight.cpp:183`) adds a second condition: the ray
from the party's cell centre to the cell's centre must miss every blocking cell, corners
included. Monsters stand at the centre, so a wall beside the party hides the monster on the
diagonal tile while the sword lying on that same tile still shows (rule A).

Visible-now cells are not stored. They are rebuilt every frame and drive what moves: monsters,
missiles and clouds under the fog of war.

### Rule C: how far the light reaches

In dark palettes the far rows are drawn black, and black has not been seen.
`AutoMapSight_LightReach` (`src/AutoMapSight.cpp:441`) checks the level's front wall picture of
each row (F1, F2, F3). A row is reached when at least `AUTOMAP_REACHED_ROW_PERCENT` (4 %) of
its pixels are one brightness step above black in the palette on screen. A row behind an
unreached row is not reached. `AutoMapSight_CutAtLightReach` then removes every farther cell
from both rule A and rule B. The party's own cell is always seen.

Target for the standard wall set: reach 3 at light 0..3, 2 at light 4, 1 at light 5. This is
the calibration goal, not yet a measured result.

**In the light versus a glimpse.** A cell that rule A found in the frame *before* the light cut,
but that the cut removed, is dark. Some things can still be noticed there: a portcullis, a
shimmering teleporter, a bright scroll, the yellow of an altar. The map records them as
*glimpses*: it knows that something is there, not what. Below spoiler level All a glimpse
draws as a small "?". `AutoMapRecorder::IsLitInFrame` (`src/AutoMapRecorder.cpp:272`) is the
test for "in the light".

### Brightness, standing out and contrast

Every measured draw (step 2) is scored from its pixels that are still on screen when the frame
is finished. Pixels that nearer cells, a portcullis or a pile's upper items later drew over
are already dropped, so occlusion needs no rule of its own. Three scores exist, all in
`src/AutoMapSight.cpp`:

| Score | Function | A pixel counts when | Used for |
|---|---|---|---|
| **Stands out** | `AutoMapSight_ShapeOfItem` (`:350`) | no background pixel within `AUTOMAP_CAMOUFLAGE_RADIUS` (2) has its exact colour | items, alcove items, monsters, doors, decorations (second test) |
| **Contrasts** | `AutoMapSight_ContrastingPixels` (`:304`) | its brightness differs by `AUTOMAP_DECORATION_CONTRAST_STEPS` (2) steps from the median brightness of the wall within the radius | decorations (first test) |
| **Lit** | `AutoMapSight_LitPercent` (`:372`) | its brightness is at least one step (`ONE_BRIGHTNESS_STEP`, `:236`) above black | light reach |

Brightness is `0.30 R + 0.59 G + 0.11 B` on the palette's 0..7 scale (`BrightnessOf`, `:228`).
Stands-out also yields `largestPatch`, the most standing-out pixels touching one another,
sideways or diagonally.

"Background" means what the draw is seen against:
- for a floor item, the frame without any floor items (`AutoMapDrawTally::FrameWithoutItems`),
  so the items of one pile do not hide one another;
- for a decoration, the wall, with what the decoration covers under its own pixels;
- for a monster or a door, what each of its pixels covers.

## Object by object

Each section answers three questions: when does the party *see* it (steps 1 to 3), what does
the map *remember*, and what does the map *draw* (step 4). The vision modes are Fog of war,
Explored, live and Whole level; the spoiler levels are Seen, Learned and All.

### Floors and the cell itself

- **Seen:** rule A, cut at the light reach. A shimmering visible teleporter in the ray counts
  as explored beyond the light too, but what lies in it is only glimpsed.
- **Remembered:** the explored bit (`AutoMapExplored`) and the cell's *look*: kind, door
  position, pit state, floor picture, the picture on each wall side (`AutoMapCellMemory`).
  `AutoMapRecorder::SeeCellLooks` (`src/AutoMapRecorder.cpp:345`) refreshes the look of every
  cell in the frame that is explored or that the party can tell (`CanTell`: its own cell, the
  cell ahead, cells visible now). While the party sleeps or a character sheet is open, only the
  party's own cell refreshes.
- **Drawn:**

  | | Fog of war | Explored, live | Whole level |
  |---|---|---|---|
  | Which cells | explored only | explored only | all |
  | Look below All | remembered look | live look | live look |
  | Look at All | live look | live look | live look |

  `AddCells`, `src/AutoMapScene.cpp:512`. Below All, `HideCellSecrets` (`:368`) then removes what
  the party does not know: an unknown false wall reads as wall, a hidden pit as floor, an
  invisible teleporter or spinner as floor.

### Floor pictures and stairs

A puddle, moss, a grate or a pad is not the cell itself: exploring a cell means only that some
strip of it was on screen, and the picture can lie behind the corner of a nearer wall. Stairs
are treated the same way: the viewport interpreter's stairs draws are measured too
(`AutoMap_AfterStairsDrawn`, the stairs rectangles in `Viewport.cpp`), and below All an
explored stairs cell reads as floor until its stairs were noticed.

- **Seen:** each floor picture the 3D view draws is measured like a wall decoration
  (`AutoMap_AfterFloorDecorationDrawn`, scored with `AutoMapSight_DecorationIsSeen` in
  `LearnFromViewportDraws`). Noticed in the light, its cell's seen-floor bit is set
  (`AutoMapExplored::MarkFloorSeen`, via `AutoMapRecorder::SeeFloorPicture`); beyond the light
  nothing is recorded. Footprints are not measured.
- **Drawn:** below All, under Fog of war and Explored, live, an explored cell shows its floor
  picture only once its seen-floor bit is set (`HideFloorPictureNotSeenYet`). A pit keeps its
  picture: it is the cell. At All, and in Whole level, every explored or shown cell has its
  picture.
- **Measured:** the puddle on DM L0 (5,2), walking north up column 4: from (4,5), (4,4) and
  (4,3) it is drawn but the wall on (5,3) leaves 0 of its pixels; from (4,2) facing east 207 of
  217 contrast. The moss on (4,3) three ahead: 6 of 6, seen. The stairs on L0 (3,15): from
  (4,12) facing south not drawn at all, from (4,13) only their edge (39 pixels, patch 29),
  from (4,14) 147 of 205 contrast.

### Walls and the rock around a level

- **Seen:** a wall cell is explored by rule A like a floor. The reader adds a ring of rock
  around each level (`ADDED ROCK: OUTSIDE THE LEVEL`). That ring, the *rim*, is explored like
  the level's own cells once the 3D view painted it in the light.
- **Drawn:** below All, rim rock shows only once explored. At All it also shows beside
  explored floor, so the map does not end in a ragged edge (`RimRockIsSeen`,
  `src/AutoMapScene.cpp:495`).

### Wall faces and decorations

A wall cell has four faces. Being explored makes the stone appear on the map; each face's
decoration (switch, lever, ring, fountain, writing, alcove) needs that face to be *seen*.

- **Which faces the 3D view draws at all:** `AutoMapSight_FacesShownOfCell`
  (`src/AutoMapSight.cpp:217`). Cell 15 shows only its right face, 16 only its left; 7 and 12
  their right and front faces, 8 and 13 their left and front; 9, 14 and 17 their front. The
  cells beside the party and the party's own cell show none.
- **Seen:** a decoration the 3D view drew is scored in `LearnFromViewportDraws`
  (`src/AutoMapHooks.cpp:442`) with `AutoMapSight_DecorationIsSeen`. It is noticed when
  *any* of these holds:
  - at least `AUTOMAP_SEEN_DECORATION_PIXELS` (8) of its pixels contrast in brightness. This
    catches switches, which are made of the wall's own greys;
  - its largest patch of standing-out pixels has at least `AUTOMAP_SEEN_DECORATION_PATCH`
    (8) pixels. This catches tinted things in a dark palette, where brightness steps are few:
    a red board on a black wall;
  - its largest patch of standing-out pixels that are also tinted (not grey, as the stone is)
    and each a brightness step off the wall has at least `AUTOMAP_SEEN_DECORATION_TINT_PATCH`
    (4) pixels. This catches a thin brown line on a dark wall, where the colour alone is too
    small a patch. A grey a step off the wall reads as the stone's own speckle, so it does not
    count;
  - it has at least `AUTOMAP_SEEN_BLOT_PIXELS` (40) pixels, and together they are on average
    `AUTOMAP_SEEN_DECORATION_DARKENING` (0.6) of a step darker or brighter than the wall they
    cover. This catches a black slot on a speckled dark wall, where every black pixel has a
    black speck beside it but the eye sees the specks gone.

  If the wall is in the light, the face is marked seen (`AutoMapExplored::MarkFace`). If not,
  it is marked glimpsed (`MarkGlimpsedFace`). See `AutoMapRecorder::MarkDrawnDecoration`
  (`src/AutoMapRecorder.cpp:185`).
- **Wall text from afar:** the 3D view draws it as scribbles, thin strokes in the wall's own
  greys. Those are noticed only from `AUTOMAP_SEEN_SCRIBBLE_PIXELS` (24) contrasting pixels
  (`AutoMapSight_ScribblesAreSeen`), not by the rules above: the writing beside the first Vi
  altar (DM L0 (5,17) west) is a fleck of 10 pixels three ahead and a thin line of 18 two ahead,
  and shows with 72 one ahead.
- **Special faces:** a champion's mirror is a decoration like any other: from afar the 3D view
  draws its frame. The champion's face is drawn only on the wall straight ahead; that draw
  marks the face seen and read (`AutoMap_OnPortraitFaceDrawn`), and below All the map shows
  the face only on a read face, the frame elsewhere (`HideTextsNotReadYet`). A mirror whose
  champion was taken draws no face. Wall
  text counts as read only when drawn as words, which happens straight ahead in the light
  (`MarkTextRead`). Each item in an alcove counts as seen on its own when it stands out like a
  floor item (`SeeAlcoveItem`), or when the party put it there itself; it is remembered in the
  item memory on the wall cell. A face looked into as a whole (a clicked portrait, a map from
  before items were told apart) shows everything in it. Measured on DM L2 (36,40) west: at light 0
  cheese (patch 173) and bread (45) seen, an apple almost hidden (1) not; at light 4 the bread
  barely (6); at light 5 the cheese alone (79). A
  switch or other wall actuator the party clicked and fired straight ahead marks its face
  seen, however dark it was drawn (`AutoMapRecorder::UseFaceAhead`).
- **Drawn** (`SeenSides`, `src/AutoMapScene.cpp:111`; `HideWallSidesNotSeenYet`, `:145`):

  | | Fog of war, Explored, live | Whole level |
  |---|---|---|
  | Below All | face seen by the 3D view (`FaceIsSeen`) | every face |
  | At All | the floor cell in front of the face is explored | every face |
  | Door sides | the floor cell in front is explored (stone faces only are recorded) | every face |

  Below All, a seen decoration still looks like plain dressing (no side strip, no link badge)
  until the party has used it. A glimpsed face draws a "?" on that side.

  Goods in a wall (alcove contents, coins in a slot, a portrait's gear) show below All once
  the party looked into that face (`HideTextsNotReadYet`). Whole level below All shows them on
  every alcove face, since the 3D view draws goods only there, and on faces looked into
  (`HideGoodsNotDrawn`). The coins in CSB L10's (36,36) slot stay hidden until it opens.

Measured in the game: everything noticed while walking past scored 10 or more, everything
missed 4 or fewer. The table in `docs/automap.md` (search "The decoration threshold") lists the
cases.

### Doors

- **Blocking:** a door blocks the view only when it is shut or almost shut and its picture is
  solid (`AutoMapSight_CanSeeThroughDoor`, `src/AutoMapSight.cpp:42`). "See-through" is decided
  from the picture, not from the engine's flag, which is about what monsters see. A shut
  door's graphic, with its ornament painted on, is see-through when at least
  `MIN_SEE_THROUGH_PIXELS` of its pixels are transparent, that is 1/40 of its 96×88 pixels
  (`src/AutoMapRecorder.cpp:71`). A plain wooden door leaves 114 stray transparent pixels, a
  window ornament 304, grates thousands.
- **Seen beyond the light:** a drawn door is scored like an item but needs no patch. It is
  seen from `AUTOMAP_SEEN_DOOR_PIXELS` (16) standing-out pixels, touching or not
  (`AutoMapSight_DoorIsSeen`). A portcullis two ahead at light 5 makes 65 and is seen; a wooden
  door there is black. A seen door's cell is explored, and its look is recorded
  (`AutoMapRecorder::SeeDoor`).
- **Drawn:** as part of the cell look (above). Below All, a breakable door looks like any
  other door, and the break force is never shown.

### Floor items

- **Seen:** each item draw is scored in `LearnFromViewportDraws`. The item is seen when its
  largest patch of standing-out pixels has at least `AUTOMAP_SEEN_ITEM_PATCH` (4) pixels: a
  sword's hilt in the dark (4) or corn (5) are noticed, bread mostly behind a wall (2) is not.
  Grey bread on grey stone, or a black club on a black floor, are camouflaged and score low.
  In the light, the item is remembered on its cell (`AutoMapItemMemory::See`). Beyond the
  light it is remembered as a glimpse. An item already remembered and identified on that cell
  is not measured again.
- **Size and darkness in the 3D view:** items are drawn smaller and darker with distance, and
  not at all on the far half of three ahead (table in `docs/automap.md`, search "Floor items
  are darkened"). The measurement sees the result, so this needs no rule of its own.
- **Forgotten:** when an explored cell in the light no longer holds a remembered item, or
  the record now holds another kind of item (`ForgetItemsGoneFromFrame`,
  `src/AutoMapRecorder.cpp:172`). Picking an item up forgets it. Dropping one remembers it,
  in the dark too, because the party knows what it dropped.
- **Drawn:**

  | | Fog of war | Explored, live | Whole level |
  |---|---|---|---|
  | Below All | remembered items where last seen (`RememberItems`, `src/AutoMapScene.cpp:458`) | live pile, but only items seen somewhere, wherever they lie now (`ShowSeenLiveItems`, `:467`) | live pile |
  | At All | live pile on explored cells | live pile on explored cells | live pile |
  | Glimpse "?" below All | yes | yes | no |

  Below All, chest contents need the chest opened and scroll text the scroll read
  (`HideItemSecrets`, `:326`).

### Monsters

- **Seen now:** below All, each group's draws in the frame are scored together. The group is
  seen from `AUTOMAP_SEEN_MONSTER_PIXELS` (2) standing-out pixels, touching or not
  (`AutoMapRecorder::SeeMonster`). Two orange eyes in the dark are enough. A Rock monster
  drawn as dark as the floor is not. Brightness contrast misses monsters, because bright eyes
  on black stand out by colour, so only stands-out is used. At All, rule B decides instead,
  cut at the light reach.
- **Remembered:** nothing. Monsters move, so the map never draws an old position.
- **Drawn** (`AddMonsters`, `src/AutoMapScene.cpp:661`; `MonsterIsSeen`, `:635`;
  `ShowsLiveAt`, `:652`):

  | | Fog of war | Explored, live | Whole level |
  |---|---|---|---|
  | Below All | groups seen now (measured) | every group on an explored cell, on any level, except in a cell drawn as stone | all |
  | At All | groups on cells visible now (rule B) | every group on an explored cell | all |

  Below All a monster shows no hit points (only the damage the game showed) and nothing it
  carries.

### Missiles and clouds

- **Seen:** not measured. Under Fog of war they show on cells visible now (rule B, cut at the
  light), at every spoiler level.
- **Drawn:** Explored, live shows them on every explored cell, with the same stone-cell
  exception as monsters below All. Whole level shows all (`AddMissiles`, `:678`; `AddClouds`,
  `:700`).

### Floor actuators (pads)

At All, they show on cells visible now under Fog of war, and on explored cells under
Explored, live. Below All, an actuator shows only once the party knows it fired, and then
stays on its explored cell (`AddActuators`, `src/AutoMapScene.cpp:735`). This is learning
rather than sight; `docs/automap.md` (section "Spoilers") has the rules.

## The knobs

Raising a threshold makes the map see *less*; lowering it makes it see *more*.

| Knob | Value | Where | Governs | Evidence |
|---|---|---|---|---|
| `AUTOMAP_SEEN_SCREEN_COLUMNS` | 4 | `src/AutoMapSight.h:38` | columns of a cell left uncovered for it to count as on screen | narrowest real strips are 9 and 11 columns; fewer are picture edges meeting |
| `AUTOMAP_REACHED_ROW_PERCENT` | 4 % | `src/AutoMapSight.h:191` | share of lit pixels for a row to be in the light | target reach 3/3/3/3/2/1 at light 0..5, not yet calibrated |
| `AUTOMAP_REACH_ROWS` | 3 | `src/AutoMapSight.h:190` | rows checked for light | the view's depth |
| `ONE_BRIGHTNESS_STEP` | 100 | `src/AutoMapSight.cpp:249` | brightness unit: one step of the 0..7 scale | |
| `AUTOMAP_CAMOUFLAGE_RADIUS` | 2 px | `src/AutoMapSight.h:128` | how far around a pixel the background is searched for its colour | |
| `AUTOMAP_DECORATION_CONTRAST_STEPS` | 2 | `src/AutoMapSight.h:96` | brightness steps from the wall's median for a decoration pixel to contrast | |
| `AUTOMAP_SEEN_DECORATION_PIXELS` | 8 | `src/AutoMapSight.h:97` | contrasting pixels for a decoration or floor picture to be noticed | noticed ≥ 10, missed ≤ 4 |
| `AUTOMAP_SEEN_SCRIBBLE_PIXELS` | 24 | `src/AutoMapSight.h` | contrasting pixels for wall text drawn as scribbles | missed 10 and 18 (three and two ahead), noticed 72 (one ahead) |
| `AUTOMAP_SEEN_DECORATION_PATCH` | 8 | `src/AutoMapSight.h:105` | standing-out patch for a decoration in dark palettes | red board 38..258; light 5 always ≤ 1 |
| `AUTOMAP_SEEN_DECORATION_TINT_PATCH` | 4 | `src/AutoMapSight.h:114` | tinted standing-out patch a step off the wall | brown line (2,1,0) light 2: 6 seen; (1,0,0) light 3: 0 missed; grey line 7 at light 4 missed |
| `AUTOMAP_SEEN_DECORATION_DARKENING` | 60 | `src/AutoMapSight.h:121` | mean brightness difference, in hundredths of a step, between a decoration and the wall it covers | slot of 86 px: light 3 72 seen, light 4 47 only when looked for, light 5 23 missed |
| `AUTOMAP_SEEN_BLOT_PIXELS` | 40 | `src/AutoMapSight.h:122` | pixels a decoration needs to be told by its tone alone | |
| `AUTOMAP_SEEN_ITEM_PATCH` | 4 | `src/AutoMapSight.h:126` | standing-out patch for a floor or alcove item | hilt 4, corn 5 seen; bread 2 missed |
| `AUTOMAP_SEEN_MONSTER_PIXELS` | 2 | `src/AutoMapSight.h:133` | standing-out pixels of a whole group | two eyes 2 seen; one eye 1 barely; dark Rock 0 |
| `AUTOMAP_SEEN_DOOR_PIXELS` | 16 | `src/AutoMapSight.h:167` | standing-out pixels of a shut door beyond the light | portcullis two ahead at light 5: 65 |
| `MIN_SEE_THROUGH_PIXELS` | 1/40 of the door | `src/AutoMapRecorder.cpp:71` | transparent pixels for a shut door to let the view through | wooden 114 (solid), window 304 (see-through) |
| Stone-only cells | 2, 3 | `src/AutoMapRecorder.cpp:125` | view cells the 3D view draws only as a strip of stone | |

The rule *shapes* matter as much as the numbers. Each one is a single line you can change:

| Choice | Where |
|---|---|
| Items need a *patch*; monsters and doors count pixels *touching or not* | `LearnFromViewportDraws`, `src/AutoMapHooks.cpp:442` |
| Decorations pass on contrast, patch, tint patch *or* blot | `AutoMapSight_DecorationIsSeen`, `src/AutoMapSight.cpp:358` |
| Monsters use rule B at All and the measurement below All | `MonsterIsSeen`, `src/AutoMapScene.cpp:635` |
| What blocks the view | `CanSeeThrough`, `src/AutoMapRecorder.cpp:100` |
| What counts as "in the light" versus a glimpse | `IsLitInFrame`, `src/AutoMapRecorder.cpp:272` |

## How to tune against the 3D view

1. **Reproduce it.** Stand where the map and the 3D view disagree. With `map_debug = 1` in
   `[settings]` of `csb.cfg`, the map's right-click menu can teleport the party and spawn
   monsters. **FORGET** `LEVEL` clears the explored cells, faces and items, so the same view
   can be tried again.
2. **Read the scores.** With `map_timing` set, each new view prints its decorations as
   `drawLocation:contrasting of kept patch largest`, its items as
   `record@x,y:visible of kept patch largest`, its doors as `door@x,y:...` and its monsters as
   `record pos positions:visible of kept patch largest`. It also writes pictures of the frame
   (`automap-decorations-*.png`, `automap-items-*.png`, `automap-monsters-*.png`): magenta
   pixels counted, cyan did not, yellow were drawn over. Compare the numbers with what your
   own eye noticed.
3. **Sweep the whole level.** **MEASURE** `WALLS` draws the view from every floor cell in all
   four facings. It writes `automap-walls-L<level>.csv` (decorations) and
   `automap-things-L<level>.csv` (items and monsters) as colour-index pairs, so one sweep
   serves every light level. Use it to see where a candidate threshold would split noticed
   from missed.
4. **Decide what "noticed" means.** The rule so far: count what a player *running past* would
   notice. "Only when looking for it" does not count.
5. **Pin the case down.** Add it to `src/visibility_tests/dungeon-master.txt`: an `at` line with
   the place (as the map's right-click menu title writes it), facing and light, and one check
   line per thing, `seen`, `glimpsed` or `not seen`. Name an item of a pile as the game shows
   it, `item (4,15) "Scroll" seen`, adding its corner (`NW`, `NE`, `SE`, `SW`) when two items
   there share the name. A floor picture is checked by its cell, `floor (5,2) not seen`, a
   champion's face by its wall face, `portrait (14,2) S seen`, an item in an alcove by its face
   and name, `alcove (36,40) W "BREAD" seen` (without a name: anything in it). An
   `at` line without checks is a survey:
   `make -C src visibility-test` then prints a check line for every thing the frame drew, with
   the numbers it was judged by. Keep the lines that match your eye and correct the others; a
   corrected line fails until the scoring agrees. Each `save` line starts a section whose cases
   are drawn in that saved game, from `Dungeons/Dungeonmaster`: keep focused saves (one for
   monsters, one for doors) rather than one crowded one. The run loads every save without a
   window, one process per save, and draws every case with the engine, so it needs the game
   data, which is not in git.
6. **Change the knob or rule**, then add the measured case to the table in `docs/automap.md`
   and to the comment above the constant, as the existing ones do.
7. **Test.** `make -C src visibility-test` for the cases drawn by the engine, and
   `make -C src test`. The scoring functions are tested in
   `src/automap_tests/SightTest.cpp`; the drawing rules in `SceneTest.cpp`, `SpoilerTest.cpp`
   and `VisionTest.cpp`. A threshold change usually needs a SightTest case at the new
   boundary. Run `make -C src clean` before a release build, because the tests leave
   sanitizer objects in `src/obj`.

## Where the code lives

| Unit | Role | Engine-free and tested |
|---|---|---|
| `AutoMapSight` | rules A, B and C; the scores and thresholds | yes (`SightTest.cpp`) |
| `AutoMapMeasure`, `AutoMapDrawTally` | capture each draw's pixels and what is left of them at the end of the frame | tally yes |
| `AutoMapRecorder` | turns a frame into explored cells, faces, looks, items, monsters seen now | no |
| `AutoMapHooks` (`LearnFromViewportDraws`) | scores each draw and tells the recorder | no |
| `AutoMapExplored`, `AutoMapItemMemory`, `AutoMapCellMemory` | the stores, saved in the `.map` file | yes |
| `AutoMapScene` (`AutoMapScene_BuildDrawList`, `:954`) | step 4: vision mode × spoiler level → draw list | yes (`SceneTest`, `SpoilerTest`, `VisionTest`) |
