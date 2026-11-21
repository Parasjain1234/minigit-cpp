#include "repo/repository.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "core/error.h"
#include "objects/commit.h"
#include "test_util.h"

using namespace minigit;
namespace fs = std::filesystem;
using minigit_test::readTextFile;
using minigit_test::TempDir;
using minigit_test::writeTextFile;

namespace {

struct Fixture {
  TempDir temp;
  Repository repo = Repository::init(temp.path());

  void commitFile(const std::string& name, const std::string& contents,
                  const std::string& message) {
    writeTextFile(temp.path() / name, contents);
    repo.add({temp.path() / name});
    repo.commit(message);
  }
};

}  // namespace

TEST(MergeTest, FastForwardsWhenMainHasNotMoved) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "base");
  fixture.repo.createBranch("feature");
  fixture.repo.checkout("feature");
  fixture.commitFile("a.txt", "two\n", "feature work");
  const Oid featureTip = *fixture.repo.headCommit();

  fixture.repo.checkout("main");
  const MergeReport report = fixture.repo.merge("feature", "");

  EXPECT_EQ(report.kind, MergeKind::FastForward);
  EXPECT_EQ(*fixture.repo.headCommit(), featureTip);
  EXPECT_EQ(readTextFile(fixture.temp.path() / "a.txt"), "two\n");
}

TEST(MergeTest, CreatesAMergeCommitWithTwoParents) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "base");
  fixture.commitFile("b.txt", "bee\n", "base b");
  const Oid base = *fixture.repo.headCommit();

  fixture.repo.createBranch("feature");
  fixture.repo.checkout("feature");
  fixture.commitFile("b.txt", "BEE\n", "feature changes b");

  fixture.repo.checkout("main");
  fixture.commitFile("a.txt", "ONE\n", "main changes a");

  const MergeReport report = fixture.repo.merge("feature", "merge feature");
  ASSERT_EQ(report.kind, MergeKind::Merged);

  const auto head = fixture.repo.headCommit();
  ASSERT_TRUE(head.has_value());
  const auto commit = fixture.repo.readCommit(*head);
  ASSERT_TRUE(commit.has_value());
  ASSERT_EQ(commit->parents.size(), std::size_t{2});
  EXPECT_EQ(commit->summary(), "merge feature");
  EXPECT_NE(commit->parents[0], commit->parents[1]);

  // Both sides survived the merge.
  EXPECT_EQ(readTextFile(fixture.temp.path() / "a.txt"), "ONE\n");
  EXPECT_EQ(readTextFile(fixture.temp.path() / "b.txt"), "BEE\n");
  EXPECT_EQ(report.base, base);
}

TEST(MergeTest, ReportsConflictsAndWritesMarkers) {
  Fixture fixture;
  fixture.commitFile("conflict.txt", "one\ntwo\nthree\n", "base");
  fixture.repo.createBranch("feature");
  fixture.repo.checkout("feature");
  fixture.commitFile("conflict.txt", "one\ntheirs\nthree\n", "their edit");

  fixture.repo.checkout("main");
  fixture.commitFile("conflict.txt", "one\nours\nthree\n", "our edit");

  const MergeReport report = fixture.repo.merge("feature", "");
  ASSERT_EQ(report.kind, MergeKind::Conflicts);
  ASSERT_EQ(report.conflicts.size(), std::size_t{1});
  EXPECT_EQ(report.conflicts.front(), "conflict.txt");

  const std::string content =
      readTextFile(fixture.temp.path() / "conflict.txt");
  EXPECT_NE(content.find("<<<<<<< ours\n"), std::string::npos);
  EXPECT_NE(content.find("=======\n"), std::string::npos);
  EXPECT_NE(content.find(">>>>>>> theirs\n"), std::string::npos);
  EXPECT_TRUE(fs::exists(fixture.temp.path() / ".minigit" / "MERGE_HEAD"));
}

TEST(MergeTest, CommitAfterResolvingUsesBothParents) {
  Fixture fixture;
  fixture.commitFile("f.txt", "one\ntwo\n", "base");
  fixture.repo.createBranch("feature");
  fixture.repo.checkout("feature");
  fixture.commitFile("f.txt", "one\ntheirs\n", "their edit");

  fixture.repo.checkout("main");
  fixture.commitFile("f.txt", "one\nours\n", "our edit");

  ASSERT_EQ(fixture.repo.merge("feature", "").kind, MergeKind::Conflicts);

  writeTextFile(fixture.temp.path() / "f.txt", "one\nresolved\n");
  const Oid mergeCommit = fixture.repo.commit("resolution");

  const auto commit = fixture.repo.readCommit(mergeCommit);
  ASSERT_TRUE(commit.has_value());
  EXPECT_EQ(commit->parents.size(), std::size_t{2});
  EXPECT_FALSE(fs::exists(fixture.temp.path() / ".minigit" / "MERGE_HEAD"));
}

TEST(MergeTest, AlreadyMergedBranchIsUpToDate) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "base");
  fixture.repo.createBranch("feature");
  fixture.repo.checkout("feature");
  fixture.commitFile("b.txt", "bee\n", "feature");
  fixture.repo.checkout("main");
  fixture.commitFile("c.txt", "sea\n", "main");

  ASSERT_EQ(fixture.repo.merge("feature", "").kind, MergeKind::Merged);
  EXPECT_EQ(fixture.repo.merge("feature", "").kind, MergeKind::UpToDate);
}

TEST(MergeTest, RefusesToMergeWithLocalChanges) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "base");
  fixture.repo.createBranch("feature");
  fixture.repo.checkout("feature");
  fixture.commitFile("b.txt", "bee\n", "feature");
  fixture.repo.checkout("main");

  writeTextFile(fixture.temp.path() / "a.txt", "dirty\n");
  EXPECT_THROW(fixture.repo.merge("feature", ""), MinigitError);
}

TEST(MergeTest, UnknownBranchFails) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "base");
  EXPECT_THROW(fixture.repo.merge("nope", ""), MinigitError);
}
