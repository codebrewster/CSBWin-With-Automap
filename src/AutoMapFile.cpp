// AutoMapFile.cpp : reads and writes the .map file that accompanies a saved game.
#include "AutoMapFile.h"

#include <cstring>
#include <istream>
#include <ostream>
#include <vector>

static const char FILE_TYPE_MARKER[4] = {'C', 'S', 'B', 'M'};
static const uint32_t FORMAT_VERSION = 15;
static const uint32_t FIRST_VERSION_WITH_FACES = 2;
static const uint32_t FIRST_VERSION_WITH_ITEMS = 3;
static const uint32_t FIRST_VERSION_WITH_GLIMPSES = 4;
static const uint32_t FIRST_VERSION_WITH_GLIMPSED_FACES = 5;
static const uint32_t FIRST_VERSION_WITH_READ_TEXTS = 6;
static const uint32_t FIRST_VERSION_WITH_LOOKED_INTO_ALCOVES = 7;
static const uint32_t FIRST_VERSION_WITH_CELL_LOOKS = 8;
static const uint32_t FIRST_VERSION_WITH_NOTES = 9;
static const uint32_t FIRST_VERSION_WITH_DAMAGE = 10;
static const uint32_t FIRST_VERSION_WITH_RIMS = 11;
static const uint32_t FIRST_VERSION_WITH_SEEN_FLOORS = 12;
static const uint32_t FIRST_VERSION_WITH_STATISTICS = 13;
// A floor link source has no side: it is stored as all ones in its byte
static const uint32_t NO_SIDE_BYTE = 0xff;
// A missing picture is stored as all ones in its two bytes
static const uint32_t NO_PICTURE_BYTES = 0xffff;
// In a record fact the three cell bytes (and in a cell fact the record) hold no value
static const uint32_t NO_CELL_BYTE = 0xff;

static void WriteNumber(std::ostream &out, uint32_t value, int byteCount)
{
  for (int i = 0; i < byteCount; i++) out.put((char)((value >> (8 * i)) & 0xff));
}

static bool ReadNumber(std::istream &in, int byteCount, uint32_t *value)
{
  *value = 0;
  for (int i = 0; i < byteCount; i++)
  {
    int byte = in.get();
    if (!in.good()) return false;
    *value |= (uint32_t)byte << (8 * i);
  }
  return true;
}

static uint32_t PictureToBytes(int picture)
{
  return picture == AUTOMAP_NO_PICTURE ? NO_PICTURE_BYTES : (uint32_t)picture;
}

static int PictureFromBytes(uint32_t bytes)
{
  return bytes == NO_PICTURE_BYTES ? AUTOMAP_NO_PICTURE : (int)bytes;
}

AutoMapFile::AutoMapFile(const AutoMapSaveSignature &saveSignature)
  : m_saveSignature(saveSignature)
{
}

