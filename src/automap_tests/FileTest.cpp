#include "TestSupport.h"
#include "../AutoMapCellMemory.h"
#include "../AutoMapFile.h"
#include "../AutoMapItemMemory.h"
#include "../AutoMapKnowledge.h"
#include "../AutoMapMonsterDamage.h"
#include "../AutoMapPlayerNotes.h"

#include <cstddef>
#include <sstream>
#include <string>

static const AutoMapSaveSignature SAVE_SIGNATURE = {0x12345678u, 0x9abcdef0u};

// Bytes 0-3 marker, 4-5 version, 6-13 signature, 14-15 level count, 16-17 size of level 0
static const size_t VERSION_OFFSET = 4;
static const size_t SIGNATURE_OFFSET = 6;
static const size_t LEVEL_COUNT_OFFSET = 14;
static const size_t FIRST_LEVEL_WIDTH_OFFSET = 16;
// A file with no damaged monster group ends in its count of 4 bytes
static const size_t NO_DAMAGE_BYTES = 4;

static AutoMapExplored EmptyDungeon(void)
{
  std::vector<AutoMapLevelSize> sizes;
  sizes.push_back(AutoMapLevelSize{5, 3});
  sizes.push_back(AutoMapLevelSize{32, 32});
  AutoMapExplored explored;
  explored.Reset(sizes);
  return explored;
}

static AutoMapExplored ExploredDungeon(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.Mark(AutoMapCell{0, 0, 0});
  explored.Mark(AutoMapCell{0, 4, 2});
  explored.Mark(AutoMapCell{1, 31, 31});
  explored.Mark(AutoMapCell{1, 7, 9});
  return explored;
}

static std::string WrittenFile(void)
{
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, ExploredDungeon(), AutoMapKnowledge(), AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  return out.str();
}

static AutoMapFileResult ReadInto(const std::string &bytes, AutoMapExplored *explored,
                                  AutoMapKnowledge *knowledge, AutoMapItemMemory *items,
                                  bool *hadItemMemory, AutoMapCellMemory *cells,
                                  bool *hadCellMemory, AutoMapPlayerNotes *notes,
                                  AutoMapMonsterDamage *damage)
{
  std::istringstream in(bytes);
  return AutoMapFile(SAVE_SIGNATURE).Read(in, explored, knowledge, items, hadItemMemory, cells,
                                          hadCellMemory, notes, damage);
}

static AutoMapFileResult ReadInto(const std::string &bytes, AutoMapExplored *explored,
                                  AutoMapKnowledge *knowledge, AutoMapItemMemory *items,
                                  bool *hadItemMemory, AutoMapCellMemory *cells,
                                  bool *hadCellMemory, AutoMapPlayerNotes *notes)
{
  AutoMapMonsterDamage damage;
  return ReadInto(bytes, explored, knowledge, items, hadItemMemory, cells, hadCellMemory, notes,
                  &damage);
}

static AutoMapFileResult ReadInto(const std::string &bytes, AutoMapExplored *explored,
                                  AutoMapKnowledge *knowledge, AutoMapItemMemory *items,
                                  bool *hadItemMemory, AutoMapCellMemory *cells,
                                  bool *hadCellMemory)
{
  AutoMapPlayerNotes notes;
  return ReadInto(bytes, explored, knowledge, items, hadItemMemory, cells, hadCellMemory, &notes);
}

static AutoMapFileResult ReadInto(const std::string &bytes, AutoMapExplored *explored,
                                  AutoMapKnowledge *knowledge, AutoMapItemMemory *items,
                                  bool *hadItemMemory)
{
  AutoMapCellMemory cells;
  bool hadCellMemory;
  return ReadInto(bytes, explored, knowledge, items, hadItemMemory, &cells, &hadCellMemory);
}

static AutoMapFileResult ReadInto(const std::string &bytes, AutoMapExplored *explored,
                                  AutoMapKnowledge *knowledge)
{
  AutoMapItemMemory items;
  bool hadItemMemory;
  return ReadInto(bytes, explored, knowledge, &items, &hadItemMemory);
}

static AutoMapFileResult ReadInto(const std::string &bytes, AutoMapExplored *explored)
{
  AutoMapKnowledge knowledge;
  return ReadInto(bytes, explored, &knowledge);
}

static bool NothingIsExplored(const AutoMapExplored &explored)
{
  return !explored.IsExplored(AutoMapCell{0, 0, 0})
      && !explored.IsExplored(AutoMapCell{0, 4, 2})
      && !explored.IsExplored(AutoMapCell{1, 31, 31})
      && !explored.IsExplored(AutoMapCell{1, 7, 9});
}

static void CheckRejected(const std::string &bytes, AutoMapFileResult expectedResult)
{
  AutoMapExplored explored = EmptyDungeon();
  CHECK(ReadInto(bytes, &explored) == expectedResult);
  CHECK(NothingIsExplored(explored));
}

// A file of EmptyDungeon's shape as version 10 wrote it: the rims cut out of every level
static std::string WithoutRims(std::string bytes)
{
  AutoMapExplored dungeon = EmptyDungeon();
  size_t at = FIRST_LEVEL_WIDTH_OFFSET;
  for (int level = 0; level < dungeon.LevelCount(); level++)
  {
    AutoMapLevelSize size = dungeon.SizeOfLevel(level);
    at += 2 + AutoMapExplored::ByteCountFor(size) + 4 * AutoMapExplored::FaceByteCountFor(size);
    size_t rim = 0;
    for (int set = 0; set < AUTOMAP_BIT_SET_COUNT; set++)
    {
      rim += AutoMapExplored::RimByteCountFor(size, (AutoMapBitSet)set);
    }
    bytes.erase(at, rim);
  }
  bytes[VERSION_OFFSET] = 10;
  return bytes;
}

