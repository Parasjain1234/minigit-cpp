#pragma once

#include <string>
#include <vector>

#include "diff/diff_types.h"

namespace minigit {

/// Myers' O((N+M)D) diff algorithm.
///
/// Where the LCS table pays O(N*M) memory regardless of how similar the two
/// inputs are, Myers only explores the "diagonal band" reachable with D edits.
/// For a one-line change in a 10,000 line file that is D ~ 2 instead of a
/// 100,000,000 cell table.
///
/// Implementation: the greedy furthest-reaching-path loop keeps the furthest
/// x reachable on every diagonal k for the current edit distance d, recording
/// one snapshotted V array per d.  The snapshots are then replayed backwards
/// to recover the edit script.
std::vector<Edit> myersDiff(const std::vector<std::string>& oldLines,
                            const std::vector<std::string>& newLines);

}  // namespace minigit
