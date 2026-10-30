// HeroStatsRunes.cpp : the mana of each rune of the incantation being entered.
#include "HeroStatsRunes.h"

void HeroStatsRunes::AddRune(int slot, int mana)
{
  if (slot >= 0 && slot <= 3)
    m_runes[slot].push_back(mana);
}

void HeroStatsRunes::RemoveLastRune(int slot, int *mana)
{
  *mana = 0;
  if (slot < 0 || slot > 3 || m_runes[slot].empty())
    return;
  *mana = m_runes[slot].back();
  m_runes[slot].pop_back();
}

int HeroStatsRunes::TakeAll(int slot)
{
  if (slot < 0 || slot > 3)
    return 0;
  int total = 0;
  for (size_t i = 0; i < m_runes[slot].size(); i++)
    total += m_runes[slot][i];
  m_runes[slot].clear();
  return total;
}

void HeroStatsRunes::ClearAll(void)
{
  for (int i = 0; i < 4; i++)
    m_runes[i].clear();
}

void HeroStatsRunes::RemoveSlot(int slot)
{
  if (slot < 0 || slot > 3)
    return;
  for (int i = slot; i < 3; i++)
    m_runes[i] = m_runes[i + 1];
  m_runes[3].clear();
}
