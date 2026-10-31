# CSB documentation

This is the documentation for this repository: CSBwin, a portable C++ port of the
Dungeon Master / Chaos Strikes Back engine, plus the Python tools kept here for
savegame work and damage analysis.

It is written for a new maintainer: each file explains what a part of the project
is for, how it fits with the rest, and where to look in the source. It is meant to
be read, not to replace the code — every non-obvious claim points at a
`file.cpp:line`.

## Contents

| File | What it covers |
|------|----------------|
| [architecture.md](architecture.md) | The big picture: what the program is, the directory map, the engine main loop and data flow |
| [engine-core.md](engine-core.md) | Game logic: `CSBCode`, `Chaos`, `DSA`, characters, monsters, combat, magic, timers, saving |
| [rendering.md](rendering.md) | Graphics decoding, the 3D viewport, screens, Dear ImGui, sound |
| [platform.md](platform.md) | The platform mains, the UI layers, and the differences between Linux/macOS/Windows/Raspberry |
| [automap-manual.md](automap-manual.md) | How to use the automap: controls, settings, tooltips, search, notes, testing aids |
| [statistics-manual.md](statistics-manual.md) | How to use the Statistics window: pages, levels, champion pages, sorting, export as HTML, what is counted |
| [automap.md](automap.md) | The `AutoMap*` feature: units, hooks, save/load, tests |
| [data-formats.md](data-formats.md) | `dungeon.dat`, `graphics.dat`, savegames, `csb.cfg`, text/locale files, the decode tables |
| [python-tools.md](python-tools.md) | The eight Python scripts, their origin, and the C++ they mirror |
| [reference-disassembly.md](reference-disassembly.md) | The `CodeNNNN.cpp` files, the address naming convention, notable decode tables |

## See also

* [../README.md](../README.md) — what this repository is, where the game data comes from, forum links.
* [../BUILDING.md](../BUILDING.md) — building and running on macOS and Linux.
* Game-mechanics research:
  * [dungeon-master-reincarnate-vs-resurrect.md](dungeon-master-reincarnate-vs-resurrect.md)
  * [light-spells.md](light-spells.md)
  * [mana-stamina-food.md](mana-stamina-food.md)
  * [spell-checks.md](spell-checks.md)
