#include "TestSupport.h"
#include "../HeroStatsLive.h"

// The slots as the game numbers them: hands, body, pouches and quivers, neck, then the backpack
static void SlotsAreNamedAsTheGameNumbersThem(void)
{
  CHECK(HeroStatsLive_SlotName(0) == "Ready hand" && HeroStatsLive_SlotName(1) == "Action hand");
  CHECK(HeroStatsLive_SlotName(5) == "Feet" && HeroStatsLive_SlotName(10) == "Neck");
  CHECK(HeroStatsLive_SlotName(11) == "Pouch 1" && HeroStatsLive_SlotName(12) == "Quiver 1");
  CHECK(HeroStatsLive_SlotName(13) == "Backpack 1" && HeroStatsLive_SlotName(29) == "Backpack 17");
  CHECK(HeroStatsLive_SlotName(-1).empty() && HeroStatsLive_SlotName(30).empty());
}

static void WoundsAttributesAndTenthsRead(void)
{
  CHECK(HeroStatsLive_Wounds(0).empty());
  CHECK(HeroStatsLive_Wounds(0x04 | 0x20) == "Head, Feet");
  CHECK(HeroStatsLive_Wounds(0x01) == "Ready hand");
  CHECK(std::string(HeroStatsLive_AttributeName(0)) == "Strength" && std::string(HeroStatsLive_AttributeName(5)) == "Anti-fire");
  CHECK(std::string(HeroStatsLive_AttributeName(6)).empty());
  CHECK(HeroStatsLive_Tenths(123) == "12.3" && HeroStatsLive_Tenths(0) == "0.0" && HeroStatsLive_Tenths(-5) == "-0.5");
}

static void AChampionIsFoundByFingerprint(void)
{
  HeroStatsLiveParty party;
  HeroStatsLiveChampion one;
  one.who = 7;
  one.name = "HALK";
  party.champions.push_back(one);
  CHECK(HeroStatsLive_Find(party, 7) != NULL && HeroStatsLive_Find(party, 7)->name == "HALK");
  CHECK(HeroStatsLive_Find(party, 8) == NULL);
}

void TestHeroStatsLive(void)
{
  SlotsAreNamedAsTheGameNumbersThem();
  WoundsAttributesAndTenthsRead();
  AChampionIsFoundByFingerprint();
}
