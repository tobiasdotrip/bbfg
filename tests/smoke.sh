#!/bin/sh

set -eu

bbfg=$1
tmpdir=$(mktemp -d)

cleanup()
{
	rm -rf "$tmpdir"
}
trap cleanup EXIT INT TERM

repo=$tmpdir/repo
git init -q "$repo"

git -C "$repo" config user.name "bbfg test"
git -C "$repo" config user.email "bbfg@example.invalid"

printf 'hello\n' > "$repo/file.txt"
git -C "$repo" add file.txt
git -C "$repo" commit -q -m "Initial commit"

"$bbfg" "$repo" >/tmp/bbfg-open.out
git_dir=$(git -C "$repo" rev-parse --absolute-git-dir)
git_dir=$(cd "$git_dir" && pwd -P)
printf 'bbfg: using repository: %s/\n' "$git_dir" >/tmp/bbfg-open.expected
diff -u /tmp/bbfg-open.expected /tmp/bbfg-open.out

"$bbfg" --help >/tmp/bbfg-help.out
grep '^usage: ' /tmp/bbfg-help.out >/dev/null

if "$bbfg" --head-commit --head-tree "$repo" >/tmp/bbfg-invalid.out 2>&1; then
	echo "expected duplicate commands to fail" >&2
	exit 1
fi

if "$bbfg" "$repo" --head-commit >/tmp/bbfg-invalid.out 2>&1; then
	echo "expected options after repo to fail" >&2
	exit 1
fi

"$bbfg" --head-commit "$repo" >/tmp/bbfg-head.out
git -C "$repo" rev-parse HEAD >/tmp/bbfg-head.expected
diff -u /tmp/bbfg-head.expected /tmp/bbfg-head.out

"$bbfg" --head-tree "$repo" >/tmp/bbfg-tree.out
git -C "$repo" rev-parse 'HEAD^{tree}' >/tmp/bbfg-tree.expected
diff -u /tmp/bbfg-tree.expected /tmp/bbfg-tree.out

"$bbfg" --rebuild-head-tree "$repo" >/tmp/bbfg-rebuilt-tree.out
diff -u /tmp/bbfg-tree.expected /tmp/bbfg-rebuilt-tree.out

"$bbfg" --list-head-tree "$repo" >/tmp/bbfg-list-tree.out
git -C "$repo" ls-tree HEAD >/tmp/bbfg-list-tree.expected
diff -u /tmp/bbfg-list-tree.expected /tmp/bbfg-list-tree.out

"$bbfg" --remove-head-entry file.txt "$repo" >/tmp/bbfg-remove-entry.out
git -C "$repo" mktree </dev/null >/tmp/bbfg-remove-entry.expected
diff -u /tmp/bbfg-remove-entry.expected /tmp/bbfg-remove-entry.out

"$bbfg" --list-rewrite-refs "$repo" >/tmp/bbfg-rewrite-refs.out
git -C "$repo" for-each-ref --format='%(refname)' refs/heads refs/tags \
	>/tmp/bbfg-rewrite-refs.expected
diff -u /tmp/bbfg-rewrite-refs.expected /tmp/bbfg-rewrite-refs.out

"$bbfg" --walk-rewrite-commits "$repo" >/tmp/bbfg-walk.out
git -C "$repo" rev-list --topo-order --reverse refs/heads/main \
	>/tmp/bbfg-walk.expected
diff -u /tmp/bbfg-walk.expected /tmp/bbfg-walk.out
