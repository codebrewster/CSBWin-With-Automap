#ifndef AUTOMAPCELLMEMORY_H
#define AUTOMAPCELLMEMORY_H

#include <map>
#include <utility>
#include <vector>

#include "AutoMapExplored.h"
#include "AutoMapScene.h"

// How the party last saw each explored cell. Doors close, pits open and walls change out of
// sight; the map shows what was seen, not what is true now.
class AutoMapCellMemory
{
public:
  void Forget(void);
  void ForgetLevel(int level);
  // The cell is seen: a new cell takes the whole look, a known one only its kind, door, pit
  // and floor fields (wall sides refresh per face, SeeSide)
  void See(const AutoMapCell &cell, const AutoMapCellLook &look);
  // One face of a known cell is seen; ignored for a cell never seen
  void SeeSide(const AutoMapCell &cell, int side, int picture);
  // Stores the whole look, only for a cell not yet remembered (first impression, migration)
  void RememberIfNew(const AutoMapCell &cell, const AutoMapCellLook &look);
  // Stores the whole look unconditionally (file load)
  void Restore(const AutoMapCell &cell, const AutoMapCellLook &look);
  bool LookOf(const AutoMapCell &cell, AutoMapCellLook *look) const;
  // In (level, y, x) order, for the .map file
  std::vector<std::pair<AutoMapCell, AutoMapCellLook> > Looks(void) const;

private:
  // A lookup runs for every cell in view on every tick: each level keeps its looks in a grid
  // that grows to take any cell seen, rim cells (-1) included; a plain value, copied whole
  struct LevelLooks
  {
    int x0, y0, width, height;            // the grid's first cell and size
    std::vector<AutoMapCellLook> looks;   // row by row
    std::vector<unsigned char> known;     // 1 where a look is stored
  };
  const AutoMapCellLook *Find(const AutoMapCell &cell) const;
  // The look stored for cell, made room for if none; *isNew tells whether it was there
  AutoMapCellLook *Slot(const AutoMapCell &cell, bool *isNew);
  std::vector<LevelLooks> m_levels;       // by level
  // Cells of a level number no dungeon has (negative): in (level, y, x) order like the grids
  std::map<std::pair<int, std::pair<int, int> >, AutoMapCellLook> m_odd;
};

#endif