void AutoMapFile::Write(std::ostream &out, const AutoMapExplored &explored,
                        const AutoMapKnowledge &knowledge,
                        const AutoMapItemMemory &items, const AutoMapCellMemory &cells,
                        const AutoMapPlayerNotes &notes,
                        const AutoMapMonsterDamage &damage, const HeroStats &stats,
                        const HeroStatsOrigins &origins) const
{
  out.write(FILE_TYPE_MARKER, sizeof(FILE_TYPE_MARKER));
  WriteNumber(out, FORMAT_VERSION, 2);
  WriteNumber(out, m_saveSignature.first, 4);
  WriteNumber(out, m_saveSignature.second, 4);
  WriteNumber(out, (uint32_t)explored.LevelCount(), 2);
  for (int level = 0; level < explored.LevelCount(); level++)
  {
    AutoMapLevelSize size = explored.SizeOfLevel(level);
    const std::vector<uint8_t> &bits = explored.BitsOfLevel(level);
    WriteNumber(out, (uint32_t)size.width, 1);
    WriteNumber(out, (uint32_t)size.height, 1);
    out.write((const char *)bits.data(), (std::streamsize)bits.size());
    const std::vector<uint8_t> &faceBits = explored.FaceBitsOfLevel(level);
    out.write((const char *)faceBits.data(), (std::streamsize)faceBits.size());
    const std::vector<uint8_t> &glimpsedFaceBits = explored.GlimpsedFaceBitsOfLevel(level);
    out.write((const char *)glimpsedFaceBits.data(), (std::streamsize)glimpsedFaceBits.size());
    const std::vector<uint8_t> &readTextBits = explored.ReadTextBitsOfLevel(level);
    out.write((const char *)readTextBits.data(), (std::streamsize)readTextBits.size());
    const std::vector<uint8_t> &lookedIntoBits = explored.LookedIntoBitsOfLevel(level);
    out.write((const char *)lookedIntoBits.data(), (std::streamsize)lookedIntoBits.size());
    const std::vector<uint8_t> &floorBits = explored.FloorBitsOfLevel(level);
    out.write((const char *)floorBits.data(), (std::streamsize)floorBits.size());
    for (int set = 0; set < AUTOMAP_BIT_SET_COUNT; set++)
    {
      const std::vector<uint8_t> &rimBits = explored.RimBitsOfLevel(level, (AutoMapBitSet)set);
      out.write((const char *)rimBits.data(), (std::streamsize)rimBits.size());
    }
  }
  std::vector<AutoMapFact> facts = knowledge.Facts();
  WriteNumber(out, (uint32_t)facts.size(), 4);
  for (size_t i = 0; i < facts.size(); i++)
  {
    const AutoMapFact &fact = facts[i];
    bool hasCell = fact.cell.level >= 0;
    WriteNumber(out, (uint32_t)fact.kind, 1);
    WriteNumber(out, fact.record, 4);
    WriteNumber(out, hasCell ? (uint32_t)fact.cell.level : NO_CELL_BYTE, 1);
    WriteNumber(out, hasCell ? (uint32_t)fact.cell.x : NO_CELL_BYTE, 1);
    WriteNumber(out, hasCell ? (uint32_t)fact.cell.y : NO_CELL_BYTE, 1);
  }
  std::vector<AutoMapRememberedItem> remembered = items.Items();
  WriteNumber(out, (uint32_t)remembered.size(), 4);
  for (size_t i = 0; i < remembered.size(); i++)
  {
    const AutoMapRememberedItem &item = remembered[i];
    WriteNumber(out, item.record, 4);
    WriteNumber(out, (uint32_t)item.descIndex, 2);
    WriteNumber(out, (uint32_t)item.cell.level, 1);
    WriteNumber(out, (uint32_t)item.cell.x, 1);
    WriteNumber(out, (uint32_t)item.cell.y, 1);
    WriteNumber(out, item.isGlimpsed ? 1 : 0, 1);
  }
  std::vector<std::pair<AutoMapCell, AutoMapCellLook> > looks = cells.Looks();
  WriteNumber(out, (uint32_t)looks.size(), 4);
  for (size_t i = 0; i < looks.size(); i++)
  {
    const AutoMapCell &cell = looks[i].first;
    const AutoMapCellLook &look = looks[i].second;
    WriteNumber(out, (uint32_t)cell.level, 1);
    WriteNumber(out, (uint32_t)cell.x, 1);
    WriteNumber(out, (uint32_t)cell.y, 1);
    WriteNumber(out, (uint32_t)look.kind, 1);
    WriteNumber(out, (uint32_t)look.doorState, 1);
    WriteNumber(out, look.pitIsOpen ? 1 : 0, 1);
    WriteNumber(out, look.pitIsHidden ? 1 : 0, 1);
    WriteNumber(out, look.spinnerIsVisible ? 1 : 0, 1);
    WriteNumber(out, PictureToBytes(look.floorPicture), 2);
    for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
    {
      WriteNumber(out, PictureToBytes(look.sidePictures[side]), 2);
    }
  }
  std::vector<std::pair<AutoMapCell, bool> > pits = notes.Pits();
  WriteNumber(out, (uint32_t)pits.size(), 4);
  for (size_t i = 0; i < pits.size(); i++)
  {
    WriteNumber(out, (uint32_t)pits[i].first.level, 1);
    WriteNumber(out, (uint32_t)pits[i].first.x, 1);
    WriteNumber(out, (uint32_t)pits[i].first.y, 1);
    WriteNumber(out, pits[i].second ? 1 : 0, 1);
  }
  std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > links = notes.Links();
  WriteNumber(out, (uint32_t)links.size(), 4);
  for (size_t i = 0; i < links.size(); i++)
  {
    const AutoMapLinkSource &source = links[i].first;
    const AutoMapCell &target = links[i].second;
    WriteNumber(out, (uint32_t)source.cell.level, 1);
    WriteNumber(out, (uint32_t)source.cell.x, 1);
    WriteNumber(out, (uint32_t)source.cell.y, 1);
    WriteNumber(out, source.side < 0 ? NO_SIDE_BYTE : (uint32_t)source.side, 1);
    WriteNumber(out, (uint32_t)target.level, 1);
    WriteNumber(out, (uint32_t)target.x, 1);
    WriteNumber(out, (uint32_t)target.y, 1);
  }
  std::vector<std::pair<AutoMapRecordId, std::vector<int> > > groups = damage.Groups();
  WriteNumber(out, (uint32_t)groups.size(), 4);
  for (size_t i = 0; i < groups.size(); i++)
  {
    WriteNumber(out, groups[i].first, 4);
    for (int member = 0; member < AUTOMAP_DAMAGE_MEMBERS; member++)
    {
      WriteNumber(out, (uint32_t)groups[i].second[(size_t)member], 2);
    }
  }
  // The statistics block: its length, then the length of the statistics proper (the origins
  // fill the rest), so a reader can take the whole block off the stream before parsing it
  std::vector<uint8_t> statsBytes;
  stats.Serialize(&statsBytes);
  std::vector<uint8_t> originsBytes;
  origins.Serialize(&originsBytes);
  WriteNumber(out, (uint32_t)(4 + statsBytes.size() + originsBytes.size()), 4);
  WriteNumber(out, (uint32_t)statsBytes.size(), 4);
  out.write((const char *)statsBytes.data(), (std::streamsize)statsBytes.size());
  out.write((const char *)originsBytes.data(), (std::streamsize)originsBytes.size());
}

