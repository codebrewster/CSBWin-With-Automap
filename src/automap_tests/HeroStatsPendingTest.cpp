#include "TestSupport.h"
#include "../HeroStatsPending.h"
#include "../HeroStats.h"

static HeroStatsBlow MonsterBlow(int type)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByMonster);
  blow.monsterType = type;
  return blow;
}

static HeroStatsBlow FireballBlow(HeroStatsChampion who)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = who;
  blow.kind = HeroDamageMagic;
  blow.spell = 0x010203;
  return blow;
}

static void TieGoesToTheLater(void)
{
  HeroStatsPending pending;
  HeroHarmSource source;
  HeroStatsBlow blow;
  CHECK(!pending.Cause(1, &source, &blow));
  pending.Add(1, HeroHarmMonster, MonsterBlow(5), 30);
  pending.Add(1, HeroHarmFriendlyFire, FireballBlow(2), 30);
  CHECK(pending.Cause(1, &source, &blow));
  CHECK(source == HeroHarmFriendlyFire && blow.champion == 2);
}

static void LargestWins(void)
{
  HeroStatsPending pending;
  HeroHarmSource source;
  HeroStatsBlow blow;
  pending.Add(0, HeroHarmMonster, MonsterBlow(5), 50);
  pending.Add(0, HeroHarmFriendlyFire, FireballBlow(2), 20);
  CHECK(pending.Cause(0, &source, &blow));
  CHECK(source == HeroHarmMonster && blow.monsterType == 5);
}

static void SameCauserAddsUp(void)
{
  HeroStatsPending pending;
  HeroHarmSource source;
  HeroStatsBlow blow;
  pending.Add(3, HeroHarmMonster, MonsterBlow(5), 20);
  pending.Add(3, HeroHarmFriendlyFire, FireballBlow(2), 30);
  pending.Add(3, HeroHarmMonster, MonsterBlow(5), 20);
  CHECK(pending.Cause(3, &source, &blow));
  CHECK(source == HeroHarmMonster && blow.monsterType == 5);
}

static void ClearAndBadSlots(void)
{
  HeroStatsPending pending;
  HeroHarmSource source;
  HeroStatsBlow blow;
  pending.Add(0, HeroHarmMonster, MonsterBlow(5), 10);
  pending.Add(1, HeroHarmMonster, MonsterBlow(5), 10);
  pending.Add(4, HeroHarmMonster, MonsterBlow(5), 10);
  pending.Add(-1, HeroHarmMonster, MonsterBlow(5), 10);
  CHECK(!pending.Cause(4, &source, &blow) && !pending.Cause(-1, &source, &blow));
  pending.Clear(0);
  CHECK(!pending.Cause(0, &source, &blow) && pending.Cause(1, &source, &blow));
  pending.ClearAll();
  CHECK(!pending.Cause(1, &source, &blow));
}

// The runes are kept per champion: a champion who moves to another slot keeps them
static void RunesKeepTheirMana(void)
{
  HeroStats stats;
  stats.EnterRune(7, 1, 4);
  stats.EnterRune(7, 1, 6);
  stats.EnterRune(7, 1, 9);
  stats.TakeBackRune(7);
  CHECK(stats.RunesCleared(7, 1) == 9);
  CHECK(stats.ResolveRunes(7, HeroRuneCast) == 10);
  CHECK(stats.ResolveRunes(7, HeroRuneCast) == 0);
  stats.TakeBackRune(7);
  CHECK(stats.RunesCleared(7, 1) == 9);
  stats.EnterRune(9, 1, 3);
  CHECK(stats.ResolveRunes(7, HeroRuneCast) == 0 && stats.RunesNotCastYet(9, 1) == 3);
  stats.Forget();
  CHECK(stats.PendingRunes(9).empty());
}

void TestHeroStatsPending(void)
{
  TieGoesToTheLater();
  LargestWins();
  SameCauserAddsUp();
  ClearAndBadSlots();
  RunesKeepTheirMana();
}
