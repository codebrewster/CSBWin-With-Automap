#include "TestSupport.h"
#include "../HeroStatsExport.h"
#include "../HeroStatsPages.h"

// Type 6 has two variants the dungeon does not name apart; type 25 is named
static std::string MonsterName(int monsterType, int variant)
{
  (void)variant;
  if (monsterType == 6) return "Screamer";
  if (monsterType == 25) return "Lord Order";
  return "Monster #" + std::to_string(monsterType);
}

static std::string WeaponName(int weapon)
{
  return weapon == HERO_STATS_UNARMED ? "UNARMED" : "SWORD";
}

static std::string OptionName(int option)
{
  return option == 1 ? "PUNCH" : option == 2 ? "KICK" : option == 8 ? "WAR CRY" : "";
}

static const HeroStatsNames NAMES = {MonsterName, WeaponName, OptionName};

static HeroStatsBlow ChampionBlow(HeroStatsChampion who, int weapon, int option)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = who;
  blow.weapon = weapon;
  blow.option = option;
  return blow;
}

static std::vector<HeroStatsColumn> Columns(const HeroStats &stats)
{
  std::vector<HeroStatsChampion> party;
  party.push_back(7);
  party.push_back(9);
  return HeroStatsView_Columns(stats, party);
}

static void VariantsWithoutNamesShareALine(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.CountMonsterKill(2, 6, 0, ChampionBlow(7, HERO_STATS_UNARMED, 1));
  stats.CountMonsterKill(2, 6, 3, ChampionBlow(9, HERO_STATS_UNARMED, 1));
  stats.CountMonsterKill(2, 25, 0, HeroStatsBlow_Nobody(HeroBlowByDoor));
  std::vector<HeroStatsMonsterLine> lines = HeroStatsPages_Monsters(stats, Columns(stats), HERO_STATS_WHOLE_RUN, NAMES);
  CHECK(lines.size() == 2);
  if (lines.size() == 2)
  {
    CHECK(lines[0].name == "Screamer" && lines[0].total == 2);
    CHECK(lines[0].byColumn[0] == 1 && lines[0].byColumn[1] == 1);
    CHECK(lines[1].name == "Lord Order" && lines[1].door == 1);
  }
  HeroStatsMonsterLine all = HeroStatsPages_AllMonsters(lines, 2);
  CHECK(all.total == 3 && all.door == 1 && all.byColumn[0] == 1);
}

static void UnarmedAttacksAreOneWeapon(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.CountAttackTry(1, ChampionBlow(7, HERO_STATS_UNARMED, 1));
  stats.CountAttackTry(1, ChampionBlow(7, HERO_STATS_UNARMED, 2));
  stats.CreditMonsterDamage(1, ChampionBlow(7, HERO_STATS_UNARMED, 2), 9);
  HeroStatsHeroPage page = HeroStatsPages_Hero(stats, Columns(stats), 7, HERO_STATS_WHOLE_RUN, NAMES);
  CHECK(page.weapons.size() == 1);
  if (page.weapons.size() == 1)
  {
    CHECK(page.weapons[0].name == "UNARMED" && page.weapons[0].tries == 2 && page.weapons[0].damage == 9);
    CHECK(page.weapons[0].options.size() == 2 && page.weapons[0].options[0].name == "KICK");
  }
}

static void WarCryIsAnUnarmedAction(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.CountAttackTry(1, ChampionBlow(7, HERO_STATS_UNARMED, 1));
  stats.CountAttackHit(1, ChampionBlow(7, HERO_STATS_UNARMED, 1));
  stats.CreditMonsterDamage(1, ChampionBlow(7, HERO_STATS_UNARMED, 1), 6);
  stats.CountActionUse(1, ChampionBlow(7, HERO_STATS_UNARMED, 8));
  HeroStatsHeroPage page = HeroStatsPages_Hero(stats, Columns(stats), 7, HERO_STATS_WHOLE_RUN, NAMES);
  CHECK(page.weapons.size() == 1);
  if (page.weapons.size() == 1)
  {
    const HeroStatsWeaponLine &unarmed = page.weapons[0];
    CHECK(unarmed.name == "UNARMED" && unarmed.tries == 1 && unarmed.uses == 1 && unarmed.hits == 1);
    CHECK(unarmed.options.size() == 2);
    if (unarmed.options.size() == 2)
      CHECK(unarmed.options[1].name == "WAR CRY" && unarmed.options[1].uses == 1 && unarmed.options[1].tries == 0);
  }
  // Tries in the summary count the War Cry; the hit rate is the punch's
  CHECK(page.summaries.attacks.find("2 tries · 100% hits") == 0);
}

