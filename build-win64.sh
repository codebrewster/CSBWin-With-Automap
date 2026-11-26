#!/bin/bash
# Cross-compiles the Windows build on Linux with MinGW-w64 (the posix thread variant, as the Qt
# kit's compiler that build.ps1 uses): build/win64-cross/csb.exe and the four DLLs it needs.
#
#   ./build-win64.sh [-j JOBS] [-o OUTDIR] [-c]
#
#   -j JOBS    files compiled at once (default: the number of processors)
#   -o OUTDIR  where csb.exe goes (default build/win64-cross, so the exe a Windows machine builds
#              in build/win64 stays as it is)
#   -c         deletes the objects and the exe first
#
# The sources are the lists in build.ps1 ($CppSources, $ImguiSources, $SdlSoundSources,
# $CSources), read from there so there is only one list to keep up to date; the defines and the
# link flags are build.ps1's too. Needs: sudo apt install g++-mingw-w64-x86-64-posix, and SDL2
# for MinGW in third_party/SDL2-2.30.9 (see BUILDING-WINDOWS.md).
set -eu
root=$(cd "$(dirname "$0")" && pwd)
jobs=$(nproc 2>/dev/null || echo 4)
out="$root/build/win64-cross"
clean=0
while getopts "j:o:c" opt; do
  case $opt in
    j) jobs=$OPTARG ;;
    o) out=$OPTARG ;;
    c) clean=1 ;;
    *) sed -n '2,15p' "$0"; exit 64 ;;
  esac
done
case $out in /*) ;; *) out="$PWD/$out" ;; esac

CXX=x86_64-w64-mingw32-g++-posix
CC=x86_64-w64-mingw32-gcc-posix
sdl="$root/third_party/SDL2-2.30.9/x86_64-w64-mingw32"
src="$root/src"
obj="$out/obj"
command -v $CXX >/dev/null || { echo "no $CXX: sudo apt install g++-mingw-w64-x86-64-posix"; exit 1; }
[ -f "$sdl/include/SDL2/SDL.h" ] || { echo "no SDL2 in $sdl: see BUILDING-WINDOWS.md"; exit 1; }

# The names in one of build.ps1's lists: $Name = @( 'a', 'b', ... ), over one line or several
list() {
  awk -v name="\$$1" '$1 == name && /@\(/ { on = 1 } on { print } on && /\)/ { exit }' "$root/build.ps1" |
    grep -o "'[^']*'" | tr -d "'" | tr '\\' '/'
}
cpp=$(list CppSources); imgui=$(list ImguiSources); sound=$(list SdlSoundSources); csrc=$(list CSources)
[ -n "$cpp" ] && [ -n "$imgui" ] && [ -n "$sound" ] && [ -n "$csrc" ] || { echo "could not read the lists in build.ps1"; exit 1; }

defines="-D_LINUX -DRGB655 -DSDL20 -DMINGW"
includes="-Isdl_sound -Iimgui -Ifnmatch -I$sdl/include/SDL2 -I$sdl/include"
export CXXFLAGS="-O2 -std=c++11 $defines $includes"
export CFLAGS="-O2 $defines $includes"
LDFLAGS="-L$sdl/lib -lmingw32 -lSDL2main -lSDL2 -lcomdlg32 -lole32 -mwindows"

[ $clean = 1 ] && rm -rf "$obj" "$out/csb.exe"
mkdir -p "$obj/imgui" "$obj/sdl_sound" "$obj/fnmatch"

# As build.ps1: an object older than its source or than the newest header is built again
newest=$(find "$src" \( -name '*.h' -o -name '*.hpp' -o -name '*.inl' \) -printf '%T@\n' | sort -n | tail -1)
export newest CXX CC

# One line per file: compiler, source (relative to src), object
jobsfile=$(mktemp)
trap 'rm -f "$jobsfile"' EXIT
for n in $cpp;   do echo "$CXX $n.cpp $obj/$n.o"; done >> "$jobsfile"
for n in $imgui; do echo "$CXX imgui/$n.cpp $obj/imgui/$n.o"; done >> "$jobsfile"
for n in $sound; do echo "$CC sdl_sound/$n.c $obj/sdl_sound/$n.o"; done >> "$jobsfile"
for n in $csrc;  do echo "$CC $n $obj/fnmatch/$(basename "${n%.c}").o"; done >> "$jobsfile"

cd "$src"
compile() {
  local compiler=$1 source=$2 target=$3 flags
  if [ -f "$target" ]; then
    local t; t=$(stat -c %Y "$target")
    if [ "$t" -gt "$(stat -c %Y "$source")" ] && [ "$t" -gt "${newest%.*}" ]; then return 0; fi
  fi
  if [ "$compiler" = "$CXX" ]; then flags=$CXXFLAGS; else flags=$CFLAGS; fi
  echo "Compiling $source..."
  # shellcheck disable=SC2086
  $compiler $flags -c "$source" -o "$target" 2> "$target.log" || { echo "FAILED: $source"; grep -E 'error|fatal' "$target.log" | head -5; return 1; }
  rm -f "$target.log"
}
export -f compile
if ! xargs -P "$jobs" -L 1 bash -c 'compile "$@"' _ < "$jobsfile"; then
  echo "Some files failed to compile."
  exit 1
fi

echo "Linking csb.exe..."
objects=$(awk '{print $3}' "$jobsfile")
# shellcheck disable=SC2086
$CXX $objects $LDFLAGS -o "$out/csb.exe"

# The exe needs its DLLs beside it: SDL2, and the C++ runtime and pthread, which the compiler
# links dynamically (as the Qt kit's does)
runtime_dir=$(dirname "$($CXX -print-file-name=libstdc++-6.dll)")
for dll in "$sdl/bin/SDL2.dll" "$runtime_dir/libstdc++-6.dll" "$runtime_dir/libgcc_s_seh-1.dll" \
           "$($CXX -print-file-name=libwinpthread-1.dll)"; do
  [ -f "$dll" ] || { echo "runtime DLL not found: $dll"; exit 1; }
  cp "$dll" "$out/"
done
# Debian's runtime DLLs keep their debug information (libstdc++ 26 MB instead of 2)
x86_64-w64-mingw32-strip --strip-unneeded "$out/libstdc++-6.dll" "$out/libgcc_s_seh-1.dll" "$out/libwinpthread-1.dll"
echo "Built $out/csb.exe ($(du -k "$out/csb.exe" | cut -f1) KB)"
