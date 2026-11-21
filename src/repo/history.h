#pragma once

#include <optional>
#include <vector>

#include "objects/commit.h"
#include "objects/object_store.h"
#include "objects/oid.h"

namespace minigit {

struct CommitEntry {
  Oid oid;
  Commit commit;
};

/// Reads a commit object.  Returns std::nullopt when the object is missing.
std::optional<Commit> readCommit(const ObjectStore& store, const Oid& commitId);

/// Walks the commit DAG from `start`, newest first.
///
/// Ordering is by commit timestamp descending, with the object id as a
/// deterministic tie-breaker.  Because every commit is pushed only after it
/// is popped, this is a best-first traversal of the DAG (a priority queue:
/// the graph equivalent of a merge of the parent chains).  Each reachable
/// commit is emitted exactly once, so merges never duplicate history.
std::vector<CommitEntry> walkHistory(const ObjectStore& store,
                                     const Oid& start);

}  // namespace minigit
