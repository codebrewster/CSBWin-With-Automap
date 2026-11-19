#ifndef HEROSTATSDEFENCE_H
#define HEROSTATSDEFENCE_H

#include <vector>

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// What took damage off a hit on a champion, as DamageCharacter works it out. No engine: the hooks
// hand in the steps, this splits what they took off.
//
// DamageCharacter lowers the damage in steps that depend on the attack type: resistance
// (anti-fire, anti-magic) and the party's fire or spell shield take points off the damage; then
// for most types protection multiplies it by (130 - protection) / 64, and for a psychic attack
// wisdom multiplies it by (115 - wisdom) / 64. Protection under 66 makes a hit bigger than its
// roll, so the measure is the damage the champion would take with no defence at all: the roll
// times 130 / 64 (115 / 64 for a psychic attack, the roll itself for a magic one). Everything is
// in 64ths of a hit point, where all of these are whole numbers.
//
// Protection is a sum for each part of the body the attack aims at (TAG01680a): the armour worn
// there, shields in the hands, a random share of Vitality, the champion's shield (a Ya potion),
// the party shield, the stance of the champion's last action, less a wound on that part; halved
// while the party sleeps, then halved and held to 0 ... 100. Since damage falls in a straight line
// with protection, what protection took off is shared out by each component's points.

// The parts in the order the .map file keeps them: new ones go to the end
enum HeroDefencePart { HeroDefenceResistance, HeroDefenceShieldSpell, HeroDefenceWisdom,
                       HeroDefenceArmour, HeroDefenceHandShields, HeroDefenceVitality, HeroDefenceChampionShield,
                       HeroDefencePartyShield, HeroDefenceStance, HeroDefenceWounds, HeroDefenceAsleep,
                       HERO_DEFENCE_PART_COUNT };
// The components of protection: HeroDefenceArmour up to the end
const int HERO_DEFENCE_FIRST_PROTECTION = HeroDefenceArmour;
const int HERO_DEFENCE_PROTECTION_PARTS = HERO_DEFENCE_PART_COUNT - HERO_DEFENCE_FIRST_PROTECTION;

// One part of the body as TAG01680a added it up. points: each component's points in the order of
// HeroDefenceArmour ... HeroDefenceAsleep (wound and asleep are 0 or less; asleep is what the
// halving took). value: the protection TAG01680a returned for that part.
struct HeroProtectionPart { int points[HERO_DEFENCE_PROTECTION_PARTS]; int value; };

// The scale of the last step: what one point of damage left after the shields is, in 64ths, with
// no defence
enum HeroDefenceScale { HeroScaleNone = 64, HeroScaleProtection = 130, HeroScaleWisdom = 115 };

// raw: the damage DamageCharacter was given; afterResistance, afterShield: after those steps (the
// raw damage when the attack type has none); final: what was left before the dungeon's damage
// filter. parts: the parts of the body protection was worked out for (HeroScaleProtection only).
struct HeroDamageSteps
{
  int raw, afterResistance, afterShield, final;
  HeroDefenceScale scale;
  std::vector<HeroProtectionPart> parts;
};

// What each part took off, in 64ths of a hit point. They add up to the damage with no defence less
// the final damage, but for the part of a 64th the engine's rounding leaves when protection is 0.
// Stance, wounds and sleep can come out below 0: they made protection smaller.
void HeroDefence_Split(const HeroDamageSteps &steps, int out[HERO_DEFENCE_PART_COUNT]);

#pragma pack(pop)

#endif
