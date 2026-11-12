# Python tools

Four Python scripts live in `src/`. They are not part of the game build — the
`makefile` never touches them. They convert savegames between the versions of
the game. (The damage calculator that used to live here in Python is now a C++
program of its own, with its own savegame reader: see
[calculator.md](calculator.md).) Each section below says what the script does, how to
run it, where it came from, and the C++ it mirrors.

## Common conventions

- The classic savegames are **big-endian** (Atari ST, Amiga, X68000). The scripts
  default to that; `savefile.py` also handles the little-endian PC/CSBwin case.
- `savefile.py` and `dmitems.py` are **libraries** (no `main`); the others are
  run directly.
- No third-party packages: `struct`, `os`, `sys` and `argparse` from the
  standard library.
- `__pycache__/` is git-ignored; `savefile.py` and `dmitems.py` may leave byte
  caches behind.
- The line numbers in the scripts' own docstrings drift as the C++ changes. The
  refs in this document were checked against the current tree.

## `savefile.py` — read champions from a savegame

A library, not a program. It decodes a DM/CSB savegame and exposes the champion
records: name, title, HP/mana/stamina, the seven attributes and the twenty skills.
`dm_save_to_csbwin.py` builds on it.

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
