# Changelog

What changed for the player in each release, newest first. Every commit that changes what the
game, the map, the statistics, the replay or the calculator does adds a line under
**Unreleased**; at a release the section is renamed to the version, and the release notes and the
forum post are written from it. Internal changes (refactoring, tests) do not belong here.

## Unreleased

**Statistics**
- Telefrags and landings: a teleport that ends on a square with monsters, or a fall down a pit onto them, kills the whole group at once. Exploration counts them under Teleports and Pit falls ("onto monsters").
- Monsters has a new top number, Monsters killed, with the six causes that add up to it: Champions, Crushed by doors, Fell into pits, Telefragged, Landed on and Other causes. Telefrag kills are no longer counted a second time in Other causes.
- The Monsters killed table has a Telefrag and a Landed column, so you see which monsters died that way and how many (kills from older saves stay under Other).
- A new look: the title strip is gone (who, the levels and Export as HTML share one row), the headline numbers stand in a strip without boxes, the Numbers tables are grouped under small headings (Dealing damage, Attacks, Mana in, Mana out ...) with parts behind a guide line, and the numbers only the party has stand in panels on Exploration (Movement, Travel, Time, Discoveries).
- The champion cards of the Overview call Experience "Exp", so the bars get the room; the bars of all cards are equally long.
- A table too wide for the window gives up, one after the other: its bar, the champions' names in the header (their coloured dots stay; hover for the name), half its padding, the worked-out columns, the label's single line, and the long form of big numbers (86k, 1.2M; hover for the number). Only then does it scroll sideways.
- A dotted line under a label says that it has a hovertext.
- The HTML export has the same new look, groups, panels, narrow-width steps and hovertexts (in its own tooltip box, with the dotted line).
- The tiles of Monsters are Monsters killed, By champions, Telefragged and Landed on; a `csb.cfg` that still holds the earlier default (`kills, door_kills, pit_kills, other_kills`) gets the new one. New names for `csb.cfg`: `monsters_killed`, `telefragged`, `landed_on`, `landings`.
- Hovertexts only where the label does not say it: the ones that repeated their label are gone; new ones say what is in Damage taken, Hit rate, Damage per hit, Friendly fire caused, Resurrected, Eaten, Teleports and Pit falls.
- New number Monster bumps (Exploration): moves that were stopped by a monster on the next square, next to Wall bumps.
- A champion's card on the Overview shows Experience instead of Healed (too close to Damage taken); Healed is still in the Key numbers.
- During a replay, a click on a value in the Records table goes to that tick in the game window and pauses there, as if typed into its "pause at" box.
- The Party column of the Numbers tables is called Total (it adds up the champions).
- The big numbers and names in the tiles at the top of a tab (Champion, Experience, Whole run ...) are drawn sharp instead of blocky.
- The summary on an Overview tile (Records: biggest melee hit, biggest spell in all ...) wraps onto a second line instead of being cut off at the tile's edge.
- Tooltips on rows, column headers and tiles say what a value counts and what it does not (Food wasted, kills by doors and falls, Tries against Attacks ...); they wrap instead of being cut off and have an edge that sets them apart from the rows.
- The HTML export has them too: the column headers and the tiles show the explanations the rows already had.
- The HTML export fills the browser window: the panel takes the window's full width and height (it stopped at 1500 x 680 pixels before), and only its contents scroll.
- Champions: a weapon shows its damage and kinetic energy, a bow or sling its kinetic energy and shoot damage, armor its strength and sharp resistance, under the pointer and in the list (new columns Damage, Kinetic, Armor; Weight is now the last column). The values are the dungeon's own, so a custom dungeon shows its own.
- Champions: each attribute has a bar against the best champion of the party; a drained part is hatched in red, a raised one continues in gold.
- Champions, Party: in a window big enough for it (about 1300 x 980 with four champions) each card shows the attributes and the whole inventory; a little smaller, the inventory alone.
- The HTML export shows the same: item values, the list's new columns, attribute bars, and the Party with attributes and inventory when four cards fit across.
- A climb down a rope onto monsters is counted on Exploration under Pit climbs ("onto monsters"), as a fall onto them is under Pit falls. Saves from before start it at 0.
- Exploration's headline shows Teleports in place of Doors bashed open (a `csb.cfg` with the earlier default follows; your own choice stays).
- Zoomed in (or out), the text is sharp: the letters no longer come out damaged or blurred, and no longer sit unevenly from line to line.
- The window zooms in up to 400 % (was 250 %).
- The Party's cards no longer have the fold listing each poison dose, so a poisoned champion's card is no taller than the others; the champion's own page still lists them.

**Map**
- The monster pictures in a tile's tooltip (and in the HTML export) have their own colours and no coloured backdrop: they were drawn with the wrong transparent colour, so a blue, red or white box showed behind them.
- WALKED also tints the teleporters and pits the party stepped into (it is carried off or falls at once, so it never stood on them for a tick).

**Game window**
- The party position in the top bar reads "L12 (32,43)" instead of "Party 12,32,43" and is right-aligned by its real width, so it is no longer cut off.
- File > Save game under a new name no longer crashes the game when the path of the new file is long ("buffer overflow detected").

## 1.4.0 (2026-11-14)

