#ifndef HEROSTATSRUNES_H
#define HEROSTATSRUNES_H

#include <stdint.h>
#include <vector>

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// The mana the engine takes for the runes of an incantation (CHARDESC::incantation: rune
// characters 96 + 6 * position + button, four bytes with no terminator when all four are
// entered). The engine (TAG01b990) takes runeCosts[rune - 96] (d.Byte19010, 24 entries), for the
// runes after the power rune times powerFactors[power - 96] (d.Byte19016, 6 entries) / 8.
namespace HeroStatsRunes
{
  // The runes entered: those before the first 0, at most four
  int RuneCount(const uint8_t *incantation);
  // The mana of the rune at position (0-3); -1 for a character that is no rune of its position
  int RuneCost(const uint8_t *incantation, int position, const int8_t *runeCosts, const int8_t *powerFactors);
  // The mana of each rune entered, in order; empty when one is no rune the engine could have made
  std::vector<int> Costs(const uint8_t *incantation, const int8_t *runeCosts, const int8_t *powerFactors);
}

#pragma pack(pop)

#endif
