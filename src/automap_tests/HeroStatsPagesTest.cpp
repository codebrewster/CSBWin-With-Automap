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
  // The War Cry is no attack: it counts in Other actions; the hit rate is the punch's
  HeroStatsRecord record = HeroStatsCatalogue_Record(stats, 7, HERO_STATS_WHOLE_RUN);
  CHECK(record.value[HeroNumAttacks] == 1 && record.value[HeroNumOtherActions] == 1 && record.value[HeroNumHitRate] == 100);
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
    // Mana: every cast's, the failed one too; Wasted is its part
    CHECK(page.spells[0].mana == 47 && page.spells[0].wasted == 7);
  }
  // Cleared runes cast nothing: they are no spell line
  stats.CreditManaSpent(9, 1, HERO_SPELL_RUNES_CLEARED, 3);
  page = HeroStatsPages_Hero(stats, Columns(stats), 9, HERO_STATS_WHOLE_RUN, NAMES);
  CHECK(page.spells.size() == 1);
}

static void PartySpellsAreSplitByChampion(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  uint32_t fireball = 0x00006f69;  // Ful Ir
  stats.CountCast(7, 1, fireball, 13);
  stats.CountCast(9, 1, fireball, 13);
  stats.CountCast(9, 1, fireball, 13);
  stats.CountFailedCast(9, 1, fireball, 5);
  HeroStatsHeroPage page = HeroStatsPages_Hero(stats, Columns(stats), HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, NAMES);
  CHECK(page.spells.size() == 1);
  if (page.spells.size() != 1) return;
  const std::vector<HeroStatsSpellCounts> &by = page.spells[0].byColumn;
  CHECK(by.size() == 2);
  if (by.size() != 2) return;
  CHECK(by[0].casts == 1 && by[0].mana == 13 && by[0].failed == 0);
  CHECK(by[1].casts == 3 && by[1].mana == 31 && by[1].failed == 1 && by[1].wasted == 5);
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
  stats.CountItem(1, 13, HeroItemTakenFromChest);
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
    CHECK(lines[0].name == "TORCH" && lines[0].pickedUp == 2 && lines[0].fromChest == 1 && lines[0].usedInWall == 0 && lines[0].eaten == 1);
    CHECK(lines[1].name == "GOLD KEY" && lines[1].pickedUp == 1 && lines[1].usedInWall == 2);
    CHECK(lines[2].name == "GOLD COIN" && lines[2].pickedUp == 0 && lines[2].usedInWall == 1);
    CHECK(lines[3].name == "SWORD" && lines[3].drunk == 1 && lines[3].pickedUp == 0);
  }
  HeroStatsItemLine all = HeroStatsPages_AllItems(lines);
  CHECK(all.name == "All items" && all.pickedUp == 3 && all.fromChest == 1 && all.usedInWall == 3 && all.drunk == 1 && all.eaten == 1);
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

// The Champions tab: the party when the page was written, a champion found by its column
static void ExportHoldsTheChampionsAsTheyWere(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  HeroStatsExportInfo info = {"dungeon.dat", "2026-11-06 12:00"};
  std::string json = HeroStatsExport_Json(stats, Columns(stats), NAMES, info);
  CHECK(json.find("\"live\":{\"cursor\":null,\"list\":false,\"icons\":[],\"champions\":[]}") != std::string::npos);
  HeroStatsLiveChampion halk = {};
  halk.who = 7;
  halk.name = "HALK";
  halk.health = 80;
  halk.maxHealth = 120;
  halk.wounds = 0x04;
  halk.rank[0] = 2;
  HeroStatsLiveItem dagger = HeroStatsLive_Item(0, "DAGGER", false);
  halk.items.push_back(dagger);
  info.live.champions.push_back(halk);
  info.live.cursor.name = "TORCH";
  json = HeroStatsExport_Json(stats, Columns(stats), NAMES, info);
  CHECK(json.find("\"cursor\":[-1,\"TORCH\",false,null,0,-1,false]") != std::string::npos);
  CHECK(json.find("{\"hero\":0,\"name\":\"HALK\"") != std::string::npos);
  CHECK(json.find("\"hp\":[80,120]") != std::string::npos);
  CHECK(json.find("\"wounds\":\"Head\"") != std::string::npos);
  CHECK(json.find("[\"Fighter\",\"Neophyte\",0]") != std::string::npos);
  CHECK(json.find("\"woundBits\":4") != std::string::npos && json.find("\"portrait\":null") != std::string::npos);
  CHECK(json.find("\"items\":[[0,\"DAGGER\",false,null,0,-1,false]]") != std::string::npos);
}

