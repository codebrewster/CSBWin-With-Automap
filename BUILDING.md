# Building CSB on macOS and Linux

One makefile in `src/` builds the game on both systems. It needs a C++11 compiler, GNU make,
pkg-config and the development files of SDL2. Everything else (Dear ImGui, SDL_sound, miniz, tinyfiledialogs) is part of
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
| `make clean`     | Removes the object folders and `src/csb`                                 |

The optimized build keeps its objects in `src/obj-release`, the debug build and the tests in
`src/obj`, so you can switch between them without `make clean`.

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

## The calculator

The calculator in `src/calculator` is built on its own, with CMake. It needs a C++17 compiler and
Qt 6 or Qt 5 (Widgets only); it does not need SDL2 or anything of the game build.

```
cd src/calculator
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build -j8
./build/dm_calculator
ctest --test-dir build
```

Leave `CMAKE_PREFIX_PATH` out when Qt comes from the system's packages (`qt6-base-dev` on Debian
and Ubuntu, `brew install qt` on macOS). On Windows, run the same commands from a prompt of the
compiler that Qt was built for. Without Qt, CMake builds only the checks (`calc_tests`) and the
two command-line tools, `calc_dump` (the tables of a `graphics.dat`) and `calc_save` (what the
calculator reads out of a savegame).

### The calculator in a web browser

The calculator also builds for the browser with Qt for WebAssembly. It needs Qt's
"WebAssembly (single-threaded)" kit and the Emscripten version that kit was made with (4.0.7 for
Qt 6.10; `qt-cmake` names it if it differs), installed with
[emsdk](https://emscripten.org/docs/getting_started/downloads.html):

```
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install 4.0.7 && ~/emsdk/emsdk activate 4.0.7

cd src/calculator
source ~/emsdk/emsdk_env.sh
/path/to/Qt/6.x/wasm_singlethread/bin/qt-cmake -S . -B build-wasm -DQT_HOST_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build-wasm --target dm_calculator -j8
```

`build-wasm` then holds `dm_calculator.html`, `.js`, `.wasm` and `qtloader.js`. A browser loads
them only from a web server, not from the disk; to try them:

```
cd build-wasm && python3 -m http.server 8765 --bind 127.0.0.1
```

and open http://127.0.0.1:8765/dm_calculator.html. In the browser, "Open savegame…" reads the
file the visitor picks; no `graphics.dat` or `csb.cfg` comes with it, so the built-in tables and
the default rules apply (both can be changed in the side panel). The browser's own zoom
(Ctrl + and Ctrl −) replaces the Zoom button.

## What was checked

- macOS on Apple Silicon with Apple clang and SDL2 2.32 from Homebrew: both builds, the tests,
  and playing the game.
- Debian 12 on arm64 with GCC 12.2 and SDL2 2.26: both builds and the tests. The map window has
  not been tried on a Linux desktop yet.
