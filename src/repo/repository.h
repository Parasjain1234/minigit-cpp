#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "objects/object_store.h"
#include "objects/oid.h"
#include "repo/index.h"

namespace minigit {

enum class ChangeKind { Added, Modified, Deleted };

const char* changeKindLabel(ChangeKind kind);

struct StagedChange {
  std::string path;
  ChangeKind kind;
};

/// Everything `minigit status` needs, without any formatting decisions.
struct StatusReport {
  std::string branch;
  bool detached = false;
  bool hasCommits = false;
  std::vector<StagedChange> staged;    // index vs HEAD
  std::vector<StagedChange> unstaged;  // working tree vs index
  std::vector<std::string> untracked;
};

/// Owns the layout of a MiniGit repository:
///
///     .minigit/
///       HEAD                  "ref: refs/heads/main"
///       config                user.name / user.email
///       index                 staged paths -> object ids
///       objects/              content-addressed object store
///       refs/heads/main       branch tip
class Repository {
 public:
  /// Creates the repository layout (idempotent).
  static Repository init(const std::filesystem::path& directory,
                         const std::string& defaultBranch = "main");

  /// Walks up from `start` looking for a .minigit directory.
  static std::optional<std::filesystem::path> findRoot(
      const std::filesystem::path& start);

  /// Like findRoot but throws a clear error when there is no repository.
  static Repository open(
      const std::filesystem::path& start = std::filesystem::current_path());

  const std::filesystem::path& workTree() const { return workTree_; }
  const std::filesystem::path& gitDir() const { return gitDir_; }
  ObjectStore& objects() { return objects_; }
  const ObjectStore& objects() const { return objects_; }

  /// Stages each path (file or directory, recursively) and returns the
  /// repository-relative paths that were staged, sorted.
  std::vector<std::string> add(const std::vector<std::filesystem::path>& paths);

  /// Writes the trees for the index, then a commit object, then moves the
  /// current branch to it.  Returns the new commit id.
  Oid commit(const std::string& message);

  StatusReport status() const;

  std::optional<Oid> headCommit() const;

  /// Current branch name, or an empty string when HEAD is detached.
  std::string headBranch() const;
  bool isDetached() const;

  /// Flattened map (path -> blob id) of the HEAD commit's tree.
  std::map<std::string, Oid> headTree() const;

  /// Reads the tree id out of a commit object.
  Oid treeOf(const Oid& commitId) const;

  std::string userName() const;
  std::string userEmail() const;

  std::filesystem::path indexPath() const { return gitDir_ / "index"; }
  std::filesystem::path headPath() const { return gitDir_ / "HEAD"; }

 private:
  Repository(std::filesystem::path workTree, std::filesystem::path gitDir);

  std::string relativeToWorkTree(const std::filesystem::path& absolute) const;
  Oid writeTreeFromIndex(const Index& index);
  void updateHeadRef(const Oid& commitId);
  void stageFile(const std::filesystem::path& absolute, Index& index,
                 std::vector<std::string>& staged);

  std::filesystem::path workTree_;
  std::filesystem::path gitDir_;
  ObjectStore objects_;
};

}  // namespace minigit
