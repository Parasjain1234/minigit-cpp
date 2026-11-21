#include "repo/history.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "core/error.h"
#include "objects/commit.h"
#include "objects/object.h"
#include "objects/object_store.h"
#include "test_util.h"

using namespace minigit;
using minigit_test::TempDir;

namespace {

Oid makeCommit(ObjectStore& store, const Oid& tree,
               const std::vector<Oid>& parents, std::int64_t timestamp,
               const std::string& message) {
  Commit commit;
  commit.tree = tree;
  commit.parents = parents;
  commit.author = "Test <test@example.com>";
  commit.timestamp = timestamp;
  commit.message = message;
  return store.write(ObjectType::Commit, commit.serialize());
}

}  // namespace

TEST(HistoryTest, ReadsCommitById) {
  TempDir temp;
  ObjectStore store(temp.path());
  const Oid tree = store.write(ObjectType::Tree, "");
  const Oid commit = makeCommit(store, tree, {}, 100, "hello");

  const auto read = readCommit(store, commit);
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(read->tree, tree);
  EXPECT_EQ(read->summary(), "hello");
  EXPECT_TRUE(read->parents.empty());
}

TEST(HistoryTest, MissingCommitReturnsNullopt) {
  TempDir temp;
  ObjectStore store(temp.path());
  EXPECT_FALSE(readCommit(store, Oid::hashBytes("nope")).has_value());
}

TEST(HistoryTest, WalksLinearHistoryNewestFirst) {
  TempDir temp;
  ObjectStore store(temp.path());
  const Oid tree = store.write(ObjectType::Tree, "");

  const Oid first = makeCommit(store, tree, {}, 10, "first");
  const Oid second = makeCommit(store, tree, {first}, 20, "second");
  const Oid third = makeCommit(store, tree, {second}, 30, "third");

  const auto entries = walkHistory(store, third);
  ASSERT_EQ(entries.size(), std::size_t{3});
  EXPECT_EQ(entries[0].oid, third);
  EXPECT_EQ(entries[1].oid, second);
  EXPECT_EQ(entries[2].oid, first);
}

TEST(HistoryTest, MergeVisitsEveryCommitExactlyOnce) {
  TempDir temp;
  ObjectStore store(temp.path());
  const Oid tree = store.write(ObjectType::Tree, "");

  const Oid base = makeCommit(store, tree, {}, 10, "base");
  const Oid left = makeCommit(store, tree, {base}, 20, "left");
  const Oid right = makeCommit(store, tree, {base}, 30, "right");
  const Oid merge = makeCommit(store, tree, {left, right}, 40, "merge");

  const auto entries = walkHistory(store, merge);
  ASSERT_EQ(entries.size(), std::size_t{4});
  EXPECT_EQ(entries.front().oid, merge);
  EXPECT_EQ(entries.back().oid, base);
  ASSERT_EQ(entries.front().commit.parents.size(), std::size_t{2});

  std::vector<Oid> seen;
  for (const auto& entry : entries) {
    for (const Oid& other : seen) {
      EXPECT_NE(other, entry.oid);
    }
    seen.push_back(entry.oid);
  }
}

TEST(HistoryTest, ReportsMissingParent) {
  TempDir temp;
  ObjectStore store(temp.path());
  const Oid tree = store.write(ObjectType::Tree, "");
  const Oid orphan =
      makeCommit(store, tree, {Oid::hashBytes("missing")}, 10, "orphan");

  EXPECT_THROW(walkHistory(store, orphan), MinigitError);
}
