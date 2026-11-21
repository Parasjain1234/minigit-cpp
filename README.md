# minigit

A Git-like version control system written from scratch in **C++17**, using only
the standard library (`std::filesystem`, `std::optional`, `std::regex`, ...)
plus GoogleTest for the test suite.

Every core mechanism is implemented by hand: SHA-1, the content-addressable
object store, Merkle trees, the commit DAG, LCS and Myers diffing, a BFS
merge-base, three-way merge and Huffman compression.

> **All five phases are complete.** `init`, `add`, `commit`, `status`, `log`,
> `log --graph`, `branch`, `checkout`, `diff`, `merge` and `export` work, with
> 123 unit tests and a deployable web UI in `web/`.
>
> **Live demo:** <https://minigit-bpye3c-preview.freebuff.site>

---

## Why this project exists

Git is usually a black box. MiniGit rebuilds the parts that matter — object
storage, history, diffs, merges — so the data structures are visible and
testable. It is deliberately small, but it is not a mock: the CLI really
creates repositories, really merges branches, and really writes conflict
markers.

## Quick start

```bash
cmake -S . -B build -G Ninja      # any generator works
cmake --build build
ctest --test-dir build --output-on-failure
```

Requires CMake 3.16+ and a compiler with `<filesystem>` support
(GCC 9+, Clang 9+, MSVC 2019+). GoogleTest is fetched automatically when not
installed.

```bash
mkdir demo && cd demo
minigit init

echo "hello" > hello.txt
minigit add hello.txt
minigit commit -m "first commit"

minigit branch feature
minigit checkout feature
echo "more" >> hello.txt
minigit add hello.txt
minigit commit -m "feature work"

minigit checkout main
minigit merge feature
minigit log --graph
```

## Commands

| Command | Description |
| --- | --- |
| `init` | Create `.minigit/` with an object store, HEAD and refs |
| `add <path>...` | Hash files into blobs and stage them (directories recurse) |
| `commit -m "msg"` | Write trees + a commit object, advance the branch |
| `status` | Staged, unstaged and untracked changes |
| `log [--graph]` | Walk the DAG newest-first, optionally as ASCII lanes |
| `branch [<name>]` | List branches, or create one at HEAD |
| `checkout <branch\|commit>` | Switch branches or detach at a commit |
| `diff [--staged] [--myers]` | Unified diff via LCS or Myers |
| `merge <branch> [-m msg]` | Fast-forward or three-way merge with conflicts |
| `export` | JSON snapshot used by the web UI |

## Architecture

```
                    +-----------------------------+
                    |            cli/             |
                    | main.cpp   graph.cpp        |
                    +--------------+--------------+
                                   |
                    +--------------v--------------+
                    |            repo/            |
                    | repository  index  history  |
                    | ignore                      |
                    +------+-------------+--------+
                           |             |
            +--------------v---+   +-----v------------------+
            |     objects/     |   | diff/     merge/       |
            | oid object store |   | lcs myers lca          |
            | tree commit      |   | unified   three_way    |
            +--------+---------+   +------------------------+
                     |
            +--------v---------+
            |      hash/       |
            | sha1   compress/ |
            |        huffman   |
            +------------------+
```

- **`hash/sha1`** — incremental SHA-1: 64-byte blocks, 80-round schedule,
  FIPS 180-1 padding.
- **`compress/huffman`** — frequency table, tree build, bit packing; objects
  are compressed on disk and decompressed on read (raw legacy objects still
  load).
- **`objects/`** — `Oid` (typed 20-byte id), `Object` (`<type> <size>\0<body>`),
  sharded `ObjectStore`, sorted `Tree`, `Commit` with parent list.
- **`repo/`** — index, refs/HEAD, Merkle tree construction from the index,
  history walk, `.minignore` matching, checkout materialisation, merge
  orchestration and JSON export.
- **`diff/`** — LCS DP table, Myers greedy diff, unified hunk renderer with
  context merging and an automatic Myers fallback for huge inputs.
- **`merge/`** — BFS merge-base, line-level three-way merge with conflict
  markers.
- **`cli/`** — argument parsing, git-style output, ASCII lane graph.

### On-disk format

```
.minigit/
  HEAD                  "ref: refs/heads/main"
  MERGE_HEAD            second parent while a conflicted merge is open
  config                user.name / user.email
  index                 "<object-id> <path>" per staged file
  objects/ab/cdef...    compressed objects, sharded by first 2 hex chars
  refs/heads/main       commit id of the branch tip
```

