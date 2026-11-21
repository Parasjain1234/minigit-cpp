#include "objects/object_store.h"
#include "objects/tree.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "core/error.h"
#include "test_util.h"

using namespace minigit;
namespace fs = std::filesystem;

TEST(ObjectTest, SerializeUsesHeaderAndPayload) {
  const Object object(ObjectType::Blob, "hello\n");
  EXPECT_EQ(object.serialize(), std::string("blob 6\0hello\n", 13));
}

TEST(ObjectTest, OidIsSha1OfSerializedForm) {
  const Object object(ObjectType::Blob, "hello\n");
  EXPECT_EQ(object.oid().hex(), Oid::hashBytes(object.serialize()).hex());
  EXPECT_EQ(object.oid().hex(), "ce013625030ba8dba906f756967f9e9ca394464a");
}

TEST(ObjectTest, RoundTripsThroughSerialization) {
  const Object original(ObjectType::Commit, "tree abc\n\nmessage\n");
  const Object parsed = Object::deserialize(original.serialize());
  EXPECT_EQ(parsed.type(), ObjectType::Commit);
  EXPECT_EQ(parsed.data(), original.data());
}

TEST(ObjectTest, RejectsMalformedHeader) {
  EXPECT_THROW(Object::deserialize("blob xyz\0data"), MinigitError);
  EXPECT_THROW(Object::deserialize("blob 99\0data"), MinigitError);
  EXPECT_THROW(Object::deserialize("widget 4\0data"), MinigitError);
}

TEST(ObjectStoreTest, StoresUnderTwoCharacterShard) {
  minigit_test::TempDir temp;
  ObjectStore store(temp.path() / "objects");

  const Oid oid = store.write(ObjectType::Blob, "content\n");
  const std::string hex = oid.hex();
  const fs::path expected = temp.path() / "objects" / hex.substr(0, 2) /
                            hex.substr(2);

  EXPECT_TRUE(fs::exists(expected));
  EXPECT_EQ(store.pathFor(oid), expected);
  EXPECT_TRUE(store.contains(oid));
}

TEST(ObjectStoreTest, ReadsBackTheSamePayload) {
  minigit_test::TempDir temp;
  ObjectStore store(temp.path() / "objects");

  const Oid oid = store.write(ObjectType::Tree, "tree payload");
  const auto read = store.read(oid);
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(read->type(), ObjectType::Tree);
  EXPECT_EQ(read->data(), "tree payload");
}

TEST(ObjectStoreTest, IdenticalContentIsDeduplicated) {
  minigit_test::TempDir temp;
  ObjectStore store(temp.path() / "objects");

  const Oid first = store.write(ObjectType::Blob, "same bytes");
  const Oid second = store.write(ObjectType::Blob, "same bytes");
  EXPECT_EQ(first, second);

  std::size_t objectFiles = 0;
  for (const auto& entry : fs::recursive_directory_iterator(temp.path())) {
    if (entry.is_regular_file()) {
      ++objectFiles;
    }
  }
  EXPECT_EQ(objectFiles, std::size_t{1});
}

TEST(ObjectStoreTest, MissingObjectReadsAsNullopt) {
  minigit_test::TempDir temp;
  ObjectStore store(temp.path() / "objects");
  EXPECT_FALSE(store.read(Oid::hashBytes("absent")).has_value());
  EXPECT_THROW(store.readOrThrow(Oid::hashBytes("absent")), MinigitError);
}

TEST(ObjectStoreTest, DifferentTypesProduceDifferentIds) {
  const Oid blob = Object(ObjectType::Blob, "x").oid();
  const Oid tree = Object(ObjectType::Tree, "x").oid();
  EXPECT_NE(blob, tree);
}

TEST(TreeTest, SerializesEntriesSortedByName) {
  const Oid a = Oid::hashBytes("a");
  const Oid b = Oid::hashBytes("b");
  Tree tree({TreeEntry{kFileMode, ObjectType::Blob, b, "zeta.txt"},
             TreeEntry{kFileMode, ObjectType::Blob, a, "alpha.txt"}});

  const std::string serialized = tree.serialize();
  EXPECT_LT(serialized.find("alpha.txt"), serialized.find("zeta.txt"));
  EXPECT_EQ(tree.entries().front().name, "alpha.txt");
  EXPECT_EQ(tree.find("zeta.txt")->oid, b);
}

TEST(TreeTest, ParsesBackToTheSameEntries) {
  const Oid blobOid = Oid::hashBytes("blob");
  const Oid treeOid = Oid::hashBytes("tree");
  Tree original({TreeEntry{kFileMode, ObjectType::Blob, blobOid, "main.cpp"},
                 TreeEntry{kTreeMode, ObjectType::Tree, treeOid, "src"}});

  const Tree parsed = Tree::parse(original.serialize());
  ASSERT_EQ(parsed.entries().size(), std::size_t{2});
  EXPECT_EQ(parsed.entries()[0].name, "main.cpp");
  EXPECT_EQ(parsed.entries()[1].type, ObjectType::Tree);
  EXPECT_EQ(parsed.entries()[1].mode, kTreeMode);
}

TEST(TreeTest, RejectsCorruptEntries) {
  EXPECT_THROW(Tree::parse("100644 blob\n"), MinigitError);
  EXPECT_THROW(Tree::parse("100644 widget abcdef name\n"), MinigitError);
}
