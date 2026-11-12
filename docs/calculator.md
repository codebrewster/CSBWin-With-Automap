# The calculator

`src/calculator` is a program of its own: C++17 with Qt (Widgets), no Python,
nothing shared with the game build. It answers "what happens if" questions about
fights and resting from the engine's own formulas.

Every result is **exact**: each random step of the engine is a roll over a small
range, so the program carries a table of probabilities through the same steps in
the same order instead of running trials. Results appear as you type.

## Building and running

See [BUILDING.md](../BUILDING.md#the-calculator). In short:

```
cd src/calculator
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build -j8
./build/dm_calculator [SAVEGAME]
```

## Where the numbers come from

- **A savegame** (Open savegame…, or drop the file on the window): classic
  Atari ST and Amiga saves, saves of CSBwin and of this port, and Amiga save
  disks (`.adf`). It gives the heroes (attributes, skill levels, hit points,
  stamina, mana, food, water, load, wounds, what they wear and hold), the party's
  shields, the game clock, and for every level its experience factor and the
  monster groups on it with their hit points.
- **The game's tables** (monsters, weapons, armour, actions, spells, food) are
  read from the `graphics.dat` beside the savegame. Without one, the built-in
  tables of Dungeon Master or Chaos Strikes Back are used; the two differ in a
  few places (Berzerk costs 26 stamina in one and 40 in the other).
- **The rules** ("Monsters vulnerable on attack", Nostalgia Mode) are read from
  the `csb.cfg` beside the savegame.

Every value can be changed by hand. A field that no longer holds the savegame's
value is marked; a right-click puts the value back.

## The tabs

**Hero attacks.** Each physical action of the weapon in hand, or of every
weapon, against the monster: hit chance, damage per hit and per swing, least and
most, damage per second, stamina, experience, and how long the monster lasts.
"Swings in a fight" above 1 gives the average over a fight in which luck drifts
and, if ticked, stamina drains.

**Monster attacks.** One blow of the monster on the hero: the chance it lands,
the damage, which body part it is aimed at and that part's armour value, the
chance of a wound and of poison, what the armour saves compared with the bare
hero, and how many attacks the hero survives. For monsters that throw spells, a
table of their missiles from a distance you set.

**Spells.** A grid of the damage spells by power, LO to MON. The grid shows one
number per cell (average damage, range, chance to kill, damage per mana, chance
the cast works, or mana); the cell you click is shown in full below.

**Recovery.** How long hit points, stamina and mana take to come back when
walking, standing still or sleeping, and what the hero eats and drinks until
then. Recovery is not random in the engine: it follows the game clock.

**What if.** One result plotted against one input, everything else as set.

## What to know about the numbers

- **Time.** The game clock runs at 6 ticks a second; all "per second" values
  use that.
- **A monster's attack rhythm** is taken from its timers when it stands and
  strikes. A monster that turns or steps in between is slower; the field
  "Ticks between attacks" overrides the value.
- **Luck in a monster's attack** is the hero's luck as it stands. Luck moves by
  2 with every luck roll, so over a long fight it drifts; the Hero attacks tab
  models that drift, the Monster attacks tab does not.
- **Missiles fade** with every half square they fly. The distance is counted
  from the square in front of the caster; "back row" adds half a square.
- **Poison Bolt** follows this port: the bolt itself does 64 ÷ the monster's
  defence, and nothing at all against a defence above 64.
- **Harm Non-Material against a Zytaz** is calculated like against any other
  non-material being; the engine has a special case for Zytaz that are attacking.
- **Other weapons** in "Every weapon" are tried on with the hero's load as it is.
- **What a hand holds** counts with its weight only for weapons and shields.
- **Walking** in the Recovery tab counts no stamina spent on the steps.

## How it is checked

`calc_tests` (run by `ctest`) plays every formula a few hundred thousand times
with dice, as a line-by-line transcription of the engine function, and compares
the outcome with the exact table: the mean, the hit chance and every single
damage value. Recovery is checked against cases worked out by hand.

```
./build/calc_tests                 # the formulas
./build/calc_tests --files GRAPHICS.DAT SAVEGAME...   # the file readers, on your files
./build/calc_dump GRAPHICS.DAT     # the tables of a graphics.dat
./build/calc_save SAVEGAME         # what is read out of a savegame
```

The dice and the exact tables are both written from the engine's source, so
this proves that the two agree, not that either matches a running game. The
hero-attack numbers agree with the earlier Python calculator.

## Layout of the sources

| Folder | What is in it |
|--------|---------------|
| `core/` | The formulas, each as an exact table and as a dice version; no Qt |
| `io/` | Readers for savegames, Amiga disk images, `graphics.dat`, `csb.cfg`; no Qt |
| `ui/` | The window: theme, side panel, the five tabs, charts |
| `cli/` | `calc_dump` and `calc_save` |
| `tests/` | `calc_tests` |

`core/Builtin.cpp` is generated: `calc_dump --emit function "label" graphics.dat`
writes one of its two functions.

Each formula names the engine function it follows and where that is:

| File | Engine |
|------|--------|
| `core/HeroAttack.cpp` | `DeterminePhysicalAttackDamage` (Attack.cpp), `Quickness`, `IsCharacterLucky`, `DetermineThrowingDistance` (Character.cpp), `MaxLoad` (Code11f52.cpp) |
| `core/MonsterAttack.cpp` | `MonsterDamagesCharacter` (Monster.cpp) |
| `core/Defense.cpp` | `DamageCharacter`, `TAG01680a`, `TAG016426` (Character.cpp), `PoisonCharacter` (CSBCode.cpp) |
| `core/Spells.cpp` | `CastSpell`, `CharacterThrowsSpell` (Magic.cpp), `DetermineMagicDamage`, `ProcessMissileEncounter`, `CreateCloud` (Code11f52.cpp), `ProcessTT_25` (Timer.cpp), `MonsterAttacks` (Monster.cpp) |
| `core/Recovery.cpp` | `TenSecondUpdate` (Code17818.cpp), `AdjustStamina` (Code11f52.cpp) |
| `io/SaveGame.cpp` | `SaveGame.cpp`, `Unscramble` (CSBCode.cpp) |
| `io/GraphicsDat.cpp` | `ReadTablesFromGraphicsFile` (CSBCode.cpp), `LZWExpand` (Graphics.cpp) |
