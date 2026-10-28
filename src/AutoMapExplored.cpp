// AutoMapExplored.cpp : remembers which cells of each level the party has seen.
#include "AutoMapExplored.h"

static size_t BitIndex(const AutoMapLevelSize &size, const AutoMapCell &cell)
{
  return (size_t)cell.y * size.width + cell.x;
}

static bool IsInside(const AutoMapLevelSize &size, const AutoMapCell &cell)
{
  return cell.x >= 0 && cell.x < size.width && cell.y >= 0 && cell.y < size.height;
}

// The rim: the row above and below the level, corners included, then the column left and right
static size_t RimCellCount(const AutoMapLevelSize &size)
{
  return 2 * ((size_t)size.width + 2) + 2 * (size_t)size.height;
}

// The place of a cell of the rim; false for a cell inside the level or farther out
static bool RimIndex(const AutoMapLevelSize &size, const AutoMapCell &cell, size_t *index)
{
  size_t across = (size_t)size.width + 2;
  if (cell.x < -1 || cell.x > size.width || cell.y < -1 || cell.y > size.height) return false;
  if (cell.y == -1) *index = (size_t)(cell.x + 1);
  else if (cell.y == size.height) *index = across + (size_t)(cell.x + 1);
  else if (cell.x == -1) *index = 2 * across + (size_t)cell.y;
  else if (cell.x == size.width) *index = 2 * across + (size_t)size.height + (size_t)cell.y;
  else return false;
  return true;
}

size_t AutoMapExplored::ByteCountFor(const AutoMapLevelSize &size)
{
  return ((size_t)size.width * size.height + 7) / 8;
}

size_t AutoMapExplored::RimByteCountFor(const AutoMapLevelSize &size, AutoMapBitSet set)
{
  size_t bitsPerCell = set == AutoMapExploredBits ? 1 : 4;
  return (RimCellCount(size) * bitsPerCell + 7) / 8;
}

void AutoMapExplored::Reset(const std::vector<AutoMapLevelSize> &levelSizes)
{
  m_levels.clear();
  for (size_t i = 0; i < levelSizes.size(); i++)
  {
    Level level;
    level.size = levelSizes[i];
    level.bits.assign(ByteCountFor(level.size), 0);
    level.faceBits.assign(FaceByteCountFor(level.size), 0);
    level.glimpsedFaceBits.assign(FaceByteCountFor(level.size), 0);
    level.readTextBits.assign(FaceByteCountFor(level.size), 0);
    level.lookedIntoBits.assign(FaceByteCountFor(level.size), 0);
    for (int set = 0; set < AUTOMAP_BIT_SET_COUNT; set++)
    {
      level.rimBits[set].assign(RimByteCountFor(level.size, (AutoMapBitSet)set), 0);
    }
    m_levels.push_back(level);
  }
}

void AutoMapExplored::ForgetLevel(int level)
{
  if (level < 0 || level >= LevelCount()) return;
  Level &forgotten = m_levels[level];
  forgotten.bits.assign(forgotten.bits.size(), 0);
  forgotten.faceBits.assign(forgotten.faceBits.size(), 0);
  forgotten.glimpsedFaceBits.assign(forgotten.glimpsedFaceBits.size(), 0);
  forgotten.readTextBits.assign(forgotten.readTextBits.size(), 0);
  forgotten.lookedIntoBits.assign(forgotten.lookedIntoBits.size(), 0);
  for (int set = 0; set < AUTOMAP_BIT_SET_COUNT; set++)
  {
    forgotten.rimBits[set].assign(forgotten.rimBits[set].size(), 0);
  }
}

int AutoMapExplored::LevelCount(void) const
{
  return (int)m_levels.size();
}

AutoMapLevelSize AutoMapExplored::SizeOfLevel(int level) const
{
  return m_levels[level].size;
}

