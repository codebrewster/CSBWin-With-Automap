// AutoMapCellMemory.cpp : the look the map remembers for each explored cell.
#include "AutoMapCellMemory.h"

#include <algorithm>

const AutoMapCellLook *AutoMapCellMemory::Find(const AutoMapCell &cell) const
{
  if (cell.level < 0)
  {
    std::map<std::pair<int, std::pair<int, int> >, AutoMapCellLook>::const_iterator found =
      m_odd.find(std::make_pair(cell.level, std::make_pair(cell.y, cell.x)));
    return found == m_odd.end() ? NULL : &found->second;
  }
  if ((size_t)cell.level >= m_levels.size()) return NULL;
  const LevelLooks &level = m_levels[(size_t)cell.level];
  int x = cell.x - level.x0, y = cell.y - level.y0;
  if (x < 0 || y < 0 || x >= level.width || y >= level.height) return NULL;
  size_t at = (size_t)y * level.width + x;
  return level.known[at] ? &level.looks[at] : NULL;
}

AutoMapCellLook *AutoMapCellMemory::Slot(const AutoMapCell &cell, bool *isNew)
{
  if (cell.level < 0)
  {
    std::pair<int, std::pair<int, int> > key = std::make_pair(cell.level, std::make_pair(cell.y, cell.x));
    *isNew = m_odd.find(key) == m_odd.end();
    return &m_odd[key];
  }
  if ((size_t)cell.level >= m_levels.size())
  {
    LevelLooks empty = {0, 0, 0, 0, std::vector<AutoMapCellLook>(), std::vector<unsigned char>()};
    m_levels.resize((size_t)cell.level + 1, empty);
  }
  LevelLooks &level = m_levels[(size_t)cell.level];
  int x = cell.x - level.x0, y = cell.y - level.y0;
  if (level.width == 0 || x < 0 || y < 0 || x >= level.width || y >= level.height)
  {
    // Grow to take the cell, a level's usual size and its rim at once
    int x0 = level.width == 0 ? std::min(cell.x, -1) : std::min(level.x0, cell.x);
    int y0 = level.width == 0 ? std::min(cell.y, -1) : std::min(level.y0, cell.y);
    int x1 = level.width == 0 ? std::max(cell.x + 1, 34) : std::max(level.x0 + level.width, cell.x + 1);
    int y1 = level.width == 0 ? std::max(cell.y + 1, 34) : std::max(level.y0 + level.height, cell.y + 1);
    LevelLooks grown = {x0, y0, x1 - x0, y1 - y0,
                        std::vector<AutoMapCellLook>((size_t)(x1 - x0) * (y1 - y0)),
                        std::vector<unsigned char>((size_t)(x1 - x0) * (y1 - y0), 0)};
    for (int oy = 0; oy < level.height; oy++)
      for (int ox = 0; ox < level.width; ox++)
      {
        size_t from = (size_t)oy * level.width + ox;
        if (!level.known[from]) continue;
        size_t to = (size_t)(oy + level.y0 - y0) * grown.width + (ox + level.x0 - x0);
        grown.looks[to] = level.looks[from];
        grown.known[to] = 1;
      }
    level = grown;
    x = cell.x - level.x0;
    y = cell.y - level.y0;
  }
  size_t at = (size_t)y * level.width + x;
  *isNew = !level.known[at];
  level.known[at] = 1;
  return &level.looks[at];
}

void AutoMapCellMemory::Forget(void)
{
  m_levels.clear();
  m_odd.clear();
}

void AutoMapCellMemory::ForgetLevel(int level)
{
  if (level < 0)
  {
    for (std::map<std::pair<int, std::pair<int, int> >, AutoMapCellLook>::iterator it = m_odd.begin(); it != m_odd.end();)
    {
      if (it->first.first == level) m_odd.erase(it++);
      else ++it;
    }
    return;
  }
  if ((size_t)level < m_levels.size())
  {
    LevelLooks &forgotten = m_levels[(size_t)level];
    forgotten.known.assign(forgotten.known.size(), 0);
  }
}

void AutoMapCellMemory::See(const AutoMapCell &cell, const AutoMapCellLook &look)
{
  bool isNew = false;
  AutoMapCellLook *known = Slot(cell, &isNew);
  if (isNew)
  {
    *known = look;
    return;
  }
  // The faces of a wall are seen one by one, so only SeeSide refreshes them
  known->kind = look.kind;
  known->doorState = look.doorState;
  known->pitIsOpen = look.pitIsOpen;
  known->pitIsHidden = look.pitIsHidden;
  known->floorPicture = look.floorPicture;
}

void AutoMapCellMemory::SeeSide(const AutoMapCell &cell, int side, int picture)
{
  if (side < 0 || side >= AUTOMAP_SIDE_COUNT) return;
  AutoMapCellLook *known = const_cast<AutoMapCellLook *>(Find(cell));
  if (known != NULL) known->sidePictures[side] = picture;
}

void AutoMapCellMemory::RememberIfNew(const AutoMapCell &cell, const AutoMapCellLook &look)
{
  if (Find(cell) != NULL) return;
  bool isNew = false;
  *Slot(cell, &isNew) = look;
}

void AutoMapCellMemory::Restore(const AutoMapCell &cell, const AutoMapCellLook &look)
{
  bool isNew = false;
  *Slot(cell, &isNew) = look;
}

bool AutoMapCellMemory::LookOf(const AutoMapCell &cell, AutoMapCellLook *look) const
{
  const AutoMapCellLook *found = Find(cell);
  if (found == NULL) return false;
  *look = *found;
  return true;
}

std::vector<std::pair<AutoMapCell, AutoMapCellLook> > AutoMapCellMemory::Looks(void) const
{
  std::vector<std::pair<AutoMapCell, AutoMapCellLook> > looks;
  for (std::map<std::pair<int, std::pair<int, int> >, AutoMapCellLook>::const_iterator it = m_odd.begin(); it != m_odd.end(); ++it)
  {
    AutoMapCell cell = {it->first.first, it->first.second.second, it->first.second.first};
    looks.push_back(std::make_pair(cell, it->second));
  }
  for (size_t l = 0; l < m_levels.size(); l++)
  {
    const LevelLooks &level = m_levels[l];
    for (int y = 0; y < level.height; y++)
      for (int x = 0; x < level.width; x++)
      {
        size_t at = (size_t)y * level.width + x;
        if (!level.known[at]) continue;
        AutoMapCell cell = {(int)l, x + level.x0, y + level.y0};
        looks.push_back(std::make_pair(cell, level.looks[at]));
      }
  }
  return looks;
}
