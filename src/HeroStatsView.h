#ifndef HEROSTATSVIEW_H
#define HEROSTATSVIEW_H

#include <stdint.h>
#include <string>
#include <vector>
#include "HeroStats.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// What the statistics window shows, made from a HeroStats. No engine, no ImGui.

// The mark between thousands: "," (the default), ".", " ", "'" or "" for none
void HeroStatsView_SetThousandsSeparator(const std::string &separator);
const std::string &HeroStatsView_ThousandsSeparator(void);
// csb.cfg's stats_separator ("comma", "dot", "space", "apostrophe", "none") as the mark; "," for
// anything else
std::string HeroStatsView_SeparatorOfSetting(const std::string &setting);
std::string HeroStatsView_Number(int value);             // 8931 -> "8,931"
std::string HeroStatsView_Percent(int part, int whole);  // "64%", "-" when whole == 0

struct HeroStatsColumn { HeroStatsChampion id; std::string name; bool hasLeft; };
// inParty: fingerprints of the champions now in the party, slot order. Result: those, then the
// champions who left (hasLeft). The Party column is not included.
std::vector<HeroStatsColumn> HeroStatsView_Columns(const HeroStats &stats,
                                                   const std::vector<HeroStatsChampion> &inParty);

// previous/next: -2 none, -1 whole run, else a level
struct HeroStatsPicker { bool stepper; std::vector<int> levels; int previous, next; };
HeroStatsPicker HeroStatsView_Picker(const HeroStats &stats, int shownLevel);

struct HeroStatsSummaries { std::string attacks, spells, harm, healing, hardship; };
HeroStatsSummaries HeroStatsView_Summaries(const HeroStats &stats, HeroStatsChampion who, int level);

// "Ful Ir Fireball", the special rows, "Ful Ya Ku (no spell)", "(no runes)" for key 0
std::string HeroStatsView_SpellName(uint32_t spell);
// The missile of a monster's blow (HERO_SPELL_MONSTER_MISSILE | kind): "Fireball", "Lightning",
// "Poison blob" ...; "" for anything else
std::string HeroStatsView_MissileName(uint32_t spell);
// "Monster attacks", "Fireball" (a monster's), "Poison", "Pits", "Friendly fire (from Tiggy)" ...
// causer: the column of row.causer when it is a champion, else 0
std::string HeroStatsView_HarmName(const HeroStatsHarmRow &row, const HeroStatsColumn *causer,
                                   const std::string &monsterName);
// "Mummy · Lv 4", "Fireball from Lord Order · Lv 3", "own Fireball · Lv 9", "Fireball from Tiggy · Lv 5", "Fell into a pit · Lv 7".
// causerName: the monster's name or the causing champion's name; empty when neither. The level
// is the number the automap shows (the level index, not index + 1).
std::string HeroStatsView_DeathName(const HeroStatsDeath &death, const std::string &causerName);

#pragma pack(pop)

#endif