bool AutoMapExplored::Contains(const AutoMapCell &cell) const
{
  if (cell.level < 0 || cell.level >= LevelCount()) return false;
  const AutoMapLevelSize &size = m_levels[cell.level].size;
  size_t index;
  return IsInside(size, cell) || RimIndex(size, cell, &index);
}

const std::vector<uint8_t> *AutoMapExplored::BitOf(const AutoMapCell &cell, AutoMapBitSet set,
                                                   int side, size_t *bit) const
{
  if (!Contains(cell)) return NULL;
  if (set != AutoMapExploredBits && (side < 0 || side > 3)) return NULL;
  const Level &level = m_levels[cell.level];
  size_t index;
  if (RimIndex(level.size, cell, &index))
  {
    *bit = set == AutoMapExploredBits ? index : 4 * index + side;
    return &level.rimBits[set];
  }
  index = BitIndex(level.size, cell);
  *bit = set == AutoMapExploredBits ? index : 4 * index + side;
  switch (set)
  {
  case AutoMapExploredBits:      return &level.bits;
  case AutoMapSeenFaceBits:     return &level.faceBits;
  case AutoMapGlimpsedFaceBits: return &level.glimpsedFaceBits;
  case AutoMapReadTextBits:     return &level.readTextBits;
  case AutoMapLookedIntoBits:    return &level.lookedIntoBits;
  }
  return NULL;
}

std::vector<uint8_t> *AutoMapExplored::BitOf(const AutoMapCell &cell, AutoMapBitSet set, int side,
                                             size_t *bit)
{
  const AutoMapExplored *self = this;
  return const_cast<std::vector<uint8_t> *>(self->BitOf(cell, set, side, bit));
}

