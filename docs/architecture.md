# Architecture

CSBwin is a portable C++ re-implementation of the 68k Dungeon Master / Chaos
Strikes Back engine. The original game ran on the Atari ST and Amiga; this
repository keeps the game logic but replaces the machine layer (graphics, sound,
input, timers, files) with SDL2 and a small per-platform front end. The same
source builds the Linux, macOS, Windows and Raspberry Pi versions — see
[platform.md](platform.md) and [../BUILDING.md](../BUILDING.md).

At runtime the program reads a game folder containing `dungeon.dat` and
`graphics.dat` (the game data, not in this repository) plus `csb.cfg`, `en.txt`,
`fr.txt` and `locale/` (copied from this repository's root). See
[data-formats.md](data-formats.md).

## The port: what is original and what is new

The engine is a faithful port of the original 68k code. The disassembly was
translated into C++ function by function, and each function keeps its original
address as a name — `vblInterrupt` in `CSBCode.cpp:901` is tagged `TAG000c3a`,
meaning it lived at `$000c3a` in the original binary. There are hundreds of such
`TAG`-named functions (783 distinct tags across the ported files). When a file grew too large or a group of functions formed
a theme, it was split into a separate `CodeNNNN.cpp` file named after its load
address; see [reference-disassembly.md](reference-disassembly.md).

Around that ported core, files without a `Code`/`TAG` name are the new C++:
`CSBUI.cpp` and the platform UIs, the SDL layer, the automap, the statistics, and
the sound mixer.

## Directory map (`src/`)

The cheat-sheet below groups the source by role. The makefile's `OBJS` list
(`src/makefile`) is the authoritative build list.

| Group | Files | Role |
|-------|-------|------|
| Ported engine | `CSBCode.cpp`, `Code*.cpp`, `data.cpp`, `system.cpp`, `VBL.cpp`, `utility.cpp` | Game logic translated from the disassembly; `CSBCode.cpp` is the largest at ~11.7k lines |
| Game systems | `Chaos.cpp`, `DSA.cpp`, `Character.cpp`, `Monster.cpp`, `Attack.cpp`, `Magic.cpp`, `MoveObject.cpp`, `Timer.cpp`, `SaveGame.cpp`, `Recording.cpp`, `Translation.cpp`, `Statistics.cpp` | Party, monsters, combat, magic, object movement, timers, saves, replay |
| Graphics & view | `Viewport.cpp`, `Graphics.cpp`, `Bitmaps.cpp`, `*Screen*.cpp`, `MacScreen.cpp` | Palette and `graphics.dat` decoding, the 3D viewport, screens |
| UI | `UI.h`, `CSBUI.cpp`, `LinCSBUI.cpp`, `Menu.cpp`, `Mouse.cpp`, `Hint.cpp`, `HintCE.cpp`, `AsciiDump.cpp` | The UI message interface, the menu bar, the mouse, help/hint screens, debug dumps |
| Platform mains | `CSBlinux.cpp`, `CSBMac.cpp`, `CSBraspian.cpp`, `CSBwin.cpp`, `CSBWinCE2002.cpp` | One front end per target; owns the event loop |
| Automap | `AutoMap*.cpp/.h` | The live map window and its engine-free rules units |
| Statistics | `HeroStats*.cpp/.h` | The hero statistics: counting hooks, the store, and the Statistics window and HTML export |
| Sound | `Sound.cpp`, `SoundMixer.cpp`, `Win32_SoundMixer.cpp`, `sdl_sound/` | Sound playback and the bundled `SDL_sound` decoder |
| Vendored | `imgui/`, `sdl_sound/`, `fnmatch/` | Third-party: Dear ImGui (UI) and SDL_sound (MP3/WAV) |
| Headers | `CSB.h`, `CSBTypes.h`, `Data.h`, `Objects.h`, `UI.h`, `Dispatch.h`, `Transition.h`, `ShotMissiles.h` | Shared types, the engine state, object layouts, the UI contract |
| Data & config | `csb.cfg`, `en.txt`, `fr.txt`, `locale/`, `*.txt`, `MemoryMap.txt`, `DSAInfo.txt` | Runtime config, text, notes from the porting |
| Python tools | `*.py` (6 files) | Savegame conversion and damage analysis; see [python-tools.md](python-tools.md) |

Build metadata (Visual Studio projects, CodeWarrior files and resource files)
sits alongside as `*.sln`, `*.vcxproj`, `*.dsp`, `*.rc`, `*.hct`, etc.

## Main loop and data flow

Each platform front end has its own `main` — `CSBlinux.cpp:1286`,
`CSBMac.cpp:163`, `CSBraspian.cpp:394` — but they share the same shape. Using the
Linux front end as the reference:

```
main (CSBlinux.cpp:1286)
  parse options, read_conf()
  create the SDL window / renderer, ImGui context, fonts
  send UIM_INITIALIZE to the engine     CSBUI(&csbMessage)  CSBlinux.cpp:1565
  start the tick timer                  SDL_AddTimer -> __Timer_Callback  CSBlinux.cpp:1572,142
  while (true)                          CSBlinux.cpp:1585
    SDL_WaitEvent, then ImGui_ProcessEvent
    route the event to the engine       Process_SDL_*  CSBlinux.cpp:1651-1660
    pump the automap window             AutoMapWindow_PaintIfDirty
    and the statistics window           HeroStatsWindow_Paint
```

An event becomes a `CSB_UI_MESSAGE` carrying a `UIM_*` code (`UI.h:41`).
`CSBUI()` (`CSBUI.cpp:735`) is a big `switch` over those codes: it either handles
the message itself or queues it and drives the ported state machine through
`Dispatch()`. The machine's states are the `CODESTATE` enum in `Dispatch.h`
(`st_StartCSB`, `st_MagicSelection`, …), each corresponding to a
`TAG`-named function in the original.

The engine's clock comes from SDL. `__Timer_Callback` (`CSBlinux.cpp:142`)
fires a `UIM_TIMER`, which reaches `CheckVBL()` and the original
`vblInterrupt` (`CSBCode.cpp:901`) — the vertical-blank handler is where a tick of
game time is processed. Game speed is a table of VBLs per tick
(`CSBlinux.cpp:1548`).

All mutable engine state lives in one global struct, `d` (`extern DBank d;`,
`Data.h:1520`), mirroring the original's memory block. The save-relevant structs
are packed (`#pragma pack(1)` in `CSB.h:25`) and byte-order-sensitive; endianness
is selected in `CSBTypes.h`. That is why saves need the conversion tools in
[python-tools.md](python-tools.md).

```
            platform main
                 |
        SDL event / timer
                 |
       CSB_UI_MESSAGE (UIM_*)
                 |
             CSBUI()  ------> Dispatch() ----> TAG...() engine functions
                 |                                   |
            UI_Invalidate()                    mutate global `d`
                 |                                   |
        redraw via Viewport/Graphics  <-------------+
                 |
        SDL renderer / ImGui  +  AutoMap window
```

## Subsystem index

| Subsystem | Doc |
|-----------|-----|
| Party, monsters, combat, magic, timers, saving, DSA, Chaos | [engine-core.md](engine-core.md) |
| Graphics decoding, viewport, screens, ImGui, sound | [rendering.md](rendering.md) |
| Platform mains, UI layers, per-platform differences | [platform.md](platform.md) |
| Automap | [automap.md](automap.md) |
| Statistics | [statistics.md](statistics.md) |
| Game files, saves, config, tables | [data-formats.md](data-formats.md) |
| The `CodeNNNN.cpp` files and decode tables | [reference-disassembly.md](reference-disassembly.md) |
