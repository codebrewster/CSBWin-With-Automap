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

// Real time per game tick, measured by the player.  Normal speed gives about
// 4.2 ticks a second; while the party sleeps the game sets its speed to 0 and
// ticks on every vertical blank, which is far faster (58.8 measured).
struct TickRates {
  double awake = 4.2;
  double asleep = 58.8;
};
inline TickRates &tickRates() { static TickRates r; return r; }
inline double ticksPerSecond(bool sleeping = false)
{
  return sleeping ? tickRates().asleep : tickRates().awake;
}

// What a swing costs beyond the weapon's own delay.  The game handles one mouse
// click per tick and a swing takes two (the first opens the action panel, the
// second picks the action), and the hand only becomes ready after that tick's
// click has been handled, so even a perfect clicker swings once per
// delay + 2 ticks (measured: Eye of Time 1 + 2 = 3, Hardcleave 8 + 2 = 10).
// extra is the player's own slowness, ticks lost per swing.
struct SwingTiming {
  double input = 2.0;
  double extra = 0.0;
};
inline SwingTiming &swingTiming() { static SwingTiming t; return t; }
// Ticks from one swing to the next.  A swing at a monster keeps the full delay
// whether it hits or misses (measured: 96 swings at a pain rat, 43 hits, every
// one 1 + 2 ticks).  Only a swing that attacks no monster at all, or a Disrupt
// on a monster that is not non-material, gets the delay halved by integer
// division.
inline double swingPeriod(int delay, bool halved = false)
{
  return (halved ? delay / 2 : delay) + swingTiming().input + swingTiming().extra;
}

}  // namespace calc
