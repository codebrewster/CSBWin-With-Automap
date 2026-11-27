// What a calculation is given: a hero, the party's state, a monster on a
// level, and the rules in force.  Every value a formula reads is a field
// here, so each one can be typed in or come from a savegame.
#pragma once
#include <algorithm>
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
  bool negativeActionIsZero = false;   // the calculation uses 0 for a negative actionDefense
  int actionBonus() const { return negativeActionIsZero && actionDefense < 0 ? 0 : actionDefense; }

  // What is worn and held.  -1 = nothing.
  int weapon = -1;              // weapon type in the action hand
  int armour[6] = {-1, -1, -1, -1, -1, -1};   // armour type per slot; hands hold shields
  int handWeight[2] = {0, 0};   // weight of what each hand holds (ready, action)
  bool elvenBoots = false;      // on the feet: more load
  bool ekkhardCross = false;    // round the neck: faster healing
  bool gemOfAges = false;       // round the neck: +1 Heal (as the Sceptre of Lyf in the hand)

  int strength() const { return attr[Strength]; }
  int dexterity() const { return attr[Dexterity]; }
  int luck() const { return attr[Luck]; }
};

// DetermineMastery (Code17818.cpp): what the weapon in the action hand adds to the skill
// levels.  The Firestaff adds to every skill, the Sceptre of Lyf to Heal unless a Gem of Ages
// round the neck already does.
inline int allSkillsBonus(int weapon)
{
  return weapon == WeaponFirestaffA ? 1 : (weapon == WeaponFirestaffB ? 2 : 0);
}
inline int healBonus(const Hero &h, int weapon)
{
  return h.gemOfAges || weapon == WeaponSceptreOfLyf ? 1 : 0;
}
// Put another weapon (-1 = nothing) in the action hand: its weight and the skill levels it
// adds replace those of the weapon held before.  The load is left as it is.
inline void holdWeapon(Hero &h, int weapon, int weight)
{
  const int all = allSkillsBonus(weapon) - allSkillsBonus(h.weapon);
  const int heal = healBonus(h, weapon) - healBonus(h, h.weapon);
  for (int &m : h.mastery) m += all;
  h.mastery[SkillHeal] += heal;
  h.weapon = weapon;
  h.handWeight[1] = weight;
}

// The highest values the game's numbers can hold.  Attributes are single bytes; skill
// experience stops at 0x10000000 (LimitSkillExperience, CSB.h), which DetermineMastery turns
// into level 21 for every skill (the game names levels only up to Archmaster, 16); level-ups
// stop hit points at 999, stamina at 9999 and mana at 900 (MAXMANA) (Magic.cpp, LevelUp).
enum { MaxAttribute = 255, MaxSkillLevel = 21, MaxHitPoints = 999, MaxStaminaPoints = 9999,
       MaxManaPoints = 900 };

// Everything at the game's maximum, current and maximum alike, to see what a hero at the
// very top could do.  What is held and worn stays, and still adds its skill levels.
inline void maxOut(Hero &h)
{
  for (int a = 0; a < AttributeCount; a++) {
    h.attr[a] = h.attrMax[a] = MaxAttribute;
    if (h.attrMin[a] > MaxAttribute) h.attrMin[a] = MaxAttribute;
  }
  for (int k = 0; k < SkillCount; k++) {
    int level = MaxSkillLevel + allSkillsBonus(h.weapon);
    if (k == SkillHeal) level += healBonus(h, h.weapon);
    if (k == SkillDefend && h.ekkhardCross) level++;
    // a necklace the hero model does not record (Pendant Feral, Moonstone) keeps its level
    h.mastery[k] = std::max(h.mastery[k], level);
  }
  h.hp = h.maxHp = MaxHitPoints;
  h.stamina = h.maxStamina = MaxStaminaPoints;
  h.mana = h.maxMana = MaxManaPoints;
}

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
  bool shootExperience20 = true;    // SHOOT gives 20 experience where the table says 9 (DM 1.1;
                                    // this port's rule unless Nostalgia Mode is on)
};

// Real time per game tick.  It varies: Normal speed was measured at about 4.2
// ticks a second with no input and faster while fighting; 6 is the reference
// the numbers use.  While the party sleeps the game sets its speed to 0 and
// ticks on every vertical blank, which is far faster (58.8 measured).
struct TickRates {
  double awake = 6.0;
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
