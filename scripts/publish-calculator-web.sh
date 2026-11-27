#!/bin/bash
# Builds the calculator for the web browser (Qt for WebAssembly) and puts it together as a
# folder for a web server; with -p it also adds that folder to the GitHub Pages site, as
# calculator/ on the gh-pages branch (https://codebrewster.github.io/CSBwin-Plus/calculator/).
#
#   scripts/publish-calculator-web.sh [-p] [-o SITEDIR]
#
#   -p          publish: commit the folder as calculator/ on top of origin/gh-pages and push
#   -o SITEDIR  where the folder goes (default src/calculator/build-wasm/site)
#
# Needs Emscripten in the version Qt's WebAssembly kit was made with (see BUILDING.md); the
# paths below can be set from outside:
#   EMSDK_DIR   default ~/emsdk
#   QT_WASM     default ~/Qt/6.10.1/wasm_singlethread
#   QT_HOST     default ~/Qt/6.10.1/gcc_64
#
# To try the folder: cd SITEDIR && python3 -m http.server 8765 --bind 127.0.0.1, then open
# http://127.0.0.1:8765/ (a browser does not load the program from the disk).
#
# Publishing adds one commit to gh-pages and leaves everything else there (the records pages)
# as it is. Republishing the records from scratch must carry calculator/ over.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
calc="$root/src/calculator"
emsdk=${EMSDK_DIR:-$HOME/emsdk}
qt_wasm=${QT_WASM:-$HOME/Qt/6.10.1/wasm_singlethread}
qt_host=${QT_HOST:-$HOME/Qt/6.10.1/gcc_64}
build="$calc/build-wasm"
site="$build/site"
publish=0
while getopts "po:" opt; do
  case $opt in
    p) publish=1 ;;
    o) site=$OPTARG ;;
    *) sed -n '2,20p' "$0"; exit 1 ;;
  esac
done
cmake=/usr/bin/cmake   # a cmake earlier on PATH may be a broken pip shim
[ -x "$cmake" ] || cmake=cmake

# shellcheck disable=SC1091
source "$emsdk/emsdk_env.sh" > /dev/null
"$qt_wasm/bin/qt-cmake" -S "$calc" -B "$build" -DQT_HOST_PATH="$qt_host" -DCMAKE_BUILD_TYPE=Release > /dev/null
"$cmake" --build "$build" --target dm_calculator -j "$(nproc 2>/dev/null || echo 4)"

rm -rf "$site"
mkdir -p "$site"
cp "$calc/web/index.html" "$calc/web/favicon.svg" "$calc/ui/fonts/OFL.txt" "$site/"
cp "$build/dm_calculator.js" "$build/dm_calculator.wasm" "$build/qtloader.js" "$site/"
echo "Site in $site:"
ls -l "$site"

[ "$publish" = 1 ] || exit 0

# A worktree of gh-pages beside the repository's own, so main's working tree is not touched
pages=$(mktemp -d)
trap 'git -C "$root" worktree remove --force "$pages" 2> /dev/null || true; rm -rf "$pages"' EXIT
git -C "$root" fetch -q origin gh-pages
git -C "$root" worktree add -q --detach "$pages" origin/gh-pages
rm -rf "$pages/calculator"
cp -r "$site" "$pages/calculator"
git -C "$pages" add -A calculator
if git -C "$pages" diff --cached --quiet; then
  echo "gh-pages already has this calculator; nothing to publish"
  exit 0
fi
source_commit=$(git -C "$root" rev-parse --short HEAD)
# GitHub declines pushes that show a private e-mail address: commit as the noreply one
git -C "$pages" -c user.name="$(git -C "$root" config user.name)" \
  -c user.email=349450296+codebrewster@users.noreply.github.com \
  commit -q -m "Calculator for the browser, built from $source_commit"
git -C "$pages" -c credential.helper= -c credential.helper='!gh auth git-credential' \
  push -q origin HEAD:gh-pages
echo "Published: https://codebrewster.github.io/CSBwin-Plus/calculator/"
