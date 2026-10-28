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
  // Ordered by level, then y, then x; a plain value, as a lookup runs per cell and tick
  struct Key
  {
    int level;
    int y;
    int x;
    bool operator<(const Key &other) const;
  };
  typedef std::map<Key, AutoMapCellLook> LookMap;
  static Key KeyOf(const AutoMapCell &cell);
  LookMap m_looks;
};

#endif
