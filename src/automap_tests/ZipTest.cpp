#include "TestSupport.h"
#include "../AutoMapZip.h"

#include <string>

// Map data compresses well: long runs of the same explored bits
static std::string MapLikeBytes(void)
{
  std::string bytes("CSBM\x14\x00", 6);
  for (int i = 0; i < 20000; i++) bytes.push_back((char)((i % 97) < 80 ? 0xff : (i & 0x0f)));
  return bytes;
}

static void APackedFileUnpacksToTheSameBytes(void)
{
  std::string bytes = MapLikeBytes();
  std::string packed = AutoMapZip_Pack("csbgame.dat.map", bytes);
  CHECK(!packed.empty());
  CHECK(packed.compare(0, 2, "PK") == 0);
  CHECK(packed.size() < bytes.size() / 4);
  std::string unpacked;
  CHECK(AutoMapZip_Unpack(packed, &unpacked) == AutoMapZipUnpacked);
  CHECK(unpacked == bytes);
}

static void AnEmptyEntryRoundTrips(void)
{
  std::string packed = AutoMapZip_Pack("empty.map", std::string());
  CHECK(!packed.empty());
  std::string unpacked("left over");
  CHECK(AutoMapZip_Unpack(packed, &unpacked) == AutoMapZipUnpacked);
  CHECK(unpacked.empty());
}

static void AnOldBareFileIsTakenAsItIs(void)
{
  std::string bytes = MapLikeBytes();
  std::string unpacked;
  CHECK(AutoMapZip_Unpack(bytes, &unpacked) == AutoMapZipWasBare);
  CHECK(unpacked == bytes);
  CHECK(AutoMapZip_Unpack(std::string("CS"), &unpacked) == AutoMapZipWasBare);
  CHECK(unpacked == "CS");
}

static void ACutArchiveIsBroken(void)
{
  std::string packed = AutoMapZip_Pack("csbgame.dat.map", MapLikeBytes());
  std::string unpacked;
  CHECK(AutoMapZip_Unpack(packed.substr(0, packed.size() / 2), &unpacked) == AutoMapZipIsBroken);
  CHECK(AutoMapZip_Unpack(std::string("PK\x03\x04", 4), &unpacked) == AutoMapZipIsBroken);
}

static void ADamagedEntryIsBroken(void)
{
  std::string packed = AutoMapZip_Pack("csbgame.dat.map", MapLikeBytes());
  // Inside the compressed data, past the 30 byte local header and the name: the checksum fails
  packed[60] = (char)(packed[60] ^ 0x5a);
  std::string unpacked;
  CHECK(AutoMapZip_Unpack(packed, &unpacked) == AutoMapZipIsBroken);
}

void TestAutoMapZip(void)
{
  APackedFileUnpacksToTheSameBytes();
  AnEmptyEntryRoundTrips();
  AnOldBareFileIsTakenAsItIs();
  ACutArchiveIsBroken();
  ADamagedEntryIsBroken();
}
