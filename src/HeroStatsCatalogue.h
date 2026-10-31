#ifndef HEROSTATSCATALOGUE_H
#define HEROSTATSCATALOGUE_H

#include <string>
#include <vector>
#include "HeroStats.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// Every number the Statistics window shows, defined once: its label, the topic it belongs to and
// what kind of number it is. The topic pages, Per level and the HTML export are all made from
// this list, so a number is either everywhere it applies or nowhere. No engine, no ImGui.

enum HeroStatsTopic { HeroTopicOverview, HeroTopicCombat, HeroTopicMagic, HeroTopicSurvival, HeroTopicSupplies,
                      HeroTopicExploration, HeroTopicMonsters, HERO_TOPIC_COUNT };

enum HeroStatsNumber {
  HeroNumDamage, HeroNumMelee, HeroNumRanged, HeroNumMagic, HeroNumKills, HeroNumAttacks, HeroNumHits,
  HeroNumHitRate, HeroNumDamagePerHit, HeroNumFriendlyFire,
  HeroNumManaGenerated, HeroNumManaRegeneration, HeroNumManaPotions, HeroNumManaSpent, HeroNumManaWasted,
  HeroNumCasts, HeroNumCastsFailed,
  HeroNumTaken, HeroNumHealed, HeroNumHealRegeneration, HeroNumHealPotions, HeroNumHealAction, HeroNumHealingGiven,
  HeroNumDeaths, HeroNumResurrected, HeroNumWipes, HeroNumReloads,
  HeroNumEaten, HeroNumDrunk, HeroNumFoodGained, HeroNumFoodWasted, HeroNumWaterGained, HeroNumWaterWasted,
  HeroNumPickedUp, HeroNumUsedInWalls,
  HeroNumSteps, HeroNumTilesWalked, HeroNumTilesExplored, HeroNumTurns, HeroNumBumps, HeroNumTeleports,
  HeroNumPitFalls, HeroNumPitClimbs, HeroNumTicks, HeroNumSleepTicks,
  HeroNumDoorsBashed, HeroNumDoorsSmashed, HeroNumDoorsShot, HeroNumDoorsBySpell, HeroNumDoorsByZo,
  HeroNumDoorsButtonOpened, HeroNumDoorsButtonClosed, HeroNumChests, HeroNumScrolls, HeroNumWallTexts,
  HeroNumChampionsSeen, HeroNumChampionsViewed,
  HeroNumDoorKills, HeroNumPitKills, HeroNumOtherKills,
  HERO_NUMBER_COUNT };

// partyOnly: kept for the party, not per champion (steps, chests ...). wholeRunOnly: not kept per
// level (wipes, reloads). isBad: more is bad news. isRatio: a percentage or an average, which
// cannot be added up or shared out. isTicks: game ticks. help: a sentence for the tooltip, or "".
struct HeroStatsNumberInfo { const char *label; const char *help; bool partyOnly, wholeRunOnly, isBad, isRatio, isTicks; };
const HeroStatsNumberInfo &HeroStatsCatalogue_Info(HeroStatsNumber number);

struct HeroStatsTopicRow { HeroStatsNumber number; int depth; };  // depth 1: a part of the row above
struct HeroStatsTopicInfo { const char *label; std::vector<HeroStatsTopicRow> rows; std::vector<HeroStatsNumber> tiles; };
const HeroStatsTopicInfo &HeroStatsCatalogue_Topic(int topic);

// Every value of one champion (or HERO_STATS_PARTY) on one level (or HERO_STATS_WHOLE_RUN). A
// ratio is -1 when it is undefined (no attacks, no hits); a number that does not apply is 0.
struct HeroStatsRecord { int value[HERO_NUMBER_COUNT]; };
HeroStatsRecord HeroStatsCatalogue_Record(const HeroStats &stats, HeroStatsChampion who, int level);
bool HeroStatsCatalogue_Applies(HeroStatsNumber number, HeroStatsChampion who, int level);
bool HeroStatsCatalogue_Summable(HeroStatsNumber number);
// "4,412", "63%", "-" for an undefined ratio
std::string HeroStatsCatalogue_Format(HeroStatsNumber number, int value);
// The numbers Per level's "Champions side by side" offers: the topic's rows that are neither
// party-only nor ratios, in row order
std::vector<HeroStatsNumber> HeroStatsCatalogue_GridNumbers(int topic);
// csb.cfg's stats_page as written before the redesign (0 Table, 1 Bars, 2 By level, 3 Monsters,
// 4 Items) as a topic; anything else is the Overview
int HeroStatsCatalogue_TopicOfOldPage(int oldPage);
// "4,412 damage dealt · 31 kills · 63% hit rate": the topic's first three tiles that apply and
// are defined
std::string HeroStatsCatalogue_Summary(int topic, const HeroStatsRecord &record, HeroStatsChampion who, int level);

#pragma pack(pop)

#endif
