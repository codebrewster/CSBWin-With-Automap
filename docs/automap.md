# Automap

The automap is a second OS window showing a live top-down map of the level the
party is on, drawn with the game's own pictures. It is the newest large feature
in the tree. This page is the map of the *code*.
How the map decides what the party has seen, with every rule and threshold in one place, is in
`docs/automap-visibility.md`.

## The unit split

The feature is deliberately split so that most of its rules can be tested without
the engine or SDL. The `src/makefile` makes the split explicit: the units in
`AUTOMAP_TESTED` link into the test program built by `make test`
(`src/makefile:80`).

**Engine-free, tested** (`make test`):

| Unit | Holds |
|------|-------|
| `AutoMapExplored` | The fog-of-war state: which cells have been explored, and which wall faces the 3D view has shown, per level (`AutoMapExplored.h:37`) |
| `AutoMapKnowledge` | The facts the party has learned by playing, as a set that only grows (`AutoMapKnowledge.h:48`) |
| `AutoMapLinkWatch` | Learns which cells a fired actuator changes by watching them; also learns false walls seen opening or closing (`AutoMapLinkWatch.h:15`) |
| `AutoMapSight` | The line-of-sight rule over the cells the 3D view paints (`AutoMapSight.h`) |
| `AutoMapFile` | The `.map` file: loading/saving explored cells, seen faces and learned facts, keyed by the save signature (`AutoMapFile.h:41`) |
| `AutoMapScene` | The drawing *rules*: turns plain level records into a draw list, tooltip slots, trigger text (`AutoMapScene.h`) |

`ShotMissiles` is tested alongside them (it also needs neither the engine nor SDL).

**Engine-facing** (link only into the game):

| Unit | Holds |
|------|-------|
| `AutoMapLevelReader` | Reads the engine's dungeon into the plain records `AutoMapScene` works on |
| `AutoMapRecorder` | Keeps what the party has seen, sees now and has learned, fed by the 3D view and the hooks (`AutoMapRecorder.h`) |
| `AutoMapHooks` | The functions the engine and platform layer call (declare-only header, safe after `CSB.h`) |
| `AutoMapSprites` | Decodes the game pictures into SDL textures for the window |

**The window itself:** `AutoMapWindow.cpp` — the SDL window, painting, input,
the legend, and the `csb.cfg` settings.

## How it flows

```
DrawViewport (Viewport.cpp:6677)
        | summarises its 21 cells
        v
AutoMap_OnViewportDrawn   (AutoMapHooks)      once per drawn 3D frame
        |
AutoMapRecorder -- feeds --> AutoMapExplored + AutoMapSight   (fog + what is seen)
        |
AutoMap_OnEngineTick      (AutoMapHooks)      once per game tick, view drawn or not
        |
AutoMapLevelReader -- reads engine d --> AutoMapScene   (builds the draw list)
        |
AutoMapWindow            (SDL window)  draws the sprites from AutoMapSprites
```

The hooks are the only coupling to the engine: `AutoMap_OnViewportDrawn`,
`AutoMap_OnEngineTick`, and the ready/save/load entry points in
`AutoMapHooks.h`. Engine files gain only one-line calls; see
[architecture.md](architecture.md).

## State that survives saving

What the party has explored, seen, learned and left lying on the floor is kept in a `.map` file beside the game, not inside
the savegame (see [data-formats.md](data-formats.md)). It is keyed by the saved
game's identity — the two words `Signature()` computes over the save file
(`AutoMapSaveSignature`, `AutoMapFile.h:10`) — so a save and its map stay
together. The automap writes its own settings (window size, whether it was open)
into `csb.cfg` via `AutoMapWindow_ReadSettings`/`AutoMapWindow_WriteSettings`
(`AutoMapWindow.h`).

