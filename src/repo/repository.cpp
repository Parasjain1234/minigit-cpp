#include "repo/repository.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <utility>
#include <system_error>

#include "core/error.h"
#include "diff/unified.h"
#include "objects/commit.h"
#include "objects/object.h"
#include "objects/tree.h"
#include "repo/history.h"

namespace minigit {
namespace fs = std::filesystem;
namespace {

constexpr const char* kObjectsDir = "objects";
constexpr const char* kRefsDir = "refs/heads";
constexpr const char* kGitDirName = ".minigit";

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw MinigitError("cannot open file: " + path.string());
  }
  std::string data((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  if (in.bad()) {
    throw MinigitError("failed while reading file: " + path.string());
  }
  return data;
}

void writeFile(const fs::path& path, const std::string& contents) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw MinigitError("cannot write file: " + path.string());
  }
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  if (!out) {
    throw MinigitError("failed while writing file: " + path.string());
  }
}

std::string trim(const std::string& text) {
  const std::size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const std::size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

/// In-memory directory tree used while building tree objects from the index.
struct TreeNode {
  std::map<std::string, Oid> blobs;
  std::map<std::string, std::unique_ptr<TreeNode>> directories;
};

void insertFile(TreeNode& root, const std::string& path, const Oid& oid) {
  TreeNode* node = &root;
  std::size_t position = 0;
  while (true) {
    const std::size_t slash = path.find('/', position);
    if (slash == std::string::npos) {
      node->blobs[path.substr(position)] = oid;
      return;
    }
    const std::string directory = path.substr(position, slash - position);
    auto& child = node->directories[directory];
    if (!child) {
      child = std::make_unique<TreeNode>();
    }
    node = child.get();
    position = slash + 1;
  }
}

/// Post-order write: children are stored before the parent tree that points
/// at them, because a parent needs its children's ids.  This is the Merkle
/// tree construction step.
Oid writeTreeNode(ObjectStore& store, const TreeNode& node) {
  std::vector<TreeEntry> entries;
  entries.reserve(node.blobs.size() + node.directories.size());

  for (const auto& [name, oid] : node.blobs) {
    entries.push_back(TreeEntry{kFileMode, ObjectType::Blob, oid, name});
  }
  for (const auto& [name, child] : node.directories) {
    entries.push_back(TreeEntry{kTreeMode, ObjectType::Tree,
                                writeTreeNode(store, *child), name});
  }

  Tree tree(std::move(entries));
  return store.write(ObjectType::Tree, tree.serialize());
}

void flattenTree(const ObjectStore& store, const Oid& treeId,
                 const std::string& prefix, std::map<std::string, Oid>& out) {
  const Object object = store.readOrThrow(treeId);
  if (object.type() != ObjectType::Tree) {
    throw MinigitError("object " + treeId.hex() + " is not a tree");
  }

  const Tree tree = Tree::parse(object.data());
  for (const TreeEntry& entry : tree.entries()) {
    const std::string path =
        prefix.empty() ? entry.name : prefix + "/" + entry.name;
    if (entry.type == ObjectType::Tree) {
      flattenTree(store, entry.oid, path, out);
    } else {
      out[path] = entry.oid;
    }
  }
}

std::map<std::string, std::string> parseConfig(const fs::path& path) {
  std::map<std::string, std::string> values;
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) {
    return values;
  }

  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const std::string key = trim(line.substr(0, equals));
    const std::string value = trim(line.substr(equals + 1));
    if (!key.empty()) {
      values[key] = value;
    }
  }
  return values;
}

Oid hashWorkingFile(const fs::path& path) {
  return Object(ObjectType::Blob, readFile(path)).oid();
}

bool shouldSkipEntry(const fs::path& path) {
  return path.filename() == kGitDirName;
}