**Replay**
- Fast forward and go back by 25, 250 or 2500 ticks (buttons, Left/Right, Down/Up, Page Down/Page Up); elapsed and total time on the bar.
- Pause at a typed tick, ahead or behind; a breakpoint or pause that a fast forward runs into stops it.
- Go back after the end, and from the win and death screens; a popup asks before progress is thrown away; an x hides the buttons.
- File > Continue recording: replays to the end at full speed and records on into the same file.
- Nostalgia Mode plays by the rules of CSBwin 17.6, so a recording replays there too.
- More old playfiles replay exactly (reincarnation penalty before 9.6, killing blow and defense adjustment before 9.9; DMMovie8).

**Statistics**
- Champions tab: a card per champion, the inventory as picture or list with icons, weight, charges, wounded slots and chest contents; also in the HTML export.
- Food and water an item gives: in the tooltip and as sortable Food and Water columns (window and export).
- Items with the same name are told apart (MAGICAL BOX (BLUE)); a dead champion's bones carry the champion's name (TIGGY BONES); THE FIRESTAFF (COMPLETE).
- Biggest hit split into biggest melee hit and biggest ranged hit; PROJECTILE and SPELL DAMAGE show the hit points lost.
- Window and HTML export lay out by width; the export keeps its view in the address bar.
- The statistics are an entry of their own in the zipped .map file (format version 26).

**Map**
- Ctrl+click on a stair, or ASCEND / DESCEND in the right-click menu, comes out on the other level.
- Pit style (AUTO, PICTURE, STRIPES) in the Show menu.
- A fluxcage is a yellow shimmering field, also in the tooltip and the export.
- Oh Ew Ra shows what lies beyond the hole; wall switches that rotate the actuator list; teleporters and spinners that take or turn missiles, items and monsters.
- The export's hovered ruler number gets a gold plate; the export's tooltips are about 10x faster.
- The party can stand on a passable illusionary wall (also in the visibility test).

**Game**
- The game window keeps its place on the desktop, like the map and the statistics.
- Reset, Load and Select dungeon work from the menu during the Fusion end sequence.

**Calculator**
- A program of its own in C++ with Qt (replaces the Python one): exact results for hero and monster attacks, spells, recovery, thrown weapons and launcher shots, chance to kill, damage against distance, What if graphs; reads savegames.

## 1.3.0 (2026-11-08)

**Replay**
- File > Record writes every input into logs/RecordNNNNN.log (red REC on the menu bar); File > Playback.. replays it tick by tick with the same fights, deaths and win.
- Pause with Space; nine speeds, 0.5x to 64x and max, with + and - or the Replay menu; from 4x on the sounds are thinned out.
- Pause when the party steps onto a cell.
- CSBwin 7.x to 14.3 playfiles replay with the rules of the version that made them; CSBwin 17.6 playfiles (Antman's DM run) replay to their end on the same tick.
- Stop the replay and play on; no more "End Of PlaybackFile" box; map and statistics fill up during the replay.

**Map**
- A new look, drawn pixel for pixel, with its settings on a bar in the map window and a level list; the game menu keeps only Window.
- Zoom towards the pointer, left-drag pans; WALKED tints the cells the party stood on.
- Monsters stand in formation with facing marks; tooltips group what is on a tile; Shift shows a press's whole route; links are drawn as fans.
- Search finds monsters, generators and stairs; EXPORT writes the map into one offline .html file with level and vision/spoiler limits; Controls help replaces the legend.
- .map files are zipped (about a sixth of the size), format version 23.

**Statistics**
- Two new topics, nine in all: Experience and Records.
- A Champions tab (vitals, ranks, food and water bars, poison, wounds), also in the export.
- New counts and records (poisonings, wounds, items taken out of chests); zoom with Ctrl and the wheel; one row per weapon; csb.cfg chooses the tiles; fixes for poison cloud damage, mana spent and resurrections.

## 1.2.0 (2026-10-31)

**Statistics**
- A Statistics window (Misc > Statistics > Window) that counts while you play, opens by itself on a win and is kept in the .map file.
- Seven topics: Overview, Combat, Magic, Survival, Food & items, Exploration, Monsters; whole run, one level or per level; Party or a champion; a Who bar in every table.
- Hardship (deaths, party wipes, reloads) survives loading an earlier save; Export as HTML writes the window into one file.

**Fixes**
- The menu works after a win and after a loss; Reset, Load and Save are kept out of the end sequence.
- Loading after a win: the next FUSE works again.
- File > Reset clears the map at once; Page Up/Down while zoomed no longer makes the old level jump.
- .map format version 20.

## 1.1.0 (2026-10-29)

**Map**
- Wall decorations sit in a small square against their face (switch back with Small wall decorations).
- Show menu: hide weapons, armour, potions, scrolls, chests, food, keys, other items, monsters, wall decorations, writing or floor decorations.
- Page Up/Down changes levels in the map window, Ctrl+Page Up/Down in the game window.
- Spoils less: floor pictures and stairs, wall writing, champion mirrors and alcove items appear only once the 3D view really shows them.

**Fixes**
- File > Save game no longer crashes on Windows after a new game; it keeps a .BAK backup.
- HINT on the title screen no longer crashes; TrueType save names sit on their buttons again.
- .map format version 12.

## 1.0.0 (2026-10-28)

- The first release of CSBwin with the automap: a live, spoiler-aware map beside the game, with fog of war, explored and whole-level vision, three spoiler levels (seen, learned, all), tooltips for items, monsters, doors and triggers, the full chain behind a switch at spoiler level all, search with / or Ctrl+F, your own notes, and a .map file beside each saved game.
