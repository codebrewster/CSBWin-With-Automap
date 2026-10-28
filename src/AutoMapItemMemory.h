#ifndef AUTOMAPITEMMEMORY_H
#define AUTOMAPITEMMEMORY_H

#include <vector>

#include "AutoMapExplored.h"
#include "AutoMapKnowledge.h"

// An item on the floor: its record and the object description it was drawn with
struct AutoMapFloorItemTag
{
  AutoMapRecordId record;
  int descIndex;
};

struct AutoMapRememberedItem
{
  AutoMapRecordId record;
  int descIndex;
  AutoMapCell cell;   // level coordinates
  // Seen only as something in the dark, on a cell the light did not reach: the map shows that
  // something lies there, not what. Last, so that aggregate initialisation leaves it false.
  bool isGlimpsed;
};

// Where the 3D view last showed each floor item. Unlike facts, items come and go: picking one up
// or finding it gone from a cell the party sees makes the map forget it.
class AutoMapItemMemory
{
public:
  void Forget(void);
  void ForgetLevel(int level);
  // Sets the item's cell; an item seen on another cell moves there and goes last. Seen on its
  // cell again, it is glimpsed only while every look was a glimpse.
  void See(const AutoMapRememberedItem &item);
  void ForgetItem(AutoMapRecordId record);
  // Same record, description and cell
  bool Remembers(const AutoMapRememberedItem &item) const;
  // Remembers it, and not as a mere glimpse
  bool Identifies(const AutoMapRememberedItem &item) const;
  // Seen somewhere, with this description and not as a mere glimpse, wherever it lies now
  bool HasSeen(const AutoMapFloorItemTag &tag) const;
  // In the order seen
  std::vector<AutoMapRememberedItem> ItemsOn(const AutoMapCell &cell) const;
  // The party sees the cell: an item not among itemsThere, or there with another description, is forgotten
  void KeepOnlyItemsStillOn(const AutoMapCell &cell, const std::vector<AutoMapFloorItemTag> &itemsThere);
  // Every item in the order seen, for the .map file
  std::vector<AutoMapRememberedItem> Items(void) const;

private:
  // A few hundred entries at most, so a vector in the order seen is enough
  std::vector<AutoMapRememberedItem> m_items;
};

#endif
