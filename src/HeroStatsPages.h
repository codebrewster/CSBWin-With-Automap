#ifndef HEROSTATSPAGES_H
#define HEROSTATSPAGES_H

#include <string>
#include <vector>
#include "HeroStats.h"
#include "HeroStatsView.h"
#include "HeroStatsCatalogue.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// The numbers of the statistics pages, as the window draws them and the export writes them. No
// engine, no ImGui: the names the game knows come in through HeroStatsNames.

struct HeroStatsNames
{
  std::string (*monster)(int monsterType, int variant);  // the dungeon's name of the variant
  std::string (*weapon)(int weapon);                     // HeroStats_WeaponName of the game's items
  std::string (*option)(int option);                     // "" when unknown
};

// One column of the Table and the Bars
struct HeroStatsCell
{
  int melee, ranged, magic, total, kills, taken, friendlyFire, healed, given, generated, spent, wasted, deaths;
  int drunk, eaten, fountainDrinks, foodGained, foodWasted, waterGained, waterWasted;
  int doorsBashed, doorsSmashed, doorsShot, doorsBySpell;   // the total includes doors of unknown method
  int doorsOpenedByZo;                                      // no bash, not in the total
};

struct HeroStatsOverview
{
  std::vector<HeroStatsColumn> columns;  // champions; the party's cell is cells.back()
  std::vector<HeroStatsCell> cells;      // columns.size() + 1
  int wipes, reloads;                    // the whole run's, whatever the level
  HeroStatsMovement movement;            // the party's, on the level
  int itemsPickedUp, itemsUsedInWalls;   // the party's, on the level
  int chestsOpened;                      // the party's, on the level
  int doorsOpenedByButton, doorsClosedByButton;  // the party's, on the level
  int scrollsRead, wallTextsRead;        // the party's, on the level
  int championsSeen, championsViewed;    // the party's, on the level
  // What each column ate and drank, one line per item name (most first), the cells' order; the
  // drunk lines end with "Fountain". Empty when the overview was made without names.
  struct CountLine { std::string name; std::vector<int> byCell; };
  std::vector<CountLine> eatenLines, drunkLines;
};
HeroStatsOverview HeroStatsPages_Overview(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level,
                                          const HeroStatsNames *names = 0);

// The Table's groups, in the order they are drawn, with the party's numbers summed up in one line
// for each group's folding header (like the champion pages' sections)
enum HeroStatsTableGroup { HeroGroupDealt, HeroGroupTaken, HeroGroupHealing, HeroGroupHardship, HeroGroupMana,
                           HeroGroupMovement, HeroGroupItems, HeroGroupFood, HeroGroupDungeon, HeroGroupReading,
                           HeroGroupChampions, HERO_TABLE_GROUP_COUNT };
extern const char *const HERO_STATS_TABLE_GROUP_TITLES[HERO_TABLE_GROUP_COUNT];
// wholeRun: the wipes and reloads belong to the whole run only
std::vector<std::string> HeroStatsPages_TableSummaries(const HeroStatsOverview &data, bool wholeRun);

// What the By level page adds up. From HeroMetricMovement on, a button shows a table of the
// party's numbers per level instead of a number per champion (HeroStatsPages_Metric gives 0).
enum HeroStatsMetric { HeroMetricDealt, HeroMetricKills, HeroMetricTaken, HeroMetricHealed, HeroMetricSpent,
                       HeroMetricMovement, HeroMetricItems, HeroMetricFood, HeroMetricDungeon, HeroMetricReading,
                       HeroMetricChampions, HERO_METRIC_COUNT };
const int HERO_STATS_FIRST_PARTY_TABLE = HeroMetricMovement;
const int HERO_STATS_PARTY_TABLE_COUNT = HERO_METRIC_COUNT - HeroMetricMovement;
// The party table of a metric from HERO_STATS_FIRST_PARTY_TABLE on: its column labels, and the
// party's numbers on a level (or the whole run) in that order
std::vector<std::string> HeroStatsPages_PartyTableLabels(int metric);
std::vector<int> HeroStatsPages_PartyTableRow(const HeroStats &stats, int level, int metric);
extern const char *const HERO_STATS_METRIC_LABELS[HERO_METRIC_COUNT];
int HeroStatsPages_Metric(const HeroStats &stats, HeroStatsChampion who, int level, int metric);

// The columns of the movement table, after the level
enum HeroStatsMovementColumn { HeroMoveSteps, HeroMoveTilesWalked, HeroMoveTilesExplored, HeroMoveTurns, HeroMoveBumps, HeroMoveTeleports, HeroMovePitFalls,
                               HeroMovePitClimbs, HeroMoveTicks, HeroMoveSleepTicks, HERO_MOVE_COLUMN_COUNT };
extern const char *const HERO_STATS_MOVEMENT_LABELS[HERO_MOVE_COLUMN_COUNT];
int HeroStatsPages_MovementValue(const HeroStatsMovement &movement, int column);

// One row of the Monsters page: the monsters of one name, killed by each champion column, by
// doors, pits and anything else (clouds, traps, monsters, the game)
struct HeroStatsMonsterLine
{
  std::string name;
  std::vector<int> byColumn;
  int door, pit, other, total;
};
// Most killed first. Variants the dungeon gives no name of their own share one line.
std::vector<HeroStatsMonsterLine> HeroStatsPages_Monsters(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                                          int level, const HeroStatsNames &names);
HeroStatsMonsterLine HeroStatsPages_AllMonsters(const std::vector<HeroStatsMonsterLine> &lines, size_t columnCount);

