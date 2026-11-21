#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "diff/diff_types.h"
#include "objects/commit.h"
#include "objects/object.h"
#include "objects/object_store.h"

namespace minigit_test {

/// Writes a commit object directly; used by DAG/merge tests that need shapes
/// the command line cannot produce yet.
inline minigit::Oid writeTestCommit(minigit::ObjectStore& store,
                                    const minigit::Oid& tree,
                                    const std::vector<minigit::Oid>& parents,
                                    std::int64_t timestamp,
                                    const std::string& message) {
  minigit::Commit commit;
  commit.tree = tree;
  commit.parents = parents;
  commit.author = "Test <test@example.com>";
  commit.timestamp = timestamp;
  commit.message = message;
  return store.write(minigit::ObjectType::Commit, commit.serialize());
}

/// Rebuilds the "new" side of a diff from its edit script; used by every
/// algorithm test as a correctness oracle.
inline std::vector<std::string> applyEdits(
    const std::vector<minigit::Edit>& edits) {
  std::vector<std::string> result;
  result.reserve(edits.size());
  for (const minigit::Edit& edit : edits) {
    if (edit.kind != minigit::EditKind::Delete) {
      result.push_back(edit.line);
    }
  }
  return result;
}

/// Creates a unique temporary directory and removes it on destruction.
class TempDir {
 public:
  explicit TempDir(const std::string& tag = "minigit") {
    static std::atomic<unsigned> counter{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (tag + "_" + std::to_string(now) + "_" +
             std::to_string(counter.fetch_add(1)));
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
    std::filesystem::create_directories(path_, ec);
  }

  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

inline void writeTextFile(const std::filesystem::path& path,
                          const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << contents;
}

inline std::string readTextFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

}  // namespace minigit_test
