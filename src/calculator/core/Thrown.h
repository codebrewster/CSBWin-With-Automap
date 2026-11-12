// An object thrown by hand at a monster, as an exact table and with dice:
//   ThrowByCharacter()         Mouse.cpp:699      range, damage and decay as it leaves the hand
//   LaunchMissile()            Magic.cpp:22       range kept to a byte, at most 255 (a deliberate
//                                                 wrap: release ub() is a plain (ui8) cast, CSBTypes.h:163)
//   DetermineMagicDamage()     Code11f52.cpp:1657 the strength of a weapon that arrives
//   ProcessMissileEncounter()  Code11f52.cpp:1815 monster branch: 64 * strength / defence
//   TAG016382()                Mouse.cpp:678      stamina of a throw, by the object's weight
//   _Attack()                  Attack.cpp:1790    the THROW (42) and SHOOT (32) actions: delay,
//                                                 stamina, experience, and what happens after
//   AdjustSkills()             Magic.cpp:207      how the experience is scaled
// The flight itself (flyMissile, flightSteps) is shared with the spell missiles.
#pragma once
#include "Defense.h"
#include "Spells.h"

namespace calc {

// A missile as it leaves the hand; range and damage are independent tables
struct Launch {
  Dist range;
  Dist damage;
  int decay = 5;
};
Launch handLaunch(const Hero &hero, const WeaponType &object, Dice &dice);

struct SampledLaunch {
  int range = 0, damage = 0, decay = 5;
};
SampledLaunch sampleHandLaunch(Draw &d, const Hero &hero, const WeaponType &object);

// The tail of DetermineMagicDamage for a weapon: base distance + weight / 2
Dist arrivalStrength(const WeaponType &object, const MissileState &at, Dice &dice);
int sampleArrivalStrength(Draw &d, const WeaponType &object, const MissileState &at);

// Hit points a monster loses per throw (missing mass = no damage).  `squares` and `backRow`
// as for flightSteps; shotFactor multiplies the strength of a shot from a launcher.
Dist damageOnMonster(const Launch &launch, const WeaponType &object, const Target &target,
                     int squares, bool backRow, int shotFactor, Dice &dice);
int sampleDamageOnMonster(Draw &d, SampledLaunch l, const WeaponType &object,
                          const Target &target, int squares, bool backRow, int shotFactor);

// A shot from a launcher in one hand with ammunition in the other (atk_SHOOT, Attack.cpp:1933);
// call canShoot first: shotLaunch does not check the pair
bool canShoot(const WeaponType &launcher, const WeaponType &ammo);
Launch shotLaunch(const WeaponType &launcher, const WeaponType &ammo, const Hero &hero, const Rules &rules);
SampledLaunch sampleShotLaunch(Draw &d, const WeaponType &launcher, const WeaponType &ammo,
                               const Hero &hero, const Rules &rules);

// Stamina a throw takes for the object's weight (TAG016382, Mouse.cpp:678, with the 11.024 fix)
int throwStamina(int weight);

// One throw or shot, as heroAttack gives one swing.  Experience assumes a monster attacked the
// party in the last 25 ticks (doubled), as heroAttack does.
struct ThrowResult {
  bool valid = false;
  bool shot = false;
  int object = -1, launcher = -1;
  bool usable = true;         // the hero has the skill level the action table asks
  std::string note;           // "Need ammo", "fades", and what the source does not settle
  Dist damage;                // per throw: P(damage = d) for d > 0; the rest does no harm
  double arriveChance = 0;    // P(the missile reaches the target's square)
  double perThrow = 0, perHit = 0, killChance = 0;
  int minHit = 0, maxHit = 0;
  double ticks = 0, dps = 0, stamina = 0, expPerThrow = 0, expPerSecond = 0, throwsToKill = 0,
         secondsToKill = 0;
};
// launcher == nullptr: object thrown by hand; else object is the ammunition shot from launcher
ThrowResult throwAttack(const GameData &g, const Hero &hero, const WeaponType &object,
                        const WeaponType *launcher, const Target &target, const Rules &rules,
                        int squares, bool backRow);

}  // namespace calc
