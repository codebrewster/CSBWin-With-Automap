#ifndef HEROSTATSHOOKS_H
#define HEROSTATSHOOKS_H

// Owns the hero statistics of the game in play and connects them with loading and saving. This
// header declares functions only (HeroStats.h is included by AutoMapHooks.cpp before CSB.h), so
// that it can be included after CSB.h, which leaves #pragma pack(1) switched on.

#include "HeroStats.h"
#include "HeroStatsOrigins.h"

HeroStats &HeroStats_Store(void);
HeroStatsOrigins &HeroStats_Origins(void);

// AutoMap_OnDungeonLoaded: forgets the stores of the last dungeon. A new game starts empty; for
// a saved game the hardship stays in memory until HeroStats_OnMapFileRead (or, without a usable
// .map file, HeroStats_OnNoMapFile) decides whether it is the same game.
void HeroStats_OnDungeonLoaded(void);
// ReadMapFile read a file. Same game (a game was already in play in this session, same dungeon,
// and every champion of the party known to the file by fingerprint) -> TakeFromFile +
// CountReload; otherwise the file's numbers, hardship included, replace memory. The first load
// after the program starts is never a reload.
void HeroStats_OnMapFileRead(const HeroStats &fromFile, const HeroStatsOrigins &origins);
// A saved game was loaded with no usable .map file: the stores stay empty. The hardship in
// memory survives (and the load counts as a reload) when the party is known to the game that
// was in play, in the same dungeon.
void HeroStats_OnNoMapFile(void);
// The map file is about to be written: the champions of the party are known by name
void HeroStats_BeforeGameSaved(void);
// With map_debug set, the dump is printed on every save
void HeroStats_OnGameSaved(void);

#endif
