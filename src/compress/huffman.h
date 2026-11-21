#pragma once

#include <string>

namespace minigit {

/// Huffman coding implemented from scratch, used to shrink object files.
///
/// A payload is encoded as
///
///     "MGH1" | u32 originalSize | u16 symbolCount
///            | symbolCount * (u8 symbol, u64 frequency)
///            | u8 paddingBits | bitstream
///
/// Storing symbol frequencies (not the tree) keeps the header compact and
/// makes decoding deterministic: both sides rebuild the same tree using the
/// same tie-breaking rule (smallest symbol first).
class Huffman {
 public:
  /// Compresses bytes.  An empty input round-trips as a header-only payload.
  static std::string compress(const std::string& input);

  /// Decompresses bytes produced by compress().  Throws MinigitError when the
  /// stream is truncated or corrupt.
  static std::string decompress(const std::string& input);

  /// True when the payload starts with the Huffman magic number.  Objects
  /// written before compression existed stay raw, so readers check this to
  /// remain backwards compatible.
  static bool isCompressed(const std::string& input);
};

}  // namespace minigit
