#include "TestSupport.h"
#include "../HeroStatsLive.h"

// The slots as the game numbers them: hands, body, pouches and quivers, neck, then the backpack
static void SlotsAreNamedAsTheGameNumbersThem(void)
{
  CHECK(HeroStatsLive_SlotName(0) == "Ready hand" && HeroStatsLive_SlotName(1) == "Action hand");
  CHECK(HeroStatsLive_SlotName(5) == "Feet" && HeroStatsLive_SlotName(10) == "Neck");
  CHECK(HeroStatsLive_SlotName(11) == "Pouch 1" && HeroStatsLive_SlotName(12) == "Quiver 1");
  CHECK(HeroStatsLive_SlotName(13) == "Backpack 1" && HeroStatsLive_SlotName(29) == "Backpack 17");
  CHECK(HeroStatsLive_SlotName(-1).empty() && HeroStatsLive_SlotName(30).empty());
}

static void WoundsAttributesAndTenthsRead(void)
{
  CHECK(HeroStatsLive_Wounds(0).empty());
  CHECK(HeroStatsLive_Wounds(0x04 | 0x20) == "Head, Feet");
  CHECK(HeroStatsLive_Wounds(0x01) == "Ready hand");
  CHECK(std::string(HeroStatsLive_AttributeName(0)) == "Strength" && std::string(HeroStatsLive_AttributeName(5)) == "Anti-fire");
  CHECK(std::string(HeroStatsLive_AttributeName(6)).empty());
  CHECK(HeroStatsLive_Tenths(123) == "12.3" && HeroStatsLive_Tenths(0) == "0.0" && HeroStatsLive_Tenths(-5) == "-0.5");
}

static void AChampionIsFoundByFingerprint(void)
{
  HeroStatsLiveParty party;
  HeroStatsLiveChampion one;
  one.who = 7;
  one.name = "HALK";
  party.champions.push_back(one);
  CHECK(HeroStatsLive_Find(party, 7) != NULL && HeroStatsLive_Find(party, 7)->name == "HALK");
  CHECK(HeroStatsLive_Find(party, 8) == NULL);
}

static void WeightAndChargesRead(void)
{
  CHECK(HeroStatsLive_WeightText(9, false) == "0.9 kg" && HeroStatsLive_WeightText(73, true) == "7.3 kg with contents");
  CHECK(HeroStatsLive_ChargesText(-1, false).empty());
  CHECK(HeroStatsLive_ChargesText(9, false) == "9 charges" && HeroStatsLive_ChargesText(1, false) == "1 charge");
  CHECK(HeroStatsLive_ChargesText(0, false) == "no charges");
  CHECK(HeroStatsLive_ChargesText(3, true) == "3 gulps left" && HeroStatsLive_ChargesText(1, true) == "1 gulp left");
  CHECK(HeroStatsLive_ChargesText(0, true) == "empty");
}

static void AnItemWithoutAnIconHasNoKey(void)
{
  HeroStatsLiveItem item = HeroStatsLive_Item(3, "ODD THING", false);
  CHECK(item.iconGraphic == -1 && item.charges == -1 && item.weight == 0 && !item.gulps && HeroStatsLive_IconKey(item) == -1);
  item.iconGraphic = 42;
  item.iconTile = 9;
  CHECK(HeroStatsLive_IconKey(item) == 42 * 256 + 9);
}

static void TheBodySlotsHaveTheirPlaces(void)
{
  // slot: column, row, as the design's table
  static const int PLACE[13][2] = {{1, 2}, {5, 2}, {3, 1}, {3, 2}, {3, 3}, {3, 4}, {1, 5}, {5, 4}, {4, 5}, {5, 5}, {2, 1}, {1, 4}, {4, 4}};
  for (int slot = 0; slot < 13; slot++)
  {
    int column = 0, row = 0;
    CHECK(HeroStatsLive_DollCell(slot, &column, &row) && column == PLACE[slot][0] && row == PLACE[slot][1]);
  }
  int column, row;
  CHECK(!HeroStatsLive_DollCell(13, &column, &row) && !HeroStatsLive_DollCell(-1, &column, &row));
}

