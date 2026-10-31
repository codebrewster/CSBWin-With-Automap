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
  stats.CountStep(3);
  stats.CountTick(3, false);
  stats.Forget();
  CHECK(stats.Champions().empty());
  CHECK(stats.VisitedLevels().empty());
  CHECK(stats.Attacks(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).empty());
  CHECK(stats.MonsterKills(HERO_STATS_WHOLE_RUN).empty());
  CHECK(stats.Kills(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 0);
  CHECK(stats.DamageDealt(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroDamageMelee) == 0);
  CHECK(stats.Movement(HERO_STATS_WHOLE_RUN).steps == 0 && stats.Movement(HERO_STATS_WHOLE_RUN).ticks == 0);
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

static HeroStatsBlow MonsterMissile(int type, int missileRn)
{
  HeroStatsBlow blow = MonsterBlow(type);
  blow.spell = HERO_SPELL_MONSTER_MISSILE | (uint32_t)(missileRn & 0xff);
  return blow;
}

// A monster's fireball and its lightning are rows of their own; its claws are not a missile
static void MonsterMissilesKeepTheirKind(void)
{
  HeroStats stats;
  stats.CreditHarm(9, 3, HeroHarmMonsterMissile, MonsterMissile(22, 0xff80), 30);
  stats.CreditHarm(9, 3, HeroHarmMonsterMissile, MonsterMissile(22, 0xff80), 10);
  stats.CreditHarm(9, 3, HeroHarmMonsterMissile, MonsterMissile(22, 0xff82), 25);
  stats.CreditHarm(9, 3, HeroHarmMonster, MonsterMissile(22, 0xff80), 5);
  std::vector<HeroStatsHarmRow> rows = stats.HarmRows(9, 3);
  CHECK(rows.size() == 3);
  if (rows.size() == 3)
  {
    CHECK(rows[0].source == HeroHarmMonsterMissile && rows[0].spell == (HERO_SPELL_MONSTER_MISSILE | 0x80));
    CHECK(rows[0].amount == 40);
    CHECK(rows[1].spell == (HERO_SPELL_MONSTER_MISSILE | 0x82) && rows[1].amount == 25);
    CHECK(rows[2].source == HeroHarmMonster && rows[2].spell == 0);
  }
}

static void MovementIsKeptPerLevel(void)
{
  HeroStats stats;
  stats.CountStep(1);
  stats.CountStep(1);
  stats.CountTurn(1);
  stats.CountStep(4);
  stats.CountTeleport(4);
  for (int i = 0; i < 30; i++) stats.CountTick(4, i < 10);
  HeroStatsMovement one = stats.Movement(1);
  CHECK(one.steps == 2 && one.turns == 1 && one.teleports == 0 && one.ticks == 0);
  stats.CountPitFall(4);
  stats.CountPitClimb(4);
  stats.CountPitClimb(5);
  stats.CountBump(1);
  HeroStatsMovement run = stats.Movement(HERO_STATS_WHOLE_RUN);
  CHECK(run.steps == 3 && run.turns == 1 && run.teleports == 1 && run.ticks == 30 && run.sleepTicks == 10);
  CHECK(run.pitFalls == 1 && run.pitClimbs == 2 && run.bumps == 1);
  CHECK(stats.Movement(1).bumps == 1 && stats.Movement(5).pitClimbs == 1);
  CHECK(stats.Movement(2).steps == 0);
}

// The layout of version 13: harm rows without the missile kind, no movement table. A store with
// one harm row, written now, becomes that by cutting the kind and the eight empty counts at the
// end (movement, items, eaten and drunk, doors bashed, chests opened, food and water, tiles, scrolls).
static void LayoutThirteenIsStillRead(void)
{
  HeroStats written;
  written.CreditHarm(9, 3, HeroHarmMonster, MonsterBlow(10), 12);
  std::vector<uint8_t> bytes;
  written.Serialize(&bytes);
  // Six empty tables, the harm count, then the record of 2 + 4 + 4 + 2 + 4 + 4 bytes and its kind
  size_t kind = 7 * 4 + 20;
  bytes.erase(bytes.begin() + kind, bytes.begin() + kind + 4);
  bytes.erase(bytes.end() - 32, bytes.end());
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size(), 13));
  CHECK(read.Harm(9, 3) == 12 && read.HarmRows(9, 3).size() == 1);
  CHECK(read.HarmRows(9, 3)[0].monsterType == 10 && read.HarmRows(9, 3)[0].spell == 0);
  CHECK(read.Movement(HERO_STATS_WHOLE_RUN).steps == 0);
  // The same bytes are short for the current layout
  HeroStats current;
  CHECK(!current.Deserialize(bytes.data(), bytes.size()));
}

