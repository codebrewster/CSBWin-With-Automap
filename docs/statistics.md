# Statistics

The Statistics window is a second OS window, beside the map's, that shows what each champion
did in the run. How to use it is in [statistics-manual.md](statistics-manual.md); this page is
the map of the *code*.

## The unit split

Like the map, the feature is split so that its rules can be tested without the engine or SDL.
The units in `HERO_STATS_TESTED` (`src/makefile`) link into the test program that `make test`
builds; only the hooks and the window touch the game.

**Engine-free, tested** (`make test`):

| Unit | Holds |
|------|-------|
| `HeroStats` | The store: every count, keyed by champion (`CHARDESC::fingerPrint`) and level, with the queries that add them up. `HERO_STATS_PARTY` asks for every champion, `HERO_STATS_WHOLE_RUN` for every level (`HeroStats.h`). It saves and loads itself (`Serialize`). |
| `HeroStatsOrigins` | Who launched a missile, made a cloud or brewed a potion, keyed by the record's index: the engine's records have no room for it. Also the items already held, the chests opened and the scrolls read, so each counts once. |
| `HeroStatsPending` | Damage to a hero is collected and applied later (`DisplayCharacterDamage`); this names the source of the batch that killed. |
| `HeroStatsRunes` | Mana is taken rune by rune as it is clicked; this keeps each rune's cost so the spell's row gets its mana when the spell resolves. |
| `HeroStatsGame` | Decides whether a loaded game is the one in play, so that deaths, wipes and reloads survive a reload. |
| `HeroStatsCatalogue` | **Every number the window shows, defined once**: its label, its help text, the topic it belongs to, and what kind of number it is (party only, whole run only, bad news, a ratio). `HeroStatsCatalogue_Record` fills all of them for one champion and level. The pages, Per level and the HTML export are all made from this list, so a number is either everywhere it applies or nowhere. |
| `HeroStatsView` | Formatting and the window's choices: thousands separator, spell and cause names, the champion columns, the level picker. |
| `HeroStatsPages` | The numbers of each page as the window draws them: a topic's tiles, its Numbers rows and its detail tables (weapons, spells, damage taken, healing, deaths, food, items, monsters), the Overview cards, and the Per level tables. On the Party's page each line also holds every champion's own numbers, for the Who bar. |
| `HeroStatsExport` | The whole window as one HTML file: the catalogue and every page's numbers as JSON, and a script in the file that draws the same pages. |
| `HeroStatsDump` | Every non-zero count as text, printed on each save with `map_debug = 1`. |

**Engine-facing** (link only into the game):

| Unit | Holds |
|------|-------|
| `HeroStatsHooks` | Owns the store of the game in play. The functions the engine calls (`HeroStats_On...`, and `Begin`/`End` pairs that name the source of the damage done in between), and loading, saving and reloads. Include it before `CSB.h`. |
| `HeroStatsDraw` | Every page, drawn with Dear ImGui: the title strip, the champion row, the level picker, the topic tabs, tiles, tables and bars. |
| `HeroStatsWindow` | The OS window, its own ImGui context, its events and its `csb.cfg` settings. Declare-only header, safe after `CSB.h`. |

## How a number gets counted

The engine calls a hook where something happens, for example `HeroStats_OnMeleeAttack` in
`Attack.cpp` or `HeroStats_OnMonsterDied` in `Code11f52.cpp`. Damage needs to know who caused it,
which the engine often no longer knows when the damage lands, so:

- A champion's missile is announced before it is launched (`HeroStats_ExpectHeroMissile`) and
  remembered in `HeroStatsOrigins` by the missile's record; its hits, the cloud it leaves and the
  kills of that cloud go to the same champion.
- `HeroStats_BeginHarm` / `HeroStats_EndHarm` around the engine call that hurts the heroes names
  the source of that damage. The pairs nest, and the outermost names the source.
- `HeroStats_BeginMonsterBlow` / `HeroStats_EndMonsterBlow` do the same for a door or a pit that
  kills a monster.

Everything counts on the level the party is on (`HeroStats_OnPartyOnLevel`, called from
`AutoMap_OnEngineTick`).

## Saving and loading

The store and the origins are written into the statistics block of the save's `.map` file
(`AutoMapFile`, format version 13 and later; the layout is in the header comment of
`AutoMapFile.h` and in [data-formats.md](data-formats.md)). `AutoMap_OnDungeonLoaded` calls
`HeroStats_OnDungeonLoaded`; after the `.map` file is read, `HeroStats_OnMapFileRead` (or
`HeroStats_OnNoMapFile`) asks `HeroStatsGame` whether this is the game in play. If it is, the
deaths, wipes and reloads in memory stay and the load counts as a reload.

Counts that come from the map rather than from hooks (tiles explored, wall texts read, champions
seen and viewed) are taken from the map's memory when the window shows them
(`HeroStats_RefreshMapCounts`), so they are there for games saved before the statistics existed.

## Adding a number

1. Count it in `HeroStats` (and in `Serialize`, with a new `.map` format version), with a hook
   in the engine.
2. Add it to the catalogue: an entry in `HeroStatsNumber`, its `HeroStatsNumberInfo`, its place
   in a topic's rows (and tiles, if it is one of the four main numbers), and its value in
   `HeroStatsCatalogue_Record` and `HeroStatsCatalogue_Value`.
3. That is all the window and the export need: the topic's Numbers table, Per level, the Overview
   and the HTML file pick it up from the catalogue. `HeroStatsCatalogueTest` checks that every
   number belongs to a topic and that the single-number path matches the record.

## Tests

`make -C src test` runs the statistics tests with the map's: `HeroStatsTest` (the store),
`HeroStatsCatalogueTest`, `HeroStatsPagesTest` (pages and the HTML export),
`HeroStatsViewTest`, `HeroStatsOriginsTest`, `HeroStatsPendingTest`, `HeroStatsGameTest` and
`HeroStatsDumpTest`. The drawing in `HeroStatsDraw` and the hooks need the game and are tried by
playing.