// The rim around each level is kept like the level's own cells: explored, faces, glimpses
static void RimsRoundTrip(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.Mark(AutoMapCell{0, -1, 1});
  explored.MarkFace(AutoMapCell{0, -1, 1}, 1);
  explored.MarkGlimpsedFace(AutoMapCell{1, 32, 0}, 3);
  explored.Mark(AutoMapCell{1, 5, -1});
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, explored, AutoMapKnowledge(), AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  AutoMapExplored readExplored = EmptyDungeon();
  CHECK(ReadInto(out.str(), &readExplored) == AutoMapFileRead);
  CHECK(readExplored.IsExplored(AutoMapCell{0, -1, 1}));
  CHECK(readExplored.FaceIsSeen(AutoMapCell{0, -1, 1}, 1));
  CHECK(readExplored.FaceIsGlimpsed(AutoMapCell{1, 32, 0}, 3));
  CHECK(readExplored.IsExplored(AutoMapCell{1, 5, -1}));
  CHECK(!readExplored.IsExplored(AutoMapCell{0, -1, 0}));
  CHECK(!readExplored.IsExplored(AutoMapCell{0, 0, 0}));
}

// Version 10 kept a painted rim cell on the floor beside it, as that floor's face towards it
static void OlderFilesTakeTheRimFromTheFloorBesideIt(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.Mark(AutoMapCell{0, 0, 1});
  explored.MarkFace(AutoMapCell{0, 0, 1}, 3);
  explored.MarkFace(AutoMapCell{1, 4, 4}, 2);
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, explored, AutoMapKnowledge(), AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  AutoMapExplored readExplored = EmptyDungeon();
  CHECK(ReadInto(WithoutRims(out.str()), &readExplored) == AutoMapFileRead);
  CHECK(readExplored.IsExplored(AutoMapCell{0, -1, 1}));
  CHECK(!readExplored.FaceIsSeen(AutoMapCell{0, 0, 1}, 3));
  // A face inside the level stays
  CHECK(readExplored.FaceIsSeen(AutoMapCell{1, 4, 4}, 2));
}

static void RoundTripReturnsTheSameCells(void)
{
  AutoMapExplored explored = EmptyDungeon();
  CHECK(ReadInto(WrittenFile(), &explored) == AutoMapFileRead);
  CHECK(explored.IsExplored(AutoMapCell{0, 0, 0}));
  CHECK(explored.IsExplored(AutoMapCell{0, 4, 2}));
  CHECK(explored.IsExplored(AutoMapCell{1, 31, 31}));
  CHECK(explored.IsExplored(AutoMapCell{1, 7, 9}));
  CHECK(!explored.IsExplored(AutoMapCell{0, 1, 0}));
  CHECK(!explored.IsExplored(AutoMapCell{1, 0, 0}));
}

static void EmptyFileIsRejected(void)
{
  CheckRejected("", AutoMapFileIsNotAMapFile);
}

static void FileOfAnotherKindIsRejected(void)
{
  std::string bytes = WrittenFile();
  bytes[0] = 'X';
  CheckRejected(bytes, AutoMapFileIsNotAMapFile);
}

static void UnknownVersionIsRejected(void)
{
  std::string bytes = WrittenFile();
  bytes[VERSION_OFFSET] = 12;
  CheckRejected(bytes, AutoMapFileHasUnknownVersion);
}

static void FileOfAnotherSaveIsRejected(void)
{
  std::string bytes = WrittenFile();
  bytes[SIGNATURE_OFFSET] ^= 0x01;
  CheckRejected(bytes, AutoMapFileBelongsToAnotherSave);
}

static void OtherLevelCountIsRejected(void)
{
  std::string bytes = WrittenFile();
  bytes[LEVEL_COUNT_OFFSET] = 3;
  CheckRejected(bytes, AutoMapFileHasOtherLevels);
}

// A damaged header must not make the reader allocate cells for levels the dungeon does not have
static void AbsurdLevelCountIsRejected(void)
{
  std::string bytes = WrittenFile();
  bytes[LEVEL_COUNT_OFFSET] = (char)0xff;
  bytes[LEVEL_COUNT_OFFSET + 1] = (char)0xff;
  CheckRejected(bytes, AutoMapFileHasOtherLevels);
}

static void OtherLevelSizeIsRejected(void)
{
  std::string bytes = WrittenFile();
  bytes[FIRST_LEVEL_WIDTH_OFFSET] = 6;
  CheckRejected(bytes, AutoMapFileHasOtherLevels);
}

static void FileCutShortAnywhereIsRejected(void)
{
  std::string bytes = WrittenFile();
  for (size_t length = sizeof("CSBM") - 1; length < bytes.size(); length++)
  {
    CheckRejected(bytes.substr(0, length), AutoMapFileIsTruncated);
  }
}

// The second level fails after the first was read; the cells of the first must not be kept
static void DamageInALaterLevelLeavesTheMapEmpty(void)
{
  std::string bytes = WrittenFile();
  // size, explored bits, seen-face bits, glimpsed-face bits, read-text bits, looked-into bits,
  // then the rim: 20 cells, 3 explored bytes and 10 for each set of faces
  size_t secondLevelWidthOffset = FIRST_LEVEL_WIDTH_OFFSET + 2 + 2 + 8 + 8 + 8 + 8 + 3 + 4 * 10;
  bytes[secondLevelWidthOffset] = 31;
  CheckRejected(bytes, AutoMapFileHasOtherLevels);
}

// Hand-built streams, byte by byte. Level 0 is 5x3 (2 explored bytes, 2 face bytes), level 1 is
// 32x32 (128 explored bytes, 512 face bytes).
static void AppendNumber(std::string *bytes, uint32_t value, int byteCount)
{
  for (int i = 0; i < byteCount; i++) bytes->push_back((char)((value >> (8 * i)) & 0xff));
}

static std::string HeaderOfVersion(int version)
{
  std::string bytes = "CSBM";
  AppendNumber(&bytes, (uint32_t)version, 2);
  AppendNumber(&bytes, SAVE_SIGNATURE.first, 4);
  AppendNumber(&bytes, SAVE_SIGNATURE.second, 4);
  AppendNumber(&bytes, 2, 2);
  return bytes;
}

static std::string OldFileExploring(const AutoMapCell &cell)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.Mark(cell);
  std::string bytes = HeaderOfVersion(1);
  for (int level = 0; level < 2; level++)
  {
    AutoMapLevelSize size = explored.SizeOfLevel(level);
    AppendNumber(&bytes, (uint32_t)size.width, 1);
    AppendNumber(&bytes, (uint32_t)size.height, 1);
    const std::vector<uint8_t> &bits = explored.BitsOfLevel(level);
    bytes.append((const char *)bits.data(), bits.size());
  }
  return bytes;
}

