# Architecture

MiniGit is a static library (`minigit_core`) plus a thin CLI. The library owns
all repository behavior; the CLI only parses arguments and formats output.

```
hash/  ──► objects/  ──► repo/  ──► cli/
              ▲            │
              └── diff/ ◄──┴──► merge/
```

`repo` depends on `objects` and `diff`; `merge` depends on `objects`, `diff`
and `repo::history`; `cli` sits on top of everything. Nothing depends on
`cli`.

## Module responsibilities

| Module | Owns | Does not do |
| --- | --- | --- |
| `core/error.h` | `MinigitError` | nothing else |
| `hash/sha1` | SHA-1 state machine | file I/O |
| `compress/huffman` | byte-level compression | decide what to compress |
| `objects/oid` | validated 20-byte ids | look things up |
| `objects/object` | `<type> <size>\0<payload>` encoding | compression |
| `objects/object_store` | sharding, atomic writes, Huffman | tree semantics |
| `objects/tree` | sorted directory entries | walking the filesystem |
| `objects/commit` | commit header + message | DAG traversal |
| `repo/index` | staged path → blob id | content hashing |
| `repo/repository` | init, refs, staging, trees, checkout, merge, export | parsing flags |
| `repo/history` | priority-queue DAG walk | rendering |
| `repo/ignore` | `.minignore` rules | applying them |
| `diff/*` | edit scripts and unified output | reading files |
| `merge/*` | base finding, text merge | writing the index |
| `cli/*` | flags, formatting, ASCII graph | storage decisions |

## Data flow of a commit

```
add(path)
  └─ read bytes                     objects::readFile
  └─ Object(Blob, bytes).oid()      SHA-1 over "<type> <size>\0<body>"
  └─ ObjectStore::write             Huffman-compress, write objects/ab/cdef…
  └─ Index::add(path, oid)          .minigit/index

commit(message)
  └─ Index::load
  └─ build TreeNode tree            split paths on '/'
  └─ writeTreeNode (post-order)     children first: Merkle construction
  └─ Commit{tree, parents, author}
  └─ ObjectStore::write             commit object
  └─ updateHeadRef                  refs/heads/<branch> or detached HEAD
```

## Invariants

1. **Content addressing.** An object's file name is the SHA-1 of its
   uncompressed serialized bytes. Reads re-hash and reject mismatches.
2. **Atomic objects.** Objects are written to a temporary file and renamed, so
   a crash can never leave a partial object.
3. **Deterministic trees.** Tree entries are sorted by name, so equal
   directory content always yields the same tree id (and the same commit id
   for the same metadata).
4. **Index = next tree.** `commit` never reads the working tree; it only
   converts the index into trees.
5. **Refs are files.** `HEAD` is either `ref: refs/heads/<name>` or a raw
   commit id (detached). Branch refs contain one commit id.
6. **Merge state is a file.** `MERGE_HEAD` exists only while a conflicted
   merge is unresolved; `commit` consumes it as the second parent.

## Error strategy

Every expected failure throws `MinigitError` with a user-facing sentence.
`main` catches it once and prints `minigit: error: …` to stderr with exit code
1. Corruption inside stored objects is detected on read by comparing the
re-hashed payload with the requested id.

## Checkout safety

`checkout` refuses to run unless the index matches HEAD and the working tree
matches the index. It also refuses to overwrite an untracked file that exists
in the target tree. Files tracked before but missing from the target are
deleted and their now-empty directories pruned.

## Why a facade

`Repository` is the only type that knows the physical layout
(`.minigit/...`). Every other module works with logical values: ids, trees,
commits, edit scripts. That keeps the algorithms unit-testable without
touching the filesystem (see `tests/lca_test.cpp`, `tests/three_way_test.cpp`,
`tests/myers_test.cpp`).
