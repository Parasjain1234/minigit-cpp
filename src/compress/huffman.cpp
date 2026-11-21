#include "compress/huffman.h"

#include <array>
#include <cstdint>
#include <queue>
#include <string>
#include <vector>

#include "core/error.h"

namespace minigit {
namespace {

constexpr char kMagic[4] = {'M', 'G', 'H', '1'};
constexpr std::size_t kSymbolCount = 256;
constexpr int kInternalSymbol = 256;

/// Nodes live in a flat vector; children are indices, which avoids any
/// ownership juggling while the tree is assembled.
struct Node {
  std::uint64_t frequency = 0;
  int symbol = kInternalSymbol;
  int left = -1;
  int right = -1;

  bool isLeaf() const { return left < 0 && right < 0; }
};

struct NodeGreater {
  const std::vector<Node>* nodes = nullptr;

  bool operator()(int left, int right) const {
    const Node& a = (*nodes)[static_cast<std::size_t>(left)];
    const Node& b = (*nodes)[static_cast<std::size_t>(right)];
    if (a.frequency != b.frequency) {
      return a.frequency > b.frequency;  // min-heap on frequency
    }
    return a.symbol > b.symbol;  // deterministic tie-break
  }
};

void appendU16(std::string& out, std::uint16_t value) {
  out.push_back(static_cast<char>(value & 0xFF));
  out.push_back(static_cast<char>((value >> 8) & 0xFF));
}

void appendU32(std::string& out, std::uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
  }
}

void appendU64(std::string& out, std::uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
  }
}

void requireBytes(const std::string& data, std::size_t offset,
                  std::size_t count, const char* what) {
  if (offset + count > data.size()) {
    throw MinigitError(std::string("corrupt compressed object: truncated ") +
                       what);
  }
}

std::uint16_t readU16(const std::string& data, std::size_t offset) {
  requireBytes(data, offset, 2, "symbol count");
  return static_cast<std::uint16_t>(
      static_cast<unsigned char>(data[offset]) |
      (static_cast<unsigned char>(data[offset + 1]) << 8));
}

std::uint32_t readU32(const std::string& data, std::size_t offset) {
  requireBytes(data, offset, 4, "size field");
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    value |= static_cast<std::uint32_t>(
                 static_cast<unsigned char>(data[offset + i]))
             << (8 * i);
  }
  return value;
}

std::uint64_t readU64(const std::string& data, std::size_t offset) {
  requireBytes(data, offset, 8, "frequency field");
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(
                 static_cast<unsigned char>(data[offset + i]))
             << (8 * i);
  }
  return value;
}

/// Builds the Huffman tree for a frequency table and returns the node index
/// of the root, or -1 when there are no symbols at all.
int buildTree(const std::array<std::uint64_t, kSymbolCount>& frequencies,
              std::vector<Node>& nodes) {
  NodeGreater comparator;
  comparator.nodes = &nodes;
  std::priority_queue<int, std::vector<int>, NodeGreater> queue(comparator);

  for (int symbol = 0; symbol < 256; ++symbol) {
    if (frequencies[static_cast<std::size_t>(symbol)] == 0) {
      continue;
    }
    Node leaf;
    leaf.frequency = frequencies[static_cast<std::size_t>(symbol)];
    leaf.symbol = symbol;
    nodes.push_back(leaf);
    queue.push(static_cast<int>(nodes.size()) - 1);
  }

  if (queue.empty()) {
    return -1;
  }

  while (queue.size() > 1) {
    const int first = queue.top();
    queue.pop();
    const int second = queue.top();
    queue.pop();

    Node parent;
    parent.frequency =
        nodes[static_cast<std::size_t>(first)].frequency +
        nodes[static_cast<std::size_t>(second)].frequency;
    parent.symbol = kInternalSymbol;
    parent.left = first;
    parent.right = second;
    nodes.push_back(parent);
    queue.push(static_cast<int>(nodes.size()) - 1);
  }

  return queue.top();
}

void assignCodes(const std::vector<Node>& nodes, int index,
                 const std::string& prefix,
                 std::array<std::string, kSymbolCount>& codes) {
  const Node& node = nodes[static_cast<std::size_t>(index)];
  if (node.isLeaf()) {
    // A single-symbol alphabet would otherwise get the empty code; the
    // encoder stores no bits in that case and the decoder repeats the symbol.
    codes[static_cast<std::size_t>(node.symbol)] = prefix;
    return;
  }
  assignCodes(nodes, node.left, prefix + "0", codes);
  assignCodes(nodes, node.right, prefix + "1", codes);
}

}  // namespace