// Actions that cannot hit are uses, apart from the tries, and survive the file
static void ActionUsesAreKeptApartFromTries(void)
{
  HeroStats stats;
  HeroStatsBlow warCry = ChampionBlow(7, HERO_STATS_UNARMED, 8);
  stats.CountActionUse(2, warCry);
  stats.CountActionUse(2, warCry);
  stats.CountAttackTry(2, ChampionBlow(7, HERO_STATS_UNARMED, 6));
  std::vector<HeroStatsAttackRow> rows = stats.Attacks(7, 2);
  CHECK(rows.size() == 2);
  if (rows.size() == 2)
  {
    CHECK(rows[0].option == 6 && rows[0].tries == 1 && rows[0].uses == 0);
    CHECK(rows[1].option == 8 && rows[1].tries == 0 && rows[1].uses == 2);
  }
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.Attacks(7, 2).size() == 2 && read.Attacks(7, 2)[1].uses == 2);
}

// The layout of version 14: attack rows without uses, movement without sleep, pits and bumps.
// A store with one attack row and one level of movement, written now, becomes that by cutting
// the uses, the four newer movement numbers and the seven empty counts after them.
static void LayoutFourteenIsStillRead(void)
{
  HeroStats written;
  written.CountAttackTry(1, ChampionBlow(7, AXE, 2));
  written.CountStep(1);
  written.CountTick(1, true);
  written.CountBump(1);
  std::vector<uint8_t> bytes;
  written.Serialize(&bytes);
  // Two empty tables, the attack count, then the record: 2 + 4 + 4 + 1 + 4 bytes of key, four
  // counts, the uses
  size_t uses = 3 * 4 + 15 + 16;
  bytes.erase(bytes.begin() + uses, bytes.begin() + uses + 4);
  bytes.erase(bytes.end() - 44, bytes.end());
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size(), 14));
  CHECK(read.Attacks(7, 1).size() == 1 && read.Attacks(7, 1)[0].tries == 1 && read.Attacks(7, 1)[0].uses == 0);
  HeroStatsMovement moved = read.Movement(1);
  CHECK(moved.steps == 1 && moved.ticks == 1 && moved.sleepTicks == 0 && moved.bumps == 0);
}

static void ItemsAreKeptPerLevel(void)
{
  const int GOLD_KEY = 11, COIN = 12, TORCH = 4;
  HeroStats stats;
  stats.CountItem(1, TORCH, HeroItemPickedUp);
  stats.CountItem(1, GOLD_KEY, HeroItemPickedUp);
  stats.CountItem(3, GOLD_KEY, HeroItemUsedInWall);
  stats.CountItem(3, COIN, HeroItemUsedInWall);
  stats.CountItem(3, COIN, HeroItemUsedInWall);
  std::vector<HeroStatsItemRow> run = stats.Items(HERO_STATS_WHOLE_RUN);
  CHECK(run.size() == 3);
  if (run.size() == 3)
  {
    CHECK(run[0].item == TORCH && run[0].pickedUp == 1 && run[0].usedInWall == 0);
    CHECK(run[1].item == GOLD_KEY && run[1].pickedUp == 1 && run[1].usedInWall == 1);
    CHECK(run[2].item == COIN && run[2].pickedUp == 0 && run[2].usedInWall == 2);
  }
  CHECK(stats.Items(1).size() == 2 && stats.Items(2).empty() && stats.Items(3).size() == 2);
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.Items(3).size() == 2 && read.Items(3)[1].item == COIN && read.Items(3)[1].usedInWall == 2);
  stats.Forget();
  CHECK(stats.Items(HERO_STATS_WHOLE_RUN).empty());
}

// The layout of version 15 has no item table and nothing after it: the seven counts at the end are cut
static void LayoutFifteenIsStillRead(void)
{
  HeroStats written;
  written.CountStep(1);
  std::vector<uint8_t> bytes;
  written.Serialize(&bytes);
  bytes.erase(bytes.end() - 28, bytes.end());
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size(), 15));
  CHECK(read.Movement(1).steps == 1 && read.Items(HERO_STATS_WHOLE_RUN).empty());
  HeroStats current;
  CHECK(!current.Deserialize(bytes.data(), bytes.size()));
}

