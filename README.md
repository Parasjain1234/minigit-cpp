# minigit

A Git-like version control system written from scratch in C++17, using only
the C++ standard library (plus GoogleTest for the test suite).

> **Status: Phase 1 of 5 complete.** Content-addressable object storage,
> SHA-1 from scratch, `init`, `add`, `commit` and `status` work. `log`,
> branches, `checkout`, diff and merge follow in later phases.

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
```

## Layout

```
src/
  core/error.h            MinigitError, the single user-facing failure type
  hash/sha1.{h,cpp}       SHA-1 from scratch (incremental, 64-byte blocks)
  objects/                oid, object, object_store, tree, commit
  repo/                   index + repository facade
  cli/                    command dispatch and output formatting
tests/                    GoogleTest suite
```

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
| 1 | Project structure, SHA-1, object storage, init/add/commit/status |
| 2 | log, ASCII graph, branches, checkout |
| 3 | LCS diff (DP) and Myers diff |
| 4 | Merge-base via BFS, 3-way merge, conflict markers |
| 5 | Huffman compression, `.minignore`, benchmarks, full README |
