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
  // The champion's rank in a class now and how far to the next (HeroStats_RankNow); -1 when not
  // known (a champion who left). May be NULL: never known.
  int (*rankNow)(HeroStatsChampion who, int cls, int *percentToNext);
};

// One row of the Monsters page: the monsters of one name, killed by each champion column, by
// doors, pits, telefrags, landings (the party arriving on them by teleporter or pit) and anything
// else (clouds, traps, monsters, the game). A game saved before the statistics knew telefrags as
// a cause has those kills in other: its rows do not say which monsters they were.
struct HeroStatsMonsterLine
{
  std::string name;
  std::vector<int> byColumn;
  int door, pit, other, total;
  int telefrag, landed;
};
// Most killed first. Variants the dungeon gives no name of their own share one line.
std::vector<HeroStatsMonsterLine> HeroStatsPages_Monsters(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                                          int level, const HeroStatsNames &names);
HeroStatsMonsterLine HeroStatsPages_AllMonsters(const std::vector<HeroStatsMonsterLine> &lines, size_t columnCount);

// One row of the Items page: the items of one name the party picked up the first time, the ones
// walls took (keys, coins, gems), and the ones the champions drank or ate
struct HeroStatsItemLine { std::string name; int pickedUp, fromChest, usedInWall, drunk, eaten; };
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
  std::vector<HeroStatsOptionLine> byColumn;  // the Party's page: each column's own numbers with it (no name)
};
// casts: every cast, successful ones and failures
struct HeroStatsSpellCounts { int casts, successful, failed, mana, wasted, damage, kills; };
// byColumn: on the Party's page, each column's own numbers with the spell
// mana: what all its casts cost, the failed ones included; wasted: the failed ones' part
struct HeroStatsSpellLine { std::string name; int casts, successful, failed, mana, wasted, damage, kills; std::vector<HeroStatsSpellCounts> byColumn; };
// byColumn: on the Party's page, each column's part of the amount
struct HeroStatsAmountLine { std::string text; int amount; bool isBad; std::vector<int> byColumn; };
// A death: who died (column: theirs in the columns, -1 for none), what killed them
// ("Mummy", "own Poison cloud"), on which level and when (the game tick, -1 when not known)
struct HeroStatsDeathLine { HeroStatsChampion who; int column; std::string killedBy; int level, tick; };

struct HeroStatsHeroPage
{
  std::vector<HeroStatsWeaponLine> weapons;   // most damage first, their options too
  std::vector<HeroStatsSpellLine> spells;     // most mana first
  std::vector<HeroStatsAmountLine> taken;     // biggest first
  std::vector<HeroStatsAmountLine> reduced;   // damage the defences took off, by harm source, biggest first
  std::vector<HeroStatsDeathLine> deaths;     // in the order they happened
  int wipes, reloads;
  // Food and water: everything eaten and drunk (the fountain included), a line per item (most
  // first; the drunk lines end with "Fountain"), what it gave and what was lost over full
  int eatenTotal, drunkTotal;
  std::vector<HeroStatsAmountLine> eaten, drunk;
  HeroStatsNourishment nourishment;
};
HeroStatsHeroPage HeroStatsPages_Hero(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                      HeroStatsChampion who, int level, const HeroStatsNames &names);

// A topic page (HeroStatsCatalogue): its summary tiles, its numbers and its detail tables

// One row of the Numbers table. byColumn: the Party's page: one value per column (empty for a
// party-only number); a champion's page: that champion's value. party: the party's value.
// label: what the row is called here; group: its place in the page's groups. A part whose sum
// does not apply to who (a champion's Kills under the party's Monsters killed) stands at depth 0
// under the number's own label.
struct HeroStatsNumbersRow
{
  HeroStatsNumber number;
  int depth;
  std::vector<int> byColumn;
  int party;
  bool partyOnly;
  std::string label;
  int group;
  std::string runShare;  // HeroStatsCatalogue_RunShare of the party's value: the time on a level
};
// A group of the page's numbers (HeroStatsTopicGroup) that has a row for who on the level
struct HeroStatsNumbersGroup { std::string title; bool panel; };
// HeroDetailHealing is no longer shown (the Numbers split potion healing by brewer); it keeps its
// place so the later sections keep their numbers (fold state, the HTML export)
enum HeroStatsDetail { HeroDetailWeapons, HeroDetailSpells, HeroDetailHarm, HeroDetailHealing, HeroDetailDeaths,
                       HeroDetailConsumed, HeroDetailItems, HeroDetailMonsters, HeroDetailSkills, HeroDetailXpSources,
                       HeroDetailRankUps, HeroDetailRecords, HeroDetailReduced, HERO_DETAIL_COUNT };

