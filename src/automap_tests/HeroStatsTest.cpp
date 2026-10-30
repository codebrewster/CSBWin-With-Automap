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

static HeroStatsBlow MonsterBlow(int type)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByMonster);
  blow.monsterType = type;
  return blow;
}

static void FriendlyFireIsHarmToTheVictimAndCausedByTheCauser(void)
{
  HeroStats stats;
  stats.CreditHarm(9, 3, HeroHarmFriendlyFire, ChampionBlow(7, AXE, 2), 30);
  CHECK(stats.Harm(9, 3) == 30);
  CHECK(stats.Harm(7, 3) == 0);
  CHECK(stats.FriendlyFireCaused(7, 3) == 30);
  CHECK(stats.FriendlyFireCaused(9, 3) == 0);
  CHECK(stats.FriendlyFireCaused(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 30);
  std::vector<HeroStatsHarmRow> rows = stats.HarmRows(9, 3);
  CHECK(rows.size() == 1);
  if (rows.size() == 1)
  {
    CHECK(rows[0].source == HeroHarmFriendlyFire && rows[0].causer == 7 && rows[0].amount == 30);
  }

  stats.CreditHarm(7, 3, HeroHarmFriendlyFire, ChampionBlow(7, AXE, 2), 5);
  CHECK(stats.Harm(7, 3) == 5);
  CHECK(stats.FriendlyFireCaused(7, 3) == 35);
}

static void MonsterHarmNamesTheMonsterType(void)
{
  HeroStats stats;
  stats.CreditHarm(9, 3, HeroHarmMonster, MonsterBlow(10), 12);
  stats.CreditHarm(9, 3, HeroHarmMonster, MonsterBlow(10), 8);
  stats.CreditHarm(9, 3, HeroHarmPit, HeroStatsBlow_Nobody(HeroBlowByPit), 50);
  stats.CreditHarm(9, 3, HeroHarmWall, HeroStatsBlow_Nobody(HeroBlowByOther), 0);
  std::vector<HeroStatsHarmRow> rows = stats.HarmRows(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(rows.size() == 2);
  if (rows.size() == 2)
  {
    CHECK(rows[0].source == HeroHarmPit && rows[0].amount == 50);
    CHECK(rows[1].source == HeroHarmMonster && rows[1].causer == HERO_STATS_NOBODY);
    CHECK(rows[1].monsterType == 10 && rows[1].amount == 20);
  }
  CHECK(stats.Harm(HERO_STATS_PARTY, 4) == 0);
}

static void HealingRemembersTheBrewer(void)
{
  HeroStats stats;
  stats.CreditHealing(9, 3, HeroHealPotion, 7, 25);
  stats.CreditHealing(9, 3, HeroHealRegeneration, HERO_STATS_NOBODY, 4);
  CHECK(stats.Healed(9, 3) == 29);
  CHECK(stats.HealingGiven(7, 3) == 25);
  CHECK(stats.HealingGiven(9, 3) == 0);
  CHECK(stats.HealingGiven(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 25);
  std::vector<HeroStatsHealRow> rows = stats.HealRows(9, 3);
  CHECK(rows.size() == 2);
  if (rows.size() == 2)
  {
    CHECK(rows[0].kind == HeroHealPotion && rows[0].brewer == 7 && rows[0].amount == 25);
    CHECK(rows[1].kind == HeroHealRegeneration && rows[1].brewer == HERO_STATS_NOBODY);
  }
}

static void ManaIsKeptPerSpell(void)
{
  HeroStats stats;
  const uint32_t K = 0x00060a03;
  stats.CountCast(7, 3, K, 13);
  stats.CountCast(7, 3, K, 47);
  stats.CountFailedCast(7, 3, K, 6);
  stats.CountFailedCast(7, 3, K | HERO_SPELL_NO_SUCH_SPELL, 8);
  stats.CreditManaSpent(7, 3, HERO_SPELL_RUNES_CLEARED, 4);
  stats.CreditManaGained(7, 3, HeroManaPotion, 20);
  stats.CreditManaGained(7, 3, HeroManaRegeneration, 3);
  CHECK(stats.ManaGained(7, 3) == 23);
  CHECK(stats.ManaSpent(7, 3) == 78);
  CHECK(stats.ManaWasted(7, 3) == 18);
  CHECK(stats.ManaSpent(9, 3) == 0);
  std::vector<HeroStatsSpellRow> rows = stats.Spells(7, 3);
  CHECK(rows.size() == 3);
  if (rows.size() == 3)
  {
    CHECK(rows[0].spell == K);
    CHECK(rows[0].casts == 2 && rows[0].mana == 60 && rows[0].failed == 1 && rows[0].wasted == 6);
  }
}

static void SpellBlowsCreditTheSpellRow(void)
{
  HeroStats stats;
  const uint32_t K = 0x00060a03;
  HeroStatsBlow blow = ChampionBlow(7, HERO_STATS_NO_WEAPON, HERO_STATS_NO_OPTION);
  blow.kind = HeroDamageMagic;
  blow.spell = K;
  stats.CountAttackTry(3, blow);
  stats.CreditMonsterDamage(3, blow, 100);
  stats.CountMonsterKill(3, 10, 1, blow);
  CHECK(stats.DamageDealt(7, 3, HeroDamageMagic) == 100);
  std::vector<HeroStatsSpellRow> rows = stats.Spells(7, 3);
  CHECK(rows.size() == 1);
  if (rows.size() == 1)
  {
    CHECK(rows[0].spell == K && rows[0].damage == 100 && rows[0].kills == 1);
  }
}

void TestHeroStats(void)
{
  ChampionsKeepOrderAndRename();
  BlowsAreCountedPerChampionAndLevel();
  BlowsOfMonstersCreditNothing();
  KillsAreCountedPerChampionAndCause();
  VisitedLevelsAreSortedAndUnique();
  ForgetEmptiesEverything();
  FriendlyFireIsHarmToTheVictimAndCausedByTheCauser();
  MonsterHarmNamesTheMonsterType();
  HealingRemembersTheBrewer();
  ManaIsKeptPerSpell();
  SpellBlowsCreditTheSpellRow();
}
