#ifndef AUTOMAPMONSTERDAMAGE_H
#define AUTOMAPMONSTERDAMAGE_H

#include <map>
#include <utility>
#include <vector>

#include "AutoMapKnowledge.h"

// DB4 hitPoints6 holds four members
const int AUTOMAP_DAMAGE_MEMBERS = 4;
// Stored in two bytes in the .map file
const int AUTOMAP_MOST_DAMAGE = 0xffff;

// The damage the game showed the player per monster group and member slot. Only shown numbers go
// in; the hit points never do.
class AutoMapMonsterDamage
{
public:
  void Forget(void);
  // amount <= 0, a member outside 0..3 or AUTOMAP_NO_RECORD change nothing; the sum stops at
  // AUTOMAP_MOST_DAMAGE
  void Credit(AutoMapRecordId group, int member, int amount);
  // The member left the group: the ones above it move down one slot, the top slot is empty. A
  // group left with no damage is dropped.
  void RemoveMember(AutoMapRecordId group, int member);
  void ForgetGroup(AutoMapRecordId group);
  // 0 when none
  int DealtTo(AutoMapRecordId group, int member) const;
  // Every group with damage, ordered by record, AUTOMAP_DAMAGE_MEMBERS values each (the .map file)
  std::vector<std::pair<AutoMapRecordId, std::vector<int> > > Groups(void) const;

private:
  typedef std::map<AutoMapRecordId, std::vector<int> > GroupMap;
  GroupMap m_groups;
};

#endif