// An item of the Champions tab: its slot, name, whether it is in a chest, its icon (a place in the
// icons, each written once in the order first used), its weight and its charges
static void TheExportCarriesIconsWeightAndCharges(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  HeroStatsExportInfo info = {"dungeon.dat", "2026-11-10 12:00"};
  HeroStatsLiveChampion halk = {};
  halk.who = 7;
  halk.name = "HALK";
  HeroStatsLiveItem staff = HeroStatsLive_Item(1, "STAFF OF CLAWS", false);
  staff.iconGraphic = 1;
  staff.iconTile = 44;  // key 300
  staff.weight = 9;
  staff.charges = 9;
  HeroStatsLiveItem chest = HeroStatsLive_Item(13, "CHEST", false);
  chest.weight = 51;
  HeroStatsLiveItem apple = HeroStatsLive_Item(13, "APPLE", true);
  apple.iconGraphic = 1;
  apple.iconTile = 45;  // key 301
  apple.weight = 4;
  HeroStatsLiveItem second = apple;
  second.slot = 14;
  second.inChest = false;
  HeroStatsLiveItem skin = HeroStatsLive_Item(15, "WATERSKIN", false);
  skin.charges = 3;
  skin.gulps = true;
  halk.items.push_back(staff);
  halk.items.push_back(chest);
  halk.items.push_back(apple);
  halk.items.push_back(second);
  halk.items.push_back(skin);
  info.live.champions.push_back(halk);
  info.icons[301] = "data:image/png;base64,BBB";
  info.icons[300] = "data:image/png;base64,AAA";
  info.portraits[7] = "data:image/png;base64,PPP";
  info.inventoryList = true;
  std::string json = HeroStatsExport_Json(stats, Columns(stats), NAMES, info);
  CHECK(json.find("\"icons\":[\"data:image/png;base64,AAA\",\"data:image/png;base64,BBB\"]") != std::string::npos);
  CHECK(json.find("[1,\"STAFF OF CLAWS\",false,0,9,9,false]") != std::string::npos);
  CHECK(json.find("[13,\"CHEST\",false,null,51,-1,false],[13,\"APPLE\",true,1,4,-1,false],[14,\"APPLE\",false,1,4,-1,false]") != std::string::npos);
  CHECK(json.find("[15,\"WATERSKIN\",false,null,0,3,true]") != std::string::npos);
  CHECK(json.find("\"list\":true") != std::string::npos);
  CHECK(json.find("\"portrait\":\"data:image/png;base64,PPP\"") != std::string::npos);
}

