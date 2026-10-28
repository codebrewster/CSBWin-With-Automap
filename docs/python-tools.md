# Python tools

Eight Python scripts live in `src/`. They are not part of the game build — the
`makefile` never touches them. Some were copied in from other projects because
they already did something the port needed (the savegame format, the damage
maths), and some are original tooling written here; all of them either read or
reproduce the C++ engine. Each section below says what the script does, how to
run it, where it came from, and the C++ it mirrors.

## Common conventions

- The classic savegames are **big-endian** (Atari ST, Amiga, X68000). The scripts
  default to that; `savefile.py` also handles the little-endian PC/CSBwin case.
- `savefile.py` and `dmitems.py` are **libraries** (no `main`); the others are
  run directly. `adf.py` and `dps_graph.py` are both: the GUI imports them, and
  each also runs on its own.
- No third-party packages: `struct`, `os`, `sys`, `argparse`, `random`, and
  `tkinter` (for the GUI only) from the standard library.
- `__pycache__/` is git-ignored; `savefile.py` and `dmitems.py` may leave byte
  caches behind.
- The line numbers in the scripts' own docstrings drift as the C++ changes. The
  refs in this document were checked against the current tree.

## `damage_calc.py` — physical damage simulation

A faithful Monte-Carlo port of the player→monster physical-attack pipeline. It
configures a character, a weapon and a monster in a `CONFIG` block at the top and
prints a table of hit %, damage per try/hit, DPS, stamina cost and XP for each
weapon+attack combination.

```
python3 damage_calc.py
```

Mirrors, in order:
- `Quickness` — `Character.cpp:2192`
- `IsCharacterLucky` — `Character.cpp:2148` (in the script luck is held fixed;
  the game itself drifts it ±2 per call)
- `DetermineThrowingDistance` — `Character.cpp:2250`
- `DeterminePhysicalAttackDamage` — `Attack.cpp:484`, including the D6≤1 graze
  chain and the final randomize/shrink chain inside that function
- `DamageMonster` — `Code11f52.cpp:1084` (plain HP subtraction)

The attack numbers (`P7` = strength required, `P8` = damage factor) are the
decoded 7-column attack table values; see [data-formats.md](data-formats.md).

## `damage_calc_gui.py` — a Tkinter front end

A GUI around `damage_calc.py`. It lets you tick weapons and their attack methods,
pick and edit a monster, edit all character details or load a champion from a
DM/CSB savegame, set the fight length, run the simulation, and sort the result
table. Champion loading goes through `savefile.py`; a champion can also be loaded
straight from an Amiga save disk image (`.adf`) through `adf.py`. **Graph DPS vs
Str** opens the `dps_graph.py` plot for the selected weapon and attack.

```
python3 damage_calc_gui.py
```

## `dps_graph.py` — DPS against strength

Plots damage per second against strength for one weapon and attack, barefoot and
with Elven Boots. It shows how the boots' MaxLoad bonus moves the throw-weight
branches of `DetermineThrowingDistance` (`Character.cpp:2250`), including the
window where a higher MaxLoad briefly lowers the bonus. It runs the simulation of
`damage_calc.py` and draws on a Tkinter canvas; the GUI calls it from its
**Graph DPS vs Str** button.

```
python3 dps_graph.py axe chop
python3 dps_graph.py morningstar melee --monster Dragon --str-lo 30 --str-hi 110
python3 dps_graph.py axe chop --no-show   # print the table only
```

## `adf.py` — read files from an Amiga disk image

A minimal, read-only reader for AmigaDOS disk images (ADF, 901120 bytes, OFS or
FFS). It finds files by scanning for file-header blocks, which is enough for the
flat save disks of the Amiga games, so the GUI can pull `DMGAME.DAT` off a save
disk without extracting it first. The image is never modified.

```
python3 adf.py save.adf   # list the files on the disk
```

## `savefile.py` — read champions from a savegame

A library, not a program. It decodes a DM/CSB savegame and exposes the champion
records: name, title, HP/mana/stamina, the seven attributes and the twenty skills.
The `damage_calc_gui.py` "load a champion" feature and the two converter scripts
both build on it.

The format is documented in its docstring and in [data-formats.md](data-formats.md).
It mirrors the engine's loader: `SaveGame.cpp`, the `Unscramble` decryptor
(`CSBCode.cpp:9102`, reimplemented as `_unscramble`), and the `Chaos.cpp` load
path. The `DB_ENTRY_SIZES` table matches the packed object records in `CSB.h`.

## `dmitems.py` — item names by RN

A library mapping an object's RN (indirect object number) to its name, for both
Dungeon Master and Chaos Strikes Back saves. It is **generated from another
project**: the `knowitem` table in MCE (Multi-game Character Editor by James
Jacobs, Amigan Software), in `dm.c` — 1088 observed `(RN, DM name, CSB name)`
triples. The lookup mirrors MCE's `writeitemgad()`: `0xFFFF` is empty, only
values with `(rn & 0x3FFF)` in `0x1000..0x2FFF` are valid, and the name index is
`rn & 0x1FFF` mapped through the `0x00/0x800/0x1400/0x1800` ranges.

Used by `savefile.py` to name carried items.

## `dm_save_to_csbwin.py` — make a DM save loadable by CSBwin

Rewrites a classic Amiga Dungeon Master (2.0) save *in place* so CSBwin's own
loader accepts it. CSBwin's converter follows the inventory `m_link` chains and
aborts with "SAVED GAME DAMAGED" because DM stores a champion's 30 possession
slots as a linked list; this tool sets those links to `RNeof` and reshapes Block1
from the DM layout to the CSB layout (the `RandomGameID` field widens from `i16`
to `i32`, shifting later fields by two bytes), then recomputes the checksums.

```
python3 dm_save_to_csbwin.py input [output]   # output defaults to <input>_csbwin.dat
```

It relates to the `SaveGame.cpp` loader and uses `savefile.py` helpers
(`_find_classic`, `_decode_block1`, `load_champions_bytes`). After conversion,
load the file in CSBwin (`DUNGEON → RESUME`) and use `File → Save game` to write a
native CSBwin (Extended Features) save — see [data-formats.md](data-formats.md).

## `convert-Amiga20-to-Amiga36.py` — Amiga 2.0 → 3.6 save

Converts an Amiga 2.0/2.1/2.2 save to the 3.6 layout. Both share the same
encryption, big-endian order and Block1 key, so the only structural change is the
champion section: 2.0 stores four 800-byte records with portraits embedded,
3.6 stores four 320-byte records plus a separate 464-byte portrait block. The
script applies the two 3.6 normalizations needed by tools like DMute: the
32-bit portrait pointer (record offset 276) and the `MaxITEM16` capacity
(2.0's typical 60 is grown to 110).

```
python3 convert-Amiga20-to-Amiga36.py DMGame.dat DMGame36.dat [--force]
                                 [--pointer-base 0x1DFC30] [--zero-pointers]
                                 [--no-expand-item16]
```

The ITEM16 expansion mirrors the engine's `EnsureItem16Available`
(`Monster.cpp:83`); the scramble mirrors `Unscramble` (`CSBCode.cpp:9102`).
