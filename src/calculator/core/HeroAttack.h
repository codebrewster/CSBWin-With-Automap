// A hero's physical attack on a monster: the engine's pipeline as integer
// arithmetic with C semantics, written line by line after
//   MaxLoad()                    Code11f52.cpp:4443
//   StaminaAdjustedAttribute()   Code11f52.cpp:4416
//   Quickness()                  Character.cpp:2200
//   IsCharacterLucky()           Character.cpp:2156 (with the luck drift)
//   DetermineThrowingDistance()  Character.cpp:2258
//   DeterminePhysicalAttackDamage() Attack.cpp:518 (to-hit gate, power, graze
//                                chain, randomise/shrink chain, Vorpal, mastery)
//   _Attack()                    Attack.cpp:1797 (stamina: energy + coin flip)
// heroAttack() gives the exact distribution; sampleSwing() plays one swing
// with dice and is what the tests check the exact tables against.
#pragma once
#include <vector>
#include "Dist.h"
#include "Model.h"
#include "Rng.h"

namespace calc {

namespace eng {
int staminaAdjusted(const Hero &h, int stamina, int base);
int maxLoad(const Hero &h, int stamina);
// the three STRandom(8) rolls of Quickness()
int quickness(const Hero &h, int maxLoad, int a, int b, int c);
int throwingDistance(const Hero &h, const WeaponType &w, int hand, int maxLoad, int stamina,
                     int roll16);
// The monster's defence for one roll r32 of STRandom(32)
int armorTerm(const Target &t, const WeaponType &w, bool halved, int r32);
}  // namespace eng

struct SwingOptions {
  int swings = 1;             // length of the fight the numbers are averaged over
  bool staminaDrain = false;  // stamina falls from swing to swing
};

struct SwingResult {
  bool valid = false;
  int weapon = -1, action = 0;
  int minMastery = 0;         // skill level the action needs
  bool usable = true;         // the hero has that level
  Dist damage;                // per swing: P(damage = d) for d > 0; the rest misses
  double hitChance = 0;       // per swing
  double perSwing = 0;        // mean damage over all swings
  double perHit = 0;
  int minHit = 0, maxHit = 0;
  double ticks = 0;           // mean ticks a swing keeps the hero busy
  double dps = 0;             // damage per second
  double staminaPerSwing = 0;
  double damagePerStamina = 0;
  double expPerSwing = 0, expPerSecond = 0;
  double endStamina = 0;      // mean stamina after the fight
  double swingsToKill = 0;    // mean swings until the target's hit points are gone
  double secondsToKill = 0;
};

SwingResult heroAttack(const Hero &hero, const WeaponType &weapon, const ActionType &action,
                       const Target &target, const Rules &rules, const SwingOptions &options);

// One swing with dice.  state carries stamina and luck from swing to swing.
struct SwingState {
  int stamina;
  int luck;
};
struct SwingSample {
  int damage = 0;
  bool hit = false;
  int cost = 0;
};
SwingSample sampleSwing(Draw &d, const Hero &hero, const WeaponType &weapon,
                        const ActionType &action, const Target &target, const Rules &rules,
                        bool staminaDrain, SwingState &state);

// Mean number of draws from `damage` (misses included) until the sum reaches hp
double meanTriesToReach(const Dist &damage, int hp);
// P(the sum of n draws reaches hp), for n = 0..maxTries
std::vector<double> killCurve(const Dist &damage, int hp, int maxTries);

}  // namespace calc
