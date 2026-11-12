// Damage spells, cast by a hero at monsters or by a monster at the party:
//   CastSpell()              Magic.cpp:1124     success, missile strength
//   CharacterThrowsSpell()   Magic.cpp:502      how fast the missile fades
//   ProcessMissile           Timer.cpp:225      fading in flight
//   DetermineMagicDamage()   Code11f52.cpp:1657 the missile's own blow
//   ProcessMissileEncounter  Code11f52.cpp:1815 on a monster / on the party
//   CreateCloud()            Code11f52.cpp:1469 the explosion of fire and lightning
//   ProcessTT_25()           Timer.cpp:445      poison cloud, harm non-material
//   MonsterAttacks()         Monster.cpp:954    a monster's missiles
#pragma once
#include <string>
#include <vector>
#include "Defense.h"

namespace calc {

// The spells that do damage, in the order they are shown
const std::vector<int> &damageMissiles();
const char *missileName(int missile);
// The spell of the table that launches this missile; -1 if none
int spellForMissile(const GameData &data, int missile);

// A missile at the moment it arrives
struct MissileState {
  bool arrives = true;
  int range = 0;        // rangeRemaining: what it has left
  int damage = 0;       // damageRemaining
};
// After `steps` half squares of flight, fading by `decay` each
MissileState flyMissile(int range, int damage, int decay, int steps);
// Half squares a missile flies before it meets a target `squares` away
// (1 = the next square); backRow = the target stands in the far half
int flightSteps(int squares, bool backRow);

struct Cast {
  bool known = false;       // the table has such a spell
  int spell = -1;
  int power = 1;            // 1..6
  int mana = 0;
  int levelNeeded = 0;      // difficulty + power
  int level = 0;            // the hero's level in the spell's skill
  double success = 1;       // chance the cast does not fail for lack of practice
  int range = 0, damage = 90, decay = 1;   // the missile as it leaves the hand
};
Cast castSpell(const GameData &data, const Hero &hero, int missile, int power);

// The shield spells (Magic.cpp:1366 YA IR, Magic.cpp:1432 + Attack.cpp:1218 FUL BRO NETA)
enum ShieldSpell { ShieldYaIr, ShieldFulBroNeta };
struct ShieldCast {
  int added = 0;      // what one cast adds to the shield
  int ticks = 0;      // how long it lasts before the same amount is taken off
};
// `current` = the shield the party already has: above 50 a new cast adds only a quarter
ShieldCast castShield(ShieldSpell spell, int power, int current);

// DetermineMagicDamage for a spell missile
Dist magicDamage(int missile, const MissileState &at, Dice &dice);
int sampleMagicDamage(Draw &d, int missile, const MissileState &at, int *poison = nullptr);

struct MonsterHit {
  bool arrives = true;
  bool noEffect = false;    // this monster cannot be harmed this way
  std::string note;         // why, or what to know
  Dist struck;              // hit points the monster it strikes loses (all its parts)
  Dist others;              // ... and each other monster of the group
  Dist perTick;             // poison cloud: each monster, each tick it stays inside
  int ticks = 0;            // poison cloud: how long it lasts
  Summary stats;            // of struck
  double killChance = 0;    // struck >= Target::hp
};
MonsterHit missileOnMonster(int missile, const MissileState &at, const Target &target,
                            Dice &dice);
// With dice: the struck monster's loss (poison cloud: one tick at full strength)
int sampleMissileOnMonster(Draw &d, int missile, const MissileState &at, const Target &target,
                           bool struck = true);

struct HeroHit {
  bool arrives = true;
  std::string note;
  Dist struck;              // the hero the missile hits
  Dist others;              // each other hero (explosion, cloud)
  Dist perTick;             // poison cloud: each hero, each tick inside
  int ticks = 0;
  Summary stats;            // of struck
  double poisonChance = 0;
  Dist poisonStrength;      // when poisoned
  double woundChance = 0;
};
HeroHit missileOnHero(int missile, const MissileState &at, const Hero &hero, const Party &party,
                      const GameData &data, Dice &dice);
int sampleMissileOnHero(Draw &d, int missile, const MissileState &at, const Hero &hero,
                        const Party &party, const GameData &data, bool struck = true);

// What a monster throws, with the chance of each (MissileZo does no harm)
struct MonsterMissile {
  int missile;
  double chance;
};
std::vector<MonsterMissile> monsterMissiles(const MonsterType &m);
// The missile as it leaves the monster: the range is rolled
struct MonsterLaunch {
  Dist range;
  int damage = 0;
  int decay = 8;
};
MonsterLaunch monsterLaunch(const MonsterType &m, Dice &dice);

}  // namespace calc
