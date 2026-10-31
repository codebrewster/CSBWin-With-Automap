# Statistics user manual

The Statistics window shows what each champion did in the run: damage dealt and taken, kills,
attacks and spells, healing, mana, deaths, and how the party moved. It shows these for the whole
run and for each dungeon level. Like the map, it opens in a window of its own beside the game.
The numbers are saved with your game, so after loading a save you see the run as it was at that
save.

## Contents

- [Opening the window](#opening-the-window)
- [The window](#the-window)
- [Overview](#overview)
- [Combat](#combat)
- [Magic](#magic)
- [Survival](#survival)
- [Food & items](#food--items)
- [Exploration](#exploration)
- [Monsters](#monsters)
- [Per level](#per-level)
- [Numbers](#numbers)
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

The window has three controls that never move:

- **Who**, the first row on the left: **Party**, then one button per champion with their colour,
  the game's own champion colours (green, yellow, red, blue by party slot). The champions in the
  party come first, in their slot order. In custom dungeons a champion can leave the party; such a
  champion is shown greyed out after the others, can still be picked, and still counts in the
  Party's numbers.
- **The level**, the first row on the right: **Whole run** (everything since the game began),
  **Per level** (see [Per level](#per-level)), or one level number. A level counts once the party
  has stood on it; the levels are numbered as the map numbers them, starting at 0. With more than
  16 visited levels (large custom dungeons such as Conflux) the numbers become **◀ list ▶**: the
  arrows step to the previous or next visited level, and the list jumps to any of them.
- **The topic**, the tabs under them: **Overview**, **Combat**, **Magic**, **Survival**,
  **Food & items**, **Exploration** and **Monsters**. The tabs are the same whoever is picked.

The **title strip** shows what you are looking at ("SYRA · Combat · Lv 3") and holds the
**Export as HTML...** button (see [Sharing](#sharing-export-as-html)).

Every topic is built the same way:

1. **Tiles** at the top: the topic's main numbers.
2. **Numbers**: every number of the topic. For the Party there is one column per champion, the
   Party column, and a **Share** bar that splits the number in the champions' colours. For a
   champion there is their column, the Party column, and **Share of party**, a bar of their part.
   Numbers kept only for the party as a whole (steps, chests opened ...) show for the Party only.
   A row indented further and greyed is a part of the row above it. Hover a number's name for a
   word on what it counts, where there is one.
3. **Detail tables** of the topic (weapons, spells, sources of damage, monsters ...). For the
   Party they have a **Who** bar: who of the champions the line belongs to.

Click a section's title to fold or open it; the window remembers which sections are folded.

Throughout the window:

- a **dim dot** (·) is a zero; a **-** means "does not apply", for example a hit rate without
  attacks;
- a number on a **red pill** is bad news: friendly fire, deaths, wasted mana, food wasted;
- the last row of a table, shaded, is its total.

## Overview

For the **Party**: tiles with the damage dealt, kills, deaths and tiles explored; a **card per
champion** with their share of the party's damage and small bars for damage, kills, damage taken,
healing, mana spent and deaths (each bar against the best champion); and **Key numbers**, one
row from each topic. Click a card to see that champion.

For a **champion**: their damage, kills, damage taken and deaths; a **tile per topic** with its
main numbers (click one to open the topic); and the Key numbers next to the party's.

## Combat

| Number | What it counts |
|---|---|
| **Damage dealt** | with **Melee**, **Ranged** and **Magic** below it (see [What is counted](#what-is-counted-and-how)) |
| **Kills** | monsters the champion's blow finished |
| **Attacks** | blows and missiles aimed at a monster, and the actions that cannot hit; with **Hits**, **Hit rate** (hits per blow or missile) and **Damage per hit** below it |
| **Friendly fire caused** | damage the champion's missiles, spells and explosions did to the party |

**Weapons** lists the attacks by weapon, with each action (SWING, CHOP, THRUST, SHOOT ...) below
it; click a weapon to show its actions. Columns: Tries, Hits, Rate (hits per try), Avg (damage per
hit), Damage, Kills.

- Actions with an empty hand (PUNCH, KICK, WAR CRY) are listed under **UNARMED**.
- Actions that cannot hit are listed too: WAR CRY, CALM, BRANDISH, BLOW HORN, CONFUSE, the shields
  and spells of wands and staffs that make no missile (SPELLSHIELD, FIRESHIELD, LIGHT, FREEZE LIFE,
  FLUXCAGE ...), CLIMB DOWN and the like. Their Tries count how often you used them; Hits, Rate,
  Avg, Damage and Kills show "-". They are left out of a weapon's hit rate, so a War Cry does not
  lower your punches' rate.
- An item thrown by hand is listed as "*item*, thrown". A shot belongs to the bow, crossbow or
  sling, not to the arrow.
- Only attacks aimed at a monster count as tries; swinging at an empty square does not.
- Spells entered as runes are no attacks: they are under [Magic](#magic).

## Magic

| Number | What it counts |
|---|---|
| **Mana generated** | with **Regeneration** and **Mana potions** below it |
| **Mana spent** | taken rune by rune, as the game does; with **Wasted on failures** below it |
| **Spells cast** | every attempt, successful or not; with **Failed** below it |

**Spells** has one row per spell, all power levels together (Lo Ful Ir and Mon Ful Ir are both
"Fireball"). Columns: **Casts** (every attempt), **Failed** (needs more practice, fizzled, no flask
for a potion ...), **Mana** (of the successful casts), **Wasted** (lost on failures), and the
**Damage** and **Kills** of the spell, including the cloud or explosion it made. Extra rows: runes
that form no spell ("Ful Bro Ku (no spell)"), **Runes cleared** (runes taken back with the arrow;
the game does not refund their mana), **Shield action**, **Heal action** and **Spells from items**
(staffs and wands).

## Survival

| Number | What it counts |
|---|---|
| **Damage taken** | after armour and resistances |
| **Healed** | HP actually gained; with **Regeneration**, **Potions** and **Heal action** below it |
| **Healing given** | HP healed by potions the champion brewed, whoever drank them |
| **Deaths**, **Resurrected** | how often the champion died and was brought back |
| **Party wiped out**, **Reloads** | for the Party and the whole run only |

**Damage taken by source**: one line per source. **Monster attacks** are the blows of monsters
(claws, weapons, bites), all monsters on one line. Spells and missiles that monsters cast are
listed by what they are and who cast them: **Fireball (Lord Order)**, **Lightning (Vexirk)**,
**Poison bolt**, **Poison blob**, **Poison cloud** ... Then **Friendly fire** (from whom, or
"own"), **Poison**, **Pits**, **Walls**, **Doors**, **Starvation**, **Exhaustion**, **Traps**,
**Harmful potions or food**.

**Healing by source**: regeneration, the Heal action and potions (from whom, own, or found in the
dungeon).

**Deaths** lists each death with its cause and level, for example "Fireball from Lord Order · Lv
3", "own Fireball · Lv 9", "Fell into a pit · Lv 7". The Party's list names the champion.

## Food & items

| Number | What it counts |
|---|---|
| **Eaten**, **Drunk** | everything eaten and drunk; drinks straight from a fountain count as drunk |
| **Food gained**, **Water gained** | how much the champion's food and water actually rose, in the game's own units (a champion is full at 2048; an apple gives 500, a water flask 1600, a gulp from a waterskin 800, a fountain 1600) |
| **Food wasted (over full)**, **Water wasted (over full)** | the part that went over full and was lost. An apple worth 500 eaten by a champion 200 short of full gains 200 and wastes 300. |
| **Items picked up** | items the party took from the dungeon into the hand, the first time only (Party) |
| **Used in walls** | items a wall took: keys in locks, coins and gems in slots (Party) |

**Eaten and drunk** has one row per item (APPLE, CORN, WATERSKIN, potions ..., and **Fountain**).
A gulp from an empty waterskin or flask does not count.

**Items** (Party only) has one row per item name:

- **Picked up**: from the floor, an alcove, a torch holder, an alcove that swaps one item for
  another, a gift from the gods, or an open chest. An item counts only **the first time** the party
  holds it, so dropping a dagger and picking it up again does not count twice. What the champions
  carried when they joined is not counted.
- **Used in walls**: keys in locks, coins and gems in slots, and anything else a lock in a custom
  dungeon takes.

## Exploration

Most of these are the party's, not a champion's; a champion's Exploration shows the doors only.

| Number | What it counts |
|---|---|
| **Steps** | Moves you made to another square: forward, back, sideways, and up or down stairs |
| **Tiles walked** | The different squares the party stood on, however it got there; each square counts once |
| **Tiles explored** | The squares the map shows as explored that are not solid wall: floors, doors, stairs, pits, teleporters ... A false wall counts once it is seen to be passable. This comes from the map itself, so it is there for games saved before the statistics existed. |
| **Turns** | Turns left or right you made. Turning while on stairs takes the stairs, so it counts as a step. |
| **Wall bumps** | Moves that ran into a wall, a closed door or a solid false wall |
| **Teleports** | Teleporters that moved the party. Each teleporter in a chain counts. |
| **Pit falls** | Pits the party fell into. Each pit of a chain counts, on the level of the pit. |
| **Pit climbs** | Pits the party climbed down with a rope (its CLIMB DOWN action), so nobody was hurt. Pits further down the same shaft are falls. |
| **Time (ticks)** | Game ticks spent on the level, by the game's own clock, with **Asleep (ticks)** below it. How long a tick lasts in real time depends on the game speed. |
| **Doors bashed open** | Doors a champion broke down ("bashed" is the game's own word), with **Smashed** (an attack), **Shot open** (a thrown or fired item) and **By spell** (a spell's missile or its explosion) below it. Doors broken by monsters or traps are not counted. |
| **Opened with Zo** | Closed doors a champion's Zo spell opened: the spell works the door's switch like its button and leaves the door whole, so these are not bashed. A Zo on an open door closes it and does not count. |
| **Opened with button**, **Closed with button** | Clicks on a door's button: a click on an open door closes it, a click on any other door opens it. Doors moved by levers, wall switches, pressure plates or scripts are not counted. |
| **Chests opened** | each chest once, the first time the party opens it |
| **Scrolls read** | each scroll once, the first time it is opened |
| **Wall texts read** | the wall texts the map knows as read |
| **Champions seen**, **Champions viewed** | portraits in mirrors seen face to face, and mirrors clicked to look at the champion; each mirror counts once |

Only what you did counts as steps and turns. Falling, climbing down, being teleported or being
moved by a script is not a step. Movement and the other party numbers count from the start of a
new game, before the first champion joins.

## Monsters

The numbers: **Kills** (by the champions), **Crushed by doors**, **Killed by falls** and
**Other causes** (clouds nobody is known to have made, traps, other monsters, the game removing them).

**Monsters killed** has one row per kind of monster. For the Party: one column per champion,
**Door**, **Pit** and **Other** when something was killed that way, **All**, and a **Who** bar.
For a champion: the monsters they killed. Monsters carry the names the dungeon gives them; kinds
that look alike (variants the dungeon gives no name of their own) share one row.

## Per level

**Per level** in the level picker shows the topic one row per visited level, with the **Whole
run** in the last row. Click a level's row to see that level on its own.

For the Party, **Show** chooses between two tables:

- **Party totals**: the topic's numbers for the whole party, one column each, and a **Who** bar
  that splits the topic's first champion number (damage dealt in Combat, for example).
- **Champions side by side**: one number, picked with the **Number** buttons, with one column per
  champion, the Party, and a **Share** bar. Only the numbers kept per champion that can be added
  up are offered: a hit rate or the party's steps cannot be split by champion.

For a champion, Per level shows that champion's numbers of the topic.

## Numbers

Numbers above 999 are written with a comma between the thousands, for example 4,261. The
`stats_separator` entry in `csb.cfg` changes that, see [Settings](#settings-in-csbcfg). The HTML
export uses the same.
## Sorting

Click a column's header to sort a table by it. Click it again to reverse the order. The small
triangle shows the column and the direction. Numbers sort largest first on the first click, names
alphabetically. The total row always stays at the bottom, and a weapon's actions stay below it,
sorted the same way.

All tables sort except **Numbers** and **Key numbers**, which keep their order so that the parts
stay under their row.

## Sharing: export as HTML

Click **Export as HTML...** in the title strip. The game writes a file named like
`statistics-2026-10-31-153000.html` into the game folder (the folder the game was started in,
where `csb.cfg` is), and tells you where. Click **Open** in that message to look at it in your web browser.

The file is a copy of the Statistics window as a web page:

- every topic for the party and for every champion, every visited level and Per level, with
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
| `stats_page` | The topic last chosen: 0 Overview, 1 Combat, 2 Magic, 3 Survival, 4 Food & items, 5 Exploration, 6 Monsters |
| `stats_layout` | 2: `stats_page` holds a topic. Without it, `stats_page` is read as the front page of earlier versions (3 Monsters, 4 Items; the others open the Overview). |
| `stats_separator` | The mark between thousands: `comma` (4,261, the default), `dot` (4.261), `space` (4 261), `apostrophe` (4'261) or `none` (4261). Edit it while the game is closed. |
| `stats_closed` | Which sections are folded, added up: 1 Weapons, 2 Spells, 4 Damage taken by source, 8 Healing by source, 16 Deaths, 32 Eaten and drunk, 64 Items, 128 Monsters killed, 256 Numbers, 512 Key numbers; 0 none |

Who is picked, Per level and its choices are not kept: the window opens on the Party.
