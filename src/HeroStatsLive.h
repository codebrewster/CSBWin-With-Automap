// HeroStatsLive.h : the champions of the party as they are now, what the game's inventory screen
// shows of each: health, stamina, mana, food, water, load, the attributes, the class ranks,
// poison, wounds and every item carried. The statistics window's Champions tab draws it.
//
// The engine fills it (HeroStats_LiveParty in HeroStatsHooks.cpp) by reading the champions only:
// nothing here opens a champion's screen or changes the game, so a replay watched with the tab
// open stays in step with its recording. No engine, no ImGui.
#ifndef HEROSTATSLIVE_H
#define HEROSTATSLIVE_H

#include <string>
#include <vector>

#include "HeroStats.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// The possession slots of a champion, as the game numbers them (CHARDESC::Possession)
const int HERO_LIVE_SLOT_COUNT = 30;
// Strength, Dexterity, Wisdom, Vitality, Anti-magic, Anti-fire: the game's Attributes[1..6]
const int HERO_LIVE_ATTRIBUTE_COUNT = 6;
// The wound bits of CHARDESC::ouches: ready hand, action hand, head, torso, legs, feet
const int HERO_LIVE_WOUND_COUNT = 6;
// Food and water as the game counts them: full at this, starving or parched below 0
const int HERO_LIVE_FOOD_FULL = 2048;

// One item a champion carries; a chest's contents follow the chest, with inChest set
struct HeroStatsLiveItem
{
  int slot;
  std::string name;
  bool inChest;
};

struct HeroStatsLiveChampion
{
  HeroStatsChampion who;
  std::string name;
  std::string title;
  int health, maxHealth;
  int stamina, maxStamina;  // in tenths, as the engine keeps it; the game shows stamina / 10
  int mana, maxMana;
  int food, water;          // up to HERO_LIVE_FOOD_FULL
  int load, maxLoad;        // in tenths of a kilogram
  int attributeNow[HERO_LIVE_ATTRIBUTE_COUNT];
  int attributeMax[HERO_LIVE_ATTRIBUTE_COUNT];
  // Per class (Fighter, Ninja, Priest, Wizard): the mastery (HeroXp_Mastery, 1 no rank) and how
  // far to the next rank, 0-99
  int rank[4];
  int percentToNext[4];
  int poisonCount;          // poison doses working on the champion; 0 not poisoned
  int wounds;               // the wound bits
  std::vector<HeroStatsLiveItem> items;  // slot order
};

struct HeroStatsLiveParty
{
  std::vector<HeroStatsLiveChampion> champions;  // party order; a candidate in a mirror left out
  std::string inHand;                            // the item on the cursor, "" for none
};

// "Ready hand", "Action hand", "Head", "Torso", "Legs", "Feet", "Pouch 2", "Quiver 2" .. "Quiver
// 4", "Neck", "Pouch 1", "Quiver 1", "Backpack 1" .. "Backpack 17"; "" outside the slots
std::string HeroStatsLive_SlotName(int slot);
const char *HeroStatsLive_AttributeName(int attribute);  // "" outside them
const char *HeroStatsLive_WoundName(int bit);            // "" outside them
// The wounded parts, "Head, Feet"; "" when none
std::string HeroStatsLive_Wounds(int mask);
// Tenths as the game writes them: 123 -> "12.3", -5 -> "-0.5"
std::string HeroStatsLive_Tenths(int tenths);
// The champion of the party with this fingerprint, or NULL
const HeroStatsLiveChampion *HeroStatsLive_Find(const HeroStatsLiveParty &party, HeroStatsChampion who);

#pragma pack(pop)

#endif