static void AChestCountsWhatFollowsIt(void)
{
  std::vector<HeroStatsLiveItem> items;
  items.push_back(HeroStatsLive_Item(1, "SWORD", false));
  items.push_back(HeroStatsLive_Item(13, "CHEST", false));
  items.push_back(HeroStatsLive_Item(13, "GOLD COIN", true));
  items.push_back(HeroStatsLive_Item(13, "SCROLL", true));
  items.push_back(HeroStatsLive_Item(14, "APPLE", false));
  items.push_back(HeroStatsLive_Item(11, "ROCK", false));
  CHECK(HeroStatsLive_HeldCount(items, 1) == 2 && HeroStatsLive_HeldCount(items, 0) == 0 && HeroStatsLive_HeldCount(items, 4) == 0);
  CHECK(HeroStatsLive_HeldCount(items, 99) == 0);
  CHECK(HeroStatsLive_CarriedCount(items) == 3);  // the chest, the apple, the rock
}

static void ItemsSortByWhatTheyFeed(void)
{
  std::vector<HeroStatsLiveItem> items;
  items.push_back(HeroStatsLive_Item(1, "SWORD", false));
  items.push_back(HeroStatsLive_Item(2, "BREAD", false));
  items.push_back(HeroStatsLive_Item(3, "WATERSKIN", false));
  items.push_back(HeroStatsLive_Item(4, "APPLE", false));
  items[1].food = 600;
  items[2].water = 800;
  items[3].food = 200;
  CHECK(items[0].food == 0 && items[0].water == 0);
  std::vector<size_t> byFood = HeroStatsLive_Order(items, HeroLiveByFood, true);
  std::vector<size_t> byWater = HeroStatsLive_Order(items, HeroLiveByWater, true);
  CHECK(byFood.size() == 4 && byFood[0] == 1 && byFood[1] == 3 && byFood[2] == 0 && byFood[3] == 2);
  CHECK(byWater.size() == 4 && byWater[0] == 2 && byWater[1] == 0 && byWater[2] == 1 && byWater[3] == 3);
}

static HeroStatsLiveItem Weighing(int slot, const char *name, bool inChest, int weight)
{
  HeroStatsLiveItem item = HeroStatsLive_Item(slot, name, inChest);
  item.weight = weight;
  return item;
}

static bool OrderIs(const std::vector<size_t> &order, size_t a, size_t b, size_t c, size_t d, size_t e, size_t f)
{
  const size_t want[6] = {a, b, c, d, e, f};
  return order == std::vector<size_t>(want, want + 6);
}

// The inventory list sorts by slot, name or weight; what a chest holds stays under its chest,
// sorted the same way
static void TheListSortsWithAChestsContentsUnderIt(void)
{
  std::vector<HeroStatsLiveItem> items;
  items.push_back(Weighing(1, "SWORD", false, 32));
  items.push_back(Weighing(13, "CHEST", false, 88));
  items.push_back(Weighing(13, "GOLD COIN", true, 1));
  items.push_back(Weighing(13, "APPLE", true, 4));
  items.push_back(Weighing(14, "APPLE", false, 4));
  items.push_back(Weighing(11, "ROCK", false, 10));
  CHECK(OrderIs(HeroStatsLive_Order(items, HeroLiveBySlot, false), 0, 1, 2, 3, 4, 5));
  CHECK(OrderIs(HeroStatsLive_Order(items, HeroLiveBySlot, true), 5, 4, 1, 2, 3, 0));
  CHECK(OrderIs(HeroStatsLive_Order(items, HeroLiveByName, false), 4, 1, 3, 2, 5, 0));
  CHECK(OrderIs(HeroStatsLive_Order(items, HeroLiveByWeight, true), 1, 3, 2, 0, 5, 4));
  CHECK(HeroStatsLive_Order(std::vector<HeroStatsLiveItem>(), HeroLiveByName, false).empty());
}

// A weapon, what shoots, and armor, by the numbers the dungeon gives their kind
static HeroStatsLiveItem Weapon(int slot, const char *name, bool inChest, int damage, int kinetic, int shootDamage)
{
  HeroStatsLiveItem item = HeroStatsLive_Item(slot, name, inChest);
  item.damage = damage;
  item.kinetic = kinetic;
  item.shootDamage = shootDamage;
  return item;
}

static HeroStatsLiveItem Armor(int slot, const char *name, int armor, int sharpResistance)
{
  HeroStatsLiveItem item = HeroStatsLive_Item(slot, name, false);
  item.armor = armor;
  item.sharpResistance = sharpResistance;
  return item;
}