void validateBranchName(const std::string& name) {
  if (name.empty()) {
    throw MinigitError("branch name cannot be empty");
  }
  if (name.front() == '-' || name.front() == '/') {
    throw MinigitError("'" + name + "' is not a valid branch name");
  }
  if (name.back() == '/' || name.back() == '.') {
    throw MinigitError("'" + name + "' is not a valid branch name");
  }
  const std::string forbidden = " ~^:?*[\\\t";
  if (name.find_first_of(forbidden) != std::string::npos) {
    throw MinigitError("'" + name +
                       "' is not a valid branch name (no spaces or "
                       "~^:?*[\\ allowed)");
  }
  if (name.find("..") != std::string::npos ||
      name.find("@{") != std::string::npos ||
      name.find("//") != std::string::npos) {
    throw MinigitError("'" + name + "' is not a valid branch name");
  }
  if (name.size() >= 5 && name.compare(name.size() - 5, 5, ".lock") == 0) {
    throw MinigitError("'" + name + "' is not a valid branch name");
  }
}

std::optional<Oid> resolveIdPrefix(const ObjectStore& store,
                                   const std::string& prefix) {
  if (prefix.size() < 4 || prefix.size() >= Oid::kHexSize) {
    return std::nullopt;
  }

  const fs::path directory = store.root() / prefix.substr(0, 2);
  std::error_code ec;
  if (!fs::is_directory(directory, ec)) {
    return std::nullopt;
  }

  std::vector<Oid> matches;
  for (const auto& entry : fs::directory_iterator(directory, ec)) {
    const std::string candidate =
        prefix.substr(0, 2) + entry.path().filename().string();
    if (candidate.rfind(prefix, 0) != 0) {
      continue;
    }
    if (auto oid = Oid::tryParse(candidate)) {
      matches.push_back(*oid);
    }
  }

  if (matches.size() > 1) {
    throw MinigitError("short object id '" + prefix + "' is ambiguous");
  }
  if (matches.size() == 1) {
    return matches.front();
  }
  return std::nullopt;
}

}  // namespace

const char* changeKindLabel(ChangeKind kind) {
  switch (kind) {
    case ChangeKind::Added:
      return "new file:   ";
    case ChangeKind::Modified:
      return "modified:   ";
    case ChangeKind::Deleted:
      return "deleted:    ";
  }
  return "changed:    ";
}

Repository::Repository(fs::path workTree, fs::path gitDir)
    : workTree_(std::move(workTree)),
      gitDir_(std::move(gitDir)),
      objects_(gitDir_ / kObjectsDir) {}

Repository Repository::init(const fs::path& directory,
                            const std::string& defaultBranch) {
  std::error_code ec;
  fs::path workTree = fs::absolute(directory, ec);
  if (ec) {
    throw MinigitError("cannot resolve path " + directory.string() + ": " +
                       ec.message());
  }
  workTree = workTree.lexically_normal();

  const fs::path gitDir = workTree / kGitDirName;
  fs::create_directories(gitDir / kObjectsDir, ec);
  fs::create_directories(gitDir / kRefsDir, ec);
  if (ec) {
    throw MinigitError("cannot create repository at " + gitDir.string() + ": " +
                       ec.message());
  }

  if (!fs::exists(gitDir / "HEAD")) {
    writeFile(gitDir / "HEAD", "ref: refs/heads/" + defaultBranch + "\n");
  }
  if (!fs::exists(gitDir / "config")) {
    writeFile(gitDir / "config",
              "[core]\n"
              "\tdefaultBranch = " +
                  defaultBranch +
                  "\n"
                  "[user]\n"
                  "\tuser.name = MiniGit User\n"
                  "\tuser.email = minigit@example.com\n");
  }
  if (!fs::exists(gitDir / "index")) {
    writeFile(gitDir / "index", "");
  }

  return Repository(workTree, gitDir);
}

std::optional<fs::path> Repository::findRoot(const fs::path& start) {
  std::error_code ec;
  fs::path current = fs::absolute(start, ec);
  if (ec) {
    return std::nullopt;
  }
  current = current.lexically_normal();

  while (true) {
    if (fs::is_directory(current / kGitDirName, ec)) {
      return current;
    }
    const fs::path parent = current.parent_path();
    if (parent.empty() || parent == current) {
      return std::nullopt;
    }
    current = parent;
  }
}