static void EatingDoorsAndChestsAreCounted(void)
{
  const int APPLE = 20, VI_POTION = 30;
  HeroStats stats;
  stats.CountConsumed(7, 1, APPLE, HeroConsumeEaten);
  stats.CountConsumed(7, 1, APPLE, HeroConsumeEaten);
  stats.CountConsumed(9, 2, VI_POTION, HeroConsumeDrunk);
  stats.CountDoorBashed(7, 2, HeroDoorSmashed);
  stats.CountDoorBashed(9, 2, HeroDoorBySpell);
  stats.CountChestOpened(1);
  CHECK(stats.Eaten(7, 1) == 2 && stats.Eaten(7, 2) == 0 && stats.Drunk(7, HERO_STATS_WHOLE_RUN) == 0);
  CHECK(stats.Drunk(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 1);
  std::vector<HeroStatsConsumedRow> rows = stats.Consumed(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(rows.size() == 2);
  if (rows.size() == 2)
  {
    CHECK(rows[0].item == APPLE && rows[0].eaten == 2 && rows[0].drunk == 0);
    CHECK(rows[1].item == VI_POTION && rows[1].drunk == 1);
  }
  CHECK(stats.DoorsBashed(7, 2) == 1 && stats.DoorsBashed(HERO_STATS_PARTY, 2) == 2 && stats.DoorsBashed(9, 1) == 0);
  CHECK(stats.DoorsBashed(HERO_STATS_PARTY, 2, HeroDoorSmashed) == 1 && stats.DoorsBashed(9, 2, HeroDoorBySpell) == 1);
  CHECK(stats.DoorsBashed(HERO_STATS_PARTY, 2, HeroDoorShot) == 0);
  // A door opened by Zo is no bash
  stats.CountDoorOpenedByZo(9, 2);
  CHECK(stats.DoorsOpenedByZo(9, 2) == 1 && stats.DoorsOpenedByZo(7, 2) == 0 && stats.DoorsBashed(9, 2) == 1);
  // Nor are the door buttons the party clicks
  stats.CountDoorButton(2, true);
  stats.CountDoorButton(2, true);
  stats.CountDoorButton(1, false);
  CHECK(stats.DoorsOpenedByButton(2) == 2 && stats.DoorsClosedByButton(HERO_STATS_WHOLE_RUN) == 1);
  CHECK(stats.DoorsBashed(HERO_STATS_PARTY, 2) == 2);
  CHECK(stats.ChestsOpened(1) == 1 && stats.ChestsOpened(HERO_STATS_WHOLE_RUN) == 1 && stats.ChestsOpened(2) == 0);
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.Eaten(7, 1) == 2 && read.DoorsBashed(9, 2, HeroDoorBySpell) == 1 && read.ChestsOpened(1) == 1);
  CHECK(read.DoorsOpenedByZo(9, 2) == 1 && read.DoorsOpenedByButton(2) == 2 && read.DoorsClosedByButton(1) == 1);
  // Version 17: a door row without its method, no food and water table. One door row of
  // 2 + 4 + 4 bytes, its method, then the empty chest, food, tile and scroll counts.
  HeroStats door;
  door.CountDoorBashed(7, 3, HeroDoorShot);
  bytes.clear();
  door.Serialize(&bytes);
  bytes.erase(bytes.end() - 12, bytes.end());
  bytes.erase(bytes.end() - 8, bytes.end() - 4);
  CHECK(read.Deserialize(bytes.data(), bytes.size(), 17));
  CHECK(read.DoorsBashed(7, 3) == 1 && read.DoorsBashed(7, 3, HeroDoorUnknown) == 1 && read.DoorsBashed(7, 3, HeroDoorShot) == 0);
  // Version 16 has none of the six tables
  HeroStats empty;
  empty.CountItem(1, 4, HeroItemPickedUp);
  bytes.clear();
  empty.Serialize(&bytes);
  bytes.erase(bytes.end() - 24, bytes.end());
  CHECK(read.Deserialize(bytes.data(), bytes.size(), 16));
  CHECK(read.Items(1).size() == 1 && read.Eaten(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 0);
  stats.Forget();
  CHECK(stats.Consumed(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).empty() && stats.ChestsOpened(HERO_STATS_WHOLE_RUN) == 0);
}

// Food and water: what rose counts as gained, the rest of the value as wasted
static void FoodAndWaterAreGainedOrWasted(void)
{
  HeroStats stats;
  stats.CreditFood(7, 1, 500, 500);
  stats.CreditFood(7, 1, 500, 120);
  stats.CreditFood(7, 1, -200, 0);     // a harmful item adds nothing
  stats.CreditWater(7, 2, 1600, 1600);
  stats.CreditWater(9, 2, 1600, 0);
  stats.CountFountainDrink(9, 2);
  HeroStatsNourishment halk = stats.Nourishment(7, HERO_STATS_WHOLE_RUN);
  CHECK(halk.foodGained == 620 && halk.foodWasted == 380 && halk.waterGained == 1600 && halk.waterWasted == 0);
  HeroStatsNourishment party = stats.Nourishment(HERO_STATS_PARTY, 2);
  CHECK(party.waterGained == 1600 && party.waterWasted == 1600 && party.fountainDrinks == 1 && party.foodGained == 0);
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.Nourishment(7, 1).foodWasted == 380 && read.Nourishment(9, 2).fountainDrinks == 1);
}

// Tiles walked count each cell once; explored tiles come from outside and are not stored
static void TilesWalkedAndExplored(void)
{
  HeroStats stats;
  stats.MarkTileWalked(1, 3, 4);
  stats.MarkTileWalked(1, 3, 4);
  stats.MarkTileWalked(1, 3, 5);
  stats.MarkTileWalked(2, 3, 4);
  std::map<int, int> explored;
  explored[1] = 40;
  explored[2] = 12;
  stats.SetExploredTiles(explored);
  CHECK(stats.Movement(1).tilesWalked == 2 && stats.Movement(1).tilesExplored == 40);
  HeroStatsMovement run = stats.Movement(HERO_STATS_WHOLE_RUN);
  CHECK(run.tilesWalked == 3 && run.tilesExplored == 52);
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.Movement(1).tilesWalked == 2 && read.Movement(1).tilesExplored == 0);
  // Version 18 has no tiles and no scrolls: the two counts at the end are cut
  HeroStats none;
  bytes.clear();
  none.Serialize(&bytes);
  bytes.erase(bytes.end() - 8, bytes.end());
  CHECK(read.Deserialize(bytes.data(), bytes.size(), 18) && read.Movement(1).tilesWalked == 0);
}