// A version-2 file with no seen faces and the given facts, each already as 8 bytes
static std::string NewFileWithFacts(uint32_t declaredCount, const std::string &facts)
{
  std::string bytes = HeaderOfVersion(2);
  AutoMapExplored explored = EmptyDungeon();
  for (int level = 0; level < 2; level++)
  {
    AutoMapLevelSize size = explored.SizeOfLevel(level);
    AppendNumber(&bytes, (uint32_t)size.width, 1);
    AppendNumber(&bytes, (uint32_t)size.height, 1);
    bytes.append(AutoMapExplored::ByteCountFor(size) + AutoMapExplored::FaceByteCountFor(size), '\0');
  }
  AppendNumber(&bytes, declaredCount, 4);
  return bytes + facts;
}

static std::string FactBytes(int kind, uint32_t record, int level, int x, int y)
{
  std::string bytes;
  AppendNumber(&bytes, (uint32_t)kind, 1);
  AppendNumber(&bytes, record, 4);
  AppendNumber(&bytes, (uint32_t)level, 1);
  AppendNumber(&bytes, (uint32_t)x, 1);
  AppendNumber(&bytes, (uint32_t)y, 1);
  return bytes;
}

static void GlimpsedFacesRoundTrip(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.MarkGlimpsedFace(AutoMapCell{1, 3, 4}, 2);
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, explored, AutoMapKnowledge(), AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  AutoMapExplored readExplored = EmptyDungeon();
  AutoMapKnowledge readKnowledge;
  CHECK(ReadInto(out.str(), &readExplored, &readKnowledge) == AutoMapFileRead);
  CHECK(readExplored.FaceIsGlimpsed(AutoMapCell{1, 3, 4}, 2));
  CHECK(!readExplored.FaceIsGlimpsed(AutoMapCell{1, 3, 4}, 1));
  CHECK(!readExplored.FaceIsSeen(AutoMapCell{1, 3, 4}, 2));
}

static void ReadTextsRoundTrip(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.MarkFace(AutoMapCell{1, 3, 4}, 2);
  explored.MarkTextRead(AutoMapCell{1, 3, 4}, 2);
  explored.MarkFace(AutoMapCell{1, 5, 4}, 2);
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, explored, AutoMapKnowledge(), AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  AutoMapExplored readExplored = EmptyDungeon();
  AutoMapKnowledge readKnowledge;
  CHECK(ReadInto(out.str(), &readExplored, &readKnowledge) == AutoMapFileRead);
  CHECK(readExplored.TextIsRead(AutoMapCell{1, 3, 4}, 2));
  CHECK(!readExplored.TextIsRead(AutoMapCell{1, 5, 4}, 2));
}

// Before version 6 a seen face showed its text: what was seen stays readable
static void OlderFilesHaveTheirSeenTextsRead(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.MarkFace(AutoMapCell{1, 3, 4}, 2);
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, explored, AutoMapKnowledge(), AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  std::string bytes = out.str();
  bytes = WithoutRims(bytes);
  bytes[VERSION_OFFSET] = 5;
  // Version 5 has no read-text and looked-into bits: cut them out after the glimpsed faces of
  // each level
  size_t at = FIRST_LEVEL_WIDTH_OFFSET;
  for (int level = 0; level < 2; level++)
  {
    AutoMapLevelSize size = explored.SizeOfLevel(level);
    size_t faces = AutoMapExplored::FaceByteCountFor(size);
    at += 2 + AutoMapExplored::ByteCountFor(size) + 2 * faces;
    bytes.erase(at, 2 * faces);
  }
  AutoMapExplored readExplored = EmptyDungeon();
  AutoMapKnowledge readKnowledge;
  CHECK(ReadInto(bytes, &readExplored, &readKnowledge) == AutoMapFileRead);
  CHECK(readExplored.FaceIsSeen(AutoMapCell{1, 3, 4}, 2));
  CHECK(readExplored.TextIsRead(AutoMapCell{1, 3, 4}, 2));
  CHECK(!readExplored.TextIsRead(AutoMapCell{1, 3, 4}, 1));
}

static void LookedIntoAlcovesRoundTrip(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.MarkFace(AutoMapCell{1, 3, 4}, 2);
  explored.MarkAlcoveLookedInto(AutoMapCell{1, 3, 4}, 2);
  explored.MarkFace(AutoMapCell{1, 5, 4}, 2);
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, explored, AutoMapKnowledge(), AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  AutoMapExplored readExplored = EmptyDungeon();
  AutoMapKnowledge readKnowledge;
  CHECK(ReadInto(out.str(), &readExplored, &readKnowledge) == AutoMapFileRead);
  CHECK(readExplored.AlcoveIsLookedInto(AutoMapCell{1, 3, 4}, 2));
  CHECK(!readExplored.AlcoveIsLookedInto(AutoMapCell{1, 5, 4}, 2));
}

// Before version 7 a seen face showed what lies in its alcove: what was seen stays known
static void OlderFilesHaveTheirSeenAlcovesLookedInto(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.MarkFace(AutoMapCell{1, 3, 4}, 2);
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, explored, AutoMapKnowledge(), AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  std::string bytes = out.str();
  bytes = WithoutRims(bytes);
  bytes[VERSION_OFFSET] = 6;
  // Version 6 has no looked-into bits: cut them out after the read texts of each level
  size_t at = FIRST_LEVEL_WIDTH_OFFSET;
  for (int level = 0; level < 2; level++)
  {
    AutoMapLevelSize size = explored.SizeOfLevel(level);
    size_t faces = AutoMapExplored::FaceByteCountFor(size);
    at += 2 + AutoMapExplored::ByteCountFor(size) + 3 * faces;
    bytes.erase(at, faces);
  }
  AutoMapExplored readExplored = EmptyDungeon();
  AutoMapKnowledge readKnowledge;
  CHECK(ReadInto(bytes, &readExplored, &readKnowledge) == AutoMapFileRead);
  CHECK(readExplored.AlcoveIsLookedInto(AutoMapCell{1, 3, 4}, 2));
  CHECK(!readExplored.AlcoveIsLookedInto(AutoMapCell{1, 3, 4}, 1));
}

