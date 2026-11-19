#include "TestSupport.h"
#include "../HeroStats.h"
#include "../HeroStatsDefence.h"
#include "../HeroStatsCatalogue.h"
#include "../HeroStatsPages.h"

// One part of the body: the points of armour, shields in hand, vitality, shield potion, party
// shield, stance, wound and sleep, and the protection the engine made of them
static HeroProtectionPart Part(int armour, int vitality, int stance, int wound, int asleep, int value)
{
  HeroProtectionPart part = {};
  part.points[HeroDefenceArmour - HERO_DEFENCE_FIRST_PROTECTION] = armour;
  part.points[HeroDefenceVitality - HERO_DEFENCE_FIRST_PROTECTION] = vitality;
  part.points[HeroDefenceStance - HERO_DEFENCE_FIRST_PROTECTION] = stance;
  part.points[HeroDefenceWounds - HERO_DEFENCE_FIRST_PROTECTION] = wound;
  part.points[HeroDefenceAsleep - HERO_DEFENCE_FIRST_PROTECTION] = asleep;
  part.value = value;
  return part;
}

static HeroDamageSteps Steps(int raw, int afterResistance, int afterShield, int final, HeroDefenceScale scale)
{
  HeroDamageSteps steps = {raw, afterResistance, afterShield, final, scale, {}};
  return steps;
}

static int Sum(const int *out)
{
  int sum = 0;
  for (int k = 0; k < HERO_DEFENCE_PART_COUNT; k++) sum += out[k];
  return sum;
}

static void ProtectionIsMeasuredFromNoDefence(void)
{
  // A roll of 20 against protection 22 (armour 40 and vitality 4, halved): 20 * 108 / 64 = 33.
  // With no defence it would be 20 * 130 / 64 = 40.6, so 2600 - 33 * 64 = 488 64ths were taken off.
  HeroDamageSteps steps = Steps(20, 20, 20, 33, HeroScaleProtection);
  steps.parts.push_back(Part(40, 4, 0, 0, 0, 22));
  int out[HERO_DEFENCE_PART_COUNT];
  HeroDefence_Split(steps, out);
  CHECK(out[HeroDefenceArmour] == 444 && out[HeroDefenceVitality] == 44);
  CHECK(out[HeroDefenceResistance] == 0 && out[HeroDefenceShieldSpell] == 0 && out[HeroDefenceWisdom] == 0);
  CHECK(Sum(out) == 488);
}

static void ResistanceAndShieldsCountInTheSameMeasure(void)
{
  // Fire: anti-fire takes 30 to 20, the fire shield 20 to 15, no protection: 15 * 130 / 64 = 30.
  // Resistance and shield count times 130 / 64 like protection; the engine's rounding (30 64ths)
  // belongs to nobody.
  HeroDamageSteps steps = Steps(30, 20, 15, 30, HeroScaleProtection);
  steps.parts.push_back(Part(0, 0, 0, 0, 0, 0));
  int out[HERO_DEFENCE_PART_COUNT];
  HeroDefence_Split(steps, out);
  CHECK(out[HeroDefenceResistance] == 1300 && out[HeroDefenceShieldSpell] == 650);
  CHECK(Sum(out) == 1950);
  // The shield takes all that is left: the engine stops there, before protection
  HeroDefence_Split(Steps(10, 8, -2, 0, HeroScaleProtection), out);
  CHECK(out[HeroDefenceResistance] == 260 && out[HeroDefenceShieldSpell] == 1040 && Sum(out) == 1300);
}

static void MagicAndPsychicAttacks(void)
{
  int out[HERO_DEFENCE_PART_COUNT];
  // Magic: anti-magic and the spellshield, no protection, measured from the roll itself
  HeroDefence_Split(Steps(20, 15, 5, 5, HeroScaleNone), out);
  CHECK(out[HeroDefenceResistance] == 320 && out[HeroDefenceShieldSpell] == 640 && Sum(out) == 960);
  // Psychic: wisdom 50 makes 20 into 20 * 65 / 64 = 20; with no wisdom it would be 20 * 115 / 64
  HeroDefence_Split(Steps(20, 20, 20, 20, HeroScaleWisdom), out);
  CHECK(out[HeroDefenceWisdom] == 20 * 115 - 20 * 64 && Sum(out) == 1020);
}

static void DrawbacksComeOutBelowZero(void)
{
  // Armour 60, vitality 4, stance -10, a wound -10: 44, halved asleep to 22 (asleep -22), then
  // halved to protection 11. A roll of 64: 64 * 119 / 64 = 119, 64 * 11 = 704 64ths taken off.
  HeroDamageSteps steps = Steps(64, 64, 64, 119, HeroScaleProtection);
  steps.parts.push_back(Part(60, 4, -10, -10, -22, 11));
  int out[HERO_DEFENCE_PART_COUNT];
  HeroDefence_Split(steps, out);
  CHECK(out[HeroDefenceArmour] == 1920 && out[HeroDefenceVitality] == 128);
  CHECK(out[HeroDefenceStance] == -320 && out[HeroDefenceWounds] == -320 && out[HeroDefenceAsleep] == -704);
  CHECK(Sum(out) == 704);
  // Drawbacks bigger than the rest: no protection, nothing to share
  steps = Steps(64, 64, 64, 130, HeroScaleProtection);
  steps.parts.push_back(Part(4, 2, -10, -10, 0, 0));
  HeroDefence_Split(steps, out);
  CHECK(Sum(out) == 0);
}

