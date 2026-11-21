#pragma once

#include <string>
#include <vector>

namespace minigit {

struct MergeFileResult {
  std::string content;
  bool conflicted = false;
};

/// Line-based three-way merge: base is the common ancestor, ours and theirs
/// are the two sides.
///
/// The algorithm turns both diffs (base->ours and base->theirs) into
/// `Change` records anchored on base line ranges, then walks the two sorted
/// change lists once, copying unchanged base lines and applying changes as
/// it goes.  Two changes that touch the same base range are a conflict: the
/// result contains
///
///     <<<<<<< ours
///     ...
///     =======
///     ...
///     >>>>>>> theirs
///
/// Identical edits from both sides are applied once without a conflict, which
/// is what makes independent work merge cleanly.
MergeFileResult mergeText3Way(const std::string& base, const std::string& ours,
                              const std::string& theirs);

}  // namespace minigit