static AutoMapFileResult ReadHeader(std::istream &in, uint32_t *version,
                                    AutoMapSaveSignature *saveSignature)
{
  char marker[sizeof(FILE_TYPE_MARKER)];
  in.read(marker, sizeof(marker));
  if (!in.good()) return AutoMapFileIsNotAMapFile;
  if (memcmp(marker, FILE_TYPE_MARKER, sizeof(marker)) != 0) return AutoMapFileIsNotAMapFile;
  if (!ReadNumber(in, 2, version)) return AutoMapFileIsTruncated;
  if (*version < 1 || *version > FORMAT_VERSION) return AutoMapFileHasUnknownVersion;
  if (!ReadNumber(in, 4, &saveSignature->first)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 4, &saveSignature->second)) return AutoMapFileIsTruncated;
  return AutoMapFileRead;
}

// The sizes are compared with the dungeon before any cell is read, so a damaged header cannot
// ask for more memory than the dungeon needs
static AutoMapFileResult ReadLevel(std::istream &in, uint32_t version, int level,
                                   AutoMapExplored *explored)
{
  AutoMapLevelSize expectedSize = explored->SizeOfLevel(level);
  uint32_t width, height;
  if (!ReadNumber(in, 1, &width)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &height)) return AutoMapFileIsTruncated;
  if ((int)width != expectedSize.width || (int)height != expectedSize.height)
  {
    return AutoMapFileHasOtherLevels;
  }
  std::vector<uint8_t> bits(AutoMapExplored::ByteCountFor(expectedSize));
  in.read((char *)bits.data(), (std::streamsize)bits.size());
  if (!in.good()) return AutoMapFileIsTruncated;
  explored->ReplaceBitsOfLevel(level, bits);
  if (version < FIRST_VERSION_WITH_FACES) return AutoMapFileRead;
  // ReplaceFaceBitsOfLevel does not check the size, so exactly this many bytes are read
  std::vector<uint8_t> faceBits(AutoMapExplored::FaceByteCountFor(expectedSize));
  in.read((char *)faceBits.data(), (std::streamsize)faceBits.size());
  if (!in.good()) return AutoMapFileIsTruncated;
  explored->ReplaceFaceBitsOfLevel(level, faceBits);
  // Before version 5 nothing on a wall was glimpsed
  std::vector<uint8_t> glimpsedFaceBits(AutoMapExplored::FaceByteCountFor(expectedSize), 0);
  if (version >= FIRST_VERSION_WITH_GLIMPSED_FACES)
  {
    in.read((char *)glimpsedFaceBits.data(), (std::streamsize)glimpsedFaceBits.size());
    if (!in.good()) return AutoMapFileIsTruncated;
  }
  explored->ReplaceGlimpsedFaceBitsOfLevel(level, glimpsedFaceBits);
  // Before version 6 a seen face showed its text, so what was seen stays read
  std::vector<uint8_t> readTextBits = faceBits;
  if (version >= FIRST_VERSION_WITH_READ_TEXTS)
  {
    in.read((char *)readTextBits.data(), (std::streamsize)readTextBits.size());
    if (!in.good()) return AutoMapFileIsTruncated;
  }
  explored->ReplaceReadTextBitsOfLevel(level, readTextBits);
  // Before version 7 a seen face showed what lies in its alcove, so what was seen stays known
  std::vector<uint8_t> lookedIntoBits = faceBits;
  if (version >= FIRST_VERSION_WITH_LOOKED_INTO_ALCOVES)
  {
    in.read((char *)lookedIntoBits.data(), (std::streamsize)lookedIntoBits.size());
    if (!in.good()) return AutoMapFileIsTruncated;
  }
  explored->ReplaceLookedIntoBitsOfLevel(level, lookedIntoBits);
  // Before version 12 floors were not measured; Read marks those of the explored cells
  if (version >= FIRST_VERSION_WITH_SEEN_FLOORS)
  {
    std::vector<uint8_t> floorBits(AutoMapExplored::ByteCountFor(expectedSize));
    in.read((char *)floorBits.data(), (std::streamsize)floorBits.size());
    if (!in.good()) return AutoMapFileIsTruncated;
    explored->ReplaceFloorBitsOfLevel(level, floorBits);
  }
  if (version < FIRST_VERSION_WITH_RIMS) return AutoMapFileRead;
  // The sets stored, in the order of AutoMapBitSet: version 11 had all but the seen floors
  int storedSets = version >= FIRST_VERSION_WITH_SEEN_FLOORS ? AUTOMAP_BIT_SET_COUNT : AutoMapSeenFloorBits;
  for (int set = 0; set < storedSets; set++)
  {
    std::vector<uint8_t> rimBits(AutoMapExplored::RimByteCountFor(expectedSize, (AutoMapBitSet)set));
    in.read((char *)rimBits.data(), (std::streamsize)rimBits.size());
    if (!in.good()) return AutoMapFileIsTruncated;
    explored->ReplaceRimBitsOfLevel(level, (AutoMapBitSet)set, rimBits);
  }
  return AutoMapFileRead;
}

