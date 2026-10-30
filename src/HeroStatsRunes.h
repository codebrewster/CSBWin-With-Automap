#ifndef HEROSTATSRUNES_H
#define HEROSTATSRUNES_H

#include <stddef.h>
#include <vector>

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// Mana is taken per rune as it is clicked. This keeps the cost of each rune of the incantation
// being entered, per slot, so the spell row gets its mana when the spell resolves.
class HeroStatsRunes
{
public:
  void AddRune(int slot, int mana);
  void RemoveLastRune(int slot, int *mana);   // backspace: the rune's mana, 0 if none
  int TakeAll(int slot);                      // mana of all runes entered; the slot is empty after
  void ClearAll(void);

private:
  std::vector<int> m_runes[4];
};

#pragma pack(pop)

#endif
