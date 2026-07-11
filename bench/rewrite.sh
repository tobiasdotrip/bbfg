#!/bin/sh

set -eu

commits=${1:-20000}
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/bbfg-benchmark.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

template="$tmpdir/template.git"
git init -q --bare "$template"

awk -v commits="$commits" '
  BEGIN {
    print "blob"
    print "mark :1"
    print "data 7"
    print "remove\n"
    print "blob"
    print "mark :2"
    print "data 5"
    print "keep\n"

    for (i = 1; i <= commits; i++) {
      print "commit refs/heads/main"
      printf "mark :%d\n", i + 2
      printf "author Bench <bench@example.invalid> %d +0000\n", 1000000000 + i
      printf "committer Bench <bench@example.invalid> %d +0000\n", 1000000000 + i
      print "data 1"
      print "x"

      if (i == 1) {
        print "M 100644 :1 delete.txt"
        print "M 100644 :2 keep.txt"
      } else {
        printf "from :%d\n", i + 1
      }

      print ""
    }

    print "done"
  }
' | git -C "$template" fast-import --quiet
git -C "$template" symbolic-ref HEAD refs/heads/main

cp -R "$template" "$tmpdir/changed.git"
cp -R "$template" "$tmpdir/unchanged.git"
original_head=$(git -C "$template" rev-parse refs/heads/main)

printf 'rewrite %s commits\n' "$commits"
/usr/bin/time -p ./build/bbfg \
  --rewrite-refs --delete-files delete.txt "$tmpdir/changed.git" >/dev/null
test -z "$(git -C "$tmpdir/changed.git" ls-tree -r --name-only HEAD -- delete.txt)"
git -C "$tmpdir/changed.git" fsck --full --no-progress >/dev/null

printf 'scan %s unchanged commits\n' "$commits"
/usr/bin/time -p ./build/bbfg \
  --rewrite-refs --delete-files absent.txt "$tmpdir/unchanged.git" >/dev/null
test "$original_head" = \
  "$(git -C "$tmpdir/unchanged.git" rev-parse refs/heads/main)"

if [ -n "${BFG_JAR:-}" ]; then
  cp -R "$template" "$tmpdir/bfg.git"
  java_bin=${JAVA_BIN:-java}

  printf 'BFG rewrite %s commits\n' "$commits"
  /usr/bin/time -p sh -c \
    '"$1" -jar "$2" --delete-files delete.txt --no-blob-protection "$3" \
      >/dev/null 2>&1' \
    sh "$java_bin" "$BFG_JAR" "$tmpdir/bfg.git"
  test -z "$(git -C "$tmpdir/bfg.git" ls-tree -r --name-only HEAD -- delete.txt)"
  git -C "$tmpdir/bfg.git" fsck --full --no-progress >/dev/null
fi
