// AutoMapCellMemory.cpp : the look the map remembers for each explored cell.
#include "AutoMapCellMemory.h"

#include <climits>

bool AutoMapCellMemory::Key::operator<(const Key &other) const
{
  if (level != other.level) return level < other.level;
  if (y != other.y) return y < other.y;
  return x < other.x;
}

AutoMapCellMemory::Key AutoMapCellMemory::KeyOf(const AutoMapCell &cell)
{
  Key key = {cell.level, cell.y, cell.x};
  return key;
}

void AutoMapCellMemory::Forget(void)
{
  m_looks.clear();
}

void AutoMapCellMemory::ForgetLevel(int level)
{
  Key first = {level, INT_MIN, INT_MIN};
  Key past = {level + 1, INT_MIN, INT_MIN};
  m_looks.erase(m_looks.lower_bound(first), m_looks.lower_bound(past));
}

void AutoMapCellMemory::See(const AutoMapCell &cell, const AutoMapCellLook &look)
{
  LookMap::iterator found = m_looks.find(KeyOf(cell));
  if (found == m_looks.end())
  {
    m_looks[KeyOf(cell)] = look;
    return;
  }
  // The faces of a wall are seen one by one, so only SeeSide refreshes them
  AutoMapCellLook &known = found->second;
  known.kind = look.kind;
  known.doorState = look.doorState;
  known.pitIsOpen = look.pitIsOpen;
  known.pitIsHidden = look.pitIsHidden;
  known.floorPicture = look.floorPicture;
}

void AutoMapCellMemory::SeeSide(const AutoMapCell &cell, int side, int picture)
{
  if (side < 0 || side >= AUTOMAP_SIDE_COUNT) return;
  LookMap::iterator found = m_looks.find(KeyOf(cell));
  if (found != m_looks.end()) found->second.sidePictures[side] = picture;
}

void AutoMapCellMemory::RememberIfNew(const AutoMapCell &cell, const AutoMapCellLook &look)
{
  m_looks.insert(std::make_pair(KeyOf(cell), look));
}

void AutoMapCellMemory::Restore(const AutoMapCell &cell, const AutoMapCellLook &look)
{
  m_looks[KeyOf(cell)] = look;
}

bool AutoMapCellMemory::LookOf(const AutoMapCell &cell, AutoMapCellLook *look) const
{
  LookMap::const_iterator found = m_looks.find(KeyOf(cell));
  if (found == m_looks.end()) return false;
  *look = found->second;
  return true;
}

std::vector<std::pair<AutoMapCell, AutoMapCellLook> > AutoMapCellMemory::Looks(void) const
{
  std::vector<std::pair<AutoMapCell, AutoMapCellLook> > looks;
  for (LookMap::const_iterator it = m_looks.begin(); it != m_looks.end(); ++it)
  {
    AutoMapCell cell = {it->first.level, it->first.x, it->first.y};
    looks.push_back(std::make_pair(cell, it->second));
  }
  return looks;
}
