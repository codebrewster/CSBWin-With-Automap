#ifndef HEROSTATSPAGES_H
#define HEROSTATSPAGES_H

#include <string>
#include <vector>
#include "HeroStats.h"
#include "HeroStatsView.h"

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
};

struct HeroStatsOverview
{
  std::vector<HeroStatsColumn> columns;  // champions; the party's cell is cells.back()
  std::vector<HeroStatsCell> cells;      // columns.size() + 1
  int wipes, reloads;                    // the whole run's, whatever the level
  HeroStatsMovement movement;            // the party's, on the level
};
HeroStatsOverview HeroStatsPages_Overview(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level);

// What the By level page adds up. HeroMetricMovement is the party's movement table instead of a
// number per champion (HeroStatsPages_Metric gives 0 for it).
enum HeroStatsMetric { HeroMetricDealt, HeroMetricKills, HeroMetricTaken, HeroMetricHealed, HeroMetricSpent,
                       HeroMetricMovement, HERO_METRIC_COUNT };
extern const char *const HERO_STATS_METRIC_LABELS[HERO_METRIC_COUNT];
int HeroStatsPages_Metric(const HeroStats &stats, HeroStatsChampion who, int level, int metric);

// The columns of the movement table, after the level
enum HeroStatsMovementColumn { HeroMoveSteps, HeroMoveTurns, HeroMoveBumps, HeroMoveTeleports, HeroMovePitFalls,
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
};
// casts: every cast, successful ones and failures
struct HeroStatsSpellLine { std::string name; int casts, successful, failed, mana, wasted, damage, kills; };
struct HeroStatsAmountLine { std::string text; int amount; bool isBad; };

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
  HeroStatsSummaries summaries;
};
HeroStatsHeroPage HeroStatsPages_Hero(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                      HeroStatsChampion who, int level, const HeroStatsNames &names);

#pragma pack(pop)

#endif
