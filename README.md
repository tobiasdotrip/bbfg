# bbfg

`bbfg` is a small C rewrite experiment inspired by BFG Repo-Cleaner.

For now it prints refs and object ids, rebuilds HEAD trees, and can create a
rewritten commit without moving any ref.

Build:

```sh
make
```

Test dependency: Criterion.

Run the tests:

```sh
make test
```

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