static void FacesAndFactsRoundTrip(void)
{
  AutoMapExplored explored = EmptyDungeon();
  explored.MarkFace(AutoMapCell{1, 3, 4}, 2);
  AutoMapKnowledge knowledge;
  AutoMapFact actuator = AutoMapFact_OfRecord(AutoMapFactActuator, 9);
  AutoMapFact pit = AutoMapFact_OfCell(AutoMapFactPit, AutoMapCell{0, 1, 1});
  AutoMapFact link = AutoMapFact_OfLink(9, AutoMapCell{1, 2, 2});
  knowledge.Learn(actuator);
  knowledge.Learn(pit);
  knowledge.Learn(link);
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, explored, knowledge, AutoMapItemMemory(), AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());

  AutoMapExplored readExplored = EmptyDungeon();
  AutoMapKnowledge readKnowledge;
  CHECK(ReadInto(out.str(), &readExplored, &readKnowledge) == AutoMapFileRead);
  CHECK(readExplored.FaceIsSeen(AutoMapCell{1, 3, 4}, 2));
  CHECK(!readExplored.FaceIsSeen(AutoMapCell{1, 3, 4}, 1));
  CHECK(readKnowledge.Knows(actuator));
  CHECK(readKnowledge.Knows(pit));
  CHECK(readKnowledge.Knows(link));
  CHECK(readKnowledge.Facts().size() == 3);
}

static void OldFileFillsFacesTowardsExploredCells(void)
{
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactScrollRead, 1));
  CHECK(ReadInto(OldFileExploring(AutoMapCell{0, 2, 1}), &explored, &knowledge) == AutoMapFileRead);
  CHECK(explored.IsExplored(AutoMapCell{0, 2, 1}));
  CHECK(explored.FaceIsSeen(AutoMapCell{0, 2, 0}, 2));
  CHECK(knowledge.Facts().empty());
}

static void CheckFileLeavesStoresUnchanged(const std::string &bytes, AutoMapFileResult expectedResult)
{
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapFact known = AutoMapFact_OfRecord(AutoMapFactChestOpened, 5);
  knowledge.Learn(known);
  CHECK(ReadInto(bytes, &explored, &knowledge) == expectedResult);
  CHECK(NothingIsExplored(explored));
  CHECK(knowledge.Facts().size() == 1);
  CHECK(knowledge.Knows(known));
}

static void UnknownFactKindIsRejected(void)
{
  CheckFileLeavesStoresUnchanged(NewFileWithFacts(1, FactBytes(9, 3, 0xff, 0xff, 0xff)),
                                 AutoMapFileHasOtherLevels);
}

static void FactOutsideTheDungeonIsRejected(void)
{
  CheckFileLeavesStoresUnchanged(
    NewFileWithFacts(1, FactBytes(AutoMapFactPit, AUTOMAP_NO_RECORD, 0, 200, 1)),
    AutoMapFileHasOtherLevels);
}

// The count is not trusted: facts are read until the file ends
static void HugeFactCountIsTruncatedNotAllocated(void)
{
  CheckFileLeavesStoresUnchanged(
    NewFileWithFacts(0xffffffffu, FactBytes(AutoMapFactActuator, 3, 0xff, 0xff, 0xff)),
    AutoMapFileIsTruncated);
}

static AutoMapRememberedItem ItemAt(AutoMapRecordId record, int descIndex, int level, int x, int y)
{
  AutoMapRememberedItem item = {record, descIndex, {level, x, y}};
  return item;
}

static std::string FileWithItems(const AutoMapItemMemory &items)
{
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, ExploredDungeon(), AutoMapKnowledge(), items, AutoMapCellMemory(), AutoMapPlayerNotes(), AutoMapMonsterDamage());
  return out.str();
}

static void VersionThreeKeepsRememberedItems(void)
{
  AutoMapItemMemory items;
  items.See(ItemAt(0x00050012u, 27, 1, 7, 9));
  items.See(ItemAt(0x00050020u, 300, 0, 4, 2));
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory read;
  bool hadItemMemory = false;
  CHECK(ReadInto(FileWithItems(items), &explored, &knowledge, &read, &hadItemMemory) == AutoMapFileRead);
  CHECK(hadItemMemory);
  std::vector<AutoMapRememberedItem> back = read.Items();
  CHECK(back.size() == 2);
  if (back.size() == 2)
  {
    CHECK(back[0].record == 0x00050012u && back[0].descIndex == 27);
    CHECK(back[0].cell.level == 1 && back[0].cell.x == 7 && back[0].cell.y == 9);
    CHECK(back[1].record == 0x00050020u && back[1].descIndex == 300);
    CHECK(back[1].cell.level == 0 && back[1].cell.x == 4 && back[1].cell.y == 2);
  }
}

static void AGlimpseStaysAGlimpse(void)
{
  AutoMapItemMemory items;
  AutoMapRememberedItem glimpse = ItemAt(0x00050012u, 27, 1, 7, 9);
  glimpse.isGlimpsed = true;
  items.See(glimpse);
  items.See(ItemAt(0x00050020u, 300, 0, 4, 2));
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory read;
  bool hadItemMemory = false;
  CHECK(ReadInto(FileWithItems(items), &explored, &knowledge, &read, &hadItemMemory) == AutoMapFileRead);
  std::vector<AutoMapRememberedItem> back = read.Items();
  CHECK(back.size() == 2 && back[0].isGlimpsed && !back[1].isGlimpsed);
}

// Version 3 did not tell glimpses apart: what it remembers was seen
static void VersionThreeItemsAreIdentified(void)
{
  std::string bytes = HeaderOfVersion(3);
  AutoMapExplored empty = EmptyDungeon();
  for (int level = 0; level < 2; level++)
  {
    AutoMapLevelSize size = empty.SizeOfLevel(level);
    AppendNumber(&bytes, (uint32_t)size.width, 1);
    AppendNumber(&bytes, (uint32_t)size.height, 1);
    bytes.append(AutoMapExplored::ByteCountFor(size) + AutoMapExplored::FaceByteCountFor(size), '\0');
  }
  AppendNumber(&bytes, 0, 4);
  AppendNumber(&bytes, 1, 4);
  AppendNumber(&bytes, 0x00050012u, 4);
  AppendNumber(&bytes, 27, 2);
  AppendNumber(&bytes, 1, 1);
  AppendNumber(&bytes, 7, 1);
  AppendNumber(&bytes, 9, 1);
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory read;
  bool hadItemMemory = false;
  CHECK(ReadInto(bytes, &explored, &knowledge, &read, &hadItemMemory) == AutoMapFileRead);
  CHECK(read.Items().size() == 1 && read.Identifies(ItemAt(0x00050012u, 27, 1, 7, 9)));
}

