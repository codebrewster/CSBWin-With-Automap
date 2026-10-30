#ifndef HEROSTATSGAME_H
#define HEROSTATSGAME_H

#include <string>
#include <vector>
#include "HeroStats.h"

// Is the game just loaded the one that was in play? Then the hardship in memory outlives the
// load and the load counts as a reload. All of these must hold: a game was already in play in
// this session; the dungeon is the one of that game; and every champion now in the party is
// one the file (or, without a file, the memory of the game) knows by fingerprint.
bool HeroStatsGame_IsSame(bool gameWasInPlay, const std::string &dungeonInPlay,
                          const std::string &dungeonLoaded,
                          const std::vector<HeroStatsChampion> &party, const HeroStats &known);

#endif
