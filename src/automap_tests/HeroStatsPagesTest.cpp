#include "TestSupport.h"
#include "../HeroStatsExport.h"
#include "../HeroStatsPages.h"
#include "../HeroStatsCatalogue.h"

// Type 6 has two variants the dungeon does not name apart; type 25 is named
static std::string MonsterName(int monsterType, int variant)
{
  (void)variant;
  if (monsterType == 6) return "Screamer";
  if (monsterType == 25) return "Lord Order";
  return "Monster #" + std::to_string(monsterType);
}

// Items 11 to 13 are named apart; 14 is a second kind of key with the same name as 11
static std::string WeaponName(int weapon)
{
  if (weapon == 11 || weapon == 14) return "GOLD KEY";
  if (weapon == 12) return "GOLD COIN";
  if (weapon == 13) return "TORCH";
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
  // Attacks count the War Cry; the hit rate is the punch's
  HeroStatsRecord record = HeroStatsCatalogue_Record(stats, 7, HERO_STATS_WHOLE_RUN);
  CHECK(record.value[HeroNumAttacks] == 2 && record.value[HeroNumHitRate] == 100);
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
  HeroStatsBlow screamerClaws = claws;
  screamerClaws.monsterType = 6;
  HeroStatsBlow screamerFireball = fireball;
  screamerFireball.monsterType = 6;
  stats.CreditHarm(7, 3, HeroHarmMonster, claws, 10);
  stats.CreditHarm(7, 3, HeroHarmMonster, screamerClaws, 4);
  stats.CreditHarm(7, 3, HeroHarmMonsterMissile, fireball, 30);
  stats.CreditHarm(7, 3, HeroHarmMonsterMissile, screamerFireball, 20);
  HeroStatsHeroPage page = HeroStatsPages_Hero(stats, Columns(stats), 7, 3, NAMES);
  // A monster's spell names its caster; melee stays one line for all monsters
  CHECK(page.taken.size() == 3);
  if (page.taken.size() == 3)
  {
    CHECK(page.taken[0].text == "Fireball (Lord Order)" && page.taken[0].amount == 30);
    CHECK(page.taken[1].text == "Fireball (Screamer)" && page.taken[1].amount == 20);
    CHECK(page.taken[2].text == "Monster attacks" && page.taken[2].amount == 14);
  }
}

static void RecordsCarryTheMovement(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.CountStep(1);
  stats.CountTurn(1);
  stats.CountTick(2, false);
  HeroStatsRecord level1 = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, 1);
  CHECK(level1.value[HeroNumSteps] == 1 && level1.value[HeroNumTurns] == 1 && level1.value[HeroNumTicks] == 0);
  CHECK(HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).value[HeroNumTicks] == 1);
}

