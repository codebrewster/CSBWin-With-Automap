# Data formats

The files the program reads and writes: the game data, the savegames, the config,
and the text/locale files. Savegame details are also codified by the Python tools
in [python-tools.md](python-tools.md), which is a good cross-check.

## Runtime game data

A game folder holds the game data plus copies of this repository's config and
text files (see [../BUILDING.md](../BUILDING.md)):

| File | Who reads it |
|------|--------------|
| `dungeon.dat` | The level, cell and object data. Loaded by `ReadDatabases` (`SaveGame.cpp:2571`) into `dungeonDatIndex` / the cell arrays of `d` (`Data.h:300-330`) |
| `graphics.dat` | The pictures and palettes; decoded in `Graphics.cpp` and `Bitmaps.cpp`, read for the automap in `AutoMapSprites.cpp:51` |
| `csb.cfg` | Settings and key bindings, read by `read_conf` (`CSBlinux.cpp:1888`) |
| `en.txt`, `fr.txt`, `locale/` | In-game text, read by `ReadTranslationFile` (`Translation.cpp:175`) |

The config file `csb.cfg` has two sections. `[settings]` holds the timer, window
size, `gameSpeed`, and — important — the file names `dungeonName` and
`graphicName` (`csb.cfg:18-19`), which is how the program finds the game data
(`CSBlinux.cpp:1903`, `1905`). `[keys]` maps PC scan codes to Atari key codes and
to mouse actions (`csb.cfg:24-160`); `mscan` lines turn a key press into a click
on the original 320×200 screen.

The game's own data tables — level descriptors (`LEVELDESC`), the object-index
tables (`DUNGEONDATINDEX`), cell flags, weapon descriptors (`WEAPONDESC`,
`CSB.h:1374`) and legal-attack masks (`LEGALATTACKS`, `Data.h:227`) — are all
fixed-size records read straight from `dungeon.dat`. The `DB_ENTRY_SIZES` table
in `savefile.py` lists the record size per object class.

## Savegames

CSBwin reads and writes the classic Dungeon Master / Chaos Strikes Back
savegame, with a native extension. The reference layout (from
`savefile.py` and the loader) is, for the big-endian versions (Atari ST, Amiga,
X68000):

- **Block1 (512 B)** — first 256 B random filler, last 256 B scrambled data. The
  key that decodes the tail is the word at offset 20 (DM) or 58 (CSB). The
  decoded tail carries the hashes/checksums for the following streams (hashes at
  310…318, checksums at 342…350).
- **Block2 (128 B)** — descrambled with the hash at 310; holds `numcharacter`
  (+10), `MaxITEM16` (+46), and the rest of the game state.
- **ITEM16** — `16 × MaxITEM16` bytes of object records, hash at 312.
- **Champion data** — hash at 314. Size depends on the version: 3328 B
  (4 × 800 + 128, portraits embedded) for Atari DM and Amiga DM 2.x, or 1408 B
  (4 × 320 + 128, portraits stored separately) for Amiga DM 3.6, X68000 and
  Amiga CSB. The first 286 bytes of each record match the `CHARDESC` layout
  (`CSB.h:2563`).

The scrambling is `Unscramble` (`CSBCode.cpp:9102`); the write side is
`WriteScrambled`/`ScrambleAndWrite` (`SaveGame.cpp:674`, `718`) with the checksum
recomputed by `FormChecksum` (`SaveGame.cpp:207`). Little-endian saves (PC) decode
the same way with LE word assembly.

**CSBwin native saves** add an *Extended Features* block. It is detected by the
`" Extended Features \x00"` sentinel and read by `ReadExtendedFeatures`
(`SaveGame.cpp:301`); the block is what lets CSBwin store things the original
format has no room for. `Chaos.cpp` builds a CSB save from an old DM/Prison save
on the pre-game path (see [engine-core.md](engine-core.md)).

**File names.** CSBwin uses four save slots — `csbgame.dat`, `csbgame2.dat`,
`csbgame3.dat`, `csbgame4.dat` (`CSBCode.cpp:9831-9840`). The load dialog only
lists files starting with `csb`.

**Signatures.** A file's identity is its content hash: `Signature`
(`data.cpp:1947`) runs MD5 over the file (six significant bytes). Text is hashed
by the simple rotate-and-add `Signature(char*)` (`data.cpp:1982`), used for the
version string at startup (`CSBlinux.cpp:1309`).

## The automap file

