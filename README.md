# bbfg

`bbfg` is a small C99/libgit2 rewrite tool inspired by BFG Repo-Cleaner.

It can inspect a repository, rebuild trees, rewrite commit history, remove
paths or filenames, strip large blobs, and update local branches and annotated
tags.

## Build

The build uses a Makefile. `libgit2` is required through `pkg-config`.
Criterion is required for the test target.

```sh
make
make release
make test
make test TEST_ARGS='--filter=cli/rewrite_combined_filters'
make test-sanitize
make tidy
make format
make compdb
```

## Rewrite

Inspection commands:

```sh
./build/bbfg .
./build/bbfg --head-commit .
./build/bbfg --head-tree .
./build/bbfg --list-head-tree .
./build/bbfg --list-refs .
./build/bbfg --list-rewrite-refs .
./build/bbfg --walk-rewrite-commits .
```

Tree and commit commands:

```sh
./build/bbfg --rebuild-head-tree .
./build/bbfg --remove-head-entry path/to/file .
./build/bbfg --commit-without-entry path/to/file .
./build/bbfg --write-rewrite-ref path/to/file .
./build/bbfg --rewrite-head-history path/to/file .
```

Rewrite one ref or all rewriteable refs:

```sh
./build/bbfg --rewrite-ref refs/heads/main --delete path/to/file .
./build/bbfg --rewrite-ref refs/tags/v1 --delete-files filename .
./build/bbfg --rewrite-refs --delete path/to/file .
```

Only direct refs under `refs/heads` and `refs/tags` are updated. Symbolic refs
such as `HEAD`, remote-tracking refs, and refs that do not peel to a commit are
not rewrite targets.

History rewrites protect the tree of each rewritten tip by default. Older
commits are filtered, while a protected tip keeps its tree; its commit ID can
still change because its parents were rewritten. With `--rewrite-refs`, this
rule applies independently to every branch and tag tip in the rewrite set.
Use `--no-blob-protection` to filter the tips too. This option is available on
history rewrite commands only; the one-commit commands are explicit tree
operations and do not use tip protection.

Filters are additive and can be repeated in the same command:

```sh
./build/bbfg --rewrite-refs \
  --delete-files filename \
  --strip-blobs-bigger-than 100M \
  .
```

For a complete rewrite that also filters the current tips:

```sh
./build/bbfg --rewrite-refs \
  --strip-blobs-bigger-than 100M \
  --no-blob-protection \
  .
```

`--delete` removes one exact path, `--delete-files` matches a filename at any
depth, and `--strip-blobs-bigger-than` removes blobs strictly larger than the
limit. Size suffixes `K`, `M`, `G`, and `T` use base 1024.

Before rewriting, work on a fresh clone or keep a backup. Review the rewritten
refs before pushing them, and run `git reflog expire` followed by `git gc` only
when the old objects are no longer needed.

## Git equivalents

These commands show the closest Git operation; they are not always exact
replacements for the full rewrite.

| bbfg | Git |
| --- | --- |
| `./build/bbfg .` | `git -C . rev-parse --absolute-git-dir` |
| `./build/bbfg --head-commit .` | `git -C . rev-parse HEAD` |
| `./build/bbfg --head-tree .` | `git -C . rev-parse 'HEAD^{tree}'` |
| `./build/bbfg --list-head-tree .` | `git -C . ls-tree HEAD` |
| `./build/bbfg --list-refs .` | `git -C . for-each-ref --format='%(refname)'` |
| `./build/bbfg --list-rewrite-refs .` | `git -C . for-each-ref --format='%(refname)' refs/heads refs/tags` |
| `./build/bbfg --walk-rewrite-commits .` | `git -C . rev-list --topo-order --reverse --branches --tags` |
| `./build/bbfg --rebuild-head-tree .` | `git -C . read-tree HEAD && git -C . write-tree` |
| `./build/bbfg --remove-head-entry path/to/file .` | `git -C . read-tree HEAD && git -C . rm --cached path/to/file && git -C . write-tree` |
| `./build/bbfg --commit-without-entry path/to/file .` | `git -C . read-tree HEAD && git -C . rm --cached path/to/file && git -C . write-tree && git -C . commit-tree TREE -p HEAD` |
| `./build/bbfg --write-rewrite-ref path/to/file .` | `git -C . update-ref refs/heads/bbfg-rewrite COMMIT` |
| `./build/bbfg --rewrite-head-history path/to/file .` | `git -C . rev-list --topo-order --reverse HEAD`, then `git commit-tree` for each commit |
| `./build/bbfg --rewrite-ref refs/heads/main --delete path/to/file .` | rewrite commits reachable from `refs/heads/main`, then `git update-ref refs/heads/main COMMIT` |
| `./build/bbfg --rewrite-ref refs/heads/main --strip-blobs-bigger-than 100M .` | `git filter-repo --strip-blobs-bigger-than 100M` |
| `./build/bbfg --rewrite-refs --delete path/to/file .` | `git filter-repo --invert-paths --path path/to/file` |
| `./build/bbfg --rewrite-refs --delete-files filename .` | `git filter-repo --invert-paths --path filename --use-base-name` |
| `./build/bbfg --rewrite-refs --delete-files filename --strip-blobs-bigger-than 100M .` | combine the two corresponding `git filter-repo` filters |

## Benchmark

```sh
make benchmark
BFG_JAR=/tmp/bfg-1.15.0.jar \
JAVA_BIN=/opt/homebrew/opt/openjdk/bin/java make benchmark
```

The repository contains a benchmark script for comparing a 20,000-commit
linear history with BFG. Results depend on the machine, libgit2 version, Java
version, and build flags.