static bool CellIsInDungeon(const AutoMapExplored &explored, const AutoMapCell &cell)
{
  if (cell.level < 0 || cell.level >= explored.LevelCount()) return false;
  AutoMapLevelSize size = explored.SizeOfLevel(cell.level);
  return cell.x >= 0 && cell.x < size.width && cell.y >= 0 && cell.y < size.height;
}

static AutoMapFileResult ReadFact(std::istream &in, const AutoMapExplored &dungeon,
                                  AutoMapKnowledge *facts)
{
  uint32_t kind, record, level, x, y;
  if (!ReadNumber(in, 1, &kind)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 4, &record)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &level)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &x)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &y)) return AutoMapFileIsTruncated;
  if (!AutoMapFact_KindIsKnown((int)kind)) return AutoMapFileHasOtherLevels;
  AutoMapFactKind factKind = (AutoMapFactKind)kind;
  AutoMapCell cell = {(int)level, (int)x, (int)y};
  if (factKind == AutoMapFactLink)
  {
    if (!CellIsInDungeon(dungeon, cell)) return AutoMapFileHasOtherLevels;
    facts->Learn(AutoMapFact_OfLink(record, cell));
  }
  else if (factKind >= AutoMapFactFalseWall)
  {
    if (!CellIsInDungeon(dungeon, cell)) return AutoMapFileHasOtherLevels;
    facts->Learn(AutoMapFact_OfCell(factKind, cell));
  }
  else
  {
    if (level != NO_CELL_BYTE || x != NO_CELL_BYTE || y != NO_CELL_BYTE)
    {
      return AutoMapFileHasOtherLevels;
    }
    facts->Learn(AutoMapFact_OfRecord(factKind, record));
  }
  return AutoMapFileRead;
}

