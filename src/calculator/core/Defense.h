// What stands between a blow and a hero's hit points:
//   TAG01680a()        Character.cpp:2383  protection of one body part
//   TAG016426()        Character.cpp:2029  damage scaled by an attribute
//   DamageCharacter()  Character.cpp:2704  armour, shields, wounds
//   PoisonCharacter()  CSBCode.cpp:7290
// Each comes as an exact table and as a dice version for the tests.
#pragma once
#include "Dist.h"
#include "Model.h"
#include "Rng.h"

namespace calc {

// DamageCharacter's P4
enum DamageKind {
  KindPlain = 0,      // nothing reduces it (poison, exhaustion)
  KindFire = 1,       // Anti-Fire, the fire shield, then armour
  KindBlunt2 = 2,     // armour counts half (walls, falls)
  KindBlunt = 3,      // armour
  KindSharp = 4,      // armour, scaled by its sharp resistance
  KindMagic = 5,      // Anti-Magic and the spell shield, no armour
  KindPsychic = 6,    // Wisdom, no armour
  KindMissile = 7     // armour (lightning)
};
const char *damageKindName(int kind);

// TAG016426: damage * (170 - attribute) / 128
int scaleByAttribute(int attribute, int damage);

struct HarmResult {
  Dist damage;                 // P(hit points lost = d), d > 0; what is missing did no harm
  double woundChance = 0;      // P(at least one body part is newly wounded or hit again)
  double woundSlot[6] = {};    // the same per body part
  Dist protection;             // the armour value the blow met (0..100), when armour counted
};

// `raw` is the damage handed to DamageCharacter with the body parts in mask
HarmResult damageHero(const Dist &raw, int mask, int kind, const Hero &hero, const Party &party,
                      const GameData &data, Dice &dice);
int sampleDamageHero(Draw &d, int damage, int mask, int kind, const Hero &hero,
                     const Party &party, const GameData &data, int *wounds = nullptr);

// Protection of one body part (0..100)
Dist slotProtection(int slot, bool sharp, const Hero &hero, const Party &party,
                    const GameData &data, Dice &dice);

// DamageAllCharacters (Code11f52.cpp:4605): what one hero is dealt out of `damage`
Dist shareOfPartyDamage(const Dist &damage, Dice &dice);

struct Poison {
  int strength = 0;
  int totalDamage = 0;         // over its whole course, nothing reduces it
  int ticks = 0;               // how long it lasts
};
Poison poisonCourse(int strength);

// The hero without anything worn or held, for "what the armour saves"
Hero withoutArmour(const Hero &hero);

}  // namespace calc
