#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace minigit {

/// A SHA-1 object identifier: 20 raw bytes rendered as 40 hex characters.
///
/// Wrapping the raw bytes (instead of passing std::string around) makes it
/// impossible to mix an object id with arbitrary file content, and gives us
/// one place to validate hex input.
class Oid {
 public:
  static constexpr std::size_t kRawSize = 20;
  static constexpr std::size_t kHexSize = kRawSize * 2;

  Oid() = default;
  explicit Oid(const std::array<std::uint8_t, kRawSize>& bytes)
      : bytes_(bytes) {}

  /// Parses exactly 40 hex characters.  Throws MinigitError otherwise.
  static Oid fromHex(const std::string& hex);

  /// Non-throwing parse used by command line parsing.
  static std::optional<Oid> tryParse(const std::string& hex);

  /// Hashes arbitrary bytes with our own SHA-1 implementation.
  static Oid hashBytes(const std::string& data);

  const std::array<std::uint8_t, kRawSize>& bytes() const { return bytes_; }
  std::string hex() const;
  std::string shortHex(std::size_t length = 7) const;

  bool isNull() const;

  bool operator==(const Oid& other) const { return bytes_ == other.bytes_; }
  bool operator!=(const Oid& other) const { return !(*this == other); }
  bool operator<(const Oid& other) const { return bytes_ < other.bytes_; }

 private:
  std::array<std::uint8_t, kRawSize> bytes_{};
};

struct OidHash {
  std::size_t operator()(const Oid& oid) const {
    std::size_t seed = 1469598103934665603ull;
    for (std::uint8_t byte : oid.bytes()) {
      seed ^= static_cast<std::size_t>(byte);
      seed *= 1099511628211ull;
    }
    return seed;
  }
};

}  // namespace minigit