// The count is not trusted for allocation: a damaged count just meets the end of the file
static AutoMapFileResult ReadFacts(std::istream &in, const AutoMapExplored &dungeon,
                                   AutoMapKnowledge *facts)
{
  uint32_t factCount;
  if (!ReadNumber(in, 4, &factCount)) return AutoMapFileIsTruncated;
  for (uint32_t i = 0; i < factCount; i++)
  {
    AutoMapFileResult result = ReadFact(in, dungeon, facts);
    if (result != AutoMapFileRead) return result;
  }
  return AutoMapFileRead;
}

static AutoMapFileResult ReadItem(std::istream &in, uint32_t version, const AutoMapExplored &dungeon,
                                  AutoMapItemMemory *items)
{
  uint32_t record, descIndex, level, x, y;
  uint32_t isGlimpsed = 0;
  if (!ReadNumber(in, 4, &record)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 2, &descIndex)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &level)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &x)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &y)) return AutoMapFileIsTruncated;
  // Version 3 remembered only what was seen
  if (version >= FIRST_VERSION_WITH_GLIMPSES && !ReadNumber(in, 1, &isGlimpsed)) return AutoMapFileIsTruncated;
  AutoMapRememberedItem item = {record, (int)descIndex, {(int)level, (int)x, (int)y}, isGlimpsed != 0};
  if (!CellIsInDungeon(dungeon, item.cell)) return AutoMapFileHasOtherLevels;
  items->See(item);
  return AutoMapFileRead;
}

// The count is not trusted for allocation here either
static AutoMapFileResult ReadItems(std::istream &in, uint32_t version, const AutoMapExplored &dungeon,
                                   AutoMapItemMemory *items)
{
  uint32_t itemCount;
  if (!ReadNumber(in, 4, &itemCount)) return AutoMapFileIsTruncated;
  for (uint32_t i = 0; i < itemCount; i++)
  {
    AutoMapFileResult result = ReadItem(in, version, dungeon, items);
    if (result != AutoMapFileRead) return result;
  }
  return AutoMapFileRead;
}

