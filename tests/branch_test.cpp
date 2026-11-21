#include "repo/repository.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "core/error.h"
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

TEST(BranchTest, CreateBranchAtHead) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "first");

  fixture.repo.createBranch("feature");
  ASSERT_TRUE(fixture.repo.branchExists("feature"));
  EXPECT_EQ(*fixture.repo.resolveRevision("feature"),
            *fixture.repo.headCommit());

  const auto branches = fixture.repo.branches();
  ASSERT_EQ(branches.size(), std::size_t{2});
  EXPECT_EQ(branches[0].first, "feature");
  EXPECT_EQ(branches[1].first, "main");
}

TEST(BranchTest, RejectsDuplicateAndInvalidNames) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "first");
  fixture.repo.createBranch("feature");

  EXPECT_THROW(fixture.repo.createBranch("feature"), MinigitError);
  EXPECT_THROW(fixture.repo.createBranch("bad name"), MinigitError);
  EXPECT_THROW(fixture.repo.createBranch("../escape"), MinigitError);
  EXPECT_THROW(fixture.repo.createBranch(""), MinigitError);
}

TEST(BranchTest, CreateBranchNeedsACommit) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  EXPECT_THROW(repo.createBranch("feature"), MinigitError);
}

TEST(BranchTest, CheckoutSwitchesTheWorkingTree) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "main commit");
  fixture.repo.createBranch("feature");

  fixture.repo.checkout("feature");
  EXPECT_EQ(fixture.repo.headBranch(), "feature");
  EXPECT_EQ(readTextFile(fixture.temp.path() / "a.txt"), "one\n");

  fixture.commitFile("a.txt", "two\n", "feature commit");
  const Oid featureTip = *fixture.repo.headCommit();

  fixture.repo.checkout("main");
  EXPECT_EQ(fixture.repo.headBranch(), "main");
  EXPECT_EQ(readTextFile(fixture.temp.path() / "a.txt"), "one\n");

  fixture.repo.checkout("feature");
  EXPECT_EQ(*fixture.repo.headCommit(), featureTip);
  EXPECT_EQ(readTextFile(fixture.temp.path() / "a.txt"), "two\n");
}

TEST(BranchTest, CheckoutRemovesFilesMissingFromTarget) {
  Fixture fixture;
  fixture.commitFile("keep.txt", "keep\n", "base");
  fixture.repo.createBranch("feature");

  fixture.repo.checkout("feature");
  fixture.commitFile("extra.txt", "extra\n", "add extra");

  fixture.repo.checkout("main");
  EXPECT_TRUE(fs::exists(fixture.temp.path() / "keep.txt"));
  EXPECT_FALSE(fs::exists(fixture.temp.path() / "extra.txt"));

  fixture.repo.checkout("feature");
  EXPECT_TRUE(fs::exists(fixture.temp.path() / "extra.txt"));
}

TEST(BranchTest, CheckoutCommitDetachesHead) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "first");
  const Oid tip = *fixture.repo.headCommit();

  fixture.repo.checkout(tip.shortHex(8));
  EXPECT_TRUE(fixture.repo.headBranch().empty());
  EXPECT_EQ(*fixture.repo.headCommit(), tip);
}

TEST(BranchTest, CheckoutRefusesADirtyWorkingTree) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "base");
  fixture.repo.createBranch("feature");

  writeTextFile(fixture.temp.path() / "a.txt", "uncommitted\n");
  EXPECT_THROW(fixture.repo.checkout("feature"), MinigitError);
  EXPECT_EQ(fixture.repo.headBranch(), "main");
}

TEST(BranchTest, CheckoutUnknownRevisionFails) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "base");
  EXPECT_THROW(fixture.repo.checkout("does-not-exist"), MinigitError);
}

TEST(BranchTest, ResolvesFullAndPrefixedObjectIds) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "base");
  const Oid tip = *fixture.repo.headCommit();

  EXPECT_EQ(fixture.repo.resolveRevision(tip.hex()), tip);
  EXPECT_EQ(fixture.repo.resolveRevision(tip.shortHex(8)), tip);
  EXPECT_EQ(fixture.repo.resolveRevision("deadbeef"), std::nullopt);
}
