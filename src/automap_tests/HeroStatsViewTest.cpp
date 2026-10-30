#include "TestSupport.h"
#include <string>
#include "../HeroStatsView.h"

static HeroStatsBlow ChampionBlow(HeroStatsChampion who, int weapon, uint32_t spell)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = who;
  blow.kind = spell != HERO_STATS_NO_SPELL ? HeroDamageMagic : HeroDamageMelee;
  blow.weapon = weapon;
  blow.option = 0;
  blow.spell = spell;
  return blow;
}

static uint32_t Runes(int a, int b, int c)
{
  return (uint32_t)a | (uint32_t)b << 8 | (uint32_t)c << 16;
}
static const int YA = 102, VI = 103, OH = 104, FUL = 105, VEN = 108, IR = 111, KU = 114;

static void NumbersAndPercents(void)
{
  CHECK(HeroStatsView_Number(0) == "0");
  CHECK(HeroStatsView_Number(8931) == "8 931");
  CHECK(HeroStatsView_Number(1234567) == "1 234 567");
  CHECK(HeroStatsView_Number(-1500) == "-1 500");
  CHECK(HeroStatsView_Percent(1, 0) == "-");
  CHECK(HeroStatsView_Percent(407, 636) == "64%");
  CHECK(HeroStatsView_Percent(0, 5) == "0%");
}

static void ColumnsPutThePartyFirst(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.MeetChampion(11, "WUUF");
  std::vector<HeroStatsChampion> party;
  party.push_back(9);
  party.push_back(7);
  std::vector<HeroStatsColumn> columns = HeroStatsView_Columns(stats, party);
  CHECK(columns.size() == 3);
  CHECK(columns[0].id == 9 && columns[0].name == "SYRA" && !columns[0].hasLeft);
  CHECK(columns[1].id == 7 && !columns[1].hasLeft);
  CHECK(columns[2].id == 11 && columns[2].hasLeft);
}

static void PickerShowsButtonsUpToSixteenLevels(void)
{
  HeroStats few;
  for (int i = 0; i < 14; i++) few.MarkLevelVisited(i);
  CHECK(!HeroStatsView_Picker(few, 3).stepper);
  CHECK(HeroStatsView_Picker(few, 3).levels.size() == 14);
  HeroStats sixteen;
  for (int i = 0; i < 16; i++) sixteen.MarkLevelVisited(i);
  CHECK(!HeroStatsView_Picker(sixteen, 0).stepper);

  HeroStats many;
  for (int i = 0; i < 37; i++) many.MarkLevelVisited(i);
  HeroStatsPicker last = HeroStatsView_Picker(many, 36);
  CHECK(last.stepper);
  CHECK(last.next == -2 && last.previous == 35);
  HeroStatsPicker first = HeroStatsView_Picker(many, 0);
  CHECK(first.previous == -2 && first.next == 1);
  HeroStatsPicker whole = HeroStatsView_Picker(many, HERO_STATS_WHOLE_RUN);
  CHECK(whole.previous == 36 && whole.next == 0);
  CHECK(HeroStatsView_Picker(HeroStats(), HERO_STATS_WHOLE_RUN).previous == -2);
}

