# Building CSB on Windows

`build.cmd` in the repository root builds the SDL2 + Dear ImGui front end into
`build\win64\csb.exe`. It compiles the same sources as the Unix makefile, so the
game behaves the same on Windows as on Linux.

```powershell
cd C:\path\to\CSB
.\build.cmd            # build
.\build.cmd -Clean     # rebuild from scratch
.\build.cmd -Run       # build, then start in Dungeons\Dungeonmaster
```

The first build takes a minute or two; later builds only recompile what changed.

`build.cmd` is a thin wrapper around `build.ps1`. It exists because Windows
disables PowerShell scripts by default (`Get-ExecutionPolicy` reports
`Restricted`), which makes `.\build.ps1` fail with a `SecurityError`. The wrapper
passes `-ExecutionPolicy Bypass` for that one process only, so nothing on your
system is changed. If you prefer to call the script directly, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

To allow scripts permanently for your user account instead — a system-wide
change, and a security trade-off — run this once:

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

## What it needs

**A MinGW-w64 compiler.** One is already installed as part of the Qt kit:

```
C:\Qt\Tools\mingw1310_64\bin\g++.exe     (GCC 13.1.0, x86_64-w64-mingw32)
```

The script puts that directory on `PATH` itself. If you move to a machine
without Qt, install MinGW-w64 (MSYS2's `mingw-w64-x86_64-gcc` is a good source)
and change `$MinGW` at the top of `build.ps1`.

**SDL2 development files.** These are not in the repository. Download the
MinGW package from <https://github.com/libsdl-org/SDL/releases> — the file named
`SDL2-devel-<version>-mingw.tar.gz` — and unpack it so that this exists:

```
third_party\SDL2-2.30.9\x86_64-w64-mingw32\include\SDL2\SDL.h
```

The script copies `SDL2.dll` next to the executable, so the build output folder
is self-contained.

**Runtime DLLs.** The compiler links the C++ runtime dynamically, so
`csb.exe` also needs `libstdc++-6.dll`, `libgcc_s_seh-1.dll` and
`libwinpthread-1.dll`. These live in `C:\Qt\Tools\mingw1310_64\bin`, which is
*not* on the normal `PATH` — the build script adds it only for its own process,
so a program started from Explorer or a plain terminal would otherwise fail with
"libstdc++-6.dll was not found". The script copies the three DLLs into
`build\win64` alongside the executable, which keeps the folder portable. If you
move `csb.exe` somewhere else, move these four DLLs with it.

Dear ImGui, SDL_sound, `fnmatch` and the `Dirent` shim are already in `src\` and
need nothing extra.

## Running

The engine reads `dungeon.dat`, `graphics.dat` and `csb.cfg` from the *current
directory*, so start it from inside a game folder:

```powershell
cd Dungeons\Dungeonmaster
..\..\build\win64\csb.exe
```

`.\build.cmd -Run` does this for you. Saved games are written into that folder.

`csb.cfg` is required for keyboard and mouse bindings: its `[keys]` section is
where the scancodes are mapped (for example `5c`/`5e` are the numpad 4 and 6).
Without it the game starts but no keys move the party.

You can also double-click `build\win64\csb.exe` from Explorer, but it still reads
the game data from its working directory, which Explorer sets to the folder the
executable is in. Put the executable in the game folder, or use `-Run`.

### Mouse side buttons

The automap steps one level with the side buttons (back/forward, SDL `X1`/`X2`).
That path is the same on every platform, so it works under Windows with any mouse
whose driver actually reports five buttons. Virtual machines often do not: QEMU's
`usb-tablet`, for example, exposes exactly three buttons, so `X1`/`X2` never reach
Windows and nothing happens. In that case the input device or the remote console
is the cause, not CSB. A passed-through five-button mouse fixes it.

## How this differs from the Unix build

The Unix makefile has a cross-compiling branch (`target = x86_64-w64-mingw32`)
that also works under native MinGW, but it depends on `pkg-config`, a POSIX
shell and the Unix tools `sed`/`cp`/`rm` in its recipes. Those are not present
with a plain Windows MinGW install, so `build.ps1` drives the compiler directly
instead. It keeps the same defines (`-D_LINUX -D RGB655 -D SDL20 -DMINGW`), the
same include paths and the same object list.

Three source fixes were needed for GCC on Windows, because glibc's headers and
functions are not fully available in MinGW:

- `src/Recording.cpp` — added `#include <ctype.h>` for `toupper`.
- `src/SoundMixer.cpp` — added `#include <math.h>` for `sqrt`.
- `src/LinCSBUI.cpp` — MinGW has no `setenv()`, which the `--visibility-test`
  option uses to force SDL's dummy video driver. A `#ifdef MINGW` block maps it
  to `_putenv_s`, which always overwrites.

All three are harmless on Linux and macOS.

## The visibility test

`make visibility-test` runs the engine headless and checks what the map sees from
a set of saved positions. On Windows, run the equivalent by hand:

```powershell
cd Dungeons\Dungeonmaster
..\..\build\win64\csb.exe --visibility-test ..\..\src\visibility_tests\dungeon-master.txt
```

It loads the named saves without opening a window, prints a line per save, and
exits with 0 when every check passed. Verified with 554/554 and 78/78 checks.

The object list in `build.ps1` mirrors `OBJS` in `src\makefile`. When upstream adds
a source file, add it to `$CppSources` in `build.ps1` as well, or the new code will
be missing from the Windows build. Files that belong to other platforms
(`CSBwin.cpp`, `CSBMac.cpp`, `CSBraspian.cpp`, `CSBWinCE2002.cpp`, …) are
deliberately not in the list, matching the makefile's `SDL20` target.

## What does not work

`src\CSBwin.vcxproj` and `src\CMakeLists.txt` are stale and should not be used:
they reference files that no longer exist (`DungeonObjExport.cpp`,
`LinScreen.cpp`), and neither knows about SDL2, Dear ImGui or SDL_sound. The
Visual Studio project targets the old Win32 front end (`CSBwin.cpp`), which is a
different, older UI than the SDL one this script builds.

`make test` (the automap and missile unit tests) has no equivalent here yet; the
test sources are in `src\automap_tests` if you want to add one.