static void VersionTwoFileHasNoItemMemory(void)
{
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  bool hadItemMemory = true;
  CHECK(ReadInto(NewFileWithFacts(0, ""), &explored, &knowledge, &items, &hadItemMemory) == AutoMapFileRead);
  CHECK(!hadItemMemory);
  CHECK(items.Items().empty());
}

static void TruncatedItemListChangesNoStore(void)
{
  AutoMapItemMemory written;
  written.See(ItemAt(0x00050012u, 27, 1, 7, 9));
  std::string bytes = FileWithItems(written);
  bytes.erase(bytes.size() - 1);
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapFact known = AutoMapFact_OfRecord(AutoMapFactChestOpened, 5);
  knowledge.Learn(known);
  AutoMapItemMemory items;
  AutoMapRememberedItem kept = ItemAt(0x00050030u, 2, 0, 1, 1);
  items.See(kept);
  bool hadItemMemory = false;
  CHECK(ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory) == AutoMapFileIsTruncated);
  CHECK(NothingIsExplored(explored));
  CHECK(knowledge.Facts().size() == 1);
  CHECK(items.Items().size() == 1);
  CHECK(items.Remembers(kept));
}

static void ItemOutsideTheDungeonIsRejected(void)
{
  AutoMapItemMemory written;
  written.See(ItemAt(0x00050012u, 27, 9, 1, 1));
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  bool hadItemMemory = false;
  CHECK(ReadInto(FileWithItems(written), &explored, &knowledge, &items, &hadItemMemory)
        == AutoMapFileHasOtherLevels);
  CHECK(items.Items().empty());
}

static void EveryResultHasADescription(void)
{
  CHECK(std::string(AutoMapFile_Describe(AutoMapFileBelongsToAnotherSave)) == "written for another saved game");
  CHECK(std::string(AutoMapFile_Describe(AutoMapFileIsTruncated)) == "file is incomplete");
}

static AutoMapCellLook LookWith(AutoMapCellKind kind, AutoMapDoorState door, bool pitIsOpen,
                                int floorPicture, int side0)
{
  AutoMapCellLook look;
  look.kind = kind;
  look.doorState = door;
  look.pitIsOpen = pitIsOpen;
  look.pitIsHidden = !pitIsOpen;
  look.spinnerIsVisible = false;
  look.floorPicture = floorPicture;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) look.sidePictures[side] = side0 + side;
  return look;
}

static bool SameLook(const AutoMapCellLook &a, const AutoMapCellLook &b)
{
  return !AutoMapScene_LooksDiffer(a, b);
}

static std::string FileWithCells(const AutoMapCellMemory &cells)
{
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, ExploredDungeon(), AutoMapKnowledge(), AutoMapItemMemory(), cells, AutoMapPlayerNotes(), AutoMapMonsterDamage());
  return out.str();
}

static void CellLooksRoundTrip(void)
{
  AutoMapCellMemory written;
  AutoMapCell first = {0, 2, 1};
  AutoMapCell second = {1, 7, 9};
  AutoMapCellLook plain = LookWith(AutoMapCellDoor, AutoMapDoorClosed, false, 300, 40);
  AutoMapCellLook bare = LookWith(AutoMapCellPit, AutoMapDoorOpen, true, AUTOMAP_NO_PICTURE, 0);
  AutoMapCell third = {0, 3, 2};
  AutoMapCellLook field = LookWith(AutoMapCellSpinner, AutoMapDoorOpen, false, 6, 8);
  field.spinnerIsVisible = true;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) bare.sidePictures[side] = AUTOMAP_NO_PICTURE;
  written.Restore(first, plain);
  written.Restore(second, bare);
  written.Restore(third, field);
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  bool hadItemMemory = false;
  AutoMapCellMemory cells;
  bool hadCellMemory = false;
  CHECK(ReadInto(FileWithCells(written), &explored, &knowledge, &items, &hadItemMemory, &cells,
                 &hadCellMemory) == AutoMapFileRead);
  CHECK(hadCellMemory);
  CHECK(cells.Looks().size() == 3);
  AutoMapCellLook read;
  CHECK(cells.LookOf(third, &read) && SameLook(read, field));
  CHECK(read.spinnerIsVisible);
  CHECK(cells.LookOf(first, &read) && SameLook(read, plain));
  CHECK(!read.spinnerIsVisible);
  CHECK(cells.LookOf(second, &read) && SameLook(read, bare));
  CHECK(read.floorPicture == AUTOMAP_NO_PICTURE);
  CHECK(read.sidePictures[3] == AUTOMAP_NO_PICTURE);
}

static void VersionSevenHasNoCellMemory(void)
{
  AutoMapCellMemory written;
  written.Restore(AutoMapCell{0, 2, 1}, LookWith(AutoMapCellDoor, AutoMapDoorClosed, false, 3, 4));
  std::string bytes = FileWithCells(written);
  // Version 7 ends after the items: cut the damage count, the notes (two counts) and the look
  // block (count 4 bytes + 1 look of 18 bytes)
  bytes.erase(bytes.size() - (NO_DAMAGE_BYTES + 8 + 4 + 18));
  bytes = WithoutRims(bytes);
  bytes[VERSION_OFFSET] = 7;
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  bool hadItemMemory = false;
  AutoMapCellMemory cells;
  AutoMapCell kept = {0, 1, 1};
  cells.Restore(kept, LookWith(AutoMapCellFloor, AutoMapDoorOpen, false, 1, 2));
  bool hadCellMemory = true;
  CHECK(ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory, &cells, &hadCellMemory)
        == AutoMapFileRead);
  CHECK(!hadCellMemory);
  CHECK(cells.Looks().size() == 1);
  AutoMapCellLook still;
  CHECK(cells.LookOf(kept, &still));
  CHECK(SameLook(still, LookWith(AutoMapCellFloor, AutoMapDoorOpen, false, 1, 2)));
  // The rest of the file did load
  CHECK(explored.IsExplored(AutoMapCell{0, 0, 0}));
}