bool Huffman::isCompressed(const std::string& input) {
  return input.size() >= sizeof(kMagic) &&
         input.compare(0, sizeof(kMagic), kMagic, sizeof(kMagic)) == 0;
}

std::string Huffman::compress(const std::string& input) {
  std::array<std::uint64_t, kSymbolCount> frequencies{};
  for (unsigned char byte : input) {
    ++frequencies[byte];
  }

  std::size_t symbolCount = 0;
  for (std::uint64_t frequency : frequencies) {
    if (frequency > 0) {
      ++symbolCount;
    }
  }

  std::string out;
  out.append(kMagic, sizeof(kMagic));
  appendU32(out, static_cast<std::uint32_t>(input.size()));
  appendU16(out, static_cast<std::uint16_t>(symbolCount));

  for (int symbol = 0; symbol < 256; ++symbol) {
    const std::uint64_t frequency =
        frequencies[static_cast<std::size_t>(symbol)];
    if (frequency == 0) {
      continue;
    }
    out.push_back(static_cast<char>(symbol));
    appendU64(out, frequency);
  }

  if (symbolCount == 0) {
    out.push_back(static_cast<char>(0));  // padding bits
    return out;
  }

  std::vector<Node> nodes;
  const int root = buildTree(frequencies, nodes);

  std::array<std::string, kSymbolCount> codes;
  assignCodes(nodes, root, "", codes);

  std::string packed;
  unsigned char current = 0;
  int filled = 0;
  for (unsigned char byte : input) {
    for (char bit : codes[byte]) {
      current = static_cast<unsigned char>((current << 1) | (bit == '1' ? 1 : 0));
      ++filled;
      if (filled == 8) {
        packed.push_back(static_cast<char>(current));
        current = 0;
        filled = 0;
      }
    }
  }

  int padding = 0;
  if (filled > 0) {
    padding = 8 - filled;
    current = static_cast<unsigned char>(current << padding);
    packed.push_back(static_cast<char>(current));
  }

  out.push_back(static_cast<char>(padding));
  out += packed;
  return out;
}

std::string Huffman::decompress(const std::string& input) {
  if (!isCompressed(input)) {
    throw MinigitError("corrupt compressed object: bad magic number");
  }

  const std::uint32_t originalSize = readU32(input, 4);
  const std::uint16_t symbolCount = readU16(input, 8);

  std::size_t offset = 10;
  std::array<std::uint64_t, kSymbolCount> frequencies{};
  for (std::uint16_t i = 0; i < symbolCount; ++i) {
    requireBytes(input, offset, 9, "symbol table");
    const auto symbol =
        static_cast<unsigned char>(input[offset]);
    frequencies[symbol] = readU64(input, offset + 1);
    offset += 9;
  }

  requireBytes(input, offset, 1, "padding byte");
  const int padding = static_cast<int>(static_cast<unsigned char>(input[offset]));
  ++offset;
  if (padding < 0 || padding > 7) {
    throw MinigitError("corrupt compressed object: invalid padding");
  }

  if (symbolCount == 0) {
    return {};
  }

  std::vector<Node> nodes;
  const int root = buildTree(frequencies, nodes);
  if (root < 0) {
    throw MinigitError("corrupt compressed object: empty tree");
  }

  if (symbolCount == 1) {
    int symbol = kInternalSymbol;
    for (std::size_t s = 0; s < kSymbolCount; ++s) {
      if (frequencies[s] > 0) {
        symbol = static_cast<int>(s);
        break;
      }
    }
    return std::string(originalSize, static_cast<char>(symbol));
  }

  std::string out;
  out.reserve(originalSize);

  std::size_t byteIndex = offset;
  int bitIndex = 0;
  int current = root;

  while (out.size() < originalSize) {
    if (byteIndex >= input.size()) {
      throw MinigitError("corrupt compressed object: bitstream ended early");
    }
    const auto byte =
        static_cast<unsigned char>(input[byteIndex]);
    const int bit = (byte >> (7 - bitIndex)) & 1;
    ++bitIndex;
    if (bitIndex == 8) {
      bitIndex = 0;
      ++byteIndex;
    }

    current = bit == 0 ? nodes[static_cast<std::size_t>(current)].left
                       : nodes[static_cast<std::size_t>(current)].right;
    if (current < 0) {
      throw MinigitError("corrupt compressed object: invalid bit path");
    }
    const Node& node = nodes[static_cast<std::size_t>(current)];
    if (node.isLeaf()) {
      out.push_back(static_cast<char>(node.symbol));
      current = root;
    }
  }

  return out;
}

}  // namespace minigit
