# Changelog

What changed for the player in each release, newest first. Every commit that changes what the
game, the map, the statistics, the replay or the calculator does adds a line under
**Unreleased**; at a release the section is renamed to the version, and the release notes and the
forum post are written from it. Internal changes (refactoring, tests) do not belong here.

## Unreleased

## 1.4.0 (2026-11-14)

**Replay**
- Fast forward and go back by 25, 250 or 2500 ticks (buttons, Left/Right, Down/Up, Page Down/Page Up); elapsed and total time on the bar.
- Pause at a typed tick, ahead or behind; a breakpoint or pause that a fast forward runs into stops it.
- Go back after the end, and from the win and death screens; a popup asks before progress is thrown away; an x hides the buttons.
- File > Continue recording: replays to the end at full speed and records on into the same file.
- Nostalgia Mode plays by the rules of CSBwin 17.6, so a recording replays there too.
- More old playfiles replay exactly (reincarnation penalty before 9.6, killing blow and defense adjustment before 9.9; DMMovie8).

**Statistics**
- Champions tab: a card per champion, the inventory as picture or list with icons, weight, charges, wounded slots and chest contents; also in the HTML export.
- Food and water an item gives: in the tooltip and as sortable Food and Water columns (window and export).
- Items with the same name are told apart (MAGICAL BOX (BLUE)); a dead champion's bones carry the champion's name (TIGGY BONES); THE FIRESTAFF (COMPLETE).
- Biggest hit split into biggest melee hit and biggest ranged hit; PROJECTILE and SPELL DAMAGE show the hit points lost.
- Window and HTML export lay out by width; the export keeps its view in the address bar.
- The statistics are an entry of their own in the zipped .map file (format version 26).

**Map**
- Ctrl+click on a stair, or ASCEND / DESCEND in the right-click menu, comes out on the other level.
- Pit style (AUTO, PICTURE, STRIPES) in the Show menu.
- A fluxcage is a yellow shimmering field, also in the tooltip and the export.
- Oh Ew Ra shows what lies beyond the hole; wall switches that rotate the actuator list; teleporters and spinners that take or turn missiles, items and monsters.
- The export's hovered ruler number gets a gold plate; the export's tooltips are about 10x faster.
- The party can stand on a passable illusionary wall (also in the visibility test).

**Game**
- The game window keeps its place on the desktop, like the map and the statistics.
- Reset, Load and Select dungeon work from the menu during the Fusion end sequence.

**Calculator**
- A program of its own in C++ with Qt (replaces the Python one): exact results for hero and monster attacks, spells, recovery, thrown weapons and launcher shots, chance to kill, damage against distance, What if graphs; reads savegames.
