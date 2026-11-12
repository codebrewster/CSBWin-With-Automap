// What a calculation is given: a hero, the party's state, a monster on a
// level, and the rules in force.  Every value a formula reads is a field
// here, so each one can be typed in or come from a savegame.
#pragma once
#include <string>
#include "GameData.h"

namespace calc {

struct Hero {
  std::string name = "Hero";
  int attr[AttributeCount] = {50, 55, 43, 40, 45, 30, 30};      // current
  int attrMin[AttributeCount] = {10, 10, 10, 10, 10, 10, 10};
  int attrMax[AttributeCount] = {100, 55, 43, 40, 45, 30, 30};
  int hp = 100, maxHp = 100;
  int stamina = 750, maxStamina = 750;
  int mana = 30, maxMana = 30;
  int load = 200;               // 0.1 kg
  int wounds = 0;               // Wound bits
  int food = 1500, water = 1500;
  int mastery[SkillCount] = {5, 4, 3, 3, 5, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3};
  int shield = 0;               // shieldStrength: a potion's or spell's own shield
  int actionDefense = 0;        // word64: protection of the action under way

  // What is worn and held.  -1 = nothing.
  int weapon = -1;              // weapon type in the action hand
  int armour[6] = {-1, -1, -1, -1, -1, -1};   // armour type per slot; hands hold shields
  int handWeight[2] = {0, 0};   // weight of what each hand holds (ready, action)
  bool elvenBoots = false;      // on the feet: more load
  bool ekkhardCross = false;    // round the neck: faster healing

  int strength() const { return attr[Strength]; }
  int dexterity() const { return attr[Dexterity]; }
  int luck() const { return attr[Luck]; }
};

struct Party {
  int partyShield = 0;          // d.PartyShield (YA IR)
  int spellShield = 0;          // d.SpellShield
  int fireShield = 0;           // d.FireShield
  bool sleeping = false;
};

struct Target {
  MonsterType type;
  int level = 1;                // the level's experience multiplier
  int hp = 0;                   // hit points of one monster
  int count = 1;                // monsters in the fight (struck together, or striking the hero)
  bool attacking = false;       // caught while it attacks
};

// Hit points a newly made monster gets: baseHealth * level + STRandom(baseHealth/4 + 1)
inline int monsterHpMin(const MonsterType &m, int level) { return m.baseHealth * level; }
inline int monsterHpMax(const MonsterType &m, int level)
{
  return m.baseHealth * level + m.baseHealth / 4;
}

struct Rules {
  bool vulnerableOnAttack = false;  // an attacking monster has half its defence
  int shotDamageFactor = 1;         // a missile from a launcher hits this many times as hard
  bool moduloBias = true;           // STRandom(n) as the Atari had it
  bool luckDrift = true;            // IsCharacterLucky moves Luck by 2 each call
};

// Game speed: the clock runs at 6 ticks a second at normal speed
constexpr double kTicksPerSecond = 6.0;

}  // namespace calc
