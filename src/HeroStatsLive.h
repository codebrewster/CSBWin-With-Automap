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
const int HERO_LIVE_FOOD_EMPTY = -1024;  // the lowest the game lets food and water fall

// One item a champion carries; a chest's contents follow the chest, with inChest set
struct HeroStatsLiveItem
{
  int slot;            // -1: the item on the cursor
  std::string name;    // as the inventory names it now (a spent Fury, an empty waterskin)
  bool inChest;
  int iconGraphic;     // the picture of the graphics file holding the inventory icon, -1: no icon
  int iconTile;        // the 16 x 16 tile in it
  int weight;          // in tenths of a kilogram; a chest's includes what is in it
  int charges;         // -1: this kind of item holds none
  bool gulps;          // the charges are a waterskin's gulps
  bool torch;          // the charges are a torch's fuel
  int food;            // what eating it adds to a champion's food, 0: it is no food
  int water;           // what a drink of it adds to the water (a waterskin: per gulp), 0: none
};
// An item with no icon, no weight and no charges
HeroStatsLiveItem HeroStatsLive_Item(int slot, const std::string &name, bool inChest);

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
  // The strength still to tick of each dose, strongest first (a dose of strength v hurts
  // max(1, v / 64) at each of its v ticks), and their sum: the poison on the champion
  std::vector<int> poisonDoses;
  int poison;
  int wounds;               // the wound bits
  std::vector<HeroStatsLiveItem> items;  // slot order
};

struct HeroStatsLiveParty
{
  std::vector<HeroStatsLiveChampion> champions;  // party order; a candidate in a mirror left out
  HeroStatsLiveItem cursor;                      // the item on the cursor, its name "" for none
  HeroStatsLiveParty() : cursor(HeroStatsLive_Item(-1, "", false)) {}
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
// What tells an item's icon from every other: graphic * 256 + tile, -1 without an icon
int HeroStatsLive_IconKey(const HeroStatsLiveItem &item);
// "0.9 kg"; a chest that holds something: "7.3 kg with contents"
std::string HeroStatsLive_WeightText(int tenths, bool withContents);
// "9 charges", "1 charge", "no charges"; a waterskin: "3 gulps left", "empty"; "" for -1
std::string HeroStatsLive_ChargesText(int charges, bool gulps, bool torch = false);
// The inventory as the game lays it out: the body slots (hands, head, neck, torso, legs, feet,
// pouch, quiver) on a grid of five by five, the backpack's seventeen slots after them
const int HERO_LIVE_DOLL_COLUMNS = 5;
const int HERO_LIVE_DOLL_ROWS = 5;
const int HERO_LIVE_FIRST_BACKPACK_SLOT = 13;
const int HERO_LIVE_BACKPACK_ACROSS = 6;
// The place of a body slot on that grid, column and row from 1; false for the backpack and outside
bool HeroStatsLive_DollCell(int slot, int *column, int *row);
// How many items follow items[index] inside it (0 when it is no chest, or empty)
int HeroStatsLive_HeldCount(const std::vector<HeroStatsLiveItem> &items, size_t index);
// The items a Party card sums up as "+N carried": not in a hand or on the head, neck, torso,
// legs or feet, and not inside a chest
int HeroStatsLive_CarriedCount(const std::vector<HeroStatsLiveItem> &items);
// The order the inventory list shows the items in: their places in items, sorted by slot (the
// order they come in), name or weight. What a chest holds stays under its chest, sorted the same
// way; by slot it keeps its own order.
enum HeroStatsLiveOrder { HeroLiveBySlot, HeroLiveByName, HeroLiveByWeight, HeroLiveByFood, HeroLiveByWater };
std::vector<size_t> HeroStatsLive_Order(const std::vector<HeroStatsLiveItem> &items, HeroStatsLiveOrder by, bool descending);
// The champion of the party with this fingerprint, or NULL
const HeroStatsLiveChampion *HeroStatsLive_Find(const HeroStatsLiveParty &party, HeroStatsChampion who);

#pragma pack(pop)

#endif