static void SummariesReadLikeTheMockup(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.MarkLevelVisited(3);
  HeroStatsBlow axe = ChampionBlow(7, 13, HERO_STATS_NO_SPELL);
  for (int i = 0; i < 636; i++) stats.CountAttackTry(3, axe);
  for (int i = 0; i < 407; i++) stats.CountAttackHit(3, axe);
  stats.CreditMonsterDamage(3, axe, 8931);
  for (int i = 0; i < 77; i++) stats.CountMonsterKill(3, 10, 0, axe);
  CHECK(HeroStatsView_Summaries(stats, 7, HERO_STATS_WHOLE_RUN).attacks ==
        "636 tries · 64% hits · 8 931 damage · 77 kills");

  uint32_t fireball = Runes(FUL, IR, 0);
  HeroStatsBlow ball = ChampionBlow(7, HERO_STATS_NO_WEAPON, fireball);
  for (int i = 0; i < 150; i++) stats.CountCast(7, 3, fireball, 27);
  stats.CountFailedCast(7, 3, fireball, 292);
  stats.CreditMonsterDamage(3, ball, 3130);
  for (int i = 0; i < 29; i++) stats.CountMonsterKill(3, 10, 0, ball);
  std::string spells = HeroStatsView_Summaries(stats, 7, HERO_STATS_WHOLE_RUN).spells;
  CHECK(spells.find("150 casts · ") == 0);
  CHECK(spells.find(" mana · 3 130 damage · 29 kills · 292 wasted") != std::string::npos);

  stats.CreditHarm(7, 3, HeroHarmMonster, HeroStatsBlow_Nobody(HeroBlowByMonster), 5136);
  stats.CreditHarm(7, 3, HeroHarmFriendlyFire, ChampionBlow(9, HERO_STATS_NO_WEAPON, fireball), 204);
  CHECK(HeroStatsView_Summaries(stats, 7, 3).harm == "5 340");  // 204 caused by 9, not by 7
  CHECK(HeroStatsView_Summaries(stats, 9, 3).harm == "0 · friendly fire 204");
  CHECK(HeroStatsView_Summaries(stats, 9, 3).healing == "0 healed");
  stats.CreditHealing(7, 3, HeroHealPotion, 9, 2945);
  CHECK(HeroStatsView_Summaries(stats, 7, 3).healing == "2 945 healed");
  CHECK(HeroStatsView_Summaries(stats, 9, 3).healing == "0 healed · 2 945 given");
  stats.CreditHealing(7, 3, HeroHealPotion, 7, 980);
  CHECK(HeroStatsView_Summaries(stats, 7, 3).healing == "3 925 healed · 980 given");
}

