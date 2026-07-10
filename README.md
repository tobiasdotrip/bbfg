# bbfg

`bbfg` is a small C rewrite experiment inspired by BFG Repo-Cleaner.

For now it prints refs and object ids, rebuilds HEAD trees, and can create a
rewritten commit without moving any ref.

Build:

```sh
make
make release
```

Test dependency: Criterion.

Run the tests:

```sh
make test
```

Run the rewrite benchmark:

```sh
make benchmark
BFG_JAR=/tmp/bfg-1.15.0.jar \
JAVA_BIN=/opt/homebrew/opt/openjdk/bin/java make benchmark
```

The comparison below was measured on macOS arm64 with 20,000 linear commits,
libgit2 1.9.4, clang `-O3`, BFG 1.15.0, and OpenJDK 26.0.1. Compilation time is
not included.

| tool | real | user | sys |
| --- | ---: | ---: | ---: |
| BBFG | 7.97 s | 0.48 s | 7.31 s |
| BFG | 4.99 s | 3.82 s | 3.76 s |

BFG was 1.60x faster in this run. Its CPU time is higher than its elapsed time
because the rewrite uses parallel workers.

Current commands:

```sh
./build/bbfg .
./build/bbfg --head-commit .
./build/bbfg --head-tree .
./build/bbfg --list-head-tree .
./build/bbfg --list-refs .
./build/bbfg --list-rewrite-refs .
./build/bbfg --walk-rewrite-commits .
./build/bbfg --rebuild-head-tree .
./build/bbfg --remove-head-entry path/to/file .
./build/bbfg --commit-without-entry path/to/file .
./build/bbfg --write-rewrite-ref path/to/file .
./build/bbfg --rewrite-head-history path/to/file .
./build/bbfg --rewrite-ref refs/heads/main --delete path/to/file .
./build/bbfg --rewrite-refs --delete path/to/file .
```

Git equivalents:

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
| `./build/bbfg --rewrite-refs --delete path/to/file .` | `git -C . for-each-ref refs/heads refs/tags`, rewrite each direct commit ref, then `git update-ref REF COMMIT` |