static void TruncatedLooksLeaveEveryStore(void)
{
  AutoMapCellMemory written;
  written.Restore(AutoMapCell{0, 2, 1}, LookWith(AutoMapCellDoor, AutoMapDoorClosed, false, 3, 4));
  std::string bytes = FileWithCells(written);
  bytes.erase(bytes.size() - 1);
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapFact known = AutoMapFact_OfRecord(AutoMapFactChestOpened, 5);
  knowledge.Learn(known);
  AutoMapItemMemory items;
  AutoMapRememberedItem keptItem = ItemAt(0x00050030u, 2, 0, 1, 1);
  items.See(keptItem);
  AutoMapCellMemory cells;
  cells.Restore(AutoMapCell{0, 1, 1}, LookWith(AutoMapCellFloor, AutoMapDoorOpen, false, 1, 2));
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  CHECK(ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory, &cells, &hadCellMemory)
        == AutoMapFileIsTruncated);
  CHECK(NothingIsExplored(explored));
  CHECK(knowledge.Facts().size() == 1);
  CHECK(items.Items().size() == 1);
  CHECK(items.Remembers(keptItem));
  CHECK(cells.Looks().size() == 1);
}

static void LookOutsideTheDungeonIsRejected(void)
{
  AutoMapCellMemory written;
  written.Restore(AutoMapCell{9, 1, 1}, LookWith(AutoMapCellDoor, AutoMapDoorClosed, false, 3, 4));
  AutoMapExplored explored = EmptyDungeon();
  CHECK(ReadInto(FileWithCells(written), &explored) == AutoMapFileHasOtherLevels);
}

// Every store must be as it was before the failed read
static void CheckLooksFileLeavesEveryStore(const std::string &bytes, AutoMapFileResult expected)
{
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapFact known = AutoMapFact_OfRecord(AutoMapFactChestOpened, 5);
  knowledge.Learn(known);
  AutoMapItemMemory items;
  AutoMapRememberedItem keptItem = ItemAt(0x00050030u, 2, 0, 1, 1);
  items.See(keptItem);
  AutoMapCellMemory cells;
  AutoMapCell kept = {0, 1, 1};
  AutoMapCellLook keptLook = LookWith(AutoMapCellFloor, AutoMapDoorOpen, false, 1, 2);
  cells.Restore(kept, keptLook);
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  CHECK(ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory, &cells, &hadCellMemory)
        == expected);
  CHECK(NothingIsExplored(explored));
  CHECK(knowledge.Facts().size() == 1);
  CHECK(items.Items().size() == 1);
  CHECK(items.Remembers(keptItem));
  CHECK(cells.Looks().size() == 1);
  AutoMapCellLook still;
  CHECK(cells.LookOf(kept, &still) && SameLook(still, keptLook));
}

static void HugeLookCountIsTruncatedNotAllocated(void)
{
  AutoMapCellMemory none;
  std::string bytes = FileWithCells(none);
  bytes.erase(bytes.size() - (4 + 8 + NO_DAMAGE_BYTES));
  AppendNumber(&bytes, 0xFFFFFFFFu, 4);
  AppendNumber(&bytes, 0, 4);
  AppendNumber(&bytes, 0, 4);
  AppendNumber(&bytes, 0, NO_DAMAGE_BYTES);
  CheckLooksFileLeavesEveryStore(bytes, AutoMapFileIsTruncated);
}

static void BadLookKindOrDoorIsRejected(void)
{
  AutoMapCellMemory written;
  AutoMapCell cell = {0, 2, 1};
  written.Restore(cell, LookWith(AutoMapCellDoor, AutoMapDoorClosed, false, 3, 4));
  std::string good = FileWithCells(written);
  // The look is the 18 bytes before the two note counts and the damage count: level, x, y,
  // kind, door state, ...
  size_t look = good.size() - NO_DAMAGE_BYTES - 8 - 18;
  std::string badKind = good;
  badKind[look + 3] = (char)(AutoMapCellDormantSpinner + 1);
  CheckLooksFileLeavesEveryStore(badKind, AutoMapFileHasOtherLevels);
  std::string badDoor = good;
  badDoor[look + 4] = (char)(AutoMapDoorDestroyed + 1);
  CheckLooksFileLeavesEveryStore(badDoor, AutoMapFileHasOtherLevels);
}

static std::string FileWithNotes(const AutoMapPlayerNotes &notes)
{
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, ExploredDungeon(), AutoMapKnowledge(), AutoMapItemMemory(),
                                    AutoMapCellMemory(), notes, AutoMapMonsterDamage());
  return out.str();
}

static void NotesRoundTrip(void)
{
  AutoMapPlayerNotes written;
  written.SetPit(AutoMapCell{0, 2, 1}, true);
  written.SetPit(AutoMapCell{1, 7, 9}, false);
  AutoMapLinkSource floorSource = {AutoMapCell{1, 3, 4}, -1};
  AutoMapLinkSource faceSource = {AutoMapCell{0, 4, 2}, 3};
  written.ToggleLinkTarget(floorSource, AutoMapCell{1, 31, 30});
  written.ToggleLinkTarget(faceSource, AutoMapCell{0, 0, 1});
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  AutoMapCellMemory cells;
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  AutoMapPlayerNotes notes;
  CHECK(ReadInto(FileWithNotes(written), &explored, &knowledge, &items, &hadItemMemory, &cells,
                 &hadCellMemory, &notes) == AutoMapFileRead);
  CHECK(notes.Pits().size() == 2);
  CHECK(notes.PitAt(AutoMapCell{0, 2, 1}) == 1);
  CHECK(notes.PitAt(AutoMapCell{1, 7, 9}) == 0);
  CHECK(notes.Links().size() == 2);
  std::vector<AutoMapCell> floorTargets = notes.LinksFrom(floorSource);
  CHECK(floorTargets.size() == 1);
  CHECK(floorTargets[0].level == 1 && floorTargets[0].x == 31 && floorTargets[0].y == 30);
  std::vector<AutoMapCell> faceTargets = notes.LinksFrom(faceSource);
  CHECK(faceTargets.size() == 1);
  CHECK(faceTargets[0].level == 0 && faceTargets[0].x == 0 && faceTargets[0].y == 1);
}

