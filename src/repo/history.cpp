#include "repo/history.h"

#include <queue>
#include <unordered_set>
#include <utility>

#include "core/error.h"
#include "objects/object.h"

namespace minigit {

std::optional<Commit> readCommit(const ObjectStore& store, const Oid& commitId) {
  const auto object = store.read(commitId);
  if (!object.has_value()) {
    return std::nullopt;
  }
  if (object->type() != ObjectType::Commit) {
    throw MinigitError("object " + commitId.hex() + " is not a commit");
  }
  return Commit::parse(object->data());
}

std::vector<CommitEntry> walkHistory(const ObjectStore& store,
                                     const Oid& start) {
  const auto startCommit = readCommit(store, start);
  if (!startCommit.has_value()) {
    throw MinigitError("commit " + start.hex() + " not found");
  }

  // (timestamp, oid) pairs: the largest timestamp is emitted first, ties are
  // broken by oid so the output is stable between runs.
  std::priority_queue<std::pair<std::int64_t, Oid>> pending;
  std::unordered_set<Oid, OidHash> discovered;

  pending.push({startCommit->timestamp, start});
  discovered.insert(start);

  std::vector<CommitEntry> entries;
  while (!pending.empty()) {
    const Oid oid = pending.top().second;
    pending.pop();

    auto commit = readCommit(store, oid);
    if (!commit.has_value()) {
      throw MinigitError("commit " + oid.hex() + " not found");
    }

    for (const Oid& parent : commit->parents) {
      if (discovered.insert(parent).second) {
        auto parentCommit = readCommit(store, parent);
        if (!parentCommit.has_value()) {
          throw MinigitError("commit " + oid.hex() +
                             " references missing parent " + parent.hex());
        }
        pending.push({parentCommit->timestamp, parent});
      }
    }

    entries.push_back(CommitEntry{oid, std::move(*commit)});
  }

  return entries;
}

}  // namespace minigit
