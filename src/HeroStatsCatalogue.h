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
                      HeroTopicExploration, HeroTopicMonsters, HeroTopicExperience, HeroTopicRecords, HERO_TOPIC_COUNT };

enum HeroStatsNumber {
  HeroNumDamage, HeroNumMelee, HeroNumRanged, HeroNumMagic, HeroNumKills, HeroNumAttacks, HeroNumHits,
  HeroNumHitRate, HeroNumDamagePerHit, HeroNumOtherActions, HeroNumFriendlyFire,
  HeroNumManaGenerated, HeroNumManaRegeneration, HeroNumManaPotions, HeroNumManaSpent, HeroNumManaOnSpells, HeroNumManaSuccessful, HeroNumManaWasted,
  HeroNumRunesCleared, HeroNumRunesNotCast,
  HeroNumCasts, HeroNumCastsSuccessful, HeroNumCastsFailed,
  HeroNumTaken, HeroNumHealed, HeroNumHealRegeneration, HeroNumHealPotions, HeroNumHealAction, HeroNumHealingGiven, HeroNumPoisonings, HeroNumWounds,
  HeroNumDeaths, HeroNumResurrected, HeroNumWipes, HeroNumReloads,
  HeroNumEaten, HeroNumDrunk, HeroNumFoodGained, HeroNumFoodWasted, HeroNumWaterGained, HeroNumWaterWasted,
  HeroNumPickedUp, HeroNumUsedInWalls,
  HeroNumSteps, HeroNumTilesWalked, HeroNumTilesExplored, HeroNumTurns, HeroNumBumps, HeroNumTeleports,
  HeroNumPitFalls, HeroNumPitClimbs, HeroNumTicks, HeroNumSleepTicks,
  HeroNumDoorsBashed, HeroNumDoorsSmashed, HeroNumDoorsShot, HeroNumDoorsBySpell, HeroNumDoorsByZo,
  HeroNumDoorsButtonOpened, HeroNumDoorsButtonClosed, HeroNumChests, HeroNumScrolls, HeroNumWallTexts,
  HeroNumChampionsSeen, HeroNumChampionsViewed,
  HeroNumDoorKills, HeroNumPitKills, HeroNumOtherKills,
  HeroNumExperience, HeroNumXpFighter, HeroNumXpNinja, HeroNumXpPriest, HeroNumXpWizard, HeroNumRanksGained,
  // The records, in HeroRecordKind order (HeroNumRecFirst + kind)
  HeroNumRecBiggestHit, HeroNumRecBiggestSpellHit, HeroNumRecBiggestSpellTotal, HeroNumRecMostKillsAtOnce,
  HeroNumRecKillStreak, HeroNumRecBiggestHitTaken, HeroNumRecBiggestSpellHitTaken, HeroNumRecClosestCall,
  HeroNumRecBiggestHeal, HeroNumRecMostManaOnSpell, HeroNumRecBiggestXpGain, HeroNumRecHeaviestLoad,
  HeroNumRecLongestFall, HeroNumRecLongestTeleportChain, HeroNumRecLongestSleep,
  HeroNumRecMostPoison, HeroNumRecLongestPoisoned, HeroNumRecMostPoisonings,
  HeroNumRecMostWoundsAtOnce, HeroNumRecMostWoundsInOneBlow, HeroNumRecTotalPoisonings, HeroNumRecTotalWounds,
  HeroNumRecMostPoisonDoses, HeroNumRecMostPoisonPerTurn,
  HERO_NUMBER_COUNT };
const HeroStatsNumber HeroNumRecFirst = HeroNumRecBiggestHit;

// partyOnly: kept for the party, not per champion (steps, chests ...). wholeRunOnly: not kept per
// level (wipes, reloads). isBad: more is bad news. isRatio: a percentage or an average, which
// cannot be added up or shared out. isTicks: game ticks. help: a sentence for the tooltip, or "".
// isRecord: a record (HeroStatsRecords): the best, not a sum; -1 when there is none.
struct HeroStatsNumberInfo { const char *label; const char *help; bool partyOnly, wholeRunOnly, isBad, isRatio, isTicks, isRecord; };
bool HeroStatsCatalogue_IsRecord(HeroStatsNumber number);
HeroRecordKind HeroStatsCatalogue_RecordOf(HeroStatsNumber number);  // for a record
const HeroStatsNumberInfo &HeroStatsCatalogue_Info(HeroStatsNumber number);

struct HeroStatsTopicRow { HeroStatsNumber number; int depth; };  // depth 1: a part of the row above, 2: a part of that
struct HeroStatsTopicInfo { const char *label; std::vector<HeroStatsTopicRow> rows; std::vector<HeroStatsNumber> tiles; };
const HeroStatsTopicInfo &HeroStatsCatalogue_Topic(int topic);

// Each number's name in csb.cfg: "damage", "hit_rate", "other_actions" ...
std::string HeroStatsCatalogue_Key(HeroStatsNumber number);
// "damage, kills hit_rate": names split by commas or spaces, any case, in their order; unknown
// names and repeats are left out, and no more than most
std::vector<HeroStatsNumber> HeroStatsCatalogue_ParseNumbers(const std::string &text, size_t most);
std::string HeroStatsCatalogue_JoinNumbers(const std::vector<HeroStatsNumber> &numbers);  // "damage, kills"

// What csb.cfg can choose: the tiles at the top of each topic (the Overview's are the Party's),
// the tiles of a champion's Overview and the Key numbers. A topic's tiles are also what its
// summary on a champion's Overview says.
enum HeroStatsSetting { HeroSettingOverviewTiles, HeroSettingChampionTiles, HeroSettingCombatTiles, HeroSettingMagicTiles,
                        HeroSettingSurvivalTiles, HeroSettingSuppliesTiles, HeroSettingExplorationTiles,
                        HeroSettingMonstersTiles, HeroSettingExperienceTiles, HeroSettingRecordsTiles,
                        HeroSettingKeyNumbers, HERO_SETTING_COUNT };
const char *HeroStatsCatalogue_SettingEntry(HeroStatsSetting setting);   // "stats_tiles_combat"
std::string HeroStatsCatalogue_SettingDefault(HeroStatsSetting setting); // the default list as names
// Takes the setting's list (at most 4 tiles); a list with no number it knows keeps the default
void HeroStatsCatalogue_ApplySetting(HeroStatsSetting setting, const std::string &text);
std::vector<HeroStatsNumber> HeroStatsCatalogue_DefaultTiles(int topic);
const std::vector<HeroStatsNumber> &HeroStatsCatalogue_ChampionTiles(void);  // a champion's Overview

// Every value of one champion (or HERO_STATS_PARTY) on one level (or HERO_STATS_WHOLE_RUN). A
// ratio is -1 when it is undefined (no attacks, no hits); a number that does not apply is 0.
struct HeroStatsRecord { int value[HERO_NUMBER_COUNT]; };
HeroStatsRecord HeroStatsCatalogue_Record(const HeroStats &stats, HeroStatsChampion who, int level);
// One number of the record, without the rest
int HeroStatsCatalogue_Value(const HeroStats &stats, HeroStatsChampion who, int level, HeroStatsNumber number);
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