static void SpellLinesCountEveryCast(void)
{
  HeroStats stats;
  stats.MeetChampion(9, "SYRA");
  uint32_t fireball = 0x00006f69;  // Ful Ir
  stats.CountCast(9, 1, fireball, 20);
  stats.CountCast(9, 1, fireball, 20);
  stats.CountFailedCast(9, 1, fireball, 7);
  HeroStatsHeroPage page = HeroStatsPages_Hero(stats, Columns(stats), 9, HERO_STATS_WHOLE_RUN, NAMES);
  CHECK(page.spells.size() == 1);
  if (page.spells.size() == 1)
  {
    CHECK(page.spells[0].casts == 3 && page.spells[0].successful == 2 && page.spells[0].failed == 1);
    CHECK(page.spells[0].mana == 40 && page.spells[0].wasted == 7);
  }
}

static void MonsterSpellsAreNotMonsterAttacks(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  HeroStatsBlow claws = HeroStatsBlow_Nobody(HeroBlowByMonster);
  claws.monsterType = 25;
  HeroStatsBlow fireball = claws;
  fireball.spell = HERO_SPELL_MONSTER_MISSILE | 0x80;
  stats.CreditHarm(7, 3, HeroHarmMonster, claws, 10);
  stats.CreditHarm(7, 3, HeroHarmMonsterMissile, fireball, 30);
  HeroStatsHeroPage page = HeroStatsPages_Hero(stats, Columns(stats), 7, 3, NAMES);
  CHECK(page.taken.size() == 2);
  if (page.taken.size() == 2)
  {
    CHECK(page.taken[0].text == "Fireball" && page.taken[0].amount == 30);
    CHECK(page.taken[1].text == "Monster attacks" && page.taken[1].amount == 10);
  }
}

static void OverviewCarriesTheMovement(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.CountStep(1);
  stats.CountTurn(1);
  stats.CountTick(2, false);
  HeroStatsOverview level1 = HeroStatsPages_Overview(stats, Columns(stats), 1);
  CHECK(level1.movement.steps == 1 && level1.movement.turns == 1 && level1.movement.ticks == 0);
  HeroStatsOverview run = HeroStatsPages_Overview(stats, Columns(stats), HERO_STATS_WHOLE_RUN);
  CHECK(run.movement.ticks == 1 && run.cells.size() == 2);
}

static void JsonStringsAreSafeInAScript(void)
{
  CHECK(HeroStatsExport_JsonString("HALK") == "\"HALK\"");
  CHECK(HeroStatsExport_JsonString("a\"b\\c") == "\"a\\\"b\\\\c\"");
  CHECK(HeroStatsExport_JsonString("</script>") == "\"\\u003c/script\\u003e\"");
  CHECK(HeroStatsExport_JsonString("Lv 3 \xc2\xb7 own") == "\"Lv 3 \xc2\xb7 own\"");  // UTF-8 kept
  CHECK(HeroStatsExport_JsonString("\xe9t\xe9") == "\"\\u00e9t\\u00e9\"");          // Latin-1 taken as such
  CHECK(HeroStatsExport_JsonString("\x01") == "\"\\u0001\"");
}

static void ExportHoldsEveryLevel(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "</script>");
  stats.MarkLevelVisited(0);
  stats.MarkLevelVisited(4);
  stats.CountStep(4);
  stats.CountMonsterKill(4, 6, 0, ChampionBlow(7, HERO_STATS_UNARMED, 1));
  HeroStatsExportInfo info = {"dungeon.dat", "2026-10-31 12:00"};
  std::string json = HeroStatsExport_Json(stats, Columns(stats), NAMES, info);
  CHECK(json.find("\"-1\":{") != std::string::npos);
  CHECK(json.find("\"0\":{") != std::string::npos);
  CHECK(json.find("\"4\":{") != std::string::npos);
  CHECK(json.find("\"levels\":[0,4]") != std::string::npos);
  CHECK(json.find("\"separator\":\",\"") != std::string::npos);
  CHECK(json.find("\"move\":[1,0,0,0,0,0,0,0]") != std::string::npos);
  CHECK(json.find("[\"Screamer\",[1,0],0,0,0,1]") != std::string::npos);
  std::string html = HeroStatsExport_Html(stats, Columns(stats), NAMES, info);
  CHECK(html.find("<!doctype html>") == 0);
  // The only </script> are the ones that close the three scripts
  size_t closings = 0;
  for (size_t at = html.find("</script>"); at != std::string::npos; at = html.find("</script>", at + 1)) closings++;
  CHECK(closings == 3);
}

void TestHeroStatsPages(void)
{
  VariantsWithoutNamesShareALine();
  UnarmedAttacksAreOneWeapon();
  WarCryIsAnUnarmedAction();
  SpellLinesCountEveryCast();
  MonsterSpellsAreNotMonsterAttacks();
  OverviewCarriesTheMovement();
  JsonStringsAreSafeInAScript();
  ExportHoldsEveryLevel();
}
