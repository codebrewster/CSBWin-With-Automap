// AutoMapMonsterDamage.cpp : the damage the game showed the player per monster group and member.
#include "AutoMapMonsterDamage.h"

static bool MemberIsInGroup(int member)
{
  return member >= 0 && member < AUTOMAP_DAMAGE_MEMBERS;
}

void AutoMapMonsterDamage::Forget(void)
{
  m_groups.clear();
}

void AutoMapMonsterDamage::Credit(AutoMapRecordId group, int member, int amount)
{
  if (amount <= 0 || !MemberIsInGroup(member) || group == AUTOMAP_NO_RECORD) return;
  std::vector<int> &dealt = m_groups[group];
  if (dealt.empty()) dealt.assign(AUTOMAP_DAMAGE_MEMBERS, 0);
  // Added as wide as needed first, so a huge amount cannot wrap past the cap
  long long sum = (long long)dealt[(size_t)member] + amount;
  dealt[(size_t)member] = sum > AUTOMAP_MOST_DAMAGE ? AUTOMAP_MOST_DAMAGE : (int)sum;
}

void AutoMapMonsterDamage::RemoveMember(AutoMapRecordId group, int member)
{
  GroupMap::iterator found = m_groups.find(group);
  if (found == m_groups.end() || !MemberIsInGroup(member)) return;
  std::vector<int> &dealt = found->second;
  dealt.erase(dealt.begin() + member);
  dealt.push_back(0);
  for (size_t i = 0; i < dealt.size(); i++)
  {
    if (dealt[i] != 0) return;
  }
  m_groups.erase(found);
}

void AutoMapMonsterDamage::ForgetGroup(AutoMapRecordId group)
{
  m_groups.erase(group);
}

int AutoMapMonsterDamage::DealtTo(AutoMapRecordId group, int member) const
{
  GroupMap::const_iterator found = m_groups.find(group);
  if (found == m_groups.end() || !MemberIsInGroup(member)) return 0;
  return found->second[(size_t)member];
}

std::vector<std::pair<AutoMapRecordId, std::vector<int> > > AutoMapMonsterDamage::Groups(void) const
{
  return std::vector<std::pair<AutoMapRecordId, std::vector<int> > >(m_groups.begin(), m_groups.end());
}
