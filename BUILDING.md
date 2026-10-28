# Building CSB on macOS and Linux

One makefile in `src/` builds the game on both systems. It needs a C++11 compiler, GNU make,
pkg-config and the development files of SDL2. Everything else (Dear ImGui, SDL_sound) is part of
this repository.

For Windows see [BUILDING-WINDOWS.md](BUILDING-WINDOWS.md).

## What you need

**macOS**

```sh
xcode-select --install          # compiler and make
brew install sdl2 pkg-config
```

**Linux** (Debian, Ubuntu)

```sh
sudo apt install g++ make pkg-config libsdl2-dev
```

Other distributions need the same four things under their own package names.

## Build

```sh
cd src
make RELEASE=1
```

The result is the executable `src/csb`. Add `-j8` (or your number of cores) to build faster.

| Command          | What it does                                                             |
|------------------|--------------------------------------------------------------------------|
| `make RELEASE=1` | The optimized build, for playing                                         |
| `make`           | The debug build with the address sanitizer: slower, catches memory errors |
| `make test`      | Builds and runs the tests of the units that need neither engine nor SDL  |
| `make visibility-test` | Runs the map's visibility cases with the engine, without a window; needs the game data and the save in `Dungeons/Dungeonmaster` |
| `make clean`     | Removes `src/obj` and `src/csb`                                          |

The debug and the optimized build share `src/obj`. Run `make clean` when you switch from one to
the other.

## Run

The engine reads its data files and `csb.cfg` from the current directory, so start it from inside
a game folder:

```sh
cd <game folder>
<path to this repository>/src/csb
```

A game folder holds:

- `dungeon.dat` and `graphics.dat` of the game. They are not in this repository; `README.md`
  says where to get them.
- `csb.cfg`, `en.txt`, `fr.txt` and the folder `locale`, copied from the root of this repository.
- Optionally `sounds`, `fonts` and `lsb.bmp` from the binary packages. The game runs without
  them.

Saved games are written into the game folder. The dialog for loading a saved game only lists
files whose name starts with `csb`.

## macOS: a double-clickable application

```sh
macos/build_app.sh "Dungeon Master" <game folder> <output folder>
```

The script builds the optimized executable itself, starting with a `make clean`, and puts it
together with the game folder and its own copy of SDL2 into `<output folder>/Dungeon Master.app`.
The application copies its game files to `~/Library/Application Support/CSB/Dungeon Master` on
the first launch and keeps saved games and settings there.

## What was checked

- macOS on Apple Silicon with Apple clang and SDL2 2.32 from Homebrew: both builds, the tests,
  and playing the game.
- Debian 12 on arm64 with GCC 12.2 and SDL2 2.26: both builds and the tests. The map window has
  not been tried on a Linux desktop yet.