static AutoMapFileResult ReadLook(std::istream &in, const AutoMapExplored &dungeon,
                                  AutoMapCellMemory *cells)
{
  uint32_t level, x, y, kind, doorState, pitIsOpen, pitIsHidden, spinnerIsVisible, floorPicture;
  if (!ReadNumber(in, 1, &level)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &x)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &y)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &kind)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &doorState)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &pitIsOpen)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &pitIsHidden)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &spinnerIsVisible)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 2, &floorPicture)) return AutoMapFileIsTruncated;
  AutoMapCellLook look;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    uint32_t picture;
    if (!ReadNumber(in, 2, &picture)) return AutoMapFileIsTruncated;
    look.sidePictures[side] = PictureFromBytes(picture);
  }
  AutoMapCell cell = {(int)level, (int)x, (int)y};
  if (!CellIsInDungeon(dungeon, cell)) return AutoMapFileHasOtherLevels;
  if (kind > (uint32_t)AutoMapCellDormantSpinner) return AutoMapFileHasOtherLevels;
  if (doorState > (uint32_t)AutoMapDoorDestroyed) return AutoMapFileHasOtherLevels;
  look.kind = (AutoMapCellKind)kind;
  look.doorState = (AutoMapDoorState)doorState;
  look.pitIsOpen = pitIsOpen != 0;
  look.pitIsHidden = pitIsHidden != 0;
  look.spinnerIsVisible = spinnerIsVisible != 0;
  look.floorPicture = PictureFromBytes(floorPicture);
  cells->Restore(cell, look);
  return AutoMapFileRead;
}

// The count is not trusted for allocation here either
static AutoMapFileResult ReadLooks(std::istream &in, const AutoMapExplored &dungeon,
                                   AutoMapCellMemory *cells)
{
  uint32_t lookCount;
  if (!ReadNumber(in, 4, &lookCount)) return AutoMapFileIsTruncated;
  for (uint32_t i = 0; i < lookCount; i++)
  {
    AutoMapFileResult result = ReadLook(in, dungeon, cells);
    if (result != AutoMapFileRead) return result;
  }
  return AutoMapFileRead;
}

static AutoMapFileResult ReadPit(std::istream &in, const AutoMapExplored &dungeon,
                                 AutoMapPlayerNotes *notes)
{
  uint32_t level, x, y, open;
  if (!ReadNumber(in, 1, &level)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &x)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &y)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &open)) return AutoMapFileIsTruncated;
  AutoMapCell cell = {(int)level, (int)x, (int)y};
  if (!CellIsInDungeon(dungeon, cell)) return AutoMapFileHasOtherLevels;
  if (open > 1) return AutoMapFileHasOtherLevels;
  notes->SetPit(cell, open == 1);
  return AutoMapFileRead;
}

static AutoMapFileResult ReadLink(std::istream &in, const AutoMapExplored &dungeon,
                                  AutoMapPlayerNotes *notes)
{
  uint32_t level, x, y, side, targetLevel, targetX, targetY;
  if (!ReadNumber(in, 1, &level)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &x)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &y)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &side)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &targetLevel)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &targetX)) return AutoMapFileIsTruncated;
  if (!ReadNumber(in, 1, &targetY)) return AutoMapFileIsTruncated;
  AutoMapLinkSource source = {{(int)level, (int)x, (int)y}, side == NO_SIDE_BYTE ? -1 : (int)side};
  AutoMapCell target = {(int)targetLevel, (int)targetX, (int)targetY};
  if (!CellIsInDungeon(dungeon, source.cell) || !CellIsInDungeon(dungeon, target))
  {
    return AutoMapFileHasOtherLevels;
  }
  if (side != NO_SIDE_BYTE && side >= (uint32_t)AUTOMAP_SIDE_COUNT) return AutoMapFileHasOtherLevels;
  if (targetLevel != level) return AutoMapFileHasOtherLevels;
  notes->AddLinkTarget(source, target);
  return AutoMapFileRead;
}