static void ItemsAreListedByName(void)
{
  HeroStats stats;
  stats.CountItem(1, 13, HeroItemPickedUp);
  stats.CountItem(1, 13, HeroItemPickedUp);
  stats.CountItem(1, 11, HeroItemPickedUp);
  stats.CountItem(2, 14, HeroItemUsedInWall);
  stats.CountItem(2, 11, HeroItemUsedInWall);
  stats.CountItem(2, 12, HeroItemUsedInWall);
  // Eaten and drunk items join the lines of their names; an item never picked up gets its own
  stats.MeetChampion(7, "HALK");
  stats.CountConsumed(7, 2, 13, HeroConsumeEaten);
  stats.CountConsumed(7, 2, 30, HeroConsumeDrunk);
  stats.CountDoorBashed(7, 2, HeroDoorShot);
  stats.CountChestOpened(2);
  std::vector<HeroStatsItemLine> lines = HeroStatsPages_Items(stats, HERO_STATS_WHOLE_RUN, NAMES);
  CHECK(lines.size() == 4);
  if (lines.size() == 4)
  {
    CHECK(lines[0].name == "TORCH" && lines[0].pickedUp == 2 && lines[0].usedInWall == 0 && lines[0].eaten == 1);
    CHECK(lines[1].name == "GOLD KEY" && lines[1].pickedUp == 1 && lines[1].usedInWall == 2);
    CHECK(lines[2].name == "GOLD COIN" && lines[2].pickedUp == 0 && lines[2].usedInWall == 1);
    CHECK(lines[3].name == "SWORD" && lines[3].drunk == 1 && lines[3].pickedUp == 0);
  }
  HeroStatsItemLine all = HeroStatsPages_AllItems(lines);
  CHECK(all.name == "All items" && all.pickedUp == 3 && all.usedInWall == 3 && all.drunk == 1 && all.eaten == 1);
  CHECK(HeroStatsPages_Items(stats, 2, NAMES).size() == 4);
  HeroStatsRecord party2 = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, 2);
  HeroStatsRecord halk2 = HeroStatsCatalogue_Record(stats, 7, 2);
  CHECK(party2.value[HeroNumPickedUp] == 0 && party2.value[HeroNumUsedInWalls] == 3 && party2.value[HeroNumChests] == 1);
  CHECK(halk2.value[HeroNumEaten] == 1 && halk2.value[HeroNumDrunk] == 1 && halk2.value[HeroNumDoorsBashed] == 1);
  CHECK(party2.value[HeroNumDoorsBashed] == 1 && party2.value[HeroNumDoorsShot] == 1 && party2.value[HeroNumDoorsSmashed] == 0);
  stats.CreditFood(7, 2, 500, 400);
  stats.CountFountainDrink(7, 2);
  stats.CountDoorOpenedByZo(7, 2);
  stats.CountDoorButton(2, true);
  stats.CountDoorButton(2, false);
  stats.CountDoorButton(2, false);
  party2 = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, 2);
  halk2 = HeroStatsCatalogue_Record(stats, 7, 2);
  CHECK(halk2.value[HeroNumDoorsByZo] == 1 && halk2.value[HeroNumDoorsBashed] == 1);
  CHECK(party2.value[HeroNumDoorsButtonOpened] == 1 && party2.value[HeroNumDoorsButtonClosed] == 2);
  CHECK(halk2.value[HeroNumFoodGained] == 400 && halk2.value[HeroNumFoodWasted] == 100);
  // Drunk includes the fountain, as the Eaten and drunk table lists it
  CHECK(halk2.value[HeroNumDrunk] == 2 && halk2.value[HeroNumEaten] == 1);
  CHECK(HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).value[HeroNumPickedUp] == 3);
  // The champion's page: eaten and drunk by item, the fountain last
  HeroStatsHeroPage halk = HeroStatsPages_Hero(stats, Columns(stats), 7, 2, NAMES);
  CHECK(halk.eatenTotal == 1 && halk.drunkTotal == 2 && halk.eaten.size() == 1 && halk.eaten[0].text == "TORCH");
  CHECK(halk.drunk.size() == 2 && halk.drunk[1].text == "Fountain" && halk.nourishment.foodWasted == 100);
  CHECK(halk.drunkTotal == halk2.value[HeroNumDrunk]);
  stats.CountScrollRead(2);
  CHECK(HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, 2).value[HeroNumScrolls] == 1);
  std::map<int, int> seen, viewed;
  seen[2] = 3;
  viewed[2] = 1;
  stats.SetChampionsSeen(seen, viewed);
  HeroStatsRecord mirrors = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, 2);
  CHECK(mirrors.value[HeroNumChampionsSeen] == 3 && mirrors.value[HeroNumChampionsViewed] == 1);
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
  stats.CountItem(4, 11, HeroItemUsedInWall);
  HeroStatsExportInfo info = {"dungeon.dat", "2026-10-31 12:00"};
  std::string json = HeroStatsExport_Json(stats, Columns(stats), NAMES, info);
  CHECK(json.find("\"-1\":{") != std::string::npos);
  CHECK(json.find("\"0\":{") != std::string::npos);
  CHECK(json.find("\"4\":{") != std::string::npos);
  CHECK(json.find("\"levels\":[0,4]") != std::string::npos);
  CHECK(json.find("\"separator\":\",\"") != std::string::npos);
  // The catalogue travels with the numbers
  CHECK(json.find("{\"label\":\"Steps\"") != std::string::npos);
  CHECK(json.find("\"label\":" + HeroStatsExport_JsonString("Food & items")) != std::string::npos);
  // Level 4's records: the party's holds the step, a champion's has nothing for a party number
  std::string party = HeroStatsExport_RecordJson(HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, 4), HERO_STATS_PARTY, 4);
  std::string halk = HeroStatsExport_RecordJson(HeroStatsCatalogue_Record(stats, 7, 4), 7, 4);
  CHECK(json.find("\"4\":{\"records\":[" + halk) != std::string::npos);
  CHECK(json.find(party) != std::string::npos);
  std::vector<std::string> partyValues, halkValues;
  std::string token;
  for (size_t i = 1; i < party.size(); i++)
    if (party[i] == ',' || party[i] == ']') { partyValues.push_back(token); token.clear(); } else token += party[i];
  for (size_t i = 1; i < halk.size(); i++)
    if (halk[i] == ',' || halk[i] == ']') { halkValues.push_back(token); token.clear(); } else token += halk[i];
  CHECK(partyValues.size() == HERO_NUMBER_COUNT && halkValues.size() == HERO_NUMBER_COUNT);
  if (partyValues.size() == HERO_NUMBER_COUNT && halkValues.size() == HERO_NUMBER_COUNT)
  {
    CHECK(partyValues[HeroNumSteps] == "1");
    CHECK(halkValues[HeroNumSteps] == "null");
    CHECK(halkValues[HeroNumKills] == "1");
    CHECK(partyValues[HeroNumWipes] == "null");  // the whole run's only
  }
  CHECK(json.find("[\"Screamer\",[1,0],0,0,0,1]") != std::string::npos);
  CHECK(json.find("\"items\":[[\"GOLD KEY\",0,1]]") != std::string::npos);
  std::string html = HeroStatsExport_Html(stats, Columns(stats), NAMES, info);
  CHECK(html.find("<!doctype html>") == 0);
  // The only </script> are the ones that close the three scripts
  size_t closings = 0;
  for (size_t at = html.find("</script>"); at != std::string::npos; at = html.find("</script>", at + 1)) closings++;
  CHECK(closings == 3);
}

