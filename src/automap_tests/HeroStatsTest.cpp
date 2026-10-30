#include "TestSupport.h"
#include "../HeroStats.h"

static const int AXE = 13;

static HeroStatsBlow ChampionBlow(HeroStatsChampion who, int weapon, int option)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = who;
  blow.kind = HeroDamageMelee;
  blow.weapon = weapon;
  blow.option = option;
  return blow;
}

static void ChampionsKeepOrderAndRename(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.MeetChampion(7, "HALK2");
  CHECK(stats.Champions().size() == 2);
  CHECK(stats.Champions()[0].id == 7 && stats.Champions()[0].name == "HALK2");
  CHECK(stats.Champions()[1].id == 9 && stats.Champions()[1].name == "SYRA");
}

static void BlowsAreCountedPerChampionAndLevel(void)
{
  HeroStats stats;
  HeroStatsBlow blow = ChampionBlow(7, AXE, 2);
  stats.CountAttackTry(3, blow);
  stats.CountAttackTry(3, blow);
  stats.CountAttackHit(3, blow);
  stats.CreditMonsterDamage(3, blow, 40);
  CHECK(stats.DamageDealt(7, 3, HeroDamageMelee) == 40);
  CHECK(stats.DamageDealt(7, HERO_STATS_WHOLE_RUN, HeroDamageMelee) == 40);
  CHECK(stats.DamageDealt(HERO_STATS_PARTY, 3, HeroDamageMelee) == 40);
  CHECK(stats.DamageDealt(7, 3, HeroDamageMagic) == 0);
  CHECK(stats.DamageDealt(7, 4, HeroDamageMelee) == 0);
  std::vector<HeroStatsAttackRow> rows = stats.Attacks(7, 3);
  CHECK(rows.size() == 1);
  if (rows.size() == 1)
  {
    CHECK(rows[0].weapon == AXE && !rows[0].thrown && rows[0].option == 2);
    CHECK(rows[0].tries == 2 && rows[0].hits == 1 && rows[0].damage == 40 && rows[0].kills == 0);
  }

  HeroStatsBlow other = ChampionBlow(9, AXE, 2);
  stats.CountAttackTry(4, other);
  stats.CreditMonsterDamage(4, other, 10);
  rows = stats.Attacks(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(rows.size() == 1);
  if (rows.size() == 1)
  {
    CHECK(rows[0].tries == 3);
    CHECK(rows[0].damage == 50);
  }
}

static void BlowsOfMonstersCreditNothing(void)
{
  HeroStats stats;
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByMonster);
  blow.monsterType = 10;
  stats.CountAttackTry(3, blow);
  stats.CountAttackHit(3, blow);
  stats.CreditMonsterDamage(3, blow, 25);
  CHECK(stats.DamageDealt(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroDamageMelee) == 0);
  CHECK(stats.Attacks(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).empty());
}

static void KillsAreCountedPerChampionAndCause(void)
{
  HeroStats stats;
  HeroStatsBlow blow = ChampionBlow(7, AXE, 2);
  stats.CountAttackTry(3, blow);
  stats.CountMonsterKill(3, 10, 1, blow);
  CHECK(stats.Kills(7, 3) == 1);
  CHECK(stats.Attacks(7, 3)[0].kills == 1);
  std::vector<HeroStatsMonsterRow> kills = stats.MonsterKills(3);
  CHECK(kills.size() == 1);
  if (kills.size() == 1)
  {
    CHECK(kills[0].monsterType == 10 && kills[0].variant == 1);
    CHECK(kills[0].by == HeroBlowByChampion && kills[0].champion == 7 && kills[0].count == 1);
  }

  stats.CountMonsterKill(3, 10, 1, HeroStatsBlow_Nobody(HeroBlowByDoor));
  kills = stats.MonsterKills(3);
  CHECK(kills.size() == 2);
  bool found = false;
  for (size_t i = 0; i < kills.size(); i++)
  {
    if (kills[i].by == HeroBlowByDoor)
    {
      found = kills[i].champion == HERO_STATS_NOBODY && kills[i].count == 1;
    }
  }
  CHECK(found);
  CHECK(stats.Kills(7, 3) == 1);
  CHECK(stats.Kills(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 1);
}

static void VisitedLevelsAreSortedAndUnique(void)
{
  HeroStats stats;
  stats.MarkLevelVisited(5);
  stats.MarkLevelVisited(2);
  stats.MarkLevelVisited(5);
  std::vector<int> levels = stats.VisitedLevels();
  CHECK(levels.size() == 2 && levels[0] == 2 && levels[1] == 5);
}

static void ForgetEmptiesEverything(void)
{
  HeroStats stats;
  HeroStatsBlow blow = ChampionBlow(7, AXE, 2);
  stats.MeetChampion(7, "HALK");
  stats.MarkLevelVisited(3);
  stats.CountAttackTry(3, blow);
  stats.CreditMonsterDamage(3, blow, 5);
  stats.CountMonsterKill(3, 10, 1, blow);
  stats.Forget();
  CHECK(stats.Champions().empty());
  CHECK(stats.VisitedLevels().empty());
  CHECK(stats.Attacks(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).empty());
  CHECK(stats.MonsterKills(HERO_STATS_WHOLE_RUN).empty());
  CHECK(stats.Kills(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 0);
  CHECK(stats.DamageDealt(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroDamageMelee) == 0);
}

void TestHeroStats(void)
{
  ChampionsKeepOrderAndRename();
  BlowsAreCountedPerChampionAndLevel();
  BlowsOfMonstersCreditNothing();
  KillsAreCountedPerChampionAndCause();
  VisitedLevelsAreSortedAndUnique();
  ForgetEmptiesEverything();
}
