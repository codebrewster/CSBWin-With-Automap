#!/bin/bash
# Writes map.html and statistics.html for every playfile.log below a folder, as fast as the
# machine allows: each replay runs headless at top speed (csb --export-html), several at once.
#
#   tools/export-all-html.sh [-j JOBS] [-o OUTDIR] [-c CSB] [-t SECONDS] [GAMES_DIR [NAME-FILTER]]
#
#   GAMES_DIR    where the movies are (default ~/Games/CSB-Games); every folder holding a
#                playfile.log (any case; symbolic links are followed) is one game
#   NAME-FILTER  only the games whose folder path contains this text (grep -i)
#   -o OUTDIR    where the pages go (default GAMES_DIR/html): OUTDIR/<folder path>/map.html
#                and statistics.html
#   -j JOBS      games at once (default: the number of processors)
#   -c CSB       the program (default src/csb of this repository, a RELEASE=1 build)
#   -t SECONDS   gives up on one game after this long (default 1800)
#
# The game files are copied to a work folder first (the program writes beside them), so the
# movies stay as they are. Prints one line per game and a summary; the exit code is the number
# of games that failed.
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
jobs=$(nproc 2>/dev/null || echo 4)
csb="$here/src/csb"
limit=1800
out=""
while getopts "j:o:c:t:" opt; do
  case $opt in
    j) jobs=$OPTARG ;;
    o) out=$OPTARG ;;
    c) csb=$OPTARG ;;
    t) limit=$OPTARG ;;
    *) sed -n '2,20p' "$0"; exit 64 ;;
  esac
done
shift $((OPTIND - 1))
games=${1:-$HOME/Games/CSB-Games}
filter=${2:-}
[ -x "$csb" ] || { echo "no program at $csb (make RELEASE=1 in src)"; exit 64; }
games=$(cd "$games" && pwd)
[ -n "$out" ] || out="$games/html"
mkdir -p "$out"
out=$(cd "$out" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/csb-export.XXXXXX")
trap 'rm -rf "$work"' EXIT

one() { # one PLAYFILE: runs a single game; prints "name: result"
  playfile=$1
  dir=$(dirname "$playfile")
  rel=${dir#"$games"/}
  dest="$out/$rel"
  w=$(mktemp -d "$work/game.XXXXXX")
  cp -a --reflink=auto "$dir/." "$w/" 2>/dev/null
  [ -f "$w/playfile.log" ] || cp "$playfile" "$w/playfile.log"
  [ -f "$w/csb.cfg" ] || cp "$here/csb.cfg" "$w/csb.cfg"
  start=$(date +%s)
  msg=$(cd "$w" && timeout -s KILL "$limit" "$csb" --directory . --play playfile.log --export-html "$dest" 2>&1 | grep -a "export-html" | tail -1)
  code=$?
  secs=$(( $(date +%s) - start ))
  rm -rf "$w"
  if [ -f "$dest/map.html" ] && [ -f "$dest/statistics.html" ] && echo "$msg" | grep -q "written to"; then
    echo "ok   ${secs}s  $rel  (${msg#export-html: })"
  else
    echo "FAIL ${secs}s  $rel  (${msg:-no answer, killed or crashed})"
  fi
}
export -f one
export here csb limit out games work

# Longest first (the biggest playfiles), so that the last game does not run on alone
find -L "$games" -iname 'playfile.log' -not -path "$out/*" -not -path "*/Back up*" -printf '%s %p\n' 2>/dev/null |
  sort -rn | cut -d' ' -f2- | { if [ -n "$filter" ]; then grep -i -- "$filter"; else cat; fi; } |
  tr '\n' '\0' | xargs -0 -P "$jobs" -I{} bash -c 'one "$1"' _ {} | tee "$work/results.txt"
failed=$(grep -c '^FAIL' "$work/results.txt")
total=$(wc -l < "$work/results.txt")
echo "$((total - failed)) of $total games done, the pages are in $out"
exit "$failed"
