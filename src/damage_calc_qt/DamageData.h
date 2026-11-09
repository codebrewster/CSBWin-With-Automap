// Tables of the damage calculator: weapons, attacks, monsters.
// DamageData.cpp is generated from ../damage_calc.py by gen_data.py.
#pragma once
#include <string>
#include <utility>
#include <vector>

struct Weapon {
  const char *name;
  int weight;      // 0.1 kg
  int ubyte1;      // weapon class (mastery selection)
  int ubyte2;      // strength bonus
  int armorDiv;    // 0, or 4 (Diamond Edge) / 8 (Hardcleave)
  bool vorpal;
};

struct Attack {
  const char *name;
  int p7;          // strength required (luck threshold 75-P7)
  int p8;          // damage factor, D7 = D7*P8/32
  int skill;       // skill improved / +10 bonus
  int time;        // recovery ticks
  int energy;      // base stamina cost
  int exp;         // base experience
  bool disrupt;
};

struct Monster {
  std::string label;
  std::string name;
  int armor;
  int dex;
  int level;       // level difficulty / experience multiplier
  int baseHealth;
  int exp;         // word16 bits 8-11
  bool nonMaterial;
};

// Wound bits of Character::wounds (CHARDESC::ouches)
enum Wound {
  WoundReadyHand = 1, WoundActionHand = 2, WoundHead = 4,
  WoundTorso = 8, WoundLegs = 16, WoundFeet = 32
};

struct Character {
  int strength = 55, dexterity = 43, luck = 50, load = 200;
  int stamina = 750, maxStamina = 750;
  int wounds = 0;          // bit mask, see Wound
  bool elvenBoots = false;
  int mastery[32] = {0};   // effective DetermineMastery per skill number
  int luckMin = 10;        // the game's Attributes[Luck].Minimum()
  int luckMax = 100;       // ... and Maximum()
};

const std::vector<Weapon> &weapons();
const std::vector<Attack> &attacks();
const std::vector<Monster> &monsters();
const std::vector<std::vector<int>> &weaponAttacks();
const std::vector<std::pair<int, int>> &defaultCombos();
Character defaultCharacter();
