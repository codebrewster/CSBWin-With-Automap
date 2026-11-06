// HeroStatsLive.cpp : see HeroStatsLive.h
#include "HeroStatsLive.h"

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
