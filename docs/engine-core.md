# Engine core

This is the game logic: the ported engine, the party and monsters, combat, magic,
object movement, timers, saving and replay. It follows the same porting style
described in [architecture.md](architecture.md) — most functions keep their
original 68k address as a `TAG...` name.

## Shared foundation

Everything runs against one global state struct, `DBank d` (`Data.h:234`,
declared `Data.h:1520`). It is the C++ mirror of the original memory block and
holds, among hundreds of fields:

| Member | Ref | Holds |
|--------|-----|-------|
| `hero[4]` | `Data.h:783` | The four champions (see `CHARDESC`, `CSB.h:2563`) |
| `MonsterDescriptor[27]` | `Data.h:1093` | Per-monster-type stats (`MONSTERDESC`, `CSB.h:2340`) |
| `partyLevel`, `newPartyLevel` | `Data.h:932`, `904` | The level the party is on |
| `MaxITEM16` | `Data.h:840` | Size of the object table |
| Level/cell data pointers | `Data.h:300-330` | `pLevelDescriptors`, `objectLevelIndex`, the cell arrays read from `dungeon.dat` |

Objects are addressed by an indirect index, `RN` (the `RNeof` sentinel means "no
object"), and stored as 16-byte `ITEM16` records (`CSB.h:2274`). The record
classes `DB0`…`DB15` in `CSB.h:842-1374` reinterpret an `ITEM16` as its concrete
kind — `DB3` is an actuator/pad/keyhole (`CSB.h:951`), `DB4` a monster
(`CSB.h:1099`), `DB1` a teleporter (`CSB.h:873`), `DB14` a missile
(`CSB.h:1323`). Overlayed on `ITEM16` this is the whole game-object model.

The timer system is `GameTimers` (`Data.h:1542`), a queue borrowed from the
original: `TIMER` entries are processed in `Timer.cpp` (see below). Saves pack
all of this — see [data-formats.md](data-formats.md).

## CSBCode.cpp — the engine proper

`CSBCode.cpp` is the largest file (~11.7k lines) and holds the ported core:
screen drawing, palette handling, the mouse, and the vertical-blank tick
`vblInterrupt` (`CSBCode.cpp:901`, tagged `TAG000c3a`), which is where a unit of
game time advances and which drives the timer queue. It also holds `Unscramble`,
the savegame decryptor (`CSBCode.cpp:9102`), and helpers such as `LE32`/`BE32`
byte-swaps (`CSBCode.cpp:240`, `250`) and `die` (`CSBCode.cpp:98`) used
throughout. If a function does not have a more thematic home, it lives here.

## Chaos.cpp — the CSB game flow

Despite the name, this is not "AI chaos": the file is the translation of the
original *Create New Adventure* function (`Chaos.cpp:1-6`). It reads an old
Dungeon Master or Prison save and builds a Chaos Strikes Back save from it — the
pre-game flow. It also carries the `_CALL0`…`_CALL9` helpers
(`Chaos.cpp:62-71`) that invoke `TAG...` functions through `Dispatch()`.

## DSA.cpp — Dungeon-Specific Actions

Dungeon-Specific Actions are the scripting layer built into later engines: small
programs embedded in a dungeon that run on triggers. `DSA.cpp` (~6.5k lines)
interprets them — `Indirect` (`DSA.cpp:2087`) is the bytecode dispatcher, and
`ForEach` (`DSA.cpp:1544`) walks objects for a script. The command set and the
per-script opcodes are listed in `DSAInfo.txt` at the repo root. The runtime state
classes live in `Data.h:1896-2500` (`DSA`, `DSAState`, `DSAAction`). Triggered
DSA timers are handled in `Timer.cpp` (`ActivateDSA`, `Timer.cpp:1503`).

## Character.cpp — the party

Champion handling: drawing the character boxes and life bars
(`DrawCharacterState`, `Character.cpp:1231`; `DrawLifeBars`,
`Character.cpp:897`), inventory (`AddCharacterPossession`,
`Character.cpp:1918`; `DisplayBackpackItem`, `Character.cpp:1098`), the
character-creation path (`AddCharacter`, `Character.cpp:443`;
`BringCharacterToLife`, `Character.cpp:791`), and the
attribute rules used by combat — `IsCharacterLucky` (`Character.cpp:2148`),
`Quickness` (`Character.cpp:2192`), `DetermineThrowingDistance`
(`Character.cpp:2250`). These three are exactly the functions the Python
`damage_calc.py` re-implements; see [python-tools.md](python-tools.md).

## Monster.cpp — monsters and movement

The monster rules: whether a cell is passable (`BlockedTypeA`, `Monster.cpp:294`;
`Blocked`, `Monster.cpp:518`), pathfinding (`PossibleMove`, `Monster.cpp:635`),
turning (`TurnMonster`, `Monster.cpp:825`), and the attack driver
(`MonsterAttacks`, `Monster.cpp:949`). Monster timers (fear, wandering, group
turns) are set up here as well (`Set_Monster_Timer`, `Monster.cpp:1351`).

## Attack.cpp — physical combat and spells

`DeterminePhysicalAttackDamage` (`Attack.cpp:484`) is the physical-damage
pipeline: strength, weapon, monster armour, luck, and the randomisation chain.
`AttackWithPhysicalForce` (`Attack.cpp:1031`) resolves a melee/ranged attack,
`AttackWithSpell` (`Attack.cpp:1289`) the spell variant, and filters let DSAs
intercept attacks (`CallAttackFilter`, `Attack.cpp:407`). The `damage_calc.py`
port follows this file closely.

## Magic.cpp — spells and skills

Spell casting and character advancement: `CastSpell` (`Magic.cpp:1093`),
`CastMagic` (`Magic.cpp:934`), missile launching (`LaunchMissile`,
`Magic.cpp:18`; `LaunchObject`, `Magic.cpp:456`), and skill progression
(`LevelUp`, `Magic.cpp:86`; `AdjustSkills`, `Magic.cpp:192`; `AddToSkill`,
`Magic.cpp:367`).

## Timer.cpp — the timer queue

The timer queue executes all delayed events. `MissileTimer`
(`Timer.cpp:188`) moves thrown objects; `ThrowMissile` (`Timer.cpp:2019`) starts
one; `OpenTeleporterPitOrDoor` (`Timer.cpp:628`), `ProcessTT_DOOR`
(`Timer.cpp:1559`), `ProcessTT_TELEPORTER` (`Timer.cpp:2393`) and the other
`ProcessTT_*` functions are the per-timer-type handlers. `TraceTimer`
(`Timer.cpp:47`) is the debug dump used by the trace options.

## MoveObject.cpp — object movement

Picks up and drops objects. It resolves a source and a destination for a move —
`CheckSource` (`MoveObject.cpp:369`) and `CheckDestination`
(`MoveObject.cpp:674`) over the character/cell/monster/chest cases — and
`CursorFilter` (`MoveObject.cpp:797`) lets DSAs veto a move.

## SaveGame.cpp — saving and loading

Reads and writes savegames and the per-game state: byte-order swaps
(`SwapDataIndexMap`, `SaveGame.cpp:168`), the checksum (`FormChecksum`,
`SaveGame.cpp:207`), DSA blocks and the level index (`ReadDSAs`,
`SaveGame.cpp:218`; `ReadDSALevelIndex`, `SaveGame.cpp:288`), and game info
(`ReadGameInfo`, `SaveGame.cpp:251`). The on-disk layout is documented in
[data-formats.md](data-formats.md) and mirrored by the Python tools.

## Recording.cpp — record and replay

A built-in macro system: `RecordfileOpen`/`PlayfileOpen`
(`Recording.cpp:1299`, `850`) open a recording or a playback, and the thin
`PlayFile_*`/`RecordFile_*` wrappers feed mouse events through it. This is how
the game's "record your run" feature and the test traces work.

## Translation.cpp — text localisation

Reads the language file (`ReadTranslationFile`, `Translation.cpp:175`) and
translates strings at display time (`TranslateWallLanguage`,
`Translation.cpp:263`); the in-game text is wrapped in the `_()` macro
(`CSB.h`). The source text lives in `en.txt`/`fr.txt` and `locale/`.

## Statistics.cpp — the monster info screen

The in-game statistics screen: `Statistics` (`Statistics.cpp:94`) shows monster
stats, with names resolved by `GetMonsterName` (`Statistics.cpp:48`).

## ShotMissiles

`ShotMissiles.cpp` / `ShotMissiles.h` hold the missile-flight rules shared by the
engine and the automap tests. The unit needs neither the engine nor SDL, so it is
tested by `make test` alongside the automap units.
