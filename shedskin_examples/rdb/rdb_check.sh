#!/bin/bash
# pyc check driver for rdb (added, not an upstream file: see
# ../PYC_CHANGES.md). rdb rebuilds an iPod shuffle's database, so it needs
# an iPod: run in the corpus it finds no iPod_Control/iTunes and exits 1,
# under CPython and pyc alike. This builds a synthetic one in a scratch
# directory and runs the command it is given (`./rdb`, or
# `python3 rdb.py`) there, three times, printing rdb's output and a hex dump
# of every database file it writes. corpus_sweep.sh runs it for both arms.
#
# rdb reads only names, extensions, directory structure and sizes, never
# audio content, so the files are zero-filled at chosen sizes. Its shuffle is
# random.seed(1)-deterministic (MT19937, pyc_lib/random.py), so the
# sequence it writes is comparable too.
set -u
orig=$(pwd)
args=()
for a in "$@"; do  # the program path, made absolute: rdb runs elsewhere
  if [ -e "$a" ]; then args+=("$orig/$a"); else args+=("$a"); fi
done
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

mk() {  # mk PATH SIZE
  mkdir -p "$(dirname "$1")"
  head -c "$2" /dev/zero > "$1"
}
mkdir -p iPod_Control/iTunes
# Music/Fxx: the layout iTunes writes; its subdirectories are merged.
mk iPod_Control/Music/F00/track01.mp3 4000
mk iPod_Control/Music/F00/track02.mp3 5000
mk iPod_Control/Music/F00/song.m4a 6000
mk iPod_Control/Music/F01/a.wav 7000
mk iPod_Control/Music/F01/b.mp3 8000
mk iPod_Control/Music/F01/notes.txt 100          # not audio: skipped
mk iPod_Control/Music/F02/.hidden.mp3 100        # dot file: skipped
mk iPod_Control/Music/F02/c.mp3 9000
# user directories outside iPod_Control: each is a shuffle "domain"
mk Albums/Rock/one.mp3 10000
mk Albums/Rock/two.mp3 11000
mk Albums/Rock/three.mp3 12000
mk Albums/Jazz/x.m4a 13000
mk Albums/Jazz/y.m4a 14000
mk Podcasts/episode.m4b 15000                    # bookmark rule
mk Books/story.book.mp3 16000                    # *.book.??? rule
mk Audible/title.aa 17000                        # .aa rule
mk recycled/old.mp3 18000                        # /recycled/* ignore rule
mk README.txt 50

dump() {
  for f in iTunesSD iTunesPState iTunesStats iTunesShuffle; do
    p=iPod_Control/iTunes/$f
    if [ -f "$p" ]; then
      echo "-- $f $(wc -c < "$p") bytes"
      od -An -v -tx1 "$p"
    else
      echo "-- $f missing"
    fi
  done
}

run() {  # run LABEL ARGS...
  local label=$1; shift
  echo "== $label"
  "${args[@]}" -n "$@"
  echo "rc=$?"
  echo "-- log"
  cat rebuild_db.log.txt 2>/dev/null
  dump
}

run "fresh tree"
run "again, reusing the database it wrote"
run "plain shuffle, ignore existing database, volume 10" -s -f -v 10