static void AWeaponSaysItsDamageAndKineticEnergy(void)
{
  HeroStatsLiveItem axe = Weapon(1, "AXE", false, 49, 33, 0), bow = Weapon(1, "BOW", false, 1, 50, 50);
  HeroStatsLiveItem wand = Weapon(1, "WAND", false, 0, 0, 0), apple = HeroStatsLive_Item(0, "APPLE", false);
  CHECK(apple.damage == -1 && apple.kinetic == -1 && apple.shootDamage == -1 && apple.armor == -1 && apple.sharpResistance == -1);
  CHECK(HeroStatsLive_WeaponText(axe) == "Damage 49 \xC2\xB7 kinetic energy 33");
  CHECK(HeroStatsLive_WeaponText(bow) == "Kinetic energy 50 \xC2\xB7 shoot damage 50");
  CHECK(HeroStatsLive_WeaponText(Weapon(1, "ODD", false, 0, 12, 0)) == "Damage 0 \xC2\xB7 kinetic energy 12");
  CHECK(HeroStatsLive_WeaponText(wand).empty() && HeroStatsLive_WeaponText(apple).empty());
  CHECK(HeroStatsLive_ListDamage(axe) == 49 && HeroStatsLive_ListDamage(bow) == -1 && HeroStatsLive_ListDamage(apple) == -1);
  CHECK(HeroStatsLive_ListDamage(wand) == -1);
  CHECK(HeroStatsLive_ListKinetic(axe) == 33 && HeroStatsLive_ListKinetic(bow) == 50);
  CHECK(HeroStatsLive_ListKinetic(wand) == -1 && HeroStatsLive_ListKinetic(apple) == -1);
}

static void ArmorSaysItsStrengthAndSharpResistance(void)
{
  CHECK(HeroStatsLive_ArmorText(Armor(0, "WOODEN SHIELD", 20, 3)) == "Armor 20 \xC2\xB7 sharp resistance 3");
  CHECK(HeroStatsLive_ArmorText(Armor(0, "RAGS", 0, 0)) == "Armor 0 \xC2\xB7 sharp resistance 0");
  CHECK(HeroStatsLive_ArmorText(Weapon(1, "AXE", false, 49, 33, 0)).empty());
  CHECK(HeroStatsLive_WeaponText(Armor(0, "WOODEN SHIELD", 20, 3)).empty());
}

static void TheListSortsByDamageKineticAndArmor(void)
{
  std::vector<HeroStatsLiveItem> items;
  items.push_back(Weapon(1, "AXE", false, 49, 33, 0));
  items.push_back(Weapon(0, "BOW", false, 1, 50, 50));
  items.push_back(HeroStatsLive_Item(11, "APPLE", false));
  items.push_back(Armor(3, "WOODEN SHIELD", 20, 3));
  items.push_back(HeroStatsLive_Item(13, "CHEST", false));
  items.push_back(Weapon(13, "DAGGER", true, 10, 19, 0));
  items.push_back(Weapon(13, "CLUB", true, 19, 10, 0));
  // What has no value is the lowest and keeps its order; a chest's contents stay under it, sorted the same way
  CHECK(HeroStatsLive_Order(items, HeroLiveByDamage, true) == (std::vector<size_t>{0, 1, 2, 3, 4, 6, 5}));
  CHECK(HeroStatsLive_Order(items, HeroLiveByDamage, false) == (std::vector<size_t>{1, 2, 3, 4, 5, 6, 0}));
  CHECK(HeroStatsLive_Order(items, HeroLiveByKinetic, true) == (std::vector<size_t>{1, 0, 2, 3, 4, 5, 6}));
  CHECK(HeroStatsLive_Order(items, HeroLiveByArmor, true) == (std::vector<size_t>{3, 0, 1, 2, 4, 5, 6}));
}

static bool Near(float a, float b)
{
  return a - b < 0.0005f && b - a < 0.0005f;
}

