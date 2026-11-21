# Algorithms

Five data-structure problems appear in a version control system. MiniGit
implements each one directly, with tests that check the property that makes it
correct rather than a golden string.

## 1. SHA-1 (`hash/sha1.cpp`)

Message padded with `0x80`, zeros, and the 64-bit big-endian bit length, then
processed in 512-bit blocks. Each block expands 16 words into 80 using
`w[i] = rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1)`, then runs 80 rounds with
four different Boolean functions and constants.

- Time: O(n), one pass.
- Space: O(1) — a 64-byte buffer and five state words.
- Tested against FIPS 180-1 vectors, including the 1,000,000-character `'a'`
  case and a byte-at-a-time feed equivalent to a one-shot hash.

## 2. Merkle tree (`repo/repository.cpp`)

The index is a flat map `path → blob id`. Splitting each path on `/` builds an
in-memory tree; writing is post-order, so a directory is serialized only after
its children have ids.

- Time: O(P) entries plus sorting, O(P log P) worst case per directory.
- Space: O(P).
- Key property: identical directory content produces byte-identical trees,
  so unchanged subtrees are shared across commits — free structural
  deduplication.

## 3. History as a DAG (`repo/history.cpp`)

Commits reference their parents, so `log` is a best-first graph traversal.
A max-heap keyed by `(timestamp, oid)` emits the newest commit; parents are
pushed only if unseen.

- Time: O((V + E) log V) with the heap, O(V + E) graph work.
- Space: O(V) for the visited set and queue.
- Merges are handled naturally: the shared ancestor is visited once.

`log --graph` (`cli/graph.cpp`) keeps a lane vector; the first parent continues
in the commit's lane, extra parents open lanes, and a lane that reaches an
already-drawn commit is closed with a slash connector.

## 4. Diff: LCS dynamic programming (`diff/lcs.cpp`)

```
for i in 1..n:
  for j in 1..m:
    table[i][j] = a[i-1] == b[j-1]
                ? table[i-1][j-1] + 1
                : max(table[i-1][j], table[i][j-1])
```

Backtracking from `(n, m)` yields equal/delete/insert steps. Deletions are
emitted before insertions inside a changed run for a familiar diff shape.

- Time: O(n·m), one table fill plus one walk.
- Space: O(n·m) 32-bit cells — the reason Myers exists.
- Property tested: applying the edit script to the old lines reproduces the
  new lines exactly, and the number of edits equals
  `n + m − 2·LCS(n, m)`.

## 5. Diff: Myers (`diff/myers.cpp`)

The greedy furthest-reaching-path algorithm. For each edit distance `d`, it
tracks the furthest `x` reached on every diagonal `k`: either a deletion from
`k+1` or an insertion from `k-1`, followed by a snake over equal lines. It
snapshots `V` per `d`; replaying the snapshots backwards recovers the script.

- Time: O((N + M)·D), where `D` is the minimal edit distance.
- Space: O(D²) for snapshots (O(D) per level over `D` levels).
- Property tested: minimal edit count on small inputs (matches LCS), correct
  reconstruction on repeated lines and scrambled 60-line inputs.

`renderUnifiedDiff` groups edits into hunks with three context lines and
merges hunks closer than twice the context, then falls back to Myers when the
LCS table would exceed 4M cells.

## 6. Merge base (`merge/lca.cpp`)

Two BFS passes over the commit DAG. Pass one records every ancestor of `ours`
with its distance. Pass two walks `theirs` level by level and considers each
commit present in both maps, choosing the one minimising
`distanceA + distanceB` (ties broken by newer timestamp).

- Time: O(V + E) per pass.
- Space: O(V) distance maps.
- Correctness intuition: any common ancestor is reached by both passes; the
  minimum summed distance is the deepest shared commit. Level-order traversal
  also lets pass two stop early once its depth exceeds the best cost.

## 7. Three-way merge (`merge/three_way.cpp`)

Both diffs (base→ours, base→theirs) are collapsed into replacement blocks
anchored on base line ranges. The two sorted block lists are walked once:

- disjoint blocks are applied in order,
- identical overlapping blocks are applied once,
- overlapping blocks with different content become a conflict block with
  `<<<<<<<`, `=======`, `>>>>>>>` markers.

- Time: O(n + m + k), after the two Myers diffs.
- Space: O(n + m).
- Properties tested: independent edits merge cleanly, the same edit from both
  sides is not a conflict, deletion-vs-modification conflicts, insertions at
  the same point conflict, insertions at different points do not.

## 8. Huffman coding (`compress/huffman.cpp`)

Frequencies → min-heap of nodes (ties broken by smallest symbol so the tree is
reproducible) → prefix codes → bitstream. The file stores the frequency table
instead of the tree; the decoder rebuilds it identically.

- Time: O(n log σ) to encode, O(n) to decode.
- Space: O(σ) for the table, plus the output.
- Edge cases: empty input, single-symbol alphabet (stored with zero bits and
  repeated from the header), truncated streams (rejected with a clear error).

## 9. `.minignore` glob matching (`repo/ignore.cpp`)

Patterns are translated to ECMAScript regexes: `*` → `[^/]*`, `**/` → `(.*/)?`
(zero or more directories), `?` → `[^/]`. Rules are evaluated in order and the
last match wins, which is what makes a trailing `!pattern` re-include a path.
