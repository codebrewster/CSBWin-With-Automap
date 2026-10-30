#include "TestSupport.h"
#include "../HeroStatsPending.h"
#include "../HeroStatsRunes.h"

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

static void RunesKeepTheirMana(void)
{
  HeroStatsRunes runes;
  int mana = -1;
  runes.AddRune(2, 4);
  runes.AddRune(2, 6);
  runes.AddRune(2, 9);
  runes.RemoveLastRune(2, &mana);
  CHECK(mana == 9);
  CHECK(runes.TakeAll(2) == 10);
  CHECK(runes.TakeAll(2) == 0);
  runes.RemoveLastRune(2, &mana);
  CHECK(mana == 0);
  runes.AddRune(7, 3);
  CHECK(runes.TakeAll(7) == 0);
  runes.AddRune(0, 3);
  runes.ClearAll();
  CHECK(runes.TakeAll(0) == 0);
}

// A champion leaves the party: the champions after it move down one slot, and their runes with them
static void RunesMoveWithTheirSlot(void)
{
  HeroStatsRunes runes;
  runes.AddRune(0, 1);
  runes.AddRune(1, 2);
  runes.AddRune(2, 3);
  runes.AddRune(2, 4);
  runes.AddRune(3, 5);
  runes.RemoveSlot(1);
  CHECK(runes.TakeAll(0) == 1);
  int mana = -1;
  runes.RemoveLastRune(1, &mana);
  CHECK(mana == 4);
  CHECK(runes.TakeAll(1) == 3);
  CHECK(runes.TakeAll(2) == 5);
  CHECK(runes.TakeAll(3) == 0);
  runes.AddRune(3, 6);
  runes.RemoveSlot(3);
  CHECK(runes.TakeAll(3) == 0);
  runes.AddRune(0, 7);
  runes.RemoveSlot(4);
  runes.RemoveSlot(-1);
  CHECK(runes.TakeAll(0) == 7);
}

void TestHeroStatsPending(void)
{
  TieGoesToTheLater();
  LargestWins();
  SameCauserAddsUp();
  ClearAndBadSlots();
  RunesKeepTheirMana();
  RunesMoveWithTheirSlot();
}
