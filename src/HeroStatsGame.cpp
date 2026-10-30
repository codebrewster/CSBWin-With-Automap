// HeroStatsGame.cpp : decides whether a load is a reload of the game in play.
#include "HeroStatsGame.h"

bool HeroStatsGame_IsSame(bool gameWasInPlay, const std::string &dungeonInPlay,
                          const std::string &dungeonLoaded,
                          const std::vector<HeroStatsChampion> &party, const HeroStats &known)
{
  if (!gameWasInPlay || dungeonInPlay != dungeonLoaded) return false;
  for (size_t i = 0; i < party.size(); i++)
  {
    bool found = false;
    for (size_t k = 0; k < known.Champions().size(); k++)
    {
      if (known.Champions()[k].id == party[i]) found = true;
    }
    if (!found) return false;
  }
  return true;
}
