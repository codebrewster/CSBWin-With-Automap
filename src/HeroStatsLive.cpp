// HeroStatsLive.cpp : see HeroStatsLive.h
#include "HeroStatsLive.h"

#include <algorithm>
#include <cstdlib>

std::string HeroStatsLive_SlotName(int slot)
{
  static const char *const FIXED[13] = {"Ready hand", "Action hand", "Head", "Torso", "Legs", "Feet", "Pouch 2",
                                        "Quiver 2", "Quiver 3", "Quiver 4", "Neck", "Pouch 1", "Quiver 1"};
  if (slot < 0 || slot >= HERO_LIVE_SLOT_COUNT) return "";
  if (slot < 13) return FIXED[slot];
  return "Backpack " + std::to_string(slot - 12);
}

const char *HeroStatsLive_AttributeName(int attribute)
{
  static const char *const NAMES[HERO_LIVE_ATTRIBUTE_COUNT] = {"Strength", "Dexterity", "Wisdom",
                                                               "Vitality", "Anti-magic", "Anti-fire"};
  if (attribute < 0 || attribute >= HERO_LIVE_ATTRIBUTE_COUNT) return "";
  return NAMES[attribute];
}

const char *HeroStatsLive_WoundName(int bit)
{
  static const char *const NAMES[HERO_LIVE_WOUND_COUNT] = {"Ready hand", "Action hand", "Head",
                                                           "Torso", "Legs", "Feet"};
  if (bit < 0 || bit >= HERO_LIVE_WOUND_COUNT) return "";
  return NAMES[bit];
}

std::string HeroStatsLive_Wounds(int mask)
{
  std::string out;
  for (int bit = 0; bit < HERO_LIVE_WOUND_COUNT; bit++)
  {
    if ((mask & (1 << bit)) == 0) continue;
    if (!out.empty()) out += ", ";
    out += HeroStatsLive_WoundName(bit);
  }
  return out;
}

std::string HeroStatsLive_Tenths(int tenths)
{
  std::string sign = tenths < 0 ? "-" : "";
  int magnitude = std::abs(tenths);
  return sign + std::to_string(magnitude / 10) + "." + std::to_string(magnitude % 10);
}

const HeroStatsLiveChampion *HeroStatsLive_Find(const HeroStatsLiveParty &party, HeroStatsChampion who)
{
  for (size_t i = 0; i < party.champions.size(); i++)
    if (party.champions[i].who == who) return &party.champions[i];
  return NULL;
}

HeroStatsLiveItem HeroStatsLive_Item(int slot, const std::string &name, bool inChest)
{
  HeroStatsLiveItem item = {slot, name, inChest, -1, 0, 0, -1, false, false, 0, 0};
  return item;
}

int HeroStatsLive_IconKey(const HeroStatsLiveItem &item)
{
  return item.iconGraphic < 0 ? -1 : item.iconGraphic * 256 + item.iconTile;
}

std::string HeroStatsLive_WeightText(int tenths, bool withContents)
{
  return HeroStatsLive_Tenths(tenths) + " kg" + (withContents ? " with contents" : "");
}

std::string HeroStatsLive_ChargesText(int charges, bool gulps, bool torch)
{
  if (charges < 0) return "";
  if (gulps) return charges == 0 ? "empty" : std::to_string(charges) + (charges == 1 ? " gulp left" : " gulps left");
  if (charges == 0) return torch ? "burnt out" : "no charges";
  return std::to_string(charges) + (charges == 1 ? " charge" : " charges");
}

bool HeroStatsLive_DollCell(int slot, int *column, int *row)
{
  // By slot: ready hand, action hand, head, torso, legs, feet, pouch 2, quiver 2 to 4, neck, pouch 1, quiver 1
  static const int PLACE[13][2] = {{1, 2}, {5, 2}, {3, 1}, {3, 2}, {3, 3}, {3, 4}, {1, 5},
                                   {5, 4}, {4, 5}, {5, 5}, {2, 1}, {1, 4}, {4, 4}};
  if (slot < 0 || slot >= HERO_LIVE_FIRST_BACKPACK_SLOT) return false;
  *column = PLACE[slot][0];
  *row = PLACE[slot][1];
  return true;
}

int HeroStatsLive_HeldCount(const std::vector<HeroStatsLiveItem> &items, size_t index)
{
  if (index >= items.size() || items[index].inChest) return 0;
  int count = 0;
  for (size_t i = index + 1; i < items.size() && items[i].inChest; i++) count++;
  return count;
}

int HeroStatsLive_CarriedCount(const std::vector<HeroStatsLiveItem> &items)
{
  int count = 0;
  for (size_t i = 0; i < items.size(); i++)
  {
    int slot = items[i].slot;
    bool wornOrHeld = (slot >= 0 && slot <= 5) || slot == 10;
    if (!items[i].inChest && !wornOrHeld) count++;
  }
  return count;
}

// Sorts places in items by name or weight; equal ones keep the order they came in
static void SortPlaces(const std::vector<HeroStatsLiveItem> &items, std::vector<size_t> *places, HeroStatsLiveOrder by, bool descending)
{
  if (by == HeroLiveBySlot)
  {
    if (descending) std::reverse(places->begin(), places->end());
    return;
  }
  std::stable_sort(places->begin(), places->end(), [&](size_t a, size_t b) {
    if (by == HeroLiveByFood) return descending ? items[a].food > items[b].food : items[a].food < items[b].food;
    if (by == HeroLiveByWater) return descending ? items[a].water > items[b].water : items[a].water < items[b].water;
    if (by == HeroLiveByWeight) return descending ? items[a].weight > items[b].weight : items[a].weight < items[b].weight;
    return descending ? items[a].name > items[b].name : items[a].name < items[b].name;
  });
}

std::vector<size_t> HeroStatsLive_Order(const std::vector<HeroStatsLiveItem> &items, HeroStatsLiveOrder by, bool descending)
{
  std::vector<size_t> outside;
  for (size_t i = 0; i < items.size(); i++)
    if (!items[i].inChest) outside.push_back(i);
  SortPlaces(items, &outside, by, descending);
  std::vector<size_t> order;
  for (size_t k = 0; k < outside.size(); k++)
  {
    order.push_back(outside[k]);
    std::vector<size_t> held;
    for (size_t i = outside[k] + 1; i < items.size() && items[i].inChest; i++) held.push_back(i);
    // What is in a chest has no slot of its own: by slot it stays as it lies
    if (by != HeroLiveBySlot) SortPlaces(items, &held, by, descending);
    order.insert(order.end(), held.begin(), held.end());
  }
  return order;
}
