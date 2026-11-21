#include "diff/myers.h"

#include <algorithm>
#include <cstddef>

namespace minigit {

std::vector<Edit> myersDiff(const std::vector<std::string>& oldLines,
                            const std::vector<std::string>& newLines) {
  const int n = static_cast<int>(oldLines.size());
  const int m = static_cast<int>(newLines.size());
  const int maxDistance = n + m;
  // The greedy loop looks one diagonal to the left and right of the current
  // k, so the table needs a guard cell on each side (and at least one cell
  // for the empty/empty case).
  const int offset = maxDistance + 1;

  std::vector<int> v(static_cast<std::size_t>(2 * maxDistance + 3), 0);
  std::vector<std::vector<int>> trace;

  int finalDistance = 0;
  bool finished = false;

  for (int d = 0; d <= maxDistance && !finished; ++d) {
    trace.push_back(v);  // state before round d, replayed during backtracking

    for (int k = -d; k <= d; k += 2) {
      const int index = offset + k;
      int x;
      if (k == -d || (k != d && v[static_cast<std::size_t>(index - 1)] <
                                     v[static_cast<std::size_t>(index + 1)])) {
        x = v[static_cast<std::size_t>(index + 1)];
      } else {
        x = v[static_cast<std::size_t>(index - 1)] + 1;
      }
      int y = x - k;

      while (x < n && y < m && oldLines[x] == newLines[y]) {
        ++x;
        ++y;
      }

      v[static_cast<std::size_t>(index)] = x;

      if (x >= n && y >= m) {
        finalDistance = d;
        finished = true;
        break;
      }
    }
  }

  std::vector<Edit> reversed;
  int x = n;
  int y = m;

  for (int d = finalDistance; d > 0; --d) {
    const std::vector<int>& previous = trace[static_cast<std::size_t>(d)];
    const int k = x - y;

    int previousK;
    if (k == -d || (k != d && previous[static_cast<std::size_t>(offset + k - 1)] <
                                   previous[static_cast<std::size_t>(offset + k + 1)])) {
      previousK = k + 1;
    } else {
      previousK = k - 1;
    }

    const int previousX = previous[static_cast<std::size_t>(offset + previousK)];
    const int previousY = previousX - previousK;

    while (x > previousX && y > previousY) {
      reversed.push_back({EditKind::Equal, oldLines[x - 1]});
      --x;
      --y;
    }

    if (d > 0) {
      if (x == previousX) {
        reversed.push_back({EditKind::Insert, newLines[y - 1]});
        --y;
      } else {
        reversed.push_back({EditKind::Delete, oldLines[x - 1]});
        --x;
      }
    }
  }

  // Whatever remains is the common prefix that both sequences share.
  while (x > 0 && y > 0) {
    reversed.push_back({EditKind::Equal, oldLines[x - 1]});
    --x;
    --y;
  }

  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

}  // namespace minigit