static void TheCapAndSeveralParts(void)
{
  int out[HERO_DEFENCE_PART_COUNT];
  // Armour 300 would be 150, held to 100: armour gets all of it
  HeroDamageSteps steps = Steps(64, 64, 64, 30, HeroScaleProtection);
  steps.parts.push_back(Part(300, 0, 0, 0, 0, 100));
  HeroDefence_Split(steps, out);
  CHECK(out[HeroDefenceArmour] == 6400 && Sum(out) == 6400);
  // Two parts of the body averaged: one armoured (20), one with only vitality (10), protection 15
  steps = Steps(64, 64, 64, 115, HeroScaleProtection);
  steps.parts.push_back(Part(40, 0, 0, 0, 0, 20));
  steps.parts.push_back(Part(0, 20, 0, 0, 0, 10));
  HeroDefence_Split(steps, out);
  CHECK(out[HeroDefenceArmour] == 640 && out[HeroDefenceVitality] == 320);
}

static void StoreCountsAttacksAndDefences(void)
{
  HeroStats stats;
  stats.CountMonsterAttack(7, 1, 12, HeroAttackHit);
  stats.CountMonsterAttack(7, 1, 12, HeroAttackDodged);
  stats.CountMonsterAttack(7, 1, 12, HeroAttackDodged);
  stats.CountMonsterAttack(7, 2, 3, HeroAttackLucky);
  stats.CountMonsterAttack(9, 2, 3, HeroAttackTooWeak);
  stats.CountMonsterAttack(9, 2, 3, HeroAttackBlocked);
  int parts[HERO_DEFENCE_PART_COUNT] = {};
  parts[HeroDefenceArmour] = 444;
  parts[HeroDefenceVitality] = 44;
  stats.CreditDefence(7, 1, HeroHarmMonster, parts);
  int fire[HERO_DEFENCE_PART_COUNT] = {};
  fire[HeroDefenceResistance] = 1300;
  fire[HeroDefenceShieldSpell] = 650;
  fire[HeroDefenceAsleep] = -64;
  stats.CreditDefence(7, 2, HeroHarmMonsterMissile, fire);

  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.MonsterAttacks(7, HERO_STATS_WHOLE_RUN) == 4 && read.MonsterAttacks(7, 1, HeroAttackDodged) == 2);
  CHECK(read.MonsterAttacks(HERO_STATS_PARTY, 2) == 3 && read.MonsterAttacks(9, 2, HeroAttackBlocked) == 1);
  CHECK(read.Defence(7, 1, HeroDefenceArmour) == 444 && read.Defence(7, HERO_STATS_WHOLE_RUN, -1) == 488 + 1950 - 64);
  CHECK(read.Defence(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, -1, HeroHarmMonster) == 488);

  CHECK(HeroStatsCatalogue_Value(read, 7, 1, HeroNumAttacksReceived) == 3);
  CHECK(HeroStatsCatalogue_Value(read, 7, 1, HeroNumHitsTaken) == 1);
  CHECK(HeroStatsCatalogue_Value(read, 7, HERO_STATS_WHOLE_RUN, HeroNumLucky) == 1);
  CHECK(HeroStatsCatalogue_Value(read, 9, 2, HeroNumTooWeak) == 1);
  // 64ths rounded to whole hit points: 488 is 7.6, 444 is 6.9, 44 is 0.7
  CHECK(HeroStatsCatalogue_Value(read, 7, 1, HeroNumReduced) == 8);
  CHECK(HeroStatsCatalogue_Value(read, 7, 1, HeroNumReducedProtection) == 8);
  CHECK(HeroStatsCatalogue_Value(read, 7, 1, HeroNumReducedArmour) == 7);
  CHECK(HeroStatsCatalogue_Value(read, 7, 1, HeroNumReducedVitality) == 1);
  CHECK(HeroStatsCatalogue_Value(read, 7, 2, HeroNumReducedResistance) == 20);
  CHECK(HeroStatsCatalogue_Value(read, 7, 2, HeroNumReducedAsleep) == -1);

  // By source on the Survival page: the missile's first
  std::vector<HeroStatsColumn> columns;
  HeroStatsNames names = {};
  HeroStatsHeroPage page = HeroStatsPages_Hero(read, columns, 7, HERO_STATS_WHOLE_RUN, names);
  CHECK(page.reduced.size() == 2);
  if (page.reduced.size() == 2)
    CHECK(page.reduced[0].text == "Monster missiles" && page.reduced[0].amount == 29 && page.reduced[1].text == "Monster attacks");

  // A layout before defences writes and reads none of them
  std::vector<uint8_t> older;
  stats.Serialize(&older, HeroStats::FIRST_LAYOUT_WITH_DEFENCES - 1);
  HeroStats readOlder;
  CHECK(readOlder.Deserialize(older.data(), older.size(), HeroStats::FIRST_LAYOUT_WITH_DEFENCES - 1));
  CHECK(readOlder.MonsterAttacks(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 0);
  CHECK(!readOlder.Deserialize(bytes.data(), bytes.size(), HeroStats::FIRST_LAYOUT_WITH_DEFENCES - 1));
}

void TestHeroStatsDefence(void)
{
  ProtectionIsMeasuredFromNoDefence();
  ResistanceAndShieldsCountInTheSameMeasure();
  MagicAndPsychicAttacks();
  DrawbacksComeOutBelowZero();
  TheCapAndSeveralParts();
  StoreCountsAttacksAndDefences();
}
