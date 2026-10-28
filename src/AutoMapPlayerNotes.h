#ifndef AUTOMAPPLAYERNOTES_H
#define AUTOMAPPLAYERNOTES_H

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "AutoMapExplored.h"

// Where a player link starts: a floor tile (side -1) or one wall face of a cell
struct AutoMapLinkSource
{
  AutoMapCell cell;   // level coordinates
  int side;           // AutoMapDirection of a wall face, -1 for a floor source
};

// What the player marks by hand: hidden pits and links between cells. Never checked against
// the dungeon and never read by the press, puzzle, relay or chain logic.
class AutoMapPlayerNotes
{
public:
  void Forget(void);
  void ForgetLevel(int level);
  void SetPit(const AutoMapCell &cell, bool open);
  void ClearPit(const AutoMapCell &cell);
  // -1 no mark, 0 closed, 1 open
  int PitAt(const AutoMapCell &cell) const;
  // Adds the target, or removes it when present; true when it is now a target
  bool ToggleLinkTarget(const AutoMapLinkSource &source, const AutoMapCell &target);
  // Adds the target; one already there stays, so adding twice is adding once (the .map loader)
  void AddLinkTarget(const AutoMapLinkSource &source, const AutoMapCell &target);
  // Removes every link that starts on the cell, from its floor or any face, or ends on it
  void RemoveLinksOf(const AutoMapCell &cell);
  std::vector<AutoMapCell> LinksFrom(const AutoMapLinkSource &source) const;
  // Every (source, target) on the level, sources ordered (y, x, side), targets (y, x)
  std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > LinksOfLevel(int level) const;
  // Everything, for the .map file, in (level, y, x[, side]) order
  std::vector<std::pair<AutoMapCell, bool> > Pits(void) const;
  std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > Links(void) const;

private:
  // Plain values ordered by level, y, x and side; a floor source (-1) sorts before the faces
  // of its cell. A link target lies on the source's level.
  struct Key
  {
    int level;
    int y;
    int x;
    int side;
    bool operator<(const Key &other) const;
  };
  typedef std::map<Key, bool> PitMap;
  typedef std::map<Key, std::set<Key> > LinkMap;
  static Key KeyOf(const AutoMapCell &cell, int side);
  static AutoMapCell CellOf(const Key &key);
  static AutoMapLinkSource SourceOf(const Key &key);
  static void Collect(const LinkMap::const_iterator &from, const LinkMap::const_iterator &to,
                      std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > *out);
  PitMap m_pits;
  LinkMap m_links;
};

#endif