Repository Repository::open(const fs::path& start) {
  const auto root = findRoot(start);
  if (!root.has_value()) {
    throw MinigitError(
        "not a minigit repository (or any of the parent directories): run "
        "'minigit init' first");
  }
  return Repository(*root, *root / kGitDirName);
}

std::optional<Oid> Repository::headCommit() const {
  std::error_code ec;
  if (!fs::is_regular_file(headPath(), ec)) {
    return std::nullopt;
  }

  const std::string head = trim(readFile(headPath()));
  if (head.rfind("ref: ", 0) == 0) {
    const fs::path refPath = gitDir_ / head.substr(5);
    if (!fs::is_regular_file(refPath, ec)) {
      return std::nullopt;  // unborn branch
    }
    return Oid::fromHex(trim(readFile(refPath)));
  }

  return Oid::fromHex(head);
}

std::string Repository::headBranch() const {
  std::error_code ec;
  if (!fs::is_regular_file(headPath(), ec)) {
    return "";
  }
  const std::string head = trim(readFile(headPath()));
  const std::string prefix = "ref: refs/heads/";
  if (head.rfind(prefix, 0) == 0) {
    return head.substr(prefix.size());
  }
  return "";
}

bool Repository::isDetached() const {
  return headBranch().empty() && headCommit().has_value();
}

std::string Repository::userName() const {
  const auto config = parseConfig(gitDir_ / "config");
  const auto it = config.find("user.name");
  return it == config.end() || it->second.empty() ? "MiniGit User"
                                                  : it->second;
}

std::string Repository::userEmail() const {
  const auto config = parseConfig(gitDir_ / "config");
  const auto it = config.find("user.email");
  return it == config.end() || it->second.empty() ? "minigit@example.com"
                                                  : it->second;
}

std::string Repository::relativeToWorkTree(const fs::path& absolute) const {
  const fs::path relative =
      absolute.lexically_normal().lexically_relative(workTree_);
  if (relative.empty() || *relative.begin() == "..") {
    throw MinigitError("path '" + absolute.string() +
                       "' is outside the repository at " +
                       workTree_.string());
  }
  if (relative.generic_string().rfind(std::string(kGitDirName) + "/", 0) == 0 ||
      relative.generic_string() == kGitDirName) {
    throw MinigitError("refusing to add the internal MiniGit directory");
  }
  return relative.generic_string();
}

void Repository::stageFile(const fs::path& absolute, Index& index,
                           std::vector<std::string>& staged) {
  const std::string relative = relativeToWorkTree(absolute);
  const Oid oid = objects_.write(ObjectType::Blob, readFile(absolute));
  index.add(relative, oid);
  staged.push_back(relative);
}

std::vector<std::string> Repository::add(
    const std::vector<fs::path>& paths) {
  if (paths.empty()) {
    throw MinigitError("nothing specified, nothing added");
  }

  Index index(indexPath());
  index.load();

  std::vector<std::string> staged;

  for (const fs::path& input : paths) {
    std::error_code ec;
    const fs::path absolute = fs::absolute(input, ec).lexically_normal();
    if (ec) {
      throw MinigitError("cannot resolve path '" + input.string() + "'");
    }
    if (!fs::exists(absolute, ec)) {
      throw MinigitError("pathspec '" + input.string() +
                         "' did not match any files");
    }

    if (fs::is_directory(absolute, ec)) {
      for (auto it = fs::recursive_directory_iterator(
               absolute, fs::directory_options::skip_permission_denied, ec);
           it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
          throw MinigitError("cannot walk '" + absolute.string() +
                             "': " + ec.message());
        }
        if (it->is_directory(ec) && shouldSkipEntry(it->path())) {
          it.disable_recursion_pending();
          continue;
        }
        if (it->is_regular_file(ec)) {
          stageFile(it->path(), index, staged);
        }
      }
    } else if (fs::is_regular_file(absolute, ec)) {
      stageFile(absolute, index, staged);
    } else {
      throw MinigitError("pathspec '" + input.string() +
                         "' is not a regular file");
    }
  }

  index.save();

  std::sort(staged.begin(), staged.end());
  staged.erase(std::unique(staged.begin(), staged.end()), staged.end());
  return staged;
}