The file is format version 11. After the explored bits of each level it holds four
seen-face bits per cell, four glimpsed-face bits per cell, four read-text bits per cell and four looked-into bits per cell, then the same five sets for the level's rim (since version 11, see the rim rock below), and after the levels the list of learned facts: kind, record,
level, x, y, then the floor items the 3D view last showed (`AutoMapItemMemory`): record,
description index, level, x, y and whether it was a mere glimpse, in the order seen, and
last the remembered cell looks (`AutoMapCellMemory`): level, x, y, kind, door state, pit
open and hidden, floor picture and the four side pictures, and since version 9 the player's
notes (`AutoMapPlayerNotes`, see [Player notes](#player-notes)): a count of pit marks with
level, x, y and open per mark, then a count of player links with the source's level, x, y and
side (a signed byte, -1 for a floor source) and the target's level, x, y (layout in
`AutoMapFile.h`, [data-formats.md](data-formats.md)). A cell outside the dungeon or a side
outside -1..3 fails the read. A file older than version 9 reads with no notes. A
version-1 file is still read: its seen faces are filled in by the old rule (every face
whose facing floor cell is explored, `AutoMapExplored::MarkFacesTowardsExploredCells`)
and it holds no facts. A file older than version 3 holds no items, and `Read` reports that so the
caller can fill the memory itself; the items of a version-3 file count as seen, not glimpsed.
A file older than version 8 holds no cell looks, and `Read` reports that too: the loader then
gives every explored cell of every level the look it has now
(`RememberLooksOfExploredCells` in `AutoMapHooks.cpp`), so no cell flips on the first map
open. `Read` changes no store unless the whole file was read.

## Vision

The vision says *where* the map looks. It is the `map_vision` key in `[settings]` of
`csb.cfg` and the menu Map ▸ Vision: `0` fog of war (the default), `1` explored, live,
`2` whole level (`AutoMap_Vision` in `AutoMapHooks.h`). A `csb.cfg` without `map_vision`
takes the old `map_fog` key: `1` is the fog of war, `0` the whole level. `map_fog` is still
written, so an older build opens with the nearest mode.

- **Fog of war** draws only explored cells, each as the party last saw it, and monsters,
  missiles and clouds only where the party sees right now. Below, "the fog on" means this mode.
- **Explored, live** draws only explored cells too, but live, as if the party still saw each
  one: its live look and item pile, and monsters, missiles and clouds on every explored cell,
  on any level. Below `all` the pile holds only items the party has seen somewhere, drawn
  where they lie now: an item the 3D view never showed (a key under ashes, or one a monster
  dropped out of sight) stays off the map until it is seen, and a glimpsed item keeps its "?"
  as under the fog of war. It is the fog on with `AutoMapFog::isLive`. The spoiler level still
  strips secrets, a wall face still needs the 3D view to have shown it, and a "?" on a face
  not seen stays.
- **Whole level** draws every cell live. Below, "the fog off" means this mode.

The vision changes only what is drawn: the party learns nothing from the map, whatever the
mode. The explored-cell store is `AutoMapExplored`; the "what can be seen right
now" rule is `AutoMapSight`; the filter that combines them into a draw list is in
`AutoMapScene`.

`AutoMapSight` reads what the 3D view shows from how it paints. It paints far rows
first and in each row the sides before the middle, and every wall covers the screen
columns of its picture (the size rectangles of graphic 558, `d.wallRectangles`)
from floor to ceiling. A cell is on screen where no blocking cell (stone, shut solid
door) painted after it covers its columns, a floor cell taking the columns its wall
would; `AUTOMAP_SEEN_SCREEN_COLUMNS` columns left uncovered are enough. So past a
wall straight ahead the strip of the column beside it shows, and past a wall beside
the way the inner edge of the tile behind it. The view
never shows more than three tiles ahead or more than one tile to either side, so
only those twelve cells can be seen, and the two three ahead and two to a side
when they are stone: there the view draws a strip of wall at the screen's edge and
nothing else.

The light reach cuts both sets below, the explored cells and the visible-now cells, at every
spoiler level (`all` included): a cell more rows ahead than the light reaches is neither
explored nor seen now, so nothing is learned from what happens there either. `CanTell` still counts the
party's own cell and the cell ahead of it.

Two questions come out of this. What is seen *right now* (drives monsters,
missiles, clouds) on a floor cell also needs the ray from the party's cell centre to
the cell centre to miss every blocking cell, because a monster stands at the centre: a
wall beside the party hides the monster on the diagonal tile. What is *explored*
(drives the static map and its floor objects) is looser — a floor cell is explored
as soon as any sliver of it is in sight, because objects lie anywhere in the cell,
so a sword on that same diagonal tile still shows. This is the rule whose
captures were taken from the running game.

The map is a memory aid, not a live replica. At spoiler level `all`, with Explored, live or
with the fog off, an
explored floor cell shows *all* of its live floor items, whether or not they are in sight.
Below `all` (`seen` and `learned`) with the fog on, a cell shows the items the party
remembers on it (`AutoMapItemMemory`): each item where the 3D view last showed it, drawn
from the live pile while it is still there and from its object description once it has left.
An item the party never saw is not on the map, even on an explored cell. Chests and scrolls
keep the usual rule: contents only once opened, text only once read. Old (version 2) map
files remembered nothing, so `AutoMapScene_RememberExploredItems` seeds the memory with
every live item on an explored cell. Monsters, missiles and clouds are the exception: they
show only while actually in sight (with Explored, live: on every explored cell).

Light decides how far ahead the map explores. The 3D view draws the far rows darker, and the
dark palettes turn them black, so what the light does not reach has not been seen. A row is
reached when at least `AUTOMAP_REACHED_ROW_PERCENT` of the pixels of the level's front wall
picture of that row (F1, F2, F3) are one brightness step above black in the palette shown (the
debug light included); a row behind an unreached one is not reached. `AutoMapSight_LightReach`
(tested) counts the reached rows, and the hook caches the result per level, palette and wall
set. In the standard wall set the target is reach 3 at light 0..3, 2 at light 4 and 1 at light 5;
this is what the calibration must reach, not yet a measured result (the in-game calibration
is open).
The party's own cell is always seen. A cell explored before, in better light, stays explored.
A shut door is measured too (`AutoMap_AfterDoorDrawn`, `AutoMapDrawDoor`): when its picture
leaves `AUTOMAP_SEEN_DOOR_PIXELS` pixels standing out in the finished frame, touching or not
(`AutoMapSight_DoorIsSeen`), its cell is explored and its look seen (`AutoMapRecorder::SeeDoor`)
beyond the light's reach. A portcullis two ahead at light 5 shows as a grid of 25 tiny patches
(65 pixels) and counts; a wooden door there is black. The items line of the trace lists doors as
`door@x,y:visible of drawn patch largest`, and the items' picture marks their pixels.
A visible teleporter is the exception: its shimmering light blue field shows three tiles ahead
even at light 5, so a cell of one in the ray counts as explored beyond the light's reach too.
It is not lit, though (`AutoMapRecorder::IsLitInFrame`): what lies in it is only glimpsed, and
a remembered item there is not forgotten.

Floor items are darkened like monsters, with their own colour maps and one row sooner
(`Code51a4.cpp:1850`):

| Where the item lies | Colour map | Size |
|---|---|---|
| The party's own tile | none | full |
| One tile ahead, or the near half of two ahead | `Byte5744` | 20/32 |
| The far half of two ahead, or the near half of three ahead | `Byte5728` | 16/32 |
| The far half of three ahead | not drawn | none |

The 3D view also moves a monster's colours to darker ones two ahead (`Byte6628`) and three
ahead (`Byte6612`, `Code51a4.cpp`), and the dark palettes turn many of those black, so in
palette 5 a Rock monster is gone even right in front of the party while a Screamer still
shows three ahead. The map does not work this out from the colours: below `all`, with the fog
on, a monster group is measured like a floor item. Both `DrawMonster` blits report the
pixels they put on screen under the group's record, the tally adds up the members, and the
group shows exactly when the finished frame holds `AUTOMAP_SEEN_MONSTER_PIXELS` pixels of it
that stand out (below; `AutoMapRecorder::MonstersSeenNow`). That covers a group half behind a
corner and one in the ray but black in the dark alike. At `all` monsters keep the ray, cut at
the light's reach. The measuring sweep writes `monster` lines to its second CSV.

## Spoilers

The map reads the engine's dungeon, so left alone it would show what the player cannot
know: hidden pads, relays, where a switch leads, chest contents. The spoiler level says
*what* the map shows on the cells fog lets through (fog says *where*). It is the
`map_spoilers` key in `[settings]` of `csb.cfg` and the menu Map ▸ Spoilers:
`0` seen, `1` learned (the default), `2` all. `AutoMap_SpoilerLevel`
(`AutoMapHooks.h`) speaks these numbers; inside `AutoMapScene` the same choice is
`AutoMapSpoilerLevel` (`AutoMapKnowledge.h:14`), where `All` is 0, `Learned` 1 and `Seen`
2, so a smaller value shows more. `AutoMapFog` carries the level and a pointer to the
knowledge (`AutoMapScene.h:652`); `AutoMapScene_BuildDrawList` strips the secrets and its
comment (`AutoMapScene.h:677`) lists what goes at each level. At `all` nothing is changed
from the plain map.

**Seen** shows what the 3D view drew. **Learned** adds what the party found out by playing.
Below `all`:

- A wall face keeps its decoration only once the 3D view has shown enough of it, in the
  light on screen, for a player to notice, and the sight rule sees that face. A tiny switch
  passed on the side stays off the map; faced from the tile in front it is seen. Every decoration looks alike (a plain picture, no side strip, no link
  badge) until the party has used that actuator. With fog off, every face counts as seen.
  The 3D view records faces of stone only, so a door's side (its button, its lock) counts as
  seen once the floor cell in front of it is explored, as at `all`.
- The rock the reader adds around a level (`ADDED ROCK: OUTSIDE THE LEVEL`) is the rock the
  engine makes up when the 3D view looks past the edge, random decorations included.
  `AutoMapExplored` keeps that ring of cells, the level's rim, like the level's own: explored
  once the 3D view painted it in the light, and its faces, so a ring on it is noticed and kept
  like any decoration. Below All the rock shows once explored; two tiles to the side of the
  party the floor beside it is explored but the rock is never drawn. At All it also shows beside
  explored floor.
- Hidden machinery (pads, relays, counters, gates, generators, hidden pits, invisible
  teleporters, false walls) is left out, or drawn as floor or wall, until a fact exposes it.
  A concealed pit reads as floor in the 3D view open or closed, so it is drawn as floor
  either way until the party knows it (fell through it, or saw something fall); a sighting
  does not refresh its open state until then, and its toggling teaches no link.
  A spinner whose field the 3D view draws looks like a teleporter there, so the map draws it
  as one (`spinnerIsVisible`) until it turned the party; an invisible spinner is floor till then.
  A learned pad or generator then stays on its explored cell with its links, whether or not
  it is in sight; at `all` every floor actuator does, so `all` never shows less than `learned`.
- A cell is drawn the way the party last saw it, not the way it is now: a door shut or
  open, a pit open or closed, hidden or not, the floor picture, the cell's kind
  and the picture on each wall side (`AutoMapCellMemory`, applied in
  `AutoMapScene_BuildDrawList` through `AutoMapFog::cells`). Every engine tick
  `AutoMapRecorder::SeeCellLooks` refreshes the cells of the last 3D frame that the frame
  explored (any sliver in the light) or that the party can tell (`CanTell`), reading single
  cells of the loaded level only (`AutoMapLevelReader_CellLookAt`); a wall's side picture
  refreshes only for the faces the 3D view draws (`AutoMapSight_FacesShownOfCell`, from the
  same table as the decoration placements: never the front of the cells front left and front
  right, never the sides of the cells beside the party).
  While the party sleeps or a character sheet is open only its own cell refreshes. A frame
  cell explored without being looked at in the light (a teleporter shimmering in the dark, a
  wall whose alcove item stood out) takes its look on that tick; any other cell explored
  outside a frame takes it when the map is next built (`AutoMapScene_RememberExploredLooks`).
  Forgetting a level or the dungeon forgets its looks too. So a trap that opens out of sight, or a door that
  an alcove opens elsewhere, keeps its old look on the map until the party sees it. At
  `all`, with Explored, live or with the fog off, the map draws the live cells as before. The looks are kept in
  the `.map` file (version 8).
- A breakable door looks like any other door; the break force is never shown.
- Chest contents need the chest opened, scroll text the scroll read; monsters show nothing
  they carry. A chest in an alcove counts too: the reader tags each item it holds with the
  chest (`AutoMapAlcoveEntry::inChest`), and the alcove's tooltip, its stocked dot and the
  item search leave those items out until the chest was opened.
- Tooltips show no pad condition, no `LAUNCHES` line and no `SEALED`. A link's tooltip is
  only `TARGET: L<level> (<x>, <y>)` (with ` DOOR` where it applies) and
  `ADDED ROCK: OUTSIDE THE LEVEL`. A learned link draws its arrow with an open head (two
  strokes, neither the filled SET head nor the hollow CLEAR one) and no door-preview ghost:
  the party saw the target change, not which signal did it. The legend's `SET HOVER` row is
  therefore shown at `all` only; `LINKED` stays from `learned` on.
- What a press really does is `all` only too (`AutoMapScene_PressEffects`). Hovering an
  actuator walks its message down the relay chain (counters, gates, DSAs, loops) through
  explored cells only, and the tooltip shows a `THIS PRESS` block (the receivers it reaches
  first) and a `PUZZLE` block (every goal behind them and how far it is: `OPENS IN 2 MORE`,
  `1 BIT OFF`, `BLOCKED: COUNTER AT 0`...; a cell with nothing the walk knows to happen
  reads `TARGET ... TOGGLES ON THIS PRESS`). Holding Shift swaps them for the whole tree,
  indented one space per depth, with delays and gate edge modes, and lifts the line cap of
  the press block and of the relay hover. When the footer is taller than the window leaves
  room for, the wheel scrolls it by one line per notch while Shift is held (on macOS, where
  Shift turns the wheel sideways, the sideways scroll does it), instead of
  zooming; the scroll starts over when the hovered cell changes or Shift is let go. One click fires every
  actuator on a face and a step every pad of a cell, so each face (or the pads) is one press;
  a cell with several gets a header per press (`SWITCH (E):`, `PAD:`). At most 10 lines
  show, then `+N MORE (SHIFT)` or `+N MORE`. Hovering a counter or gate presses nothing: it
  lists its relays and `FED BY`. The walk and its lines are in `AutoMapPress.cpp`.
- On the map the same hover lights every link of the chain, not just the first hop, and puts
  a ring and a badge on each goal (relays in between get none): `NOW`, `2 MORE`, `1 BIT OFF`,
  `NEEDS OTHERS`, `BLOCKED`, `HOLD`, `DSA`, `DONE`. Goals that happen now, soon or already are
  yellow, those that wait on something are orange. A hovered relay draws its feeders in blue
  and what it feeds in yellow. Door previews stay forward-only (the first hop). Below `all`
  none of this draws and the arrows stay as described above.
- The legend gives each row the lowest level that can draw it (`LegendRowLastLevel`,
  `AutoMapWindow.cpp:3557`). Rows above the current level are left out, and legend pages
  that end up empty are skipped.

**Measured draws.** Wall decorations and floor items count as seen only when the 3D view
shows enough of them. Each blit is surrounded by a hook pair: `AutoMap_BeforeWallDecorationDrawn`
/ `AutoMap_AfterWallDecorationDrawn` in `DrawWallDecoration`, and `AutoMap_BeforeItemDrawn` /
`AutoMap_AfterItemDrawn` in `DrawSingleObject` (floor items only: not missiles, not alcove
items). `DrawSingleObject` does not know its cell, so `ProcessOnePosition` names it first
(`AutoMap_OnObjectCellDrawn`). Before the blit `AutoMapMeasure` copies the target rectangle of
the viewport; after it, it keeps the pixels that changed with the colour behind each one, under
the draw's key (`AutoMapDrawKey`: the drawLocation, or the item's record, description and
cell). `AutoMap_OnViewportFinished` runs just before the frame is shown and keeps only the
pixels still on screen, after the nearer cells, a portcullis, the ash or a pile's upper items
were drawn over them, so occlusion needs no rule of its own. A colour alone cannot tell, as a
wall may cover an item in the item's own colours: every blit into the viewport (`TAG0088b2`)
stamps the pixels it writes, from its source's transparency (`AutoMap_OnViewportBlit`), and a
pixel a later blit stamped is not kept. It scores them in the palette on screen, the debug
light included, against their surroundings (below). With `map_timing`
set, each new view's decorations are printed as `drawLocation:contrasting of kept patch largest`, and its
items on a line of their own as `record@x,y:visible of kept patch largest`, and its monsters
on another as `record pos positions:visible of kept patch largest`, each with a picture of
the frame (`automap-decorations-<start>-<n>.png`, `automap-items-<start>-<n>.png`,
`automap-monsters-<start>-<n>.png`, indexed
and uncompressed: the frame as shown, with the pixels marked, magenta those that count, cyan
those that do not, yellow those drawn over, and the background they are judged against).