static void VersionEightHasNoNotes(void)
{
  AutoMapPlayerNotes written;
  written.SetPit(AutoMapCell{0, 2, 1}, true);
  std::string bytes = FileWithNotes(written);
  // Version 8 ends after the looks: cut the damage count, the marks (count + 4 bytes) and the
  // links (count)
  bytes.erase(bytes.size() - (NO_DAMAGE_BYTES + 4 + 4 + 4));
  bytes = WithoutRims(bytes);
  bytes[VERSION_OFFSET] = 8;
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  AutoMapCellMemory cells;
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  AutoMapPlayerNotes notes;
  notes.SetPit(AutoMapCell{0, 1, 1}, false);
  CHECK(ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory, &cells, &hadCellMemory,
                 &notes) == AutoMapFileRead);
  CHECK(hadCellMemory);
  // The notes are left as they were
  CHECK(notes.Pits().size() == 1);
  CHECK(explored.IsExplored(AutoMapCell{0, 0, 0}));
}

// Every store must be as it was before the failed read
static void CheckNotesFileLeavesEveryStore(const std::string &bytes, AutoMapFileResult expected)
{
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactChestOpened, 5));
  AutoMapItemMemory items;
  items.See(ItemAt(0x00050030u, 2, 0, 1, 1));
  AutoMapCellMemory cells;
  cells.Restore(AutoMapCell{0, 1, 1}, LookWith(AutoMapCellFloor, AutoMapDoorOpen, false, 1, 2));
  AutoMapPlayerNotes notes;
  AutoMapCell keptPit = {0, 1, 2};
  notes.SetPit(keptPit, true);
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  CHECK(ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory, &cells, &hadCellMemory,
                 &notes) == expected);
  CHECK(NothingIsExplored(explored));
  CHECK(knowledge.Facts().size() == 1);
  CHECK(items.Items().size() == 1);
  CHECK(cells.Looks().size() == 1);
  CHECK(notes.Pits().size() == 1);
  CHECK(notes.PitAt(keptPit) == 1);
  CHECK(notes.Links().empty());
}

static void HugeNoteCountIsTruncatedNotAllocated(void)
{
  std::string none = FileWithNotes(AutoMapPlayerNotes());
  std::string hugeMarks = none;
  hugeMarks.erase(hugeMarks.size() - 8 - NO_DAMAGE_BYTES);
  AppendNumber(&hugeMarks, 0xFFFFFFFFu, 4);
  AppendNumber(&hugeMarks, 0, 4);
  AppendNumber(&hugeMarks, 0, 4);
  CheckNotesFileLeavesEveryStore(hugeMarks, AutoMapFileIsTruncated);
  std::string hugeLinks = none;
  hugeLinks.erase(hugeLinks.size() - 4 - NO_DAMAGE_BYTES);
  AppendNumber(&hugeLinks, 0xFFFFFFFFu, 4);
  AppendNumber(&hugeLinks, 0, 4);
  CheckNotesFileLeavesEveryStore(hugeLinks, AutoMapFileIsTruncated);
}

static void BadNoteFieldsAreRejected(void)
{
  AutoMapPlayerNotes pit;
  pit.SetPit(AutoMapCell{0, 2, 1}, true);
  std::string goodPit = FileWithNotes(pit);
  // The mark is the 4 bytes before the link count and the damage count: level, x, y, open
  size_t mark = goodPit.size() - NO_DAMAGE_BYTES - 4 - 4;
  std::string badOpen = goodPit;
  badOpen[mark + 3] = 2;
  CheckNotesFileLeavesEveryStore(badOpen, AutoMapFileHasOtherLevels);
  std::string outside = goodPit;
  outside[mark + 1] = 5;   // level 0 is 5 wide
  CheckNotesFileLeavesEveryStore(outside, AutoMapFileHasOtherLevels);
  std::string otherLevel = goodPit;
  otherLevel[mark] = 9;
  CheckNotesFileLeavesEveryStore(otherLevel, AutoMapFileHasOtherLevels);

  AutoMapPlayerNotes link;
  link.ToggleLinkTarget(AutoMapLinkSource{AutoMapCell{0, 4, 2}, 3}, AutoMapCell{0, 0, 1});
  std::string goodLink = FileWithNotes(link);
  // The link is the 7 bytes before the damage count: source level, x, y, side, target level, x, y
  size_t at = goodLink.size() - NO_DAMAGE_BYTES - 7;
  std::string badSide = goodLink;
  badSide[at + 3] = 7;
  CheckNotesFileLeavesEveryStore(badSide, AutoMapFileHasOtherLevels);
  std::string crossLevel = goodLink;
  crossLevel[at + 4] = 1;
  CheckNotesFileLeavesEveryStore(crossLevel, AutoMapFileHasOtherLevels);
  std::string targetOutside = goodLink;
  targetOutside[at + 5] = 5;
  CheckNotesFileLeavesEveryStore(targetOutside, AutoMapFileHasOtherLevels);
  std::string sourceOutside = goodLink;
  sourceOutside[at + 2] = 3;   // level 0 is 3 high
  CheckNotesFileLeavesEveryStore(sourceOutside, AutoMapFileHasOtherLevels);
}

// A damaged file may hold one link twice: it loads once, never toggled away
static void DuplicateLinkLoadsOnce(void)
{
  AutoMapPlayerNotes link;
  AutoMapLinkSource source = {AutoMapCell{0, 4, 2}, 3};
  link.ToggleLinkTarget(source, AutoMapCell{0, 0, 1});
  std::string bytes = FileWithNotes(link);
  // The link count and the one 7-byte record come before the damage count
  std::string record = bytes.substr(bytes.size() - NO_DAMAGE_BYTES - 7, 7);
  bytes.erase(bytes.size() - NO_DAMAGE_BYTES - 7 - 4);
  AppendNumber(&bytes, 2, 4);
  bytes += record;
  bytes += record;
  AppendNumber(&bytes, 0, 4);
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  AutoMapCellMemory cells;
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  AutoMapPlayerNotes notes;
  CHECK(ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory, &cells, &hadCellMemory,
                 &notes) == AutoMapFileRead);
  CHECK(notes.Links().size() == 1);
  std::vector<AutoMapCell> targets = notes.LinksFrom(source);
  CHECK(targets.size() == 1 && targets[0].x == 0 && targets[0].y == 1);
}