// One row of the Items page: the items of one name the party picked up the first time, the ones
// walls took (keys, coins, gems), and the ones the champions drank or ate
struct HeroStatsItemLine { std::string name; int pickedUp, usedInWall, drunk, eaten; };
// Most picked up first, then most used in walls, drunk and eaten, then by name. Items of one name
// share one line.
std::vector<HeroStatsItemLine> HeroStatsPages_Items(const HeroStats &stats, int level, const HeroStatsNames &names);
HeroStatsItemLine HeroStatsPages_AllItems(const std::vector<HeroStatsItemLine> &lines);

// A champion's or the party's page
// tries: blows and missiles, which can hit; uses: actions that cannot (War Cry, Calm ...). The
// table shows them added up as tries, and the hit rate of the tries alone.
struct HeroStatsOptionLine { std::string name; int tries, hits, damage, kills, uses; };
struct HeroStatsWeaponLine
{
  std::string name;
  int tries, hits, damage, kills, uses;
  int id;  // the same for the same weapon on every page, for its fold state
  std::vector<HeroStatsOptionLine> options;
  std::vector<int> byColumn;  // the Party's page: each column's damage with it
};
// casts: every cast, successful ones and failures
// byColumn: on the Party's page, each column's share of the line (a spell's mana, an amount)
struct HeroStatsSpellLine { std::string name; int casts, successful, failed, mana, wasted, damage, kills; std::vector<int> byColumn; };
struct HeroStatsAmountLine { std::string text; int amount; bool isBad; std::vector<int> byColumn; };

struct HeroStatsHeroPage
{
  std::vector<HeroStatsWeaponLine> weapons;   // most damage first, their options too
  std::vector<HeroStatsSpellLine> spells;     // most mana first
  std::vector<HeroStatsAmountLine> taken;     // biggest first
  int healedTotal;
  std::vector<HeroStatsAmountLine> healing;   // biggest first
  int given, resurrected;
  std::vector<std::string> deathLines;
  int wipes, reloads;
  // Food and water: everything eaten and drunk (the fountain included), a line per item (most
  // first; the drunk lines end with "Fountain"), what it gave and what was lost over full
  int eatenTotal, drunkTotal;
  std::vector<HeroStatsAmountLine> eaten, drunk;
  HeroStatsNourishment nourishment;
  HeroStatsSummaries summaries;
};
HeroStatsHeroPage HeroStatsPages_Hero(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                      HeroStatsChampion who, int level, const HeroStatsNames &names);

// A topic page (HeroStatsCatalogue): its summary tiles, its numbers and its detail tables

// One row of the Numbers table. byColumn: the Party's page: one value per column (empty for a
// party-only number); a champion's page: that champion's value. party: the party's value.
struct HeroStatsNumbersRow { HeroStatsNumber number; int depth; std::vector<int> byColumn; int party; bool partyOnly; };
enum HeroStatsDetail { HeroDetailWeapons, HeroDetailSpells, HeroDetailHarm, HeroDetailHealing, HeroDetailDeaths,
                       HeroDetailConsumed, HeroDetailItems, HeroDetailMonsters, HERO_DETAIL_COUNT };
struct HeroStatsTopicPage
{
  std::vector<HeroStatsNumber> tiles;         // the tiles that apply, at most 4
  HeroStatsRecord record;                     // who's numbers on the level
  std::vector<HeroStatsNumbersRow> numbers;   // the topic's rows that apply
  std::vector<HeroStatsDetail> details;       // in drawing order
  HeroStatsHeroPage hero;                     // weapons, spells, taken, healing, deaths, eaten, drunk
  std::vector<HeroStatsMonsterLine> monsters;
  std::vector<HeroStatsItemLine> items;
  std::string summary;                        // HeroStatsCatalogue_Summary
};
HeroStatsTopicPage HeroStatsPages_Topic(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                        HeroStatsChampion who, int level, int topic, const HeroStatsNames &names);

// The Party's Overview: a card per champion; damagePercent: their share of the party's damage
struct HeroStatsCard { HeroStatsColumn column; HeroStatsRecord record; int damagePercent; };
std::vector<HeroStatsCard> HeroStatsPages_Cards(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level);

// Per level: one row per visited level (ascending), then the whole run (level HERO_STATS_WHOLE_RUN)
struct HeroStatsLevelRow { int level; std::vector<int> values; std::vector<int> share; };
struct HeroStatsPerLevel
{
  std::vector<HeroStatsNumber> numbers;  // the columns after the level
  HeroStatsNumber shareNumber;           // what share splits by column, when hasShare
  bool hasShare;
  std::vector<HeroStatsLevelRow> rows;
};
// The party's (who HERO_STATS_PARTY) or a champion's totals: the topic's top-level rows that
// apply to who on a level. The party's rows share out the topic's first number kept per champion.
HeroStatsPerLevel HeroStatsPages_PerLevelTotals(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                                HeroStatsChampion who, int topic);
// Champions side by side: values are one per column, then the party; share one per column
HeroStatsPerLevel HeroStatsPages_PerLevelGrid(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                              HeroStatsNumber number);

#pragma pack(pop)

#endif