- A decoration is seen from `AUTOMAP_SEEN_DECORATION_PIXELS` (8) contrasting pixels
  (`AutoMapSight_ContrastingPixels`, tested): pixels whose brightness differs by
  `AUTOMAP_DECORATION_CONTRAST_STEPS` (2) steps from the median brightness of the wall within
  two pixels around them, under the decoration what it covers. It is also seen, like a floor
  item, from a patch of `AUTOMAP_SEEN_DECORATION_PATCH` (8) touching pixels whose colour the
  wall within two pixels does not show (`AutoMapSight_ShapeOfItem`): a dark palette has only a
  few steps of brightness, and a tint tells what they cannot. A wooden board seen from the side
  at light 4 is dark red 1,0,0 on a wall gone black, no pixel two steps off, a patch of 38;
  straight ahead it makes 258, a crack at the foot of a wall at light 4 makes 14; at light 5
  every one measured made 1 or less. Switches are made of the
  wall's own greys, so their colours alone cannot tell them; their contrast can. A black ring
  in the dark covers lit specks of a black wall and does not contrast. Measured: everything
  noticed while walking past had 10 or more, everything missed 4 or fewer. On a wall the
  frame's light did not reach, a decoration seen this way is a glimpse
  (`AutoMapExplored::MarkGlimpsedFace`): below All the map draws a small "?" on that side of
  the wall (`AutoMapDrawList::glimpsedFaces`), the wall itself still in the fog, until the face
  is seen in the light. Small switches never contrast enough to be glimpsed; the yellow of an
  altar of Vi in the dark does.
  `AutoMapRecorder::MarkDrawnDecoration` turns the draw location into a level cell and a side
  and sets the face bit only when the view cell was explored in that frame and drawn as a
  wall. The champion portrait straight ahead counts at once: `DrawWallDecoration` calls
  `AutoMap_OnWallDecorationDrawn` when it puts it on screen. Wall text is drawn as words only
  on the wall straight ahead, and from afar as scribbles: when the words are on screen
  (`AutoMap_OnWallTextRead`) the face counts as seen and, if the light reaches it, its text as
  read (`AutoMapExplored::MarkTextRead`). Below All the tooltip shows the words of read texts
  only; a face seen from afar shows that writing is there. Likewise the 3D view draws what lies
  in an alcove only on a wall facing the party, so an altar of Vi seen front right shows no
  scroll. The items in an alcove are measured like floor items, in the light or beyond its
  reach, against the alcove they cover (`AutoMap_AfterAlcoveItemDrawn`,
  `AutoMapDrawAlcoveItem`): one with a patch of `AUTOMAP_SEEN_ITEM_PATCH` standing out, such as
  a scroll in an altar of Vi two ahead at light 5, makes the wall explored and the face seen and
  looked into (`AutoMapRecorder::SeeAlcoveContents`), so the "?" turns into the altar with its
  scroll; a black club on the altar in the dark, camouflaged even right in front of the party,
  leaves the alcove not looked into. What the party puts into an alcove itself it knows, as it
  knows what it dropped: the face is seen and looked into at once (`AutoMap_OnItemPutIntoAlcove`,
  `AutoMapRecorder::PutIntoAlcove`), in the dark too. The
  items line of the trace lists them as `alcove record@x,y:visible of kept patch largest`, in
  the items' picture. Below All the map shows the contents of looked-into alcoves only; the
  alcove itself shows once its face is seen.
