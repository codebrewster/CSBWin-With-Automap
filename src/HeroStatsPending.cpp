// HeroStatsPending.cpp : the cause of a death, from the batch of damage that was pending.
#include "HeroStatsPending.h"

static bool SameGroup(HeroHarmSource source, const HeroStatsBlow &blow, HeroHarmSource otherSource,
                      const HeroStatsBlow &other)
{
  return source == otherSource && blow.by == other.by && blow.champion == other.champion &&
         blow.monsterType == other.monsterType && blow.spell == other.spell;
}

void HeroStatsPending::Add(int slot, HeroHarmSource source, const HeroStatsBlow &blow, int amount)
{
  if (slot < 0 || slot > 3)
    return;
  std::vector<Group> &groups = m_groups[slot];
  for (size_t i = 0; i < groups.size(); i++)
  {
    if (SameGroup(source, blow, groups[i].source, groups[i].blow))
    {
      Group group = groups[i];
      group.amount += amount;
      groups.erase(groups.begin() + i);
      groups.push_back(group);  // added to most recently: wins ties
      return;
    }
  }
  Group group;
  group.source = source;
  group.blow = blow;
  group.amount = amount;
  groups.push_back(group);
}

bool HeroStatsPending::Cause(int slot, HeroHarmSource *source, HeroStatsBlow *blow) const
{
  if (slot < 0 || slot > 3 || m_groups[slot].empty())
    return false;
  const std::vector<Group> &groups = m_groups[slot];
  size_t best = 0;
  for (size_t i = 1; i < groups.size(); i++)
    if (groups[i].amount >= groups[best].amount)
      best = i;
  *source = groups[best].source;
  *blow = groups[best].blow;
  return true;
}

void HeroStatsPending::Clear(int slot)
{
  if (slot >= 0 && slot <= 3)
    m_groups[slot].clear();
}

void HeroStatsPending::ClearAll(void)
{
  for (int i = 0; i < 4; i++)
    m_groups[i].clear();
}
