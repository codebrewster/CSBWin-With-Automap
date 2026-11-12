// Byte-order helpers for the game's files: classic saves and graphics.dat are
// big-endian (Atari ST, Amiga), CSBwin saves little-endian.
#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace calc {

using Bytes = std::vector<uint8_t>;

struct FormatError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// A bounds-checked view with a fixed byte order.
class Reader {
 public:
  Reader(const uint8_t *data, size_t size, bool littleEndian)
      : d_(data), n_(size), le_(littleEndian) {}
  Reader(const Bytes &b, bool littleEndian) : Reader(b.data(), b.size(), littleEndian) {}

  size_t size() const { return n_; }
  bool littleEndian() const { return le_; }
  uint8_t u8(size_t o) const { need(o, 1); return d_[o]; }
  uint16_t u16(size_t o) const {
    need(o, 2);
    return le_ ? uint16_t(d_[o] | (d_[o + 1] << 8)) : uint16_t((d_[o] << 8) | d_[o + 1]);
  }
  int16_t i16(size_t o) const { return int16_t(u16(o)); }
  uint32_t u32(size_t o) const {
    need(o, 4);
    return le_ ? uint32_t(u16(o)) | (uint32_t(u16(o + 2)) << 16)
               : (uint32_t(u16(o)) << 16) | uint32_t(u16(o + 2));
  }
  int32_t i32(size_t o) const { return int32_t(u32(o)); }
  Reader sub(size_t o, size_t len) const { need(o, len); return Reader(d_ + o, len, le_); }
  Reader from(size_t o) const { need(o, 0); return Reader(d_ + o, n_ - o, le_); }
  const uint8_t *data() const { return d_; }

 private:
  void need(size_t o, size_t len) const {
    if (o > n_ || len > n_ - o) throw FormatError("file is shorter than its contents claim");
  }
  const uint8_t *d_;
  size_t n_;
  bool le_;
};

Bytes readFile(const std::string &path);   // throws FormatError

}  // namespace calc
