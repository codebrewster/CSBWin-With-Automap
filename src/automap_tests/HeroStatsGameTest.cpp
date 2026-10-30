#include "TestSupport.h"
#include "../HeroStatsGame.h"

static void SameGameNeedsGameDungeonAndKnownChampions(void)
{
  HeroStats known;
  known.MeetChampion(7, "HALK");
  known.MeetChampion(9, "SYRA");
  std::vector<HeroStatsChampion> party;
  party.push_back(7);
  party.push_back(9);
  CHECK(HeroStatsGame_IsSame(true, "dungeon.dat", "dungeon.dat", party, known));
  // The first load after the program starts
  CHECK(!HeroStatsGame_IsSame(false, "dungeon.dat", "dungeon.dat", party, known));
  // Another dungeon
  CHECK(!HeroStatsGame_IsSame(true, "dungeon.dat", "other.dat", party, known));
  // A champion the file does not know: another game
  party.push_back(12);
  CHECK(!HeroStatsGame_IsSame(true, "dungeon.dat", "dungeon.dat", party, known));
}

static void ASmallerPartyIsStillTheSameGame(void)
{
  HeroStats known;
  known.MeetChampion(7, "HALK");
  known.MeetChampion(9, "SYRA");
  std::vector<HeroStatsChampion> party;
  party.push_back(9);
  CHECK(HeroStatsGame_IsSame(true, "d", "d", party, known));
  // An empty party (nobody recruited yet) knows nothing against it
  party.clear();
  CHECK(HeroStatsGame_IsSame(true, "d", "d", party, known));
}

void TestHeroStatsGame(void)
{
  SameGameNeedsGameDungeonAndKnownChampions();
  ASmallerPartyIsStillTheSameGame();
}
