// A monster's blow on a hero: MonsterDamagesCharacter() (Monster.cpp:4635)
// followed by DamageCharacter() (Defense.h).
#pragma once
#include "Defense.h"

namespace calc {

struct MonsterAttackResult {
  double reachChance = 0;      // the hero neither dodges nor is lucky
  double hitChance = 0;        // the blow costs hit points
  Dist damage;                 // P(hit points lost = d), d > 0
  Summary stats;               // of damage
  Dist rawDamage;              // before armour
  Summary bare;                // the same blow on the hero with nothing worn
  double meanProtection = 0;   // armour value met, 0..100
  double slotChance[6] = {};   // where the blow lands
  double noSlotChance = 0;     // ... or on no body part: no armour counts at all
  double woundChance = 0;      // per attack
  double woundSlot[6] = {};
  double poisonChance = 0;     // per attack
  Poison poison;
  double interval = 0;         // ticks from one attack to the next
  double dps = 0;              // one monster
  double groupDps = 0;         // all the monsters of Target::count striking this hero
  int attackers = 1;
  double hpShare = 0;          // mean blow as a share of the hero's hit points
  double attacksToKill = 0;    // mean attacks until the hero's hit points are gone
  double secondsToKill = 0;    // against the whole group
  int attacksToKillP05 = 0, attacksToKillMedian = 0, attacksToKillP95 = 0;
};

// Ticks between two attacks of one monster, as the timers add them up
// (Monster.cpp:3934 and :4119).  A monster that also has to turn or step
// takes longer.
double monsterAttackInterval(const MonsterType &m);

// interval <= 0 takes monsterAttackInterval()
MonsterAttackResult monsterAttack(const Hero &hero, const Party &party, const Target &target,
                                  const GameData &data, const Rules &rules, double interval = 0);

// One attack with dice; gives the hit points lost.  luck is carried along.
int sampleMonsterAttack(Draw &d, const Hero &hero, const Party &party, const Target &target,
                        const GameData &data, const Rules &rules, int &luck,
                        bool *poisoned = nullptr, int *wounds = nullptr);

}  // namespace calc
