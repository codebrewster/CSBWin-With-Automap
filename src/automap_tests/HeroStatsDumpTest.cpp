#include "TestSupport.h"
#include <string>
#include "../HeroStatsDump.h"

static HeroStatsBlow ChampionBlow(HeroStatsChampion who, int weapon, int option)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = who;
  blow.kind = HeroDamageMelee;
  blow.weapon = weapon;
  blow.option = option;
  return blow;
}

static void NothingDumpsAsNames(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  CHECK(HeroStatsDump_Text(stats) == "HALK\nSYRA\nPARTY\n");
}

static void EveryNumberGetsALine(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  HeroStatsBlow axe = ChampionBlow(7, 13, 2);
  stats.CountAttackTry(3, axe);
  stats.CountAttackTry(3, axe);
  stats.CountAttackHit(3, axe);
  stats.CreditMonsterDamage(3, axe, 40);
  HeroStatsBlow fists = ChampionBlow(9, HERO_STATS_UNARMED, 0);
  stats.CountAttackTry(3, fists);
  stats.CountAttackHit(3, fists);
  stats.CreditMonsterDamage(3, fists, 5);
  stats.CountMonsterKill(3, 10, 1, fists);
  HeroStatsBlow monster = HeroStatsBlow_Nobody(HeroBlowByMonster);
  monster.monsterType = 10;
  stats.CreditHarm(7, 4, HeroHarmMonster, monster, 12);
  HeroStatsDeath death = { 7, 4, HeroHarmMonster, monster };
  stats.CountDeath(death);
  stats.CountResurrection(9, 4);
  stats.CreditHealing(9, 2, HeroHealPotion, 7, 20);
  stats.CreditManaGained(9, 2, HeroManaPotion, 30);
  stats.CountPartyWipe();
  stats.CountReload();
  stats.MarkLevelVisited(4);
  stats.MarkLevelVisited(0);

  std::string expected =
    "HALK\n"
    "HALK melee 40\n"
    "HALK attack 13/2 tries 2 hits 1 damage 40 kills 0\n"
    "HALK harm 12\n"
    "HALK harm source 0 by nobody monster 10 amount 12\n"
    "HALK healing given 20\n"
    "HALK death level 4 cause 0 monster 10\n"
    "SYRA\n"
    "SYRA melee 5\n"
    "SYRA kills 1\n"
    "SYRA attack -1/0 tries 1 hits 1 damage 5 kills 1\n"
    "SYRA healed 20\n"
    "SYRA heal kind 1 brewer HALK amount 20\n"
    "SYRA mana gained 30\n"
    "SYRA resurrections 1\n"
    "PARTY\n"
    "PARTY melee 45\n"
    "PARTY kills 1\n"
    "PARTY attack -1/0 tries 1 hits 1 damage 5 kills 1\n"
    "PARTY attack 13/2 tries 2 hits 1 damage 40 kills 0\n"
    "PARTY harm 12\n"
    "PARTY harm source 0 by nobody monster 10 amount 12\n"
    "PARTY healed 20\n"
    "PARTY heal kind 1 brewer HALK amount 20\n"
    "PARTY healing given 20\n"
    "PARTY mana gained 30\n"
    "PARTY resurrections 1\n"
    "PARTY death level 4 cause 0 monster 10\n"
    "PARTY monster type 10 variant 1 by 0 champion SYRA count 1\n"
    "PARTY visited levels 0 4\n"
    "PARTY wipes 1\n"
    "PARTY reloads 1\n";
  CHECK(HeroStatsDump_Text(stats) == expected);
}

static void SpellsFriendlyFireAndManaGetLines(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.CountCast(9, 1, 0x00060102, 10);
  stats.CountFailedCast(9, 1, 0x00060103, 4);
  HeroStatsBlow friendly = ChampionBlow(7, 13, 2);
  stats.CreditHarm(9, 1, HeroHarmFriendlyFire, friendly, 6);

  std::string expected =
    "HALK\n"
    "HALK friendly fire caused 6\n"
    "SYRA\n"
    "SYRA spell 0x00060102 casts 1 mana 10 failed 0 wasted 0 damage 0 kills 0\n"
    "SYRA spell 0x00060103 casts 0 mana 0 failed 1 wasted 4 damage 0 kills 0\n"
    "SYRA harm 6\n"
    "SYRA harm source 3 by HALK monster -1 amount 6\n"
    "SYRA mana spent 14\n"
    "SYRA mana wasted 4\n"
    "PARTY\n"
    "PARTY spell 0x00060102 casts 1 mana 10 failed 0 wasted 0 damage 0 kills 0\n"
    "PARTY spell 0x00060103 casts 0 mana 0 failed 1 wasted 4 damage 0 kills 0\n"
    "PARTY harm 6\n"
    "PARTY harm source 3 by HALK monster -1 amount 6\n"
    "PARTY friendly fire caused 6\n"
    "PARTY mana spent 14\n"
    "PARTY mana wasted 4\n";
  CHECK(HeroStatsDump_Text(stats) == expected);
}

static void ThrownRowsAndPercentNamesAreSafe(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "A%s%d");
  HeroStatsBlow dagger = ChampionBlow(7, 20, HERO_STATS_THROW_OPTION);
  dagger.thrown = true;
  stats.CountAttackTry(1, dagger);
  stats.CountAttackTry(2, dagger);
  stats.CountAttackHit(2, dagger);
  stats.CreditMonsterDamage(2, dagger, 9);
  HeroStatsBlow friendly = ChampionBlow(7, 20, 0);
  stats.CreditHarm(7, 2, HeroHarmFriendlyFire, friendly, 3);
  std::string expected =
    "A%s%d\n"
    "A%s%d melee 9\n"
    "A%s%d attack 20/-1 thrown tries 2 hits 1 damage 9 kills 0\n"
    "A%s%d harm 3\n"
    "A%s%d harm source 3 by A%s%d monster -1 amount 3\n"
    "A%s%d friendly fire caused 3\n"
    "PARTY\n"
    "PARTY melee 9\n"
    "PARTY attack 20/-1 thrown tries 2 hits 1 damage 9 kills 0\n"
    "PARTY harm 3\n"
    "PARTY harm source 3 by A%s%d monster -1 amount 3\n"
    "PARTY friendly fire caused 3\n";
  CHECK(HeroStatsDump_Text(stats) == expected);
}

static void FriendlyFireDeathNamesItsKiller(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  HeroStatsBlow friendly = ChampionBlow(7, 13, 2);
  stats.CreditHarm(9, 1, HeroHarmFriendlyFire, friendly, 6);
  HeroStatsDeath death = { 9, 1, HeroHarmFriendlyFire, friendly };
  stats.CountDeath(death);
  std::string expected =
    "HALK\n"
    "HALK friendly fire caused 6\n"
    "SYRA\n"
    "SYRA harm 6\n"
    "SYRA harm source 3 by HALK monster -1 amount 6\n"
    "SYRA death level 1 cause 3 monster -1 by HALK\n"
    "PARTY\n"
    "PARTY harm 6\n"
    "PARTY harm source 3 by HALK monster -1 amount 6\n"
    "PARTY friendly fire caused 6\n"
    "PARTY death level 1 cause 3 monster -1 by HALK\n";
  CHECK(HeroStatsDump_Text(stats) == expected);
}

void TestHeroStatsDump(void)
{
  NothingDumpsAsNames();
  EveryNumberGetsALine();
  SpellsFriendlyFireAndManaGetLines();
  ThrownRowsAndPercentNamesAreSafe();
  FriendlyFireDeathNamesItsKiller();
}