static std::string FileWithDamage(const AutoMapMonsterDamage &damage)
{
  std::ostringstream out;
  AutoMapFile(SAVE_SIGNATURE).Write(out, ExploredDungeon(), AutoMapKnowledge(), AutoMapItemMemory(),
                                    AutoMapCellMemory(), AutoMapPlayerNotes(), damage);
  return out.str();
}

static AutoMapFileResult ReadDamage(const std::string &bytes, AutoMapMonsterDamage *damage)
{
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  AutoMapCellMemory cells;
  AutoMapPlayerNotes notes;
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  return ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory, &cells, &hadCellMemory,
                  &notes, damage);
}

static void DamageRoundTrips(void)
{
  AutoMapMonsterDamage written;
  written.Credit(0x40002, 0, 17);
  written.Credit(0x40002, 3, 4);
  written.Credit(0x40009, 1, 65535);
  AutoMapMonsterDamage damage;
  CHECK(ReadDamage(FileWithDamage(written), &damage) == AutoMapFileRead);
  CHECK(damage.Groups() == written.Groups());
}

static void OlderFilesKeepTheDamageAsItIs(void)
{
  std::string bytes = FileWithDamage(AutoMapMonsterDamage());
  // Version 9 ends after the notes
  bytes.erase(bytes.size() - NO_DAMAGE_BYTES);
  bytes = WithoutRims(bytes);
  bytes[VERSION_OFFSET] = 9;
  AutoMapMonsterDamage damage;
  damage.Credit(0x40002, 1, 6);
  CHECK(ReadDamage(bytes, &damage) == AutoMapFileRead);
  CHECK(damage.Groups().size() == 1);
  CHECK(damage.DealtTo(0x40002, 1) == 6);
}

// Every store must be as it was before the failed read
static void CheckDamageFileLeavesEveryStore(const std::string &bytes, AutoMapFileResult expected)
{
  AutoMapExplored explored = EmptyDungeon();
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactChestOpened, 5));
  AutoMapItemMemory items;
  AutoMapCellMemory cells;
  AutoMapPlayerNotes notes;
  notes.SetPit(AutoMapCell{0, 1, 2}, true);
  AutoMapMonsterDamage damage;
  damage.Credit(0x40002, 1, 6);
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  CHECK(ReadInto(bytes, &explored, &knowledge, &items, &hadItemMemory, &cells, &hadCellMemory,
                 &notes, &damage) == expected);
  CHECK(NothingIsExplored(explored));
  CHECK(knowledge.Facts().size() == 1);
  CHECK(notes.Pits().size() == 1);
  CHECK(damage.Groups().size() == 1);
  CHECK(damage.DealtTo(0x40002, 1) == 6);
}

static void ADamagedDamageBlockLeavesTheStoresUnchanged(void)
{
  std::string huge = FileWithDamage(AutoMapMonsterDamage());
  huge.erase(huge.size() - NO_DAMAGE_BYTES);
  AppendNumber(&huge, 0xFFFFFFFFu, 4);
  CheckDamageFileLeavesEveryStore(huge, AutoMapFileIsTruncated);
  AutoMapMonsterDamage one;
  one.Credit(0x40003, 2, 9);
  std::string cut = FileWithDamage(one);
  cut.erase(cut.size() - 3);
  CheckDamageFileLeavesEveryStore(cut, AutoMapFileIsTruncated);
}

static void ANoRecordGroupIsRejected(void)
{
  std::string bytes = FileWithDamage(AutoMapMonsterDamage());
  bytes.erase(bytes.size() - NO_DAMAGE_BYTES);
  AppendNumber(&bytes, 1, 4);
  AppendNumber(&bytes, AUTOMAP_NO_RECORD, 4);
  for (int member = 0; member < AUTOMAP_DAMAGE_MEMBERS; member++) AppendNumber(&bytes, 5, 2);
  CheckDamageFileLeavesEveryStore(bytes, AutoMapFileHasOtherLevels);
}

void TestAutoMapFile(void)
{
  RimsRoundTrip();
  OlderFilesTakeTheRimFromTheFloorBesideIt();
  RoundTripReturnsTheSameCells();
  EmptyFileIsRejected();
  FileOfAnotherKindIsRejected();
  UnknownVersionIsRejected();
  FileOfAnotherSaveIsRejected();
  OtherLevelCountIsRejected();
  AbsurdLevelCountIsRejected();
  OtherLevelSizeIsRejected();
  FileCutShortAnywhereIsRejected();
  DamageInALaterLevelLeavesTheMapEmpty();
  FacesAndFactsRoundTrip();
  OldFileFillsFacesTowardsExploredCells();
  UnknownFactKindIsRejected();
  FactOutsideTheDungeonIsRejected();
  HugeFactCountIsTruncatedNotAllocated();
  VersionThreeKeepsRememberedItems();
  VersionTwoFileHasNoItemMemory();
  AGlimpseStaysAGlimpse();
  VersionThreeItemsAreIdentified();
  GlimpsedFacesRoundTrip();
  ReadTextsRoundTrip();
  LookedIntoAlcovesRoundTrip();
  OlderFilesHaveTheirSeenAlcovesLookedInto();
  OlderFilesHaveTheirSeenTextsRead();
  TruncatedItemListChangesNoStore();
  ItemOutsideTheDungeonIsRejected();
  CellLooksRoundTrip();
  VersionSevenHasNoCellMemory();
  TruncatedLooksLeaveEveryStore();
  LookOutsideTheDungeonIsRejected();
  HugeLookCountIsTruncatedNotAllocated();
  BadLookKindOrDoorIsRejected();
  NotesRoundTrip();
  VersionEightHasNoNotes();
  HugeNoteCountIsTruncatedNotAllocated();
  BadNoteFieldsAreRejected();
  DuplicateLinkLoadsOnce();
  DamageRoundTrips();
  OlderFilesKeepTheDamageAsItIs();
  ADamagedDamageBlockLeavesTheStoresUnchanged();
  ANoRecordGroupIsRejected();
  EveryResultHasADescription();
}
