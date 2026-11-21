#include "objects/oid.h"

#include "core/error.h"
#include "hash/sha1.h"

namespace minigit {
namespace {

int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

}  // namespace

Oid Oid::fromHex(const std::string& hex) {
  if (hex.size() != kHexSize) {
    throw MinigitError("invalid object id '" + hex + "': expected " +
                       std::to_string(kHexSize) + " hex characters");
  }

  std::array<std::uint8_t, kRawSize> raw{};
  for (std::size_t i = 0; i < kRawSize; ++i) {
    const int high = hexValue(hex[i * 2]);
    const int low = hexValue(hex[i * 2 + 1]);
    if (high < 0 || low < 0) {
      throw MinigitError("invalid object id '" + hex +
                         "': expected hexadecimal characters only");
    }
    raw[i] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return Oid(raw);
}

std::optional<Oid> Oid::tryParse(const std::string& hex) {
  if (hex.size() != kHexSize) {
    return std::nullopt;
  }
  for (char c : hex) {
    if (hexValue(c) < 0) {
      return std::nullopt;
    }
  }
  return fromHex(hex);
}

Oid Oid::hashBytes(const std::string& data) {
  return fromHex(Sha1::hash(data));
}

std::string Oid::hex() const {
  static constexpr char kHexDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(kHexSize);
  for (std::uint8_t byte : bytes_) {
    out.push_back(kHexDigits[byte >> 4]);
    out.push_back(kHexDigits[byte & 0x0F]);
  }
  return out;
}

std::string Oid::shortHex(std::size_t length) const {
  const std::string full = hex();
  return length >= full.size() ? full : full.substr(0, length);
}

bool Oid::isNull() const {
  for (std::uint8_t byte : bytes_) {
    if (byte != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace minigit
