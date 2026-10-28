// AutoMapItemMemory.cpp : the floor items the map remembers.
#include "AutoMapItemMemory.h"

#include <cstddef>

static bool SameCell(const AutoMapCell &left, const AutoMapCell &right)
{
  return left.level == right.level && left.x == right.x && left.y == right.y;
}

void AutoMapItemMemory::Forget(void)
{
  m_items.clear();
}

void AutoMapItemMemory::ForgetLevel(int level)
{
  std::vector<AutoMapRememberedItem> kept;
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (m_items[i].cell.level != level) kept.push_back(m_items[i]);
  }
  m_items.swap(kept);
}

void AutoMapItemMemory::See(const AutoMapRememberedItem &item)
{
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (m_items[i].record != item.record) continue;
    if (SameCell(m_items[i].cell, item.cell))
    {
      // Same place: only the description can differ, and the order stays
      m_items[i].descIndex = item.descIndex;
      m_items[i].isGlimpsed = m_items[i].isGlimpsed && item.isGlimpsed;
      return;
    }
    m_items.erase(m_items.begin() + (std::ptrdiff_t)i);
    break;
  }
  m_items.push_back(item);
}

void AutoMapItemMemory::ForgetItem(AutoMapRecordId record)
{
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (m_items[i].record == record)
    {
      m_items.erase(m_items.begin() + (std::ptrdiff_t)i);
      return;
    }
  }
}

bool AutoMapItemMemory::Remembers(const AutoMapRememberedItem &item) const
{
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (m_items[i].record == item.record && m_items[i].descIndex == item.descIndex
        && SameCell(m_items[i].cell, item.cell))
    {
      return true;
    }
  }
  return false;
}

bool AutoMapItemMemory::Identifies(const AutoMapRememberedItem &item) const
{
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (m_items[i].record == item.record && m_items[i].descIndex == item.descIndex
        && SameCell(m_items[i].cell, item.cell))
    {
      return !m_items[i].isGlimpsed;
    }
  }
  return false;
}

bool AutoMapItemMemory::HasSeen(const AutoMapFloorItemTag &tag) const
{
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (m_items[i].record == tag.record && m_items[i].descIndex == tag.descIndex)
    {
      return !m_items[i].isGlimpsed;
    }
  }
  return false;
}

std::vector<AutoMapRememberedItem> AutoMapItemMemory::ItemsOn(const AutoMapCell &cell) const
{
  std::vector<AutoMapRememberedItem> found;
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (SameCell(m_items[i].cell, cell)) found.push_back(m_items[i]);
  }
  return found;
}

void AutoMapItemMemory::KeepOnlyItemsStillOn(const AutoMapCell &cell,
                                             const std::vector<AutoMapFloorItemTag> &itemsThere)
{
  std::vector<AutoMapRememberedItem> kept;
  for (size_t i = 0; i < m_items.size(); i++)
  {
    bool stays = !SameCell(m_items[i].cell, cell);
    for (size_t t = 0; !stays && t < itemsThere.size(); t++)
    {
      // A record number is reused once its item is gone, so the description must match too
      stays = itemsThere[t].record == m_items[i].record
              && itemsThere[t].descIndex == m_items[i].descIndex;
    }
    if (stays) kept.push_back(m_items[i]);
  }
  m_items.swap(kept);
}

std::vector<AutoMapRememberedItem> AutoMapItemMemory::Items(void) const
{
  return m_items;
}