// Neither count is trusted for allocation: a damaged count just meets the end of the file
static AutoMapFileResult ReadNotes(std::istream &in, const AutoMapExplored &dungeon,
                                   AutoMapPlayerNotes *notes)
{
  uint32_t pitCount;
  if (!ReadNumber(in, 4, &pitCount)) return AutoMapFileIsTruncated;
  for (uint32_t i = 0; i < pitCount; i++)
  {
    AutoMapFileResult result = ReadPit(in, dungeon, notes);
    if (result != AutoMapFileRead) return result;
  }
  uint32_t linkCount;
  if (!ReadNumber(in, 4, &linkCount)) return AutoMapFileIsTruncated;
  for (uint32_t i = 0; i < linkCount; i++)
  {
    AutoMapFileResult result = ReadLink(in, dungeon, notes);
    if (result != AutoMapFileRead) return result;
  }
  return AutoMapFileRead;
}

// The count is not trusted for allocation: a damaged count just meets the end of the file
static AutoMapFileResult ReadDamage(std::istream &in, AutoMapMonsterDamage *damage)
{
  uint32_t groupCount;
  if (!ReadNumber(in, 4, &groupCount)) return AutoMapFileIsTruncated;
  for (uint32_t i = 0; i < groupCount; i++)
  {
    uint32_t record;
    if (!ReadNumber(in, 4, &record)) return AutoMapFileIsTruncated;
    uint32_t dealt[AUTOMAP_DAMAGE_MEMBERS];
    for (int member = 0; member < AUTOMAP_DAMAGE_MEMBERS; member++)
    {
      if (!ReadNumber(in, 2, &dealt[member])) return AutoMapFileIsTruncated;
    }
    if (record == AUTOMAP_NO_RECORD) return AutoMapFileHasOtherLevels;
    for (int member = 0; member < AUTOMAP_DAMAGE_MEMBERS; member++)
    {
      damage->Credit(record, member, (int)dealt[member]);
    }
  }
  return AutoMapFileRead;
}

// The length is not trusted for allocation: the block is taken off the stream in pieces, so a
// damaged length just meets the end of the file
static AutoMapFileResult ReadStatistics(std::istream &in, uint32_t version, HeroStats *stats,
                                        HeroStatsOrigins *origins)
{
  uint32_t blockLength;
  if (!ReadNumber(in, 4, &blockLength)) return AutoMapFileIsTruncated;
  std::vector<uint8_t> block;
  while (block.size() < blockLength)
  {
    size_t piece = blockLength - block.size();
    if (piece > 65536) piece = 65536;
    size_t at = block.size();
    block.resize(at + piece);
    in.read((char *)&block[at], (std::streamsize)piece);
    if (!in.good()) return AutoMapFileIsTruncated;
  }
  if (block.size() < 4) return AutoMapFileIsTruncated;
  uint32_t statsLength = (uint32_t)block[0] | (uint32_t)block[1] << 8 | (uint32_t)block[2] << 16
                         | (uint32_t)block[3] << 24;
  if (statsLength > block.size() - 4) return AutoMapFileIsTruncated;
  const uint8_t *statsBytes = block.data() + 4;
  const uint8_t *originsBytes = statsBytes + statsLength;
  if (!stats->Deserialize(statsBytes, statsLength, (int)version)) return AutoMapFileIsTruncated;
  if (!origins->Deserialize(originsBytes, block.size() - 4 - statsLength)) return AutoMapFileIsTruncated;
  return AutoMapFileRead;
}