// An item whose icon has no picture (the dungeon gives none, or it could not be decoded) is
// written without one and the page still holds everything else
static void AnItemWithoutAPictureStillExports(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  HeroStatsExportInfo info = {"dungeon.dat", "2026-11-10 12:00"};
  HeroStatsLiveChampion halk = {};
  halk.who = 7;
  halk.name = "HALK";
  HeroStatsLiveItem odd = HeroStatsLive_Item(13, "ODD THING", false);
  odd.weight = 5;
  HeroStatsLiveItem lost = HeroStatsLive_Item(14, "LOST PICTURE", false);
  lost.iconGraphic = 2;
  lost.iconTile = 3;  // no picture for this key in info.icons
  halk.items.push_back(odd);
  halk.items.push_back(lost);
  info.live.champions.push_back(halk);
  std::string json = HeroStatsExport_Json(stats, Columns(stats), NAMES, info);
  CHECK(json.find("[13,\"ODD THING\",false,null,5,-1,false]") != std::string::npos);
  CHECK(json.find("[14,\"LOST PICTURE\",false,null,0,-1,false]") != std::string::npos);
  CHECK(json.find("\"icons\":[]") != std::string::npos);
  CHECK(json.find("\"portrait\":null") != std::string::npos && json.find("\"cursor\":null") != std::string::npos);
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
  CHECK(json.find("\"items\":[[\"GOLD KEY\",0,1,0]]") != std::string::npos);
  std::string html = HeroStatsExport_Html(stats, Columns(stats), NAMES, info);
  CHECK(html.find("<!doctype html>") == 0);
  // The only </script> are the ones that close the three scripts
  size_t closings = 0;
  for (size_t at = html.find("</script>"); at != std::string::npos; at = html.find("</script>", at + 1)) closings++;
  CHECK(closings == 3);
  // The Who buttons are .who: a table cell with that class would get their border and padding
  CHECK(html.find("class=\"who\">") == std::string::npos);
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
  // Each champion's own numbers with the weapon, so the Who bar can split any of its columns
  if (combat.hero.weapons.size() == 1)
  {
    const std::vector<HeroStatsOptionLine> &by = combat.hero.weapons[0].byColumn;
    CHECK(by.size() == 2);
    if (by.size() == 2)
    {
      CHECK(by[0].damage == 6 && by[0].tries == 1 && by[0].hits == 1 && by[0].kills == 0);
      CHECK(by[1].damage == 4 && by[1].tries == 1 && by[1].hits == 1);
    }
  }
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

// ---- Experience and Records ----

static int NoRankNow(HeroStatsChampion, int, int *percent)
{
  if (percent != NULL) *percent = 0;
  return -1;
}

static int RankFive(HeroStatsChampion, int, int *percent)
{
  if (percent != NULL) *percent = 40;
  return 5;
}

static HeroXpSource XpSource(HeroXpSourceKind kind, int weapon, int option, uint32_t spell, int monsterType)
{
  HeroXpSource source = HeroXpSource_Other();
  source.kind = kind;
  source.weapon = weapon;
  source.option = option;
  source.spell = spell;
  source.monsterType = monsterType;
  return source;
}

static const uint32_t FUL_IR = 0x00006f69;  // Ful Ir: a Fireball (as HeroStatsTest's runes)

static HeroStats ExperiencedParty(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.MarkLevelVisited(1);
  HeroStatsExperience &xp = stats.Experience();
  xp.Credit(7, 1, 4, XpSource(HeroXpAttack, 5, 1, HERO_STATS_NO_SPELL, -1), 30);       // Swing with a sword
  xp.Credit(7, 1, 5, XpSource(HeroXpAttack, 5, 2, HERO_STATS_NO_SPELL, -1), 10);       // Thrust
  xp.Credit(7, 1, 7, XpSource(HeroXpMonster, HERO_STATS_NO_WEAPON, HERO_STATS_NO_OPTION, HERO_STATS_NO_SPELL, 6), 4);
  xp.Credit(9, 1, 16, XpSource(HeroXpSpell, HERO_STATS_NO_WEAPON, HERO_STATS_NO_OPTION, FUL_IR, -1), 25);  // Fire
  xp.Credit(9, 1, 14, XpSource(HeroXpOtherAction, HERO_STATS_UNARMED, 8, HERO_STATS_NO_SPELL, -1), 3);     // War Cry
  xp.SetStartRank(7, 0, 3);
  xp.SetStartRank(9, 3, 4);
  HeroRankUp up = {9, 1, 3, 5, 77, XpSource(HeroXpSpell, HERO_STATS_NO_WEAPON, HERO_STATS_NO_OPTION, FUL_IR, -1)};
  xp.CountRankUp(up);
  return stats;
}

static HeroStatsTopicPage XpPage(const HeroStats &stats, HeroStatsChampion who, int (*rankNow)(HeroStatsChampion, int, int *))
{
  HeroStatsNames names = NAMES;
  names.rankNow = rankNow;
  return HeroStatsPages_Topic(stats, Columns(stats), who, HERO_STATS_WHOLE_RUN, HeroTopicExperience, names);
}

static void SourcesAddUpToTheClasses(void)
{
  HeroStats stats = ExperiencedParty();
  HeroStatsTopicPage page = XpPage(stats, HERO_STATS_PARTY, RankFive);
  int top = 0;
  const HeroStatsXpLine *attacks = NULL, *sword = NULL, *spells = NULL, *wizard = NULL, *fireball = NULL;
  for (size_t i = 0; i < page.sources.size(); i++)
  {
    const HeroStatsXpLine &line = page.sources[i];
    if (line.depth == 0) top += line.amount;
    if (line.depth == 0 && line.name == "Attacks") attacks = &line;
    if (line.depth == 1 && line.name == "SWORD") sword = &line;
    if (line.depth == 0 && line.name == "Spells") spells = &line;
    if (line.depth == 1 && line.name == "Wizard") wizard = &line;
    if (line.depth == 2 && line.note == "Fire") fireball = &line;
  }
  CHECK(top == stats.Experience().Gained(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, -1));
  CHECK(page.sources.size() > 0 && page.sources[0].name == "Attacks");
  CHECK(attacks != NULL && attacks->amount == 40 && attacks->hasChildren);
  CHECK(sword != NULL && sword->amount == 40);
  CHECK(spells != NULL && wizard != NULL && wizard->amount == 25);
  CHECK(fireball != NULL && fireball->name.find("Ful Ir") != std::string::npos);
  // The Party's lines split by column: HALK, SYRA
  CHECK(attacks != NULL && attacks->byColumn.size() == 2 && attacks->byColumn[0] == 40 && attacks->byColumn[1] == 0);
  // Every line under a weapon names its class and hidden skill
  bool swing = false;
  for (size_t i = 0; i < page.sources.size(); i++)
    if (page.sources[i].depth == 2 && page.sources[i].note == "Fighter \xc2\xb7 Swing") swing = page.sources[i].name == "PUNCH";
  CHECK(swing);
}

static void ChampionSourcesLeaveOutZeros(void)
{
  HeroStats stats = ExperiencedParty();
  HeroStatsTopicPage page = XpPage(stats, 9, RankFive);
  for (size_t i = 0; i < page.sources.size(); i++)
    if (page.sources[i].depth > 0) CHECK(page.sources[i].amount > 0);
  // The top lines are always there, with the party's amount for the share
  CHECK(page.sources.size() >= 5 && page.sources[0].name == "Attacks" && page.sources[0].amount == 0);
  CHECK(page.sources.size() >= 5 && page.sources[0].byColumn.size() == 1 && page.sources[0].byColumn[0] == 40);
}

static void ClassesOpenToTheirSkills(void)
{
  HeroStats stats = ExperiencedParty();
  HeroStatsTopicPage page = XpPage(stats, 7, RankFive);
  CHECK(page.classes.size() == 4);
  if (page.classes.size() != 4) return;
  const HeroStatsClassLine &fighter = page.classes[0];
  CHECK(fighter.name == "Fighter" && fighter.amount == 44 && fighter.rankNow == 5 && fighter.percentToNext == 40);
  CHECK(fighter.startRank == 3);
  CHECK(fighter.skills.size() == 4 && fighter.skills[0].name == "Swing" && fighter.skills[3].name == "Parry");
  stats.Experience().Credit(7, 1, 0, HeroXpSource_Other(), 2);
  page = XpPage(stats, 7, RankFive);
  CHECK(page.classes[0].skills.size() == 5 && page.classes[0].skills[4].name == "Fighter itself");
}

static void RankOfAGoneChampionIsTheLastKnown(void)
{
  HeroStats stats = ExperiencedParty();
  HeroStatsTopicPage page = XpPage(stats, 9, NoRankNow);
  CHECK(page.classes.size() == 4 && page.classes[3].rankNow == 5);  // started at 4, one rank gained
  CHECK(page.rankUps.size() == 1 && page.rankUps[0].className == "Wizard");
  CHECK(page.rankUps.size() == 1 && page.rankUps[0].rank == "Apprentice \xe2\x86\x92 Journeyman");
  CHECK(page.rankUps.size() == 1 && page.rankUps[0].with.find("Fireball") != std::string::npos);
  // No name function at all: the same
  page = XpPage(stats, 9, NULL);
  CHECK(page.classes.size() == 4 && page.classes[3].rankNow == 5);
}

static HeroRecordEntry RecordEntry(HeroStatsChampion who, int value)
{
  HeroRecordEntry entry = HeroRecordEntry_Empty();
  entry.who = who;
  entry.level = 1;
  entry.value = value;
  entry.time = 500;
  return entry;
}

static void RecordLinesShowTheirBlow(void)
{
  HeroStats stats = ExperiencedParty();
  HeroRecordEntry total = RecordEntry(9, 840);
  total.how = XpSource(HeroXpSpell, HERO_STATS_NO_WEAPON, HERO_STATS_NO_OPTION, FUL_IR, -1);
  total.monsterType = 6;
  total.count = 4;
  stats.Records().Offer(HeroRecBiggestSpellTotal, total);
  stats.Records().Offer(HeroRecBiggestMeleeHit, RecordEntry(7, 120));
  stats.Records().Offer(HeroRecBiggestMeleeHit, RecordEntry(9, 90));
  stats.Records().CountPitFall(3, 10);
  stats.Records().CountMove();
  HeroStatsTopicPage party = HeroStatsPages_Topic(stats, Columns(stats), HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroTopicRecords, NAMES);
  CHECK(party.records.size() == HERO_RECORD_COUNT - 1);
  const HeroStatsRecordLine *spell = NULL, *fall = NULL;
  for (size_t i = 0; i < party.records.size(); i++)
  {
    if (party.records[i].kind == HeroRecBiggestSpellTotal) spell = &party.records[i];
    if (party.records[i].kind == HeroRecLongestFall) fall = &party.records[i];
  }
  CHECK(spell != NULL && spell->whom == "4 \xc3\x97 Screamer" && spell->who == "SYRA" && spell->value == "840");
  CHECK(spell != NULL && spell->group == "Dealt" && spell->how.find("Fireball") != std::string::npos);
  CHECK(fall != NULL && fall->who == "party" && fall->value == "1" && fall->group == "Carrying & moving");
  HeroStatsTopicPage syra = HeroStatsPages_Topic(stats, Columns(stats), 9, HERO_STATS_WHOLE_RUN, HeroTopicRecords, NAMES);
  bool hasFall = false;
  const HeroStatsRecordLine *hit = NULL, *own = NULL;
  for (size_t i = 0; i < syra.records.size(); i++)
  {
    hasFall = hasFall || HeroRecord_PartyOnly(syra.records[i].kind);
    if (syra.records[i].kind == HeroRecBiggestMeleeHit) hit = &syra.records[i];
    if (syra.records[i].kind == HeroRecBiggestSpellTotal) own = &syra.records[i];
  }
  CHECK(!hasFall);
  CHECK(hit != NULL && !hit->held && hit->value == "90" && hit->partyValue == "120" && hit->partyHolder == 7);
  CHECK(own != NULL && own->held);
}

// The melee and ranged hits come first, among what was dealt; the biggest hit is not shown
static void MeleeAndRangedHitsReplaceTheBiggestHit(void)
{
  HeroStats stats = ExperiencedParty();
  stats.Records().Offer(HeroRecBiggestHit, RecordEntry(7, 200));  // from a file of before the split
  stats.Records().Offer(HeroRecBiggestMeleeHit, RecordEntry(7, 120));
  stats.Records().Offer(HeroRecBiggestRangedHit, RecordEntry(9, 60));
  HeroStatsTopicPage party = HeroStatsPages_Topic(stats, Columns(stats), HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroTopicRecords, NAMES);
  CHECK(party.records.size() == HERO_RECORD_COUNT - 1);
  for (size_t i = 0; i < party.records.size(); i++) CHECK(party.records[i].kind != HeroRecBiggestHit);
  CHECK(party.records.size() >= 3 && party.records[0].kind == HeroRecBiggestMeleeHit && party.records[1].kind == HeroRecBiggestRangedHit &&
        party.records[2].kind == HeroRecBiggestSpellHit);
  CHECK(party.records.size() >= 2 && party.records[0].group == "Dealt" && party.records[1].group == "Dealt");
  CHECK(party.records.size() >= 2 && party.records[0].label == "Biggest melee hit" && party.records[0].value == "120");
  CHECK(party.records.size() >= 2 && party.records[1].label == "Biggest ranged hit" && party.records[1].value == "60" &&
        party.records[1].who == "SYRA");
}

// Hover texts the spec asks for: what the actions that cannot hit train, where a streak began
static void SourcesAndStreaksSayMore(void)
{
  HeroStats stats = ExperiencedParty();
  HeroStatsTopicPage page = XpPage(stats, HERO_STATS_PARTY, RankFive);
  const HeroStatsXpLine *actions = NULL;
  for (size_t i = 0; i < page.sources.size(); i++)
    if (page.sources[i].depth == 0 && page.sources[i].name == "Other actions") actions = &page.sources[i];
  CHECK(actions != NULL && actions->help.find("Influence") != std::string::npos);
  stats.Records().CountKill(7, 2, 100, 6);
  stats.Records().CountKill(7, 4, 300, 6);
  HeroStatsTopicPage records = HeroStatsPages_Topic(stats, Columns(stats), HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroTopicRecords, NAMES);
  const HeroStatsRecordLine *streak = NULL;
  for (size_t i = 0; i < records.records.size(); i++)
    if (records.records[i].kind == HeroRecKillStreak) streak = &records.records[i];
  CHECK(streak != NULL && streak->when.find("Lv 2") != std::string::npos && streak->when.find("100") != std::string::npos);
  CHECK(streak != NULL && streak->when.find("Lv 4") != std::string::npos && streak->when.find("300") != std::string::npos);
}

static void RecordsPerLevelHaveNoShare(void)
{
  HeroStats stats = ExperiencedParty();
  stats.Records().Offer(HeroRecBiggestMeleeHit, RecordEntry(7, 120));
  stats.Records().Offer(HeroRecBiggestMeleeHit, RecordEntry(9, 90));
  HeroStatsPerLevel grid = HeroStatsPages_PerLevelGrid(stats, Columns(stats), HeroNumRecBiggestMeleeHit);
  CHECK(!grid.hasShare);
  CHECK(!grid.rows.empty() && grid.rows.back().level == HERO_STATS_WHOLE_RUN && grid.rows.back().values.back() == 120);
}

static void ExportHasExperienceAndRecords(void)
{
  HeroStats stats = ExperiencedParty();
  stats.Records().Offer(HeroRecBiggestMeleeHit, RecordEntry(7, 120));
  HeroStatsExportInfo info = {"dungeon.dat", "2026-11-05 12:00"};
  std::string json = HeroStatsExport_Json(stats, Columns(stats), NAMES, info);
  CHECK(json.find("\"xp\":{\"classes\":") != std::string::npos);
  CHECK(json.find("\"sources\":[[\"Attacks\"") != std::string::npos);
  CHECK(json.find("\"SWORD\"") != std::string::npos);
  CHECK(json.find("\"records\":[[" + std::to_string((int)HeroRecBiggestMeleeHit) + ",\"Dealt\",\"Biggest melee hit\",\"120\"") != std::string::npos);
  CHECK(json.find("\"xpFirst\":") != std::string::npos && json.find("\"recFirst\":") != std::string::npos);
  std::string html = HeroStatsExport_Html(stats, Columns(stats), NAMES, info);
  CHECK(html.find("function records(") != std::string::npos && html.find("function xpSources(") != std::string::npos);
}

void TestHeroStatsPages(void)
{
  ExportHasExperienceAndRecords();
  SourcesAddUpToTheClasses();
  ChampionSourcesLeaveOutZeros();
  ClassesOpenToTheirSkills();
  RankOfAGoneChampionIsTheLastKnown();
  RecordLinesShowTheirBlow();
  MeleeAndRangedHitsReplaceTheBiggestHit();
  RecordsPerLevelHaveNoShare();
  SourcesAndStreaksSayMore();
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
  PartySpellsAreSplitByChampion();
  MonsterSpellsAreNotMonsterAttacks();
  RecordsCarryTheMovement();
  ItemsAreListedByName();
  JsonStringsAreSafeInAScript();
  ExportHoldsEveryLevel();
  ExportHoldsTheChampionsAsTheyWere();
  TheExportCarriesIconsWeightAndCharges();
  AnItemWithoutAPictureStillExports();
}