An object is `<type> <size>\0<payload>`, and its file name is the SHA-1 of
exactly those bytes. A tree lists its children's ids, so changing one file
changes every id above it — the Merkle property that makes history
tamper-evident and deduplication automatic.

## Algorithms and complexity

| Algorithm | Where | Time | Space |
| --- | --- | --- | --- |
| SHA-1 | every object id | O(n) | O(1) |
| Tree construction | `commit` | O(files · depth log siblings) | O(paths) |
| History walk | `log` | O(V + E) with a priority queue | O(V) |
| LCS diff | `diff` (default) | O(n·m) | O(n·m) |
| Myers diff | `diff --myers` | O((N+M)·D) | O(D²) snapshots |
| Unified hunks | `diff` output | O(edits) | O(edits) |
| Merge base | `merge` | O(V + E) per BFS | O(V) |
| Three-way merge | `merge` | O(n + m) after diffs | O(n + m) |
| Huffman coding | object storage | O(n log σ) | O(σ) |

`D` is the edit distance, `V`/`E` the reachable commits and parent edges,
`σ` the number of distinct bytes.

### Diff: DP first, then Myers

`lcsDiff` fills an `(n+1) × (m+1)` table where cell `(i, j)` is the LCS length
of the first `i` old and `j` new lines, then backtracks to an edit script.
Straightforward and easy to verify, but two 4,000-line files need 16M cells.

`myersDiff` follows Myers' greedy furthest-reaching-path algorithm: it only
explores diagonals reachable with `D` edits and snapshots the `V` array per
`D` for backtracking. For typical edits `D` is tiny, so it is dramatically
cheaper. `renderUnifiedDiff` switches to Myers automatically when the LCS
table would exceed four million cells.

### Merge: graph + DP

`findMergeBase` BFS-walks all ancestors of `ours` recording distances, then
BFS-walks `theirs`; the shared commit minimising `distanceA + distanceB` is
the merge base. Both diffs (base→ours, base→theirs) are collapsed into
replacement blocks anchored on base ranges, then merged in one linear pass.
Overlapping blocks are conflicts and are written as:

```
<<<<<<< ours
our version
=======
their version
>>>>>>> theirs
```

A conflicted merge records `MERGE_HEAD`, and the next `commit` picks it up as
a second parent, so the result is a real merge commit in the DAG.

## Benchmark

`minigit_bench` (Release build, GCC 16.2, Windows 11):

```
diff, 500 lines      2.72 ms (LCS)     0.45 ms (Myers)     20 changed
diff, 2000 lines    42.45 ms (LCS)     3.80 ms (Myers)     80 changed
diff, 4000 lines   176.85 ms (LCS)    17.47 ms (Myers)    160 changed
commit, 1000 files   5.30 s (add)     24.46 ms (commit)
```

The 1000-file `add` is dominated by writing 1000 files plus 1000 object files;
`commit` itself (Merkle trees + one commit object) stays in tens of
milliseconds.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DMINIGIT_BUILD_TESTS=OFF
cmake --build build-release --target minigit_bench
./build-release/minigit_bench
```

## Tests

GoogleTest suites cover SHA-1 vectors, object encoding/sharding/corruption,
tree ordering, repository workflows, DAG walking, ASCII graphs, branches and
checkout safety, LCS/Myers reconstruction and minimality, unified hunks, merge
bases, three-way merge conflicts, Huffman round trips, `.minignore` rules and
JSON export.

```bash
ctest --test-dir build --output-on-failure
```

## Web UI

`web/` is a dependency-free static site that visualises a real exported
snapshot: the commit DAG, branch heads, a live LCS-vs-Myers diff playground,
the object format and the module map.

```bash
cd web/public
python -m http.server 4173      # http://localhost:4173
```

Point it at your own repository with `minigit export > web/public/data.json`.
Deployment files (`worker.js`, `deploy.json`) serve `public/` on Cloudflare
Workers; see `web/README.md`.

## Repository layout

```
src/            library sources (minigit_core) + CLI
tests/          GoogleTest suites
bench/          diff and commit benchmarks
web/            static UI + Worker entrypoint
docs/           architecture and algorithm notes
```

More detail: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) and
[`docs/ALGORITHMS.md`](docs/ALGORITHMS.md).

## Known limitations

- Text merges only; binary conflicts keep "ours" and are reported.
- No rename detection, no packfiles, no network protocol.
- `.minignore` supports a documented subset of gitignore syntax (see
  `src/repo/ignore.h`).
- `.minigit/refs` is read directly; there is no reflog.