static HeroStatsBlow KindBlow(HeroStatsChampion who, int weapon, HeroDamageKind kind)
{
  HeroStatsBlow blow = ChampionBlow(who, weapon, 1);
  blow.kind = kind;
  return blow;
}

static void PartyWeaponsAreSplitByChampion(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.CountAttackTry(1, KindBlow(7, 5, HeroDamageMelee));
  stats.CountAttackHit(1, KindBlow(7, 5, HeroDamageMelee));
  stats.CreditMonsterDamage(1, KindBlow(7, 5, HeroDamageMelee), 6);
  stats.CountAttackTry(1, KindBlow(9, 5, HeroDamageMelee));
  stats.CountAttackHit(1, KindBlow(9, 5, HeroDamageMelee));
  stats.CreditMonsterDamage(1, KindBlow(9, 5, HeroDamageMelee), 4);
  // Friendly fire is named apart on a champion's page ("own", "from"), but splits on the party's
  HeroStatsBlow fire = KindBlow(9, HERO_STATS_NO_WEAPON, HeroDamageMagic);
  stats.CreditHarm(7, 1, HeroHarmFriendlyFire, fire, 8);
  stats.CreditHarm(9, 1, HeroHarmFriendlyFire, fire, 3);
  HeroStatsTopicPage combat = HeroStatsPages_Topic(stats, Columns(stats), HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN,
                                                   HeroTopicCombat, NAMES);
  CHECK(combat.hero.weapons.size() == 1);
  if (combat.hero.weapons.size() == 1) CHECK(combat.hero.weapons[0].byColumn == std::vector<int>({6, 4}));
  HeroStatsTopicPage survival = HeroStatsPages_Topic(stats, Columns(stats), HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN,
                                                     HeroTopicSurvival, NAMES);
  CHECK(survival.hero.taken.size() == 1);
  if (survival.hero.taken.size() == 1) CHECK(survival.hero.taken[0].byColumn == std::vector<int>({8, 3}));
  HeroStatsTopicPage halk = HeroStatsPages_Topic(stats, Columns(stats), 7, HERO_STATS_WHOLE_RUN, HeroTopicCombat, NAMES);
  CHECK(halk.hero.weapons.size() == 1 && halk.hero.weapons[0].byColumn.empty());
  CHECK(halk.details == std::vector<HeroStatsDetail>({HeroDetailWeapons}));
}