Oid Repository::writeTreeFromIndex(const Index& index) {
  TreeNode root;
  for (const auto& [path, oid] : index.entries()) {
    insertFile(root, path, oid);
  }
  return writeTreeNode(objects_, root);
}

void Repository::updateHeadRef(const Oid& commitId) {
  const std::string head = trim(readFile(headPath()));
  if (head.rfind("ref: ", 0) == 0) {
    const fs::path refPath = gitDir_ / head.substr(5);
    std::error_code ec;
    fs::create_directories(refPath.parent_path(), ec);
    writeFile(refPath, commitId.hex() + "\n");
    return;
  }
  writeFile(headPath(), commitId.hex() + "\n");
}

Oid Repository::commit(const std::string& message) {
  if (trim(message).empty()) {
    throw MinigitError("empty commit message (use -m \"your message\")");
  }

  Index index(indexPath());
  index.load();
  if (index.empty()) {
    throw MinigitError("nothing to commit: the index is empty, use 'minigit "
                       "add <file>' first");
  }

  const auto parent = headCommit();
  if (parent.has_value()) {
    if (headTree() == index.entries()) {
      throw MinigitError("nothing to commit, working tree clean");
    }
  }

  Commit commit;
  commit.tree = writeTreeFromIndex(index);
  if (parent.has_value()) {
    commit.parents.push_back(*parent);
  }
  commit.author = userName() + " <" + userEmail() + ">";
  commit.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  commit.message = trim(message);

  const Oid commitId =
      objects_.write(ObjectType::Commit, commit.serialize());
  updateHeadRef(commitId);
  return commitId;
}

Oid Repository::treeOf(const Oid& commitId) const {
  const Object object = objects_.readOrThrow(commitId);
  if (object.type() != ObjectType::Commit) {
    throw MinigitError("object " + commitId.hex() + " is not a commit");
  }
  return Commit::parse(object.data()).tree;
}

std::map<std::string, Oid> Repository::headTree() const {
  const auto head = headCommit();
  if (!head.has_value()) {
    return {};
  }
  std::map<std::string, Oid> files;
  flattenTree(objects_, treeOf(*head), "", files);
  return files;
}

std::optional<Commit> Repository::readCommit(const Oid& commitId) const {
  return minigit::readCommit(objects_, commitId);
}

std::vector<std::pair<std::string, Oid>> Repository::branches() const {
  std::vector<std::pair<std::string, Oid>> result;

  const fs::path directory = gitDir_ / kRefsDir;
  std::error_code ec;
  if (!fs::is_directory(directory, ec)) {
    return result;
  }

  for (const auto& entry : fs::recursive_directory_iterator(directory, ec)) {
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    const std::string name =
        entry.path().lexically_relative(directory).generic_string();
    try {
      result.emplace_back(name, Oid::fromHex(trim(readFile(entry.path()))));
    } catch (const MinigitError&) {
      // A damaged ref file is ignored rather than breaking every command.
    }
  }

  std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  return result;
}

bool Repository::branchExists(const std::string& name) const {
  std::error_code ec;
  return !name.empty() &&
         fs::is_regular_file(gitDir_ / kRefsDir / name, ec);
}