- A floor item is judged by the pixels that stand out from what is around them on screen
  (`AutoMapSight_ShapeOfItem`, tested): a kept pixel stands out when no background colour within
  `AUTOMAP_CAMOUFLAGE_RADIUS` (2) pixels is its own. The background is the frame without its
  floor items (`AutoMapDrawTally::FrameWithoutItems`): the items of a pile do not camouflage
  one another, as the eye sees the pile against the floor under it. Grey bread by grey stone and a black club on a black floor between its lit specks
  are camouflaged; a brown hilt in the dark is not. The item is seen from a patch of
  `AUTOMAP_SEEN_ITEM_PATCH` (4) such pixels touching one another, as the dim hilt of a sword
  (4) and corn in the dark (5) can be noticed while the pixels of bread mostly behind a wall
  (2) are missed, and goes into the item memory on the cell it was drawn on (`AutoMapItemMemory::See`).
  Seen on a cell the frame's light did not reach (`AutoMapRecorder::IsLitInFrame`), it is a
  mere glimpse: the dark shows that something lies there, not what. Below All the map draws a
  "?" for it (`AutoMapDrawList::glimpsedItems`), on an unexplored cell too, whose floor stays in
  the fog; once seen in the light, the item replaces the "?", and a later glimpse does not turn
  it back. Wall decorations have glimpses of their own (above). An item the memory already holds on that cell,
  seen and not glimpsed, is not measured again, so after the first look a hoard costs nothing.