static void ChampionPageDropsPartyOnlyRows(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.CountStep(1);
  stats.CountDoorBashed(7, 1, HeroDoorSmashed);
  HeroStatsTopicPage halk = HeroStatsPages_Topic(stats, Columns(stats), 7, HERO_STATS_WHOLE_RUN, HeroTopicExploration, NAMES);
  CHECK(halk.numbers.size() == 5);
  for (size_t i = 0; i < halk.numbers.size(); i++) CHECK(!halk.numbers[i].partyOnly && halk.numbers[i].byColumn.size() == 1);
  if (halk.numbers.size() == 5) CHECK(halk.numbers[0].number == HeroNumDoorsBashed && halk.numbers[0].byColumn[0] == 1 && halk.numbers[0].party == 1);
  HeroStatsTopicPage party = HeroStatsPages_Topic(stats, Columns(stats), HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroTopicExploration, NAMES);
  CHECK(party.numbers.size() == 22);
  if (!party.numbers.empty()) CHECK(party.numbers[0].number == HeroNumSteps && party.numbers[0].partyOnly && party.numbers[0].party == 1);
  CHECK(party.tiles.size() == 4);
  // A champion's Overview has tiles of their own
  HeroStatsTopicPage overview = HeroStatsPages_Topic(stats, Columns(stats), 7, HERO_STATS_WHOLE_RUN, HeroTopicOverview, NAMES);
  CHECK(overview.tiles == std::vector<HeroStatsNumber>({HeroNumDamage, HeroNumKills, HeroNumTaken, HeroNumDeaths}));
}

static void LeftChampionStillCounts(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.CreditMonsterDamage(1, KindBlow(7, 5, HeroDamageMelee), 5);
  stats.CreditMonsterDamage(1, KindBlow(9, 5, HeroDamageMelee), 2);
  std::vector<HeroStatsChampion> inParty(1, 7);
  std::vector<HeroStatsColumn> columns = HeroStatsView_Columns(stats, inParty);
  CHECK(columns.size() == 2 && columns[1].hasLeft);
  HeroStatsTopicPage combat = HeroStatsPages_Topic(stats, columns, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroTopicCombat, NAMES);
  CHECK(!combat.numbers.empty());
  if (!combat.numbers.empty())
  {
    CHECK(combat.numbers[0].byColumn == std::vector<int>({5, 2}));
    CHECK(combat.numbers[0].party == 7);
  }
}

static void NoChampionsYet(void)
{
  HeroStats stats;
  stats.MarkLevelVisited(0);
  stats.CountStep(0);
  std::vector<HeroStatsColumn> none;
  HeroStatsTopicPage explore = HeroStatsPages_Topic(stats, none, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroTopicExploration, NAMES);
  CHECK(!explore.numbers.empty() && explore.numbers[0].byColumn.empty() && explore.numbers[0].party == 1);
  CHECK(HeroStatsPages_Cards(stats, none, HERO_STATS_WHOLE_RUN).empty());
  HeroStatsTopicPage combat = HeroStatsPages_Topic(stats, none, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroTopicCombat, NAMES);
  CHECK(!combat.numbers.empty() && combat.numbers[0].party == 0);
}

static void CardsShareOfDamage(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  std::vector<HeroStatsCard> zero = HeroStatsPages_Cards(stats, Columns(stats), HERO_STATS_WHOLE_RUN);
  CHECK(zero.size() == 2 && zero[0].damagePercent == 0 && zero[1].damagePercent == 0);
  stats.CreditMonsterDamage(1, KindBlow(7, 5, HeroDamageMelee), 3);
  stats.CreditMonsterDamage(1, KindBlow(9, 5, HeroDamageMelee), 1);
  std::vector<HeroStatsCard> cards = HeroStatsPages_Cards(stats, Columns(stats), HERO_STATS_WHOLE_RUN);
  CHECK(cards.size() == 2 && cards[0].damagePercent == 75 && cards[1].damagePercent == 25);
  CHECK(cards.size() == 2 && cards[0].column.id == 7 && cards[0].record.value[HeroNumDamage] == 3);
}

// HALK (7) kills on levels 0 and 4, SYRA (9) on level 4; steps on both
static HeroStats KillsOnTwoLevels(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.MarkLevelVisited(0);
  stats.MarkLevelVisited(4);
  stats.CountMonsterKill(0, 6, 0, ChampionBlow(7, HERO_STATS_UNARMED, 1));
  stats.CountMonsterKill(4, 6, 0, ChampionBlow(7, HERO_STATS_UNARMED, 1));
  stats.CountMonsterKill(4, 6, 0, ChampionBlow(9, HERO_STATS_UNARMED, 1));
  stats.CountMonsterKill(4, 6, 0, ChampionBlow(9, HERO_STATS_UNARMED, 1));
  stats.CountStep(0);
  stats.CountStep(4);
  stats.CountStep(4);
  return stats;
}