void Repository::createBranch(const std::string& name) {
  validateBranchName(name);
  if (branchExists(name)) {
    throw MinigitError("a branch named '" + name + "' already exists");
  }

  const auto head = headCommit();
  if (!head.has_value()) {
    throw MinigitError("cannot create branch '" + name +
                       "': the repository has no commits yet");
  }

  const fs::path refPath = gitDir_ / kRefsDir / name;
  std::error_code ec;
  fs::create_directories(refPath.parent_path(), ec);
  if (ec) {
    throw MinigitError("cannot create branch '" + name + "': " + ec.message());
  }
  writeFile(refPath, head->hex() + "\n");
}

std::optional<Oid> Repository::resolveRevision(
    const std::string& revision) const {
  if (revision.empty()) {
    return std::nullopt;
  }

  if (branchExists(revision)) {
    return Oid::fromHex(trim(readFile(gitDir_ / kRefsDir / revision)));
  }

  if (auto oid = Oid::tryParse(revision)) {
    if (objects_.contains(*oid)) {
      return oid;
    }
    return std::nullopt;
  }

  return resolveIdPrefix(objects_, revision);
}

bool Repository::isClean() const {
  const StatusReport report = status();
  return report.staged.empty() && report.unstaged.empty();
}

void Repository::writeWorkingTree(
    const std::map<std::string, Oid>& targetTree,
    const Index& previousIndex) {
  std::error_code ec;

  // Files tracked before but absent from the target are removed, and their
  // now-empty directories pruned.
  for (const auto& [path, oid] : previousIndex.entries()) {
    (void)oid;
    if (targetTree.count(path) > 0) {
      continue;
    }
    const fs::path file = workTree_ / fs::path(path);
    fs::remove(file, ec);

    fs::path directory = file.parent_path();
    while (directory != workTree_ && directory.string().size() >
                                          workTree_.string().size()) {
      if (!fs::is_empty(directory, ec)) {
        break;
      }
      fs::remove(directory, ec);
      directory = directory.parent_path();
    }
  }

  for (const auto& [path, oid] : targetTree) {
    const Object blob = objects_.readOrThrow(oid);
    if (blob.type() != ObjectType::Blob) {
      throw MinigitError("tree entry '" + path + "' does not point at a blob");
    }
    const fs::path file = workTree_ / fs::path(path);
    fs::create_directories(file.parent_path(), ec);
    if (ec) {
      throw MinigitError("cannot create directory for '" + path +
                         "': " + ec.message());
    }
    writeFile(file, blob.data());
  }
}

void Repository::checkout(const std::string& target) {
  const auto revision = resolveRevision(target);
  if (!revision.has_value()) {
    throw MinigitError("pathspec '" + target +
                       "' did not match any branch or commit");
  }

  const Oid targetTreeId = treeOf(*revision);
  if (!isClean()) {
    throw MinigitError(
        "your local changes would be overwritten by checkout; commit them "
        "or add them first");
  }

  std::map<std::string, Oid> targetTree;
  flattenTree(objects_, targetTreeId, "", targetTree);

  Index previous(indexPath());
  previous.load();

  for (const auto& [path, oid] : targetTree) {
    (void)oid;
    std::error_code ec;
    if (previous.oidFor(path).has_value()) {
      continue;
    }
    if (fs::exists(workTree_ / fs::path(path), ec)) {
      throw MinigitError("untracked working tree file '" + path +
                         "' would be overwritten by checkout");
    }
  }

  writeWorkingTree(targetTree, previous);

  Index next(indexPath());
  next.clear();
  for (const auto& [path, oid] : targetTree) {
    next.add(path, oid);
  }
  next.save();

  if (branchExists(target)) {
    writeFile(headPath(), "ref: refs/heads/" + target + "\n");
  } else {
    writeFile(headPath(), revision->hex() + "\n");
  }
}

std::string Repository::readBlob(const Oid& oid) const {
  const Object object = objects_.readOrThrow(oid);
  if (object.type() != ObjectType::Blob) {
    throw MinigitError("object " + oid.hex() + " is not a blob");
  }
  return object.data();
}