static void AnAttributesBarShowsNowAgainstMaxAndTheBest(void)
{
  HeroStatsAttributeBar even = HeroStatsLive_AttributeBar(41, 41, 55);
  CHECK(Near(even.solid, 41 / 55.0f) && even.lost == 0 && even.raised == 0);
  HeroStatsAttributeBar drained = HeroStatsLive_AttributeBar(38, 41, 55);
  CHECK(Near(drained.solid, 38 / 55.0f) && Near(drained.lost, 3 / 55.0f) && drained.raised == 0);
  HeroStatsAttributeBar raised = HeroStatsLive_AttributeBar(55, 52, 55);
  CHECK(Near(raised.solid, 52 / 55.0f) && raised.lost == 0 && Near(raised.raised, 3 / 55.0f));
  HeroStatsAttributeBar none = HeroStatsLive_AttributeBar(0, 0, 1);
  CHECK(none.solid == 0 && none.lost == 0 && none.raised == 0);
  // A track too short never overflows; a now below 0 is 0; a track of nothing is no division by zero
  HeroStatsAttributeBar over = HeroStatsLive_AttributeBar(90, 40, 50);
  CHECK(Near(over.solid, 40 / 50.0f) && Near(over.raised, 10 / 50.0f));
  HeroStatsAttributeBar beyond = HeroStatsLive_AttributeBar(90, 80, 50);
  CHECK(Near(beyond.solid, 1.0f) && beyond.raised == 0 && beyond.lost == 0);
  HeroStatsAttributeBar below = HeroStatsLive_AttributeBar(-5, 40, 50);
  CHECK(below.solid == 0 && Near(below.lost, 40 / 50.0f));
  HeroStatsAttributeBar nothing = HeroStatsLive_AttributeBar(10, 10, 0);
  CHECK(nothing.solid + nothing.lost + nothing.raised <= 1.0001f);
}

static HeroStatsLiveChampion WithAttribute(HeroStatsChampion who, const char *name, int attribute, int now, int most)
{
  HeroStatsLiveChampion champion = HeroStatsLiveChampion();
  champion.who = who;
  champion.name = name;
  for (int a = 0; a < HERO_LIVE_ATTRIBUTE_COUNT; a++) champion.attributeNow[a] = champion.attributeMax[a] = 0;
  champion.attributeNow[attribute] = now;
  champion.attributeMax[attribute] = most;
  return champion;
}

static void TheBestOfThePartyIsTheTrack(void)
{
  HeroStatsLiveParty party;
  party.champions.push_back(WithAttribute(7, "HALK", 0, 55, 55));
  party.champions.push_back(WithAttribute(9, "SYRA", 0, 38, 41));
  party.champions.push_back(WithAttribute(11, "TIGGY", 0, 60, 55));   // raised above the best max
  CHECK(HeroStatsLive_AttributeTop(party, 0) == 60);
  CHECK(HeroStatsLive_AttributeTop(party, 2) == 1);                    // nobody has any: never 0
  CHECK(HeroStatsLive_AttributeTop(HeroStatsLiveParty(), 0) == 1);
  CHECK(HeroStatsLive_AttributeHelp(party, party.champions[1], 0) == "SYRA: 38 now, 41 max \xC2\xB7 75% of the best (HALK 55)");
  CHECK(HeroStatsLive_AttributeHelp(party, party.champions[0], 0) == "HALK: 55 \xC2\xB7 100% of the best (HALK 55)");
  CHECK(HeroStatsLive_AttributeHelp(party, party.champions[2], 0) == "TIGGY: 60 now, 55 max \xC2\xB7 100% of the best (HALK 55)");
  CHECK(HeroStatsLive_AttributeHelp(party, party.champions[0], 2) == "HALK: 0 \xC2\xB7 0% of the best (HALK 0)");
}

void TestHeroStatsLive(void)
{
  AWeaponSaysItsDamageAndKineticEnergy();
  ArmorSaysItsStrengthAndSharpResistance();
  TheListSortsByDamageKineticAndArmor();
  AnAttributesBarShowsNowAgainstMaxAndTheBest();
  TheBestOfThePartyIsTheTrack();
  TheListSortsWithAChestsContentsUnderIt();
  WeightAndChargesRead();
  AnItemWithoutAnIconHasNoKey();
  TheBodySlotsHaveTheirPlaces();
  AChestCountsWhatFollowsIt();
  ItemsSortByWhatTheyFeed();
  SlotsAreNamedAsTheGameNumbersThem();
  WoundsAttributesAndTenthsRead();
  AChampionIsFoundByFingerprint();
}