static void PerLevelEndsWithTheWholeRun(void)
{
  HeroStats stats = KillsOnTwoLevels();
  HeroStatsPerLevel table = HeroStatsPages_PerLevelTotals(stats, Columns(stats), HERO_STATS_PARTY, HeroTopicOverview);
  CHECK(table.rows.size() == 3);
  if (table.rows.size() != 3) return;
  CHECK(table.rows[0].level == 0 && table.rows[1].level == 4 && table.rows[2].level == HERO_STATS_WHOLE_RUN);
  for (size_t n = 0; n < table.numbers.size(); n++)
    if (HeroStatsCatalogue_Summable(table.numbers[n]))
      CHECK(table.rows[2].values[n] == table.rows[0].values[n] + table.rows[1].values[n]);
  CHECK(table.hasShare && table.shareNumber == HeroNumDamage);
  CHECK(table.rows[1].share.size() == 2);
}

static void ManyLevelsAllListed(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  for (int level = 0; level < 20; level++) stats.MarkLevelVisited(level);
  HeroStatsPerLevel table = HeroStatsPages_PerLevelTotals(stats, Columns(stats), HERO_STATS_PARTY, HeroTopicExploration);
  CHECK(table.rows.size() == 21);
  if (!table.rows.empty()) CHECK(table.rows.back().level == HERO_STATS_WHOLE_RUN);
  CHECK(HeroStatsView_Picker(stats, HERO_STATS_WHOLE_RUN).stepper);
}

static void ChampionTotalsSkipPartyNumbers(void)
{
  HeroStats stats = KillsOnTwoLevels();
  HeroStatsPerLevel table = HeroStatsPages_PerLevelTotals(stats, Columns(stats), 7, HeroTopicExploration);
  CHECK(table.numbers == std::vector<HeroStatsNumber>({HeroNumDoorsBashed, HeroNumDoorsByZo}));
  CHECK(!table.hasShare);
  CHECK(!table.rows.empty() && table.rows[0].share.empty());
}

static void GridMatchesTheRecords(void)
{
  HeroStats stats = KillsOnTwoLevels();
  HeroStatsPerLevel grid = HeroStatsPages_PerLevelGrid(stats, Columns(stats), HeroNumKills);
  CHECK(grid.numbers == std::vector<HeroStatsNumber>({HeroNumKills}));
  CHECK(grid.rows.size() == 3);
  if (grid.rows.size() != 3) return;
  CHECK(grid.rows[1].level == 4 && grid.rows[1].values == std::vector<int>({1, 2, 3}));
  CHECK(grid.rows[1].share == std::vector<int>({1, 2}));
  CHECK(grid.rows[2].values == std::vector<int>({2, 2, 4}));
}

static void WipesStayOutOfPerLevel(void)
{
  HeroStats stats = KillsOnTwoLevels();
  HeroStatsPerLevel table = HeroStatsPages_PerLevelTotals(stats, Columns(stats), HERO_STATS_PARTY, HeroTopicSurvival);
  for (size_t n = 0; n < table.numbers.size(); n++) CHECK(table.numbers[n] != HeroNumWipes && table.numbers[n] != HeroNumReloads);
  CHECK(!table.numbers.empty() && table.numbers[0] == HeroNumTaken);
}

void TestHeroStatsPages(void)
{
  PerLevelEndsWithTheWholeRun();
  ManyLevelsAllListed();
  ChampionTotalsSkipPartyNumbers();
  GridMatchesTheRecords();
  WipesStayOutOfPerLevel();
  PartyWeaponsAreSplitByChampion();
  ChampionPageDropsPartyOnlyRows();
  LeftChampionStillCounts();
  NoChampionsYet();
  CardsShareOfDamage();
  VariantsWithoutNamesShareALine();
  UnarmedAttacksAreOneWeapon();
  WarCryIsAnUnarmedAction();
  SpellLinesCountEveryCast();
  MonsterSpellsAreNotMonsterAttacks();
  RecordsCarryTheMovement();
  ItemsAreListedByName();
  JsonStringsAreSafeInAScript();
  ExportHoldsEveryLevel();
}
