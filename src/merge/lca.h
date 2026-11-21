#pragma once

#include <optional>

#include "objects/object_store.h"
#include "objects/oid.h"

namespace minigit {

/// True when `ancestor` is reachable from `descendant` by walking parents.
///
/// Breadth-first search over the commit DAG; the first level that contains
/// the target wins, so the answer is found as early as possible.
bool isAncestor(const ObjectStore& store, const Oid& ancestor,
                const Oid& descendant);

/// Lowest common ancestor of two commits, computed with two BFS passes.
///
/// The first pass records every ancestor of `a` with its distance.  The
/// second walks `b` level by level; whenever it meets a commit from the first
/// map it is a candidate.  The candidate minimising
/// `distanceA + distanceB` is the "lowest" common ancestor, i.e. the most
/// recent commit both branches share.
///
/// Returns std::nullopt for unrelated histories.
std::optional<Oid> findMergeBase(const ObjectStore& store, const Oid& a,
                                 const Oid& b);

}  // namespace minigit