static void HardshipCountsAndEmptyStore(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  HeroStatsSummaries empty = HeroStatsView_Summaries(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(empty.attacks == "0 tries · - hits · 0 damage · 0 kills");
  CHECK(empty.spells == "none");
  CHECK(empty.hardship == "never died");
  HeroStatsDeath death = {7, 4, HeroHarmMonster, HeroStatsBlow_Nobody(HeroBlowByMonster)};
  stats.CountDeath(death);
  CHECK(HeroStatsView_Summaries(stats, 7, HERO_STATS_WHOLE_RUN).hardship == "1 death");
  // With a level picked: that level's deaths only, and no wipes or reloads
  CHECK(HeroStatsView_Summaries(stats, 7, 4).hardship == "1 death");
  CHECK(HeroStatsView_Summaries(stats, 7, 3).hardship == "never died");
  for (int i = 0; i < 5; i++) stats.CountDeath(death);
  stats.CountPartyWipe();
  for (int i = 0; i < 9; i++) stats.CountReload();
  CHECK(HeroStatsView_Summaries(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).hardship ==
        "6 deaths · 1 wipe · 9 reloads");
  stats.CountPartyWipe();
  CHECK(HeroStatsView_Summaries(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).hardship ==
        "6 deaths · 2 wipes · 9 reloads");
  CHECK(HeroStatsView_Summaries(stats, HERO_STATS_PARTY, 4).hardship == "6 deaths");
  CHECK(HeroStatsView_Summaries(stats, HERO_STATS_PARTY, 3).hardship == "never died");
}

static void SpellNames(void)
{
  CHECK(HeroStatsView_SpellName(Runes(FUL, IR, 0)) == "Ful Ir Fireball");
  CHECK(HeroStatsView_SpellName(Runes(VI, 0, 0)) == "Vi Health");
  CHECK(HeroStatsView_SpellName(Runes(OH, VEN, 0)) == "Oh Ven Poison cloud");
  CHECK(HeroStatsView_SpellName(Runes(FUL, YA, KU)) == "Ful Ya Ku");
  CHECK(HeroStatsView_SpellName(Runes(FUL, YA, KU) | HERO_SPELL_NO_SUCH_SPELL) == "Ful Ya Ku (no spell)");
  CHECK(HeroStatsView_SpellName(HERO_SPELL_RUNES_CLEARED) == "Runes cleared");
  CHECK(HeroStatsView_SpellName(HERO_SPELL_SHIELD_ACTION) == "Shield action");
  CHECK(HeroStatsView_SpellName(HERO_SPELL_HEAL_ACTION) == "Heal action");
  CHECK(HeroStatsView_SpellName(HERO_SPELL_ITEM_SPELLS) == "Spells from items");
  CHECK(HeroStatsView_SpellName(0) == "(no runes)");
}

static void DeathNames(void)
{
  HeroStatsBlow none = HeroStatsBlow_Nobody(HeroBlowByOther);
  HeroStatsDeath mummy = {7, 4, HeroHarmMonster, HeroStatsBlow_Nobody(HeroBlowByMonster)};
  CHECK(HeroStatsView_DeathName(mummy, "Mummy") == "Mummy · Lv 4");
  HeroStatsBlow own = ChampionBlow(7, HERO_STATS_NO_WEAPON, Runes(FUL, IR, 0));
  HeroStatsDeath self = {7, 9, HeroHarmFriendlyFire, own};
  CHECK(HeroStatsView_DeathName(self, "HALK") == "own Fireball · Lv 9");
  HeroStatsBlow theirs = ChampionBlow(3, HERO_STATS_NO_WEAPON, Runes(FUL, IR, 0));
  HeroStatsDeath shot = {9, 5, HeroHarmFriendlyFire, theirs};
  CHECK(HeroStatsView_DeathName(shot, "Tiggy") == "Fireball from Tiggy · Lv 5");
  HeroStatsDeath axed = {9, 5, HeroHarmFriendlyFire, ChampionBlow(3, 13, HERO_STATS_NO_SPELL)};
  CHECK(HeroStatsView_DeathName(axed, "Tiggy") == "blow from Tiggy · Lv 5");
  HeroStatsDeath pit = {7, 7, HeroHarmPit, none};
  CHECK(HeroStatsView_DeathName(pit, "") == "Fell into a pit · Lv 7");
  HeroStatsDeath door = {7, 6, HeroHarmDoor, none};
  CHECK(HeroStatsView_DeathName(door, "") == "Closing door · Lv 6");
  HeroStatsDeath poison = {7, 11, HeroHarmPoison, none};
  CHECK(HeroStatsView_DeathName(poison, "") == "Poison · Lv 11");
  HeroStatsDeath cloud = {9, 2, HeroHarmPoisonCloud, ChampionBlow(3, HERO_STATS_NO_WEAPON, Runes(OH, VEN, 0))};
  CHECK(HeroStatsView_DeathName(cloud, "Tiggy") == "Poison cloud from Tiggy · Lv 2");
  HeroStatsDeath wall = {7, 1, HeroHarmWall, none};
  CHECK(HeroStatsView_DeathName(wall, "") == "Ran into a wall · Lv 1");
  HeroStatsDeath other = {7, 1, HeroHarmOther, none};
  CHECK(HeroStatsView_DeathName(other, "") == "Unknown · Lv 1");
}

static void HarmNames(void)
{
  HeroStatsColumn tiggy = {3, "Tiggy", false};
  HeroStatsHarmRow ff = {HeroHarmFriendlyFire, 3, -1, 88};
  CHECK(HeroStatsView_HarmName(ff, &tiggy, "") == "Friendly fire (from Tiggy)");
  HeroStatsHarmRow mon = {HeroHarmMonster, HERO_STATS_NOBODY, 10, 5};
  CHECK(HeroStatsView_HarmName(mon, 0, "") == "Monster attacks");
  HeroStatsHarmRow pit = {HeroHarmPit, HERO_STATS_NOBODY, -1, 5};
  CHECK(HeroStatsView_HarmName(pit, 0, "") == "Pits");
}

void TestHeroStatsView(void)
{
  NumbersAndPercents();
  ColumnsPutThePartyFirst();
  PickerShowsButtonsUpToSixteenLevels();
  SummariesReadLikeTheMockup();
  HardshipCountsAndEmptyStore();
  SpellNames();
  DeathNames();
  HarmNames();
}
