#!/usr/bin/env bash
if [ $# -eq 0 ]
  then
    echo "Pass a directory with arbitrary files to be used as test data"
	exit 1
fi

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

AFL_IN="../build/afl_in"
AFL_OUT="../build/afl_out"

rm -f "$AFL_IN/*"

mkdir -p ../build
mkdir -p ../build/afl_in
mkdir -p ../build/afl_out

afl-clang-fast -O2 afl.c -o ../build/afl

SRC_DIR="$1"
SRC_DIR="${SRC_DIR%/}"

rand_exp() {
    local max=$((1024*1024))
    local bias="$1"  # larger bias -> smaller numbers
    [ -z "$bias" ] && bias=1
    local v=1
    for ((i=0; i<bias; i++)); do
        v=$(awk -v x="$v" -v r="$(od -An -N4 -tu4 < /dev/urandom)" '
            BEGIN { print (r/4294967295.0) * x }')
    done
    local result=$(awk -v v="$v" -v max="$max" 'BEGIN { print int(v * max) + 1 }')
    echo "$result"
}

for src in "$SRC_DIR"/*; do
    [ -f "$src" ] || continue
    filename="$(basename "$src")"
    dst="$AFL_IN/$filename"
    touch "$dst"
    filesize=$(stat -c%s "$src")
    if [ "$filesize" -eq 0 ]; then
        continue
    fi
    count=$(rand_exp 5)
    if [ "$count" -gt "$filesize" ]; then
        count="$filesize"
    fi
    offset=0
    echo "Copying $count bytes from $filename into $dst"
	head -c "$count" "$src" > "$dst"
	
	../build/afl c < "$dst" > "$dst.compressed"
done

if [ -z "$( ls -A "$AFL_IN" )" ]; then
    echo "Pass a directory with arbitrary files to be used as test data"
	exit 1
fi	

afl-fuzz -i ../build/afl_in -o "$AFL_OUT" -- ../build//afl
