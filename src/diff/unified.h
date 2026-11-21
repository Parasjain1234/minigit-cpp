#pragma once

#include <string>

#include "diff/diff_types.h"

namespace minigit {

/// Renders a unified diff (`---`, `+++`, `@@` hunks) for two texts.
///
/// An edit script is produced either by the LCS table or by Myers, then
/// grouped into hunks with `contextLines` of unchanged context.  Hunks that
/// are closer than twice the context window are merged, exactly like
/// `diff -u`.
///
/// Returns an empty string when the inputs are identical.
std::string renderUnifiedDiff(const std::string& oldLabel,
                              const std::string& newLabel,
                              const std::string& oldText,
                              const std::string& newText,
                              DiffAlgorithm algorithm = DiffAlgorithm::Lcs,
                              int contextLines = 3);

}  // namespace minigit
