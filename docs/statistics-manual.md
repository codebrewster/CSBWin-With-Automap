# Statistics user manual

The Statistics window shows what each champion did in the run: damage dealt and taken, kills,
attacks and spells, healing, mana, deaths, and how the party moved. It shows these for the whole
run and for each dungeon level. Like the map, it opens in a window of its own beside the game.
The numbers are saved with your game, so after loading a save you see the run as it was at that
save.

## Contents

- [Opening the window](#opening-the-window)
- [The window](#the-window)
- [Choosing a level](#choosing-a-level)
- [Table](#table)
- [Bars](#bars)
- [By level](#by-level)
- [Monsters](#monsters)
- [Items](#items)
- [Movement (in By level)](#movement-in-by-level)
- [Champion and Party pages](#champion-and-party-pages)
- [Sorting](#sorting)
- [Sharing: export as HTML](#sharing-export-as-html)
- [What is counted, and how](#what-is-counted-and-how)
- [Saving and reloading](#saving-and-reloading)
- [Settings in csb.cfg](#settings-in-csbcfg)

## Opening the window

In the menu bar of the game window, choose **Misc → Statistics → Window**. Choose it again to
close the window, or use the window's own close button. If the window was open when you left the
game, it opens again at the same place and size the next time.

When you win, the window opens by itself and shows the whole run.

The keys keep steering the game while the Statistics window is in front, as they do with the
map.

## The window

- The **title strip** at the top holds the **Export as HTML...** button (see
  [Sharing](#sharing-export-as-html)).
- The **top bar** below it has the five front pages on the left (**Table**, **Bars**,
  **By level**, **Monsters**, **Items**) and the level picker on the right.
- **Champions are columns.** The champions in the party come first, in their slot order. In custom
  dungeons a champion can leave the party; such a champion is shown greyed out after the others,
  and still counts in the Party totals.
- **Party** is the sum of all champions.
- Click a champion's name in the **Table** or **Bars** to open that champion's page. Click
  **Party** for the party's page.

## Choosing a level

The picker on the right of the top bar chooses what the pages show:

- **Whole run**: everything since the game began.
- **A level number**: only what happened while the party was on that level. A level counts once
  the party has stood on it.

With up to 16 visited levels there is one button per level. With more (large custom dungeons such
as Conflux), the picker becomes **◀ list ▶**. The arrows step to the previous or next visited
level, and the list jumps to any of them.

The levels are numbered as the map numbers them, starting at 0.

**By level** lists every level at once, so it has no picker.

## Table

One column per champion and one for the Party. Click a group's title to fold or open it; the
window remembers which groups are open.

| Group | Rows |
|---|---|
| Damage dealt | Melee, Ranged, Magic, Total, each champion's share of the party's damage, Kills |
| Damage taken | Total, Friendly fire caused |
| Healing | Healed, Healing given (potions brewed) |
| Hardship | Deaths; for the whole run also Party wiped out and Reloads (Party column) |
| Mana | Generated, Spent, Wasted on failures |
| Movement | Steps, Tiles walked, Tiles explored, Turns, Wall bumps, Teleports, Pit falls, Pit climbs, Time (ticks), Asleep (ticks), in the Party column |
| Items | Picked up, Used in walls (keys, coins ...), in the Party column (see [Items](#items)) |
| Food and water | Eaten and Drunk, each with one line per item below it (and Fountain under Drunk), Food gained, Food wasted (over full), Water gained, Water wasted (over full) |
| Dungeon | Doors bashed open with smashed, shot open and by spell below it, Doors opened with Zo; Doors opened and closed with their button, Chests opened in the Party column |
| Reading | Scrolls read (each scroll once, the first time it is opened), Wall texts read (the wall texts the map knows as read), in the Party column |
| Champions | Champions seen (portraits in mirrors seen face to face), Champions viewed (mirrors clicked to look at the champion), in the Party column; each mirror counts once |

A row indented further and greyed is a part of the row above it.

Numbers in red are bad news: friendly fire, deaths, wasted mana.

## Bars

The same numbers as horizontal bars, one per champion. They show at a glance who carries the
party:

- **Damage dealt**, split into melee (orange), ranged (blue) and magic (purple).
- **Kills**.
- **Damage taken**, with the friendly fire the champion caused in red beside it.
- **Healing**: healed, and healing given with potions the champion brewed.
- **Mana**: generated, and spent, with the wasted part in red.

## By level

Choose a number with the buttons: **Damage dealt**, **Kills**, **Damage taken**, **Healed**,
**Mana spent**, or one of the party's tables: **Movement** (see [below](#movement-in-by-level)),
**Items**, **Food and water**, **Dungeon**, **Reading** and **Champions** (the Table's groups of those names, one row per
level). For the champions' numbers the table then has one row per visited level with each champion's value, the
Party total and a **Share** bar in the champions' colours. The **Whole run** row stays at the
bottom.

Click a level's name to see that level on the Table page.

## Monsters

Every monster killed on the chosen level (or in the whole run), one row per kind:

- one column per champion: the kills that champion made;
- **Door**, **Pit**, **Other**: monsters crushed by a closing door, killed by a fall, or killed
  any other way (clouds nobody is known to have made, traps, other monsters, the game removing
  them). These columns show only when something was killed that way;
- **All causes**: all kills of that kind;
- **Killed by**: a bar of who killed them, in the colours of the legend below the table.

The **All monsters** row at the bottom adds everything up. Monsters carry the names the dungeon
gives them. Kinds that look alike (variants the dungeon gives no name of their own) share one row.

## Items

The party's items on the chosen level (or in the whole run), one row per item name:

- **Picked up**: items the party took from the dungeon into the hand: from the floor, an alcove,
  a torch holder, an alcove that swaps one item for another, a gift from the gods, or an open
  chest. An item counts only **the first time** the party holds it, so dropping a dagger and
  picking it up again does not count twice. What the champions carried when they joined is not
  counted.
- **Used in walls**: items a wall took: keys in locks, coins and gems in slots, and anything else
  a lock in a custom dungeon takes.
- **Drunk** and **Eaten**: what the champions drank (potions, water from a flask or waterskin) and
  ate. A gulp from an empty waterskin or flask does not count; water from a fountain is no item.

The **All items** row at the bottom adds everything up. The Table shows the totals of Picked up
and Used in walls in its Items group.

The Table's **Food and water** group has, per champion:

- **Eaten**: everything eaten, with one line per item below it (APPLE, CORN, BREAD ...);
- **Drunk**: everything drunk, with one line per item below it (WATERSKIN, WATER FLASK, potions
  ...) and **Fountain** for drinks straight from a fountain;
- **Food gained**, **Water gained**: how much the champion's food and water actually rose, in the
  game's own units (a champion is full at 2048; an apple gives 500, a water flask 1600, a gulp
  from a waterskin 800, a fountain 1600);
- **Food wasted**, **Water wasted**: the part that went over full and was lost. An apple worth 500
  eaten by a champion 200 short of full gains 200 and wastes 300.

The Table's **Dungeon** group counts the doors each champion broke down. **Doors bashed open** is
all of them ("bashed" is the game's own word for a broken door); below it, **smashed** with an
attack, **shot open** with a thrown or fired item, and **by spell** with a spell's missile or its
explosion. Doors broken by monsters or
traps are not counted. **Doors opened with Zo** counts the closed doors a champion's Zo spell
opened: the spell works the door's switch like its button and leaves the door whole, so these are
not in Doors bashed open. A Zo on an open door closes it and does not count. **Doors opened with
their button** and **Doors closed with their button** count the clicks on a door's button, in the
Party column: a click on an open door closes it, a click on any other door opens it. Doors moved by
levers, wall switches, pressure plates or scripts are not counted. **Chests opened** counts each chest once, the first time the party opens it.

## Movement (in By level)

The **Movement** button of By level shows the party's movement instead of the champions' numbers:
one row per visited level and the **Whole run** at the bottom. Movement and the other party
numbers count from the start of a new game, before the first champion joins; the window shows
them as soon as the party is on a level. The Table shows the same numbers
for the level you pick, in its Movement group.

| Column | What it counts |
|---|---|
| **Steps** | Moves you made to another square: forward, back, sideways, and up or down stairs |
| **Tiles walked** | The different squares the party stood on, however it got there; each square counts once |
| **Tiles explored** | The squares the map shows as explored that are not solid wall: floors, doors, stairs, pits, teleporters ... A false wall counts once it is seen to be passable. This comes from the map itself, so it is there for games saved before the statistics existed. |
| **Turns** | Turns left or right you made. Turning while on stairs takes the stairs, so it counts as a step. |
| **Wall bumps** | Moves that ran into a wall, a closed door or a solid false wall |
| **Teleports** | Teleporters that moved the party. Each teleporter in a chain counts. |
| **Pit falls** | Pits the party fell into. Each pit of a chain counts, on the level of the pit. |
| **Pit climbs** | Pits the party climbed down with a rope (its CLIMB DOWN action), so nobody was hurt. Pits further down the same shaft are falls. |
| **Time (ticks)** | Game ticks spent on the level, by the game's own clock. How long a tick lasts in real time depends on the game speed. |
| **Asleep (ticks)** | The part of that time the party slept |

Only what you did counts as steps and turns. Falling, climbing down, being teleported or being
moved by a script is not a step.

Click a level's name to see that level on the Table page.

## Champion and Party pages

Click a champion's name (or **Party**) on the Table or the Bars. The top bar then shows
**< Overview** to go back, and a button for each champion and the party to switch pages. The
level picker still works.

The page has five sections. Click a section's header to fold or unfold it. The header shows a
one-line summary even when folded. Which sections are open is remembered for all pages.

- **Attacks**: grouped by weapon, with each action (SWING, CHOP, THRUST, SHOOT ...) below it.
  Click a weapon to show its actions. Columns: Tries, Hits, Rate (hits per try), Avg (damage per
  hit), Damage, Kills.
  - Actions with an empty hand (PUNCH, KICK, WAR CRY) are listed under **UNARMED**.
  - Actions that cannot hit are listed too: WAR CRY, CALM, BRANDISH, BLOW HORN, CONFUSE, the
    shields and spells of wands and staffs that make no missile (SPELLSHIELD, FIRESHIELD, LIGHT,
    FREEZE LIFE, FLUXCAGE ...), CLIMB DOWN and the like. Their Tries count how often you used
    them; Hits, Rate, Avg, Damage and Kills show "-". They are left out of a weapon's hit rate,
    so a War Cry does not lower your punches' rate.
  - An item thrown by hand is listed as "*item*, thrown". A shot belongs to the bow, crossbow or
    sling, not to the arrow.
  - Only attacks aimed at a monster count as tries; swinging at an empty square does not.
- **Spells**: one row per spell, all power levels together (Lo Ful Ir and Mon Ful Ir are both
  "Fireball"). Columns:
  - **Casts**: every attempt, successful or not;
  - **Successful**: casts that worked;
  - **Failed**: casts that failed (needs more practice, fizzled, no flask for a potion ...);
  - **Mana**: the mana of the successful casts;
  - **Wasted**: the mana lost on failures;
  - **Damage** and **Kills** done by the spell, including the cloud or explosion it made.

  Extra rows: runes that form no spell ("Ful Bro Ku (no spell)"), **Runes cleared** (runes taken
  back with the arrow; the game does not refund their mana), **Shield action**, **Heal action**
  and **Spells from items** (staffs and wands).
- **Damage taken**: one line per source. **Monster attacks** are the blows of monsters (claws,
  weapons, bites), all monsters on one line. Spells and missiles that monsters cast are listed by
  what they are and who cast them: **Fireball (Lord Order)**, **Lightning (Vexirk)**, **Poison
  bolt**, **Poison blob**, **Poison cloud** ... Then
  **Friendly fire** (from whom, or "own"), **Poison**, **Pits**, **Walls**, **Doors**,
  **Starvation**, **Exhaustion**, **Traps**, **Harmful potions or food**.
- **Healing**: HP healed by regeneration, the Heal action and potions (from whom, own, or found
  in the dungeon), healing given with potions the champion brewed, and resurrections.
- **Food and water**: what the champion ate and drank, with one line per item (and Fountain under
  Drunk), food and water gained and what was wasted over full; see [Table](#table) for the units.
- **Hardship**: how often the champion died, each death with its cause and level, for example
  "Fireball from Lord Order · Lv 3", "own Fireball · Lv 9", "Fell into a pit · Lv 7". The
  Party page lists every death with the champion's name, and the party wipes and reloads.

The Party page adds up all champions: the same weapon used by two champions is one row.

## Numbers

Numbers above 999 are written with a comma between the thousands, for example 4,261. The
`stats_separator` entry in `csb.cfg` changes that, see [Settings](#settings-in-csbcfg). The HTML
export uses the same.

## Sorting

Click a column's header to sort the table by it. Click it again to reverse the order. The small
triangle shows the column and the direction. Numbers sort largest first on the first click, names
alphabetically. The **Whole run**, **All monsters** and **All items** rows always stay at the
bottom.

Sortable tables: **By level** (Movement too), **Monsters**, **Items**, and on the champion pages **Attacks**
(a weapon's options are sorted the same way below it) and **Spells**.

On By level and Monsters the champions' names are column headers, so clicking one sorts instead
of opening the champion's page. Open the pages from the Table or the Bars.

## Sharing: export as HTML

Click **Export as HTML...** in the title strip. The game writes a file named like
`statistics-2026-10-31-153000.html` into the game folder (the folder the game was started in,
where `csb.cfg` is), and tells you where. Click **Open** in that message to look at it in your web browser.

The file is a copy of the Statistics window as a web page:

- every front page, every visited level, and every champion and Party page, with the level picker,
  folding sections and sortable tables;
- the champions' names, the dungeon file and the date at the top;
- one file with nothing else needed and nothing loaded from the internet. Send it to a friend or
  attach it to a forum post.

It holds the numbers as they were when you clicked; export again for newer numbers.

## What is counted, and how

- **Damage is the number the game shows**: the attack panel, PROJECTILE DAMAGE, SPELL DAMAGE. A hit
  of 90 on a monster with 10 HP left counts 90.
- **Melee, ranged, magic**: melee is a weapon or empty hand at close range; ranged is a thrown
  item or a shot; magic is a spell, including spells from staffs and wands, and the clouds and
  explosions they make.
- **A kill** goes to the champion whose blow finished the monster, whether melee, missile, spell
  or the poison cloud the spell left.
- **Friendly fire**: damage from a champion's missile, spell or explosion that hits the party
  (the caster included). It is not damage dealt. It counts as "Friendly fire caused" for the
  champion who caused it and as damage taken for the one it hurt.
- **Damage taken** is the damage after armour and resistances.
- **Healed** counts HP actually gained, never more than was missing.
- **Mana spent** is taken rune by rune, as the game does; a spell's mana is the sum of its runes.
- Everything is counted on the level the party is on when it happens.
- Stats belong to the **champion**, not to the slot in the party: a champion who moves to another
  slot keeps their numbers.

## Saving and reloading

The statistics are stored in the save's `.map` companion file (the same file the map uses), so
they need no extra step. Loading a save brings back the numbers of that save.

**Hardship survives reloads.** Deaths, party wipes and the reload count stay when you load an
earlier save of the same game, so you cannot hide a death by reloading. Everything else goes
back to what it was at that save.

Saves from before the statistics existed load with empty statistics; counting starts from there.
Saves made before Movement was counted show 0 for the time before; saves made before wall bumps,
pits, sleep and the actions that cannot hit were counted show 0 for those. Saves made before items
were counted show none; from such a save on, the items the champions carry count as already held.
Saves made before eating, drinking, bashed doors and opened chests were counted show 0 for those;
a chest opened before still counts the first time it is opened again. Saves made before food,
water and the way a door was broken were kept show 0 for those; their doors count only in Doors
bashed open. Saves made before tiles walked were kept count them from the first square after loading. Saves
made before scrolls read were kept count each scroll the first time it is opened after loading;
wall texts read, champions seen and champions viewed come from the map and are there for old
saves too. A champion portrait is not a wall text, even though the map marks both the same way.

## Settings in csb.cfg

The window keeps these in the `[settings]` section of `csb.cfg`:

| Entry | Meaning |
|---|---|
| `stats_window` | 1 when the window was open when the game was left |
| `stats_x`, `stats_y`, `stats_width`, `stats_height` | The window's place and size |
| `stats_page` | The front page last chosen: 0 Table, 1 Bars, 2 By level, 3 Monsters, 4 Items |
| `stats_separator` | The mark between thousands: `comma` (4,261, the default), `dot` (4.261), `space` (4 261), `apostrophe` (4'261) or `none` (4261). Edit it while the game is closed. |
| `stats_open` | Which sections of the champion pages are open (1 Attacks, 2 Spells, 4 Damage taken, 8 Healing, 16 Hardship, 32 Food and water, added up) |
| `stats_table_closed` | Which groups of the Table are folded (1 Damage dealt, 2 Damage taken, 4 Healing, 8 Hardship, 16 Mana, 32 Movement, 64 Items, 128 Food and water, 256 Dungeon, 512 Reading, 1024 Champions, added up; 0 none) |
