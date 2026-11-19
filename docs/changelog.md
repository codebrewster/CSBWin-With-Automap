# Changelog

What changed for the player in each release, newest first. Every commit that changes what the
game, the map, the statistics, the replay or the calculator does adds a line under
**Unreleased**; at a release the section is renamed to the version, and the release notes and the
forum post are written from it. Internal changes (refactoring, tests) do not belong here.

## Unreleased

**Statistics**
- Exploration counts Stair dances under Travel: two stair changes or more with no step away from the stairs in between (fighting from the stairs and turning to get away). A step to a square next to the stairs and back keeps a dance going; Strict stair dances, which never leave the stairs, are counted beside them to compare. Each has its Dance moves (the stair changes after the one that brought the party onto the stairs), and Records has the Longest stair dance and the Longest strict stair dance.
- Magic's Spells table has a Successful column: each spell's casts that worked, next to Casts and Failed (also in the HTML export).
- Survival counts the monster attacks each champion received and how they ended: Hits taken, Dodged, Lucky, Too weak and Blocked.
- Survival shows Damage reduced: what resistance, the fire and spell shields, protection and wisdom took off each hit, measured from the damage with no defence at all (no protection about doubles a hit). Protection is split into armour, shields in hand, vitality, the shield potion, the party shield, stance, wounds and sleep. A new table, Damage reduced by source, splits it by what did the harm.
- Exploration shows a level's time as a share of the whole run: "4,321 (12%)" in the Time panel and in Per level's Time (ticks) column, also in the HTML export.
- Food & items shows its new headline numbers in games that had run before: a csb.cfg that still held the old default tiles (Eaten, Drunk, Food wasted, Items picked up) gets the new ones; a list you chose stays.
- Food & items counts Potions brewed: each potion a champion made, per level, in the table under Potions drunk.
- Exploration's Turns explains that a turn on stairs is a Step and takes the stairs.
- Exploration counts Arrivals (each time the party came onto a level, by any way) and Stair changes (each time it went up or down stairs, a turn on the stairs included), under Travel.
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
- Records: Longest fall is a record of the level the fall began on, so its Lv column shows that level ("Lv 6 → 9" for pits on 6, 7 and 8 is Lv 6); the level it landed on is in the text.
- Ranks gained sorts in the window too, and in both the window and the HTML page Rank sorts by the rank reached instead of its name, and a new Tick column sorts back to newest first.
- Food & items: Food wasted and Water wasted are no longer indented under Food gained and Water gained, since they are not part of them.
- Food & items: the headline numbers are Food gained, Water gained, Potions drunk (new) and Items picked up; Eaten, Drunk and Food wasted stay in the table below. Potions drunk counts the potions in Drunk; a Water Flask and a waterskin are water, not potions.
- HTML page: the poison doses show only on a champion's Champions page, folded under "Show the N doses", as in the window; the Party's cards no longer list them.

- Records over all replays: `scripts/export-all-html.sh` writes `records.html` beside the pages, with the Records tab over every replay of each game (Chaos Strikes Back, Dungeon Master): one row per record, for the party or a champion, with the lowest value above 0, the average and the highest, and the replay, hero, weapon or spell and monster behind each.

**Map**
- An object launcher shows at spoiler level All what it will still throw, next shot first, as icons in its tooltip (`LAUNCHES CHEST (N)`), each chest with what it holds, and its LAUNCHER line says how many are left (`LAUNCHES OBJECT, 8 LEFT`). The search finds these items, tagged `LAUNCHER` and `LAUNCHER CHEST`; the HTML export too. Example: CSB L7 (14, 34).
- The monster pictures in a tile's tooltip (and in the HTML export) have their own colours and no coloured backdrop: they were drawn with the wrong transparent colour, so a blue, red or white box showed behind them.
- WALKED also tints the teleporters and pits the party stepped into (it is carried off or falls at once, so it never stood on them for a tick).
- An illusionary wall (a wall in the 3D view that the party walks through) is drawn with its purple pinstripes on the dark floor, so it no longer looks like a solid false wall; the HTML export too. The tooltip says whether a false wall is `SOLID`, `WALK-THROUGH` or `OPEN`. The pinstripes of false walls keep their look when you zoom in instead of getting denser.
- The search finds the words of scrolls the party has read and of wall texts, from three letters on (`KING` finds THE GRAVE OF KING FILIUS); a hit shows the words around the match with the match in gold, tagged SCROLL, SCROLL ALCOVE, SCROLL IN CHEST, SCROLL CARRIED or WALL (N). The HTML export too.