The automap keeps its own state — the explored cells, the wall faces the 3D view has
shown, the facts the party has learned, the floor items it has seen and how it last saw each cell — in a `.map` file alongside the save, not in
the savegame itself. The glue is described in [automap.md](automap.md). Numbers are
stored lowest byte first. Layout of version 15 (`AutoMapFile.h`):

| Size | Content |
|------|---------|
| 4 bytes | `CSBM` |
| 2 bytes | format version (15) |
| 4 + 4 bytes | the two signature words of the saved game |
| 2 bytes | number of levels |
| per level | 1 byte width, 1 byte height, the explored bits (one per cell, row by row, lowest bit first), the seen-face bits (four per cell: bit `4 * (y * width + x) + side`, side 0 north, 1 east, 2 south, 3 west), the glimpsed-face bits (same layout: something seen on that face in the dark), the read-text bits (same layout: the wall text on that face read), the looked-into bits (same layout: the alcove in that face seen from in front, its contents drawn), the seen-floor bits (one per cell like the explored bits: the cell's floor picture noticed in the 3D view), then the same six sets for the rim, the ring of cells one step outside the level where the engine makes up rock: its cells in the order row above (corners included), row below, column left, column right, one explored bit, four bits for each face set and one seen-floor bit per cell |
| 4 bytes | number of facts |
| per fact | 1 byte kind, 4 bytes record, 1 byte level, 1 byte x, 1 byte y |
| 4 bytes | number of remembered floor items |
| per item | 4 bytes record, 2 bytes description index, 1 byte level, 1 byte x, 1 byte y, 1 byte glimpse (1: seen only as something in the dark) |
| 4 bytes | number of remembered cell looks |
| per look | 1 byte each level, x, y, kind (`AutoMapCellKind`), door state (`AutoMapDoorState`), pit open (0/1), pit hidden (0/1), spinner field visible (0/1: the 3D view draws the field of an active visible spinner); 2 bytes floor picture; 2 bytes for each of the four side pictures, side 0 north to 3 west (`0xffff`: no picture) |
| 4 bytes | number of player pit marks |
| per mark | 1 byte each level, x, y, open (1 open, 0 closed) |
| 4 bytes | number of player links |
| per link | 1 byte each source level, x, y, side (0 north to 3 west, `0xff` for a link from a floor tile), then target level, x, y (the target is on the source's level) |
| 4 bytes | number of damaged monster groups |
| per group | 4 bytes record, 2 bytes damage shown to each of the four members |
| 4 bytes | length of the statistics block (version 13) |
| block | 4 bytes length of the hero statistics (`HeroStats::Serialize`), those bytes, then the origins of missiles, clouds and potions (`HeroStatsOrigins::Serialize`); the tables inside are listed in the header comment of `AutoMapFile.h`. Version 14 adds the missile kind of a monster's missile to each harm record and the party's movement per level (steps, turns, teleports, ticks) at the end; version 15 adds the uses of actions that cannot hit to each attack record and ticks asleep, pit falls, pit climbs and wall bumps to the movement |

A record fact has `0xff` for level, x and y; a cell fact has `0xffffffff` for the record.
The record is `(dbNum << 16) | idx`. Items are stored in the order seen, looks by level, then y, then x, marks by level, y, x and links by level, y, x, side, then target y, x. Version 1 has neither
face bits nor facts, version 2 no floor items and version 3 no glimpse byte (its items count as
seen), version 4 no glimpsed-face bits, version 5 no read-text bits (a seen face counts as
read), version 6 no looked-into bits (a seen face counts as looked into) and version 7 no cell
looks (the loader gives every explored cell the look it has now), version 8 no player notes,
version 9 no damage, version 10 no rims (it kept a painted rim cell as the seen face of the
floor beside it towards the rim; the loader turns those into explored rim cells) and version 11
no seen-floor bits (the loader counts the floor of every explored cell as seen), version 12 no
statistics (they start empty), version 13 no missile kinds and no movement and version 14 no
action uses, sleep, pits or bumps; all are still read. A mark or link outside the dungeon, a side other than 0-3 or `0xff`, an open byte other than 0 or 1, or a link target on another level makes the whole file be ignored.

## Item and attack tables

Two decode tables are worth calling out because tools depend on them:

- **Item names by RN.** The `(RN → name)` mapping in `dmitems.py` was generated
  from MCE's `dm.c` and mirrors its `writeitemgad()` lookup; it names objects in
  the `ITEM16` table above.
- **Weapon attack parameters.** The 7-column attack table (P7 strength required,
  P8 damage factor, …) is decoded by `damage_calc.py` and drives
  `DeterminePhysicalAttackDamage` (`Attack.cpp:484`).

See [python-tools.md](python-tools.md) for how both are used.