static void ScrollsAndWallTextsRead(void)
{
  HeroStats stats;
  stats.CountScrollRead(1);
  stats.CountScrollRead(3);
  stats.CountScrollRead(3);
  std::map<int, int> texts;
  texts[1] = 4;
  stats.SetWallTextsRead(texts);
  CHECK(stats.ScrollsRead(3) == 2 && stats.ScrollsRead(HERO_STATS_WHOLE_RUN) == 3);
  CHECK(stats.WallTextsRead(1) == 4 && stats.WallTextsRead(2) == 0 && stats.WallTextsRead(HERO_STATS_WHOLE_RUN) == 4);
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.ScrollsRead(3) == 2 && read.WallTextsRead(1) == 0);
  std::map<int, int> seen, viewed;
  seen[0] = 12;
  viewed[0] = 5;
  stats.SetChampionsSeen(seen, viewed);
  CHECK(stats.ChampionsSeen(0) == 12 && stats.ChampionsViewed(HERO_STATS_WHOLE_RUN) == 5 && stats.ChampionsSeen(1) == 0);
  // Version 19 has no scroll table
  HeroStats none;
  bytes.clear();
  none.Serialize(&bytes);
  bytes.erase(bytes.end() - 4, bytes.end());
  CHECK(read.Deserialize(bytes.data(), bytes.size(), 19) && read.ScrollsRead(HERO_STATS_WHOLE_RUN) == 0);
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
  CHECK(HeroStats_WeaponName(HERO_STATS_UNARMED, names, 3) == "UNARMED");
  CHECK(HeroStats_WeaponName(HERO_STATS_NO_WEAPON, names, 3) == "");
  // A type with no name, or none the table has: its number
  CHECK(HeroStats_WeaponName(1, names, 3) == "#1");
  CHECK(HeroStats_WeaponName(3, names, 3) == "#3");
  CHECK(HeroStats_WeaponName(-7, names, 3) == "#-7");
}

static void ManaGainIsKeptBySource(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.CreditManaGained(7, 1, HeroManaRegeneration, 30);
  stats.CreditManaGained(7, 2, HeroManaPotion, 12);
  stats.CreditManaGained(9, 1, HeroManaPotion, 5);
  CHECK(stats.ManaGainedBy(7, HERO_STATS_WHOLE_RUN, HeroManaRegeneration) == 30);
  CHECK(stats.ManaGainedBy(7, HERO_STATS_WHOLE_RUN, HeroManaPotion) == 12);
  CHECK(stats.ManaGainedBy(HERO_STATS_PARTY, 1, HeroManaPotion) == 5);
  CHECK(stats.ManaGainedBy(7, 1, HeroManaRegeneration) + stats.ManaGainedBy(7, 1, HeroManaPotion) == stats.ManaGained(7, 1));
}

void TestHeroStats(void)
{
  ManaGainIsKeptBySource();
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
  MonsterMissilesKeepTheirKind();
  MovementIsKeptPerLevel();
  LayoutThirteenIsStillRead();
  ActionUsesAreKeptApartFromTries();
  LayoutFourteenIsStillRead();
  ItemsAreKeptPerLevel();
  LayoutFifteenIsStillRead();
  EatingDoorsAndChestsAreCounted();
  FoodAndWaterAreGainedOrWasted();
  TilesWalkedAndExplored();
  ScrollsAndWallTextsRead();
  HealingRemembersTheBrewer();
  ManaIsKeptPerSpell();
  SpellBlowsCreditTheSpellRow();
  DeathsKeepOrderCauseAndLevel();
  MergeHardshipTakesTheLargerOfEach();
  TakeFromFileReplacesTheRestAndKeepsHardship();
}
