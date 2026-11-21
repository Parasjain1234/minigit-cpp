#include "repo/repository.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "core/error.h"
#include "objects/commit.h"
#include "objects/tree.h"
#include "test_util.h"

using namespace minigit;
namespace fs = std::filesystem;
using minigit_test::readTextFile;
using minigit_test::TempDir;
using minigit_test::writeTextFile;

TEST(RepositoryTest, InitCreatesTheRepositoryLayout) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());

  EXPECT_TRUE(fs::is_directory(temp.path() / ".minigit" / "objects"));
  EXPECT_TRUE(fs::is_directory(temp.path() / ".minigit" / "refs" / "heads"));
  EXPECT_EQ(readTextFile(temp.path() / ".minigit" / "HEAD"),
            "ref: refs/heads/main\n");
  EXPECT_EQ(readTextFile(temp.path() / ".minigit" / "index"), "");
  EXPECT_EQ(repo.headBranch(), "main");
  EXPECT_FALSE(repo.headCommit().has_value());
}

TEST(RepositoryTest, InitIsIdempotent) {
  TempDir temp;
  Repository::init(temp.path());
  writeTextFile(temp.path() / ".minigit" / "HEAD", "ref: refs/heads/trunk\n");

  Repository::init(temp.path());
  EXPECT_EQ(readTextFile(temp.path() / ".minigit" / "HEAD"),
            "ref: refs/heads/trunk\n");
}

TEST(RepositoryTest, OpenFindsRepositoryFromSubdirectory) {
  TempDir temp;
  Repository::init(temp.path());
  fs::create_directories(temp.path() / "src" / "nested");

  const Repository repo = Repository::open(temp.path() / "src" / "nested");
  EXPECT_EQ(repo.workTree(), temp.path().lexically_normal());
}

TEST(RepositoryTest, OpenFailsOutsideRepository) {
  TempDir temp;
  EXPECT_THROW(Repository::open(temp.path()), MinigitError);
}

TEST(RepositoryTest, AddStoresBlobAndIndexEntry) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "hello.txt", "hello world\n");

  const auto staged = repo.add({temp.path() / "hello.txt"});
  ASSERT_EQ(staged.size(), std::size_t{1});
  EXPECT_EQ(staged.front(), "hello.txt");

  const std::string indexText = readTextFile(temp.path() / ".minigit" / "index");
  const Oid expected = Object(ObjectType::Blob, "hello world\n").oid();
  EXPECT_EQ(indexText, expected.hex() + " hello.txt\n");
}

TEST(RepositoryTest, AddRejectsMissingPath) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  EXPECT_THROW(repo.add({temp.path() / "nope.txt"}), MinigitError);
}

TEST(RepositoryTest, CommitWritesMerkleTreeAndUpdatesBranch) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "hello.txt", "hello\n");
  fs::create_directories(temp.path() / "src");
  writeTextFile(temp.path() / "src" / "main.cpp", "int main() {}\n");

  repo.add({temp.path()});
  const Oid commitId = repo.commit("first commit");

  EXPECT_EQ(readTextFile(temp.path() / ".minigit" / "refs" / "heads" / "main"),
            commitId.hex() + "\n");

  const Object commitObject = repo.objects().readOrThrow(commitId);
  ASSERT_EQ(commitObject.type(), ObjectType::Commit);
  const Commit commit = Commit::parse(commitObject.data());
  EXPECT_EQ(commit.summary(), "first commit");
  EXPECT_TRUE(commit.parents.empty());
  EXPECT_EQ(commit.tree, repo.treeOf(commitId));

  // Root tree must contain a blob and a nested tree.
  const Object rootObject = repo.objects().readOrThrow(commit.tree);
  const Tree root = Tree::parse(rootObject.data());
  ASSERT_EQ(root.entries().size(), std::size_t{2});
  ASSERT_NE(root.find("hello.txt"), nullptr);
  ASSERT_NE(root.find("src"), nullptr);
  EXPECT_EQ(root.find("hello.txt")->type, ObjectType::Blob);
  EXPECT_EQ(root.find("src")->type, ObjectType::Tree);

  // The nested tree points at the blob for src/main.cpp.
  const Object nestedObject = repo.objects().readOrThrow(root.find("src")->oid);
  const Tree nested = Tree::parse(nestedObject.data());
  ASSERT_NE(nested.find("main.cpp"), nullptr);
  EXPECT_EQ(nested.find("main.cpp")->oid,
            Object(ObjectType::Blob, "int main() {}\n").oid());
}

TEST(RepositoryTest, SecondCommitPointsAtTheFirst) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "a.txt", "one\n");
  repo.add({temp.path() / "a.txt"});
  const Oid first = repo.commit("first");

  writeTextFile(temp.path() / "a.txt", "two\n");
  repo.add({temp.path() / "a.txt"});
  const Oid second = repo.commit("second");

  const Commit commit = Commit::parse(repo.objects().readOrThrow(second).data());
  ASSERT_EQ(commit.parents.size(), std::size_t{1});
  EXPECT_EQ(commit.parents.front(), first);
}

TEST(RepositoryTest, CommitRejectsEmptyIndex) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  EXPECT_THROW(repo.commit("nothing"), MinigitError);
}

TEST(RepositoryTest, CommitRejectsUnchangedTree) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "a.txt", "one\n");
  repo.add({temp.path() / "a.txt"});
  repo.commit("first");
  EXPECT_THROW(repo.commit("again"), MinigitError);
}

TEST(RepositoryTest, CommitRejectsEmptyMessage) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "a.txt", "one\n");
  repo.add({temp.path() / "a.txt"});
  EXPECT_THROW(repo.commit("   "), MinigitError);
}

TEST(RepositoryTest, StatusReportsStagedUnstagedAndUntracked) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());

  writeTextFile(temp.path() / "tracked.txt", "one\n");
  writeTextFile(temp.path() / "loose.txt", "loose\n");
  repo.add({temp.path() / "tracked.txt"});

  StatusReport report = repo.status();
  EXPECT_FALSE(report.hasCommits);
  ASSERT_EQ(report.staged.size(), std::size_t{1});
  EXPECT_EQ(report.staged.front().kind, ChangeKind::Added);
  EXPECT_EQ(report.untracked, std::vector<std::string>{"loose.txt"});

  repo.commit("first");

  writeTextFile(temp.path() / "tracked.txt", "two\n");
  report = repo.status();
  EXPECT_TRUE(report.hasCommits);
  EXPECT_TRUE(report.staged.empty());
  ASSERT_EQ(report.unstaged.size(), std::size_t{1});
  EXPECT_EQ(report.unstaged.front().kind, ChangeKind::Modified);
  EXPECT_EQ(report.untracked, std::vector<std::string>{"loose.txt"});
}

TEST(RepositoryTest, StatusDetectsDeletedTrackedFile) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "gone.txt", "bye\n");
  repo.add({temp.path() / "gone.txt"});
  repo.commit("first");

  fs::remove(temp.path() / "gone.txt");
  const StatusReport report = repo.status();
  ASSERT_EQ(report.unstaged.size(), std::size_t{1});
  EXPECT_EQ(report.unstaged.front().kind, ChangeKind::Deleted);
}