void AutoMapExplored::SetBit(const AutoMapCell &cell, AutoMapBitSet set, int side)
{
  size_t bit;
  std::vector<uint8_t> *bits = BitOf(cell, set, side, &bit);
  if (bits != NULL) (*bits)[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

bool AutoMapExplored::GetBit(const AutoMapCell &cell, AutoMapBitSet set, int side) const
{
  size_t bit;
  const std::vector<uint8_t> *bits = BitOf(cell, set, side, &bit);
  return bits != NULL && ((*bits)[bit / 8] & (1 << (bit % 8))) != 0;
}

const std::vector<uint8_t> &AutoMapExplored::RimBitsOfLevel(int level, AutoMapBitSet set) const
{
  return m_levels[level].rimBits[set];
}

void AutoMapExplored::ReplaceRimBitsOfLevel(int level, AutoMapBitSet set, const std::vector<uint8_t> &bits)
{
  m_levels[level].rimBits[set] = bits;
}

void AutoMapExplored::TakeRimMarksFromTheEdge(void)
{
  static const int dx[4] = {0, 1, 0, -1};
  static const int dy[4] = {-1, 0, 1, 0};
  for (int l = 0; l < LevelCount(); l++)
  {
    const AutoMapLevelSize size = m_levels[l].size;
    for (int y = 0; y < size.height; y++)
    {
      for (int x = 0; x < size.width; x++)
      {
        for (int side = 0; side < 4; side++)
        {
          AutoMapCell beyond = {l, x + dx[side], y + dy[side]};
          if (IsInside(size, beyond)) continue;
          AutoMapCell edge = {l, x, y};
          size_t bit;
          std::vector<uint8_t> *faces = BitOf(edge, AutoMapSeenFaceBits, side, &bit);
          uint8_t mask = (uint8_t)(1 << (bit % 8));
          if (((*faces)[bit / 8] & mask) == 0) continue;
          (*faces)[bit / 8] &= (uint8_t)~mask;
          Mark(beyond);
        }
      }
    }
  }
}

void AutoMapExplored::Mark(const AutoMapCell &cell)
{
  SetBit(cell, AutoMapExploredBits, 0);
}

bool AutoMapExplored::IsExplored(const AutoMapCell &cell) const
{
  return GetBit(cell, AutoMapExploredBits, 0);
}

const std::vector<uint8_t> &AutoMapExplored::BitsOfLevel(int level) const
{
  return m_levels[level].bits;
}

void AutoMapExplored::ReplaceBitsOfLevel(int level, const std::vector<uint8_t> &bits)
{
  m_levels[level].bits = bits;
}

size_t AutoMapExplored::FaceByteCountFor(const AutoMapLevelSize &size)
{
  return ((size_t)size.width * size.height * 4 + 7) / 8;
}

void AutoMapExplored::MarkFace(const AutoMapCell &cell, int side)
{
  SetBit(cell, AutoMapSeenFaceBits, side);
}

bool AutoMapExplored::FaceIsSeen(const AutoMapCell &cell, int side) const
{
  return GetBit(cell, AutoMapSeenFaceBits, side);
}

const std::vector<uint8_t> &AutoMapExplored::FaceBitsOfLevel(int level) const
{
  return m_levels[level].faceBits;
}

void AutoMapExplored::ReplaceFaceBitsOfLevel(int level, const std::vector<uint8_t> &bits)
{
  m_levels[level].faceBits = bits;
}

void AutoMapExplored::MarkGlimpsedFace(const AutoMapCell &cell, int side)
{
  SetBit(cell, AutoMapGlimpsedFaceBits, side);
}

bool AutoMapExplored::FaceIsGlimpsed(const AutoMapCell &cell, int side) const
{
  return !FaceIsSeen(cell, side) && GetBit(cell, AutoMapGlimpsedFaceBits, side);
}

const std::vector<uint8_t> &AutoMapExplored::GlimpsedFaceBitsOfLevel(int level) const
{
  return m_levels[level].glimpsedFaceBits;
}

void AutoMapExplored::ReplaceGlimpsedFaceBitsOfLevel(int level, const std::vector<uint8_t> &bits)
{
  m_levels[level].glimpsedFaceBits = bits;
}

void AutoMapExplored::MarkTextRead(const AutoMapCell &cell, int side)
{
  SetBit(cell, AutoMapReadTextBits, side);
}

bool AutoMapExplored::TextIsRead(const AutoMapCell &cell, int side) const
{
  return GetBit(cell, AutoMapReadTextBits, side);
}

const std::vector<uint8_t> &AutoMapExplored::ReadTextBitsOfLevel(int level) const
{
  return m_levels[level].readTextBits;
}

void AutoMapExplored::ReplaceReadTextBitsOfLevel(int level, const std::vector<uint8_t> &bits)
{
  m_levels[level].readTextBits = bits;
}

void AutoMapExplored::MarkAlcoveLookedInto(const AutoMapCell &cell, int side)
{
  SetBit(cell, AutoMapLookedIntoBits, side);
}

bool AutoMapExplored::AlcoveIsLookedInto(const AutoMapCell &cell, int side) const
{
  return GetBit(cell, AutoMapLookedIntoBits, side);
}

const std::vector<uint8_t> &AutoMapExplored::LookedIntoBitsOfLevel(int level) const
{
  return m_levels[level].lookedIntoBits;
}

void AutoMapExplored::ReplaceLookedIntoBitsOfLevel(int level, const std::vector<uint8_t> &bits)
{
  m_levels[level].lookedIntoBits = bits;
}

void AutoMapExplored::MarkFacesTowardsExploredCells(void)
{
  // Neighbour offsets by side: north, east, south, west
  static const int dx[4] = {0, 1, 0, -1};
  static const int dy[4] = {-1, 0, 1, 0};
  for (int l = 0; l < LevelCount(); l++)
  {
    for (int y = 0; y < m_levels[l].size.height; y++)
    {
      for (int x = 0; x < m_levels[l].size.width; x++)
      {
        for (int side = 0; side < 4; side++)
        {
          if (IsExplored(AutoMapCell{l, x + dx[side], y + dy[side]})) MarkFace(AutoMapCell{l, x, y}, side);
        }
      }
    }
  }
}
