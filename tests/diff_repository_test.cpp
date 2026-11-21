#include "repo/repository.h"

#include <gtest/gtest.h>

#include <string>

#include "diff/diff_types.h"
#include "test_util.h"

using namespace minigit;
using minigit_test::TempDir;
using minigit_test::writeTextFile;

TEST(RepositoryDiffTest, ShowsUnstagedModification) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "a.txt", "one\ntwo\nthree\n");
  repo.add({temp.path() / "a.txt"});
  repo.commit("base");

  writeTextFile(temp.path() / "a.txt", "one\nTWO\nthree\n");

  const auto diffs = repo.diff(false, DiffAlgorithm::Lcs);
  ASSERT_EQ(diffs.size(), std::size_t{1});
  EXPECT_EQ(diffs.front().path, "a.txt");
  EXPECT_NE(diffs.front().text.find("--- a/a.txt"), std::string::npos);
  EXPECT_NE(diffs.front().text.find("-two\n"), std::string::npos);
  EXPECT_NE(diffs.front().text.find("+TWO\n"), std::string::npos);
}

TEST(RepositoryDiffTest, StagedDiffComparesIndexWithHead) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "a.txt", "one\n");
  repo.add({temp.path() / "a.txt"});
  repo.commit("base");

  writeTextFile(temp.path() / "a.txt", "one\ntwo\n");
  repo.add({temp.path() / "a.txt"});

  EXPECT_TRUE(repo.diff(false, DiffAlgorithm::Lcs).empty());

  const auto staged = repo.diff(true, DiffAlgorithm::Myers);
  ASSERT_EQ(staged.size(), std::size_t{1});
  EXPECT_NE(staged.front().text.find("+two\n"), std::string::npos);
}

TEST(RepositoryDiffTest, NoDiffWhenEverythingMatches) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "a.txt", "one\n");
  repo.add({temp.path() / "a.txt"});
  repo.commit("base");

  EXPECT_TRUE(repo.diff(false, DiffAlgorithm::Lcs).empty());
  EXPECT_TRUE(repo.diff(true, DiffAlgorithm::Lcs).empty());
}

TEST(RepositoryDiffTest, ReportsBinaryFiles) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  const std::string binary = std::string("head\0tail", 9);
  writeTextFile(temp.path() / "data.bin", binary);
  repo.add({temp.path() / "data.bin"});
  repo.commit("binary");

  writeTextFile(temp.path() / "data.bin", std::string("head\0changed", 12));
  const auto diffs = repo.diff(false, DiffAlgorithm::Lcs);
  ASSERT_EQ(diffs.size(), std::size_t{1});
  EXPECT_NE(diffs.front().text.find("Binary files"), std::string::npos);
}

TEST(RepositoryDiffTest, DiffOfDeletedWorkingFileShowsRemovals) {
  TempDir temp;
  Repository repo = Repository::init(temp.path());
  writeTextFile(temp.path() / "a.txt", "one\ntwo\n");
  repo.add({temp.path() / "a.txt"});
  repo.commit("base");

  std::filesystem::remove(temp.path() / "a.txt");
  const auto diffs = repo.diff(false, DiffAlgorithm::Lcs);
  ASSERT_EQ(diffs.size(), std::size_t{1});
  EXPECT_NE(diffs.front().text.find("-one\n"), std::string::npos);
}
