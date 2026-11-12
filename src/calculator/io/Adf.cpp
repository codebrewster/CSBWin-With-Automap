#include "Adf.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace calc {

namespace {

const size_t kBlock = 512;
const int kRootBlock = 880;
const int32_t kShort = 2, kData = 8, kList = 16, kRoot = 1, kFile = -3;
const size_t kOfsData = 488;   // data bytes of an OFS data block (24-byte header)

class Image {
 public:
  explicit Image(const Bytes &data) : d_(data)
  {
    if (data.size() < (kRootBlock + 1) * kBlock || data.size() % kBlock)
      throw FormatError("not a full disk image");
    if (data[0] != 'D' || data[1] != 'O' || data[2] != 'S')
      throw FormatError("no AmigaDOS boot block");
    blocks_ = int(data.size() / kBlock);
    ffs_ = (data[3] & 2) != 0;   // DOS\2, \3 = FFS; DOS\0, \1 = OFS
    Reader root = block(kRootBlock);
    if (root.i32(0) != kShort || root.i32(508) != kRoot || !checksumOk(root))
      throw FormatError("the disk's root block is damaged");
  }

  Reader block(int n) const
  {
    if (n < 0 || n >= blocks_) throw FormatError("disk image: block out of range");
    return Reader(d_.data() + size_t(n) * kBlock, kBlock, false);
  }
  static bool checksumOk(const Reader &b)   // all 128 longwords sum to 0
  {
    uint32_t sum = 0;
    for (size_t i = 0; i < 128; i++) sum += b.u32(i * 4);
    return sum == 0;
  }
  bool isHeader(int n) const
  {
    Reader b = block(n);
    return b.i32(0) == kShort && b.i32(508) == kFile && checksumOk(b);
  }
  std::vector<AdfFile> headers() const
  {
    std::vector<AdfFile> out;
    for (int n = 0; n < blocks_; n++) {
      if (n == kRootBlock || !isHeader(n)) continue;
      Reader b = block(n);
      int len = b.u8(432);
      if (len < 1 || len > 30) continue;
      AdfFile f;
      f.name.assign(reinterpret_cast<const char *>(b.data()) + 433, size_t(len));
      f.size = b.u32(81 * 4);
      f.headerBlock = n;
      out.push_back(f);
    }
    return out;
  }
  Bytes read(int headerNo) const
  {
    Reader head = block(headerNo);
    size_t size = head.u32(81 * 4);
    Bytes data;
    if (ffs_) {
      Reader b = head;
      for (;;) {   // block numbers from the header and its extension tables
        for (size_t i = 6; i < 78; i++) {
          uint32_t v = b.u32(i * 4);
          if (v == 0) continue;
          Reader blk = block(int(v));
          data.insert(data.end(), blk.data(), blk.data() + kBlock);
        }
        uint32_t next = b.u32(126 * 4);
        if (next == 0) break;
        b = block(int(next));
        if (b.i32(0) != kList || !checksumOk(b))
          throw FormatError("disk image: damaged extension block");
      }
    } else {
      std::map<uint32_t, Bytes> parts;   // an OFS file is a chain of data blocks
      std::set<uint32_t> seen;
      for (uint32_t n = head.u32(4 * 4); n != 0;) {
        if (!seen.insert(n).second) throw FormatError("disk image: the file's blocks loop");
        Reader blk = block(int(n));
        if (blk.i32(0) != kData || !checksumOk(blk))
          throw FormatError("disk image: damaged data block");
        size_t len = blk.u32(3 * 4);
        if (len > kOfsData) throw FormatError("disk image: oversize data block");
        parts[blk.u32(2 * 4)] = Bytes(blk.data() + 24, blk.data() + 24 + len);
        n = blk.u32(4 * 4);
      }
      uint32_t want = 1;
      for (auto &kv : parts) {
        if (kv.first != want++) throw FormatError("disk image: the file is incomplete");
        data.insert(data.end(), kv.second.begin(), kv.second.end());
      }
    }
    if (data.size() < size) throw FormatError("disk image: the file is shorter than it says");
    data.resize(size);
    return data;
  }

 private:
  const Bytes &d_;
  int blocks_ = 0;
  bool ffs_ = false;
};

std::string lower(std::string s)
{
  for (char &c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool endsWith(const std::string &s, const char *tail)
{
  std::string t(tail);
  return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

}  // namespace

bool looksLikeAdf(const Bytes &image)
{
  return image.size() == 901120 && image[0] == 'D' && image[1] == 'O' && image[2] == 'S';
}

std::vector<AdfFile> adfFiles(const Bytes &image) { return Image(image).headers(); }

Bytes adfRead(const Bytes &image, const AdfFile &file) { return Image(image).read(file.headerBlock); }

int adfPickSavegame(const std::vector<AdfFile> &files)
{
  for (size_t i = 0; i < files.size(); i++)
    if (lower(files[i].name) == "dmgame.dat") return int(i);
  for (const char *ext : {".dat", ".bak"}) {
    int best = -1;
    for (size_t i = 0; i < files.size(); i++)
      if (endsWith(lower(files[i].name), ext) &&
          (best < 0 || files[i].name < files[size_t(best)].name))
        best = int(i);
    if (best >= 0) return best;
  }
  return -1;
}

}  // namespace calc
