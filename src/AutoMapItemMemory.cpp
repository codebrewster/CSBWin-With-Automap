// AutoMapItemMemory.cpp : the floor items the map remembers.
#include "AutoMapItemMemory.h"

#include <cstddef>

static bool SameCell(const AutoMapCell &left, const AutoMapCell &right)
{
  return left.level == right.level && left.x == right.x && left.y == right.y;
}

static long long KeyOf(const AutoMapCell &cell)
{
  return ((long long)cell.level << 40) ^ ((long long)(unsigned)cell.x << 20) ^ (long long)(unsigned)cell.y;
}

AutoMapItemMemory::AutoMapItemMemory(void)
  : m_byCellIsCurrent(false)
{
}

void AutoMapItemMemory::Changed(void)
{
  m_byCellIsCurrent = false;
}

const std::vector<size_t> *AutoMapItemMemory::PositionsOn(const AutoMapCell &cell) const
{
  if (!m_byCellIsCurrent)
  {
    m_byCell.clear();
    for (size_t i = 0; i < m_items.size(); i++) m_byCell[KeyOf(m_items[i].cell)].push_back(i);
    m_byCellIsCurrent = true;
  }
  CellIndex::const_iterator at = m_byCell.find(KeyOf(cell));
  if (at == m_byCell.end()) return NULL;
  return &at->second;
}

void AutoMapItemMemory::Forget(void)
{
  m_items.clear();
  Changed();
}

void AutoMapItemMemory::ForgetLevel(int level)
{
  std::vector<AutoMapRememberedItem> kept;
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (m_items[i].cell.level != level) kept.push_back(m_items[i]);
  }
  if (kept.size() != m_items.size()) Changed();
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
  Changed();
}

void AutoMapItemMemory::ForgetItem(AutoMapRecordId record)
{
  for (size_t i = 0; i < m_items.size(); i++)
  {
    if (m_items[i].record == record)
    {
      m_items.erase(m_items.begin() + (std::ptrdiff_t)i);
      Changed();
      return;
    }
  }
}

bool AutoMapItemMemory::Remembers(const AutoMapRememberedItem &item) const
{
  const std::vector<size_t> *on = PositionsOn(item.cell);
  if (on == NULL) return false;
  for (size_t p = 0; p < on->size(); p++)
  {
    const AutoMapRememberedItem &kept = m_items[(*on)[p]];
    if (kept.record == item.record && kept.descIndex == item.descIndex) return true;
  }
  return false;
}

bool AutoMapItemMemory::Identifies(const AutoMapRememberedItem &item) const
{
  const std::vector<size_t> *on = PositionsOn(item.cell);
  if (on == NULL) return false;
  for (size_t p = 0; p < on->size(); p++)
  {
    const AutoMapRememberedItem &kept = m_items[(*on)[p]];
    if (kept.record == item.record && kept.descIndex == item.descIndex) return !kept.isGlimpsed;
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
  const std::vector<size_t> *on = PositionsOn(cell);
  if (on == NULL) return found;
  for (size_t p = 0; p < on->size(); p++) found.push_back(m_items[(*on)[p]]);
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
  if (kept.size() != m_items.size()) Changed();
  m_items.swap(kept);
}

std::vector<AutoMapRememberedItem> AutoMapItemMemory::Items(void) const
{
  return m_items;
}
