# minigit

A Git-like version control system written from scratch in C++17, using only
the C++ standard library (plus GoogleTest for the test suite).

> **Status: Phase 4 of 5 complete.** Content-addressable object storage,
> SHA-1 from scratch, `init`, `add`, `commit`, `status`, `log`,
> `log --graph`, `branch`, `checkout`, `diff` and `merge` work. Huffman
> compression, `.minignore`, benchmarks and the full docs land in Phase 5.

## Why this exists

MiniGit is a compact model of how Git works internally. Every piece is
written by hand so the data structures are visible: SHA-1 padding and block
processing, the Merkle tree that represents a directory, the commit DAG, and
(later) LCS/Myers diffing and a BFS merge-base over the DAG.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Requires CMake 3.16+ and a compiler with `<filesystem>` support
(GCC 9+, Clang 9+, MSVC 2019+).

## Usage

```bash
mkdir demo && cd demo
minigit init
echo "hello" > hello.txt
minigit add hello.txt
minigit commit -m "first commit"
minigit status

minigit branch feature
minigit checkout feature
minigit log --graph
minigit checkout main

minigit diff              # working tree vs index (LCS)
minigit diff --staged     # index vs HEAD
minigit diff --myers      # same, using Myers instead of the DP table

minigit merge feature     # fast-forward or three-way merge
```

## Layout

```
src/
  core/error.h            MinigitError, the single user-facing failure type
  hash/sha1.{h,cpp}       SHA-1 from scratch (incremental, 64-byte blocks)
  objects/                oid, object, object_store, tree, commit
  diff/                   edit scripts, LCS, Myers, unified rendering
  merge/                  merge-base (BFS) and three-way merge
  repo/                   index, repository facade, history walk
  cli/                    command dispatch, ASCII graph renderer
tests/                    GoogleTest suite
```

## Commit history as a graph

Each commit stores the ids of its parents, so history is a DAG rather than a
list. `log` walks it with a priority queue: the newest commit is emitted
first, parents are pushed only if they have not been seen, and a merge
commit simply pushes two parents. That gives a best-first traversal that
visits every reachable commit exactly once and never duplicates shared
history.

`log --graph` keeps a vector of lanes, one per branch line still in flight.
The first parent continues in the current lane; extra parents open new lanes
drawn with a backslash arm; a lane that reaches a commit already drawn
elsewhere is closed with a slash connector.

## Diffing two ways

`lcsDiff` builds an `(n+1) x (m+1)` dynamic programming table where cell
`(i, j)` is the length of the longest common subsequence of the first `i` old
lines and the first `j` new lines. Backtracking from `(n, m)` emits `=`,
`-` and `+` steps. It is easy to prove correct but costs O(n*m) time and
memory — two 10,000 line files need 100M cells.

`myersDiff` uses Myers' greedy furthest-reaching-path algorithm: it only
explores the diagonals reachable with `D` edits, keeping a `V` array of the
furthest `x` per diagonal and snapshotting it for backtracking. Cost is
O((N+M) · D) time and O(D²) memory in the snapshots, which is dramatically
cheaper for typical edits (a one-line change has `D = 2`).

`renderUnifiedDiff` groups the resulting edit script into `@@` hunks with
three lines of context, merging hunks closer than twice the context window.
It automatically falls back to Myers when the LCS table would exceed four
million cells.

## Merging (graph + DP)

`merge` first asks whether one side is already an ancestor of the other; if
so it fast-forwards. Otherwise it needs the **merge base**: the deepest
commit both branches share.

`findMergeBase` runs BFS from `ours` recording every ancestor's distance,
then BFS from `theirs` level by level. Any commit present in both maps is a
candidate; the one minimising `distanceA + distanceB` is the lowest common
ancestor. That is O(V + E) over the reachable history, once per direction.

With `base`, `ours` and `theirs` it does a three-way merge per file, using
the DP diff twice (base→ours, base→theirs). Each edit script is collapsed
into replacement blocks anchored on base line ranges; the two sorted block
lists are walked once, so clean changes from both sides are combined in
linear time. Overlapping blocks conflict and are written as:

```
<<<<<<< ours
our version
=======
their version
>>>>>>> theirs
```

A conflicted merge writes `.minigit/MERGE_HEAD`; the next `commit` picks it
up as a second parent, so the resulting commit really is a merge in the DAG.

## On-disk format

```
.minigit/
  HEAD                    "ref: refs/heads/main"
  config                  user.name / user.email
  index                   "<object-id> <path>" per staged file
  objects/ab/cdef...      zlib-free objects, sharded by first 2 hex chars
  refs/heads/main         commit id of the branch tip
```

An object is stored as `<type> <size>\0<payload>`, and its file name is the
SHA-1 of exactly those bytes. Because a tree stores the object ids of its
children, changing one file changes every id above it — that is the Merkle
property that makes history trustworthy.

## Roadmap

| Phase | Scope |
| ----- | ----- |
| 1 ✅ | Project structure, SHA-1, object storage, init/add/commit/status |
| 2 ✅ | log, ASCII graph, branches, checkout |
| 3 ✅ | LCS diff (DP), Myers diff, unified output |
| 4 ✅ | Merge-base via BFS, 3-way merge, conflict markers |
| 3 | LCS diff (DP) and Myers diff |
| 4 | Merge-base via BFS, 3-way merge, conflict markers |
| 5 | Huffman compression, `.minignore`, benchmarks, full README |
