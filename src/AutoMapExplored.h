#ifndef AUTOMAPEXPLORED_H
#define AUTOMAPEXPLORED_H

#include <cstddef>
#include <cstdint>
#include <vector>

struct AutoMapLevelSize
{
  int width;
  int height;
};

struct AutoMapCell
{
  int level;
  int x;
  int y;
};

// The bit sets kept per cell: explored and seen floor (one bit each) and the four sets of faces
// (four bits, one per side). New sets go last: the .map file stores the rims in this order.
enum AutoMapBitSet
{
  AutoMapExploredBits,
  AutoMapSeenFaceBits,
  AutoMapGlimpsedFaceBits,
  AutoMapReadTextBits,
  AutoMapLookedIntoBits,
  AutoMapSeenFloorBits
};
const int AUTOMAP_BIT_SET_COUNT = 6;

// Remembers which cells of each dungeon level the party has seen: one bit per cell. Every level
// also has a rim, the ring of cells one step outside it: the rock the engine makes up there when
// the 3D view looks past the edge, with its decorations. Its cells are kept like the level's own.
class AutoMapExplored
{
public:
  // Forgets everything and takes the shape of a dungeon
  void Reset(const std::vector<AutoMapLevelSize> &levelSizes);
  // Forgets the cells and faces of one level and keeps its shape; other levels are untouched
  void ForgetLevel(int level);
  int LevelCount(void) const;
  AutoMapLevelSize SizeOfLevel(int level) const;
  // Cells outside the dungeon and its rims are ignored: the 3D view also looks farther past the
  // edge of a level
  void Mark(const AutoMapCell &cell);
  bool IsExplored(const AutoMapCell &cell) const;
  // One bit per cell, row by row, lowest bit first. This is the layout stored in the .map file.
  const std::vector<uint8_t> &BitsOfLevel(int level) const;
  void ReplaceBitsOfLevel(int level, const std::vector<uint8_t> &bits);
  static size_t ByteCountFor(const AutoMapLevelSize &size);

  // Wall faces the 3D view has shown. side is the direction from the wall cell to the floor in
  // front of the face: 0 north, 1 east, 2 south, 3 west (the numbers of AutoMapDirection).
  // Cells outside the dungeon and its rims and sides outside 0..3 are ignored.
  void MarkFace(const AutoMapCell &cell, int side);
  bool FaceIsSeen(const AutoMapCell &cell, int side) const;
  // Four bits per cell, row by row, lowest bit first: bit 4 * (y * width + x) + side.
  const std::vector<uint8_t> &FaceBitsOfLevel(int level) const;
  void ReplaceFaceBitsOfLevel(int level, const std::vector<uint8_t> &bits);
  static size_t FaceByteCountFor(const AutoMapLevelSize &size);
  // Faces on which something was glimpsed in the dark, beyond the light: the map shows that
  // something is there, not what. A face seen in the light is no glimpse. Same layout.
  void MarkGlimpsedFace(const AutoMapCell &cell, int side);
  bool FaceIsGlimpsed(const AutoMapCell &cell, int side) const;
  const std::vector<uint8_t> &GlimpsedFaceBitsOfLevel(int level) const;
  void ReplaceGlimpsedFaceBitsOfLevel(int level, const std::vector<uint8_t> &bits);
  // Faces whose wall text the party has read: the 3D view draws the words only on the wall
  // straight ahead, from afar it shows scribbles. Same layout.
  void MarkTextRead(const AutoMapCell &cell, int side);
  bool TextIsRead(const AutoMapCell &cell, int side) const;
  const std::vector<uint8_t> &ReadTextBitsOfLevel(int level) const;
  void ReplaceReadTextBitsOfLevel(int level, const std::vector<uint8_t> &bits);
  // Faces whose alcove the party has looked into: the 3D view draws what lies in an alcove only
  // on a wall that faces the party, not on one seen from the side. Same layout.
  void MarkAlcoveLookedInto(const AutoMapCell &cell, int side);
  bool AlcoveIsLookedInto(const AutoMapCell &cell, int side) const;
  const std::vector<uint8_t> &LookedIntoBitsOfLevel(int level) const;
  void ReplaceLookedIntoBitsOfLevel(int level, const std::vector<uint8_t> &bits);
  // Cells whose floor picture (a puddle, moss, a pad) the 3D view has shown so that it was
  // noticed: exploring a cell only means some strip of it was on screen. One bit per cell.
  void MarkFloorSeen(const AutoMapCell &cell);
  bool FloorIsSeen(const AutoMapCell &cell) const;
  const std::vector<uint8_t> &FloorBitsOfLevel(int level) const;
  void ReplaceFloorBitsOfLevel(int level, const std::vector<uint8_t> &bits);
  // Every explored cell's floor counts as seen, rims included: the rule of files from before
  // floors were measured
  void MarkFloorsOfExploredCells(void);

  // Marks every face whose neighbour on that side is explored: the rule of version-1 files
  void MarkFacesTowardsExploredCells(void);

  // The rim's bits of one set: the row above the level, corners included, then the row below,
  // then the column left of it and the column right, top to bottom; per cell 1 bit for
  // AutoMapExploredBits and AutoMapSeenFloorBits, 4 (one per side) for the others, lowest bit
  // first
  const std::vector<uint8_t> &RimBitsOfLevel(int level, AutoMapBitSet set) const;
  void ReplaceRimBitsOfLevel(int level, AutoMapBitSet set, const std::vector<uint8_t> &bits);
  static size_t RimByteCountFor(const AutoMapLevelSize &size, AutoMapBitSet set);
  // Files before the rim was kept marked a painted rim cell on the floor beside it, as that
  // floor's seen face towards it: those marks become explored rim cells
  void TakeRimMarksFromTheEdge(void);

private:
  struct Level
  {
    AutoMapLevelSize size;
    std::vector<uint8_t> bits;
    std::vector<uint8_t> faceBits;
    std::vector<uint8_t> glimpsedFaceBits;
    std::vector<uint8_t> readTextBits;
    std::vector<uint8_t> lookedIntoBits;
    std::vector<uint8_t> floorBits;
    std::vector<uint8_t> rimBits[AUTOMAP_BIT_SET_COUNT];
  };
  bool Contains(const AutoMapCell &cell) const;
  // The byte array and bit that hold cell's bit of set (side 0..3 for a face set); NULL outside
  const std::vector<uint8_t> *BitOf(const AutoMapCell &cell, AutoMapBitSet set, int side,
                                    size_t *bit) const;
  std::vector<uint8_t> *BitOf(const AutoMapCell &cell, AutoMapBitSet set, int side, size_t *bit);
  void SetBit(const AutoMapCell &cell, AutoMapBitSet set, int side);
  bool GetBit(const AutoMapCell &cell, AutoMapBitSet set, int side) const;
  std::vector<Level> m_levels;
};

#endif
