#include "merge/lca.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

#include "core/error.h"
#include "repo/history.h"

namespace minigit {
namespace {

using DepthMap = std::unordered_map<Oid, int, OidHash>;

}  // namespace

bool isAncestor(const ObjectStore& store, const Oid& ancestor,
                const Oid& descendant) {
  if (ancestor == descendant) {
    return true;
  }

  std::queue<Oid> pending;
  std::unordered_map<Oid, bool, OidHash> visited;
  pending.push(descendant);
  visited[descendant] = true;

  while (!pending.empty()) {
    const Oid current = pending.front();
    pending.pop();

    const auto commit = readCommit(store, current);
    if (!commit.has_value()) {
      throw MinigitError("commit " + current.hex() + " not found");
    }

    for (const Oid& parent : commit->parents) {
      if (parent == ancestor) {
        return true;
      }
      if (visited.emplace(parent, true).second) {
        pending.push(parent);
      }
    }
  }

  return false;
}

std::optional<Oid> findMergeBase(const ObjectStore& store, const Oid& a,
                                 const Oid& b) {
  if (a == b) {
    return a;
  }

  DepthMap depthA;
  std::queue<Oid> pendingA;
  depthA[a] = 0;
  pendingA.push(a);

  while (!pendingA.empty()) {
    const Oid current = pendingA.front();
    pendingA.pop();
    const int depth = depthA[current];

    const auto commit = readCommit(store, current);
    if (!commit.has_value()) {
      throw MinigitError("commit " + current.hex() + " not found");
    }
    for (const Oid& parent : commit->parents) {
      if (depthA.emplace(parent, depth + 1).second) {
        pendingA.push(parent);
      }
    }
  }

  std::optional<Oid> best;
  int bestCost = std::numeric_limits<int>::max();
  std::int64_t bestTimestamp = std::numeric_limits<std::int64_t>::min();

  DepthMap depthB;
  std::queue<Oid> pendingB;
  depthB[b] = 0;
  pendingB.push(b);

  while (!pendingB.empty()) {
    const Oid current = pendingB.front();
    pendingB.pop();
    const int depth = depthB[current];

    // Every queued commit is at least `depth` steps from b, so once that
    // already costs more than the best candidate we can stop.
    if (depth > bestCost) {
      break;
    }

    const auto ancestor = depthA.find(current);
    if (ancestor != depthA.end()) {
      const int cost = ancestor->second + depth;
      auto commit = readCommit(store, current);
      const std::int64_t timestamp = commit.has_value() ? commit->timestamp : 0;
      if (cost < bestCost ||
          (cost == bestCost && timestamp > bestTimestamp)) {
        best = current;
        bestCost = cost;
        bestTimestamp = timestamp;
      }
    }

    const auto commit = readCommit(store, current);
    if (!commit.has_value()) {
      throw MinigitError("commit " + current.hex() + " not found");
    }
    for (const Oid& parent : commit->parents) {
      if (depthB.emplace(parent, depth + 1).second) {
        pendingB.push(parent);
      }
    }
  }

  return best;
}

}  // namespace minigit
