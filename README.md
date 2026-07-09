# bbfg

`bbfg` is a small C rewrite experiment inspired by BFG Repo-Cleaner.

For now it only inspects a repository. Rewriting comes later.

Build:

```sh
make
```

Run the smoke test:

```sh
make test
```

Current inspection commands:

```sh
./build/bbfg .
./build/bbfg --head-commit .
./build/bbfg --head-tree .
./build/bbfg --list-head-tree .
./build/bbfg --list-rewrite-refs .
./build/bbfg --walk-rewrite-commits .
```
