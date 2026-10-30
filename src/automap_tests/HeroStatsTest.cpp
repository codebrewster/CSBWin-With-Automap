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

static HeroStatsDeath DeathOf(HeroStatsChampion who, int level, HeroHarmSource cause)
{
  HeroStatsDeath death;
  death.who = who;
  death.level = level;
  death.cause = cause;
  death.by = HeroStatsBlow_Nobody(HeroBlowByMonster);
  death.by.monsterType = 4;
  return death;
}

static void DeathsKeepOrderCauseAndLevel(void)
{
  HeroStats stats;
  stats.CountDeath(DeathOf(7, 3, HeroHarmMonster));
  stats.CountDeath(DeathOf(9, 4, HeroHarmPit));
  stats.CountDeath(DeathOf(7, 5, HeroHarmPoison));
  std::vector<HeroStatsDeath> deaths = stats.Deaths(7);
  CHECK(deaths.size() == 2);
  if (deaths.size() == 2)
  {
    CHECK(deaths[0].level == 3 && deaths[0].cause == HeroHarmMonster && deaths[0].by.monsterType == 4);
    CHECK(deaths[1].level == 5 && deaths[1].cause == HeroHarmPoison);
  }
  CHECK(stats.Deaths(HERO_STATS_PARTY).size() == 3);
  CHECK(stats.Deaths(1).empty());
  stats.CountResurrection(7, 3);
  stats.CountResurrection(7, 3);
  stats.CountResurrection(7, 4);
  CHECK(stats.Resurrections(7, 3) == 2);
  CHECK(stats.Resurrections(7, HERO_STATS_WHOLE_RUN) == 3);
  CHECK(stats.Resurrections(HERO_STATS_PARTY, 4) == 1);
  CHECK(stats.Resurrections(9, 3) == 0);
}

static void MergeHardshipTakesTheLargerOfEach(void)
{
  HeroStats memory, file;
  memory.CountDeath(DeathOf(7, 3, HeroHarmMonster));
  memory.CountDeath(DeathOf(9, 4, HeroHarmPit));
  file.CountDeath(DeathOf(7, 3, HeroHarmMonster));
  memory.CountPartyWipe();
  memory.CountReload();
  memory.CountReload();
  memory.CountReload();
  for (int i = 0; i < 5; i++) file.CountReload();
  memory.MergeHardship(file);
  CHECK(memory.Deaths(HERO_STATS_PARTY).size() == 2);
  CHECK(memory.PartyWipes() == 1);
  CHECK(memory.Reloads() == 5);
  // The other way round the file's longer list wins
  file.CountDeath(DeathOf(1, 1, HeroHarmOther));
  file.CountDeath(DeathOf(2, 1, HeroHarmOther));
  memory.MergeHardship(file);
  CHECK(memory.Deaths(HERO_STATS_PARTY).size() == 3);
}

static void TakeFromFileReplacesTheRestAndKeepsHardship(void)
{
  HeroStats memory, file;
  HeroStatsBlow blow = ChampionBlow(7, 1, 2);
  memory.MeetChampion(7, "Memory");
  memory.MarkLevelVisited(9);
  memory.CreditMonsterDamage(3, blow, 100);
  memory.CountResurrection(7, 3);
  memory.CountDeath(DeathOf(7, 3, HeroHarmMonster));
  memory.CountDeath(DeathOf(9, 4, HeroHarmPit));
  file.MeetChampion(8, "File");
  file.MarkLevelVisited(2);
  file.CreditMonsterDamage(3, blow, 40);
  file.CountDeath(DeathOf(7, 3, HeroHarmMonster));
  memory.TakeFromFile(file);
  CHECK(memory.DamageDealt(7, 3, HeroDamageMelee) == 40);
  CHECK(memory.Deaths(HERO_STATS_PARTY).size() == 2);
  CHECK(memory.Resurrections(7, 3) == 0);
  CHECK(memory.Champions().size() == 1 && memory.Champions()[0].name == "File");
  CHECK(memory.VisitedLevels().size() == 1 && memory.VisitedLevels()[0] == 2);

  // A different game: Forget first, then only the file's hardship is left
  memory.Forget();
  CHECK(memory.Deaths(HERO_STATS_PARTY).empty() && memory.PartyWipes() == 0 && memory.Reloads() == 0);
  file.CountPartyWipe();
  memory.TakeFromFile(file);
  CHECK(memory.Deaths(HERO_STATS_PARTY).size() == 1);
  CHECK(memory.PartyWipes() == 1);
}

static void RuneSpellKeyLeavesOutThePowerRune(void)
{
  // FUL IR at power LO and at power MON: one row
  const signed char lo[4] = {0x60, 0x66, 0x6c + 1, 0};
  const signed char mon[4] = {0x60 + 5, 0x66, 0x6c + 1, 0};
  CHECK(HeroStats_RuneSpellKey(lo) == (0x66u | 0x6du << 8));
  CHECK(HeroStats_RuneSpellKey(mon) == HeroStats_RuneSpellKey(lo));
  // All four runes fill the array; there is no terminator to read past
  const signed char four[4] = {0x60, 0x67, 0x6d, 0x73};
  CHECK(HeroStats_RuneSpellKey(four) == (0x67u | 0x6du << 8 | 0x73u << 16));
  // A short incantation: whatever follows the first 0 is not a rune
  const signed char shortOne[4] = {0x61, 0x68, 0, 0x75};
  CHECK(HeroStats_RuneSpellKey(shortOne) == 0x68u);
}

static void WeaponNamesComeFromTheItemNames(void)
{
  const char *names[3] = {"DAGGER", NULL, "TORCH"};
  CHECK(HeroStats_WeaponName(0, names, 3) == "DAGGER");
  CHECK(HeroStats_WeaponName(2, names, 3) == "TORCH");
  CHECK(HeroStats_WeaponName(HERO_STATS_FISTS, names, 3) == "FISTS");
  CHECK(HeroStats_WeaponName(HERO_STATS_NO_WEAPON, names, 3) == "");
  // A type with no name, or none the table has: its number
  CHECK(HeroStats_WeaponName(1, names, 3) == "#1");
  CHECK(HeroStats_WeaponName(3, names, 3) == "#3");
  CHECK(HeroStats_WeaponName(-7, names, 3) == "#-7");
}

void TestHeroStats(void)
{
  RuneSpellKeyLeavesOutThePowerRune();
  WeaponNamesComeFromTheItemNames();
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
  DeathsKeepOrderCauseAndLevel();
  MergeHardshipTakesTheLargerOfEach();
  TakeFromFileReplacesTheRestAndKeepsHardship();
}