- A monster group is judged by the item rule's pixels that stand out
  (`AutoMapSight_ShapeOfItem`), against what each pixel covers, but needs no patch: it is seen
  from `AUTOMAP_SEEN_MONSTER_PIXELS` (2) of them, all members together. The decoration's
  contrast rule misses monsters, as a lit body or bright eyes on the black of the dark stand
  out by their colour, not by a brightness far from the median. Measured in palettes 3..5:
  every monster noticed stood out with 2 or more (two orange eyes of a demon or a rive in the
  dark: 2; a Giggler's light blue hand: 10; an Oitu: over 300); a single bright eye pixel (1)
  was barely noticed; a Rock monster and worms at light 5 and a Vexirk at light 4 (0) were
  missed, though the older brightness-step count gave that Vexirk 16.
- The party's own moves teach the memory without a measurement. Picking an item up
  (`Mouse.cpp`, after `MoveObject`) forgets it; dropping one (`DropObject`) or a champion's
  death (`DropAllPossessions`) remembers it on the cell it lies on, but only if it stayed on
  that floor: an item dropped into a pit or an alcove is forgotten.
- Every frame, `AutoMapRecorder::RecordViewport` looks at the cells it explores (after the
  light's cut, on the level, not stone) and forgets each remembered item that is no longer
  there or whose record now holds another kind of item (`KeepOnlyItemsStillOn`). Beyond the
  light's reach the map keeps it.

The decoration threshold comes from measurements in the game, with the `MEASURE` sweep and the
player's own eyes:

| Seen from the side, one tile ahead | Visible pixels | Noticed |
|---|---|---|
| Tiny switch, full light | 3 | only when looking for it |
| Small switch, full light | 4 | no |
| Big switch, light 3 | 9 | only when looking for it |
| Crack switch, light 3 | 11 | yes |
| Big switch, full light | 15 | yes |
| Lever, torch holder | 24 and more | obviously |

"Only when looking for it" does not count: a player running past would not see it. Faces
recorded under the older rule (any drawn decoration) stay seen.

The item threshold is still to be measured in the same way:

| Draw | Light | Visible pixels | Noticed |
|---|---|---|---|
| Club, near half of three ahead | 0..5 | | |
| Club, one ahead | 4, 5 | | |
| Dagger or coin, near half of three ahead | 0..3 | | |
| Key under ash, L13 | 0 | | |
| Item behind a portcullis | 0 | | |
| Monster on the diagonal past a wall | 0 | | |

In-game checks for monsters: a monster measured on a cell the map has not explored (e.g. a
Screamer three ahead at light 5) shows over fog; check that this looks right.

**Cost.** `map_timing` also prints `Automap: measuring took <n> us this frame (<items> item
draws, <monsters> monster draws)`, whenever the rounded value changes. It adds up the time of
the light cut and the gone check (`AutoMap_OnViewportDrawn`), the decoration, item and
monster hook pairs, and `AutoMap_OnViewportFinished`. Look at it with a large hoard in view.

**Record IDs.** A fact about an actuator, chest or scroll is keyed by the record,
`(dbNum << 16) | idx` (`AutoMapLevelReader_RecordId`, `AutoMapLevelReader.cpp:169`). The
position bits of the engine's RN are left out because they change when an item moves within
its cell. Reading `SaveGame.cpp` shows the record indices are written and read back as they
are, so IDs are stable across save and load; this was settled by reading, not by a live
test. Facts are never removed during play.

**Facts.** `AutoMapFactKind` (`AutoMapKnowledge.h:22`) lists them; the numbers are stored in
the `.map` file and never change. The engine calls the hooks of `AutoMapHooks.h`, which turn
them into facts:

| Fact | Learned when | Hook |
|------|--------------|------|
| Actuator | a wall switch fired from the party's click; a pad fired on the party's cell, a cell in sight or the cell ahead (while the 3D view is drawn: not with a character sheet open or the party asleep), **including pads fired by a monster or item in sight** | `Mouse.cpp:599`, `Code11f52.cpp:3982` |
| Lock requirement | the item in hand was the one the actuator compares with (`itemMatches`, worked out before a key is used up); an empty click or a wrong item teaches nothing, even where it fires the actuator (a normally closed lock fires for the wrong item, a constant-pressure one always), and a consumed type-17 lock passes no record | `Mouse.cpp:599` (`itemMatches`) |
| False wall | the party stood in it; clicked the wall ahead and heard no wall sound; or saw it open or close (`AutoMapFalseWallWatch`) | `Mouse.cpp:1201`, `AutoMapHooks.cpp:222` |
| Pit | the party fell or climbed down a rope, or something fell on a cell in sight | `Code11f52.cpp:3092` |
| Teleporter or spinner | it moved or turned the party | `Code11f52.cpp:2979` |
| Launcher, generator | it fired out of a face in sight; a visible monster really appeared on a cell in sight (no fact when `CreateMonster` fails or the generator makes invisible monsters) | `Timer.cpp:2368`, `1800` |
| Chest opened, scroll read | the party opened or read it | `Character.cpp:3569`, `3562` |
| Link | see below | `AutoMapHooks.cpp:226` (`LearnWatchedLinks`) |

Rope climbing goes through the pit branch of `MoveObject`, so it reaches
`AutoMap_OnFellThroughPit` with `isParty` set. The map's own debug teleport (the pretend
teleporter of `TeleportParty`) teaches nothing: the hook is skipped for it.

**Links.** A switch often reaches its door through hidden relays. When a known actuator fires,
`AutoMapLinkWatch::Start` snapshots every cell its messages can reach: the cells its links
name, then the cells the links from those name, and so on. Each is watched until now plus
the sum of the delays along its path plus `AUTOMAP_WATCH_SLACK_TICKS` (12) ticks. Each engine
tick `Observe` compares the watched cells with their baseline; one that the eye sees change
becomes an `AutoMapFactLink` from the source actuator straight to that cell, relays left out.
A cell counts as seen on a tick by the same rule as the remembered looks
(`AutoMapRecorder::SeenThisTick`): the party's cell, the cell ahead, and every cell of the 3D
frame that the frame explored or that `CanTell` accepts, so a floor tile front left of a wall
ahead counts although the ray to its centre grazes the wall. A change counts only when it is
visible (`AutoMapScene_SeenToChange`): a concealed pit's open or closed state is invisible
until the party knows the pit, so its toggling teaches nothing before that, and teaches like
any pit after. With `map_debug = 1` each learned link prints one line to stdout: the source
record and cell, the target in global coordinates, the tick the watch started, the tick it
was learned and the sight rule that saw it. Nothing is added to the engine's timers, so the
save format is unchanged. A fired actuator that changed nothing in sight still counts as
known but teaches no link.

## Player notes

The player can mark what they guess the map does not show: hidden pits, and links between
cells. A right-click on a cell the map shows (one in the draw list, so never a cell hidden by
the fog) opens a menu. With `map_debug = 1` its testing rows (TELEPORT HERE, SPAWN ...) come
first, then a separator. The player's rows (`AutoMapWindow.cpp`, `DebugMenuAddNoteRows`):

- on a tile drawn as plain floor: **HIDDEN PIT (OPEN)** and **HIDDEN PIT (CLOSED)**;
- on any tile that carries a mark: **REMOVE MARK**. Once the map has learned a marked tile to be
  a pit (the party fell through it, or saw something fall), the tile is drawn as that pit with
  the mark on top, and REMOVE MARK takes the guess away so the factual pit shows alone;
- on a floor tile or a wall: **LINK FROM HERE**. On a wall the source is the face nearest the
  pointer (`AutoMapScene_NearestSide`, tested; ties go north, east, south, west), so one wall
  can carry a link set per face.

The rim rock the reader adds around a level (`ADDED ROCK: OUTSIDE THE LEVEL`) takes no notes:
the menu offers no note rows there and link mode cannot pick it as a target
(`AutoMapScene_CellTakesNotes`), since the .map file keeps only cells of the level. The hooks
refuse such a cell as well (`AutoMap_SetPlayerPit`, `AutoMap_TogglePlayerLink`).

**LINK FROM HERE** enters link mode: the hint `LINK MODE: CLICK TARGETS, ESC TO END` shows at
the top of the map, the source and its targets are ringed in the player colour, and each
left-click on a shown cell adds it as a target or takes it off again (a click on the source
cell itself does nothing). The buttons and panels
keep their clicks. Escape (before the search box), a right-click or a change of the shown level
ends it.

The notes are drawn in the player colour, violet `#c77dff` (`PLAYER_NOTE`), apart from the
link yellow, the feeder blue and the warning orange. They are drawn at every spoiler level, on
top of the cell, which they never change, so at `all` a guess sits beside the truth:

- a mark is a dashed frame a little inside the cell, plus a centre dot when open;
- a player link draws on hover like a learned one: an arrow with an open head (it claims no
  signal), the far end ringed, offset beside a learned link between the same two cells
  (`ParallelOffset`); a target gets the link badge in the bottom-left corner, across from the
  learned badge;
- the tooltip ends with `YOUR MARK: HIDDEN PIT OPEN` (or `CLOSED`) and one
  `YOUR LINK: L<level> (<x>, <y>)` per target of the hovered source
  (`AutoMapScene_PlayerNoteLines`).

A player link shows only while both its ends are shown. The map never checks a note against
the dungeon (no colour change, no snapping, no rejection), and no press, puzzle, relay or chain
logic reads one. The window changes the notes through `AutoMap_SetPlayerPit`,
`AutoMap_TogglePlayerLink` and `AutoMap_ForgetPlayerNotes` (`AutoMapLevelReader.h`), in level
cells of the shown level, and the map is filtered anew at once. The notes are kept in the
`.map` file (version 9). FORGET `LEVEL`, `DUNGEON` and `LEARNED` leave them alone; FORGET
`MARKS` in the debug panel clears them all.

## Debug placement

A testing aid, off by default: with `map_debug = 1` in the `[settings]` section of
`csb.cfg`, the right-click menu of the map window gets, below the player's rows and a
separator, **TELEPORT HERE** and one **SPAWN** row per monster type the shown level lists in
the dungeon data (`AutoMapLevelReader_MonsterTypesOfLevel`). Only those types may
live on a level (`MonsterAllowedOnLevel`), so no other is offered. These rows work
on any level the window shows and on any cell, fog or not; a cell the map does not show
offers them alone.

Only a cell the party could stand on takes either: no wall, closed false wall,
door that is not fully open or broken, monster or the party itself. A false wall that is
drawn as stone but walked through (an imaginary wall) is no wall here
(`AutoMapScene_Placement`, tested). A refused cell names the reason in the menu
title and greys the rows out. The window only queues the request
(`AutoMap_RequestTeleport`/`AutoMap_RequestSpawn`); the next `AutoMap_OnEngineTick`
checks the cell again and carries it out inside the game loop:

- The teleport is the `TeleportParty` DSA operation: a pretend teleporter under
  the party with a relative turn of zero, so the facing is kept and pads, pits and
  teleporters at the destination act as usual. The map goes back to following the
  party.
- The spawn is a group made by `CreateMonster` the way a generator makes it, with the
  level's health multiplier, but of a size, at a spot and with a facing chosen in the menu.
  After the monster row, the menu asks how many, up to as many as share a cell
  (`AutoMapScene_LargestGroup`, tested: 4 of a quarter cell, 2 of half of it), which fills
  it; a group stands where `CreateMonster` puts it. For one monster alone it asks for the
  spot (`AutoMapScene_SpawnSpots`, tested): a monster
  of a quarter cell takes the centre or any quadrant, one of half a cell the centre or a
  half, one of a whole cell only the centre, which skips the question. Then it asks for the
  facing, or any. A half-cell monster lies along its facing, so in the west or east half it
  faces north or south, in the north or south half east or west (the engine reads its half
  from one quadrant and the facing, `OrdinalOfMonsterAtPosition`). A spot off the centre
  is set with `SetMonsterPositionByte` once the group exists. Its own AI moves a lone
  monster about its cell now and then, back to the centre or to a neighbouring quadrant,
  unless the monsters are frozen.

The same switch adds a **+** button beside **?** and **/**. It opens a panel with one row
per champion and the level of each base skill (`FIG`, `NIN`, `PRI`, `WIZ`); a click on a
skill gives that champion exactly one level in it. Legend, search and this panel share the
corner, so opening one closes the others; Escape closes it. A skill is greyed out for a
dead champion, while the party sleeps (the engine counts every skill as level 1 then) and
at the experience cap of `0x10000000`, which ends at level 21. The window only queues the
request (`AutoMap_RequestLevelUp`); the next engine tick checks again and adds exactly the
experience the next level needs (`AutoMapScene_ExperienceToNextLevel`, tested) through
`AddToSkill`, in parts of at most 65535 because the engine adds a 16-bit number. The last
part calls `LevelUp` as in play: the statistics rise by the usual random amounts and the
"JUST GAINED A ... LEVEL!" line is printed. Only the base skill gains experience, not its
hidden sub-skills.

Below the champions the panel has two more rows. **LIGHT** shows the 3D view in another
palette: `REAL n` is the party's own light (`d.CurrentPalette`, 0 brightest to 5 dark), and
`0` to `5` show that palette instead. Only what is shown changes. The vertical blank asks
`AutoMap_ShownPalette` which palette to copy into the viewport palette (`CSBCode.cpp:1022`),
while game logic keeps reading `d.CurrentPalette`, so monsters still see the party by the
real light (`Monster.cpp:580`) and torches and light spells keep working on it. Darkness
lives entirely in the palette: the far rows of the view are drawn in darker colour entries
that the dark palettes turn black, so a chest three tiles ahead vanishes in palette 5 and
the override shows exactly that. The choice stays until `REAL` is clicked again and is never
saved. **MONSTERS** `FROZEN` keeps the game's own Freeze Life (`d.freezeLifeTimer`) from
running out: `AutoMap_OnEngineTick` sets it to 255 before the main loop counts it down
(`CSBCode.cpp:697`), so every monster that is not invincible skips its moves and attacks
(`Monster.cpp:3010`) and stays put while the light or the view is tried. Freeze Life only
holds the party's level: a monster elsewhere takes a random step before the engine looks at
it, so while frozen `InitialChecks` skips that step too, and every level stands still. `MOVE` ends the
freeze at once, a real Freeze Life included; a game saved while frozen keeps up to 255
ticks of it. **FORGET** clears what the map remembers, so visibility and learning can be tried
again on cells already seen: `LEVEL` forgets the explored cells, seen wall faces and remembered
floor items of the level the map shows (`AutoMapExplored::ForgetLevel`,
`AutoMapItemMemory::ForgetLevel`, tested), `DUNGEON` those of every level, and `LEARNED` every
learned fact and the running watches. None of the three touches the player's notes; `MARKS`
clears those. The map changes at once; the `.map`
file only when the game is saved, so loading without saving brings the old map back.
**MEASURE** `WALLS` measures how visible the wall decorations and floor items of the party's level are
(`AutoMapMeasure.cpp`). On the next tick the engine's own `DrawViewport` draws the view from
every floor cell in all four facings. Every decoration `DrawWallDecoration` draws is
compared pixel by pixel with the viewport before it was drawn, and the frame is checked
again just before it is shown (`AutoMap_OnViewportFinished`, right before
`FloorAndCeilingOnly` clears it), so only the pixels left after the nearer cells drew over
them count. Each draw becomes one line of `automap-walls-L<level>.csv` in the working
folder: the wall face, the party's cell and facing, the view spot (`drawLocation`), and the
kept pixels as pairs of the colour behind and the colour drawn. Every floor item draw becomes
one line of `automap-things-L<level>.csv`: `level,partyX,partyY,facing,kind,record,descIndex,
cellX,cellY,kept,pairs`, with `kind` `item` or `monster` and the cells in the map's coordinates (monster lines have
descIndex, cellX and cellY -1); the sweep
measures every item, remembered or not. These are colour indices, before the palette, so one
sweep serves every light. The sweep teaches the map nothing and
redraws the party's own view at the end.

## Tests

```
make -C src test
```

runs `automap_tests/TestMain.cpp`, which calls `TestAutoMapExplored`,
`TestAutoMapKnowledge`, `TestAutoMapSight`, `TestAutoMapFile`, `TestAutoMapScene`,
`TestAutoMapSpoilers`, `TestAutoMapLinkWatch`, `TestAutoMapVision` and `TestShotMissiles`
and prints `All tests passed`. The tests live in `src/automap_tests/`; the
largest, `SceneTest.cpp`, covers the draw-list and tooltip rules.
