#include "diff/lcs.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace minigit {

std::size_t lcsLength(const std::vector<std::string>& oldLines,
                      const std::vector<std::string>& newLines) {
  const std::size_t rows = oldLines.size();
  const std::size_t columns = newLines.size();
  std::vector<std::uint32_t> table((rows + 1) * (columns + 1), 0);

  const auto at = [columns](std::size_t i, std::size_t j) {
    return i * (columns + 1) + j;
  };

  for (std::size_t i = 1; i <= rows; ++i) {
    for (std::size_t j = 1; j <= columns; ++j) {
      if (oldLines[i - 1] == newLines[j - 1]) {
        table[at(i, j)] = table[at(i - 1, j - 1)] + 1;
      } else {
        table[at(i, j)] = std::max(table[at(i - 1, j)], table[at(i, j - 1)]);
      }
    }
  }

  return table[at(rows, columns)];
}

std::vector<Edit> lcsDiff(const std::vector<std::string>& oldLines,
                          const std::vector<std::string>& newLines) {
  const std::size_t rows = oldLines.size();
  const std::size_t columns = newLines.size();

  std::vector<std::uint32_t> table((rows + 1) * (columns + 1), 0);
  const auto at = [columns](std::size_t i, std::size_t j) {
    return i * (columns + 1) + j;
  };

  for (std::size_t i = 1; i <= rows; ++i) {
    for (std::size_t j = 1; j <= columns; ++j) {
      if (oldLines[i - 1] == newLines[j - 1]) {
        table[at(i, j)] = table[at(i - 1, j - 1)] + 1;
      } else {
        table[at(i, j)] = std::max(table[at(i - 1, j)], table[at(i, j - 1)]);
      }
    }
  }

  std::vector<Edit> reversed;
  std::size_t i = rows;
  std::size_t j = columns;

  while (i > 0 && j > 0) {
    if (oldLines[i - 1] == newLines[j - 1]) {
      reversed.push_back({EditKind::Equal, oldLines[i - 1]});
      --i;
      --j;
    } else if (table[at(i - 1, j)] >= table[at(i, j - 1)]) {
      reversed.push_back({EditKind::Delete, oldLines[i - 1]});
      --i;
    } else {
      reversed.push_back({EditKind::Insert, newLines[j - 1]});
      --j;
    }
  }
  while (i > 0) {
    reversed.push_back({EditKind::Delete, oldLines[--i]});
  }
  while (j > 0) {
    reversed.push_back({EditKind::Insert, newLines[--j]});
  }

  std::reverse(reversed.begin(), reversed.end());

  // Within a run of changes, emit deletions before insertions.  Backtracking
  // naturally produces them in either order; normalising keeps the output
  // stable and matches the familiar "remove, then add" diff shape.
  std::size_t runStart = 0;
  while (runStart < reversed.size()) {
    if (reversed[runStart].kind == EditKind::Equal) {
      ++runStart;
      continue;
    }
    std::size_t runEnd = runStart;
    while (runEnd < reversed.size() &&
           reversed[runEnd].kind != EditKind::Equal) {
      ++runEnd;
    }
    std::stable_partition(reversed.begin() + runStart,
                          reversed.begin() + runEnd, [](const Edit& edit) {
                            return edit.kind == EditKind::Delete;
                          });
    runStart = runEnd;
  }

  return reversed;
}

}  // namespace minigit
