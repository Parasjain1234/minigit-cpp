#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "diff/diff_types.h"
#include "diff/lcs.h"
#include "diff/myers.h"
#include "objects/object.h"
#include "repo/repository.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using minigit::DiffAlgorithm;
using minigit::Edit;
using minigit::EditKind;
using minigit::lcsDiff;
using minigit::myersDiff;
using minigit::Repository;

namespace {

double milliseconds(Clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}

std::size_t changedLines(const std::vector<Edit>& edits) {
  std::size_t count = 0;
  for (const Edit& edit : edits) {
    if (edit.kind != EditKind::Equal) {
      ++count;
    }
  }
  return count;
}

void benchDiff(int lineCount) {
  std::vector<std::string> before;
  std::vector<std::string> after;
  before.reserve(static_cast<std::size_t>(lineCount));
  after.reserve(static_cast<std::size_t>(lineCount));

  for (int i = 0; i < lineCount; ++i) {
    const std::string line =
        "line " + std::to_string(i) + ": the quick brown fox jumps";
    before.push_back(line);
    after.push_back(i % 50 == 0 ? line + " (edited)" : line);
  }

  const auto lcsStart = Clock::now();
  const auto lcsEdits = lcsDiff(before, after);
  const auto lcsEnd = Clock::now();

  const auto myersStart = Clock::now();
  const auto myersEdits = myersDiff(before, after);
  const auto myersEnd = Clock::now();

  std::cout << std::left << std::setw(28) << "diff, " + std::to_string(lineCount) + " lines"
            << std::right << std::setw(12) << std::fixed << std::setprecision(2)
            << milliseconds(lcsEnd - lcsStart) << " ms (LCS)   "
            << std::setw(9) << milliseconds(myersEnd - myersStart)
            << " ms (Myers)   " << changedLines(lcsEdits) << " changed\n";

  if (changedLines(lcsEdits) != changedLines(myersEdits)) {
    std::cerr << "warning: LCS and Myers disagree on the edit count\n";
  }
}

void benchCommit(int fileCount) {
  const fs::path root =
      fs::temp_directory_path() /
      ("minigit_bench_" +
       std::to_string(Clock::now().time_since_epoch().count()));

  std::error_code ec;
  fs::create_directories(root, ec);

  Repository repo = Repository::init(root);
  for (int i = 0; i < fileCount; ++i) {
    const fs::path file =
        root / ("file_" + std::to_string(i) + ".txt");
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << "file " << i << '\n' << "some content for the benchmark\n";
  }

  const auto addStart = Clock::now();
  repo.add({root});
  const auto addEnd = Clock::now();

  const auto commitStart = Clock::now();
  repo.commit("benchmark: " + std::to_string(fileCount) + " files");
  const auto commitEnd = Clock::now();

  std::cout << std::left << std::setw(28)
            << "commit, " + std::to_string(fileCount) + " files"
            << std::right << std::setw(12) << std::fixed << std::setprecision(2)
            << milliseconds(addEnd - addStart) << " ms (add)    "
            << std::setw(9) << milliseconds(commitEnd - commitStart)
            << " ms (commit)\n";

  fs::remove_all(root, ec);
}

}  // namespace

int main() {
  std::cout << "MiniGit benchmark\n"
            << "-----------------\n";
  benchDiff(500);
  benchDiff(2000);
  benchDiff(4000);
  benchCommit(1000);
  return 0;
}