std::vector<FileDiff> Repository::diff(bool staged,
                                       DiffAlgorithm algorithm) const {
  Index index(indexPath());
  index.load();

  std::map<std::string, std::string> before;
  std::map<std::string, std::string> after;

  if (staged) {
    const auto head = headCommit();
    if (head.has_value()) {
      for (const auto& [path, oid] : headTree()) {
        before[path] = readBlob(oid);
      }
    }
    for (const auto& [path, oid] : index.entries()) {
      after[path] = readBlob(oid);
    }
  } else {
    for (const auto& [path, oid] : index.entries()) {
      before[path] = readBlob(oid);

      const fs::path file = workTree_ / fs::path(path);
      std::error_code ec;
      if (fs::is_regular_file(file, ec)) {
        after[path] = readFile(file);
      }
    }
  }

  std::set<std::string> paths;
  for (const auto& [path, content] : before) {
    (void)content;
    paths.insert(path);
  }
  for (const auto& [path, content] : after) {
    (void)content;
    paths.insert(path);
  }

  std::vector<FileDiff> diffs;
  for (const std::string& path : paths) {
    const auto oldIt = before.find(path);
    const auto newIt = after.find(path);
    const std::string oldText = oldIt == before.end() ? "" : oldIt->second;
    const std::string newText = newIt == after.end() ? "" : newIt->second;
    if (oldText == newText) {
      continue;
    }

    FileDiff fileDiff;
    fileDiff.path = path;
    if (looksBinary(oldText) || looksBinary(newText)) {
      fileDiff.text = "Binary files a/" + path + " and b/" + path +
                      " differ\n";
    } else {
      fileDiff.text = renderUnifiedDiff("a/" + path, "b/" + path, oldText,
                                        newText, algorithm);
    }
    diffs.push_back(std::move(fileDiff));
  }

  return diffs;
}

StatusReport Repository::status() const {
  StatusReport report;
  report.branch = headBranch();
  report.detached = report.branch.empty();

  const auto head = headCommit();
  report.hasCommits = head.has_value();
  if (report.detached && report.hasCommits) {
    report.branch = head->shortHex();
  }
  if (report.branch.empty()) {
    report.branch = "main";
  }

  Index index(indexPath());
  index.load();

  const std::map<std::string, Oid> committed =
      report.hasCommits ? headTree() : std::map<std::string, Oid>{};

  for (const auto& [path, oid] : index.entries()) {
    const auto old = committed.find(path);
    if (old == committed.end()) {
      report.staged.push_back({path, ChangeKind::Added});
    } else if (old->second != oid) {
      report.staged.push_back({path, ChangeKind::Modified});
    }
  }
  for (const auto& [path, oid] : committed) {
    (void)oid;
    if (index.oidFor(path) == std::nullopt) {
      report.staged.push_back({path, ChangeKind::Deleted});
    }
  }

  for (const auto& [path, oid] : index.entries()) {
    const fs::path absolute = workTree_ / fs::path(path);
    std::error_code ec;
    if (!fs::is_regular_file(absolute, ec)) {
      report.unstaged.push_back({path, ChangeKind::Deleted});
      continue;
    }
    if (hashWorkingFile(absolute) != oid) {
      report.unstaged.push_back({path, ChangeKind::Modified});
    }
  }

  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(
           workTree_, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) {
      break;
    }
    if (it->is_directory(ec) && shouldSkipEntry(it->path())) {
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file(ec)) {
      continue;
    }
    const std::string relative =
        it->path().lexically_relative(workTree_).generic_string();
    if (index.oidFor(relative) == std::nullopt) {
      report.untracked.push_back(relative);
    }
  }

  std::sort(report.staged.begin(), report.staged.end(),
            [](const StagedChange& a, const StagedChange& b) {
              return a.path < b.path;
            });
  std::sort(report.unstaged.begin(), report.unstaged.end(),
            [](const StagedChange& a, const StagedChange& b) {
              return a.path < b.path;
            });
  std::sort(report.untracked.begin(), report.untracked.end());
  return report;
}

}  // namespace minigit
