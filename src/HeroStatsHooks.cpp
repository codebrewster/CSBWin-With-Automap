// HeroStatsHooks.cpp : owns the hero statistics and connects them with loading and saving.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "HeroStatsDump.h"
#include "HeroStatsGame.h"
#include "AutoMapWindow.h"

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

#include "HeroStatsHooks.h"

static HeroStats store;
static HeroStatsOrigins origins;
// The game before the load in progress, to judge a load that has no .map file
static HeroStats gameBeforeLoad;
// A game is in play once a new game started or a saved game was loaded. The dungeon is told by
// the dungeon file name, the same one the automap uses for the save; a new game is a different
// game even in the same dungeon.
static bool gameInPlay = false;
static std::string dungeonInPlay;

HeroStats &HeroStats_Store(void)
{
  return store;
}

HeroStatsOrigins &HeroStats_Origins(void)
{
  return origins;
}

static std::string CurrentDungeon(void)
{
  return dungeonName == NULL ? std::string() : std::string(dungeonName);
}

static std::vector<HeroStatsChampion> PartyNow(void)
{
  std::vector<HeroStatsChampion> party;
  for (int i = 0; i < d.NumCharacter && i < 4; i++) party.push_back(d.hero[i].fingerPrint);
  return party;
}

static void MeetParty(void)
{
  for (int i = 0; i < d.NumCharacter && i < 4; i++)
  {
    char name[sizeof d.hero[i].name + 1];
    memcpy(name, d.hero[i].name, sizeof d.hero[i].name);
    name[sizeof d.hero[i].name] = 0;
    store.MeetChampion(d.hero[i].fingerPrint, name);
  }
}

static void GameStartsOrContinues(void)
{
  gameInPlay = true;
  dungeonInPlay = CurrentDungeon();
  gameBeforeLoad.Forget();
}

void HeroStats_OnDungeonLoaded(void)
{
  gameBeforeLoad = store;
  store.Forget();
  origins.Forget();
  if (d.gameState != GAMESTATE_ResumeSavedGame)
  {
    GameStartsOrContinues();
    return;
  }
  store.MergeHardship(gameBeforeLoad);
}

void HeroStats_OnMapFileRead(const HeroStats &fromFile, const HeroStatsOrigins &originsFromFile)
{
  if (HeroStatsGame_IsSame(gameInPlay, dungeonInPlay, CurrentDungeon(), PartyNow(), fromFile))
  {
    store.TakeFromFile(fromFile);
    store.CountReload();
  }
  else
  {
    store.Forget();
    store.TakeFromFile(fromFile);
  }
  origins = originsFromFile;
  MeetParty();
  GameStartsOrContinues();
}

void HeroStats_OnNoMapFile(void)
{
  if (HeroStatsGame_IsSame(gameInPlay, dungeonInPlay, CurrentDungeon(), PartyNow(), gameBeforeLoad))
  {
    store.CountReload();
  }
  else
  {
    store.Forget();
  }
  MeetParty();
  GameStartsOrContinues();
}

void HeroStats_BeforeGameSaved(void)
{
  MeetParty();
}

void HeroStats_OnGameSaved(void)
{
  if (!AutoMapWindow_DebugIsOn()) return;
  printf("Hero statistics:\n%s", HeroStatsDump_Text(store).c_str());
}
