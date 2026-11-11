#include "TestSupport.h"
#include "../AutoMapZip.h"

#include <string>
#include <vector>

// Map data compresses well: long runs of the same explored bits
static std::string MapLikeBytes(void)
{
  std::string bytes("CSBM\x1a\x00", 6);
  for (int i = 0; i < 20000; i++) bytes.push_back((char)((i % 97) < 80 ? 0xff : (i & 0x0f)));
  return bytes;
}

static std::string StatsLikeBytes(void)
{
  std::string bytes("CSBS\x19\x00", 6);
  for (int i = 0; i < 3000; i++) bytes.push_back((char)(i % 7 == 0 ? i & 0xff : 0));
  return bytes;
}

static std::vector<AutoMapZipEntry> Entries(const std::string &mapBytes, const std::string &statsBytes)
{
  std::vector<AutoMapZipEntry> entries(2);
  entries[0].name = "csbgame.dat.map";
  entries[0].bytes = mapBytes;
  entries[1].name = "statistics";
  entries[1].bytes = statsBytes;
  return entries;
}

static std::vector<AutoMapZipEntry> OneEntry(const std::string &name, const std::string &bytes)
{
  std::vector<AutoMapZipEntry> entries(1);
  entries[0].name = name;
  entries[0].bytes = bytes;
  return entries;
}

static void APackedFileUnpacksToTheSameEntries(void)
{
  std::string packed = AutoMapZip_Pack(Entries(MapLikeBytes(), StatsLikeBytes()));
  CHECK(!packed.empty());
  CHECK(packed.compare(0, 2, "PK") == 0);
  CHECK(packed.size() < MapLikeBytes().size() / 4);
  std::vector<AutoMapZipEntry> unpacked;
  CHECK(AutoMapZip_Unpack(packed, &unpacked) == AutoMapZipUnpacked);
  CHECK(unpacked.size() == 2);
  CHECK(unpacked[0].name == "csbgame.dat.map" && unpacked[0].isReadable && unpacked[0].bytes == MapLikeBytes());
  CHECK(unpacked[1].name == "statistics" && unpacked[1].isReadable && unpacked[1].bytes == StatsLikeBytes());
  const AutoMapZipEntry *stats = AutoMapZip_Find(unpacked, "statistics");
  CHECK(stats == &unpacked[1]);
  CHECK(AutoMapZip_Find(unpacked, "nothing") == NULL);
}

// A file of the earlier zipped format: the map alone
static void AOneEntryArchiveUnpacks(void)
{
  std::string packed = AutoMapZip_Pack(OneEntry("csbgame.dat.map", MapLikeBytes()));
  std::vector<AutoMapZipEntry> unpacked;
  CHECK(AutoMapZip_Unpack(packed, &unpacked) == AutoMapZipUnpacked);
  CHECK(unpacked.size() == 1 && unpacked[0].bytes == MapLikeBytes());
  CHECK(AutoMapZip_Find(unpacked, "statistics") == NULL);
}

static void AnEmptyEntryRoundTrips(void)
{
  std::string packed = AutoMapZip_Pack(OneEntry("empty.map", std::string()));
  CHECK(!packed.empty());
  std::vector<AutoMapZipEntry> unpacked;
  CHECK(AutoMapZip_Unpack(packed, &unpacked) == AutoMapZipUnpacked);
  CHECK(unpacked.size() == 1 && unpacked[0].isReadable && unpacked[0].bytes.empty());
}

static void AnOldBareFileIsTakenAsItIs(void)
{
  std::string bytes = MapLikeBytes();
  std::vector<AutoMapZipEntry> unpacked;
  CHECK(AutoMapZip_Unpack(bytes, &unpacked) == AutoMapZipWasBare);
  CHECK(unpacked.size() == 1 && unpacked[0].isReadable && unpacked[0].bytes == bytes);
  CHECK(AutoMapZip_Unpack(std::string("CS"), &unpacked) == AutoMapZipWasBare);
  CHECK(unpacked.size() == 1 && unpacked[0].bytes == "CS");
}

static void ACutArchiveIsBroken(void)
{
  std::string packed = AutoMapZip_Pack(Entries(MapLikeBytes(), StatsLikeBytes()));
  std::vector<AutoMapZipEntry> unpacked;
  CHECK(AutoMapZip_Unpack(packed.substr(0, packed.size() / 2), &unpacked) == AutoMapZipIsBroken);
  CHECK(unpacked.empty());
  CHECK(AutoMapZip_Unpack(std::string("PK\x03\x04", 4), &unpacked) == AutoMapZipIsBroken);
}

// Inside the first entry's compressed data, past the 30 byte local header and the name: its checksum fails
static void ADamagedMapEntryIsBroken(void)
{
  std::string packed = AutoMapZip_Pack(Entries(MapLikeBytes(), StatsLikeBytes()));
  packed[60] = (char)(packed[60] ^ 0x5a);
  std::vector<AutoMapZipEntry> unpacked;
  CHECK(AutoMapZip_Unpack(packed, &unpacked) == AutoMapZipIsBroken);
}

// A damaged statistics entry costs only itself
static void ADamagedSecondEntryLeavesTheFirst(void)
{
  std::string mapOnly = AutoMapZip_Pack(OneEntry("csbgame.dat.map", MapLikeBytes()));
  std::string packed = AutoMapZip_Pack(Entries(MapLikeBytes(), StatsLikeBytes()));
  // The second entry's local header starts where the one-entry archive's central directory did:
  // find it by its signature after the first entry, and spoil a byte of its compressed data
  size_t second = packed.find(std::string("PK\x03\x04", 4), 4);
  CHECK(second != std::string::npos && second < mapOnly.size());
  size_t inData = second + 30 + std::string("statistics").size() + 20;
  packed[inData] = (char)(packed[inData] ^ 0x5a);
  std::vector<AutoMapZipEntry> unpacked;
  CHECK(AutoMapZip_Unpack(packed, &unpacked) == AutoMapZipUnpacked);
  CHECK(unpacked.size() == 2);
  CHECK(unpacked[0].isReadable && unpacked[0].bytes == MapLikeBytes());
  CHECK(unpacked[1].name == "statistics" && !unpacked[1].isReadable && unpacked[1].bytes.empty());
}

void TestAutoMapZip(void)
{
  APackedFileUnpacksToTheSameEntries();
  AOneEntryArchiveUnpacks();
  AnEmptyEntryRoundTrips();
  AnOldBareFileIsTakenAsItIs();
  ACutArchiveIsBroken();
  ADamagedMapEntryIsBroken();
  ADamagedSecondEntryLeavesTheFirst();
}
