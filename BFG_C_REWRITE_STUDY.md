# BFG Repo-Cleaner Study for a C Rewrite

Source cloned in: `bfg-repo-cleaner/`

Upstream: <https://github.com/rtyley/bfg-repo-cleaner>

Current upstream commit inspected: `1bd9715 Post-release of v1.15.0 by @rtyley: set snapshot version`

## What BFG Does

BFG rewrites Git history to remove or replace unwanted objects while preserving normal Git structure.

Primary user operations:

- delete files by filename glob
- delete folders by folder-name glob
- remove blobs by id
- remove blobs larger than a threshold
- remove the N biggest blobs
- replace text in blob contents
- convert matching blobs to Git LFS pointers
- protect the latest version of selected refs, `HEAD` by default

The important product rule is object protection: by default BFG does not modify blobs reachable from the current protected refs. Users are expected to first clean the current tip, then let BFG clean older history.

## Source Structure

- `bfg/src/main/scala/com/madgag/git/bfg/cli/Main.scala`
  CLI entrypoint. Parses config, opens the repo, optionally prunes previous BFG output, runs `RepoRewriter.rewrite`.

- `bfg/src/main/scala/com/madgag/git/bfg/cli/CLIConfig.scala`
  Builds the cleaning pipeline from CLI flags.

- `bfg-library/src/main/scala/com/madgag/git/bfg/cleaner/RepoRewriter.scala`
  Walks all commits, applies object cleaning, then updates refs.

- `bfg-library/src/main/scala/com/madgag/git/bfg/cleaner/ObjectIdCleaner.scala`
  Core old-object-id to new-object-id mapper. Cleans trees, commits, tags, and memoizes every substitution.

- `bfg-library/src/main/scala/com/madgag/git/bfg/cleaner/treeblobs.scala`
  File/blob deletion and large-blob replacement behavior.

- `bfg-library/src/main/scala/com/madgag/git/bfg/cleaner/BlobTextModifier.scala`
  Text replacement inside eligible blobs.

- `bfg-library/src/main/scala/com/madgag/git/bfg/cleaner/protection/ProtectedObjectCensus.scala`
  Computes protected trees and blobs from protected refs.

## Execution Model

1. Resolve the repository and refs.
2. Compute protected objects from configured revisions, defaulting to `HEAD`.
3. Build a chain of cleaners from CLI flags.
4. Walk all commits in topological order, parents before children.
5. For each commit:
   - clean the commit tree recursively
   - clean parent ids through the same object-id mapper
   - optionally update old object ids mentioned in messages
   - insert a new commit object only if something changed
6. For each tree:
   - parse tree entries
   - apply entry-list cleaners, such as duplicate filename repair
   - apply blob-entry cleaners, such as delete file, replace big blob, replace text, LFS conversion
   - recursively clean subtrees
   - insert a new tree object only if entries changed
7. For each tag:
   - retarget it if the tagged object changed
   - rewrite object ids in the tag message
8. Update refs from old tips to new tips using non-fast-forward updates.
9. Report changed/deleted filenames and protected dirty data.

## Key Design Ideas to Preserve in C

- Object-id mapping is the center of the system: `old_oid -> new_oid`, identity means unchanged.
- Memoization is mandatory for speed and correctness. Once an object maps to a clean id, that clean id is also fixed as identity.
- Cleaning blobs in tree context is intentional. It gives access to filenames and file modes.
- Commit traversal must be parents-before-children so parent ids can already be mapped.
- Protected blobs and trees should be inserted into the memo table as identity mappings before cleaning begins.
- Only write new Git objects when content actually changed.
- Ref updates happen after all cleaning succeeds.

## C Rewrite Options

### Option A: Build on libgit2

Pros:

- Mature object database, revwalk, tree parsing, commit/tag builders, ref updates.
- Avoids implementing packfile/object IO at the start.
- Faster path to a correct BFG-like tool.

Cons:

- Need to understand libgit2 behavior around bare repos, packed objects, ref transactions, and object lookup performance.
- Some BFG-specific pack scanning for largest blobs may need custom iteration.

This is the recommended first path.

### Option B: Native Git Object Implementation

Pros:

- Full control over pack scanning, memory layout, and performance.
- Could become a very small, specialized cleaner.

Cons:

- Much larger implementation: loose objects, pack index, delta resolution, tree/commit/tag serialization, refs, reflogs, SHA-1/SHA-256 compatibility.
- High bug risk for a history-rewriter.

This is better as a later optimization, not the first rewrite.

## Proposed C Architecture

- `src/main.c`
  CLI parsing and command orchestration.

- `src/repo.c`
  Repository opening, ref listing, object database helpers.

- `src/protection.c`
  Protected ref/object census.

- `src/revwalk.c`
  Topological traversal setup.

- `src/oid_map.c`
  Hash map for old-to-new object ids, identity protection, memo stats.

- `src/cleaner.c`
  Cleaner pipeline interfaces and composition.

- `src/tree_cleaner.c`
  Tree parsing, blob-entry cleaning, subtree recursion, new tree writing.

- `src/blob_cleaner.c`
  Delete file, delete by blob id, replace large blob, text replacement, LFS pointer generation.

- `src/commit_cleaner.c`
  Commit parent/tree/message rewriting and new commit writing.

- `src/tag_cleaner.c`
  Tag retargeting and message rewriting.

- `src/report.c`
  CLI progress and summary reports.

## First Milestone

Implement the smallest useful tool:

```text
bfg-c --delete-files '*.class' /path/to/repo.git
```

Scope:

- bare repo support first
- SHA-1 only first
- `HEAD` protection first
- commit/tree rewrite
- ref update
- no text replacement, no LFS, no biggest-blob scan yet

This milestone proves the hard loop: read refs, walk commits, rewrite trees, rewrite commits, update refs.

## Licensing Note

Upstream BFG is GPL-3.0-or-later. A direct port or derivative implementation should be treated as GPL-compatible work. If the goal is a differently licensed C implementation, we should use a clean-room process and avoid translating source line-for-line.
