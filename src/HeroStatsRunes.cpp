// HeroStatsRunes.cpp : the mana the engine takes for each rune of an incantation.
#include "HeroStatsRunes.h"

int HeroStatsRunes::RuneCount(const uint8_t *incantation)
{
  int count = 0;
  while (count < 4 && incantation[count] != 0) count++;
  return count;
}

int HeroStatsRunes::RuneCost(const uint8_t *incantation, int position, const int8_t *runeCosts, const int8_t *powerFactors)
{
  if (position < 0 || position > 3) return -1;
  int rune = (int)incantation[position] - 96;
  if (rune < 6 * position || rune >= 6 * position + 6) return -1;
  int cost = (uint8_t)runeCosts[rune];
  if (position == 0) return cost;
  int power = (int)incantation[0] - 96;
  if (power < 0 || power >= 6) return -1;
  return (cost * (uint8_t)powerFactors[power]) >> 3;
}

std::vector<int> HeroStatsRunes::Costs(const uint8_t *incantation, const int8_t *runeCosts, const int8_t *powerFactors)
{
  std::vector<int> costs;
  int count = RuneCount(incantation);
  for (int position = 0; position < count; position++)
  {
    int cost = RuneCost(incantation, position, runeCosts, powerFactors);
    // Not an incantation the engine could have made: nothing is known of its mana
    if (cost < 0) return std::vector<int>();
    costs.push_back(cost);
  }
  return costs;
}