**Replay**
- `csb --export-html` with `--export-ending` writes the pages at the victory screen, not at the replay's last line. `scripts/export-all-html.sh` gives it to the Chaos Strikes Back folders (Chaos Strikes Back minimum item picking, Race to Fulya Pit), so their maps show where the party ends up after the last line. Dungeon Master games still write at the last line.

**Game window**
- The party position in the top bar reads "L12 (32,43)" instead of "Party 12,32,43" and is right-aligned by its real width, so it is no longer cut off.
- File > Save game under a new name no longer crashes the game when the path of the new file is long ("buffer overflow detected").

## 1.4.0 (2026-11-14)

**Replay**
- `csb --export-html DIR --play Playfile.log` writes the map and the statistics of a recording as web pages without opening a window, at top speed and with the ending played, and `scripts/export-all-html.sh` does that for every movie in a folder (all 43 of the collection in about a minute and a half).
- Fast forward and go back by 25, 250 or 2500 ticks (buttons, Left/Right, Down/Up, Page Down/Page Up); elapsed and total time on the bar.
- Pause at a typed tick, ahead or behind; a breakpoint or pause that a fast forward runs into stops it.
- Go back after the end, and from the win and death screens; a popup asks before progress is thrown away; an x hides the buttons.
- File > Continue recording: replays to the end at full speed and records on into the same file.
- Nostalgia Mode plays by the rules of CSBwin 17.6, so a recording replays there too.
- More old playfiles replay exactly (reincarnation penalty before 9.6, killing blow and defense adjustment before 9.9; DMMovie8).
- More old playfiles replay to their end: DMMovie1, 3, 5, 7 and 40 (a full database was enlarged at once to 1023 entries before 9.5; before 9.6 a torch's light level counted other bits of the torch and was only recomputed when a torch burned out, which changes how far monsters see).
- Still more old playfiles replay to their end: CSBMovie9, 10, 11, 14 and 16 (before 8.0 SmartDiscard drew no random numbers; before 9.7 a spliced recording also sets the level timer; before 9.0 a monster's remains are placed by the loaded level).
- DMMovie4 and 6 (9.07) replay to their end too: before 9.2 Lord Chaos fled from the fusion spell by picking one random direction per attempt, as on the Atari.
- CSBMovie4, 6 and 8 replay to their end: the oldest exes (3.3 to 4.x) let a hero regenerate mana, at the cost of stamina, in the opposite ticks to later versions.
- CSBMovie3 and 5 replay to their end: the 3.3 to 4.x exes drew exactly 143 raw random numbers when saving (Save and Play) and did not re-attach the monsters of the level.
- CSBMovie2 replays to its end: the 3.3 to 4.x exes kept the monster queue at 60 entries, so a full queue made a monster generator fail; the 4.1 exe subtracted 0xcd00 too much when a shield potion wore off. Also fixed a crash ("Recursive timer search") when a monster generator queued a timer.
- CSBMovie1 replays to its end: the 3.3 exe set the timer that erases a champion's damage number for the current tick instead of five ticks later (the replay needs CSBwin33.exe next to the playfile to know).
- Playfiles of the oldest versions, which have no header line, no longer lose their first event (the click on Resume): CSBMovie2 and 4 to 8 start instead of waiting at the prison door, and CSBMovie8 replays to its end.

**Statistics**
- Exploration shows a level's time as a share of the whole run: "4,321 (12%)" in the Time panel and in Per level's Time (ticks) column, also in the HTML export.
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
- Exploration shows a level's time as a share of the whole run: "4,321 (12%)" in the Time panel and in Per level's Time (ticks) column, also in the HTML export.
- Two new topics, nine in all: Experience and Records.
- A Champions tab (vitals, ranks, food and water bars, poison, wounds), also in the export.
- New counts and records (poisonings, wounds, items taken out of chests); zoom with Ctrl and the wheel; one row per weapon; csb.cfg chooses the tiles; fixes for poison cloud damage, mana spent and resurrections.

## 1.2.0 (2026-10-31)

**Statistics**
- Exploration shows a level's time as a share of the whole run: "4,321 (12%)" in the Time panel and in Per level's Time (ticks) column, also in the HTML export.
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