AutoMapFileResult AutoMapFile::Read(std::istream &in, AutoMapExplored *explored,
                                    AutoMapKnowledge *knowledge, AutoMapItemMemory *items,
                                    bool *hadItemMemory, AutoMapCellMemory *cells,
                                    bool *hadCellMemory, AutoMapPlayerNotes *notes,
                                    AutoMapMonsterDamage *damage, HeroStats *stats,
                                    HeroStatsOrigins *origins) const
{
  uint32_t version;
  AutoMapSaveSignature storedSignature;
  AutoMapFileResult result = ReadHeader(in, &version, &storedSignature);
  if (result != AutoMapFileRead) return result;
  if (storedSignature.first != m_saveSignature.first
      || storedSignature.second != m_saveSignature.second)
  {
    return AutoMapFileBelongsToAnotherSave;
  }
  uint32_t levelCount;
  if (!ReadNumber(in, 2, &levelCount)) return AutoMapFileIsTruncated;
  if ((int)levelCount != explored->LevelCount()) return AutoMapFileHasOtherLevels;
  AutoMapExplored readCells = *explored;
  for (int level = 0; level < readCells.LevelCount(); level++)
  {
    result = ReadLevel(in, version, level, &readCells);
    if (result != AutoMapFileRead) return result;
  }
  // Before version 11 a painted rim cell was kept on the floor beside it
  if (version < FIRST_VERSION_WITH_RIMS) readCells.TakeRimMarksFromTheEdge();
  // Before version 12 an explored cell showed its floor picture, so what was explored stays seen
  if (version < FIRST_VERSION_WITH_SEEN_FLOORS) readCells.MarkFloorsOfExploredCells();
  AutoMapKnowledge readFacts;
  if (version >= FIRST_VERSION_WITH_FACES)
  {
    result = ReadFacts(in, readCells, &readFacts);
    if (result != AutoMapFileRead) return result;
  }
  else
  {
    // Before faces were recorded, every wall beside an explored cell counts as seen
    readCells.MarkFacesTowardsExploredCells();
  }
  AutoMapItemMemory readItems;
  bool itemsWereStored = version >= FIRST_VERSION_WITH_ITEMS;
  if (itemsWereStored)
  {
    result = ReadItems(in, version, readCells, &readItems);
    if (result != AutoMapFileRead) return result;
  }
  AutoMapCellMemory readLooks;
  bool looksWereStored = version >= FIRST_VERSION_WITH_CELL_LOOKS;
  if (looksWereStored)
  {
    result = ReadLooks(in, readCells, &readLooks);
    if (result != AutoMapFileRead) return result;
  }
  AutoMapPlayerNotes readNotes;
  bool notesWereStored = version >= FIRST_VERSION_WITH_NOTES;
  if (notesWereStored)
  {
    result = ReadNotes(in, readCells, &readNotes);
    if (result != AutoMapFileRead) return result;
  }
  AutoMapMonsterDamage readDamage;
  bool damageWasStored = version >= FIRST_VERSION_WITH_DAMAGE;
  if (damageWasStored)
  {
    result = ReadDamage(in, &readDamage);
    if (result != AutoMapFileRead) return result;
  }
  HeroStats readStats;
  HeroStatsOrigins readOrigins;
  if (version >= FIRST_VERSION_WITH_STATISTICS)
  {
    result = ReadStatistics(in, version, &readStats, &readOrigins);
    if (result != AutoMapFileRead) return result;
  }
  *explored = readCells;
  *knowledge = readFacts;
  if (itemsWereStored) *items = readItems;
  if (looksWereStored) *cells = readLooks;
  if (notesWereStored) *notes = readNotes;
  if (damageWasStored) *damage = readDamage;
  // A file before version 13 has no statistics: the stores come out empty
  *stats = readStats;
  *origins = readOrigins;
  *hadItemMemory = itemsWereStored;
  *hadCellMemory = looksWereStored;
  return AutoMapFileRead;
}

const char *AutoMapFile_Describe(AutoMapFileResult result)
{
  switch (result)
  {
  case AutoMapFileRead:                 return "read";
  case AutoMapFileIsNotAMapFile:        return "not a map file";
  case AutoMapFileHasUnknownVersion:    return "unknown format version";
  case AutoMapFileBelongsToAnotherSave: return "written for another saved game";
  case AutoMapFileHasOtherLevels:       return "level sizes do not match the dungeon";
  case AutoMapFileIsTruncated:          return "file is incomplete";
  }
  return "unknown problem";
}