// Experience by source: a tree written out in drawing order. depth 0: Attacks, Monsters
// attacking you, Other actions, Spells, Other; under Attacks the weapons and their actions, under
// Spells the classes and their spells. note: dim text after the name ("Fighter · Swing", "Fire").
// id: the same for the same line on every page, for its fold state. byColumn: the Party's page,
// each column's amount; a champion's page: the party's amount.
// help: what a top line counts, for the hover
struct HeroStatsXpLine { std::string name, note; int depth; int id; int amount; std::vector<int> byColumn; bool hasChildren; std::string help; };
// A class: its experience, ranks and hidden skills (depth 1; "<class> itself" for gains straight to
// it). rankNow: the champion's (the Party's: the best champion's, best), with percentToNext;
// startRank: when the statistics first knew the champion (-1 unknown, and on the Party's page).
// tied: the Party's page: every champion at the best rank, best first, in column order.
struct HeroStatsClassLine
{
  std::string name;
  int rankNow, startRank, percentToNext, amount, ranksGained;
  HeroStatsChampion best;
  std::vector<HeroStatsChampion> tied;
  std::vector<int> byColumn;
  std::vector<HeroStatsXpLine> skills;
};
// rank: "Artisan → Adept"; with: what brought it ("SWING with DIAMOND EDGE", "Ful Ir Fireball");
// rankNumber: the rank reached (HeroXp_RankName's), for sorting; tick: when it came
struct HeroStatsRankUpLine { HeroStatsChampion who; std::string className, rank, with; int level; int rankNumber; int tick; };
// One record. value, how, whom, when as shown ("38.4 kg · 92%", "4 × Screamer", "tick 4,321"). who:
// the champion's name or "party". held: a champion's page: the champion holds the party's record.
// partyValue, partyHolder: the party's best and who holds it.
struct HeroStatsRecordLine
{
  HeroRecordKind kind;
  std::string group, label, value, who, how, whom, when;
  int level;
  int tick;                 // the game tick of the record (where a streak ended), -1 without one
  bool has, held;
  std::string partyValue;
  HeroStatsChampion partyHolder;
};
// The detail tables of a topic for who, in drawing order (Items only for the party)
std::vector<HeroStatsDetail> HeroStatsPages_Details(int topic, HeroStatsChampion who);
struct HeroStatsTopicPage
{
  std::vector<HeroStatsNumber> tiles;         // the tiles that apply, at most 4
  HeroStatsRecord record;                     // who's numbers on the level
  std::vector<HeroStatsNumbersRow> numbers;   // the topic's rows that apply
  std::vector<HeroStatsNumbersGroup> groups;  // their groups, in order
  std::vector<HeroStatsDetail> details;       // in drawing order
  HeroStatsHeroPage hero;                     // weapons, spells, taken, deaths, eaten, drunk
  std::vector<HeroStatsMonsterLine> monsters;
  std::vector<HeroStatsItemLine> items;
  std::vector<HeroStatsClassLine> classes;    // Experience: Fighter, Ninja, Priest, Wizard
  std::vector<HeroStatsXpLine> sources;       // Experience by source
  std::vector<HeroStatsRankUpLine> rankUps;   // newest first
  std::vector<HeroStatsRecordLine> records;   // Records, in HeroRecordKind order
  std::string summary;                        // HeroStatsCatalogue_Summary
};
HeroStatsTopicPage HeroStatsPages_Topic(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                        HeroStatsChampion who, int level, int topic, const HeroStatsNames &names);

// The Party's Overview: a card per champion; damagePercent: their share of the party's damage
struct HeroStatsCard { HeroStatsColumn column; HeroStatsRecord record; int damagePercent; };
std::vector<HeroStatsCard> HeroStatsPages_Cards(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level);

// Per level: one row per visited level (ascending), then the whole run (level HERO_STATS_WHOLE_RUN)
// runShare: one per value, HeroStatsCatalogue_RunShare (the time's share of the run); empty in the grid
struct HeroStatsLevelRow { int level; std::vector<int> values; std::vector<int> share; std::vector<std::string> runShare; };
struct HeroStatsPerLevel
{
  std::vector<HeroStatsNumber> numbers;  // the columns after the level
  HeroStatsNumber shareNumber;           // what share splits by column, when hasShare
  bool hasShare;
  std::vector<HeroStatsLevelRow> rows;
};
// The party's (who HERO_STATS_PARTY) or a champion's totals: the topic's top-level rows that
// apply to who on a level (Monsters: the causes under Monsters killed too). The party's rows share out the topic's first number kept per champion.
HeroStatsPerLevel HeroStatsPages_PerLevelTotals(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                                HeroStatsChampion who, int topic);
// Champions side by side: values are one per column, then the party; share one per column. With
// one champion, who is all of the party, neither table has a share.
HeroStatsPerLevel HeroStatsPages_PerLevelGrid(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                              HeroStatsNumber number);

#pragma pack(pop)

#endif
