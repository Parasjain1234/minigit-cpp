#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "diff/diff_types.h"

namespace minigit {

/// Length of the longest common subsequence of two line sequences.
/// Dynamic programming, O(n*m) time and space.
std::size_t lcsLength(const std::vector<std::string>& oldLines,
                      const std::vector<std::string>& newLines);

/// Line diff built on a full LCS DP table.
///
/// The table at (i, j) holds the LCS length of the first i old and first j new
/// lines.  Backtracking from (n, m) walks the table once: equal lines are
/// kept, otherwise the branch with the longer common subsequence is taken,
/// emitting a deletion or an insertion.
///
/// Complexity: O(n*m) time, O(n*m) memory.  For very large inputs prefer
/// myersDiff(), which is what renderUnifiedDiff() switches to automatically.
std::vector<Edit> lcsDiff(const std::vector<std::string>& oldLines,
                          const std::vector<std::string>& newLines);

}  // namespace minigit
