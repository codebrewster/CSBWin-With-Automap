# Disassembly reference

The bulk of the engine is a function-by-function translation of the original
68k Dungeon Master / Chaos Strikes Back code. This page explains how that
translation is named and split, and where the decode tables live. It is
orientation, not a full reverse-engineering write-up.

## The `TAG` naming convention

Every ported function keeps the address it had in the original binary, as a
comment or as part of its name. `vblInterrupt` in `CSBCode.cpp:901` is tagged
`TAG000c3a`, meaning it lived at `$000c3a`. The tags are lowercase hex, six
digits (leading zeros included) in the code, e.g. `TAG000efc` for `die`
(`CSBCode.cpp:98`).

The convention lets you line the C++ up against a disassembly listing, and it is
how the original's control flow (`_CALL0`…`_CALL9`, `Dispatch()`) is preserved;
see [engine-core.md](engine-core.md). There are hundreds of such functions — 783
distinct tags across the ported `.cpp` files.

Some code that could not be reconstructed is marked with `HopefullyNotNeeded`
(e.g. `TAG021234`, `Code222ea.cpp:13`), a deliberate no-op.

## The `CodeNNNN.cpp` files

When a translation file grew too large, it was split into `Code` files named
after the address the block was cut at. The name is the address with leading
zeros removed, so `Code13ea4.cpp` is the block at `$013ea4` and `Code390e.cpp`
the block at `$00390e`. Each file contains a `TAG` of that same address, though
not always as its first function (nearby functions were grouped in).

| File | What it holds |
|------|---------------|
| `Code11f52.cpp` | Monsters and objects: positions and facings (`MonsterPositionByte`, `Code11f52.cpp:204`), room-object linking (`AddObjectToRoom`, `Code11f52.cpp:420`), icons, and `DamageMonster` (`Code11f52.cpp:1084`) |
| `Code13ea4.cpp` | `_ReIncarnate` (`Code13ea4.cpp:20`), the champion reincarnation |
| `Code17818.cpp` | Skills: `DetermineMastery` (`Code17818.cpp:13`) and `TenSecondUpdate` |
| `Code1f9e6.cpp` | Record/playback glue and screen helpers (`Darken`, `DisplayHeroName`) |
| `Code222ea.cpp` | Memory management (`CompactMemory`, `Code222ea.cpp:106`) and sorting (`SortWords`, `Code222ea.cpp:19`) |
| `Code390e.cpp` | `ReadGraphicsForLevel` (`Code390e.cpp:16`), loading a level's pictures |
| `Code51a4.cpp` | Drawing: `DrawMonster` (`Code51a4.cpp:1139`), `DrawSingleObject` (`Code51a4.cpp:1754`), doors and their sides |
| `Codea59a.cpp` | Room summarisation: `SummarizeStoneRoom` (`Codea59a.cpp:19`) and `SummarizeRoomInfo` (`Codea59a.cpp:225`) — the "what is in this cell" rule used by the viewport and the automap |

## Decode tables and porting notes

The tables that decode the game data live where they are used:

- **Graphics** are LZW-compressed in `graphics.dat`; the decoder and its tables
  are in `LZWExpand` (`Graphics.cpp:2784`).
- **Object data** is indexed by `DUNGEONDATINDEX` and the level/cell arrays in
  `data.cpp` (`DUNGEONDATINDEX`, referenced from `Data.h:300-330`).
- **Item names** by RN are not in the C++ at all; they were taken from MCE's
  `dm.c` and live in `dmitems.py` — see [python-tools.md](python-tools.md).
- **Attack parameters** (the 7-column attack table) are decoded by
  `damage_calc.py`, mirroring `DeterminePhysicalAttackDamage`
  (`Attack.cpp:484`).

Notes left by the porters sit in the repo root and `src/`: `MemoryMap.txt`
(the original memory layout, referenced by the `d` struct's field names),
`DSAInfo.txt` (the DSA opcode table), `Viewport.txt`, and `debugging.txt`.
