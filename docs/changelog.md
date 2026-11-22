# Changelog

What changed for the player in each release, newest first. Every commit that changes what the
game, the map, the statistics, the replay or the calculator does adds a line under
**Unreleased**; at a release the section is renamed to the version, and the release notes and the
forum post are written from it. Internal changes (refactoring, tests) do not belong here.

## Unreleased

**Statistics**
- Magic: Spells cast splits as Mana spent does: Successful and Wasted, with Wasted split into Failed and Meaningless (`casts_wasted` in csb.cfg). The Spells table's columns come in two groups, CASTS (All, Successful, Wasted) and MANA (All, Successful, Wasted): its Failed column is now Wasted, as on a "(no spell)" row those casts are meaningless, not failed, and the mana of the casts that worked has a column of its own, to sort by and to split among the champions.

## 1.5.0 (2026-11-22)

**Statistics**
- Exploration counts Formation changes: champions you moved to another place in the party by dragging their portrait, split into Swapped places and To an empty place. Put back in place counts the portraits dropped where they were picked up, and Swapped aside the champions a dragged portrait was dropped on. Kept per champion and for the party, in a Formation table after the doors. Turning the party, recruiting and resurrection don't count.
- Records can be sorted by any column, within each group; a click on Record puts them back in their order. The Value header no longer looks sorted while nothing is.
- Damage reduced by source adds up to Damage reduced: its lines are rounded together, not one by one (1,154, not 1,153).
- An attribute raised above its max is violet, no longer gold next to the yellow champion; gold is only a party record, always with its ★.
- Hovertexts say how the Weapons and Spells tables add up: a staff's or weapon's spells count with the item in Weapons, so Spells from items has mana but no damage, and the two Damage columns together are Damage dealt. Avg says it counts those spells as Damage per hit does, and Mana wasted says why Runes cleared are not in it.
- Every topic has the same layout. The table is always called Numbers, with its group headings shown even when there is only one. The numbers only the party has stand in The party panels on every topic: Wipes and reloads on Survival, Items on Food & items, Door buttons on Exploration. Only the causes under Monsters killed stay in the table, as parts of a sum the champions share in.
- Side by side, the sections keep their order: the first ones fill the left column and the rest the right, so the order no longer depends on the numbers.
- A zero is a dim dot on the headline tiles too, as in the tables.
- Resistance under Damage reduced is greyed in replays of CSBwin 9.8 to 12.5 or before 9.6, which ignored resistance, so it is always 0 there: its row, and its Number button in Per level, whose hovertext (dotted underneath) says why.
- A column header with a hovertext (Total, Tries, Rate, Avg, Damage, Record ...) is dotted underneath, as every label with one.
- Deaths is a table: Who, Killed by, Lv and Tick, newest first and sortable; in a replay a click on the tick goes there. A death's tick is kept from this version on (save layout 36); older games show "-".
- Ranks gained and Records align text the same way (left), and what brought a rank or a record (With, How, On / from) is a lighter grey than a zero or a hint, so it no longer looks disabled; the Tick is plain. The skill notes in Experience by source are no longer faint either.
- Eaten and drunk's Who bar splits a drink by who drank it while the table is sorted by Eaten, instead of staying empty.
- With one champion the pages no longer repeat themselves: no Total, no Share and no Who bars (all would be the champion's), no "100% of damage" on the Overview card, and Per level has no Champions side by side. The Party's Numbers shows the party's own numbers in the champion's column.
- The Party's Experience tiles name every champion at the best rank ("best: Adept (ZEDA, ELIJAB)"), and the tile's hovertext says what best means.
- A champion's headline tiles that are the party's are filled in from the topic's own numbers: Food & items shows Eaten in place of Items picked up.
- One name for one thing: the kill causes are By champions, By doors, Into pits, Telefragged, Landed on and Other causes, as rows and as the Monsters killed table's columns; the protection lost to wounds and sleep is Wound penalty and Sleep penalty; Experience by source says Monster attacks received. Records no longer repeats Poisoned and Wounds as Poison doses suffered and Wounds suffered.
- More hovertexts: every row of Records, the Weapons table's Avg, the Overview cards' short labels (Taken, Exp, Mana), and Healed, Heal action, Food and Water gained (in the game's units), the Doors rows, Pit climbs, Chests opened, Time, Asleep, Party wiped out, Reloads, Monsters killed, Biggest heal, Biggest experience gain, Longest teleport chain and Longest sleep.
- Red means waste or a mistake: wasted mana, food and water, failed and meaningless casts, cleared runes, friendly fire, deaths, wipes and now reloads. Harm is no longer red: Poisoned, Wounds, Hits taken and the poison and wound records are the cost of fighting, like Damage taken. Bad news always sits on a faint red pill, on the tiles, the Overview cards and the Champions tab too, so it no longer looks like the red champion's colour.
- A poison cloud nobody is known to have made is no longer shown as friendly fire. Damage taken by source names the champion behind a cloud or friendly fire on the Party's page too ("Poison clouds (from SYRA)").
- Magic's Mana out rows say they are mana (On spells, On successful casts, On failed casts, On meaningless spells), and Casts counts Meaningless spells apart from Failed, as the mana does. In csb.cfg the new count is `casts_meaningless`.
- A champion's Overview sums up every tile of a topic (Experience names the wizard too), and one of something is singular: "1 death".
- Records: a streak or chain that ran over two levels (kills without a scratch, teleport chains, sleep, poisonings) belongs to the level it began on, like a fall; the Lv column and the levels' pages show it there.
- Ranks gained explains that a monster name under With is a monster attacking the champion (Parry), not a kill.
- Kills without a scratch says how many kills its two monster names leave out: "6 Mummy, 3 Trolin, +1 other".
- Records say what they count: "747 ticks" (longest sleep, longest poisoned), "1 level" (longest fall), "28 teleporters", "13 moves" (stair dances).
- Ranks gained shows the first rank with an arrow like the others ("— → Neophyte"), and a throw from the mouse pointer is called THROW BY HAND, in capitals like the other actions.
- The By champions tile on the Monsters tab says what it is a part of instead of pointing to the tab it is on, and a table's Total header mentions records only where the table holds one.
- Magic splits the mana of spells that went wrong: Wasted under On spells, with On failed casts (needs more practice) and On meaningless spells (runes that form no spell) below it. Wasted is the Wasted of All spells in the Spells table, and the Magic tile shows it. Older saves keep the meaningless spells among the failed casts. In csb.cfg `mana_wasted` is all of it; `mana_failed` is the failed casts alone.
- Magic counts Regeneration lost (mana full): the mana regeneration would have given while a champion's mana was full, or the part a nearly full champion could not take. Spend mana before it fills up and it is not lost.
- Survival splits Potions under Healed by who brewed them: Own brews, Brewed by others and Found. The Healing by source table, which showed the same numbers again, is gone.
- Exploration counts Stair dances under Travel: two stair changes or more with no step away from the stairs in between (fighting from the stairs and turning to get away). A step to a square next to the stairs and back keeps a dance going; Strict stair dances, runs of stair changes that never leave the stairs, are counted beside them to compare; any step off the stairs ends one and the next stair change begins a new one, so one stair dance can hold several strict ones, or none. Each has its Dance moves (the stair changes after the one that brought the party onto the stairs), and Records has the Longest stair dance and the Longest strict stair dance.
- Magic's Spells table has a Successful column: each spell's casts that worked, next to Casts and Failed (also in the HTML export).
- Survival counts the monster attacks each champion received and how they ended: Hits taken, Dodged, Lucky, Too weak and Blocked.
- Survival shows Damage reduced: what resistance, the fire and spell shields, protection and wisdom took off each hit, measured from the damage with no defence at all (no protection about doubles a hit, no Anti-Fire or Anti-Magic adds a third). So resistance never shows below 0, even when a low resistance still leaves more than the roll. Protection is split into armour, shields in hand, vitality, the shield potion, the party shield, stance, wounds and sleep. A new table, Damage reduced by source, splits it by what did the harm.
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
- The last wheel step out, where the level jumps to fill the window, frames the tile the pointer was on (the rest dims, as after a stair), so you find the spot again. The next click or wheel turn ends it; more notches out from a fast spin do not.
- The level list says how much of each visited level is explored (64%): the share of the cells one can stand on that the party has seen. Map window and exported page.
- A level with nothing explored on it says NOTHING EXPLORED ON L10 in the middle, instead of staying black. Map window and exported page.
- The exported page keeps the place in its address: once zoomed in, the zoom and the cell in the middle, so a copied address opens where you were. **LINK TO THIS CELL** in the right-click menu copies a link that opens on that cell with a frame round it.
- SHOW has MECHANISMS and PARTY. MECHANISMS off hides the marks the map adds for what works the dungeon (the who-badges of pads and teleporters, generators, cogs, the dot on what is operated, a switch's bright face line), so the overview is quieter; the cells, pits and the hover lines and tooltips stay. PARTY hides the red arrow. In the map window and the exported page.
- **?** has a SYMBOLS tab, in the map window and the exported page: every mark the map draws, with what it means (cells, walls, pads and who sets them off, generators, items, monsters, the lines and badges of a hover, your own marks), drawn by the map's own code. A page shared with someone who has no manual now explains itself. The wheel or the arrow keys scroll it; a click outside the panel closes it.
- The exported page draws a sleeping generator as the game does, its monster faint in an empty diamond, no longer as a cog, and the search finds sleeping generators too.
- Zoomed out (cells under 20 pixels) a monster group has one facing triangle, the way most of it faces, instead of one per member: in a small cell they piled up at its edges into white blobs. The HTML export too.
- The exported map file is about 6% smaller (Dungeon Master: 985 KB down to 929 KB): the rock added around a level no longer stores its tooltip in every view. The page writes it itself, so hovering that rock still says `ADDED ROCK: OUTSIDE THE LEVEL`.
- The exported page's header is one line of help that points to **?** on the bar, and the line under it that repeated the levels and the party's level is gone. The **?** list no longer says `(CTRL IN THE GAME)`.
- Every door has a tooltip: `DOOR` with `OPEN`, `CLOSED`, `PARTLY OPEN` or `BROKEN` under it, as the party last saw it. A door that switches or pads open had none before. The HTML export too.
- Stairs the party stood on show at spoilers Learned and Seen too, also in the dark: the 3D view shows their rails, so you know you are on stairs. Before, stairs you arrived on but never saw from in front read as floor (DM L1 (3, 15)).
- Stairs have a tooltip: `STAIRS DOWN` with where they come out, `TO L2 (6, 40)`. The HTML export too.
- HTML export: a sideways swipe no longer zooms in, and Shift+wheel scrolls a tooltip in browsers that send it sideways.
- HTML export: the search lists a wall that needs one item in several locks once, as its tooltip does (three `GOLD KEY NEEDS` rows on DM L3 (28, 8) were one).
- A walk-through false wall the party walked takes the explored tint like the floor around it (the maze on CSB L3); solid ones stay dark. The HTML export too.
- A zoomed level can no longer be dragged away from the edge of the window: a drag never adds black space beside the level (it could pull half the window black). Zooming works as before. Dragging back from an edge moves the map at once. The HTML export too.
- Zooming out keeps the spot under the pointer still, however much black space that leaves beside the level, and the level jumps to the centre only at the whole-level view; it used to slide a little on each notch once the black space reached half the window. Zooming in keeps the black space there is. The HTML export too.
- The map window no longer lags when zooming at the closest zoom in a large window: it drew the whole level into one enormous picture (several GB on a 4K screen) on every wheel notch, and now draws only the tiles on screen when the level is that large.
- HTML export: Ctrl+F with the search already open no longer opens the browser's own find.
- HTML export on a phone or tablet: the map fits the screen, one finger pans, two fingers zoom, a tap shows a tile's tooltip and acts as a click; the mouse help is left out on narrow screens.
- One set of verbs per kind of goal, the same in a switch's RESULT and under what it acts on: a door, pit or false wall OPENS / OPENED BY, a teleporter or spinner WAKES / WOKEN BY and STOPS / STOPPED BY (it was "OPENS" and "SET BY"), a generator TRIGGERS and a launcher FIRES, on any signal, with their sources on one TRIGGERED BY or FIRED BY line. Every door, pit, false wall, teleporter, generator and launcher at spoilers All now names what acts on it, not only those that are a pad or switch themselves: the door a pad opens reads `OPENED BY: PAD L1 (5, 18)`. The HTML export too.
- Counts are written one way, after the name: `GENERATES Deth Knight X4`, `LAUNCHES FIREBALL X2`, a route line that repeats ends in `X2`, and a search hit reads `DAGGER X2` with `ALCOVE` at the right (it was `4X Deth Knight`, `2X FIREBALL`, `X2 ALCOVE`). The HTML export too.
- Ctrl+click any tile, a pit too, to see the tile below on the level beneath, framed while the rest of the level dims; Ctrl+click on a stair still comes out of it. The side buttons frame the tile under the pointer on the level they step to, and the right-click menu has ASCEND / DESCEND on any tile with a level above or below. A spot you have not explored on the other level is centred but not framed.
- An object launcher shows at spoiler level All what it will still throw, next shot first, as icons in its tooltip and by name under its `LAUNCHER (S)` header, after `LAUNCHES OBJECT, 8 LEFT`, each chest with what it holds indented under it. Two launchers that take the same items (the one on a face also takes what lies on the next face) list them once: the second says `SHARES 11 WITH (S)` (CSB L3 (5, 11) listed its 11 items twice, above the headers). The search finds these items, tagged `LAUNCHER` and `LAUNCHER CHEST`, and the words of a scroll among them (`SCROLL LAUNCHER`, `SCROLL LAUNCHER CHEST`); the HTML export too. Something in the wall the map has no picture for still counts as a shot. Example: CSB L7 (14, 34).
- A tooltip's chest and monster read as groups: what an opened chest holds is indented under `CHEST`, and a monster's gear sits under its name, after `CARRIES`, `DROPS` (when it dies, for sure) and `MAY DROP`, each name indented; before, they were plain item lines among the floor's. Hit points take one line, `HP 200, 100, 150, 50`, damage too, `DMG 17, 9`. The HTML export too. Example: CSB L7 (26, 7).
- The monster pictures in a tile's tooltip (and in the HTML export) have their own colours and no coloured backdrop: they were drawn with the wrong transparent colour, so a blue, red or white box showed behind them.
- WALKED also tints the teleporters and pits the party stepped into (it is carried off or falls at once, so it never stood on them for a tick).
- An illusionary wall (a wall in the 3D view that the party walks through) is drawn with its purple pinstripes on the dark floor, so it no longer looks like a solid false wall; the HTML export too. The tooltip says whether a false wall is `SOLID`, `WALK-THROUGH` or `OPEN`. The pinstripes of false walls keep their look when you zoom in instead of getting denser.
- The search finds the words of scrolls the party has read and of wall texts, from three letters on (`KING` finds THE GRAVE OF KING FILIUS); a hit shows the words around the match with the match in gold, tagged SCROLL, SCROLL ALCOVE, SCROLL IN CHEST, SCROLL CARRIED or WALL (N). The HTML export too.
- What Shift shows is called the ROUTE everywhere: the `?` help and the export page say `THE ROUTE` (it was `THE WHOLE PRESS CHAIN`), and a cut press list ends in `+3 MORE (SHIFT: ROUTE)`. The export page's long tooltips end in `+3 MORE (SHIFT+WHEEL)`, in the same form.
- The search writes a hit's place as `(10, 15)`, with a space, as the tooltips and menus do (it was `(10,15)`). The HTML export too.
- Fewer engine words in tooltips. A gate says in words whether it matches: `MATCHES NOW`, or the bits still wrong, `NEEDS BIT 1 ON, BIT 0 OFF` (it was `BITS 0001, NEEDS 0011`); a press's `THIS PRESS` line reads `GATE A: MATCHES NOW`. The bit patterns stay in the ROUTE under Shift. `FED BY` rows put a colon and commas between what a feeder does, `PAD: A BIT 0 SET, B BIT 0 SET`, and a switch feeding a DSA reads `SWITCH (S): RUNS A` (it was `A?`). A pad's need names the item first, `NEEDS THE FIRESTAFF CARRIED`, `NEEDS COIN ON PAD`, and a teleporter `KEEPS FACING` or `TURNS RIGHT` (it was `TURN: STRAIGHT`). The HTML export too.
- A relay's `FED BY:` heading is gold, as `GATE A` and the other part headings above it are; it was white like the rows under it. The HTML export too.
- The map's small pixel font has a zero that leans like a slashed one, its top left and bottom right corners cut, so `L0` no longer reads `LO`. On the rulers, the plates and in the tooltips, in the game window and the HTML export.

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
