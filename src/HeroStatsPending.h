#ifndef HEROSTATSPENDING_H
#define HEROSTATSPENDING_H

#include <vector>
#include "HeroStats.h"

// Damage to a hero is collected and applied later (DisplayCharacterDamage). This names the
// source of the batch that killed: the one that added most, ties to the later.
class HeroStatsPending
{
public:
  void Add(int slot, HeroHarmSource source, const HeroStatsBlow &blow, int amount);  // slot 0..3
  // false when nothing was added since the last Clear
  bool Cause(int slot, HeroHarmSource *source, HeroStatsBlow *blow) const;
  void Clear(int slot);
  void ClearAll(void);

private:
  struct Group { HeroHarmSource source; HeroStatsBlow blow; int amount; };
  std::vector<Group> m_groups[4];  // the one added to last is at the back
};

#endif
