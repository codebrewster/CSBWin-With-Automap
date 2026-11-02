#include "TestSupport.h"
#include "../AutoMapScene.h"
#include "../AutoMapTooltip.h"

#include <cmath>
#include <set>
#include <cstddef>

static const int LEVEL = 0;
static const int LEVER_PICTURE = 148;
static const int FOUNTAIN_PICTURE = 192;
static const int TORCH_PICTURE = 214;
static const int MUMMY_FRONT_PICTURE = 475;
static const int SCREAMER_FRONT_PICTURE = 450;

// A corridor of two floor cells inside walls:
//
//        x=0 x=1 x=2 x=3
//   y=0   #   L   #   #      L: lever on the south side, fountain on the north side
//   y=1   T   .   .   #      T: torch holder on the east side
//   y=2   #   #   W   #      W: writing on the north side
static AutoMapRelay CounterRelay(int value)
{
  AutoMapRelay relay;
  relay.kind = AutoMapRelayCounter;
  relay.value = value;
  return relay;
}

static AutoMapRelay GateRelay(int value, bool local)
{
  AutoMapRelay relay;
  relay.kind = AutoMapRelayGate;
  relay.value = value;
  if (local) relay.localActionType = 0;
  return relay;
}

// The texts of styled tooltip lines, for the checks that read text only
static std::vector<std::string> Texts(const std::vector<AutoMapTipLine> &lines)
{
  std::vector<std::string> texts;
  for (size_t i = 0; i < lines.size(); i++) texts.push_back(lines[i].text);
  return texts;
}

static AutoMapLevelSnapshot Corridor(void)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 4;
  snapshot.height = 3;
  snapshot.cells.assign(12, AutoMapCellRecord());
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellFloor;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellFloor;
  snapshot.cells[0 * 4 + 1].wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  snapshot.cells[0 * 4 + 1].wallSides[AutoMapNorth].picture = FOUNTAIN_PICTURE;
  snapshot.cells[1 * 4 + 0].wallSides[AutoMapEast].picture = TORCH_PICTURE;
  snapshot.cells[2 * 4 + 2].wallSides[AutoMapNorth].hasWriting = true;
  snapshot.party.x = 1;
  snapshot.party.y = 1;
  snapshot.party.facing = AutoMapEast;
  return snapshot;
}

static AutoMapMonsterRecord MummyAt(int x, int y)
{
  AutoMapMonsterRecord monster;
  monster.x = x;
  monster.y = y;
  monster.kind.frontPicture = MUMMY_FRONT_PICTURE;
  monster.kind.hasSidePicture = true;
  monster.kind.hasBackPicture = true;
  monster.kind.transparentColour = 4;
  monster.kind.size = 0;
  monster.memberCount = 1;
  for (int member = 0; member < AUTOMAP_LARGEST_GROUP; member++) monster.memberFacings[member] = AutoMapWest;
  return monster;
}

static AutoMapItem ItemPicture(int picture)
{
  AutoMapItem item;
  item.worldPicture = picture;
  return item;
}

static AutoMapItem NamedItem(int picture, int iconTile, int nameIndex)
{
  AutoMapItem item;
  item.worldPicture = picture;
  item.hasIcon = true;
  item.iconGraphic = 42;
  item.iconTile = iconTile;
  item.nameIndex = nameIndex;
  return item;
}

static AutoMapExplored NothingExplored(void)
{
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(1, AutoMapLevelSize{4, 3}));
  return explored;
}

static AutoMapFog FogOver(const AutoMapExplored &explored, const AutoMapVisibleCells &visibleNow)
{
  AutoMapFog fog = {true, &explored, &visibleNow};
  return fog;
}

static AutoMapFog NoFog(const AutoMapExplored &explored, const AutoMapVisibleCells &visibleNow)
{
  AutoMapFog fog = {false, &explored, &visibleNow};
  return fog;
}

static AutoMapFog NoFogWithExploredHighlighted(const AutoMapExplored &explored,
                                               const AutoMapVisibleCells &visibleNow)
{
  AutoMapFog fog = NoFog(explored, visibleNow);
  fog.highlightsExplored = true;
  return fog;
}

static AutoMapExplored FirstFloorCellAndTheWallNorthOfItExplored(void)
{
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  return explored;
}

static const AutoMapShownCell *FindCell(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.cells.size(); i++)
  {
    if (drawList.cells[i].x == x && drawList.cells[i].y == y) return &drawList.cells[i];
  }
  return NULL;
}

static void MonsterFacingSouthShowsItsFront(void)
{
  AutoMapMonsterKind mummy = {MUMMY_FRONT_PICTURE, true, true, 4};
  AutoMapMonsterPicture picture = AutoMapScene_ChooseMonsterPicture(mummy, AutoMapSouth);
  CHECK(picture.picture == MUMMY_FRONT_PICTURE);
  CHECK(!picture.isMirrored);
}

static void MonsterFacingNorthShowsItsBack(void)
{
  AutoMapMonsterKind mummy = {MUMMY_FRONT_PICTURE, true, true, 4};
  AutoMapMonsterPicture picture = AutoMapScene_ChooseMonsterPicture(mummy, AutoMapNorth);
  CHECK(picture.picture == MUMMY_FRONT_PICTURE + 2);
  CHECK(!picture.isMirrored);
}

static void MonsterFacingEastShowsItsSide(void)
{
  AutoMapMonsterKind mummy = {MUMMY_FRONT_PICTURE, true, true, 4};
  AutoMapMonsterPicture picture = AutoMapScene_ChooseMonsterPicture(mummy, AutoMapEast);
  CHECK(picture.picture == MUMMY_FRONT_PICTURE + 1);
  CHECK(!picture.isMirrored);
}

static void MonsterFacingWestShowsItsSideMirrored(void)
{
  AutoMapMonsterKind mummy = {MUMMY_FRONT_PICTURE, true, true, 4};
  AutoMapMonsterPicture picture = AutoMapScene_ChooseMonsterPicture(mummy, AutoMapWest);
  CHECK(picture.picture == MUMMY_FRONT_PICTURE + 1);
  CHECK(picture.isMirrored);
}

static void BackPictureFollowsTheFrontWhenThereIsNoSidePicture(void)
{
  AutoMapMonsterKind backOnly = {MUMMY_FRONT_PICTURE, false, true, 4};
  CHECK(AutoMapScene_ChooseMonsterPicture(backOnly, AutoMapNorth).picture == MUMMY_FRONT_PICTURE + 1);
  CHECK(AutoMapScene_ChooseMonsterPicture(backOnly, AutoMapEast).picture == MUMMY_FRONT_PICTURE);
  CHECK(!AutoMapScene_ChooseMonsterPicture(backOnly, AutoMapWest).isMirrored);
}

static void MonsterWithoutSideAndBackPicturesAlwaysShowsItsFront(void)
{
  AutoMapMonsterKind screamer = {SCREAMER_FRONT_PICTURE, false, false, 10};
  for (int facing = AutoMapNorth; facing <= AutoMapWest; facing++)
  {
    AutoMapMonsterPicture picture =
        AutoMapScene_ChooseMonsterPicture(screamer, (AutoMapDirection)facing);
    CHECK(picture.picture == SCREAMER_FRONT_PICTURE);
    CHECK(!picture.isMirrored);
  }
}

static void MonsterWithSideButNoBackPictureShowsItsFrontFromBehind(void)
{
  AutoMapMonsterKind sideOnly = {MUMMY_FRONT_PICTURE, true, false, 4};
  CHECK(AutoMapScene_ChooseMonsterPicture(sideOnly, AutoMapNorth).picture == MUMMY_FRONT_PICTURE);
  CHECK(AutoMapScene_ChooseMonsterPicture(sideOnly, AutoMapWest).picture == MUMMY_FRONT_PICTURE + 1);
}

static void WithoutFogTheWholeLevelIsShown(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyAt(2, 1));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  CHECK(drawList.level == LEVEL);
  CHECK(drawList.width == 4);
  CHECK(drawList.height == 3);
  CHECK(drawList.cells.size() == 12);
  CHECK(drawList.monsters.size() == 1);
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].picture == LEVER_PICTURE);
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapNorth].picture == FOUNTAIN_PICTURE);
  CHECK(FindCell(drawList, 2, 2)->cell.wallSides[AutoMapNorth].hasWriting);
}

static void UnderFogOnlyExploredCellsAreShown(void)
{
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(Corridor(), FogOver(explored, visibleNow));
  CHECK(drawList.cells.size() == 2);
  CHECK(FindCell(drawList, 1, 1) != NULL);
  CHECK(FindCell(drawList, 1, 1)->cell.kind == AutoMapCellFloor);
  CHECK(FindCell(drawList, 1, 0) != NULL);
  CHECK(FindCell(drawList, 2, 1) == NULL);
}

// An old save without a .map file starts like this, before the first tick marked the party cell
static void UnderFogWithNothingExploredOnlyThePartyIsShown(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyAt(2, 1));
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(drawList.cells.empty());
  CHECK(drawList.monsters.empty());
  CHECK(drawList.party.x == 1);
  CHECK(drawList.party.y == 1);
  CHECK(drawList.party.facing == AutoMapEast);
}

static void UnderFogAMonsterIsShownOnlyOnACellThePartySeesNow(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyAt(1, 1));
  corridor.monsters.push_back(MummyAt(2, 1));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  visibleNow.Add(1, 1);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(drawList.cells.size() == 2);
  CHECK(drawList.monsters.size() == 1);
  CHECK(drawList.monsters[0].x == 1);
  CHECK(drawList.monsters[0].transparentColour == 4);
  CHECK(drawList.monsters[0].memberPictures[0].picture == MUMMY_FRONT_PICTURE + 1);
  CHECK(drawList.monsters[0].memberPictures[0].isMirrored);
  CHECK(drawList.monsters[0].memberFacings[0] == AutoMapWest);
}

static void EveryMemberOfAGroupGetsThePictureOfItsOwnFacing(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  AutoMapMonsterRecord group = MummyAt(2, 1);
  group.memberCount = 2;
  group.memberFacings[0] = AutoMapSouth;
  group.memberFacings[1] = AutoMapNorth;
  corridor.monsters.push_back(group);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  CHECK(drawList.monsters[0].memberCount == 2);
  CHECK(drawList.monsters[0].memberPictures[0].picture == MUMMY_FRONT_PICTURE);
  CHECK(drawList.monsters[0].memberPictures[1].picture == MUMMY_FRONT_PICTURE + 2);
}

// The party walked the corridor but never stood where it could see the far side of a wall
static void UnderFogAWallSideIsShownOnlyWhenTheFloorInFrontOfItIsExplored(void)
{
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.Mark(AutoMapCell{LEVEL, 0, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 2});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(Corridor(), FogOver(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].picture == LEVER_PICTURE);
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapNorth].picture == AUTOMAP_NO_PICTURE);
  CHECK(FindCell(drawList, 0, 1)->cell.wallSides[AutoMapEast].picture == TORCH_PICTURE);
  CHECK(!FindCell(drawList, 2, 2)->cell.wallSides[AutoMapNorth].hasWriting);
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  drawList = AutoMapScene_BuildDrawList(Corridor(), FogOver(explored, visibleNow));
  CHECK(FindCell(drawList, 2, 2)->cell.wallSides[AutoMapNorth].hasWriting);
}

static void WithoutFogAnExploredFloorCellIsHighlighted(void)
{
  AutoMapExplored explored = FirstFloorCellAndTheWallNorthOfItExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(Corridor(), NoFogWithExploredHighlighted(explored, visibleNow));
  CHECK(drawList.cells.size() == 12);
  CHECK(FindCell(drawList, 1, 1)->isHighlighted);
}

static void WithoutFogAnUnexploredFloorCellIsNotHighlighted(void)
{
  AutoMapExplored explored = FirstFloorCellAndTheWallNorthOfItExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(Corridor(), NoFogWithExploredHighlighted(explored, visibleNow));
  CHECK(!FindCell(drawList, 2, 1)->isHighlighted);
}

static void AnExploredWallIsNotHighlighted(void)
{
  AutoMapExplored explored = FirstFloorCellAndTheWallNorthOfItExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(Corridor(), NoFogWithExploredHighlighted(explored, visibleNow));
  CHECK(!FindCell(drawList, 1, 0)->isHighlighted);
}

static void NothingIsHighlightedUnlessAskedFor(void)
{
  AutoMapExplored explored = FirstFloorCellAndTheWallNorthOfItExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(Corridor(), NoFog(explored, visibleNow));
  CHECK(!FindCell(drawList, 1, 1)->isHighlighted);
}

static void UnderFogNothingIsHighlighted(void)
{
  AutoMapExplored explored = FirstFloorCellAndTheWallNorthOfItExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapFog fog = FogOver(explored, visibleNow);
  fog.highlightsExplored = true;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(Corridor(), fog);
  CHECK(!FindCell(drawList, 1, 1)->isHighlighted);
}

static void UpToThreeItemsAreShownWithoutAMark(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[1 * 4 + 1].itemPictures.assign(3, ItemPicture(360));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.itemPictures.size() == 3);
  CHECK(!FindCell(drawList, 1, 1)->hasMoreItems);
  CHECK(!FindCell(drawList, 2, 1)->hasMoreItems);
}

static void AFourthItemTurnsIntoTheMoreItemsMark(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  std::vector<AutoMapItem> &pile = corridor.cells[1 * 4 + 1].itemPictures;
  pile.push_back(ItemPicture(360));
  pile.push_back(ItemPicture(373));
  pile.push_back(ItemPicture(388));
  pile.push_back(ItemPicture(361));
  pile.push_back(ItemPicture(362));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  const AutoMapShownCell *cell = FindCell(drawList, 1, 1);
  CHECK(cell->hasMoreItems);
  CHECK(cell->cell.itemPictures.size() == 3);
  CHECK(cell->cell.itemPictures[0].worldPicture == 360);
  CHECK(cell->cell.itemPictures[2].worldPicture == 388);
}

static void TooltipKeepsTheFullPileWhenTheCellIsTruncated(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  std::vector<AutoMapItem> &pile = corridor.cells[1 * 4 + 1].itemPictures;
  pile.push_back(ItemPicture(360));
  pile.push_back(ItemPicture(373));
  pile.push_back(ItemPicture(388));
  pile.push_back(ItemPicture(361));
  pile.push_back(ItemPicture(362));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  const AutoMapShownCell *cell = FindCell(drawList, 1, 1);
  CHECK(cell->allItemPictures.size() == 5);
  CHECK(cell->allItemPictures[3].worldPicture == 361);
  CHECK(cell->allItemPictures[4].worldPicture == 362);
  std::vector<AutoMapItemStack> stacks = AutoMapScene_TooltipItems(drawList, 1, 1);
  CHECK(stacks.size() == 5);
}

static void TooltipGroupsIdenticalPicturesInFirstSeenOrder(void)
{
  std::vector<AutoMapItem> pile;
  pile.push_back(ItemPicture(360));
  pile.push_back(ItemPicture(373));
  pile.push_back(ItemPicture(360));
  pile.push_back(ItemPicture(360));
  pile.push_back(ItemPicture(373));
  std::vector<AutoMapItemStack> stacks = AutoMapScene_GroupItems(pile);
  CHECK(stacks.size() == 2);
  CHECK(stacks[0].item.worldPicture == 360);
  CHECK(stacks[0].count == 3);
  CHECK(stacks[1].item.worldPicture == 373);
  CHECK(stacks[1].count == 2);
  CHECK(AutoMapScene_GroupItems(std::vector<AutoMapItem>()).empty());
}

static AutoMapItem ItemOfType(int worldPicture, int descIndex)
{
  AutoMapItem item;
  item.worldPicture = worldPicture;
  item.descIndex = descIndex;
  return item;
}

static void ItemKindFollowsTheObjectTypeRanges(void)
{
  // Object descriptions (data.cpp): scroll 0, chest 1, potions 2..22, weapons from 23, clothing
  // from 69, everything else from 127. The name index has another order and must not decide.
  AutoMapItem scrollNamedLikeAWeapon = ItemOfType(360, 0);
  scrollNamedLikeAWeapon.nameIndex = 30;
  CHECK(AutoMapScene_KindOfItem(scrollNamedLikeAWeapon) == AutoMapItemScroll);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 0)) == AutoMapItemScroll);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 1)) == AutoMapItemChest);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 2)) == AutoMapItemPotion);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 22)) == AutoMapItemPotion);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 23)) == AutoMapItemWeapon);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 68)) == AutoMapItemWeapon);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 69)) == AutoMapItemArmour);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 126)) == AutoMapItemArmour);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, 127)) == AutoMapItemOther);
  CHECK(AutoMapScene_KindOfItem(ItemOfType(360, -1)) == AutoMapItemOther);
}

static void OnlyFlavourFloorPicturesCountAsDecoration(void)
{
  // Floor ornament n draws picture 247 + 6 n, nearest view 5: 1, 4 and 7 are pressure pads
  CHECK(!AutoMapScene_FloorPictureIsFlavour(247 + 6 * 1 + 5));
  CHECK(!AutoMapScene_FloorPictureIsFlavour(247 + 6 * 4 + 5));
  CHECK(!AutoMapScene_FloorPictureIsFlavour(247 + 6 * 7 + 5));
  CHECK(AutoMapScene_FloorPictureIsFlavour(247 + 6 * 0 + 5));   // square grate
  CHECK(AutoMapScene_FloorPictureIsFlavour(247 + 6 * 2 + 5));   // moss
  CHECK(AutoMapScene_FloorPictureIsFlavour(247 + 6 * 6 + 5));   // crack
  CHECK(AutoMapScene_FloorPictureIsFlavour(247 + 6 * 8 + 5));   // puddle
  CHECK(!AutoMapScene_FloorPictureIsFlavour(247 + 6 * 9 + 5));  // unknown: kept
  CHECK(!AutoMapScene_FloorPictureIsFlavour(AUTOMAP_NO_PICTURE));
}

static void FoodAndKeysAreKindsOfTheirOwn(void)
{
  // Misc items (description 127 on) whose name index is food (168..175) or a key (176..191)
  AutoMapItem apple = ItemOfType(360, 156);
  apple.nameIndex = 168;
  AutoMapItem dragonSteak = ItemOfType(360, 163);
  dragonSteak.nameIndex = 175;
  AutoMapItem ironKey = ItemOfType(360, 136);
  ironKey.nameIndex = 176;
  AutoMapItem masterKey = ItemOfType(360, 151);
  masterKey.nameIndex = 191;
  AutoMapItem lockPicks = ItemOfType(360, 176);
  lockPicks.nameIndex = 192;
  CHECK(AutoMapScene_KindOfItem(apple) == AutoMapItemFood);
  CHECK(AutoMapScene_KindOfItem(dragonSteak) == AutoMapItemFood);
  CHECK(AutoMapScene_KindOfItem(ironKey) == AutoMapItemKey);
  CHECK(AutoMapScene_KindOfItem(masterKey) == AutoMapItemKey);
  CHECK(AutoMapScene_KindOfItem(lockPicks) == AutoMapItemOther);
  // Only misc items: a weapon never turns into food whatever its name index
  AutoMapItem weapon = ItemOfType(360, 30);
  weapon.nameIndex = 170;
  CHECK(AutoMapScene_KindOfItem(weapon) == AutoMapItemWeapon);
}

static void ItemsOfKindsKeepsThePileOrder(void)
{
  std::vector<AutoMapItem> pile;
  pile.push_back(ItemOfType(360, 30));   // weapon
  pile.push_back(ItemOfType(361, 80));   // armour
  pile.push_back(ItemOfType(362, 150));  // other
  pile.push_back(ItemOfType(363, 40));   // weapon
  unsigned noWeapons = AUTOMAP_ALL_ITEM_KINDS & ~(1u << AutoMapItemWeapon);
  std::vector<AutoMapItem> shown = AutoMapScene_ItemsOfKinds(pile, noWeapons);
  CHECK(shown.size() == 2);
  CHECK(shown[0].worldPicture == 361);
  CHECK(shown[1].worldPicture == 362);
  CHECK(AutoMapScene_ItemsOfKinds(pile, AUTOMAP_ALL_ITEM_KINDS).size() == 4);
  CHECK(AutoMapScene_ItemsOfKinds(pile, 0).empty());
}

static void TooltipKeepsApartItemsSharingTheirWorldPicture(void)
{
  // A skeleton key and a Ra key draw the same world sprite but carry different
  // inventory tiles and names: they must never group together.
  std::vector<AutoMapItem> pile;
  pile.push_back(NamedItem(360, 5, 10));
  pile.push_back(NamedItem(360, 7, 11));
  pile.push_back(NamedItem(360, 5, 10));
  std::vector<AutoMapItemStack> stacks = AutoMapScene_GroupItems(pile);
  CHECK(stacks.size() == 2);
  CHECK(stacks[0].item.nameIndex == 10);
  CHECK(stacks[0].count == 2);
  CHECK(stacks[1].item.nameIndex == 11);
  CHECK(stacks[1].count == 1);
}

static void TooltipSlotsKeepIconAndName(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[1 * 4 + 1].itemPictures.push_back(NamedItem(360, 5, 10));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 1);
  CHECK(slots[0].item.worldPicture == 360);
  CHECK(slots[0].item.hasIcon);
  CHECK(slots[0].item.iconGraphic == 42);
  CHECK(slots[0].item.iconTile == 5);
  CHECK(slots[0].item.nameIndex == 10);
}

static void TooltipOnUnexploredCellIsEmpty(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[1 * 4 + 2].itemPictures.assign(2, ItemPicture(360));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(FindCell(drawList, 2, 1) == NULL);
  CHECK(AutoMapScene_TooltipItems(drawList, 2, 1).empty());
}

static void TooltipOnFoggedVisitedCellShowsItsItems(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[1 * 4 + 1].itemPictures.push_back(ItemPicture(360));
  corridor.cells[1 * 4 + 1].itemPictures.push_back(ItemPicture(360));
  corridor.cells[1 * 4 + 1].itemPictures.push_back(ItemPicture(373));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  // Not in visibleNow: fogged, but visited. Items persist, so the tooltip persists.
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1) != NULL);
  std::vector<AutoMapItemStack> stacks = AutoMapScene_TooltipItems(drawList, 1, 1);
  CHECK(stacks.size() == 2);
  CHECK(stacks[0].item.worldPicture == 360);
  CHECK(stacks[0].count == 2);
  CHECK(stacks[1].item.worldPicture == 373);
  CHECK(stacks[1].count == 1);
}

static void TooltipKeepsEachChestInItsOwnSlot(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  std::vector<AutoMapItem> &pile = corridor.cells[1 * 4 + 1].itemPictures;
  pile.push_back(ItemPicture(100));
  pile.push_back(ItemPicture(200));
  pile.push_back(ItemPicture(100));
  AutoMapChestContents first;
  first.item = 0;
  first.pictures.push_back(ItemPicture(300));
  first.pictures.push_back(ItemPicture(300));
  first.pictures.push_back(ItemPicture(301));
  corridor.cells[1 * 4 + 1].chestContents.push_back(first);
  AutoMapChestContents second;
  second.item = 2;
  second.pictures.push_back(ItemPicture(302));
  corridor.cells[1 * 4 + 1].chestContents.push_back(second);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 3);
  CHECK(slots[0].isChest);
  CHECK(slots[0].item.worldPicture == 100);
  CHECK(slots[0].count == 1);
  CHECK(slots[0].contents.size() == 2);
  CHECK(slots[0].contents[0].item.worldPicture == 300);
  CHECK(slots[0].contents[0].count == 2);
  CHECK(slots[0].contents[1].item.worldPicture == 301);
  CHECK(slots[0].contents[1].count == 1);
  CHECK(!slots[1].isChest);
  CHECK(slots[1].item.worldPicture == 200);
  CHECK(slots[1].count == 1);
  // Same chest picture, different contents: never grouped.
  CHECK(slots[2].isChest);
  CHECK(slots[2].item.worldPicture == 100);
  CHECK(slots[2].contents.size() == 1);
  CHECK(slots[2].contents[0].item.worldPicture == 302);
}

static void TooltipShowsChestPastTheDisplayTruncation(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  std::vector<AutoMapItem> &pile = corridor.cells[1 * 4 + 1].itemPictures;
  pile.push_back(ItemPicture(360));
  pile.push_back(ItemPicture(373));
  pile.push_back(ItemPicture(388));
  pile.push_back(ItemPicture(361));
  pile.push_back(ItemPicture(362));
  AutoMapChestContents chest;
  chest.item = 4;
  chest.pictures.push_back(ItemPicture(300));
  corridor.cells[1 * 4 + 1].chestContents.push_back(chest);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.itemPictures.size() == 3);
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 5);
  CHECK(slots[4].isChest);
  CHECK(slots[4].item.worldPicture == 362);
  CHECK(slots[4].contents.size() == 1);
  CHECK(slots[4].contents[0].item.worldPicture == 300);
}

static void TooltipOnUnexploredChestCellIsEmpty(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[1 * 4 + 2].itemPictures.push_back(ItemPicture(100));
  AutoMapChestContents chest;
  chest.item = 0;
  chest.pictures.push_back(ItemPicture(300));
  corridor.cells[1 * 4 + 2].chestContents.push_back(chest);
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_TooltipSlots(drawList, 2, 1).empty());
}

static AutoMapMonsterRecord MummyWithGearAt(int x, int y)
{
  AutoMapMonsterRecord monster = MummyAt(x, y);
  monster.possessionPictures.push_back(ItemPicture(300));
  monster.possessionPictures.push_back(ItemPicture(300));
  monster.possessionPictures.push_back(ItemPicture(301));
  return monster;
}

static void TooltipShowsMonsterPossessionsNested(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyWithGearAt(1, 1));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  visibleNow.Add(1, 1);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(drawList.monsters.size() == 1);
  CHECK(drawList.monsters[0].possessionPictures.size() == 3);
  std::vector<AutoMapItemStack> gear = AutoMapScene_MonsterPossessions(drawList, 1, 1);
  CHECK(gear.size() == 2);
  CHECK(gear[0].item.worldPicture == 300);
  CHECK(gear[0].count == 2);
  CHECK(gear[1].item.worldPicture == 301);
  CHECK(gear[1].count == 1);
}

static void PossessionsHideWithTheirMonster(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyWithGearAt(1, 1));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  // Fogged and out of sight: the monster goes, and its gear goes with it.
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(drawList.monsters.empty());
  CHECK(AutoMapScene_MonsterPossessions(drawList, 1, 1).empty());
  CHECK(FindCell(drawList, 1, 1) != NULL);
}

static AutoMapMonsterRecord MummyWithDropsAt(int x, int y)
{
  AutoMapMonsterRecord monster = MummyAt(x, y);
  monster.residuePictures.push_back(ItemPicture(300));
  monster.residuePictures.push_back(ItemPicture(300));
  monster.chanceResiduePictures.push_back(ItemPicture(301));
  return monster;
}

static void TooltipShowsMonsterDropsByTier(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyWithDropsAt(1, 1));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  visibleNow.Add(1, 1);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(drawList.monsters.size() == 1);
  CHECK(drawList.monsters[0].residuePictures.size() == 2);
  CHECK(drawList.monsters[0].chanceResiduePictures.size() == 1);
  AutoMapMonsterDrops drops = AutoMapScene_MonsterDrops(drawList, 1, 1);
  CHECK(drops.certain.size() == 1);
  CHECK(drops.certain[0].item.worldPicture == 300);
  CHECK(drops.certain[0].count == 2);
  CHECK(drops.chance.size() == 1);
  CHECK(drops.chance[0].item.worldPicture == 301);
  CHECK(drops.chance[0].count == 1);
}

static void DropsHideWithTheirMonster(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyWithDropsAt(1, 1));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  AutoMapMonsterDrops drops = AutoMapScene_MonsterDrops(drawList, 1, 1);
  CHECK(drops.certain.empty());
  CHECK(drops.chance.empty());
}

static AutoMapMonsterRecord MummyWithHealthAt(int x, int y)
{
  AutoMapMonsterRecord monster = MummyAt(x, y);
  monster.memberCount = 2;
  monster.record = 0x40002;
  monster.memberHealth.push_back(42);
  monster.memberHealth.push_back(35);
  return monster;
}

static void TooltipShowsMonsterHealthPerMember(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyWithHealthAt(1, 1));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  visibleNow.Add(1, 1);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(drawList.monsters.size() == 1);
  CHECK(drawList.monsters[0].memberHealth.size() == 2);
  std::vector<std::string> lines = AutoMapScene_MonsterHealthLines(drawList, 1, 1);
  CHECK(lines.size() == 2);
  CHECK(lines[0] == "HP 42");
  CHECK(lines[1] == "HP 35");
}

static void HealthHidesWithItsMonster(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.monsters.push_back(MummyWithHealthAt(1, 1));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  // Fogged and out of sight: the monster goes, and its health goes with it.
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(drawList.monsters.empty());
  CHECK(AutoMapScene_MonsterHealthLines(drawList, 1, 1).empty());
}

static AutoMapViewRequest RequestFor(int windowWidth, int windowHeight)
{
  AutoMapViewRequest request;
  request.levelSize = AutoMapLevelSize{32, 20};
  request.windowWidth = windowWidth;
  request.windowHeight = windowHeight;
  request.zoomSteps = 0;
  request.centerX = 0;
  request.centerY = 0;
  return request;
}

static void WholeLevelIsFittedAndCentred(void)
{
  AutoMapView view = AutoMapScene_PlaceLevel(RequestFor(700, 500));
  CHECK(view.cellSize == 21);
  CHECK(view.originX == (700 - 32 * 21) / 2);
  CHECK(view.originY == (500 - 20 * 21) / 2);
}

// A minimised window reports no size, and a window can be dragged smaller than the level
static void WindowWithoutRoomShowsNothing(void)
{
  CHECK(AutoMapScene_PlaceLevel(RequestFor(0, 0)).cellSize == 0);
  CHECK(AutoMapScene_PlaceLevel(RequestFor(-5, 300)).cellSize == 0);
  CHECK(AutoMapScene_PlaceLevel(RequestFor(31, 300)).cellSize == 0);
  AutoMapViewRequest emptyLevel = RequestFor(700, 500);
  emptyLevel.levelSize = AutoMapLevelSize{0, 0};
  CHECK(AutoMapScene_PlaceLevel(emptyLevel).cellSize == 0);
}

static void ZoomStepsStayWithinTheirRange(void)
{
  AutoMapViewRequest request = RequestFor(700, 500);
  int most = AutoMapScene_MostZoomSteps(request);
  CHECK(AutoMapScene_ClampZoomSteps(-3, request) == 0);
  CHECK(AutoMapScene_ClampZoomSteps(5, request) == 5);
  CHECK(AutoMapScene_ClampZoomSteps(1000, request) == most);
  request.zoomSteps = 1000;
  int deepest = AutoMapScene_PlaceLevel(request).cellSize;
  CHECK(deepest >= 500 / 3 && deepest < 500 / 3 * 115 / 100 + 2);
  request.zoomSteps = -1000;
  CHECK(AutoMapScene_PlaceLevel(request).cellSize == 21);
}

// Zooming in stops when about three cells fill the shorter side of the view
static void ZoomReachesAThirdOfTheView(void)
{
  AutoMapViewRequest request;
  request.levelSize = AutoMapLevelSize{32, 32};
  request.windowWidth = 900;
  request.windowHeight = 900;
  request.centerX = 16;
  request.centerY = 16;
  int most = AutoMapScene_MostZoomSteps(request);
  request.zoomSteps = most;
  CHECK(AutoMapScene_PlaceLevel(request).cellSize >= 300);
  request.zoomSteps = most - 1;
  CHECK(AutoMapScene_PlaceLevel(request).cellSize < 300);
  // A small window still zooms to cells of at least 96 pixels
  request.windowWidth = 200;
  request.windowHeight = 150;
  request.zoomSteps = AutoMapScene_MostZoomSteps(request);
  CHECK(AutoMapScene_PlaceLevel(request).cellSize >= 96);
}

// Step zero is the whole level, fitted and centred; every step after it grows the cells
static void ZoomStepZeroIsTheWholeLevel(void)
{
  AutoMapViewRequest request = RequestFor(640, 400);
  CHECK(AutoMapScene_PlaceLevel(request).cellSize == 20);
  CHECK(AutoMapScene_ShowsWholeLevel(request));
  int previous = 20;
  for (int step = 1; step <= AutoMapScene_MostZoomSteps(request); step++)
  {
    request.zoomSteps = step;
    int size = AutoMapScene_PlaceLevel(request).cellSize;
    CHECK(size > previous);
    previous = size;
  }
}

static void ZoomedInTheViewIsCentredOnTheParty(void)
{
  AutoMapViewRequest request = RequestFor(640, 400);
  request.zoomSteps = 5;
  request.centerX = 16;
  request.centerY = 10;
  AutoMapView view = AutoMapScene_PlaceLevel(request);
  CHECK(view.cellSize == 40);
  CHECK(view.originX + 16 * 40 + 20 == 320);
  CHECK(view.originY + 10 * 40 + 20 == 200);
}

static void ZoomedInTheViewStopsAtTheEdgesOfTheLevel(void)
{
  AutoMapViewRequest request = RequestFor(640, 400);
  request.zoomSteps = 5;
  AutoMapView nearTopLeft = AutoMapScene_PlaceLevel(request);
  CHECK(nearTopLeft.originX == 0);
  CHECK(nearTopLeft.originY == 0);
  request.centerX = 31;
  request.centerY = 19;
  AutoMapView nearBottomRight = AutoMapScene_PlaceLevel(request);
  CHECK(nearBottomRight.originX == 640 - 32 * 40);
  CHECK(nearBottomRight.originY == 400 - 20 * 40);
}

static void AFractionalCentreMovesTheZoomedView(void)
{
  AutoMapViewRequest request = RequestFor(640, 400);
  request.zoomSteps = 5;
  request.centerX = 16.5f;
  request.centerY = 10.25f;
  AutoMapView view = AutoMapScene_PlaceLevel(request);
  CHECK(view.cellSize == 40);
  CHECK(view.originX == 320 - (16 * 40 + 40));
  CHECK(view.originY == 200 - (10 * 40 + 30));
}

static void FarCentresStopAtTheEdgesOfTheLevel(void)
{
  AutoMapViewRequest request = RequestFor(640, 400);
  request.zoomSteps = 5;
  request.centerX = -100.0f;
  request.centerY = -100.0f;
  AutoMapView pastTopLeft = AutoMapScene_PlaceLevel(request);
  CHECK(pastTopLeft.originX == 0);
  CHECK(pastTopLeft.originY == 0);
  request.centerX = 100.0f;
  request.centerY = 100.0f;
  AutoMapView pastBottomRight = AutoMapScene_PlaceLevel(request);
  CHECK(pastBottomRight.originX == 640 - 32 * 40);
  CHECK(pastBottomRight.originY == 400 - 20 * 40);
}

static void ZoomKeepsTheSameSpotCentred(void)
{
  AutoMapViewRequest request = RequestFor(640, 400);
  request.centerX = 16.5f;
  request.centerY = 10.5f;
  request.zoomSteps = 5;
  AutoMapView closer = AutoMapScene_PlaceLevel(request);
  request.zoomSteps = 8;
  AutoMapView closerStill = AutoMapScene_PlaceLevel(request);
  CHECK(closer.originX + 16.5f * closer.cellSize + closer.cellSize / 2.0f == 320.0f);
  CHECK(closer.originY + 10.5f * closer.cellSize + closer.cellSize / 2.0f == 200.0f);
  CHECK(closerStill.originX + 16.5f * closerStill.cellSize + closerStill.cellSize / 2.0f == 320.0f);
  CHECK(closerStill.originY + 10.5f * closerStill.cellSize + closerStill.cellSize / 2.0f == 200.0f);
}

// The wheel zooms towards the pointer: the level point under it stays on the same pixel
static void ZoomKeepsThePointUnderThePointerStill(void)
{
  AutoMapViewRequest request = RequestFor(640, 400);
  request.zoomSteps = 5;
  request.centerX = 16;
  request.centerY = 10;
  AutoMapView before = AutoMapScene_PlaceLevel(request);
  int pointerX = 400, pointerY = 260;
  float pointX = (float)(pointerX - before.originX) / before.cellSize;
  float pointY = (float)(pointerY - before.originY) / before.cellSize;
  request.zoomSteps = 8;
  int newCellSize = AutoMapScene_PlaceLevel(request).cellSize;
  request.centerX = AutoMapScene_CentreKeepingPointStill(16, pointX, before.cellSize, newCellSize);
  request.centerY = AutoMapScene_CentreKeepingPointStill(10, pointY, before.cellSize, newCellSize);
  AutoMapView after = AutoMapScene_PlaceLevel(request);
  CHECK(after.cellSize == 61);
  CHECK(std::fabs(after.originX + pointX * after.cellSize - pointerX) <= 1.0f);
  CHECK(std::fabs(after.originY + pointY * after.cellSize - pointerY) <= 1.0f);
  // Back out again: the same point returns under the pointer
  request.zoomSteps = 5;
  request.centerX = AutoMapScene_CentreKeepingPointStill(request.centerX, pointX, 61, 40);
  request.centerY = AutoMapScene_CentreKeepingPointStill(request.centerY, pointY, 61, 40);
  AutoMapView back = AutoMapScene_PlaceLevel(request);
  CHECK(std::fabs(back.originX + pointX * back.cellSize - pointerX) <= 1.0f);
  CHECK(std::fabs(back.originY + pointY * back.cellSize - pointerY) <= 1.0f);
  // A pointer in the middle of the view zooms around the centre as before
  CHECK(AutoMapScene_CentreKeepingPointStill(16, 16.5f, 40, 60) == 16.0f);
  CHECK(AutoMapScene_CentreKeepingPointStill(16, 3.0f, 0, 60) == 16.0f);
}

static void OnlyTheFittingZoomShowsTheWholeLevel(void)
{
  AutoMapViewRequest request = RequestFor(640, 400);
  CHECK(AutoMapScene_ShowsWholeLevel(request));
  request.zoomSteps = 1;
  CHECK(!AutoMapScene_ShowsWholeLevel(request));
  CHECK(AutoMapScene_ShowsWholeLevel(RequestFor(0, 0)));
}

static void LabelStepKeepsNumbersApart(void)
{
  CHECK(AutoMapScene_LabelStep(40, 24) == 1);
  CHECK(AutoMapScene_LabelStep(21, 24) == 2);
  CHECK(AutoMapScene_LabelStep(10, 10) == 1);
  CHECK(AutoMapScene_LabelStep(10, 11) == 2);
  CHECK(AutoMapScene_LabelStep(0, 24) == 1);
}

static void VisibleRangeCoversTheCellsTouchingTheMap(void)
{
  int first = 0, last = -1;
  // The level starts 14 pixels into the map: the first cell still touches it
  AutoMapScene_VisibleRange(14, 21, 672, 32, &first, &last);
  CHECK(first == 0);
  CHECK(last == 31);
  // Zoomed and panned 100 pixels past the edge
  AutoMapScene_VisibleRange(-100, 40, 580, 32, &first, &last);
  CHECK(first == 2);
  CHECK(last == 16);
  // Nothing to show
  AutoMapScene_VisibleRange(0, 0, 580, 32, &first, &last);
  CHECK(last < first);
}

static void DigitsAreThreeByFive(void)
{
  CHECK(AutoMapScene_DigitPixel(0, 0, 0));
  CHECK(AutoMapScene_DigitPixel(0, 0, 1));
  CHECK(AutoMapScene_DigitPixel(0, 0, 2));
  CHECK(!AutoMapScene_DigitPixel(1, 0, 0));
  CHECK(AutoMapScene_DigitPixel(1, 0, 1));
  CHECK(!AutoMapScene_DigitPixel(1, 0, 2));
  CHECK(!AutoMapScene_DigitPixel(10, 0, 0));
  CHECK(!AutoMapScene_DigitPixel(0, 5, 0));
  CHECK(!AutoMapScene_DigitPixel(0, 0, 3));
}

static void FontMatchesTheDigits(void)
{
  CHECK(!AutoMapScene_FontPixel('A', 0, 0));
  CHECK(AutoMapScene_FontPixel('A', 0, 1));
  CHECK(!AutoMapScene_FontPixel('A', 0, 2));
  CHECK(AutoMapScene_FontPixel('A', 2, 0));
  CHECK(AutoMapScene_FontPixel('A', 2, 1));
  CHECK(AutoMapScene_FontPixel('A', 2, 2));
  // Either case, and digits agree with the ruler font.
  CHECK(!AutoMapScene_FontPixel('a', 0, 0));
  CHECK(AutoMapScene_FontPixel('a', 0, 1));
  CHECK(AutoMapScene_FontPixel('0', 0, 0) == AutoMapScene_DigitPixel(0, 0, 0));
  CHECK(AutoMapScene_FontPixel('1', 0, 0) == AutoMapScene_DigitPixel(1, 0, 0));
  CHECK(!AutoMapScene_FontPixel(' ', 0, 0));
  CHECK(!AutoMapScene_FontPixel(' ', 4, 2));
  CHECK(AutoMapScene_FontPixel('?', 0, 0));
  CHECK(!AutoMapScene_FontPixel('~', 0, 0));
  CHECK(!AutoMapScene_FontPixel('A', 5, 0));
  CHECK(!AutoMapScene_FontPixel('A', 0, 3));
  CHECK(AutoMapScene_TextWidth("") == 0);
  CHECK(AutoMapScene_TextWidth("A") == 3);
  CHECK(AutoMapScene_TextWidth("AB") == 7);
  CHECK(AutoMapScene_TextWidth("PARTY") == 19);
}

static void GlobalOffsetsReachTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.offsetX = 12;
  snapshot.offsetY = 5;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(drawList.offsetX == 12);
  CHECK(drawList.offsetY == 5);
}

static void GlobalPositionAddsTheLevelOffset(void)
{
  AutoMapDrawList drawList;
  drawList.offsetX = 12;
  drawList.offsetY = 5;
  int gx = -1, gy = -1;
  AutoMapScene_GlobalPosition(drawList, 1, 2, &gx, &gy);
  CHECK(gx == 13);
  CHECK(gy == 7);
  AutoMapScene_GlobalPosition(drawList, 0, 0, &gx, &gy);
  CHECK(gx == 12);
  CHECK(gy == 5);
}

static AutoMapLevelSnapshot CorridorWithMissileAndCloud(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapMissileRecord missile;
  missile.x = 2;
  missile.y = 1;
  missile.item = ItemPicture(360);
  missile.rangeRemaining = 150;
  missile.sizeShowsRange = true;
  snapshot.missiles.push_back(missile);
  AutoMapCloudRecord cloud = {1, 1, 351};
  snapshot.clouds.push_back(cloud);
  return snapshot;
}

static void WithoutFogMissilesAndCloudsAreShown(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithMissileAndCloud(), NoFog(explored, visibleNow));
  CHECK(drawList.missiles.size() == 1);
  CHECK(drawList.missiles[0].x == 2);
  CHECK(drawList.missiles[0].item.worldPicture == 360);
  CHECK(drawList.missiles[0].rangeRemaining == 150);
  CHECK(drawList.missiles[0].sizeShowsRange);
  CHECK(drawList.clouds.size() == 1);
  CHECK(drawList.clouds[0].x == 1);
  CHECK(drawList.clouds[0].picture == 351);
}

static void UnderFogMissilesAndCloudsShowOnlyWhereThePartySees(void)
{
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  visibleNow.Add(1, 1);
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithMissileAndCloud(), FogOver(explored, visibleNow));
  CHECK(drawList.missiles.empty());
  CHECK(drawList.clouds.size() == 1);
  visibleNow.Add(2, 1);
  drawList =
      AutoMapScene_BuildDrawList(CorridorWithMissileAndCloud(), FogOver(explored, visibleNow));
  CHECK(drawList.missiles.size() == 1);
  CHECK(drawList.clouds.size() == 1);
}

static void AFalseWallIsNotHighlighted(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[1 * 4 + 1].kind = AutoMapCellFalseWall;
  corridor.cells[1 * 4 + 2].kind = AutoMapCellOpenFalseWall;
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(corridor, NoFogWithExploredHighlighted(explored, visibleNow));
  CHECK(!FindCell(drawList, 1, 1)->isHighlighted);
  CHECK(FindCell(drawList, 2, 1)->isHighlighted);
}

static void HiddenCellKindsSurviveTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellFalseWall;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellOpenFalseWall;
  snapshot.cells[0 * 4 + 1].kind = AutoMapCellInvisibleTeleporter;
  snapshot.cells[0 * 4 + 2].kind = AutoMapCellSpinner;
  snapshot.cells[0 * 4 + 2].spinnerFacing = AutoMapSouth;
  snapshot.cells[0 * 4 + 3].kind = AutoMapCellDormantTeleporter;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.kind == AutoMapCellFalseWall);
  CHECK(FindCell(drawList, 2, 1)->cell.kind == AutoMapCellOpenFalseWall);
  CHECK(FindCell(drawList, 1, 0)->cell.kind == AutoMapCellInvisibleTeleporter);
  CHECK(FindCell(drawList, 2, 0)->cell.kind == AutoMapCellSpinner);
  CHECK(FindCell(drawList, 2, 0)->cell.spinnerFacing == AutoMapSouth);
  CHECK(FindCell(drawList, 3, 0)->cell.kind == AutoMapCellDormantTeleporter);
}

static void HiddenCellsFollowTheFog(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellInvisibleTeleporter;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellSpinner;
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.kind == AutoMapCellInvisibleTeleporter);
  CHECK(FindCell(drawList, 2, 1) == NULL);
}

static AutoMapLevelSnapshot CorridorWithActuators(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator pad = {1, 1, AutoMapActuatorPad};
  AutoMapFloorActuator generator = {2, 1, AutoMapActuatorGenerator};
  AutoMapFloorActuator cog = {1, 0, AutoMapActuatorCog};
  AutoMapFloorActuator dormant = {2, 0, AutoMapActuatorDormantPad};
  snapshot.actuators.push_back(pad);
  snapshot.actuators.push_back(generator);
  snapshot.actuators.push_back(cog);
  snapshot.actuators.push_back(dormant);
  AutoMapCell link = {LEVEL, 1, 0};
  snapshot.linkedCells.push_back(link);
  return snapshot;
}

static void WithoutFogActuatorsAndLinksAreShown(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithActuators(), NoFog(explored, visibleNow));
  CHECK(drawList.actuators.size() == 4);
  CHECK(drawList.actuators[0].kind == AutoMapActuatorPad);
  CHECK(drawList.actuators[1].kind == AutoMapActuatorGenerator);
  CHECK(drawList.linkedCells.size() == 1);
  CHECK(drawList.linkedCells[0].x == 1);
  CHECK(drawList.linkedCells[0].y == 0);
}

// An actuator does not move: it stays on its explored cell when the party looks away
static void UnderFogActuatorsShowOnlyOnExploredCells(void)
{
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithActuators(), FogOver(explored, visibleNow));
  CHECK(drawList.actuators.size() == 1);
  CHECK(drawList.actuators[0].kind == AutoMapActuatorPad);
  CHECK(drawList.linkedCells.size() == 1);
}

static AutoMapLevelSnapshot CorridorWithLinks(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator pad = {1, 1, AutoMapActuatorPad};
  AutoMapFloorActuator generator = {2, 1, AutoMapActuatorGenerator};
  snapshot.actuators.push_back(pad);
  snapshot.actuators.push_back(generator);
  AutoMapCell link = {LEVEL, 1, 0};
  snapshot.linkedCells.push_back(link);
  AutoMapLink floorEdge = {1, 1, 1, 0, false, -1};
  AutoMapLink wallEdge = {1, 0, 2, 1, true, AutoMapSouth};
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellFloor;
  snapshot.links.push_back(floorEdge);
  snapshot.links.push_back(wallEdge);
  return snapshot;
}

static void LinksSurviveWithoutFog(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithLinks(), NoFog(explored, visibleNow));
  CHECK(drawList.links.size() == 2);
  CHECK(AutoMapScene_LinksFrom(drawList, 1, 1).size() == 1);
  CHECK(AutoMapScene_LinksFrom(drawList, 1, 1)[0].dstX == 1);
  CHECK(AutoMapScene_LinksFrom(drawList, 1, 1)[0].dstY == 0);
  CHECK(AutoMapScene_LinksTo(drawList, 1, 0).size() == 1);
  CHECK(AutoMapScene_LinksTo(drawList, 2, 1).size() == 1);
  CHECK(AutoMapScene_LinksTo(drawList, 1, 1).empty());
}

static void LinkHiddenWhenTargetUnexplored(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator pad = {1, 1, AutoMapActuatorPad};
  snapshot.actuators.push_back(pad);
  AutoMapLink edge = {1, 1, 2, 1, false, -1};
  snapshot.links.push_back(edge);
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  visibleNow.Add(1, 1);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(drawList.links.empty());
  CHECK(AutoMapScene_LinksFrom(drawList, 1, 1).empty());
}

static void LinkHiddenWhenSourceUnexplored(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator pad = {1, 1, AutoMapActuatorPad};
  snapshot.actuators.push_back(pad);
  AutoMapLink edge = {1, 1, 2, 1, false, -1};
  snapshot.links.push_back(edge);
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  // The target explored and in sight, but the pad's cell never explored
  AutoMapVisibleCells visibleNow;
  visibleNow.Add(2, 1);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(drawList.links.empty());
}

static void ReverseLookupFindsAllSources(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator first = {1, 1, AutoMapActuatorPad};
  AutoMapFloorActuator second = {2, 1, AutoMapActuatorCog};
  snapshot.actuators.push_back(first);
  snapshot.actuators.push_back(second);
  AutoMapLink firstEdge = {1, 1, 1, 0, false, -1};
  AutoMapLink secondEdge = {2, 1, 1, 0, false, -1};
  snapshot.links.push_back(firstEdge);
  snapshot.links.push_back(secondEdge);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(AutoMapScene_LinksTo(drawList, 1, 0).size() == 2);
  CHECK(AutoMapScene_LinksFrom(drawList, 2, 1).size() == 1);
}

static void WallSideSurvivesTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].wallSides[AutoMapSouth].functional = true;
  AutoMapLink edge = {1, 0, 2, 1, true, AutoMapSouth};
  snapshot.links.push_back(edge);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList noFog = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(noFog.links.size() == 1);
  CHECK(noFog.links[0].srcIsWall);
  CHECK(noFog.links[0].srcSide == AutoMapSouth);
  // Under fog the south side of (1,0) is only seen from the floor at (1,1)
  AutoMapExplored seen = NothingExplored();
  seen.Mark(AutoMapCell{LEVEL, 1, 0});
  AutoMapDrawList fogged = AutoMapScene_BuildDrawList(snapshot, FogOver(seen, visibleNow));
  CHECK(fogged.links.empty());
}

static void PadOperatorsSurviveTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator pad(1, 1, AutoMapActuatorPad);
  pad.op = AutoMapPadObject;
  pad.object = ItemPicture(360);
  AutoMapFloorActuator directed(2, 1, AutoMapActuatorPad);
  directed.op = AutoMapPadPartyFacing;
  directed.requiredFacing = AutoMapEast;
  AutoMapFloorActuator plain(1, 0, AutoMapActuatorCog);
  snapshot.actuators.push_back(pad);
  snapshot.actuators.push_back(directed);
  snapshot.actuators.push_back(plain);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(drawList.actuators.size() == 3);
  CHECK(drawList.actuators[0].op == AutoMapPadObject);
  CHECK(drawList.actuators[0].object.worldPicture == 360);
  CHECK(drawList.actuators[1].op == AutoMapPadPartyFacing);
  CHECK(drawList.actuators[1].requiredFacing == AutoMapEast);
  CHECK(drawList.actuators[2].op == AutoMapPadAnything);
  CHECK(drawList.actuators[2].object.worldPicture == AUTOMAP_NO_PICTURE);
  CHECK(drawList.actuators[2].requiredFacing == -1);
}

static void PadOperatorsFollowTheFog(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator pad(1, 1, AutoMapActuatorPad);
  pad.op = AutoMapPadPossession;
  pad.object = ItemPicture(360);
  snapshot.actuators.push_back(pad);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells nothingInSight;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(snapshot, FogOver(explored, nothingInSight));
  CHECK(drawList.actuators.empty());
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, nothingInSight));
  CHECK(drawList.actuators.size() == 1);
  CHECK(drawList.actuators[0].op == AutoMapPadPossession);
  CHECK(drawList.actuators[0].object.worldPicture == 360);
}

static void LinkSignalsSurviveTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator pad = {1, 1, AutoMapActuatorPad};
  snapshot.actuators.push_back(pad);
  AutoMapLink toggle = {1, 1, 1, 0, false, -1};
  toggle.signal = AutoMapSignalToggle;
  AutoMapLink once = {1, 1, 2, 1, false, -1};
  once.signal = AutoMapSignalClear;
  once.onceOnly = true;
  snapshot.links.push_back(toggle);
  snapshot.links.push_back(once);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(drawList.links.size() == 2);
  CHECK(AutoMapScene_LinksFrom(drawList, 1, 1).size() == 2);
  CHECK(drawList.links[0].signal == AutoMapSignalToggle);
  CHECK(!drawList.links[0].onceOnly);
  CHECK(drawList.links[1].signal == AutoMapSignalClear);
  CHECK(drawList.links[1].onceOnly);
}

static void DoorPreviewFollowsTheSignal(void)
{
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorClosed, AutoMapSignalSet) == AutoMapDoorOpen);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorHalfOpen, AutoMapSignalSet) == AutoMapDoorOpen);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorOpen, AutoMapSignalClear) == AutoMapDoorClosed);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorHalfOpen, AutoMapSignalClear)
        == AutoMapDoorClosed);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorOpen, AutoMapSignalToggle) == AutoMapDoorClosed);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorClosed, AutoMapSignalToggle) == AutoMapDoorOpen);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorHalfOpen, AutoMapSignalToggle)
        == AutoMapDoorOpen);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorClosed, AutoMapSignalPressure)
        == AutoMapDoorOpen);
  // Nothing to preview: destroyed doors ignore signals, settled doors stay settled
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorDestroyed, AutoMapSignalSet) < 0);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorDestroyed, AutoMapSignalToggle) < 0);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorOpen, AutoMapSignalSet) < 0);
  CHECK(AutoMapScene_PredictDoorState(AutoMapDoorClosed, AutoMapSignalClear) < 0);
}

static AutoMapLevelSnapshot CorridorWithTeleporter(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellTeleporter;
  snapshot.cells[1 * 4 + 1].hasTeleporterTarget = true;
  snapshot.cells[1 * 4 + 1].teleporterDestLevel = LEVEL;
  snapshot.cells[1 * 4 + 1].teleporterDestX = 2;
  snapshot.cells[1 * 4 + 1].teleporterDestY = 1;
  snapshot.cells[1 * 4 + 1].teleporterDestGlobalX = 2;
  snapshot.cells[1 * 4 + 1].teleporterDestGlobalY = 1;
  return snapshot;
}

static void TeleporterTargetSurvivesWithoutFog(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithTeleporter(), NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.hasTeleporterTarget);
  CHECK(FindCell(drawList, 1, 1)->teleporterTargetVisible);
}

static void TeleporterTargetHiddenWhenDestinationUnexplored(void)
{
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithTeleporter(), FogOver(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.hasTeleporterTarget);
  CHECK(!FindCell(drawList, 1, 1)->teleporterTargetVisible);
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  drawList = AutoMapScene_BuildDrawList(CorridorWithTeleporter(), FogOver(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->teleporterTargetVisible);
}

static void CrossLevelTargetFollowsTheFog(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellInvisibleTeleporter;
  snapshot.cells[1 * 4 + 1].hasTeleporterTarget = true;
  snapshot.cells[1 * 4 + 1].teleporterDestLevel = 1;
  snapshot.cells[1 * 4 + 1].teleporterDestX = 5;
  snapshot.cells[1 * 4 + 1].teleporterDestY = 5;
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(2, AutoMapLevelSize{6, 6}));
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList fogged = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(!FindCell(fogged, 1, 1)->teleporterTargetVisible);
  explored.Mark(AutoMapCell{1, 5, 5});
  fogged = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(FindCell(fogged, 1, 1)->teleporterTargetVisible);
  AutoMapDrawList open = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(FindCell(open, 1, 1)->teleporterTargetVisible);
}

static void TeleporterScopeSurvivesTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = CorridorWithTeleporter();
  snapshot.cells[1 * 4 + 1].teleporterScope = 1;
  snapshot.cells[1 * 4 + 1].teleporterRotation = 1;
  snapshot.cells[1 * 4 + 1].teleporterFacingMode = 0;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.teleporterScope == 1);
  CHECK(FindCell(drawList, 1, 1)->cell.teleporterRotation == 1);
  CHECK(FindCell(drawList, 1, 1)->cell.teleporterFacingMode == 0);
  CHECK(FindCell(drawList, 2, 1)->cell.teleporterScope == -1);
  CHECK(FindCell(drawList, 2, 1)->cell.teleporterRotation == -1);
  CHECK(FindCell(drawList, 2, 1)->cell.teleporterFacingMode == -1);
}

static void SpinnerRotationSurvivesTheDrawList(void)
{
  // Absolute spin: exit facing known. Relative spin: exit facing unknown
  // but the turn itself is kept.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellSpinner;
  snapshot.cells[1 * 4 + 1].spinnerFacing = AutoMapEast;
  snapshot.cells[1 * 4 + 1].spinnerRotation = AutoMapEast;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellSpinner;
  snapshot.cells[1 * 4 + 2].spinnerFacing = -1;
  snapshot.cells[1 * 4 + 2].spinnerRotation = 2;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.spinnerFacing == AutoMapEast);
  CHECK(FindCell(drawList, 1, 1)->cell.spinnerRotation == AutoMapEast);
  CHECK(FindCell(drawList, 2, 1)->cell.spinnerFacing == -1);
  CHECK(FindCell(drawList, 2, 1)->cell.spinnerRotation == 2);
}

static void DormantTeleporterShowsNoTarget(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellDormantTeleporter;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(!FindCell(drawList, 1, 1)->cell.hasTeleporterTarget);
  CHECK(!FindCell(drawList, 1, 1)->teleporterTargetVisible);
}

static void DormantSpinnerSurvivesTheDrawList(void)
{
  // A switched-off spinner keeps its spin (so the tooltip can name it as a
  // hypothetical) but sends nowhere, like a dormant teleporter.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellDormantSpinner;
  snapshot.cells[1 * 4 + 1].spinnerFacing = AutoMapEast;
  snapshot.cells[1 * 4 + 1].spinnerRotation = AutoMapEast;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 1)->cell.kind == AutoMapCellDormantSpinner);
  CHECK(FindCell(drawList, 1, 1)->cell.spinnerFacing == AutoMapEast);
  CHECK(FindCell(drawList, 1, 1)->cell.spinnerRotation == AutoMapEast);
  CHECK(!FindCell(drawList, 1, 1)->cell.hasTeleporterTarget);
}

static void GeneratedMonsterSurvivesTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator generator(2, 1, AutoMapActuatorGenerator);
  generator.hasGeneratedMonster = true;
  generator.generatedKind.frontPicture = 475;
  generator.generatedKind.hasSidePicture = true;
  generator.generatedKind.hasBackPicture = true;
  generator.generatedKind.transparentColour = 4;
  AutoMapFloorActuator spent(1, 1, AutoMapActuatorSpentGenerator);
  snapshot.actuators.push_back(generator);
  snapshot.actuators.push_back(spent);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(drawList.actuators.size() == 2);
  CHECK(drawList.actuators[0].hasGeneratedMonster);
  CHECK(drawList.actuators[0].generatedKind.frontPicture == 475);
  CHECK(drawList.actuators[0].generatedKind.transparentColour == 4);
  CHECK(!drawList.actuators[1].hasGeneratedMonster);
}

static void DoorBreakabilitySurvivesTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 2].doorBreakableWithAxe = true;
  snapshot.cells[1 * 4 + 2].doorBreakableWithFireball = true;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 2, 1)->cell.doorBreakableWithAxe);
  CHECK(FindCell(drawList, 2, 1)->cell.doorBreakableWithFireball);
  CHECK(!FindCell(drawList, 1, 1)->cell.doorBreakableWithAxe);
  CHECK(!FindCell(drawList, 1, 1)->cell.doorBreakableWithFireball);
}

static void WallTierFlagsSurviveTheDrawList(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].wallSides[AutoMapSouth].functional = true;
  snapshot.cells[1 * 4 + 0].wallSides[AutoMapEast].hidden = true;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].functional);
  CHECK(!FindCell(drawList, 1, 0)->cell.wallSides[AutoMapEast].functional);
  CHECK(FindCell(drawList, 0, 1)->cell.wallSides[AutoMapEast].hidden);
}

static void UnderFogATierFlagGoesWithItsHiddenSide(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].wallSides[AutoMapSouth].hidden = true;
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  const AutoMapShownCell *wall = FindCell(drawList, 1, 0);
  CHECK(!wall->cell.wallSides[AutoMapSouth].hidden);
  CHECK(wall->cell.wallSides[AutoMapSouth].picture == AUTOMAP_NO_PICTURE);
}

static void SpellFireShrinksWithItsRange(void)
{
  CHECK(AutoMapScene_MissileScale(255) == 1.0f);
  float freshFireball = AutoMapScene_MissileScale(150);
  CHECK(freshFireball > 0.58f && freshFireball < 0.60f);
  CHECK(AutoMapScene_MissileScale(200) > freshFireball);
  CHECK(AutoMapScene_MissileScale(95) == 0.375f);
  CHECK(AutoMapScene_MissileScale(0) == 0.375f);
}

static AutoMapAlcoveEntry AlcoveEntry(int picture, int side)
{
  AutoMapAlcoveEntry entry;
  entry.item = ItemPicture(picture);
  entry.side = side;
  return entry;
}

static AutoMapWallLock WallLock(int picture, int side)
{
  AutoMapWallLock lock;
  lock.side = side;
  lock.object = ItemPicture(picture);
  lock.actuatorType = 3;
  lock.required = true;
  return lock;
}

// A type-16 swap lock (Mouse.cpp TouchWall case 16): the hand object must
// match the value, like any other keyhole, so it reads as a NEEDS too.
static AutoMapWallLock SwapLock(int picture, int side)
{
  AutoMapWallLock lock;
  lock.side = side;
  lock.object = ItemPicture(picture);
  lock.actuatorType = 16;
  lock.required = true;
  return lock;
}

static AutoMapWallLock PlainHolder(int picture, int side)
{
  AutoMapWallLock lock;
  lock.side = side;
  lock.object = ItemPicture(picture);
  lock.actuatorType = 13;
  lock.required = false;
  return lock;
}

static void TooltipShowsAlcoveContentsWithoutFog(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(373, AutoMapSouth));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 2);
  CHECK(slots[0].item.worldPicture == 360);
  CHECK(slots[0].count == 2);
  CHECK(slots[1].item.worldPicture == 373);
  CHECK(slots[1].count == 1);
}

static void TooltipShowsWallLockWithoutFog(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(WallLock(360, AutoMapSouth));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 1);
  CHECK(slots[0].item.worldPicture == 360);
  CHECK(slots[0].count == 1);
  CHECK(slots[0].required);
  CHECK(slots[0].side == AutoMapSouth);
}

static void UnderFogAlcoveAndLockHideWithTheirSide(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  // South side of (1,0) faces the floor at (1,1); north faces (1,-1), off the level.
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(361, AutoMapNorth));
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(WallLock(373, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(WallLock(374, AutoMapNorth));
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 2);
  CHECK(slots[0].item.worldPicture == 360);
  CHECK(slots[1].item.worldPicture == 373);
  // Without the floor in front, the south side hides too.
  AutoMapExplored wallOnly = NothingExplored();
  wallOnly.Mark(AutoMapCell{LEVEL, 1, 0});
  drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(wallOnly, visibleNow));
  CHECK(AutoMapScene_TooltipSlots(drawList, 1, 0).empty());
}

static void AlcoveAndLockStaySeparateFromFloorItems(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].itemPictures.push_back(ItemPicture(360));
  snapshot.cells[1 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapNorth));
  snapshot.cells[1 * 4 + 1].wallLocks.push_back(WallLock(360, AutoMapSouth));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 3);
  CHECK(!slots[0].required);
  CHECK(slots[0].side == -1);
  CHECK(slots[0].count == 1);
  CHECK(!slots[1].required);
  CHECK(slots[1].side == AutoMapNorth);
  CHECK(slots[2].required);
  CHECK(slots[2].side == AutoMapSouth);
}

static void RequiredNeverMergesWithPresent(void)
{
  // A stocked alcove next to its lock: same key, same side, still two slots.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(WallLock(360, AutoMapSouth));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 2);
  CHECK(!slots[0].required);
  CHECK(slots[0].count == 2);
  CHECK(slots[1].required);
  CHECK(slots[1].count == 1);
}

static void SwapLockNeedsItsKeyAlongsideStockedAlcove(void)
{
  // The firestaff wall (DM L13): a type-16 swap lock wanting the empty staff
  // next to the upgraded staff stocked in its alcove. Same side, different
  // items: one HAVE plus one NEEDS, never a single merged slot.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(361, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(SwapLock(360, AutoMapSouth));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 2);
  CHECK(!slots[0].required);
  CHECK(slots[0].item.worldPicture == 361);
  CHECK(slots[0].side == AutoMapSouth);
  CHECK(slots[1].required);
  CHECK(slots[1].item.worldPicture == 360);
  CHECK(slots[1].side == AutoMapSouth);
}

static void PlainHolderShowsItemWhenEmpty(void)
{
  // A holder without an action is a mere container: plain icon and name, no
  // NEEDS, no keyhole badge. Duplicates collapse like requirements do.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(PlainHolder(360, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(PlainHolder(360, AutoMapSouth));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 1);
  CHECK(!slots[0].required);
  CHECK(slots[0].side == AutoMapSouth);
  CHECK(slots[0].item.worldPicture == 360);
  CHECK(slots[0].count == 1);
}

static void PlainHolderSilentWhenStocked(void)
{
  // The torch is already in the holder: showing it again would double the pile.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(PlainHolder(360, AutoMapSouth));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 1);
  CHECK(!slots[0].required);
  CHECK(slots[0].count == 1);
}

static void DoorLockShowsInTooltip(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 2].wallLocks.push_back(WallLock(360, AutoMapSouth));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 2, 1);
  CHECK(slots.size() == 1);
  CHECK(slots[0].required);
  CHECK(slots[0].side == AutoMapSouth);
  CHECK(slots[0].item.worldPicture == 360);
}

static void DoorLockFollowsSideVisibility(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 2].wallLocks.push_back(WallLock(360, AutoMapSouth));
  // South of (2,1) is (2,2): unexplored, so the lock stays hidden with its side.
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_TooltipSlots(drawList, 2, 1).empty());
  explored.Mark(AutoMapCell{LEVEL, 2, 2});
  drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_TooltipSlots(drawList, 2, 1).size() == 1);
}

static void SameKeyOnDifferentSidesStaysSeparate(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(WallLock(360, AutoMapSouth));
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(WallLock(360, AutoMapEast));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 2);
  CHECK(slots[0].required);
  CHECK(slots[0].side == AutoMapSouth);
  CHECK(slots[0].count == 1);
  CHECK(slots[1].required);
  CHECK(slots[1].side == AutoMapEast);
  CHECK(slots[1].count == 1);
}

static void StackedActuatorsBehindOneKeyholeShowOneNeed(void)
{
  // One keyhole driving several targets: several actuator records, one key in
  // hand satisfies them all on a single click, so the tooltip names it once.
  AutoMapLevelSnapshot snapshot = Corridor();
  for (int i = 0; i < 5; i++)
  {
    snapshot.cells[0 * 4 + 1].wallLocks.push_back(WallLock(360, AutoMapSouth));
  }
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 0);
  CHECK(slots.size() == 1);
  CHECK(slots[0].required);
  CHECK(slots[0].item.worldPicture == 360);
  CHECK(slots[0].count == 1);
}

static AutoMapFloorActuator PadWithOp(int x, int y, AutoMapPadOperator op);
static AutoMapFloorActuator PadNeed(int x, int y, AutoMapPadOperator op, int picture);

static void PadCarryNeedShowsAsRequired(void)
{
  // A CARRY pad alone: one NEEDS CARRIED slot, no side, never plain.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadNeed(1, 1, AutoMapPadPossession, 360));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 1);
  CHECK(slots[0].required);
  CHECK(slots[0].carried);
  CHECK(slots[0].side == -1);
  CHECK(slots[0].item.worldPicture == 360);
  CHECK(slots[0].count == 1);
}

static void PadCarryNeedShowsAlongsideIdenticalFloorItem(void)
{
  // A floor firestaff does not satisfy "carried": HAVE plus NEEDS, two slots.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].itemPictures.push_back(ItemPicture(360));
  snapshot.actuators.push_back(PadNeed(1, 1, AutoMapPadPossession, 360));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 2);
  CHECK(!slots[0].required);
  CHECK(slots[0].item.worldPicture == 360);
  CHECK(slots[1].required);
  CHECK(slots[1].carried);
  CHECK(slots[1].side == -1);
  CHECK(slots[1].item.worldPicture == 360);
}

static void PadObjectNeedShowsWhenMissing(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadNeed(1, 1, AutoMapPadObject, 360));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 1);
  CHECK(slots[0].required);
  CHECK(!slots[0].carried);
  CHECK(slots[0].side == -1);
  CHECK(slots[0].item.worldPicture == 360);
}

static void PadObjectNeedSilentWhenSatisfied(void)
{
  // The item is actually on the pad: it satisfies the trigger and is floor
  // loot, so a single HAVE with no NEEDS.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].itemPictures.push_back(ItemPicture(360));
  snapshot.actuators.push_back(PadNeed(1, 1, AutoMapPadObject, 360));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 1);
  CHECK(!slots[0].required);
  CHECK(slots[0].item.worldPicture == 360);
}

static void PadNeedsFoldDuplicatesButKeepCarryApartFromObject(void)
{
  // One carried item satisfies every matching pad at once, like one key in
  // hand satisfies every lock on its side: identical CARRY pads fold to one
  // NEEDS. CARRY and OBJECT needs for the same item stay separate slots:
  // they ask for the item in different places.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadNeed(1, 1, AutoMapPadPossession, 360));
  snapshot.actuators.push_back(PadNeed(1, 1, AutoMapPadPossession, 360));
  snapshot.actuators.push_back(PadNeed(1, 1, AutoMapPadObject, 360));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, 1, 1);
  CHECK(slots.size() == 2);
  CHECK(slots[0].required && slots[0].carried);
  CHECK(slots[0].count == 1);
  CHECK(slots[1].required && !slots[1].carried);
  CHECK(slots[1].count == 1);
}

static void PadNeedWithoutPictureStaysOut(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator unknown = PadWithOp(1, 1, AutoMapPadPossession);
  snapshot.actuators.push_back(unknown);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(AutoMapScene_TooltipSlots(drawList, 1, 1).empty());
}

static void PadNeedHidesWithUnexploredActuator(void)
{
  // The cell is in sight but not explored: the pad the fog hides names no
  // NEEDS, like its TRIGGER line stays silent.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadNeed(1, 1, AutoMapPadPossession, 360));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  visibleNow.Add(1, 1);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_TooltipSlots(drawList, 1, 1).empty());
}

static void ExcerptCollapsesWhitespaceAndCapsLength(void)
{
  CHECK(AutoMapScene_MakeExcerpt("") == "");
  CHECK(AutoMapScene_MakeExcerpt("  hello\n\nworld  ") == "hello world");
  std::string longText(200, 'x');
  std::string excerpt = AutoMapScene_MakeExcerpt(longText);
  CHECK(excerpt.size() == 180);
  CHECK(excerpt.substr(177) == "...");
}

static void ScrollTextsPersistLikeItemsUnderFog(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[1 * 4 + 1].scrollTexts.push_back("eat me");
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList hidden =
      AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_ScrollTexts(hidden, 1, 1).empty());
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapDrawList shown =
      AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  std::vector<std::string> texts = AutoMapScene_ScrollTexts(shown, 1, 1);
  CHECK(texts.size() == 1);
  CHECK(texts[0] == "eat me");
}

static void WallTextsHideWithTheirSide(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[0 * 4 + 1].wallSides[AutoMapSouth].text = "beware";
  corridor.cells[0 * 4 + 1].wallSides[AutoMapNorth].text = "far side";
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  std::vector<AutoMapWallText> texts = AutoMapScene_WallTexts(drawList, 1, 0);
  CHECK(texts.size() == 1);
  CHECK(texts[0].side == AutoMapSouth);
  CHECK(texts[0].text == "beware");
  CHECK(AutoMapScene_WallTexts(drawList, 3, 0).empty());
}

static void AlcoveAndChestScrollTextsShowWithoutFog(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapSouth));
  corridor.cells[0 * 4 + 1].alcoveContents.back().text = "alcove scroll";
  corridor.cells[1 * 4 + 1].scrollTexts.push_back("floor scroll");
  AutoMapChestContents chest;
  chest.item = 0;
  chest.pictures.push_back(ItemPicture(360));
  chest.pictureTexts.push_back("");
  chest.pictures.push_back(ItemPicture(361));
  chest.pictureTexts.push_back("chest scroll");
  corridor.cells[1 * 4 + 1].itemPictures.push_back(ItemPicture(362));
  corridor.cells[1 * 4 + 1].chestContents.push_back(chest);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  std::vector<std::string> alcoveTexts = AutoMapScene_ScrollTexts(drawList, 1, 0);
  CHECK(alcoveTexts.size() == 1);
  CHECK(alcoveTexts[0] == "alcove scroll");
  std::vector<std::string> floorTexts = AutoMapScene_ScrollTexts(drawList, 1, 1);
  CHECK(floorTexts.size() == 2);
  CHECK(floorTexts[0] == "floor scroll");
  CHECK(floorTexts[1] == "chest scroll");
}

static void AlcoveScrollTextHidesWithItsSide(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[0 * 4 + 1].alcoveContents.push_back(AlcoveEntry(360, AutoMapNorth));
  corridor.cells[0 * 4 + 1].alcoveContents.back().text = "far side scroll";
  // North of (1,0) is off the level: the side stays hidden, so does its text.
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_ScrollTexts(drawList, 1, 0).empty());
}

static void MonsterCarriedScrollTextFollowsItsMonster(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  AutoMapMonsterRecord mummy = MummyAt(1, 1);
  mummy.possessionPictures.push_back(ItemPicture(360));
  mummy.possessionTexts.push_back("");
  mummy.possessionPictures.push_back(ItemPicture(361));
  mummy.possessionTexts.push_back("stolen scroll");
  corridor.monsters.push_back(mummy);
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList hidden =
      AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_ScrollTexts(hidden, 1, 1).empty());
  AutoMapDrawList shown =
      AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  std::vector<std::string> texts = AutoMapScene_ScrollTexts(shown, 1, 1);
  CHECK(texts.size() == 1);
  CHECK(texts[0] == "stolen scroll");
}

static const char *TestItemName(int nameIndex)
{
  if (nameIndex == 7) return "SKELETON KEY";
  return "";
}

static AutoMapFloorActuator PadWithOp(int x, int y, AutoMapPadOperator op)
{
  AutoMapFloorActuator pad(x, y, AutoMapActuatorPad);
  pad.op = op;
  return pad;
}

static AutoMapFloorActuator PadNeed(int x, int y, AutoMapPadOperator op, int picture)
{
  AutoMapFloorActuator pad(x, y, AutoMapActuatorPad);
  pad.op = op;
  pad.object = ItemPicture(picture);
  return pad;
}

static void TriggerLinesNameEveryOperator(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadAnything));
  AutoMapFloorActuator directed = PadWithOp(2, 1, AutoMapPadPartyFacing);
  directed.requiredFacing = AutoMapNorth;
  snapshot.actuators.push_back(directed);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(Texts(AutoMapScene_TriggerLines(drawList, 1, 1, TestItemName)).size() == 1);
  CHECK(Texts(AutoMapScene_TriggerLines(drawList, 1, 1, TestItemName))[0] == "TRIGGER: ANYTHING");
  std::vector<std::string> directedLines =
      Texts(AutoMapScene_TriggerLines(drawList, 2, 1, TestItemName));
  CHECK(directedLines.size() == 1);
  CHECK(directedLines[0] == std::string("TRIGGER: ") + AUTOMAP_GLYPH_FACING + '0' + " PARTY FACING N");

  AutoMapLevelSnapshot rest = Corridor();
  rest.actuators.push_back(PadWithOp(1, 1, AutoMapPadPartyOrMonster));
  rest.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  rest.actuators.push_back(PadWithOp(1, 1, AutoMapPadMonster));
  AutoMapFloorActuator unknownFacing = PadWithOp(1, 1, AutoMapPadPartyFacing);
  unknownFacing.requiredFacing = -1;
  rest.actuators.push_back(unknownFacing);
  AutoMapDrawList restList = AutoMapScene_BuildDrawList(rest, NoFog(explored, visibleNow));
  std::vector<std::string> restLines =
      Texts(AutoMapScene_TriggerLines(restList, 1, 1, TestItemName));
  CHECK(restLines.size() == 4);
  CHECK(restLines[0] == "TRIGGER: \x01\x01 PARTY \x02\x02 MONSTER");
  CHECK(restLines[1] == "TRIGGER: \x01\x01 PARTY");
  CHECK(restLines[2] == "TRIGGER: \x02\x02 MONSTER");
  // An unknown facing has no arrow to turn: the plain party glyph
  CHECK(restLines[3] == "TRIGGER: \x01\x01 PARTY FACING ?");
}

static void TriggerLinesNameRequiredObjects(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator objectPad = PadWithOp(1, 1, AutoMapPadObject);
  objectPad.object = NamedItem(360, 0, 7);
  AutoMapFloorActuator carriedPad = PadWithOp(2, 1, AutoMapPadPossession);
  carriedPad.object = NamedItem(360, 0, 7);
  AutoMapFloorActuator unknownPad = PadWithOp(1, 1, AutoMapPadObject);
  snapshot.actuators.push_back(objectPad);
  snapshot.actuators.push_back(carriedPad);
  snapshot.actuators.push_back(unknownPad);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> objectLines =
      Texts(AutoMapScene_TriggerLines(drawList, 1, 1, TestItemName));
  CHECK(objectLines.size() == 2);
  CHECK(objectLines[0] == std::string("TRIGGER: ") + AUTOMAP_GLYPH_ITEM + "0 OBJECT SKELETON KEY");
  CHECK(objectLines[1] == std::string("TRIGGER: ") + AUTOMAP_GLYPH_ITEM + "0 OBJECT (UNKNOWN)");
  std::vector<std::string> carriedLines =
      Texts(AutoMapScene_TriggerLines(drawList, 2, 1, TestItemName));
  CHECK(carriedLines.size() == 1);
  CHECK(carriedLines[0] == std::string("TRIGGER: \x01\x01 CARRY ") + AUTOMAP_GLYPH_ITEM + "0 SKELETON KEY");
}

static void TriggerLinesFoldDuplicatesAndNameContraptions(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  snapshot.actuators.push_back(AutoMapFloorActuator(2, 1, AutoMapActuatorDormantPad));
  AutoMapFloorActuator generator(2, 1, AutoMapActuatorGenerator);
  generator.hasGeneratedMonster = true;
  generator.generatedMonsterName = "Mummy";
  generator.generatedCount = 3;
  snapshot.actuators.push_back(generator);
  AutoMapFloorActuator single(1, 1, AutoMapActuatorGenerator);
  single.hasGeneratedMonster = true;
  single.generatedMonsterName = "Screamer";
  snapshot.actuators.push_back(single);
  AutoMapFloorActuator unknown(1, 1, AutoMapActuatorGenerator);
  snapshot.actuators.push_back(unknown);
  snapshot.actuators.push_back(AutoMapFloorActuator(2, 1, AutoMapActuatorSpentGenerator));
  snapshot.actuators.push_back(AutoMapFloorActuator(2, 1, AutoMapActuatorCog));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> partyLines =
      Texts(AutoMapScene_TriggerLines(drawList, 1, 1, TestItemName));
  CHECK(partyLines.size() == 3);
  CHECK(partyLines[0] == "TRIGGER: \x01\x01 PARTY");
  CHECK(partyLines[1] == "GENERATES Screamer");
  CHECK(partyLines[2] == "GENERATES MONSTER");
  std::vector<std::string> machineLines =
      Texts(AutoMapScene_TriggerLines(drawList, 2, 1, TestItemName));
  // The cog is left to the relay hover block; the dormant pad and the spent generator are
  // named by their tooltip headers (AutoMapTooltip_Machinery), not by a line here
  CHECK(machineLines.size() == 1);
  CHECK(machineLines[0] == "GENERATES 3X Mummy");
}

// A party pad and a monster pad want different things: two presses
static void EachPadOfACellIsItsOwnPress(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellFloor;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[0 * 4 + 1].kind = AutoMapCellDoor;
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadMonster));
  snapshot.actuators[0].record = 101;
  snapshot.actuators[1].record = 102;
  AutoMapLink toFirst = {1, 1, 2, 1, false, -1};
  toFirst.source = 101;
  AutoMapLink toSecond = {1, 1, 1, 0, false, -1};
  toSecond.source = 102;
  snapshot.links.push_back(toFirst);
  snapshot.links.push_back(toSecond);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 1, 1);
  CHECK(sources.size() == 2);
  if (sources.size() != 2) return;
  CHECK(sources[0].source == 101);
  CHECK(sources[0].effects.size() == 1);
  CHECK(sources[0].effects[0].x == 2 && sources[0].effects[0].y == 1);
  CHECK(sources[1].source == 102);
  CHECK(sources[1].effects.size() == 1);
  CHECK(sources[1].effects[0].x == 1 && sources[1].effects[0].y == 0);
}

// Two party pads fire on the same step: one press with both their doors
static void SameTriggerPadsAreOnePress(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellFloor;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[0 * 4 + 1].kind = AutoMapCellDoor;
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  snapshot.actuators[0].record = 101;
  snapshot.actuators[1].record = 102;
  AutoMapLink toFirst = {1, 1, 2, 1, false, -1};
  toFirst.source = 101;
  AutoMapLink toSecond = {1, 1, 1, 0, false, -1};
  toSecond.source = 102;
  snapshot.links.push_back(toFirst);
  snapshot.links.push_back(toSecond);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 1, 1);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  CHECK(sources[0].source == 101);
  CHECK(sources[0].effects.size() == 2);
  CHECK(AutoMapScene_PressGoalsByCell(sources).size() == 2);
  // Facing pads are one press only when they want the same facing
  AutoMapLevelSnapshot facing = snapshot;
  facing.actuators[0].op = AutoMapPadPartyFacing;
  facing.actuators[0].requiredFacing = AutoMapEast;
  facing.actuators[1].op = AutoMapPadPartyFacing;
  facing.actuators[1].requiredFacing = AutoMapWest;
  AutoMapDrawList facingList = AutoMapScene_BuildDrawList(facing, NoFog(explored, visibleNow));
  CHECK(AutoMapScene_PressEffects(facingList, 1, 1).size() == 2);
  facing.actuators[1].requiredFacing = AutoMapEast;
  facingList = AutoMapScene_BuildDrawList(facing, NoFog(explored, visibleNow));
  CHECK(AutoMapScene_PressEffects(facingList, 1, 1).size() == 1);
  // Object pads only when they want the same object
  AutoMapLevelSnapshot objects = snapshot;
  objects.actuators[0].op = AutoMapPadObject;
  objects.actuators[0].object = ItemPicture(360);
  objects.actuators[1].op = AutoMapPadObject;
  objects.actuators[1].object = ItemPicture(361);
  AutoMapDrawList objectList = AutoMapScene_BuildDrawList(objects, NoFog(explored, visibleNow));
  CHECK(AutoMapScene_PressEffects(objectList, 1, 1).size() == 2);
}

static void PadsReachingOneDoorGetOneBadge(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellFloor;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 2].doorState = AutoMapDoorClosed;
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(CounterRelay(3));
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadMonster));
  snapshot.actuators[0].record = 101;
  snapshot.actuators[1].record = 102;
  // The party pad counts the counter down, the monster pad opens the door directly
  AutoMapLink viaCounter = {1, 1, 0, 1, false, -1};
  viaCounter.source = 101;
  viaCounter.signal = AutoMapSignalClear;
  AutoMapLink counterToDoor = {0, 1, 2, 1, true, AutoMapEast};
  AutoMapLink straight = {1, 1, 2, 1, false, -1};
  straight.source = 102;
  snapshot.links.push_back(viaCounter);
  snapshot.links.push_back(counterToDoor);
  snapshot.links.push_back(straight);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 1, 1);
  CHECK(sources.size() == 2);
  std::vector<const AutoMapEffect *> goals = AutoMapScene_PressGoalsByCell(sources);
  CHECK(goals.size() == 1);
  if (goals.size() != 1) return;
  CHECK(goals[0]->x == 2 && goals[0]->y == 1);
  CHECK(goals[0]->distance.status == AutoMapGoalOnThisPress);
}

static void TriggerLinesStaySilentWithoutTheCell(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList hidden = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(Texts(AutoMapScene_TriggerLines(hidden, 1, 1, TestItemName)).empty());
  CHECK(Texts(AutoMapScene_TeleporterLines(hidden, 1, 1)).empty());
}

static AutoMapFloorActuator GeneratorWithHealth(int x, int y, const char *name,
                                                int levelMultiplier, int healthMultiplier,
                                                int minHealth, int maxHealth)
{
  AutoMapFloorActuator generator(x, y, AutoMapActuatorGenerator);
  generator.hasGeneratedMonster = true;
  generator.generatedMonsterName = name == NULL ? "" : name;
  generator.hasGeneratorHealth = true;
  generator.levelMultiplier = levelMultiplier;
  generator.healthMultiplier = healthMultiplier;
  generator.minHealth = minHealth;
  generator.maxHealth = maxHealth;
  return generator;
}

static void GeneratorHealthRangeFollowsEngineFormula(void)
{
  int minHealth = -1;
  int maxHealth = -1;
  // HP = M * BH + random 0..BH/4: M = LEM when HM is 0
  CHECK(AutoMapScene_GeneratorHealthRange(3, 0, 10, &minHealth, &maxHealth));
  CHECK(minHealth == 30);
  CHECK(maxHealth == 32);
  // HM overrides LEM
  CHECK(AutoMapScene_GeneratorHealthRange(3, 5, 10, &minHealth, &maxHealth));
  CHECK(minHealth == 50);
  CHECK(maxHealth == 52);
  // BH < 4 leaves no random part: min == max
  CHECK(AutoMapScene_GeneratorHealthRange(2, 0, 3, &minHealth, &maxHealth));
  CHECK(minHealth == 6);
  CHECK(maxHealth == 6);
  // Missing level value while needed: unknown
  CHECK(!AutoMapScene_GeneratorHealthRange(-1, 0, 10, &minHealth, &maxHealth));
  // ... but an override still resolves its own range without the level
  CHECK(AutoMapScene_GeneratorHealthRange(-1, 5, 10, &minHealth, &maxHealth));
  CHECK(minHealth == 50);
  CHECK(maxHealth == 52);
  CHECK(!AutoMapScene_GeneratorHealthRange(3, -1, 10, &minHealth, &maxHealth));
  CHECK(!AutoMapScene_GeneratorHealthRange(3, 0, -1, &minHealth, &maxHealth));
}

static void GeneratorHealthLinesNameEachValueOnItsOwnLine(void)
{
  std::vector<std::string> normal = AutoMapScene_GeneratorHealthLines(3, 0, 30, 32);
  CHECK(normal.size() == 2);
  CHECK(normal[0] == "LEVEL EXPERIENCE MULTIPLIER 3");
  CHECK(normal[1] == "HP RANGE 30-32");
  std::vector<std::string> single = AutoMapScene_GeneratorHealthLines(2, 0, 6, 6);
  CHECK(single.size() == 2);
  CHECK(single[0] == "LEVEL EXPERIENCE MULTIPLIER 2");
  CHECK(single[1] == "HP RANGE 6");
  std::vector<std::string> overridden = AutoMapScene_GeneratorHealthLines(3, 5, 50, 52);
  CHECK(overridden.size() == 3);
  CHECK(overridden[0] == "HEALTH MULTIPLIER 5");
  CHECK(overridden[1] == "LEVEL EXPERIENCE MULTIPLIER 3");
  CHECK(overridden[2] == "HP RANGE 50-52");
  std::vector<std::string> noLevel = AutoMapScene_GeneratorHealthLines(-1, 5, 50, 52);
  CHECK(noLevel.size() == 2);
  CHECK(noLevel[0] == "HEALTH MULTIPLIER 5");
  CHECK(noLevel[1] == "HP RANGE 50-52");
  CHECK(AutoMapScene_GeneratorHealthLines(-1, 0, 30, 32).empty());
  CHECK(AutoMapScene_GeneratorHealthLines(3, 0, -1, -1).empty());
}

static void TriggerLinesShowGeneratorHealth(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(GeneratorWithHealth(1, 1, "Screamer", 3, 0, 30, 32));
  snapshot.actuators.push_back(GeneratorWithHealth(2, 1, "Mummy", 3, 5, 50, 52));
  // Same generator twice: the health line folds like every other trigger line
  snapshot.actuators.push_back(GeneratorWithHealth(2, 1, "Mummy", 3, 5, 50, 52));
  // Unresolved generator: name line only, no health line
  snapshot.actuators.push_back(AutoMapFloorActuator(1, 1, AutoMapActuatorGenerator));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> normal =
      Texts(AutoMapScene_TriggerLines(drawList, 1, 1, TestItemName));
  CHECK(normal.size() == 4);
  CHECK(normal[0] == "GENERATES Screamer");
  CHECK(normal[1] == "LEVEL EXPERIENCE MULTIPLIER 3");
  CHECK(normal[2] == "HP RANGE 30-32");
  CHECK(normal[3] == "GENERATES MONSTER");
  std::vector<std::string> overridden =
      Texts(AutoMapScene_TriggerLines(drawList, 2, 1, TestItemName));
  CHECK(overridden.size() == 4);
  CHECK(overridden[0] == "GENERATES Mummy");
  CHECK(overridden[1] == "HEALTH MULTIPLIER 5");
  CHECK(overridden[2] == "LEVEL EXPERIENCE MULTIPLIER 3");
  CHECK(overridden[3] == "HP RANGE 50-52");
}

static void CounterCogCountsOnceAtAll(void)
{
  // A floor counter's live count shows in the relay hover block, not again as a trigger
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator counter(1, 1, AutoMapActuatorCog);
  counter.counterValue = 2;
  counter.relay = CounterRelay(2);
  snapshot.actuators.push_back(counter);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(Texts(AutoMapScene_TriggerLines(drawList, 1, 1, TestItemName)).empty());
  std::vector<std::string> relay = AutoMapScene_RelayHoverLines(drawList, 1, 1, false);
  CHECK(relay.size() == 2);
  if (relay.size() != 2) return;
  CHECK(relay[0] == "FLOOR: 1 COUNTER");
  CHECK(relay[1] == " A COUNT 2");
}

static std::string Joined(const std::vector<std::string> &lines);

// The press blocks of the sources one after another, as text (AutoMapScene_PressBlock)
static std::vector<std::string> PressText(const AutoMapDrawList &drawList,
                                          const std::vector<AutoMapPressSource> &sources,
                                          bool shift)
{
  std::vector<std::string> texts;
  for (size_t s = 0; s < sources.size(); s++)
  {
    std::vector<AutoMapTipLine> block = AutoMapScene_PressBlock(drawList, sources[s], shift);
    for (size_t i = 0; i < block.size(); i++) texts.push_back(block[i].text);
  }
  return texts;
}

static void RelayLinesDescribeWallCounterChain(void)
{
  // Coin slot (1,0, wall) -> counter (0,1, wall-embedded, value 2) -> door (2,1).
  // Global coordinates follow the rulers (local + offset), levels from zero.
  // (0,1) carries its east torch side and (1,0) its south lever side in Corridor(),
  // so both wall-source links survive the draw list without fog.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = 2;
  snapshot.offsetX = 12;
  snapshot.offsetY = 22;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(CounterRelay(2));
  AutoMapLink slotToCounter = {1, 0, 0, 1, true, AutoMapSouth};
  AutoMapLink counterToDoor = {0, 1, 2, 1, true, AutoMapEast};
  // Coin slots decrement the counter (CLEAR), the relay opens the door (SET).
  slotToCounter.signal = AutoMapSignalClear;
  snapshot.links.push_back(slotToCounter);
  snapshot.links.push_back(counterToDoor);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  // The cell block is silent at All; the press and relay-hover blocks name the chain.
  CHECK(Joined(PressText(drawList, AutoMapScene_PressEffects(drawList, 1, 0), true))
        == "ROUTE:\n  COUNTER A L2 (12, 23)\n    COUNTS 2 -> 1");
  CHECK(Joined(AutoMapScene_RelayHoverLines(drawList, 0, 1, false))
        == "STONE: 1 COUNTER\n A COUNT 2\nFED BY:\n L2 (13, 22) SWITCH (S) A-1");
  // A SET firing increments instead: same chain, opposite delta.
  AutoMapLevelSnapshot upSnapshot = Corridor();
  upSnapshot.level = 2;
  upSnapshot.offsetX = 12;
  upSnapshot.offsetY = 22;
  upSnapshot.cells[1 * 4 + 0].wallRelays.push_back(CounterRelay(2));
  AutoMapLink upLink = {1, 0, 0, 1, true, AutoMapSouth};
  upLink.signal = AutoMapSignalSet;
  upSnapshot.links.push_back(upLink);
  AutoMapDrawList upDrawList =
      AutoMapScene_BuildDrawList(upSnapshot, NoFog(explored, visibleNow));
  CHECK(Joined(PressText(upDrawList, AutoMapScene_PressEffects(upDrawList, 1, 0), true))
        == "ROUTE:\n  COUNTER A L2 (12, 23)\n    COUNTS UP 2 -> 3");
}

static void RelayHoverLinesNameEachCounterSourceCount(void)
{
  // Two counters share (0,1) yet feed different targets, the DM L5 vault shape:
  // the gate counter still needs 4 decrements, the door counter 3. The cell
  // names both counters, and each source link names its own live count instead
  // of the first counter on the cell hiding the other.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = 2;
  snapshot.offsetX = 12;
  snapshot.offsetY = 22;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  AutoMapRelay gateCounter = CounterRelay(4);
  gateCounter.record = 60;
  AutoMapRelay doorCounter = CounterRelay(3);
  doorCounter.record = 61;
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(gateCounter);
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(doorCounter);
  AutoMapLink toGate = {0, 1, 2, 0, true, AutoMapEast};
  toGate.signal = AutoMapSignalClear;
  toGate.fromCounter = true;
  toGate.source = 60;
  AutoMapLink toDoor = {0, 1, 2, 1, true, AutoMapEast};
  toDoor.signal = AutoMapSignalSet;
  toDoor.fromCounter = true;
  toDoor.source = 61;
  snapshot.links.push_back(toGate);
  snapshot.links.push_back(toDoor);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(Joined(AutoMapScene_RelayHoverLines(drawList, 0, 1, false))
        == "STONE: 2 COUNTERS\n A COUNT 4 -> L2 (14, 22) TARGET CLEAR AT 0\n"
           " B COUNT 3 -> L2 (14, 23) DOOR OPEN AT 0");
}

static void PressLinesDescribeGateBitsAndSwitches(void)
{
  // Four-switch gate puzzle shape: switches flip bits, the gate opens the door
  // on equality. (0,1) is the wall-embedded gate (value 3 = 0011, needs 0000),
  // (1,0) a switch flipping bit 1, (2,1) the door.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = 2;
  snapshot.offsetX = 12;
  snapshot.offsetY = 22;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(GateRelay(3, false));
  AutoMapLink gateToDoor = {0, 1, 2, 1, true, AutoMapEast};
  gateToDoor.signal = AutoMapSignalSet;
  gateToDoor.gatePattern = 0;
  snapshot.links.push_back(gateToDoor);
  AutoMapLink switchToGate = {1, 0, 0, 1, true, AutoMapSouth};
  switchToGate.signal = AutoMapSignalToggle;
  switchToGate.targetPos = 1;
  snapshot.links.push_back(switchToGate);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(Joined(AutoMapScene_RelayHoverLines(drawList, 0, 1, false))
        == "STONE: 1 GATE\n A GATE 0011 (NEEDS 0000)\nFED BY:\n L2 (13, 22) SWITCH (S) A BIT 1 TOGGLE");
  CHECK(Joined(PressText(drawList, AutoMapScene_PressEffects(drawList, 1, 0), true))
        == "ROUTE:\n  GATE A L2 (12, 23), TOGGLES BIT 1\n    BITS 0011 -> 0001, NEEDS 0000");
  // Same switch, second bit: same source and target but another position stays
  // its own line instead of folding into the first.
  AutoMapLevelSnapshot twoBits = snapshot;
  AutoMapLink secondBit = switchToGate;
  secondBit.targetPos = 2;
  twoBits.links.push_back(secondBit);
  AutoMapDrawList twoDrawList =
      AutoMapScene_BuildDrawList(twoBits, NoFog(explored, visibleNow));
  CHECK(Joined(PressText(twoDrawList, AutoMapScene_PressEffects(twoDrawList, 1, 0), true))
        == "ROUTE:\n  GATE A L2 (12, 23), TOGGLES BIT 1\n    BITS 0011 -> 0001, NEEDS 0000\n"
           "  GATE A L2 (12, 23), TOGGLES BIT 2\n    BITS 0001 -> 0101, NEEDS 0000");
  // A bank of gates sharing one cell: one GATE line per distinct pattern.
  AutoMapLevelSnapshot bank = snapshot;
  bank.cells[1 * 4 + 0].wallRelays.push_back(GateRelay(0xF1, false));
  AutoMapDrawList bankDrawList =
      AutoMapScene_BuildDrawList(bank, NoFog(explored, visibleNow));
  CHECK(Joined(AutoMapScene_RelayHoverLines(bankDrawList, 0, 1, false))
        == "STONE: 2 GATES\n A GATE 0011 (NEEDS 0000)\n B GATE 0001 (NEEDS 1111)\nFED BY:\n L2 (13, 22) SWITCH (S) A BIT 1 TOGGLE B BIT 1 TOGGLE");
}

static void RelayHoverLinesKeepIdenticalGates(void)
{
  // Two gate records of one value keep their own entries (each can feed its own
  // target); the hover block lists each as its own letter.
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapRelay first = GateRelay(3, false);
  first.record = 11;
  AutoMapRelay second = GateRelay(3, false);
  second.record = 12;
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(first);
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(second);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(FindCell(drawList, 0, 1)->cell.wallRelays.size() == 2);
  CHECK(Joined(AutoMapScene_RelayHoverLines(drawList, 0, 1, false))
        == "STONE: 2 GATES\n A GATE 0011 (NEEDS 0000)\n B GATE 0011 (NEEDS 0000)");
}

static void RelayHoverLinesNameLocalGatesAsMessageDriven(void)
{
  // DM L5's four-alcove gate is local-action (word4 bit 11): 0000/0000 means
  // "pressed", but it fires only when the counter at (18,28) messages it. It
  // must not read as a standing "NEEDS 0000" that is already satisfied.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(GateRelay(0, true));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(Joined(AutoMapScene_RelayHoverLines(drawList, 0, 1, false))
        == "STONE: 1 GATE\n A GATE 0000 (NEEDS 0000) -> LOCAL ACTION");
  // A co-located ordinary gate keeps its own line, even at the same value.
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(GateRelay(0, false));
  AutoMapDrawList both = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(Joined(AutoMapScene_RelayHoverLines(both, 0, 1, false))
        == "STONE: 2 GATES\n A GATE 0000 (NEEDS 0000) -> LOCAL ACTION\n B GATE 0000 (NEEDS 0000)");
}

static void RelayLinesAtAllKeepOnlyCellFacts(void)
{
  // The press block owns what a press reaches; the cell block keeps what the cell is.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = 2;
  snapshot.offsetX = 12;
  snapshot.offsetY = 22;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(CounterRelay(2));
  AutoMapLink slotToCounter = {1, 0, 0, 1, true, AutoMapSouth};
  slotToCounter.signal = AutoMapSignalClear;
  AutoMapLink counterToDoor = {0, 1, 2, 1, true, AutoMapEast};
  snapshot.links.push_back(slotToCounter);
  snapshot.links.push_back(counterToDoor);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(drawList.spoilers == AutoMapSpoilersAll);
  CHECK(Texts(AutoMapScene_RelayLines(drawList, 1, 0)).empty());
  CHECK(Texts(AutoMapScene_RelayLines(drawList, 0, 1)).empty());
}

static void RelayLinesBelowAllUnchanged(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = 2;
  snapshot.offsetX = 12;
  snapshot.offsetY = 22;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  AutoMapLink open = {1, 0, 2, 1, true, AutoMapSouth};
  snapshot.links.push_back(open);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  drawList.spoilers = AutoMapSpoilersLearned;
  std::vector<std::string> lines = Texts(AutoMapScene_RelayLines(drawList, 1, 0));
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "TARGET: L2 (14, 23) DOOR");
}

static void RelayLinesDescribeLaunchers(void)
{
  // Stone-embedded launchers name ammunition and firing side; paired types
  // loose two; unknown values stay silent instead of inventing ammunition.
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapLauncher cloud;
  cloud.actuatorType = 8;
  cloud.value = 7;
  cloud.side = AutoMapEast;
  snapshot.cells[1 * 4 + 0].wallLaunchers.push_back(cloud);
  AutoMapLauncher pair;
  pair.actuatorType = 10;
  pair.value = 0;
  pair.side = AutoMapSouth;
  snapshot.cells[1 * 4 + 0].wallLaunchers.push_back(pair);
  AutoMapLauncher thrown;
  thrown.actuatorType = 14;
  snapshot.cells[1 * 4 + 0].wallLaunchers.push_back(thrown);
  AutoMapLauncher unknown;
  unknown.actuatorType = 8;
  unknown.value = 5;
  unknown.side = AutoMapNorth;
  snapshot.cells[1 * 4 + 0].wallLaunchers.push_back(unknown);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> lines = Texts(AutoMapScene_RelayLines(drawList, 0, 1));
  CHECK(lines.size() == 6);
  if (lines.size() != 6) return;
  CHECK(lines[0] == "LAUNCHER (E)");
  CHECK(lines[1] == "  LAUNCHES POISON CLOUD");
  CHECK(lines[2] == "LAUNCHER (S)");
  CHECK(lines[3] == "  LAUNCHES 2X FIREBALL");
  CHECK(lines[4] == "LAUNCHER");
  CHECK(lines[5] == "  LAUNCHES OBJECT");
}

static void PressLinesNameBothTriggersOnOneTarget(void)
{
  // One cell opens a door now and closes it later: a plain switch (SET) plus a
  // counter firing at zero (CLEAR, delayed). Same target, two lines, no folding.
  // (1,0) carries its south lever side in Corridor(), so the wall links survive.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = 2;
  snapshot.offsetX = 12;
  snapshot.offsetY = 22;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  AutoMapLink open = {1, 0, 2, 1, true, AutoMapSouth};
  open.signal = AutoMapSignalSet;
  AutoMapLink close = {1, 0, 2, 1, true, AutoMapSouth};
  close.signal = AutoMapSignalClear;
  close.fromCounter = true;
  close.delay = 6;
  snapshot.links.push_back(open);
  snapshot.links.push_back(close);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(Joined(PressText(drawList, AutoMapScene_PressEffects(drawList, 1, 0), true))
        == "RESULT:\n  DOOR L2 (14, 23) ALREADY OPEN\n"
           "  DOOR L2 (14, 23) CLOSES ON THIS PRESS, AFTER 6 TICKS");
}

static void RelayLinesStaySilentWithoutTheCell(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(CounterRelay(2));
  AutoMapLink slotToCounter = {1, 0, 0, 1, true, AutoMapSouth};
  snapshot.links.push_back(slotToCounter);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList hidden = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(Texts(AutoMapScene_RelayLines(hidden, 0, 1)).empty());
  CHECK(Texts(AutoMapScene_RelayLines(hidden, 1, 0)).empty());
}

static AutoMapCellRecord PlainDoor(void)
{
  AutoMapCellRecord door;
  door.kind = AutoMapCellDoor;
  door.doorState = AutoMapDoorClosed;
  return door;
}

static void SealedDoorsNeedNoSwitchLinkOrBash(void)
{
  CHECK(AutoMapScene_DoorIsSealed(PlainDoor(), false));
  AutoMapCellRecord withSwitch = PlainDoor();
  withSwitch.doorHasSwitch = true;
  CHECK(!AutoMapScene_DoorIsSealed(withSwitch, false));
  CHECK(!AutoMapScene_DoorIsSealed(PlainDoor(), true));
  AutoMapCellRecord bashable = PlainDoor();
  bashable.doorBreakableWithAxe = true;
  CHECK(!AutoMapScene_DoorIsSealed(bashable, false));
  AutoMapCellRecord fireBashable = PlainDoor();
  fireBashable.doorBreakableWithFireball = true;
  CHECK(!AutoMapScene_DoorIsSealed(fireBashable, false));
  AutoMapCellRecord destroyed = PlainDoor();
  destroyed.doorState = AutoMapDoorDestroyed;
  CHECK(!AutoMapScene_DoorIsSealed(destroyed, false));
  // A door nothing operates any more, open for good (a once-only lock spent its link): it lets
  // the party through, so it is no wall
  AutoMapCellRecord open = PlainDoor();
  open.doorState = AutoMapDoorOpen;
  CHECK(!AutoMapScene_DoorIsSealed(open, false));
  AutoMapCellRecord floor;
  floor.kind = AutoMapCellFloor;
  CHECK(!AutoMapScene_DoorIsSealed(floor, false));
}

static void RelayLinesNameSealedDoors(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 2].doorState = AutoMapDoorClosed;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> sealed = Texts(AutoMapScene_RelayLines(drawList, 2, 1));
  CHECK(sealed.size() == 2);
  CHECK(sealed.size() == 2 && sealed[0] == "DOOR" && sealed[1] == "  SEALED");
  // An own switch, an incoming link or a bashable door each counts as openable.
  AutoMapLevelSnapshot withSwitch = snapshot;
  withSwitch.cells[1 * 4 + 2].doorHasSwitch = true;
  AutoMapDrawList switchDrawList =
      AutoMapScene_BuildDrawList(withSwitch, NoFog(explored, visibleNow));
  CHECK(Texts(AutoMapScene_RelayLines(switchDrawList, 2, 1)).empty());
  AutoMapLevelSnapshot withLink = snapshot;
  AutoMapLink open = {1, 0, 2, 1, true, AutoMapSouth};
  withLink.links.push_back(open);
  AutoMapDrawList linkDrawList =
      AutoMapScene_BuildDrawList(withLink, NoFog(explored, visibleNow));
  std::vector<std::string> target = Texts(AutoMapScene_RelayLines(linkDrawList, 2, 1));
  CHECK(target.empty());
  AutoMapLevelSnapshot bashable = snapshot;
  bashable.cells[1 * 4 + 2].doorBreakableWithAxe = true;
  AutoMapDrawList bashDrawList =
      AutoMapScene_BuildDrawList(bashable, NoFog(explored, visibleNow));
  CHECK(Texts(AutoMapScene_RelayLines(bashDrawList, 2, 1)).empty());
}

static void RelayLinesNameTheForceThatBreaksADoor(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 2].doorState = AutoMapDoorClosed;
  snapshot.cells[1 * 4 + 2].doorBreakableWithAxe = true;
  snapshot.cells[1 * 4 + 2].doorBreakForce = 60;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList axeDrawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> axe = Texts(AutoMapScene_RelayLines(axeDrawList, 2, 1));
  CHECK(axe.size() == 2);
  CHECK(axe.size() == 2 && axe[0] == "DOOR" && axe[1] == "  BREAK FORCE: 60 (AXE)");
  AutoMapLevelSnapshot both = snapshot;
  both.cells[1 * 4 + 2].doorBreakableWithFireball = true;
  AutoMapDrawList bothDrawList = AutoMapScene_BuildDrawList(both, NoFog(explored, visibleNow));
  CHECK(Texts(AutoMapScene_RelayLines(bothDrawList, 2, 1))[1] == "  BREAK FORCE: 60 (AXE, FIREBALL)");
  AutoMapLevelSnapshot fire = snapshot;
  fire.cells[1 * 4 + 2].doorBreakableWithAxe = false;
  fire.cells[1 * 4 + 2].doorBreakableWithFireball = true;
  AutoMapDrawList fireDrawList = AutoMapScene_BuildDrawList(fire, NoFog(explored, visibleNow));
  CHECK(Texts(AutoMapScene_RelayLines(fireDrawList, 2, 1))[1] == "  BREAK FORCE: 60 (FIREBALL)");
  // A destroyed door has nothing left to break.
  AutoMapLevelSnapshot destroyed = snapshot;
  destroyed.cells[1 * 4 + 2].doorState = AutoMapDoorDestroyed;
  AutoMapDrawList destroyedDrawList =
      AutoMapScene_BuildDrawList(destroyed, NoFog(explored, visibleNow));
  CHECK(Texts(AutoMapScene_RelayLines(destroyedDrawList, 2, 1)).empty());
}

static void TeleporterLinesDescribeScopeDestinationAndTurn(void)
{
  AutoMapLevelSnapshot snapshot = CorridorWithTeleporter();
  snapshot.cells[1 * 4 + 1].teleporterScope = 2;
  snapshot.cells[1 * 4 + 1].teleporterRotation = 1;
  snapshot.cells[1 * 4 + 1].teleporterFacingMode = 0;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> lines = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "TAKES: PARTY+OBJECTS");
  CHECK(lines[1] == "SENDS: L0 (2, 1)");
  CHECK(lines[2] == "TURN: RIGHT");
  CHECK(Texts(AutoMapScene_TeleporterLines(drawList, 2, 1)).empty());
}

static void TeleporterLinesDescribeAbsoluteFacing(void)
{
  AutoMapLevelSnapshot snapshot = CorridorWithTeleporter();
  snapshot.cells[1 * 4 + 1].teleporterScope = 3;
  snapshot.cells[1 * 4 + 1].teleporterRotation = AutoMapWest;
  snapshot.cells[1 * 4 + 1].teleporterFacingMode = 1;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> lines = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "TAKES: ANYTHING");
  CHECK(lines[1] == "SENDS: L0 (2, 1)");
  CHECK(lines[2] == "FACE W");
}

static void DestinationPrintsGlobalCoordinates(void)
{
  // Levels count from zero as in the title bar; coordinates are global as on
  // the rulers, not local to the level.
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellTeleporter;
  snapshot.cells[1 * 4 + 1].hasTeleporterTarget = true;
  snapshot.cells[1 * 4 + 1].teleporterDestLevel = 4;
  snapshot.cells[1 * 4 + 1].teleporterDestX = 2;
  snapshot.cells[1 * 4 + 1].teleporterDestY = 1;
  snapshot.cells[1 * 4 + 1].teleporterDestGlobalX = 15;
  snapshot.cells[1 * 4 + 1].teleporterDestGlobalY = 10;
  snapshot.cells[1 * 4 + 1].teleporterScope = 3;
  snapshot.cells[1 * 4 + 1].teleporterRotation = 1;
  snapshot.cells[1 * 4 + 1].teleporterFacingMode = 0;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> lines = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "TAKES: ANYTHING");
  CHECK(lines[1] == "SENDS: L4 (15, 10)");
  CHECK(lines[2] == "TURN: RIGHT");
}

static void SpinnerLinesDescribeAbsoluteAndRelativeSpin(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellSpinner;
  snapshot.cells[1 * 4 + 1].spinnerFacing = AutoMapSouth;
  snapshot.cells[1 * 4 + 1].spinnerRotation = AutoMapSouth;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellSpinner;
  snapshot.cells[1 * 4 + 2].spinnerFacing = -1;
  snapshot.cells[1 * 4 + 2].spinnerRotation = 3;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> absolute = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(absolute.size() == 1);
  CHECK(absolute[0] == "SPIN: FACE S");
  std::vector<std::string> relative = Texts(AutoMapScene_TeleporterLines(drawList, 2, 1));
  CHECK(relative.size() == 1);
  CHECK(relative[0] == "SPIN: LEFT");
}

static void DormantSpinnerLinesNameHypotheticalSpin(void)
{
  // A dormant spinner names itself, then what it would do if switched on,
  // the way a dormant teleporter reads "DORMANT TELEPORTER" + "WOULD ...".
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellDormantSpinner;
  snapshot.cells[1 * 4 + 1].spinnerFacing = AutoMapSouth;
  snapshot.cells[1 * 4 + 1].spinnerRotation = AutoMapSouth;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDormantSpinner;
  snapshot.cells[1 * 4 + 2].spinnerFacing = -1;
  snapshot.cells[1 * 4 + 2].spinnerRotation = 3;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> absolute = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(absolute.size() == 1);
  CHECK(absolute[0] == "WOULD SPIN: FACE S");
  CHECK(Texts(AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName))[0] == "DORMANT SPINNER");
  std::vector<std::string> relative = Texts(AutoMapScene_TeleporterLines(drawList, 2, 1));
  CHECK(relative.size() == 1);
  CHECK(relative[0] == "WOULD SPIN: LEFT");
  CHECK(AutoMapScene_TeleporterLines(drawList, 2, 1)[0].style == AutoMapTipDormant);
}

static void DormantTeleporterLineNamesNoEffect(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellDormantTeleporter;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> lines = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "NO EFFECT");
}

static void PitLinesNameOpenAndHiddenState(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.width = 6;
  snapshot.cells.assign(18, AutoMapCellRecord());
  snapshot.cells[1 * 6 + 1].kind = AutoMapCellPit;
  snapshot.cells[1 * 6 + 1].pitIsHidden = true;
  snapshot.cells[1 * 6 + 1].pitIsOpen = true;
  snapshot.cells[1 * 6 + 2].kind = AutoMapCellPit;
  snapshot.cells[1 * 6 + 2].pitIsHidden = true;
  snapshot.cells[1 * 6 + 2].pitIsOpen = false;
  snapshot.cells[1 * 6 + 3].kind = AutoMapCellPit;
  snapshot.cells[1 * 6 + 3].pitIsHidden = false;
  snapshot.cells[1 * 6 + 3].pitIsOpen = true;
  snapshot.cells[1 * 6 + 4].kind = AutoMapCellPit;
  snapshot.cells[1 * 6 + 4].pitIsHidden = false;
  snapshot.cells[1 * 6 + 4].pitIsOpen = false;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<std::string> hiddenOpen = Texts(AutoMapScene_PitLines(drawList, 1, 1));
  CHECK(hiddenOpen.size() == 1);
  CHECK(hiddenOpen[0] == "OPEN");
  CHECK(Texts(AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName))[0] == "HIDDEN PIT");
  std::vector<std::string> hiddenClosed = Texts(AutoMapScene_PitLines(drawList, 2, 1));
  CHECK(hiddenClosed.size() == 1);
  CHECK(hiddenClosed[0] == "CLOSED");
  CHECK(Texts(AutoMapTooltip_Machinery(drawList, 2, 1, false, TestItemName))[0] == "HIDDEN PIT");
  std::vector<std::string> open = Texts(AutoMapScene_PitLines(drawList, 3, 1));
  CHECK(open.size() == 1);
  CHECK(open[0] == "OPEN");
  CHECK(Texts(AutoMapTooltip_Machinery(drawList, 3, 1, false, TestItemName))[0] == "PIT");
  std::vector<std::string> closed = Texts(AutoMapScene_PitLines(drawList, 4, 1));
  CHECK(closed.size() == 1);
  CHECK(closed[0] == "CLOSED");
  CHECK(Texts(AutoMapTooltip_Machinery(drawList, 4, 1, false, TestItemName))[0] == "PIT");
  CHECK(Texts(AutoMapScene_PitLines(drawList, 5, 1)).empty());
}

static bool CloseAngle(float a, float b)
{
  return fabs(a - b) < 1e-5f;
}

static void SpinnerGeometrySeparatesAbsoluteFromRelative(void)
{
  static const float PI = 3.14159265f;
  // Absolute spinners force their facing: full ring, radial head out
  AutoMapSpinnerGeometry north = AutoMapScene_SpinnerGeometry(AutoMapNorth, AutoMapNorth);
  CHECK(north.glyph == AutoMapSpinnerAbsolute);
  CHECK(CloseAngle(north.headAngle, PI / 2.0f));
  CHECK(CloseAngle(north.sweepAngle, 2.0f * PI));
  AutoMapSpinnerGeometry east = AutoMapScene_SpinnerGeometry(AutoMapEast, AutoMapEast);
  CHECK(east.glyph == AutoMapSpinnerAbsolute);
  CHECK(CloseAngle(east.headAngle, 0.0f));
  AutoMapSpinnerGeometry south = AutoMapScene_SpinnerGeometry(AutoMapSouth, AutoMapSouth);
  CHECK(south.glyph == AutoMapSpinnerAbsolute);
  CHECK(CloseAngle(south.headAngle, -PI / 2.0f));
  AutoMapSpinnerGeometry west = AutoMapScene_SpinnerGeometry(AutoMapWest, AutoMapWest);
  CHECK(west.glyph == AutoMapSpinnerAbsolute);
  CHECK(CloseAngle(west.headAngle, -PI));
  // Absolute wins when both fields are set: the facing is what the map shows
  AutoMapSpinnerGeometry forced = AutoMapScene_SpinnerGeometry(AutoMapEast, 3);
  CHECK(forced.glyph == AutoMapSpinnerAbsolute);
  CHECK(CloseAngle(forced.headAngle, 0.0f));
  // Relative turns share a north start so left/right mirror each other
  AutoMapSpinnerGeometry right = AutoMapScene_SpinnerGeometry(-1, 1);
  CHECK(right.glyph == AutoMapSpinnerRight);
  CHECK(CloseAngle(right.startAngle, PI / 2.0f));
  CHECK(CloseAngle(right.sweepAngle, -PI / 2.0f));
  CHECK(CloseAngle(right.headAngle, 0.0f));
  AutoMapSpinnerGeometry left = AutoMapScene_SpinnerGeometry(-1, 3);
  CHECK(left.glyph == AutoMapSpinnerLeft);
  CHECK(CloseAngle(left.startAngle, PI / 2.0f));
  CHECK(CloseAngle(left.sweepAngle, PI / 2.0f));
  CHECK(CloseAngle(left.headAngle, PI));
  CHECK(CloseAngle(right.sweepAngle, -left.sweepAngle));
  // Around is a half circle, twice the sweep of a quarter turn
  AutoMapSpinnerGeometry around = AutoMapScene_SpinnerGeometry(-1, 2);
  CHECK(around.glyph == AutoMapSpinnerAround);
  CHECK(CloseAngle(around.sweepAngle, PI));
  CHECK(CloseAngle(around.headAngle, PI / 2.0f + PI));
  // Straight keeps the facing: no arrow to follow
  AutoMapSpinnerGeometry straight = AutoMapScene_SpinnerGeometry(-1, 0);
  CHECK(straight.glyph == AutoMapSpinnerStraight);
  // Missing rotation stays on the old fallback mark
  AutoMapSpinnerGeometry unknown = AutoMapScene_SpinnerGeometry(-1, -1);
  CHECK(unknown.glyph == AutoMapSpinnerUnknown);
}

static AutoMapLevelSnapshot CorridorWithDormantTeleporter(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellDormantTeleporter;
  snapshot.cells[1 * 4 + 1].hasTeleporterTarget = true;
  snapshot.cells[1 * 4 + 1].teleporterDestLevel = LEVEL;
  snapshot.cells[1 * 4 + 1].teleporterDestX = 2;
  snapshot.cells[1 * 4 + 1].teleporterDestY = 1;
  snapshot.cells[1 * 4 + 1].teleporterDestGlobalX = 2;
  snapshot.cells[1 * 4 + 1].teleporterDestGlobalY = 1;
  snapshot.cells[1 * 4 + 1].teleporterScope = 1;
  snapshot.cells[1 * 4 + 1].teleporterRotation = 2;
  snapshot.cells[1 * 4 + 1].teleporterFacingMode = 0;
  return snapshot;
}

static void DormantTeleporterNamesWhatItWouldDo(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithDormantTeleporter(), NoFog(explored, visibleNow));
  std::vector<std::string> lines = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "WOULD TAKE: MONSTERS");
  CHECK(lines[1] == "WOULD SEND: L0 (2, 1)");
  CHECK(lines[2] == "WOULD TURN: AROUND");
}

static void DormantDestinationHidesWithItsCell(void)
{
  // The hypothetical scope and turn are local to the shown cell; the
  // destination coordinates follow the arrow's visibility rule.
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithDormantTeleporter(), FogOver(explored, visibleNow));
  std::vector<std::string> lines = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(lines.size() == 2);
  CHECK(lines[0] == "WOULD TAKE: MONSTERS");
  CHECK(lines[1] == "WOULD TURN: AROUND");
}

static void LiveDestinationHidesWithItsCell(void)
{
  // Same rule for firing teleporters: the DEST line must not name a cell
  // the map itself hides under fog.
  AutoMapLevelSnapshot snapshot = CorridorWithTeleporter();
  snapshot.cells[1 * 4 + 1].teleporterScope = 2;
  snapshot.cells[1 * 4 + 1].teleporterRotation = 1;
  snapshot.cells[1 * 4 + 1].teleporterFacingMode = 0;
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  std::vector<std::string> lines = Texts(AutoMapScene_TeleporterLines(drawList, 1, 1));
  CHECK(lines.size() == 2);
  CHECK(lines[0] == "TAKES: PARTY+OBJECTS");
  CHECK(lines[1] == "TURN: RIGHT");
}

static std::string JoinedTip(const std::vector<AutoMapTipLine> &lines)
{
  std::string text;
  for (size_t i = 0; i < lines.size(); i++) text += (i == 0 ? "" : "\n") + lines[i].text;
  return text;
}

static const AutoMapTipLine *TipLineAt(const std::vector<AutoMapTipLine> &lines,
                                      const std::string &text)
{
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (lines[i].text == text) return &lines[i];
  }
  return NULL;
}

// The user's L12 tile in small: a firing teleporter for monsters and a party pad on one cell,
// the pad wired straight to a generator
static AutoMapLevelSnapshot TeleporterOnAPartyPad(void)
{
  AutoMapLevelSnapshot snapshot = CorridorWithTeleporter();
  snapshot.cells[1 * 4 + 1].teleporterScope = 1;
  snapshot.cells[1 * 4 + 1].teleporterRotation = 0;
  snapshot.cells[1 * 4 + 1].teleporterFacingMode = 0;
  AutoMapFloorActuator pad = PadWithOp(1, 1, AutoMapPadParty);
  pad.record = 101;
  snapshot.actuators.push_back(pad);
  AutoMapFloorActuator generator(2, 1, AutoMapActuatorGenerator);
  generator.hasGeneratedMonster = true;
  generator.generatedMonsterName = "Mummy";
  generator.record = 102;
  snapshot.actuators.push_back(generator);
  AutoMapLink padToGenerator = {1, 1, 2, 1, false, -1};
  padToGenerator.source = 101;
  snapshot.links.push_back(padToGenerator);
  return snapshot;
}

static void MachineryGroupsTeleporterAndPad(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(TeleporterOnAPartyPad(), NoFog(explored, visibleNow));
  std::vector<AutoMapTipLine> lines =
      AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName);
  CHECK(JoinedTip(lines)
        == "TELEPORTER\n"
           "  TAKES: \x02\x02 MONSTERS\n"
           "  SENDS: L0 (2, 1)\n"
           "  TURN: STRAIGHT\n"
           "PAD\n"
           "  TRIGGER: \x01\x01 PARTY\n"
           "  RESULT:\n"
           "    GENERATOR L0 (2, 1) OPENS ON THIS PRESS");
  if (lines.size() != 8) return;
  CHECK(lines[0].style == AutoMapTipHeader);
  CHECK(lines[4].style == AutoMapTipHeader);
  CHECK(lines[7].style == AutoMapTipResult);
  for (size_t i = 0; i < lines.size(); i++) CHECK(lines[i].text.find("SHIFT") == std::string::npos);
}

// The user's L12 tile has several party pads, each wired to a generator: one PAD, one
// TRIGGER, every generator under one RESULT
static void SameTriggerPadsShareOnePadGroup(void)
{
  AutoMapLevelSnapshot snapshot = TeleporterOnAPartyPad();
  snapshot.width = 5;
  snapshot.cells.assign(15, AutoMapCellRecord());
  AutoMapLevelSnapshot narrow = TeleporterOnAPartyPad();
  for (int y = 0; y < 3; y++)
  {
    for (int x = 0; x < 4; x++) snapshot.cells[y * 5 + x] = narrow.cells[y * 4 + x];
  }
  snapshot.cells[1 * 5 + 3].kind = AutoMapCellFloor;
  AutoMapFloorActuator pad = PadWithOp(1, 1, AutoMapPadParty);
  pad.record = 103;
  snapshot.actuators.push_back(pad);
  AutoMapFloorActuator generator(3, 1, AutoMapActuatorGenerator);
  generator.record = 104;
  snapshot.actuators.push_back(generator);
  AutoMapLink padToGenerator = {1, 1, 3, 1, false, -1};
  padToGenerator.source = 103;
  snapshot.links.push_back(padToGenerator);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(JoinedTip(AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName))
        == "TELEPORTER\n"
           "  TAKES: \x02\x02 MONSTERS\n"
           "  SENDS: L0 (2, 1)\n"
           "  TURN: STRAIGHT\n"
           "PAD\n"
           "  TRIGGER: \x01\x01 PARTY\n"
           "  RESULT:\n"
           "    GENERATOR L0 (2, 1) OPENS ON THIS PRESS\n"
           "    GENERATOR L0 (3, 1) OPENS ON THIS PRESS");
}

static void DormantTeleporterKeepsItsWouldLines(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(CorridorWithDormantTeleporter(), NoFog(explored, visibleNow));
  std::vector<AutoMapTipLine> lines =
      AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName);
  CHECK(JoinedTip(lines)
        == "DORMANT TELEPORTER\n"
           "  WOULD TAKE: \x02\x02 MONSTERS\n"
           "  WOULD SEND: L0 (2, 1)\n"
           "  WOULD TURN: AROUND");
  CHECK(!lines.empty() && lines[0].style == AutoMapTipDormantHeader);
  for (size_t i = 1; i < lines.size(); i++)
  {
    CHECK(lines[i].text.compare(0, 8, "  WOULD ") == 0);
    CHECK(lines[i].style == AutoMapTipDormant);
  }
  // Without a record there is nothing it would do
  AutoMapLevelSnapshot bare = Corridor();
  bare.cells[1 * 4 + 1].kind = AutoMapCellDormantTeleporter;
  AutoMapDrawList bareList = AutoMapScene_BuildDrawList(bare, NoFog(explored, visibleNow));
  CHECK(JoinedTip(AutoMapTooltip_Machinery(bareList, 1, 1, false, TestItemName))
        == "DORMANT TELEPORTER\n  NO EFFECT");
}

static const char *MachineryItemName(int nameIndex)
{
  if (nameIndex == 3) return "DAGGER";
  if (nameIndex == 4) return "APPLE";
  return "";
}

static void TriggerIconsForEveryPadKind(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadParty));
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadMonster));
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadPartyOrMonster));
  AutoMapFloorActuator facing = PadWithOp(1, 1, AutoMapPadPartyFacing);
  facing.requiredFacing = AutoMapEast;
  snapshot.actuators.push_back(facing);
  AutoMapFloorActuator object = PadWithOp(1, 1, AutoMapPadObject);
  object.object = NamedItem(360, 0, 3);
  snapshot.actuators.push_back(object);
  AutoMapFloorActuator carried = PadWithOp(1, 1, AutoMapPadPossession);
  carried.object = NamedItem(361, 0, 4);
  snapshot.actuators.push_back(carried);
  snapshot.actuators.push_back(PadWithOp(1, 1, AutoMapPadAnything));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTipLine> lines =
      AutoMapTooltip_Machinery(drawList, 1, 1, false, MachineryItemName);
  std::string facingE = std::string("  TRIGGER: ") + AUTOMAP_GLYPH_FACING + '1' + " PARTY FACING E";
  std::string dagger = std::string("  TRIGGER: ") + AUTOMAP_GLYPH_ITEM + '0' + " OBJECT DAGGER";
  std::string apple = std::string("  TRIGGER: \x01\x01 CARRY ") + AUTOMAP_GLYPH_ITEM + '0' + " APPLE";
  CHECK(TipLineAt(lines, "  TRIGGER: \x01\x01 PARTY") != NULL);
  CHECK(TipLineAt(lines, "  TRIGGER: \x02\x02 MONSTER") != NULL);
  CHECK(TipLineAt(lines, "  TRIGGER: \x01\x01 PARTY \x02\x02 MONSTER") != NULL);
  CHECK(TipLineAt(lines, facingE) != NULL);
  CHECK(TipLineAt(lines, "  TRIGGER: ANYTHING") != NULL);
  const AutoMapTipLine *daggerLine = TipLineAt(lines, dagger);
  CHECK(daggerLine != NULL);
  if (daggerLine != NULL)
  {
    CHECK(daggerLine->items.size() == 1);
    CHECK(daggerLine->items.size() == 1 && daggerLine->items[0].worldPicture == 360);
  }
  const AutoMapTipLine *appleLine = TipLineAt(lines, apple);
  CHECK(appleLine != NULL);
  if (appleLine != NULL)
  {
    CHECK(appleLine->items.size() == 1 && appleLine->items[0].worldPicture == 361);
  }
  // One PAD header per pad actuator
  int pads = 0;
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (lines[i].text == "PAD")
    {
      pads++;
      CHECK(lines[i].style == AutoMapTipHeader);
    }
  }
  CHECK(pads == 7);
}

static void BelowAllTheMachineryShowsNoRouteOrRelays(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList =
      AutoMapScene_BuildDrawList(TeleporterOnAPartyPad(), NoFog(explored, visibleNow));
  drawList.spoilers = AutoMapSpoilersLearned;
  std::vector<AutoMapTipLine> lines =
      AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName);
  CHECK(!lines.empty() && lines[0].text == "TELEPORTER");
  for (size_t i = 0; i < lines.size(); i++)
  {
    CHECK(lines[i].text.find("RESULT:") == std::string::npos);
    CHECK(lines[i].text.find("ROUTE:") == std::string::npos);
    CHECK(lines[i].text.find("TRIGGER") == std::string::npos);
    CHECK(lines[i].text != "PAD");
    CHECK(lines[i].text.compare(0, 6, "FLOOR:") != 0);
    CHECK(lines[i].text.compare(0, 6, "STONE:") != 0);
  }
}

static void MachineryHeadsPitsDoorsAndGenerators(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellPit;
  snapshot.cells[1 * 4 + 1].pitIsHidden = true;
  snapshot.cells[1 * 4 + 1].pitIsOpen = false;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  snapshot.cells[1 * 4 + 2].doorState = AutoMapDoorClosed;
  snapshot.cells[1 * 4 + 2].doorBreakableWithAxe = true;
  snapshot.cells[1 * 4 + 2].doorBreakForce = 60;
  AutoMapFloorActuator generator(1, 1, AutoMapActuatorGenerator);
  generator.hasGeneratedMonster = true;
  generator.generatedMonsterName = "Mummy";
  generator.generatedCount = 3;
  snapshot.actuators.push_back(generator);
  snapshot.actuators.push_back(AutoMapFloorActuator(1, 1, AutoMapActuatorSpentGenerator));
  snapshot.actuators.push_back(AutoMapFloorActuator(1, 1, AutoMapActuatorDormantPad));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapTipLine> pit = AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName);
  CHECK(JoinedTip(pit)
        == "HIDDEN PIT\n  CLOSED\nDORMANT PAD\nGENERATOR\n  GENERATES 3X Mummy\nSPENT GENERATOR");
  const AutoMapTipLine *dormant = TipLineAt(pit, "DORMANT PAD");
  CHECK(dormant != NULL && dormant->style == AutoMapTipDormantHeader);
  CHECK(JoinedTip(AutoMapTooltip_Machinery(drawList, 2, 1, false, TestItemName))
        == "DOOR\n  BREAK FORCE: 60 (AXE)");
}

static const char *SearchItemName(int nameIndex)
{
  if (nameIndex == 1) return "RA KEY";
  if (nameIndex == 7) return "SKELETON KEY";
  if (nameIndex == 9) return "GOLD COIN";
  return "";
}

static AutoMapLevelSnapshot SearchCorridor(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].itemPictures.push_back(NamedItem(360, 0, 1));
  snapshot.cells[1 * 4 + 2].itemPictures.push_back(NamedItem(360, 1, 7));
  AutoMapWallLock lock;
  lock.side = AutoMapSouth;
  lock.object = NamedItem(360, 0, 1);
  lock.actuatorType = 3;
  lock.required = true;
  snapshot.cells[0 * 4 + 1].wallLocks.push_back(lock);
  return snapshot;
}

static void SearchFindsFloorItemsBySubstring(void)
{
  AutoMapLevelSnapshot snapshot = SearchCorridor();
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(drawList, "key", SearchItemName, NULL);
  CHECK(hits.size() == 3);
  std::vector<AutoMapSearchHit> exact =
      AutoMapScene_SearchItems(drawList, "RA KEY", SearchItemName, NULL);
  CHECK(exact.size() == 2);
  CHECK(exact[0].name == std::string("RA KEY"));
  std::vector<AutoMapSearchHit> lower =
      AutoMapScene_SearchItems(drawList, "ra key", SearchItemName, NULL);
  CHECK(lower.size() == 2);
  std::vector<AutoMapSearchHit> none =
      AutoMapScene_SearchItems(drawList, "dagger", SearchItemName, NULL);
  CHECK(none.empty());
  std::vector<AutoMapSearchHit> blank = AutoMapScene_SearchItems(drawList, "   ", SearchItemName, NULL);
  CHECK(blank.empty());
}

static void SearchRanksExactBeforeSubstring(void)
{
  AutoMapLevelSnapshot snapshot = SearchCorridor();
  snapshot.cells[1 * 4 + 1].itemPictures.push_back(NamedItem(361, 2, 9));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapSearchHit> hits =
      AutoMapScene_SearchItems(drawList, "GOLD COIN", SearchItemName, NULL);
  CHECK(hits.size() == 1);
  CHECK(hits[0].name == std::string("GOLD COIN"));
  std::vector<AutoMapSearchHit> sub = AutoMapScene_SearchItems(drawList, "COIN", SearchItemName, NULL);
  CHECK(sub.size() == 1);
}

static void SearchRespectsFog(void)
{
  AutoMapLevelSnapshot snapshot = SearchCorridor();
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(drawList, "key", SearchItemName, NULL);
  CHECK(hits.size() == 1);
  CHECK(hits[0].x == 1 && hits[0].y == 1);
}

static void SearchSkipsUnknownNames(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].itemPictures.push_back(ItemPicture(360));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(drawList, "key", SearchItemName, NULL);
  CHECK(hits.empty());
}

static void SearchHitsCarryTheirLevel(void)
{
  AutoMapLevelSnapshot snapshot = SearchCorridor();
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(drawList, "key", SearchItemName, NULL);
  CHECK(!hits.empty());
  for (size_t i = 0; i < hits.size(); i++) CHECK(hits[i].level == LEVEL);
}

static void SearchFindsMonsterPossessions(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapMonsterRecord carrier = MummyAt(1, 1);
  carrier.possessionPictures.push_back(NamedItem(361, 2, 9));
  carrier.possessionTexts.push_back("");
  snapshot.monsters.push_back(carrier);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(drawList, "coin", SearchItemName, NULL);
  CHECK(hits.size() == 1);
  CHECK(hits[0].monsterCarried);
  CHECK(hits[0].x == 1 && hits[0].y == 1);
  CHECK(hits[0].name == std::string("GOLD COIN"));
}

static void SearchPossessionsHideWithTheirMonster(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapMonsterRecord carrier = MummyAt(1, 1);
  carrier.possessionPictures.push_back(NamedItem(361, 2, 9));
  carrier.possessionTexts.push_back("");
  snapshot.monsters.push_back(carrier);
  AutoMapExplored explored = NothingExplored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapVisibleCells hidden;
  AutoMapDrawList unseen = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, hidden));
  CHECK(AutoMapScene_SearchItems(unseen, "coin", SearchItemName, NULL).empty());
  AutoMapVisibleCells seen;
  seen.Add(1, 1);
  AutoMapDrawList shown = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, seen));
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(shown, "coin", SearchItemName, NULL);
  CHECK(hits.size() == 1);
  CHECK(hits[0].monsterCarried);
}

static void SearchRankIsShared(void)
{
  CHECK(AutoMapScene_SearchRank("RA KEY", "RA KEY") == 0);
  CHECK(AutoMapScene_SearchRank("RA KEY", "ra") == 1);
  CHECK(AutoMapScene_SearchRank("SKELETON KEY", "key") == 2);
  CHECK(AutoMapScene_SearchRank("RA KEY", "dagger") == -1);
  CHECK(AutoMapScene_SearchRank("RA KEY", "   ") == -1);
  CHECK(AutoMapScene_SearchRank("(UNKNOWN)", "unknown") == -1);
  CHECK(AutoMapScene_SearchRank("", "key") == -1);
}

static bool HasShownCell(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.cells.size(); i++)
  {
    if (drawList.cells[i].x == x && drawList.cells[i].y == y) return true;
  }
  return false;
}

// The reader grows the snapshot where the engine fabricates a boundary wall (an open edge),
// shifting the real cells inward. The fog still remembers the level's own cells and the
// rulers still show global coordinates, so a shown cell must map back to its level cell.
static void BoundaryMarginKeepsFogAndGlobalsAligned(void)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 5;
  snapshot.height = 3;
  snapshot.boundaryMarginX = 1;  // an open west edge; the solid east/north/south get none
  snapshot.boundaryMarginY = 0;
  snapshot.offsetX = -1;
  snapshot.offsetY = 0;
  snapshot.cells.assign(15, AutoMapCellRecord());
  snapshot.cells[1 * 5 + 0].kind = AutoMapCellWall;   // local (-1,1): fabricated west wall
  snapshot.cells[1 * 5 + 0].isOutsideLevel = true;
  snapshot.cells[1 * 5 + 0].wallSides[AutoMapEast].picture = FOUNTAIN_PICTURE;
  snapshot.cells[1 * 5 + 1].kind = AutoMapCellWall;   // local (0,1)
  snapshot.cells[1 * 5 + 2].kind = AutoMapCellFloor;  // local (1,1)
  snapshot.party.x = 2;                               // local (1,1)
  snapshot.party.y = 1;

  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;

  // Fog off: the boundary wall is a cell like any other, drawn with its fountain, and its
  // global position matches the level coordinate it stands for.
  AutoMapDrawList unfogged = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(HasShownCell(unfogged, 0, 1));
  for (size_t i = 0; i < unfogged.cells.size(); i++)
  {
    const AutoMapShownCell &shown = unfogged.cells[i];
    CHECK(shown.cell.isOutsideLevel == (shown.x == 0 && shown.y == 1));
  }
  std::vector<std::string> rock = Texts(AutoMapScene_RelayLines(unfogged, 0, 1));
  CHECK(rock.size() == 1);
  CHECK(rock[0] == "ADDED ROCK: OUTSIDE THE LEVEL");
  CHECK(Texts(AutoMapScene_RelayLines(unfogged, 1, 1)).empty());
  int globalX = 0, globalY = 0;
  AutoMapScene_GlobalPosition(unfogged, 0, 1, &globalX, &globalY);
  CHECK(globalX == -1 && globalY == 1);
  AutoMapScene_GlobalPosition(unfogged, 2, 1, &globalX, &globalY);
  CHECK(globalX == 1 && globalY == 1);

  // Fog on: exploration is recalled in the level's own coordinates, so the two real cells
  // survive shifted and the fabricated wall (outside the level) stays hidden.
  explored.Mark(AutoMapCell{LEVEL, 0, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapDrawList fogged = AutoMapScene_BuildDrawList(snapshot, FogOver(explored, visibleNow));
  CHECK(!HasShownCell(fogged, 0, 1));
  CHECK(HasShownCell(fogged, 1, 1));
  CHECK(HasShownCell(fogged, 2, 1));
  CHECK(!HasShownCell(fogged, 3, 1));
}

// Debug placement (map_debug): only cells the party could stand on take the party
// or a new monster
static void PlacementTakesAnEmptyFloor(void)
{
  AutoMapLevelSnapshot level = Corridor();
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementFree);
}

static void PlacementRefusesWallsAndTheEdgeOfTheLevel(void)
{
  AutoMapLevelSnapshot level = Corridor();
  CHECK(AutoMapScene_Placement(level, 0, 0) == AutoMapPlacementSolid);
  CHECK(AutoMapScene_Placement(level, -1, 1) == AutoMapPlacementSolid);
  CHECK(AutoMapScene_Placement(level, 4, 1) == AutoMapPlacementSolid);
  level.cells[1 * 4 + 2].isOutsideLevel = true;
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementSolid);
}

static void PlacementRefusesAClosedFalseWallButTakesAnOpenOne(void)
{
  AutoMapLevelSnapshot level = Corridor();
  level.cells[1 * 4 + 2].kind = AutoMapCellFalseWall;
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementSolid);
  level.cells[1 * 4 + 2].kind = AutoMapCellOpenFalseWall;
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementFree);
}

// An imaginary wall is drawn as stone but walked through, so the party can be put there
static void PlacementTakesAFalseWallThePartyWalksThrough(void)
{
  AutoMapLevelSnapshot level = Corridor();
  level.cells[1 * 4 + 2].kind = AutoMapCellFalseWall;
  level.cells[1 * 4 + 2].falseWallIsPassable = true;
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementFree);
}

static void PlacementTakesADoorOnlyWhenItLetsThePartyThrough(void)
{
  AutoMapLevelSnapshot level = Corridor();
  level.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  level.cells[1 * 4 + 2].doorState = AutoMapDoorClosed;
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementSolid);
  level.cells[1 * 4 + 2].doorState = AutoMapDoorHalfOpen;
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementSolid);
  level.cells[1 * 4 + 2].doorState = AutoMapDoorOpen;
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementFree);
  level.cells[1 * 4 + 2].doorState = AutoMapDoorDestroyed;
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementFree);
}

static void PlacementRefusesACellWithAMonster(void)
{
  AutoMapLevelSnapshot level = Corridor();
  level.monsters.push_back(MummyAt(2, 1));
  CHECK(AutoMapScene_Placement(level, 2, 1) == AutoMapPlacementMonster);
}

static void PlacementRefusesThePartyCellOnlyOnThePartyLevel(void)
{
  AutoMapLevelSnapshot level = Corridor();
  CHECK(AutoMapScene_Placement(level, 1, 1) == AutoMapPlacementParty);
  level.partyIsOnShownLevel = false;
  CHECK(AutoMapScene_Placement(level, 1, 1) == AutoMapPlacementFree);
}

static void PlacementNamesWhyACellIsRefused(void)
{
  CHECK(std::string(AutoMapScene_DescribePlacement(AutoMapPlacementFree)) == "");
  CHECK(std::string(AutoMapScene_DescribePlacement(AutoMapPlacementSolid)) == "SOLID");
  CHECK(std::string(AutoMapScene_DescribePlacement(AutoMapPlacementMonster)) == "MONSTER");
  CHECK(std::string(AutoMapScene_DescribePlacement(AutoMapPlacementParty)) == "PARTY");
}

// Debug level-up (map_debug): the level of a base skill as DetermineMastery counts it
// without temporary adjustments or possessions, and the experience the next one needs
static void SkillLevelDoublesItsExperienceForEveryLevel(void)
{
  CHECK(AutoMapScene_SkillLevel(0) == 1);
  CHECK(AutoMapScene_SkillLevel(499) == 1);
  CHECK(AutoMapScene_SkillLevel(500) == 2);
  CHECK(AutoMapScene_SkillLevel(999) == 2);
  CHECK(AutoMapScene_SkillLevel(1000) == 3);
  CHECK(AutoMapScene_SkillLevel(500 << 10) == 12);
}

static void ExperienceToNextLevelReachesExactlyTheNextLevel(void)
{
  CHECK(AutoMapScene_ExperienceToNextLevel(0) == 500);
  CHECK(AutoMapScene_ExperienceToNextLevel(499) == 1);
  CHECK(AutoMapScene_ExperienceToNextLevel(500) == 500);
  CHECK(AutoMapScene_ExperienceToNextLevel(1200) == 800);
  int experience = 300000;
  int level = AutoMapScene_SkillLevel(experience);
  CHECK(AutoMapScene_SkillLevel(experience + AutoMapScene_ExperienceToNextLevel(experience))
        == level + 1);
  CHECK(AutoMapScene_SkillLevel(experience + AutoMapScene_ExperienceToNextLevel(experience) - 1)
        == level);
}

// The engine caps experience at 0x10000000 (LimitSkillExperience): no level past it
static void ExperienceToNextLevelIsZeroWhenTheCapForbidsIt(void)
{
  CHECK(AutoMapScene_ExperienceToNextLevel(500 << 18) == (500 << 19) - (500 << 18));
  CHECK(AutoMapScene_ExperienceToNextLevel(500 << 19) == 0);
  CHECK(AutoMapScene_ExperienceToNextLevel(AUTOMAP_SKILL_EXPERIENCE_CAP) == 0);
}

// Every record the snapshot names starts out as "no record", and every list empty
static void RecordIdsDefaultToNone(void)
{
  CHECK(AutoMapLink().source == AUTOMAP_NO_RECORD);
  CHECK(AutoMapFloorActuator().record == AUTOMAP_NO_RECORD);
  CHECK(AutoMapWallLock().actuator == AUTOMAP_NO_RECORD);
  CHECK(AutoMapLauncher().actuator == AUTOMAP_NO_RECORD);
  CHECK(AutoMapAlcoveEntry().scroll == AUTOMAP_NO_RECORD);
  CHECK(AutoMapChestContents().chest == AUTOMAP_NO_RECORD);
  CHECK(AutoMapChestContents().pictureScrolls.empty());
  CHECK(AutoMapWallSide().actuators.empty());
  CHECK(AutoMapCellRecord().scrollRecords.empty());
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    CHECK(AutoMapCellRecord().wallSides[side].actuators.empty());
  }
}

static void CellLooksChangedOnDoorPitWallPicture(void)
{
  AutoMapCellRecord before;
  before.kind = AutoMapCellDoor;
  before.doorState = AutoMapDoorAlmostClosed;
  AutoMapCellRecord after = before;
  CHECK(!AutoMapScene_CellLooksChanged(before, after));
  after.doorState = AutoMapDoorHalfOpen;
  CHECK(AutoMapScene_CellLooksChanged(before, after));
  after = before;
  after.pitIsOpen = !before.pitIsOpen;
  CHECK(AutoMapScene_CellLooksChanged(before, after));
  after = before;
  after.kind = AutoMapCellFloor;
  CHECK(AutoMapScene_CellLooksChanged(before, after));
  after = before;
  after.wallSides[2].picture = LEVER_PICTURE;
  CHECK(AutoMapScene_CellLooksChanged(before, after));
  after = before;
  after.floorPicture = 77;
  CHECK(AutoMapScene_CellLooksChanged(before, after));
  // Things lying on the cell are not the cell
  after = before;
  after.itemPictures.push_back(AutoMapItem());
  CHECK(!AutoMapScene_CellLooksChanged(before, after));
}

static void LookOfReadsTheLookFields(void)
{
  AutoMapCellRecord record;
  record.kind = AutoMapCellDoor;
  record.doorState = AutoMapDoorHalfOpen;
  record.pitIsOpen = true;
  record.pitIsHidden = true;
  record.spinnerIsVisible = true;
  record.floorPicture = 77;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) record.wallSides[side].picture = 100 + side;
  AutoMapCellLook look = AutoMapScene_LookOf(record);
  CHECK(look.spinnerIsVisible);
  CHECK(look.kind == AutoMapCellDoor);
  CHECK(look.doorState == AutoMapDoorHalfOpen);
  CHECK(look.pitIsOpen);
  CHECK(look.pitIsHidden);
  CHECK(look.floorPicture == 77);
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) CHECK(look.sidePictures[side] == 100 + side);
}

static void ApplyLookOverwritesOnlyThose(void)
{
  AutoMapCellRecord source;
  source.kind = AutoMapCellDoor;
  source.doorState = AutoMapDoorClosed;
  source.pitIsHidden = true;
  source.spinnerIsVisible = true;
  source.floorPicture = 5;
  source.wallSides[2].picture = 9;
  AutoMapCellRecord target;
  target.itemPictures.push_back(AutoMapItem());
  target.wallSides[2].actuators.push_back(AutoMapRecordId());
  AutoMapScene_ApplyLook(AutoMapScene_LookOf(source), &target);
  CHECK(!AutoMapScene_LooksDiffer(AutoMapScene_LookOf(source), AutoMapScene_LookOf(target)));
  CHECK(target.spinnerIsVisible);
  CHECK(target.itemPictures.size() == 1);
  CHECK(target.wallSides[2].actuators.size() == 1);
}

static void CellLooksChangedSeesConcealment(void)
{
  AutoMapCellRecord before;
  AutoMapCellRecord after = before;
  after.pitIsHidden = !before.pitIsHidden;
  CHECK(AutoMapScene_CellLooksChanged(before, after));
}

static AutoMapCellRecord ConcealedPit(bool isOpen)
{
  AutoMapCellRecord pit;
  pit.kind = AutoMapCellPit;
  pit.pitIsHidden = true;
  pit.pitIsOpen = isOpen;
  pit.floorPicture = 77;
  return pit;
}

static void VisibleLookKeepsAnUnknownConcealedPitsState(void)
{
  AutoMapCellLook seen = AutoMapScene_LookOf(ConcealedPit(true));
  AutoMapCellLook remembered = AutoMapScene_LookOf(ConcealedPit(false));
  CHECK(!AutoMapScene_VisibleLook(seen, &remembered, false).pitIsOpen);
  CHECK(AutoMapScene_VisibleLook(seen, &remembered, true).pitIsOpen);
  // Everything else of the sighting is taken
  seen.floorPicture = 78;
  CHECK(AutoMapScene_VisibleLook(seen, &remembered, false).floorPicture == 78);
  seen.floorPicture = 77;
  AutoMapCellRecord plainPit = ConcealedPit(true);
  plainPit.pitIsHidden = false;
  AutoMapCellLook plainSeen = AutoMapScene_LookOf(plainPit);
  CHECK(AutoMapScene_VisibleLook(plainSeen, &remembered, false).pitIsOpen);
  CHECK(!AutoMapScene_LooksDiffer(AutoMapScene_VisibleLook(seen, NULL, false), seen));
}

static void SeenToChangeIgnoresAnUnknownConcealedPit(void)
{
  CHECK(!AutoMapScene_SeenToChange(ConcealedPit(false), ConcealedPit(true), false));
  CHECK(!AutoMapScene_SeenToChange(ConcealedPit(true), ConcealedPit(false), false));
  CHECK(AutoMapScene_SeenToChange(ConcealedPit(false), ConcealedPit(true), true));
  // Another change of a concealed pit still shows
  AutoMapCellRecord repainted = ConcealedPit(true);
  repainted.floorPicture = 78;
  CHECK(AutoMapScene_SeenToChange(ConcealedPit(false), repainted, false));
  AutoMapCellRecord door;
  door.kind = AutoMapCellDoor;
  door.doorState = AutoMapDoorClosed;
  AutoMapCellRecord openDoor = door;
  openDoor.doorState = AutoMapDoorOpen;
  CHECK(AutoMapScene_SeenToChange(door, openDoor, false));
  CHECK(!AutoMapScene_SeenToChange(door, door, true));
}

// The quadrants (0 north-west, 1 north-east, 2 south-east, 3 south-west) a half-cell monster
// at position fills when facing, the rule of OrdinalOfMonsterAtPosition (Code11f52.cpp)
static bool HalfCellMonsterFills(int position, int facing, int quadrant)
{
  int end = quadrant;
  if ((facing & 1) == (quadrant & 1)) end = (end - 1) & 3;
  return position == end || position == ((end + 1) & 3);
}

static const AutoMapSpawnSpot *SpotNamed(const std::vector<AutoMapSpawnSpot> &spots, const char *name)
{
  for (size_t i = 0; i < spots.size(); i++)
  {
    if (spots[i].name == name) return &spots[i];
  }
  return NULL;
}

static void AQuarterCellMonsterCanTakeAnyQuadrantOrTheCentre(void)
{
  std::vector<AutoMapSpawnSpot> spots = AutoMapScene_SpawnSpots(0);
  CHECK(spots.size() == 5);
  CHECK(SpotNamed(spots, "CENTRE") != NULL && SpotNamed(spots, "CENTRE")->positions == 255);
  CHECK(SpotNamed(spots, "NORTH WEST") != NULL && SpotNamed(spots, "NORTH WEST")->positions == 0);
  CHECK(SpotNamed(spots, "NORTH EAST") != NULL && SpotNamed(spots, "NORTH EAST")->positions == 1);
  CHECK(SpotNamed(spots, "SOUTH EAST") != NULL && SpotNamed(spots, "SOUTH EAST")->positions == 2);
  CHECK(SpotNamed(spots, "SOUTH WEST") != NULL && SpotNamed(spots, "SOUTH WEST")->positions == 3);
  for (size_t i = 0; i < spots.size(); i++) CHECK(spots[i].facings.size() == 4);
}

// A half-cell monster lies along its facing: in the west or east half it faces north or south
static void AHalfCellMonsterFillsTheHalfItIsSpawnedIn(void)
{
  std::vector<AutoMapSpawnSpot> spots = AutoMapScene_SpawnSpots(1);
  CHECK(spots.size() == 5);
  const char *names[4] = {"NORTH HALF", "EAST HALF", "SOUTH HALF", "WEST HALF"};
  // The two quadrants of each half
  const int quadrants[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
  for (int half = 0; half < 4; half++)
  {
    const AutoMapSpawnSpot *spot = SpotNamed(spots, names[half]);
    CHECK(spot != NULL);
    if (spot == NULL) continue;
    CHECK(spot->facings.size() == 2);
    for (size_t f = 0; f < spot->facings.size(); f++)
    {
      for (int quadrant = 0; quadrant < 4; quadrant++)
      {
        bool inHalf = quadrant == quadrants[half][0] || quadrant == quadrants[half][1];
        CHECK(HalfCellMonsterFills(spot->positions, spot->facings[f], quadrant) == inHalf);
      }
    }
  }
  CHECK(SpotNamed(spots, "CENTRE") != NULL && SpotNamed(spots, "CENTRE")->facings.size() == 4);
}

static void AWholeCellMonsterStandsInTheCentre(void)
{
  std::vector<AutoMapSpawnSpot> spots = AutoMapScene_SpawnSpots(2);
  CHECK(spots.size() == 1 && spots[0].positions == 255 && spots[0].facings.size() == 4);
}

// Four of a quarter cell fill it, two of half of it, one of all of it (CreateMonster)
static void AsManyMonstersAsFitShareACell(void)
{
  CHECK(AutoMapScene_LargestGroup(0) == 4);
  CHECK(AutoMapScene_LargestGroup(1) == 2);
  CHECK(AutoMapScene_LargestGroup(2) == 1);
}

// A machine room of stone, with a lever on the east face of (0,1) looking at the
// floor (1,1), relays to be put in the stone cells around and a door at (5,1):
//
//        x=0 x=1 x=2 x=3 x=4 x=5
//   y=0   #   #   #   #   #   #
//   y=1   L   .   #   #   #   D      L: lever on the east side, D: door
//   y=2   #   #   #   #   #   #
static const AutoMapRecordId LEVER_RECORD = 100;

static AutoMapLevelSnapshot MachineRoom(void)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 6;
  snapshot.height = 3;
  snapshot.cells.assign(18, AutoMapCellRecord());
  snapshot.cells[1 * 6 + 1].kind = AutoMapCellFloor;
  snapshot.cells[1 * 6 + 5].kind = AutoMapCellDoor;
  snapshot.cells[1 * 6 + 0].wallSides[AutoMapEast].picture = LEVER_PICTURE;
  snapshot.party.x = 1;
  snapshot.party.y = 1;
  snapshot.party.facing = AutoMapWest;
  return snapshot;
}

// A relay record in stone. Its faces carry hidden machinery, which is what lets the
// links it sends survive the draw-list filter, as the reader marks them.
static void PutRelay(AutoMapLevelSnapshot *snapshot, int x, int y, AutoMapRelay relay,
                     AutoMapRecordId record)
{
  relay.record = record;
  AutoMapCellRecord &cell = snapshot->cells[y * snapshot->width + x];
  cell.wallRelays.push_back(relay);
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) cell.wallSides[side].hidden = true;
}

static AutoMapLink Message(int srcX, int srcY, int dstX, int dstY, AutoMapRecordId source,
                           AutoMapSignal signal, int targetPos, int delay)
{
  bool fromLever = source == LEVER_RECORD;
  AutoMapLink link = {srcX, srcY, dstX, dstY, true, fromLever ? AutoMapEast : AutoMapNorth};
  link.signal = signal;
  link.targetPos = targetPos;
  link.delay = delay;
  link.source = source;
  return link;
}

static std::vector<AutoMapPressSource> PressLever(const AutoMapLevelSnapshot &snapshot)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  return AutoMapScene_PressEffects(drawList, 0, 1);
}

static void PressEffectsReachEveryCounterInTheCell(void)
{
  // The DM L5 (18,28) shape: one lever message reaches both counters of the cell,
  // not just the first.
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(4), 200);
  PutRelay(&snapshot, 2, 0, CounterRelay(3), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  CHECK(sources[0].source == LEVER_RECORD);
  CHECK(sources[0].srcIsWall && sources[0].srcSide == AutoMapEast);
  CHECK(!sources[0].truncated);
  const std::vector<AutoMapEffect> &effects = sources[0].effects;
  CHECK(effects.size() == 2);
  if (effects.size() != 2) return;
  CHECK(effects[0].kind == AutoMapEffectCounter && effects[1].kind == AutoMapEffectCounter);
  CHECK(effects[0].x == 2 && effects[0].y == 0 && effects[0].relay == 0 && !effects[0].floorRelay);
  CHECK(effects[1].relay == 1);
  CHECK(effects[0].before == 4 && effects[0].after == 3);
  CHECK(effects[1].before == 3 && effects[1].after == 2);
  CHECK(!effects[0].fires && !effects[1].fires);
  CHECK(!effects[0].later && !effects[1].later);
  CHECK(effects[0].via.source == LEVER_RECORD);
}

static void PressEffectsFollowACounterThatReachesZero(void)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(1), 200);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 2));
  snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 3));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1 || sources[0].effects.size() != 1) return;
  const AutoMapEffect &counter = sources[0].effects[0];
  CHECK(counter.kind == AutoMapEffectCounter);
  CHECK(counter.after == 0 && counter.fires && !counter.later);
  CHECK(counter.delayTicks == 2);
  CHECK(counter.next.size() == 1);
  if (counter.next.size() != 1) return;
  const AutoMapEffect &door = counter.next[0];
  CHECK(door.kind == AutoMapEffectGoal);
  CHECK(door.x == 5 && door.y == 1 && door.relay == -1);
  CHECK(!door.later);
  CHECK(door.delayTicks == 5);
  CHECK(door.via.source == 200 && door.via.signal == AutoMapSignalSet);
  // The door starts open, so opening it again changes nothing
  CHECK(door.distance.status == AutoMapGoalAlreadyDone);
}

static void PressEffectsWalkLaterFirings(void)
{
  // The counter still needs 3 more presses, so its SET on gate bit 2 arrives only later:
  // 1011 then reads 1111 and matches the pattern.
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(4), 200);
  PutRelay(&snapshot, 3, 0, GateRelay(0xB | (0xF << 4), false), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalSet, 2, 0));
  snapshot.links.push_back(Message(3, 0, 5, 1, 201, AutoMapSignalSet, 0, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1 || sources[0].effects.size() != 1) return;
  const AutoMapEffect &counter = sources[0].effects[0];
  CHECK(counter.before == 4 && counter.after == 3 && !counter.fires && !counter.later);
  CHECK(counter.next.size() == 1);
  if (counter.next.size() != 1) return;
  const AutoMapEffect &gate = counter.next[0];
  CHECK(gate.kind == AutoMapEffectGate);
  CHECK(gate.x == 3 && gate.y == 0 && gate.relay == 0);
  CHECK(gate.later);
  CHECK(gate.before == 0xB && gate.after == 0xF);
  CHECK(gate.fires);
  CHECK(gate.next.size() == 1);
  if (gate.next.size() != 1) return;
  CHECK(gate.next[0].kind == AutoMapEffectGoal && gate.next[0].later);
}

// The effect of one lever message into a lone gate at (2,0)
static AutoMapEffect GateMessage(AutoMapRelay gate, AutoMapSignal signal, int bit)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, gate, 200);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, signal, bit, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  if (sources.size() != 1 || sources[0].effects.size() != 1) return AutoMapEffect();
  return sources[0].effects[0];
}

static void PressEffectsGateHonoursEdgeAndNormallyClosed(void)
{
  // Output change only: the bits already match and still do, so nothing fires
  AutoMapRelay outputChange = GateRelay(0xF | (0xF << 4), false);
  outputChange.edgeMode = 2;
  AutoMapEffect same = GateMessage(outputChange, AutoMapSignalSet, 0);
  CHECK(same.kind == AutoMapEffectGate && same.before == 0xF && same.after == 0xF);
  CHECK(!same.fires);
  // A normally-closed gate inverts the match
  AutoMapRelay closed = GateRelay(0xB | (0xF << 4), false);
  closed.normallyClosed = true;
  AutoMapEffect inverted = GateMessage(closed, AutoMapSignalSet, 2);
  CHECK(inverted.kind == AutoMapEffectGate && inverted.after == 0xF);
  CHECK(!inverted.fires);
  // SET messages only: a CLEAR reaching the match does not count
  AutoMapRelay setOnly = GateRelay(0xF | (0xB << 4), false);
  setOnly.edgeMode = 4;
  AutoMapEffect cleared = GateMessage(setOnly, AutoMapSignalClear, 2);
  CHECK(cleared.kind == AutoMapEffectGate && cleared.after == 0xB);
  CHECK(!cleared.fires);
  // The same CLEAR into a plain gate fires
  AutoMapEffect plain = GateMessage(GateRelay(0xF | (0xB << 4), false), AutoMapSignalClear, 2);
  CHECK(plain.after == 0xB && plain.fires);
  // A TOGGLE flips the bit
  AutoMapEffect toggled = GateMessage(GateRelay(0x1 | (0x3 << 4), false), AutoMapSignalToggle, 1);
  CHECK(toggled.before == 0x1 && toggled.after == 0x3 && toggled.fires);
}

// The effect of one lever message into a lone counter at (2,0)
static AutoMapEffect CounterMessage(AutoMapRelay counter, AutoMapSignal signal)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, counter, 200);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, signal, 0, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  if (sources.size() != 1 || sources[0].effects.size() != 1) return AutoMapEffect();
  return sources[0].effects[0];
}

static void PressEffectsCounterRules(void)
{
  // A counter at 0 never changes again
  AutoMapEffect dead = CounterMessage(CounterRelay(0), AutoMapSignalClear);
  CHECK(dead.kind == AutoMapEffectCounter && dead.before == 0 && dead.after == 0 && !dead.fires);
  // SET counts up, up to 511
  AutoMapEffect full = CounterMessage(CounterRelay(511), AutoMapSignalSet);
  CHECK(full.before == 511 && full.after == 511 && !full.fires);
  AutoMapEffect up = CounterMessage(CounterRelay(3), AutoMapSignalSet);
  CHECK(up.before == 3 && up.after == 4 && !up.fires);
  AutoMapEffect toggled = CounterMessage(CounterRelay(3), AutoMapSignalToggle);
  CHECK(toggled.after == 2 && !toggled.fires);
  // A CONSTPRESSURE counter reports its state on every change
  AutoMapRelay pressure = CounterRelay(3);
  pressure.signal = AutoMapSignalPressure;
  AutoMapEffect held = CounterMessage(pressure, AutoMapSignalClear);
  CHECK(held.before == 3 && held.after == 2 && held.fires);
}

static void PressEffectsPressureSourceSendsSet(void)
{
  // A CONSTPRESSURE lever reports its state, and a press closes it: SET, which a
  // counter counts up
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(3), 200);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalPressure, 0, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  CHECK(sources[0].effects.size() == 1);
  if (sources[0].effects.size() != 1) return;
  const AutoMapEffect &counter = sources[0].effects[0];
  CHECK(counter.kind == AutoMapEffectCounter);
  CHECK(counter.before == 3 && counter.after == 4 && !counter.fires);
}

static void PressEffectsNormallyClosedPressureSourceSendsClear(void)
{
  // DM L5's alcoves: a normally-closed CONSTPRESSURE source opens when pressed (an item put
  // in), and an open one reports CLEAR (PressurePadOrAlcove: switchClosed = pressed ^ NC), so
  // the counter counts down
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(3), 200);
  AutoMapLink alcove = Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalPressure, 0, 0);
  alcove.sourceNormallyClosed = true;
  snapshot.links.push_back(alcove);
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  CHECK(sources[0].effects.size() == 1);
  if (sources[0].effects.size() != 1) return;
  const AutoMapEffect &counter = sources[0].effects[0];
  CHECK(counter.kind == AutoMapEffectCounter);
  CHECK(counter.before == 3 && counter.after == 2 && !counter.fires);
  // A normally-closed source with a plain action keeps its action
  snapshot.links.back().signal = AutoMapSignalSet;
  sources = PressLever(snapshot);
  CHECK(sources.size() == 1 && sources[0].effects.size() == 1
        && sources[0].effects[0].after == 4);
}

static void PressEffectsMarkReceiversAfterADSA(void)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  AutoMapRelay dsa;
  dsa.kind = AutoMapRelayDSA;
  PutRelay(&snapshot, 2, 0, dsa, 200);
  PutRelay(&snapshot, 2, 0, CounterRelay(3), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  // What the DSA sends is not simulated, so it is not followed
  snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1 || sources[0].effects.size() != 2) return;
  const AutoMapEffect &first = sources[0].effects[0];
  const AutoMapEffect &second = sources[0].effects[1];
  CHECK(first.kind == AutoMapEffectDSA && first.relay == 0 && first.next.empty());
  CHECK(!first.afterDSA);
  CHECK(second.kind == AutoMapEffectCounter && second.relay == 1 && second.afterDSA);
  CHECK(second.before == 3 && second.after == 2);
}

static void PressEffectsStopAtLoops(void)
{
  // The gate matches and clears the counter to 0, which toggles the gate again: the
  // second arrival at the gate ends the walk instead of going round for ever.
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, GateRelay(0x0 | (0x1 << 4), false), 200);
  PutRelay(&snapshot, 3, 0, CounterRelay(1), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalSet, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(3, 0, 2, 0, 201, AutoMapSignalToggle, 0, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1 || sources[0].effects.size() != 1) return;
  const AutoMapEffect &gate = sources[0].effects[0];
  CHECK(gate.kind == AutoMapEffectGate && gate.fires);
  CHECK(gate.next.size() == 1);
  if (gate.next.size() != 1) return;
  const AutoMapEffect &counter = gate.next[0];
  CHECK(counter.kind == AutoMapEffectCounter && counter.after == 0 && counter.fires);
  CHECK(counter.next.size() == 1);
  if (counter.next.size() != 1) return;
  const AutoMapEffect &loop = counter.next[0];
  CHECK(loop.kind == AutoMapEffectLoop);
  // The loop keeps the relay it comes back to, so the route can name it
  CHECK(loop.x == 2 && loop.y == 0 && loop.relay == 0 && !loop.floorRelay);
  CHECK(loop.next.empty());
  CHECK(loop.via.source == 201);
}

static void PressEffectsReachARelayTwiceWithoutALoop(void)
{
  // Two counters both feed the gate, one bit each: the second message is no loop, and
  // it starts from the bits the first one left.
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(1), 200);
  PutRelay(&snapshot, 2, 0, CounterRelay(1), 201);
  PutRelay(&snapshot, 3, 0, GateRelay(0x0 | (0x3 << 4), false), 202);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalSet, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 201, AutoMapSignalSet, 1, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1 || sources[0].effects.size() != 2) return;
  const AutoMapEffect &first = sources[0].effects[0];
  const AutoMapEffect &second = sources[0].effects[1];
  CHECK(first.next.size() == 1 && second.next.size() == 1);
  if (first.next.size() != 1 || second.next.size() != 1) return;
  CHECK(first.next[0].kind == AutoMapEffectGate);
  CHECK(first.next[0].before == 0x0 && first.next[0].after == 0x1 && !first.next[0].fires);
  CHECK(second.next[0].kind == AutoMapEffectGate);
  CHECK(second.next[0].before == 0x1 && second.next[0].after == 0x3 && second.next[0].fires);
}

static void PressEffectsStaySilentUnderFogAndBelowAll(void)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(1), 200);
  PutRelay(&snapshot, 3, 0, CounterRelay(2), 201);
  PutRelay(&snapshot, 4, 0, CounterRelay(5), 202);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(0, 1, 4, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(3, 0, 5, 1, 201, AutoMapSignalSet, 0, 0));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  // The middle of the chain and the cell the lever's second link reaches are not
  // explored: neither they nor anything behind them is named.
  for (size_t i = drawList.cells.size(); i-- > 0;)
  {
    const AutoMapShownCell &cell = drawList.cells[i];
    if (cell.y == 0 && (cell.x == 3 || cell.x == 4)) drawList.cells.erase(drawList.cells.begin() + i);
  }
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  CHECK(sources[0].effects.size() == 1);
  if (sources[0].effects.size() != 1) return;
  CHECK(sources[0].effects[0].x == 2 && sources[0].effects[0].fires);
  CHECK(sources[0].effects[0].next.empty());
  // A lever whose only target is unexplored has no effects at all
  AutoMapDrawList onlyHidden = drawList;
  for (size_t i = onlyHidden.cells.size(); i-- > 0;)
  {
    if (onlyHidden.cells[i].x == 2 && onlyHidden.cells[i].y == 0)
    {
      onlyHidden.cells.erase(onlyHidden.cells.begin() + i);
    }
  }
  CHECK(AutoMapScene_PressEffects(onlyHidden, 0, 1).empty());
  // Below All the machinery is no one's business
  AutoMapFog seen = NoFog(explored, visibleNow);
  seen.spoilers = AutoMapSpoilersSeen;
  CHECK(AutoMapScene_PressEffects(AutoMapScene_BuildDrawList(snapshot, seen), 0, 1).empty());
  AutoMapDrawList learned = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  learned.spoilers = AutoMapSpoilersLearned;
  CHECK(AutoMapScene_PressEffects(learned, 0, 1).empty());
}

static void PressEffectsGroupBySource(void)
{
  // A lever on the east face and a second one on the south face of (0,1)
  AutoMapLevelSnapshot snapshot = MachineRoom();
  snapshot.cells[1 * 6 + 0].wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  PutRelay(&snapshot, 2, 0, CounterRelay(3), 200);
  PutRelay(&snapshot, 3, 0, CounterRelay(3), 201);
  PutRelay(&snapshot, 4, 0, CounterRelay(3), 202);
  AutoMapLink south = Message(0, 1, 2, 0, 101, AutoMapSignalClear, 0, 0);
  south.srcSide = AutoMapSouth;
  snapshot.links.push_back(south);
  snapshot.links.push_back(Message(0, 1, 3, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  south.dstX = 4;
  snapshot.links.push_back(south);
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 2);
  if (sources.size() != 2) return;
  CHECK(sources[0].source == 101 && sources[0].srcSide == AutoMapSouth);
  CHECK(sources[0].effects.size() == 2);
  if (sources[0].effects.size() == 2)
  {
    CHECK(sources[0].effects[0].x == 2 && sources[0].effects[1].x == 4);
  }
  CHECK(sources[1].source == LEVER_RECORD && sources[1].srcSide == AutoMapEast);
  CHECK(sources[1].effects.size() == 1 && sources[1].effects[0].x == 3);
}

static void PressEffectsLocalAction(void)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  AutoMapRelay local = GateRelay(0x0 | (0x1 << 4), true);
  local.localActionType = 1;
  local.delay = 4;
  PutRelay(&snapshot, 2, 0, local, 200);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalSet, 0, 1));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1 || sources[0].effects.size() != 1) return;
  const AutoMapEffect &gate = sources[0].effects[0];
  CHECK(gate.kind == AutoMapEffectGate && gate.fires);
  CHECK(gate.next.size() == 1);
  if (gate.next.size() != 1) return;
  CHECK(gate.next[0].kind == AutoMapEffectLocalAction);
  CHECK(gate.next[0].x == 2 && gate.next[0].y == 0 && gate.next[0].relay == 0);
  // The local action runs at once: the gate's own delay does not hold it back
  CHECK(gate.next[0].delayTicks == 1 && !gate.next[0].later);
}

static void PressEffectsFindTheGoalBehindTheRelays(void)
{
  // Floor cogs of an open cell get the message too, then the pit beneath them; a stone
  // cell is a launcher goal only through a launcher on the side the message names, and
  // a bare target otherwise.
  AutoMapLevelSnapshot snapshot = MachineRoom();
  snapshot.cells[1 * 6 + 1].kind = AutoMapCellPit;
  AutoMapFloorActuator cog(1, 1, AutoMapActuatorCog);
  cog.counterValue = 2;
  cog.relay = CounterRelay(2);
  cog.relay.record = 300;
  cog.record = 300;
  AutoMapFloorActuator pad(1, 1, AutoMapActuatorPad);
  snapshot.actuators.push_back(pad);
  snapshot.actuators.push_back(cog);
  AutoMapLauncher launcher;
  launcher.actuatorType = 8;
  launcher.side = AutoMapSouth;
  snapshot.cells[0 * 6 + 3].wallLaunchers.push_back(launcher);
  snapshot.links.push_back(Message(0, 1, 1, 1, LEVER_RECORD, AutoMapSignalSet, 0, 0));
  snapshot.links.push_back(Message(0, 1, 3, 0, LEVER_RECORD, AutoMapSignalSet, AutoMapNorth, 0));
  snapshot.links.push_back(Message(0, 1, 3, 0, LEVER_RECORD, AutoMapSignalSet, AutoMapSouth, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  const std::vector<AutoMapEffect> &effects = sources[0].effects;
  // A pit's cogs stay out of it: only an open floor walks its actuators
  CHECK(effects.size() == 3);
  if (effects.size() != 3) return;
  CHECK(effects[0].kind == AutoMapEffectGoal && effects[0].x == 1 && effects[0].y == 1);
  CHECK(!effects[0].target);
  CHECK(effects[1].kind == AutoMapEffectGoal && effects[1].x == 3 && effects[1].target);
  CHECK(effects[1].via.targetPos == AutoMapNorth);
  CHECK(effects[2].kind == AutoMapEffectGoal && effects[2].x == 3 && !effects[2].target);
  CHECK(effects[2].via.targetPos == AutoMapSouth);
  // On an open floor the cog counts and nothing else happens there
  snapshot.cells[1 * 6 + 1].kind = AutoMapCellFloor;
  sources = PressLever(snapshot);
  CHECK(sources.size() == 1 && !sources[0].effects.empty());
  if (sources.size() != 1 || sources[0].effects.empty()) return;
  const AutoMapEffect &counter = sources[0].effects[0];
  CHECK(counter.kind == AutoMapEffectCounter && counter.floorRelay && counter.relay == 1);
  CHECK(counter.before == 2 && counter.after == 3);
  CHECK(sources[0].effects.size() == 3);
}

static void PressLinesNameBareTargets(void)
{
  // A lever into a plain floor and into stone without a launcher on the message's side:
  // neither is a goal the walk knows, yet both are where the press goes
  AutoMapLevelSnapshot snapshot = MachineRoom();
  AutoMapLauncher launcher;
  launcher.actuatorType = 8;
  launcher.side = AutoMapSouth;
  snapshot.cells[0 * 6 + 3].wallLaunchers.push_back(launcher);
  snapshot.links.push_back(Message(0, 1, 1, 1, LEVER_RECORD, AutoMapSignalToggle, 0, 0));
  snapshot.links.push_back(Message(0, 1, 3, 0, LEVER_RECORD, AutoMapSignalSet, AutoMapNorth, 0));
  snapshot.links.push_back(Message(0, 1, 4, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  CHECK(Joined(PressText(drawList, sources, false))
        == "RESULT:\n  TARGET L0 (1, 1) TOGGLES ON THIS PRESS\n"
           "  TARGET L0 (3, 0) SETS ON THIS PRESS\n  TARGET L0 (4, 0) CLEARS ON THIS PRESS");
  // Nothing between the lever and its targets: Shift shows the same RESULT, no route
  CHECK(PressText(drawList, sources, true) == PressText(drawList, sources, false));
  CHECK(AutoMapScene_PressChainLinks(sources).size() == 3);
}

static void PressEffectsDeliverToTheWholeCellFirst(void)
{
  // Both counters get the lever's CLEAR before A's own SET comes back to the cell, so
  // both reach 0, and the SET finds them dead
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(1), 200);
  PutRelay(&snapshot, 2, 0, CounterRelay(1), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 2, 0, 200, AutoMapSignalSet, 0, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  const std::vector<AutoMapEffect> &effects = sources[0].effects;
  CHECK(effects.size() == 2);
  if (effects.size() != 2) return;
  CHECK(effects[0].before == 1 && effects[0].after == 0 && effects[0].fires);
  CHECK(effects[1].before == 1 && effects[1].after == 0 && effects[1].fires);
  CHECK(effects[1].next.empty());
  CHECK(effects[0].next.size() == 2);
  if (effects[0].next.size() != 2) return;
  for (size_t i = 0; i < 2; i++)
  {
    const AutoMapEffect &again = effects[0].next[i];
    CHECK(again.kind == AutoMapEffectCounter && again.relay == (int)i);
    CHECK(again.before == 0 && again.after == 0 && !again.fires);
  }
}

static void PressEffectsSpendOnceOnlyRelays(void)
{
  // The lever sets the gate's bit twice; a plain gate fires both times, a once-only
  // one is inert after its first firing
  for (int onceOnly = 0; onceOnly < 2; onceOnly++)
  {
    AutoMapLevelSnapshot snapshot = MachineRoom();
    AutoMapRelay gate = GateRelay(0x0 | (0x1 << 4), false);
    gate.onceOnly = onceOnly != 0;
    PutRelay(&snapshot, 2, 0, gate, 200);
    snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalSet, 0, 0));
    snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalSet, 0, 1));
    snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
    std::vector<AutoMapPressSource> sources = PressLever(snapshot);
    CHECK(sources.size() == 1);
    if (sources.size() != 1) return;
    const std::vector<AutoMapEffect> &effects = sources[0].effects;
    CHECK(effects.size() == (onceOnly ? 1u : 2u));
    for (size_t i = 0; i < effects.size(); i++)
    {
      CHECK(effects[i].kind == AutoMapEffectGate && effects[i].fires);
      CHECK(effects[i].next.size() == 1);
    }
  }
}

static int CountEffects(const std::vector<AutoMapEffect> &effects)
{
  int count = 0;
  for (size_t i = 0; i < effects.size(); i++) count += 1 + CountEffects(effects[i].next);
  return count;
}

static void PressEffectsStopAtTheBudget(void)
{
  // Eight gates that all feed each other have a path for every ordering of them: far
  // more than the walk may build
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 10;
  snapshot.height = 3;
  snapshot.cells.assign(30, AutoMapCellRecord());
  snapshot.cells[1 * 10 + 1].kind = AutoMapCellFloor;
  snapshot.cells[1 * 10 + 0].wallSides[AutoMapEast].picture = LEVER_PICTURE;
  snapshot.party.x = 1;
  snapshot.party.y = 1;
  snapshot.party.facing = AutoMapWest;
  for (int gate = 0; gate < 8; gate++)
  {
    PutRelay(&snapshot, gate + 1, 0, GateRelay(0x0 | (0xF << 4), false), 200 + gate);
  }
  for (int from = 0; from < 8; from++)
  {
    for (int to = 0; to < 8; to++)
    {
      if (from == to) continue;
      snapshot.links.push_back(Message(from + 1, 0, to + 1, 0, 200 + from, AutoMapSignalToggle,
                                       to % 4, 0));
    }
  }
  snapshot.links.push_back(Message(0, 1, 1, 0, LEVER_RECORD, AutoMapSignalSet, 0, 0));
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalSet, 1, 0));
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  int count = CountEffects(sources[0].effects);
  CHECK(count <= 1000);
  CHECK(count > 900);
  CHECK(sources[0].truncated);
}

static void PressEffectsLeaveARelayCellUnpressed(void)
{
  // The DM L5 (18,28) shape hovered: counters at 4 and 3 with their own links to the door.
  // Hovering the relay cell presses nothing, so it has no press block, only the relay one.
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(4), 200);
  PutRelay(&snapshot, 2, 0, CounterRelay(3), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  snapshot.links.push_back(Message(2, 0, 5, 1, 201, AutoMapSignalClear, 0, 0));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 2, 0);
  CHECK(sources.empty());
  CHECK(PressText(drawList, sources, false).empty());
  CHECK(PressText(drawList, sources, true).empty());
  CHECK(Joined(AutoMapScene_RelayHoverLines(drawList, 2, 0, false))
        == "STONE: 2 COUNTERS\n A COUNT 4 -> L0 (5, 1) DOOR OPEN AT 0\n"
           " B COUNT 3 -> L0 (5, 1) DOOR CLOSE AT 0\nFED BY:\n L0 (0, 1) SWITCH (E) A-1 B-1");
  // A floor cog's own link is no press of the floor either
  snapshot.cells[1 * 6 + 1].kind = AutoMapCellFloor;
  AutoMapFloorActuator cog(1, 1, AutoMapActuatorCog);
  cog.relay = CounterRelay(2);
  cog.relay.record = 300;
  cog.record = 300;
  snapshot.actuators.push_back(cog);
  AutoMapLink fromCog = Message(1, 1, 5, 1, 300, AutoMapSignalSet, 0, 0);
  fromCog.srcIsWall = false;
  snapshot.links.push_back(fromCog);
  drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  CHECK(AutoMapScene_PressEffects(drawList, 1, 1).empty());
  CHECK(!AutoMapScene_RelayHoverLines(drawList, 1, 1, false).empty());
}

static void PressEffectsFireEveryActuatorOfTheFace(void)
{
  // One click fires both records on the lever's face: bit 0 then bit 1 of a gate that
  // needs 0011, so the gate matches and opens the door on this press
  AutoMapLevelSnapshot snapshot = MachineRoom();
  snapshot.cells[1 * 6 + 5].doorState = AutoMapDoorClosed;
  PutRelay(&snapshot, 2, 0, GateRelay(0x0 | (0x3 << 4), false), 200);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalToggle, 0, 0));
  AutoMapLink second = Message(0, 1, 2, 0, LEVER_RECORD + 1, AutoMapSignalToggle, 1, 0);
  second.srcSide = AutoMapEast;
  snapshot.links.push_back(second);
  snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  CHECK(sources[0].source == LEVER_RECORD && sources[0].srcSide == AutoMapEast);
  CHECK(sources[0].effects.size() == 2);
  if (sources[0].effects.size() != 2) return;
  CHECK(sources[0].effects[0].before == 0x0 && sources[0].effects[0].after == 0x1);
  CHECK(sources[0].effects[1].before == 0x1 && sources[0].effects[1].after == 0x3);
  CHECK(sources[0].effects[1].fires);
  CHECK(Joined(PressText(drawList, sources, false))
        == "THIS PRESS:\n  L0 (2, 0) GATE A 0000->0001 (NEEDS 0011) NO MATCH\n"
           "  L0 (2, 0) GATE A 0001->0011 (NEEDS 0011) MATCH\n"
           "RESULT:\n  DOOR L0 (5, 1) OPENS ON THIS PRESS");
  CHECK(sources[0].effects[0].next.empty());
  // Beside a lever on another face, the east face still takes one header
  AutoMapLevelSnapshot twoFaces = snapshot;
  twoFaces.cells[1 * 6 + 0].wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  AutoMapLink south = Message(0, 1, 5, 1, LEVER_RECORD + 2, AutoMapSignalSet, 0, 0);
  south.srcSide = AutoMapSouth;
  twoFaces.links.push_back(south);
  AutoMapDrawList twoDrawList = AutoMapScene_BuildDrawList(twoFaces, NoFog(explored, visibleNow));
  std::vector<AutoMapPressSource> two = AutoMapScene_PressEffects(twoDrawList, 0, 1);
  CHECK(two.size() == 2);
  std::string twoLines =
      JoinedTip(AutoMapTooltip_Machinery(twoDrawList, 0, 1, false, TestItemName));
  CHECK(twoLines.find("SWITCH (E)\n") == 0);
  CHECK(twoLines.find("SWITCH (E)", 1) == std::string::npos);
  CHECK(twoLines.find("SWITCH (S)\n") != std::string::npos);
  // Two TOGGLEs of one bit from one face cancel: the gate ends where it began
  snapshot.links[1].targetPos = 0;
  drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  sources = AutoMapScene_PressEffects(drawList, 0, 1);
  CHECK(sources.size() == 1);
  if (sources.size() != 1 || sources[0].effects.size() != 2) return;
  CHECK(sources[0].effects[1].before == 0x1 && sources[0].effects[1].after == 0x0);
  CHECK(!sources[0].effects[0].fires && !sources[0].effects[1].fires);
  std::string lines = Joined(PressText(drawList, sources, false));
  CHECK(lines.find("GATE A 0001->0000 (NEEDS 0011) NO MATCH") != std::string::npos);
  CHECK(lines.find(") MATCH") == std::string::npos);
  CHECK(lines.find("SWITCH") == std::string::npos);
}

// The first goal of the press tree, depth first, or NULL
static const AutoMapEffect *FirstGoal(const std::vector<AutoMapEffect> &effects)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    if (effects[i].kind == AutoMapEffectGoal) return &effects[i];
    const AutoMapEffect *deeper = FirstGoal(effects[i].next);
    if (deeper != NULL) return deeper;
  }
  return NULL;
}

static AutoMapDrawList DrawAll(const AutoMapLevelSnapshot &snapshot);

// The distance of the first goal one press of the lever reaches
static AutoMapGoalDistance LeverGoalDistance(const AutoMapLevelSnapshot &snapshot)
{
  std::vector<AutoMapPressSource> sources = PressLever(snapshot);
  const AutoMapEffect *goal = sources.size() == 1 ? FirstGoal(sources[0].effects) : NULL;
  CHECK(goal != NULL);
  return goal != NULL ? goal->distance : AutoMapGoalDistance();
}

// The machine room with its door shut, so opening it is something left to do
static AutoMapLevelSnapshot ShutMachineRoom(void)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  snapshot.cells[1 * 6 + 5].doorState = AutoMapDoorClosed;
  return snapshot;
}

static void GoalDistanceCountsDownACounter(void)
{
  // The DM L5 shape: the lever counts B down, and B opens the door at 0
  for (int count = 3; count >= 1; count -= 2)
  {
    AutoMapLevelSnapshot snapshot = ShutMachineRoom();
    PutRelay(&snapshot, 2, 0, CounterRelay(count), 200);
    snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
    snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
    AutoMapGoalDistance distance = LeverGoalDistance(snapshot);
    if (count == 3)
    {
      CHECK(distance.status == AutoMapGoalMore && distance.more == 2);
    }
    else
    {
      CHECK(distance.status == AutoMapGoalOnThisPress && distance.more == -1);
    }
    CHECK(distance.bitsOff == 0 && distance.blocked == NULL);
  }
}

// The lever counts A down from 4; A sets bit 2 of the gate, and the gate opens the door
static AutoMapGoalDistance CounterIntoGate(int gateValue)
{
  AutoMapLevelSnapshot snapshot = ShutMachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(4), 200);
  PutRelay(&snapshot, 3, 0, GateRelay(gateValue, false), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalSet, 2, 0));
  snapshot.links.push_back(Message(3, 0, 5, 1, 201, AutoMapSignalSet, 0, 0));
  return LeverGoalDistance(snapshot);
}

static void GoalDistanceThroughAGate(void)
{
  // 1011 needs only bit 2, which A sets once it reaches 0
  AutoMapGoalDistance oneBit = CounterIntoGate(0xB | (0xF << 4));
  CHECK(oneBit.status == AutoMapGoalMore && oneBit.more == 3 && oneBit.bitsOff == 0);
  // 0011 still lacks bit 3 once bit 2 arrives, and nothing of this lever sets it
  AutoMapGoalDistance twoBits = CounterIntoGate(0x3 | (0xF << 4));
  CHECK(twoBits.status == AutoMapGoalNeedsOthers && twoBits.bitsOff == 1);
  CHECK(twoBits.more == -1);
  // The lever toggles bit 0 itself: this press breaks the match, the next one restores it
  AutoMapLevelSnapshot snapshot = ShutMachineRoom();
  PutRelay(&snapshot, 2, 0, GateRelay(0x1 | (0x1 << 4), false), 200);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalToggle, 0, 0));
  snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  AutoMapGoalDistance toggled = LeverGoalDistance(snapshot);
  CHECK(toggled.status == AutoMapGoalMore && toggled.more == 1 && toggled.bitsOff == 1);
}

// The lever sends one message into the gate at (2,0), and the gate opens the door
static AutoMapGoalDistance LeverIntoGate(AutoMapRelay gate, AutoMapSignal signal, int bit)
{
  AutoMapLevelSnapshot snapshot = ShutMachineRoom();
  PutRelay(&snapshot, 2, 0, gate, 200);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, signal, bit, 0));
  snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  return LeverGoalDistance(snapshot);
}

static void GoalDistanceInvertedGateNeedsOthers(void)
{
  // A normally-closed gate fires on a mismatch: the lever's SET makes it match, which it
  // ignores, and pressing again sends the same SET, so the lever alone never opens it
  AutoMapRelay inverted = GateRelay(0xB | (0xF << 4), false);
  inverted.normallyClosed = true;
  AutoMapGoalDistance distance = LeverIntoGate(inverted, AutoMapSignalSet, 2);
  CHECK(distance.status == AutoMapGoalNeedsOthers && distance.bitsOff == 0);
  CHECK(distance.more == -1);
}

static void GoalDistanceEdgeModeGate(void)
{
  // On a change only: SET on a bit already set changes nothing, now or on any press
  for (int edgeMode = 1; edgeMode <= 2; edgeMode++)
  {
    AutoMapRelay change = GateRelay(0xF | (0xF << 4), false);
    change.edgeMode = edgeMode;
    AutoMapGoalDistance distance = LeverIntoGate(change, AutoMapSignalSet, 0);
    CHECK(distance.status == AutoMapGoalNeedsOthers && distance.bitsOff == 0);
  }
  // SET only: the TOGGLE that breaks the match would restore it, but is filtered out
  AutoMapRelay setOnly = GateRelay(0x1 | (0x1 << 4), false);
  setOnly.edgeMode = 4;
  AutoMapGoalDistance toggled = LeverIntoGate(setOnly, AutoMapSignalToggle, 0);
  CHECK(toggled.status == AutoMapGoalNeedsOthers && toggled.bitsOff == 1);
  // The same TOGGLE into a plain gate restores the match on the next press
  AutoMapGoalDistance plain = LeverIntoGate(GateRelay(0x1 | (0x1 << 4), false),
                                            AutoMapSignalToggle, 0);
  CHECK(plain.status == AutoMapGoalMore && plain.more == 1);
}

static void GoalDistanceCounterBehindGate(void)
{
  // How often the gate fires over further presses is not worked out, so a counter it
  // feeds that still needs 3 messages reads as needing other sources
  AutoMapLevelSnapshot snapshot = ShutMachineRoom();
  PutRelay(&snapshot, 2, 0, GateRelay(0x0 | (0x1 << 4), false), 200);
  PutRelay(&snapshot, 3, 0, CounterRelay(3), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalToggle, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(3, 0, 5, 1, 201, AutoMapSignalSet, 0, 0));
  CHECK(LeverGoalDistance(snapshot).status == AutoMapGoalNeedsOthers);
}

static void GoalDistanceCounterBehindCounter(void)
{
  // A at 1 fires once and is dead: B at 3 can never get its other two messages from it
  for (int second = 3; second >= 1; second -= 2)
  {
    AutoMapLevelSnapshot snapshot = ShutMachineRoom();
    PutRelay(&snapshot, 2, 0, CounterRelay(1), 200);
    PutRelay(&snapshot, 3, 0, CounterRelay(second), 201);
    snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
    snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalClear, 0, 0));
    snapshot.links.push_back(Message(3, 0, 5, 1, 201, AutoMapSignalSet, 0, 0));
    AutoMapGoalDistance distance = LeverGoalDistance(snapshot);
    CHECK(distance.status == (second == 3 ? AutoMapGoalNeedsOthers : AutoMapGoalOnThisPress));
  }
}

static void GoalDistanceBlocked(void)
{
  // SET counts a counter up, away from the 0 it fires at
  AutoMapLevelSnapshot up = ShutMachineRoom();
  PutRelay(&up, 2, 0, CounterRelay(3), 200);
  up.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalSet, 0, 0));
  up.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  AutoMapGoalDistance countsUp = LeverGoalDistance(up);
  CHECK(countsUp.status == AutoMapGoalBlocked && countsUp.more == -1);
  CHECK(countsUp.blocked != NULL && std::string(countsUp.blocked) == "COUNTS UP");
  // A counter at 0 never fires again
  AutoMapLevelSnapshot dead = ShutMachineRoom();
  PutRelay(&dead, 2, 0, CounterRelay(0), 200);
  dead.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  dead.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  AutoMapGoalDistance atZero = LeverGoalDistance(dead);
  CHECK(atZero.status == AutoMapGoalBlocked);
  CHECK(atZero.blocked != NULL && std::string(atZero.blocked) == "COUNTER AT 0");
  // A once-only gate: the counter's bit 1 would reach it only later, and the lever's
  // bit 0 makes it fire now and spends it, so the wait behind the counter is in vain
  AutoMapLevelSnapshot once = ShutMachineRoom();
  AutoMapRelay gate = GateRelay(0x2 | (0x3 << 4), false);
  gate.onceOnly = true;
  PutRelay(&once, 2, 0, gate, 200);
  PutRelay(&once, 3, 0, CounterRelay(4), 201);
  once.links.push_back(Message(0, 1, 3, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  once.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalSet, 0, 0));
  once.links.push_back(Message(3, 0, 2, 0, 201, AutoMapSignalToggle, 1, 0));
  once.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  std::vector<AutoMapPressSource> sources = PressLever(once);
  CHECK(sources.size() == 1);
  if (sources.size() != 1 || sources[0].effects.size() != 2) return;
  const AutoMapEffect *waited = FirstGoal(sources[0].effects[0].next);
  const AutoMapEffect *fired = FirstGoal(sources[0].effects[1].next);
  CHECK(waited != NULL && fired != NULL);
  if (waited == NULL || fired == NULL) return;
  CHECK(waited->distance.status == AutoMapGoalBlocked);
  CHECK(waited->distance.blocked != NULL && std::string(waited->distance.blocked) == "SPENT");
  CHECK(fired->distance.status == AutoMapGoalOnThisPress);
}

static void GoalDistanceOtherStatuses(void)
{
  // A CONSTPRESSURE lever opens the door while it is held and shuts it on release
  AutoMapLevelSnapshot held = ShutMachineRoom();
  held.links.push_back(Message(0, 1, 5, 1, LEVER_RECORD, AutoMapSignalPressure, 0, 0));
  CHECK(LeverGoalDistance(held).status == AutoMapGoalHolds);
  // So does a CONSTPRESSURE counter on the way
  AutoMapLevelSnapshot relayed = ShutMachineRoom();
  AutoMapRelay pressure = CounterRelay(3);
  pressure.signal = AutoMapSignalPressure;
  PutRelay(&relayed, 2, 0, pressure, 200);
  relayed.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  relayed.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalPressure, 0, 0));
  CHECK(LeverGoalDistance(relayed).status == AutoMapGoalHolds);
  // A DSA ahead of the counter may stop the message, which the walk cannot tell
  AutoMapLevelSnapshot guarded = ShutMachineRoom();
  AutoMapRelay dsa;
  dsa.kind = AutoMapRelayDSA;
  PutRelay(&guarded, 2, 0, dsa, 200);
  PutRelay(&guarded, 2, 0, CounterRelay(1), 201);
  guarded.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  guarded.links.push_back(Message(2, 0, 5, 1, 201, AutoMapSignalSet, 0, 0));
  CHECK(LeverGoalDistance(guarded).status == AutoMapGoalThroughDSA);
  // Opening a door that is open already changes nothing; a shut one opens now
  AutoMapLevelSnapshot open = MachineRoom();
  open.links.push_back(Message(0, 1, 5, 1, LEVER_RECORD, AutoMapSignalSet, 0, 0));
  CHECK(LeverGoalDistance(open).status == AutoMapGoalAlreadyDone);
  open.cells[1 * 6 + 5].doorState = AutoMapDoorClosed;
  CHECK(LeverGoalDistance(open).status == AutoMapGoalOnThisPress);
  // So does a false wall: SET into an open one, CLEAR into a closed one
  AutoMapLevelSnapshot wall = MachineRoom();
  wall.cells[1 * 6 + 5].kind = AutoMapCellOpenFalseWall;
  wall.links.push_back(Message(0, 1, 5, 1, LEVER_RECORD, AutoMapSignalSet, 0, 0));
  CHECK(LeverGoalDistance(wall).status == AutoMapGoalAlreadyDone);
  CHECK(Joined(PressText(DrawAll(wall), PressLever(wall), false))
        == "RESULT:\n  FALSE WALL L0 (5, 1) ALREADY OPEN");
  wall.links[0].signal = AutoMapSignalClear;
  CHECK(LeverGoalDistance(wall).status == AutoMapGoalOnThisPress);
  wall.cells[1 * 6 + 5].kind = AutoMapCellFalseWall;
  CHECK(LeverGoalDistance(wall).status == AutoMapGoalAlreadyDone);
  CHECK(Joined(PressText(DrawAll(wall), PressLever(wall), false))
        == "RESULT:\n  FALSE WALL L0 (5, 1) ALREADY CLOSED");
  wall.links[0].signal = AutoMapSignalToggle;
  CHECK(LeverGoalDistance(wall).status == AutoMapGoalOnThisPress);
}

// ---- Tooltip lines of a press ----

static AutoMapDrawList DrawAll(const AutoMapLevelSnapshot &snapshot)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  return AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
}

static std::string Joined(const std::vector<std::string> &lines)
{
  std::string text;
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (i > 0) text += "\n";
    text += lines[i];
  }
  return text;
}

static bool HasText(const std::vector<std::string> &lines, const char *text)
{
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (lines[i].find(text) != std::string::npos) return true;
  }
  return false;
}

// The DM L5 machine at (18, 28): a lever on the east face of (16, 28) counts A (4) and
// B (3) down. A sets bit 2 of the gate at (25, 26), which opens the door at (26, 26) one
// second later; B opens the door at (20, 29). Local coordinates are global minus (16, 25).
static AutoMapLevelSnapshot L5Machine(void)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = 5;
  snapshot.width = 12;
  snapshot.height = 5;
  snapshot.offsetX = 16;
  snapshot.offsetY = 25;
  snapshot.cells.assign(60, AutoMapCellRecord());
  snapshot.cells[3 * 12 + 1].kind = AutoMapCellFloor;
  snapshot.cells[3 * 12 + 3].kind = AutoMapCellFloor;
  snapshot.cells[1 * 12 + 10].kind = AutoMapCellDoor;
  snapshot.cells[1 * 12 + 10].doorState = AutoMapDoorClosed;
  snapshot.cells[4 * 12 + 4].kind = AutoMapCellDoor;
  snapshot.cells[4 * 12 + 4].doorState = AutoMapDoorClosed;
  snapshot.cells[3 * 12 + 0].wallSides[AutoMapEast].picture = LEVER_PICTURE;
  snapshot.party.x = 1;
  snapshot.party.y = 3;
  snapshot.party.facing = AutoMapWest;
  PutRelay(&snapshot, 2, 3, CounterRelay(4), 200);
  PutRelay(&snapshot, 2, 3, CounterRelay(3), 201);
  PutRelay(&snapshot, 9, 1, GateRelay(0xB | (0xF << 4), false), 202);
  snapshot.links.push_back(Message(0, 3, 2, 3, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 3, 9, 1, 200, AutoMapSignalSet, 2, 0));
  snapshot.links.push_back(Message(9, 1, 10, 1, 202, AutoMapSignalSet, 0, 6));
  snapshot.links.push_back(Message(2, 3, 4, 4, 201, AutoMapSignalSet, 0, 0));
  return snapshot;
}

static void PressLinesCompact(void)
{
  AutoMapDrawList drawList = DrawAll(L5Machine());
  std::vector<std::string> lines = PressText(
      drawList, AutoMapScene_PressEffects(drawList, 0, 3), false);
  CHECK(Joined(lines) == "THIS PRESS:\n  L5 (18, 28) COUNTER A 4->3\n  L5 (18, 28) COUNTER B 3->2\n"
                         "RESULT:\n  DOOR L5 (26, 26) OPENS IN 3 MORE, AFTER 6 TICKS\n"
                         "  DOOR L5 (20, 29) OPENS IN 2 MORE");
  // Headings plain, what this press reaches plain, results gold
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 3);
  if (sources.size() == 1)
  {
    std::vector<AutoMapTipLine> block = AutoMapScene_PressBlock(drawList, sources[0], false);
    CHECK(block.size() == 6);
    if (block.size() == 6)
    {
      CHECK(block[0].style == AutoMapTipPlain && block[1].style == AutoMapTipPlain);
      CHECK(block[3].style == AutoMapTipPlain && block[4].style == AutoMapTipResult);
    }
  }
  // Nothing to say without a press
  CHECK(AutoMapScene_PressBlock(drawList, AutoMapPressSource(), false).empty());
}

static void PressLinesTree(void)
{
  AutoMapDrawList drawList = DrawAll(L5Machine());
  std::vector<std::string> lines = PressText(
      drawList, AutoMapScene_PressEffects(drawList, 0, 3), true);
  // The gate sits under A's AT 0, the first door under the gate's ON MATCH: it needs both
  CHECK(Joined(lines) == "ROUTE:\n"
                         "  COUNTER A L5 (18, 28)\n"
                         "    COUNTS 4 -> 3\n"
                         "    AT 0: (IN 3 MORE)\n"
                         "      GATE A L5 (25, 26), SETS BIT 2\n"
                         "        BITS 1011 -> 1111, NEEDS 1111\n"
                         "        ON MATCH: (IN 3 MORE)\n"
                         "          DOOR L5 (26, 26) OPENS, AFTER 6 TICKS\n"
                         "  COUNTER B L5 (18, 28)\n"
                         "    COUNTS 3 -> 2\n"
                         "    AT 0: (IN 2 MORE)\n"
                         "      DOOR L5 (20, 29) OPENS");
}


static void WhenNoteWords(void)
{
  AutoMapGoalDistance d;
  CHECK(AutoMapScene_WhenNote(d) == "");
  d.status = AutoMapGoalOnThisPress;
  CHECK(AutoMapScene_WhenNote(d) == "(THIS PRESS)");
  d.status = AutoMapGoalMore;
  d.more = 1;
  CHECK(AutoMapScene_WhenNote(d) == "(NEXT PRESS)");
  d.more = 3;
  CHECK(AutoMapScene_WhenNote(d) == "(IN 3 MORE)");
  d.more = 2;
  d.bitsOff = 1;
  CHECK(AutoMapScene_WhenNote(d) == "(1 BIT OFF, IN 2 MORE)");
  d.more = 1;
  d.bitsOff = 2;
  CHECK(AutoMapScene_WhenNote(d) == "(2 BITS OFF, NEXT PRESS)");
  d.status = AutoMapGoalNeedsOthers;
  d.bitsOff = 0;
  CHECK(AutoMapScene_WhenNote(d) == "(NEEDS OTHER SOURCES)");
  d.bitsOff = 1;
  CHECK(AutoMapScene_WhenNote(d) == "(1 BIT OFF, NEEDS OTHER SOURCES)");
  d.bitsOff = 0;
  d.status = AutoMapGoalBlocked;
  d.blocked = "COUNTER AT 0";
  CHECK(AutoMapScene_WhenNote(d) == "(BLOCKED: COUNTER AT 0)");
  d.status = AutoMapGoalHolds;
  CHECK(AutoMapScene_WhenNote(d) == "(EVERY PRESS)");
  d.status = AutoMapGoalThroughDSA;
  CHECK(AutoMapScene_WhenNote(d) == "(IF DSA PASSES)");
  d.status = AutoMapGoalAlreadyDone;
  CHECK(AutoMapScene_WhenNote(d) == "");
}

// The coin slot of RelayLinesDescribeWallCounterChain counting a counter (record 60) that
// opens the door at (14, 23) when it reaches 0
static AutoMapDrawList SlotCounterDoor(int count)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = 2;
  snapshot.offsetX = 12;
  snapshot.offsetY = 22;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  AutoMapRelay counter = CounterRelay(count);
  counter.record = 60;
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(counter);
  AutoMapLink slotToCounter = {1, 0, 0, 1, true, AutoMapSouth};
  slotToCounter.signal = AutoMapSignalClear;
  AutoMapLink counterToDoor = {0, 1, 2, 1, true, AutoMapEast};
  counterToDoor.signal = AutoMapSignalSet;
  counterToDoor.source = 60;
  snapshot.links.push_back(slotToCounter);
  snapshot.links.push_back(counterToDoor);
  return DrawAll(snapshot);
}

// The route of the one press source of (x, y)
static std::vector<AutoMapTipLine> RouteBlock(const AutoMapDrawList &drawList, int x, int y)
{
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, x, y);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return std::vector<AutoMapTipLine>();
  return AutoMapScene_PressBlock(drawList, sources[0], true);
}

static void RouteCounterThenGoal(void)
{
  AutoMapDrawList drawList = SlotCounterDoor(2);
  std::vector<AutoMapTipLine> block = RouteBlock(drawList, 1, 0);
  CHECK(JoinedTip(block) == "ROUTE:\n"
                            "  COUNTER A L2 (12, 23)\n"
                            "    COUNTS 2 -> 1\n"
                            "    AT 0: (NEXT PRESS)\n"
                            "      DOOR L2 (14, 23) OPENS");
  CHECK(block.size() == 5);
  if (block.size() != 5) return;
  CHECK(block[0].style == AutoMapTipPlain && block[1].style == AutoMapTipPlain);
  CHECK(block[2].style == AutoMapTipState);
  CHECK(block[3].style == AutoMapTipCondition);
  CHECK(block[4].style == AutoMapTipResult);
}

static void RouteHeldCounterSaysWhatThisCountSends(void)
{
  // A CONSTPRESSURE counter reports on every count: above 0 it sends CLEAR, so this press
  // closes the door (Task 7 adds the other side)
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = 2;
  snapshot.offsetX = 12;
  snapshot.offsetY = 22;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellDoor;
  AutoMapRelay counter = CounterRelay(2);
  counter.record = 60;
  counter.signal = AutoMapSignalPressure;
  snapshot.cells[1 * 4 + 0].wallRelays.push_back(counter);
  AutoMapLink slotToCounter = {1, 0, 0, 1, true, AutoMapSouth};
  slotToCounter.signal = AutoMapSignalClear;
  AutoMapLink counterToDoor = {0, 1, 2, 1, true, AutoMapEast};
  counterToDoor.signal = AutoMapSignalPressure;
  counterToDoor.source = 60;
  snapshot.links.push_back(slotToCounter);
  snapshot.links.push_back(counterToDoor);
  CHECK(JoinedTip(RouteBlock(DrawAll(snapshot), 1, 0))
        == "ROUTE:\n"
           "  COUNTER A L2 (12, 23)\n"
           "    COUNTS 2 -> 1\n"
           "    EACH COUNT: (EVERY PRESS)\n"
           "      DOOR L2 (14, 23) CLOSES");
}

// The RESULT line of the one goal of the lever's press, and its map badge
static void LeverResultAndBadge(const AutoMapDrawList &drawList, std::string *result,
                                std::string *badge)
{
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  std::vector<std::string> lines = PressText(drawList, sources, false);
  *result = lines.empty() ? std::string() : lines.back();
  std::vector<const AutoMapEffect *> goals = AutoMapScene_PressGoalsByCell(sources);
  *badge = goals.size() == 1 ? AutoMapScene_GoalBadge(goals[0]->distance) : std::string("?");
}

static void HeldSourceAgreesInRouteResultAndBadge(void)
{
  // A normally-closed held lever counts the counter down on press and up again on release:
  // at 4 press and release never get it to 0, so ROUTE, RESULT and badge say it needs other
  // sources; at 1 every step fires on this press, which holds while pressed
  std::string result, badge;
  for (int count = 1; count <= 4; count += 3)
  {
    AutoMapLevelSnapshot snapshot = MachineRoom();
    PutRelay(&snapshot, 2, 0, CounterRelay(count), 200);
    AutoMapLink held = Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalPressure, 0, 0);
    held.sourceNormallyClosed = true;
    snapshot.links.push_back(held);
    snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
    AutoMapDrawList drawList = DrawAll(snapshot);
    std::string route = JoinedTip(RouteBlock(drawList, 0, 1));
    LeverResultAndBadge(drawList, &result, &badge);
    if (count == 4)
    {
      CHECK(route == "ROUTE:\n  COUNTER A L0 (2, 0)\n    COUNTS 4 -> 3\n"
                     "    AT 0: (NEEDS OTHER SOURCES)\n      DOOR L0 (5, 1) OPENS");
      CHECK(result == "  DOOR L0 (5, 1) NEEDS OTHER SOURCES");
      CHECK(badge == "NEEDS OTHERS");
    }
    else
    {
      CHECK(route == "ROUTE:\n  COUNTER A L0 (2, 0)\n    COUNTS 1 -> 0\n"
                     "    AT 0: (EVERY PRESS)\n      DOOR L0 (5, 1) OPENS");
      CHECK(result == "  DOOR L0 (5, 1) HOLDS WHILE PRESSED");
      CHECK(badge == "HOLD");
    }
  }
  // Held straight into the door: HOLD everywhere (no steps, so no route)
  AutoMapLevelSnapshot direct = MachineRoom();
  direct.links.push_back(Message(0, 1, 5, 1, LEVER_RECORD, AutoMapSignalPressure, 0, 0));
  AutoMapDrawList drawList = DrawAll(direct);
  LeverResultAndBadge(drawList, &result, &badge);
  CHECK(result == "  DOOR L0 (5, 1) HOLDS WHILE PRESSED");
  CHECK(badge == "HOLD");
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  CHECK(sources.size() == 1 && !AutoMapScene_PressHasSteps(sources[0]));
}

static void RouteAfterADSANeverSaysThisPress(void)
{
  // A counter at 1 after a DSA reaches 0 on this press, plain or held: the DSA may stop it
  for (int heldLever = 0; heldLever < 2; heldLever++)
  {
    AutoMapLevelSnapshot snapshot = MachineRoom();
    AutoMapRelay dsa;
    dsa.kind = AutoMapRelayDSA;
    PutRelay(&snapshot, 2, 0, dsa, 200);
    PutRelay(&snapshot, 2, 0, CounterRelay(1), 201);
    AutoMapLink lever = Message(0, 1, 2, 0, LEVER_RECORD,
                                heldLever ? AutoMapSignalPressure : AutoMapSignalClear, 0, 0);
    lever.sourceNormallyClosed = heldLever != 0;
    snapshot.links.push_back(lever);
    snapshot.links.push_back(Message(2, 0, 5, 1, 201, AutoMapSignalSet, 0, 0));
    std::string route = JoinedTip(RouteBlock(DrawAll(snapshot), 0, 1));
    CHECK(route.find("    AT 0: (IF DSA PASSES)\n") != std::string::npos);
    CHECK(route.find("THIS PRESS") == std::string::npos);
    CHECK(route.find("EVERY PRESS") == std::string::npos);
  }
}

static void RouteHeldStepReachedLaterDoesNotSayEveryPress(void)
{
  // A counter at 3 feeds a held counter at 2: the held one only gets the message in 2 more
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(3), 200);
  AutoMapRelay held = CounterRelay(2);
  held.signal = AutoMapSignalPressure;
  PutRelay(&snapshot, 3, 0, held, 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(3, 0, 5, 1, 201, AutoMapSignalPressure, 0, 0));
  CHECK(JoinedTip(RouteBlock(DrawAll(snapshot), 0, 1))
        == "ROUTE:\n"
           "  COUNTER A L0 (2, 0)\n"
           "    COUNTS 3 -> 2\n"
           "    AT 0: (IN 2 MORE)\n"
           "      COUNTER A L0 (3, 0)\n"
           "        COUNTS 2 -> 1\n"
           "        EACH COUNT: (NEEDS OTHER SOURCES)\n"
           "          DOOR L0 (5, 1) CLOSES");
}

static void RouteDeadCounterIsBlocked(void)
{
  AutoMapDrawList drawList = SlotCounterDoor(0);
  std::vector<AutoMapTipLine> block = RouteBlock(drawList, 1, 0);
  CHECK(JoinedTip(block) == "ROUTE:\n"
                            "  COUNTER A L2 (12, 23)\n"
                            "    AT 0, DEAD\n"
                            "    AT 0: (BLOCKED: COUNTER AT 0)\n"
                            "      DOOR L2 (14, 23) OPENS");
  CHECK(block.size() == 5 && block[2].style == AutoMapTipState
        && block[3].style == AutoMapTipCondition);
}

// The lever's message into (2, 0): a DSA (record 200) then a counter at 2 (record 201),
// each wired to the door
static AutoMapDrawList DSAThenCounter(void)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  AutoMapRelay dsa;
  dsa.kind = AutoMapRelayDSA;
  PutRelay(&snapshot, 2, 0, dsa, 200);
  PutRelay(&snapshot, 2, 0, CounterRelay(2), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  snapshot.links.push_back(Message(2, 0, 5, 1, 201, AutoMapSignalSet, 0, 0));
  return DrawAll(snapshot);
}

static void RouteStopsAtADSA(void)
{
  std::vector<AutoMapTipLine> block = RouteBlock(DSAThenCounter(), 0, 1);
  // Nothing under the DSA: what its program sends is not followed
  CHECK(block.size() >= 4);
  if (block.size() < 4) return;
  CHECK(block[1].text == "  DSA A L0 (2, 0)" && block[1].style == AutoMapTipPlain);
  CHECK(block[2].text == "    IF ITS PROGRAM PASSES IT ON (NOT FOLLOWED)");
  CHECK(block[2].style == AutoMapTipCondition);
  CHECK(block[3].text.compare(0, 3, "  C") == 0);
}

static void RouteMarksStepsAfterADSA(void)
{
  CHECK(JoinedTip(RouteBlock(DSAThenCounter(), 0, 1))
        == "ROUTE:\n"
           "  DSA A L0 (2, 0)\n"
           "    IF ITS PROGRAM PASSES IT ON (NOT FOLLOWED)\n"
           "  COUNTER B L0 (2, 0) (IF DSA PASSES)\n"
           "    COUNTS 2 -> 1\n"
           "    AT 0: (IF DSA PASSES)\n"
           "      DOOR L0 (5, 1) OPENS");
}

static void RouteNamesTheLoop(void)
{
  // Counter A at (2, 0) feeds the counter at (3, 0), which feeds A again
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(2), 200);
  PutRelay(&snapshot, 3, 0, CounterRelay(1), 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(3, 0, 2, 0, 201, AutoMapSignalClear, 0, 0));
  CHECK(JoinedTip(RouteBlock(DrawAll(snapshot), 0, 1))
        == "ROUTE:\n"
           "  COUNTER A L0 (2, 0)\n"
           "    COUNTS 2 -> 1\n"
           "    AT 0: (NEXT PRESS)\n"
           "      COUNTER A L0 (3, 0)\n"
           "        COUNTS 1 -> 0\n"
           "        AT 0: (NEXT PRESS)\n"
           "          LOOPS BACK TO COUNTER A L0 (2, 0)");
}

static void RouteNamesBareTargetsAndLocalActions(void)
{
  // A counter at 1 reaches 0 on this press: a plain floor toggled and a rotation behind it
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(1), 200);
  AutoMapRelay rotate = GateRelay(0x0, true);
  rotate.localActionType = 1;
  PutRelay(&snapshot, 3, 0, rotate, 201);
  snapshot.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  snapshot.links.push_back(Message(2, 0, 1, 1, 200, AutoMapSignalToggle, 0, 0));
  snapshot.links.push_back(Message(2, 0, 3, 0, 200, AutoMapSignalClear, 0, 0));
  AutoMapDrawList drawList = DrawAll(snapshot);
  CHECK(JoinedTip(RouteBlock(drawList, 0, 1))
        == "ROUTE:\n"
           "  COUNTER A L0 (2, 0)\n"
           "    COUNTS 1 -> 0\n"
           "    AT 0: (THIS PRESS)\n"
           "      TARGET L0 (1, 1) TOGGLES\n"
           "      GATE A L0 (3, 0), CLEARS BIT 0\n"
           "        BITS 0000 -> 0000, NEEDS 0000\n"
           "        ON MATCH: (THIS PRESS)\n"
           "          ROTATES ACTUATORS");
}

// A party pad on the machine room's floor (1, 1), its record 300, and a link from it
static AutoMapLevelSnapshot PadRoom(int width)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  if (width != snapshot.width)
  {
    AutoMapLevelSnapshot narrow = snapshot;
    snapshot.width = width;
    snapshot.cells.assign((size_t)(width * 3), AutoMapCellRecord());
    for (int y = 0; y < 3; y++)
    {
      for (int x = 0; x < narrow.width; x++) snapshot.cells[y * width + x] = narrow.cells[y * narrow.width + x];
    }
  }
  AutoMapFloorActuator pad = PadWithOp(1, 1, AutoMapPadParty);
  pad.record = 300;
  snapshot.actuators.push_back(pad);
  return snapshot;
}

static AutoMapLink PadMessage(int dstX, int dstY, AutoMapSignal signal, int delay)
{
  AutoMapLink link = Message(1, 1, dstX, dstY, 300, signal, 0, delay);
  link.srcIsWall = false;
  link.srcSide = -1;
  return link;
}

static std::vector<std::string> PadBlock(const AutoMapDrawList &drawList, bool shift)
{
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 1, 1);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return std::vector<std::string>();
  return Texts(AutoMapScene_PressBlock(drawList, sources[0], shift));
}

static void ResultNamesItsDelay(void)
{
  AutoMapLevelSnapshot snapshot = PadRoom(6);
  snapshot.cells[1 * 6 + 5].doorState = AutoMapDoorClosed;
  snapshot.links.push_back(PadMessage(5, 1, AutoMapSignalSet, 6));
  AutoMapDrawList drawList = DrawAll(snapshot);
  CHECK(Joined(PadBlock(drawList, false))
        == "RESULT:\n  DOOR L0 (5, 1) OPENS ON THIS PRESS, AFTER 6 TICKS");
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 1, 1);
  if (sources.size() != 1) return;
  std::vector<AutoMapTipLine> block = AutoMapScene_PressBlock(drawList, sources[0], false);
  CHECK(block.size() == 2);
  if (block.size() != 2) return;
  CHECK(block[0].style == AutoMapTipPlain);
  CHECK(block[1].style == AutoMapTipResult);
  CHECK(!AutoMapScene_PressHasSteps(sources[0]));
}

static void ShiftHintOnlyWithStepsBetween(void)
{
  // Straight to the door: nothing between, no hint
  AutoMapLevelSnapshot direct = PadRoom(6);
  direct.cells[1 * 6 + 5].doorState = AutoMapDoorClosed;
  direct.links.push_back(PadMessage(5, 1, AutoMapSignalSet, 0));
  AutoMapDrawList drawList = DrawAll(direct);
  std::vector<AutoMapTipLine> hint;
  std::vector<AutoMapTipLine> lines =
      AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName, &hint);
  CHECK(hint.empty());
  CHECK(JoinedTip(lines).find("SHIFT") == std::string::npos);
  // Through a counter: the hint, last
  AutoMapLevelSnapshot counted = PadRoom(6);
  counted.cells[1 * 6 + 5].doorState = AutoMapDoorClosed;
  PutRelay(&counted, 2, 0, CounterRelay(2), 200);
  counted.links.push_back(PadMessage(2, 0, AutoMapSignalClear, 0));
  counted.links.push_back(Message(2, 0, 5, 1, 200, AutoMapSignalSet, 0, 0));
  drawList = DrawAll(counted);
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 1, 1);
  CHECK(sources.size() == 1 && AutoMapScene_PressHasSteps(sources[0]));
  lines = AutoMapTooltip_Machinery(drawList, 1, 1, false, TestItemName, &hint);
  CHECK(hint.size() == 1);
  if (hint.size() == 1)
  {
    CHECK(hint[0].text == "SHIFT: ROUTE");
    CHECK(hint[0].style == AutoMapTipHint);
  }
  CHECK(JoinedTip(lines).find("SHIFT") == std::string::npos);
  // Shift held: the route, no hint
  hint.clear();
  lines = AutoMapTooltip_Machinery(drawList, 1, 1, true, TestItemName, &hint);
  CHECK(hint.empty());
  CHECK(JoinedTip(lines).find("  ROUTE:") != std::string::npos);
}

static void ShiftOnACappedDirectListShowsEveryResult(void)
{
  // A pad wired straight to 12 doors at (2..13, 1)
  AutoMapLevelSnapshot snapshot = PadRoom(14);
  for (int x = 2; x < 14; x++)
  {
    snapshot.cells[1 * 14 + x].kind = AutoMapCellDoor;
    snapshot.cells[1 * 14 + x].doorState = AutoMapDoorClosed;
    snapshot.links.push_back(PadMessage(x, 1, AutoMapSignalSet, 0));
  }
  AutoMapDrawList drawList = DrawAll(snapshot);
  std::vector<std::string> compact = PadBlock(drawList, false);
  CHECK(compact.size() == 1 + AUTOMAP_PRESS_MAX_LINES);
  CHECK(!compact.empty() && compact[0] == "RESULT:");
  CHECK(!compact.empty() && compact.back() == "+3 MORE (SHIFT)");
  std::vector<std::string> full = PadBlock(drawList, true);
  CHECK(full.size() == 13);
  CHECK(!full.empty() && full[0] == "RESULT:");
  CHECK(!HasText(full, "ROUTE:") && !HasText(full, "MORE"));
  CHECK(!full.empty() && full.back() == "  DOOR L0 (13, 1) OPENS ON THIS PRESS");
}
static bool SameLink(const AutoMapLink &a, const AutoMapLink &b)
{
  return a.srcX == b.srcX && a.srcY == b.srcY && a.dstX == b.dstX && a.dstY == b.dstY
      && a.source == b.source;
}

static void PressChainLinksCoverTheTree(void)
{
  AutoMapDrawList drawList = DrawAll(L5Machine());
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 3);
  std::vector<AutoMapLink> links = AutoMapScene_PressChainLinks(sources);
  CHECK(links.size() == 4);
  if (links.size() != 4) return;
  // The source link first, then each branch depth first
  CHECK(links[0].source == LEVER_RECORD && links[0].dstX == 2 && links[0].dstY == 3);
  CHECK(links[1].source == 200 && links[1].dstX == 9);
  CHECK(links[2].source == 202 && links[2].dstX == 10);
  CHECK(links[3].source == 201 && links[3].dstX == 4);
  for (size_t i = 0; i < links.size(); i++)
  {
    for (size_t j = i + 1; j < links.size(); j++) CHECK(!SameLink(links[i], links[j]));
  }
  // Two paths over one link list it once, as both counters do behind the lever's link
  CHECK(AutoMapScene_PressChainLinks(std::vector<AutoMapPressSource>()).empty());
  std::vector<AutoMapPressSource> twice = sources;
  twice.push_back(sources[0]);
  CHECK(AutoMapScene_PressChainLinks(twice).size() == 4);
}

static void GoalBadgeWords(void)
{
  AutoMapGoalDistance distance;
  CHECK(AutoMapScene_GoalBadge(distance) == "");
  distance.status = AutoMapGoalOnThisPress;
  CHECK(AutoMapScene_GoalBadge(distance) == "NOW");
  distance.status = AutoMapGoalMore;
  distance.more = 2;
  CHECK(AutoMapScene_GoalBadge(distance) == "2 MORE");
  distance.status = AutoMapGoalNeedsOthers;
  CHECK(AutoMapScene_GoalBadge(distance) == "NEEDS OTHERS");
  distance.bitsOff = 1;
  CHECK(AutoMapScene_GoalBadge(distance) == "1 BIT OFF");
  distance.bitsOff = 2;
  CHECK(AutoMapScene_GoalBadge(distance) == "2 BITS OFF");
  distance.status = AutoMapGoalBlocked;
  CHECK(AutoMapScene_GoalBadge(distance) == "BLOCKED");
  distance.status = AutoMapGoalHolds;
  CHECK(AutoMapScene_GoalBadge(distance) == "HOLD");
  distance.status = AutoMapGoalThroughDSA;
  CHECK(AutoMapScene_GoalBadge(distance) == "DSA");
  distance.status = AutoMapGoalAlreadyDone;
  CHECK(AutoMapScene_GoalBadge(distance) == "DONE");
}

// One goal effect on the machine room's door (or another cell), with the given distance
static std::vector<AutoMapPressSource> GoalSource(int x, int y, AutoMapSignal signal,
                                                  AutoMapGoalStatus status, int more, int bitsOff,
                                                  const char *blocked)
{
  AutoMapEffect goal;
  goal.kind = AutoMapEffectGoal;
  goal.x = x;
  goal.y = y;
  goal.via.signal = signal;
  goal.distance.status = status;
  goal.distance.more = more;
  goal.distance.bitsOff = bitsOff;
  goal.distance.blocked = blocked;
  AutoMapPressSource source;
  source.source = LEVER_RECORD;
  source.srcIsWall = true;
  source.srcSide = AutoMapEast;
  source.effects.push_back(goal);
  return std::vector<AutoMapPressSource>(1, source);
}

// The RESULT line of a lone goal
static std::string ResultLine(const AutoMapLevelSnapshot &snapshot, int x, int y,
                              AutoMapSignal signal, AutoMapGoalStatus status, int more,
                              int bitsOff, const char *blocked)
{
  AutoMapDrawList drawList = DrawAll(snapshot);
  std::vector<std::string> lines = PressText(
      drawList, GoalSource(x, y, signal, status, more, bitsOff, blocked), false);
  CHECK(lines.size() == 2 && lines[0] == "RESULT:");
  return lines.size() == 2 ? lines[1] : std::string();
}

static void PressLinesWordTheGoalDistance(void)
{
  AutoMapLevelSnapshot room = MachineRoom();
  AutoMapSignal set = AutoMapSignalSet;
  CHECK(ResultLine(room, 5, 1, set, AutoMapGoalOnThisPress, -1, 0, NULL)
        == "  DOOR L0 (5, 1) OPENS ON THIS PRESS");
  CHECK(ResultLine(room, 5, 1, AutoMapSignalClear, AutoMapGoalOnThisPress, -1, 0, NULL)
        == "  DOOR L0 (5, 1) CLOSES ON THIS PRESS");
  CHECK(ResultLine(room, 5, 1, AutoMapSignalToggle, AutoMapGoalMore, 2, 0, NULL)
        == "  DOOR L0 (5, 1) TOGGLES IN 2 MORE");
  CHECK(ResultLine(room, 5, 1, set, AutoMapGoalMore, 1, 2, NULL)
        == "  DOOR L0 (5, 1) 2 BITS OFF, 1 MORE");
  CHECK(ResultLine(room, 5, 1, set, AutoMapGoalNeedsOthers, -1, 1, NULL)
        == "  DOOR L0 (5, 1) 1 BIT OFF, NEEDS OTHER SOURCES");
  CHECK(ResultLine(room, 5, 1, set, AutoMapGoalNeedsOthers, -1, 0, NULL)
        == "  DOOR L0 (5, 1) NEEDS OTHER SOURCES");
  CHECK(ResultLine(room, 5, 1, set, AutoMapGoalBlocked, -1, 0, "COUNTER AT 0")
        == "  DOOR L0 (5, 1) BLOCKED: COUNTER AT 0");
  CHECK(ResultLine(room, 5, 1, set, AutoMapGoalHolds, -1, 0, NULL)
        == "  DOOR L0 (5, 1) HOLDS WHILE PRESSED");
  CHECK(ResultLine(room, 5, 1, set, AutoMapGoalThroughDSA, -1, 0, NULL)
        == "  DOOR L0 (5, 1) THROUGH DSA");
  CHECK(ResultLine(room, 5, 1, set, AutoMapGoalAlreadyDone, -1, 0, NULL)
        == "  DOOR L0 (5, 1) ALREADY OPEN");
  CHECK(ResultLine(room, 5, 1, AutoMapSignalClear, AutoMapGoalAlreadyDone, -1, 0, NULL)
        == "  DOOR L0 (5, 1) ALREADY CLOSED");
  // The noun follows the cell
  AutoMapCellKind kinds[] = {AutoMapCellFalseWall, AutoMapCellPit, AutoMapCellTeleporter};
  const char *nouns[] = {"FALSE WALL", "PIT", "TELEPORTER"};
  for (int i = 0; i < 3; i++)
  {
    AutoMapLevelSnapshot other = MachineRoom();
    other.cells[1 * 6 + 5].kind = kinds[i];
    CHECK(ResultLine(other, 5, 1, set, AutoMapGoalOnThisPress, -1, 0, NULL)
          == std::string("  ") + nouns[i] + " L0 (5, 1) OPENS ON THIS PRESS");
  }
  AutoMapLevelSnapshot launcher = MachineRoom();
  AutoMapLauncher fires;
  fires.actuatorType = 8;
  fires.side = AutoMapSouth;
  launcher.cells[0 * 6 + 3].wallLaunchers.push_back(fires);
  CHECK(ResultLine(launcher, 3, 0, set, AutoMapGoalOnThisPress, -1, 0, NULL)
        == "  LAUNCHER L0 (3, 0) OPENS ON THIS PRESS");
}

// The route of one hand-built effect over a relay in the cell (2, 0), below ROUTE:
static std::string RouteOf(const AutoMapRelay &relay, AutoMapEffect effect, bool truncated)
{
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, relay, 200);
  AutoMapDrawList drawList = DrawAll(snapshot);
  effect.x = 2;
  effect.y = 0;
  AutoMapPressSource source;
  source.source = LEVER_RECORD;
  source.truncated = truncated;
  source.effects.push_back(effect);
  std::vector<std::string> lines =
      PressText(drawList, std::vector<AutoMapPressSource>(1, source), true);
  CHECK(lines.size() >= 2 && lines[0] == "ROUTE:");
  if (lines.size() < 2) return std::string();
  return Joined(std::vector<std::string>(lines.begin() + 1, lines.end()));
}

static AutoMapEffect CounterEffect(int before, int after)
{
  AutoMapEffect effect;
  effect.kind = AutoMapEffectCounter;
  effect.relay = 0;
  effect.before = before;
  effect.after = after;
  return effect;
}

static void PressLinesNameClosedSwitchablePits(void)
{
  // DM L1's lever at (6, 22) toggles a pit that looks like floor while closed: at All the
  // press names it a pit and says what the toggle does to it
  AutoMapLevelSnapshot snapshot = MachineRoom();
  snapshot.cells[1 * 6 + 2].kind = AutoMapCellFloor;
  snapshot.cells[1 * 6 + 2].pitIsClosedSwitchable = true;
  snapshot.links.push_back(Message(0, 1, 2, 1, LEVER_RECORD, AutoMapSignalToggle, 0, 0));
  AutoMapDrawList drawList = DrawAll(snapshot);
  std::vector<std::string> lines =
      PressText(drawList, AutoMapScene_PressEffects(drawList, 0, 1), false);
  CHECK(HasText(lines, " PIT L0 (2, 1) OPENS ON THIS PRESS"));
  CHECK(Texts(AutoMapScene_PitLines(drawList, 2, 1)) == std::vector<std::string>(1, "CLOSED"));
  CHECK(Texts(AutoMapTooltip_Machinery(drawList, 2, 1, false, TestItemName))[0] == "PIT");
  // Closing a closed pit does nothing
  snapshot.links.back().signal = AutoMapSignalClear;
  drawList = DrawAll(snapshot);
  lines = PressText(drawList, AutoMapScene_PressEffects(drawList, 0, 1), false);
  CHECK(HasText(lines, " PIT L0 (2, 1) ALREADY CLOSED"));
  // An open pit closes on a toggle, and opening it again does nothing
  AutoMapLevelSnapshot open = MachineRoom();
  open.cells[1 * 6 + 2].kind = AutoMapCellPit;
  open.cells[1 * 6 + 2].pitIsOpen = true;
  open.links.push_back(Message(0, 1, 2, 1, LEVER_RECORD, AutoMapSignalToggle, 0, 0));
  drawList = DrawAll(open);
  lines = PressText(drawList, AutoMapScene_PressEffects(drawList, 0, 1), false);
  CHECK(HasText(lines, " PIT L0 (2, 1) CLOSES ON THIS PRESS"));
  open.links.back().signal = AutoMapSignalSet;
  drawList = DrawAll(open);
  lines = PressText(drawList, AutoMapScene_PressEffects(drawList, 0, 1), false);
  CHECK(HasText(lines, " PIT L0 (2, 1) ALREADY OPEN"));
  // Below All a closed pit is floor and nothing gives it away
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapFog seen = NoFog(explored, visibleNow);
  seen.spoilers = AutoMapSpoilersSeen;
  drawList = AutoMapScene_BuildDrawList(snapshot, seen);
  CHECK(Texts(AutoMapScene_PitLines(drawList, 2, 1)).empty());
  bool flagged = false;
  for (size_t i = 0; i < drawList.cells.size(); i++)
  {
    if (drawList.cells[i].cell.pitIsClosedSwitchable) flagged = true;
  }
  CHECK(!flagged);
}

static void RouteStepMarks(void)
{
  CHECK(RouteOf(CounterRelay(0), CounterEffect(0, 0), false)
        == "  COUNTER A L0 (2, 0)\n    AT 0, DEAD");
  AutoMapRelay once = CounterRelay(2);
  once.onceOnly = true;
  CHECK(RouteOf(once, CounterEffect(2, 1), false)
        == "  COUNTER A L0 (2, 0)\n    COUNTS 2 -> 1\n    ONCE ONLY");
  AutoMapEffect delayed = CounterEffect(2, 1);
  delayed.delayTicks = 3;
  CHECK(RouteOf(CounterRelay(2), delayed, false)
        == "  COUNTER A L0 (2, 0), AFTER 3 TICKS\n    COUNTS 2 -> 1");
  AutoMapEffect guarded = CounterEffect(2, 1);
  guarded.afterDSA = true;
  CHECK(RouteOf(CounterRelay(2), guarded, false)
        == "  COUNTER A L0 (2, 0) (IF DSA PASSES)\n    COUNTS 2 -> 1");
  AutoMapRelay dsa;
  dsa.kind = AutoMapRelayDSA;
  AutoMapEffect dsaEffect;
  dsaEffect.kind = AutoMapEffectDSA;
  dsaEffect.relay = 0;
  CHECK(RouteOf(dsa, dsaEffect, false)
        == "  DSA A L0 (2, 0)\n    IF ITS PROGRAM PASSES IT ON (NOT FOLLOWED)");
  AutoMapEffect loop;
  loop.kind = AutoMapEffectLoop;
  loop.relay = 0;
  CHECK(RouteOf(CounterRelay(1), loop, false) == "  LOOPS BACK TO COUNTER A L0 (2, 0)");
  AutoMapRelay rotate = GateRelay(0x0 | (0x1 << 4), true);
  rotate.localActionType = 1;
  AutoMapEffect local;
  local.kind = AutoMapEffectLocalAction;
  local.relay = 0;
  CHECK(RouteOf(rotate, local, false) == "  ROTATES ACTUATORS");
  rotate.localActionType = 2;
  CHECK(RouteOf(rotate, local, false) == "  ROTATES ACTUATORS");
  rotate.localActionType = 10;
  CHECK(RouteOf(rotate, local, false) == "  SKILL BONUS");
  rotate.localActionType = 0;
  CHECK(RouteOf(rotate, local, false) == "  LOCAL ACTION");
  // The walk ran out of budget: the last line says so
  std::string cut = RouteOf(CounterRelay(2), CounterEffect(2, 1), true);
  CHECK(cut.size() >= 18 && cut.compare(cut.size() - 18, 18, "\n  CHAIN CUT SHORT") == 0);
  // A gate names the bit and what the message does to it, then its bits
  AutoMapRelay gate = GateRelay(0x3 | (0x7 << 4), false);
  gate.onceOnly = true;
  AutoMapEffect arrival;
  arrival.kind = AutoMapEffectGate;
  arrival.relay = 0;
  arrival.before = 3;
  arrival.after = 7;
  arrival.via.targetPos = 2;
  arrival.via.signal = AutoMapSignalToggle;
  CHECK(RouteOf(gate, arrival, false)
        == "  GATE A L0 (2, 0), TOGGLES BIT 2\n    BITS 0011 -> 0111, NEEDS 0111\n    ONCE ONLY");
  // A held message names what it did to the bit
  arrival.via.signal = AutoMapSignalPressure;
  CHECK(RouteOf(GateRelay(0x3 | (0x7 << 4), false), arrival, false)
        == "  GATE A L0 (2, 0), SETS BIT 2\n    BITS 0011 -> 0111, NEEDS 0111");
  arrival.after = 3;
  arrival.before = 7;
  CHECK(RouteOf(GateRelay(0x3 | (0x7 << 4), false), arrival, false)
        == "  GATE A L0 (2, 0), CLEARS BIT 2\n    BITS 0111 -> 0011, NEEDS 0111");
}

static void PressLinesFoldIdenticalReceiversAndCap(void)
{
  AutoMapLevelSnapshot triple = MachineRoom();
  for (int i = 0; i < 3; i++) PutRelay(&triple, 2, 0, CounterRelay(4), 200 + i);
  triple.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  AutoMapDrawList drawList = DrawAll(triple);
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  // Each counter keeps its own letter, so three equal counters stay three lines
  CHECK(Joined(PressText(drawList, sources, false))
        == "THIS PRESS:\n  L0 (2, 0) COUNTER A 4->3\n  L0 (2, 0) COUNTER B 4->3\n"
           "  L0 (2, 0) COUNTER C 4->3");
  // Two messages into one dead counter say the same thing twice, and fold
  AutoMapLevelSnapshot dead = MachineRoom();
  PutRelay(&dead, 2, 0, CounterRelay(0), 200);
  dead.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  dead.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 1, 0));
  drawList = DrawAll(dead);
  sources = AutoMapScene_PressEffects(drawList, 0, 1);
  CHECK(Joined(PressText(drawList, sources, true))
        == "ROUTE:\n  2X COUNTER A L0 (2, 0)\n    AT 0, DEAD");
  // A hub with 14 receivers: 10 lines under the headers, the last one the rest
  AutoMapLevelSnapshot hub = MachineRoom();
  for (int i = 0; i < 14; i++) PutRelay(&hub, 2, 0, CounterRelay(5 + i), 200 + i);
  hub.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  drawList = DrawAll(hub);
  sources = AutoMapScene_PressEffects(drawList, 0, 1);
  {
    std::vector<std::string> lines = PressText(drawList, sources, false);
    size_t body = 0;
    for (size_t i = 0; i < lines.size(); i++)
    {
      if (!lines[i].empty() && lines[i][0] == ' ') body++;
      else if (lines[i].find("MORE") != std::string::npos) body++;
    }
    CHECK(body == AUTOMAP_PRESS_MAX_LINES);
    CHECK(!lines.empty() && lines.back() == "+5 MORE (SHIFT)");
    CHECK(!HasText(lines, "SHIFT:"));
    CHECK(HasText(lines, "COUNTER A 5->4") && HasText(lines, "COUNTER I 13->12"));
    CHECK(!HasText(lines, "COUNTER J 14->13"));
  }
  // Nine lines fit
  AutoMapLevelSnapshot nine = MachineRoom();
  for (int i = 0; i < 9; i++) PutRelay(&nine, 2, 0, CounterRelay(5 + i), 200 + i);
  nine.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  drawList = DrawAll(nine);
  std::vector<std::string> fits =
      PressText(drawList, AutoMapScene_PressEffects(drawList, 0, 1), false);
  CHECK(fits.size() == 1 + 9);
  CHECK(!HasText(fits, "SHIFT:") && !HasText(fits, "MORE"));
  // Ten lines still fit: the hint is the tooltip's, not the block's, and nothing is cut
  PutRelay(&nine, 2, 0, CounterRelay(30), 300);
  drawList = DrawAll(nine);
  fits = PressText(drawList, AutoMapScene_PressEffects(drawList, 0, 1), false);
  CHECK(fits.size() == 1 + 10);
  CHECK(!HasText(fits, "SHIFT:") && !HasText(fits, "MORE"));
}

static void PressLinesGroupSources(void)
{
  // Switches on two faces of one cell press on their own, from the live state
  AutoMapLevelSnapshot snapshot = MachineRoom();
  PutRelay(&snapshot, 2, 0, CounterRelay(4), 200);
  AutoMapLink east = Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0);
  AutoMapLink south = Message(0, 1, 2, 0, LEVER_RECORD + 1, AutoMapSignalClear, 0, 0);
  south.srcSide = AutoMapSouth;
  snapshot.links.push_back(east);
  snapshot.links.push_back(south);
  snapshot.cells[1 * 6 + 0].wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  AutoMapDrawList drawList = DrawAll(snapshot);
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  CHECK(sources.size() == 2);
  if (sources.size() != 2) return;
  // Each face is its own group in the tooltip (the PAD header is the tooltip's too, see
  // SameTriggerPadsShareOnePadGroup); the counter between asks for the route hint once
  std::vector<AutoMapTipLine> hint;
  CHECK(JoinedTip(AutoMapTooltip_Machinery(drawList, 0, 1, true, TestItemName, &hint))
        == "SWITCH (E)\n  ROUTE:\n    COUNTER A L0 (2, 0)\n      COUNTS 4 -> 3\n"
           "SWITCH (S)\n  ROUTE:\n    COUNTER A L0 (2, 0)\n      COUNTS 4 -> 3");
  CHECK(hint.empty());
  CHECK(JoinedTip(AutoMapTooltip_Machinery(drawList, 0, 1, false, TestItemName, &hint))
        == "SWITCH (E)\n  THIS PRESS:\n    L0 (2, 0) COUNTER A 4->3\n"
           "SWITCH (S)\n  THIS PRESS:\n    L0 (2, 0) COUNTER A 4->3");
  CHECK(JoinedTip(hint) == "SHIFT: ROUTE");
}

static void RelayHoverLinesLetterRelays(void)
{
  AutoMapLevelSnapshot snapshot = L5Machine();
  AutoMapFloorActuator pad(3, 3, AutoMapActuatorPad);
  pad.record = 300;
  snapshot.actuators.push_back(pad);
  AutoMapLink padLink = Message(3, 3, 2, 3, 300, AutoMapSignalClear, 0, 0);
  padLink.srcIsWall = false;
  snapshot.links.push_back(padLink);
  AutoMapDrawList drawList = DrawAll(snapshot);
  // Every message to the cell reaches both counters, so the pad counts both down too
  CHECK(Joined(AutoMapScene_RelayHoverLines(drawList, 2, 3, false))
        == "STONE: 2 COUNTERS\n A COUNT 4 -> L5 (25, 26) GATE A BIT 2 SET AT 0\n"
           " B COUNT 3 -> L5 (20, 29) DOOR OPEN AT 0\nFED BY:\n"
           " L5 (16, 28) SWITCH (E) A-1 B-1\n L5 (19, 28) PAD A-1 B-1");
  // A gate shows its bits and what the counter feeding it does to which bit
  CHECK(Joined(AutoMapScene_RelayHoverLines(drawList, 9, 1, false))
        == "STONE: 1 GATE\n A GATE 1011 (NEEDS 1111) -> L5 (26, 26) DOOR OPEN ON MATCH AFTER 6 TICKS\n"
           "FED BY:\n L5 (18, 28) COUNTER A BIT 2 SET");
  // A mixed cell names each kind
  AutoMapLevelSnapshot mixed = L5Machine();
  PutRelay(&mixed, 2, 3, GateRelay(0x3, false), 203);
  mixed.links.push_back(Message(0, 3, 2, 3, LEVER_RECORD, AutoMapSignalToggle, 1, 0));
  drawList = DrawAll(mixed);
  std::vector<std::string> lines = AutoMapScene_RelayHoverLines(drawList, 2, 3, false);
  CHECK(!lines.empty() && lines[0] == "STONE: 2 COUNTERS, 1 GATE");
  CHECK(HasText(lines, " C GATE 0011 (NEEDS 0000)"));
  CHECK(HasText(lines, "C BIT 1 TOGGLE"));
  // A message reaches every gate of its target cell: the target names them all
  AutoMapLevelSnapshot bank = L5Machine();
  PutRelay(&bank, 9, 1, GateRelay(0x0 | (0xF << 4), false), 204);
  drawList = DrawAll(bank);
  CHECK(HasText(AutoMapScene_RelayHoverLines(drawList, 2, 3, false),
                " A COUNT 4 -> L5 (25, 26) GATES A/B BIT 2 SET AT 0"));
  // A relay aimed at a counter cell names the counters it counts
  AutoMapLevelSnapshot chained = L5Machine();
  PutRelay(&chained, 6, 1, CounterRelay(2), 205);
  chained.links.push_back(Message(9, 1, 6, 1, 202, AutoMapSignalClear, 0, 0));
  drawList = DrawAll(chained);
  CHECK(HasText(AutoMapScene_RelayHoverLines(drawList, 9, 1, false),
                " A GATE 1011 (NEEDS 1111) -> L5 (22, 26) COUNTER A -1 ON MATCH"));
  // SET counts up
  AutoMapLevelSnapshot up = L5Machine();
  up.links[0].signal = AutoMapSignalSet;
  drawList = DrawAll(up);
  CHECK(HasText(AutoMapScene_RelayHoverLines(drawList, 2, 3, false), "SWITCH (E) A+1 B+1"));
  // A normally-closed CONSTPRESSURE alcove sends CLEAR when an item goes in: it counts down
  AutoMapLevelSnapshot alcove = L5Machine();
  alcove.links[0].signal = AutoMapSignalPressure;
  alcove.links[0].sourceNormallyClosed = true;
  drawList = DrawAll(alcove);
  CHECK(HasText(AutoMapScene_RelayHoverLines(drawList, 2, 3, false), "SWITCH (E) A-1 B-1"));
  // Into a gate it names the bit and the signal it really sends, not HOLD
  AutoMapLevelSnapshot gated = L5Machine();
  PutRelay(&gated, 2, 3, GateRelay(0x3, false), 203);
  AutoMapLink held = Message(0, 3, 2, 3, LEVER_RECORD, AutoMapSignalPressure, 1, 0);
  held.sourceNormallyClosed = true;
  gated.links.push_back(held);
  drawList = DrawAll(gated);
  CHECK(HasText(AutoMapScene_RelayHoverLines(drawList, 2, 3, false), "C BIT 1 CLEAR"));
}

static void RelayHoverLinesStaySilent(void)
{
  AutoMapDrawList drawList = DrawAll(L5Machine());
  // No relay, an unexplored cell
  CHECK(AutoMapScene_RelayHoverLines(drawList, 0, 3, false).empty());
  CHECK(AutoMapScene_RelayHoverLines(drawList, 99, 99, false).empty());
  // Below All the machinery is no one's business
  drawList.spoilers = AutoMapSpoilersSeen;
  CHECK(AutoMapScene_RelayHoverLines(drawList, 2, 3, false).empty());
  drawList.spoilers = AutoMapSpoilersLearned;
  CHECK(AutoMapScene_RelayHoverLines(drawList, 2, 3, false).empty());
}

static void GlyphGreaterThanDraws(void)
{
  const int rows[5] = {0x4, 0x2, 0x1, 0x2, 0x4};
  for (int row = 0; row < 5; row++)
  {
    for (int col = 0; col < 3; col++)
    {
      CHECK(AutoMapScene_FontPixel('>', row, col) == ((rows[row] & (1 << (2 - col))) != 0));
    }
  }
}

static void PressLinesCutShortSurvivesTheCap(void)
{
  AutoMapLevelSnapshot hub = MachineRoom();
  for (int i = 0; i < 14; i++) PutRelay(&hub, 2, 0, CounterRelay(5 + i), 200 + i);
  hub.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  AutoMapDrawList drawList = DrawAll(hub);
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  CHECK(sources.size() == 1);
  if (sources.size() != 1) return;
  sources[0].truncated = true;
  std::vector<std::string> lines = PressText(drawList, sources, false);
  CHECK(lines.size() >= 3);
  if (lines.size() >= 3)
  {
    CHECK(lines[lines.size() - 2] == "+5 MORE (SHIFT)");
    CHECK(lines.back() == "  CHAIN CUT SHORT");
    size_t body = 0;
    for (size_t i = 0; i < lines.size(); i++)
    {
      if (!lines[i].empty() && lines[i][0] == ' ' && lines[i] != "  CHAIN CUT SHORT") body++;
      else if (lines[i].find("MORE") != std::string::npos) body++;
    }
    CHECK(body == AUTOMAP_PRESS_MAX_LINES);
  }
  // Uncapped, the tail still ends the block
  lines = PressText(drawList, sources, true);
  CHECK(!HasText(lines, "MORE"));
  CHECK(!lines.empty() && lines.back() == "  CHAIN CUT SHORT");
}

static void RelayHoverLinesCapTheTargets(void)
{
  // A counter feeding 12 targets, fed by one lever
  AutoMapLevelSnapshot snapshot = L5Machine();
  for (int i = 0; i < 12; i++)
  {
    snapshot.links.push_back(Message(2, 3, 4, 4, 200, AutoMapSignalSet, 0, i + 1));
  }
  AutoMapDrawList drawList = DrawAll(snapshot);
  std::vector<std::string> lines = AutoMapScene_RelayHoverLines(drawList, 2, 3, false);
  // A has 13 targets (its gate and the 12), B one, so 14 letter lines and 1 feeder: 15
  size_t body = 0;
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (lines[i] != "FED BY:" && lines[i].compare(0, 6, "STONE:") != 0) body++;
  }
  CHECK(body == AUTOMAP_PRESS_MAX_LINES);
  CHECK(!lines.empty() && lines.back() == "+6 MORE");
  CHECK(lines[0] == "STONE: 2 COUNTERS");
}

static void PressLinesFullChainIsUncapped(void)
{
  AutoMapLevelSnapshot hub = MachineRoom();
  for (int i = 0; i < 14; i++) PutRelay(&hub, 2, 0, CounterRelay(5 + i), 200 + i);
  hub.links.push_back(Message(0, 1, 2, 0, LEVER_RECORD, AutoMapSignalClear, 0, 0));
  AutoMapDrawList drawList = DrawAll(hub);
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, 0, 1);
  std::vector<std::string> lines = PressText(drawList, sources, true);
  CHECK(!HasText(lines, "MORE"));
  CHECK(HasText(lines, "COUNTER A L0 (2, 0)") && HasText(lines, "COUNTS 5 -> 4"));
  CHECK(HasText(lines, "COUNTER N L0 (2, 0)") && HasText(lines, "COUNTS 18 -> 17"));
  // A step line and a state line for each of the 14
  size_t body = 0;
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (!lines[i].empty() && lines[i][0] == ' ') body++;
  }
  CHECK(body == 28);
}

static void RelayHoverLinesFullIsUncapped(void)
{
  AutoMapLevelSnapshot snapshot = L5Machine();
  for (int i = 0; i < 12; i++)
  {
    snapshot.links.push_back(Message(2, 3, 4, 4, 200, AutoMapSignalSet, 0, i + 1));
  }
  AutoMapDrawList drawList = DrawAll(snapshot);
  std::vector<std::string> capped = AutoMapScene_RelayHoverLines(drawList, 2, 3, false);
  std::vector<std::string> full = AutoMapScene_RelayHoverLines(drawList, 2, 3, true);
  CHECK(HasText(capped, "MORE"));
  CHECK(!HasText(full, "MORE"));
  CHECK(full.size() > capped.size());
  // 14 letter lines and the 1 feeder, plus the STONE header, FED BY and nothing else
  CHECK(full.size() == 17);
}

static void NearestSidePicksTheEdge(void)
{
  // Each edge's mid-point, on a 16-pixel cell (offsets 0..15)
  CHECK(AutoMapScene_NearestSide(7, 0, 16) == AutoMapNorth);
  CHECK(AutoMapScene_NearestSide(15, 7, 16) == AutoMapEast);
  CHECK(AutoMapScene_NearestSide(7, 15, 16) == AutoMapSouth);
  CHECK(AutoMapScene_NearestSide(0, 7, 16) == AutoMapWest);
  // Near an edge but not on it
  CHECK(AutoMapScene_NearestSide(8, 3, 16) == AutoMapNorth);
  CHECK(AutoMapScene_NearestSide(12, 8, 16) == AutoMapEast);
  CHECK(AutoMapScene_NearestSide(8, 12, 16) == AutoMapSouth);
  CHECK(AutoMapScene_NearestSide(3, 8, 16) == AutoMapWest);
  // Corners tie two edges: north, then east, then south, then west
  CHECK(AutoMapScene_NearestSide(0, 0, 16) == AutoMapNorth);
  CHECK(AutoMapScene_NearestSide(15, 0, 16) == AutoMapNorth);
  CHECK(AutoMapScene_NearestSide(15, 15, 16) == AutoMapEast);
  CHECK(AutoMapScene_NearestSide(0, 15, 16) == AutoMapSouth);
  // The diagonals tie too, and the centre of an odd cell ties all four
  CHECK(AutoMapScene_NearestSide(5, 5, 16) == AutoMapNorth);
  CHECK(AutoMapScene_NearestSide(10, 10, 16) == AutoMapEast);
  CHECK(AutoMapScene_NearestSide(4, 11, 16) == AutoMapSouth);
  CHECK(AutoMapScene_NearestSide(2, 2, 5) == AutoMapNorth);
  // A one-pixel cell has every edge at distance 0
  CHECK(AutoMapScene_NearestSide(0, 0, 1) == AutoMapNorth);
}

static const char *FoodItemName(int nameIndex)
{
  if (nameIndex == 7) return "BREAD";
  if (nameIndex == 8) return "CHEESE";
  if (nameIndex == 9) return "FIRESTAFF";
  return "";
}

static bool FoodTest(int nameIndex)
{
  return nameIndex == 7 || nameIndex == 8;
}

// BREAD and a FIRESTAFF on (1, 1), CHEESE on (2, 1), a mummy on (1, 1) carrying BREAD
static AutoMapDrawList FoodCorridor(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[1 * 4 + 1].itemPictures.push_back(NamedItem(360, 0, 7));
  snapshot.cells[1 * 4 + 1].itemPictures.push_back(NamedItem(361, 1, 9));
  snapshot.cells[1 * 4 + 2].itemPictures.push_back(NamedItem(362, 2, 8));
  AutoMapMonsterRecord carrier = MummyAt(1, 1);
  carrier.possessionPictures.push_back(NamedItem(360, 0, 7));
  carrier.possessionTexts.push_back("");
  snapshot.monsters.push_back(carrier);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  return AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
}

static int CountNamed(const std::vector<AutoMapSearchHit> &hits, const char *name, bool carried)
{
  int count = 0;
  for (size_t i = 0; i < hits.size(); i++)
  {
    if (hits[i].name == name && hits[i].monsterCarried == carried) count++;
  }
  return count;
}

static void FoodFindsEveryFoodItem(void)
{
  std::vector<AutoMapSearchHit> hits =
      AutoMapScene_SearchItems(FoodCorridor(), "food", FoodItemName, FoodTest);
  CHECK(hits.size() == 3);
  CHECK(CountNamed(hits, "BREAD", false) == 1);
  CHECK(CountNamed(hits, "CHEESE", false) == 1);
  CHECK(CountNamed(hits, "BREAD", true) == 1);
  for (size_t i = 0; i < hits.size(); i++) CHECK(AutoMapScene_SearchHitRank(hits[i], "food") == 0);
}

static void FoodWordMatchesByPrefix(void)
{
  CHECK(AutoMapScene_SearchItems(FoodCorridor(), "fo", FoodItemName, FoodTest).size() == 3);
}

static void AFoodItemIsListedOnceWhenItsNameMatchesToo(void)
{
  // "f" begins FOOD and FIRESTAFF; "d" lies in FOOD and in BREAD
  std::vector<AutoMapSearchHit> f =
      AutoMapScene_SearchItems(FoodCorridor(), "f", FoodItemName, FoodTest);
  CHECK(f.size() == 4);
  CHECK(CountNamed(f, "FIRESTAFF", false) == 1);
  std::vector<AutoMapSearchHit> d =
      AutoMapScene_SearchItems(FoodCorridor(), "d", FoodItemName, FoodTest);
  CHECK(d.size() == 3);
  CHECK(CountNamed(d, "BREAD", false) == 1);
  CHECK(CountNamed(d, "BREAD", true) == 1);
}

static void FoodNamesStillMatch(void)
{
  std::vector<AutoMapSearchHit> hits =
      AutoMapScene_SearchItems(FoodCorridor(), "bread", FoodItemName, FoodTest);
  CHECK(hits.size() == 2);
  for (size_t i = 0; i < hits.size(); i++) CHECK(AutoMapScene_SearchHitRank(hits[i], "bread") == 0);
}

static void ANullFoodTestFindsNoFoodByTheWord(void)
{
  CHECK(AutoMapScene_SearchItems(FoodCorridor(), "food", FoodItemName, NULL).empty());
}

// BREAD and CHEESE on (1, 1), in the draw-list order given
static AutoMapDrawList BreadAndCheese(bool breadFirst)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  std::vector<AutoMapItem> &pile = snapshot.cells[1 * 4 + 1].itemPictures;
  pile.push_back(NamedItem(360, 0, breadFirst ? 7 : 8));
  pile.push_back(NamedItem(362, 2, breadFirst ? 8 : 7));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  return AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
}

static void HitsOnOneCellKeepOneOrder(void)
{
  std::vector<AutoMapSearchHit> a =
      AutoMapScene_SearchItems(BreadAndCheese(true), "food", FoodItemName, FoodTest);
  std::vector<AutoMapSearchHit> b =
      AutoMapScene_SearchItems(BreadAndCheese(false), "food", FoodItemName, FoodTest);
  CHECK(a.size() == 2 && b.size() == 2);
  CHECK(a.size() == 2 && a[0].name == "BREAD" && a[1].name == "CHEESE");
  CHECK(b.size() == 2 && b[0].name == "BREAD" && b[1].name == "CHEESE");
}

static void SortingHitsIgnoresTheirOrderBefore(void)
{
  std::vector<AutoMapSearchHit> hits =
      AutoMapScene_SearchItems(FoodCorridor(), "food", FoodItemName, FoodTest);
  std::vector<AutoMapSearchHit> reversed(hits.rbegin(), hits.rend());
  AutoMapScene_SortSearchHits(&reversed, "food");
  CHECK(reversed.size() == hits.size());
  for (size_t i = 0; i < hits.size() && i < reversed.size(); i++)
  {
    CHECK(reversed[i].name == hits[i].name && reversed[i].x == hits[i].x
          && reversed[i].monsterCarried == hits[i].monsterCarried);
  }
}

static const char *WaterItemName(int nameIndex)
{
  if (nameIndex == 5) return "WATER";
  return "";
}

// Fog off; the wall north of (1, 1) holds the feature on its south face, and a WATER flask lies
// on (2, 1)
static AutoMapDrawList CorridorWithFeature(AutoMapWallFeature feature)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.cells[0 * 4 + 1].wallSides[AutoMapSouth].feature = feature;
  snapshot.cells[1 * 4 + 2].itemPictures.push_back(NamedItem(363, 3, 5));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  return AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
}

static void FountainFindsTheFountainFace(void)
{
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(
      CorridorWithFeature(AutoMapWallFeatureFountain), "fountain", WaterItemName, NULL);
  CHECK(hits.size() == 1);
  if (hits.size() != 1) return;
  CHECK(hits[0].name == "WATER FOUNTAIN");
  CHECK(hits[0].x == 1 && hits[0].y == 0);
  CHECK(hits[0].slot.side == AutoMapSouth);
  CHECK(hits[0].wallFeature == AutoMapWallFeatureFountain);
  CHECK(!hits[0].monsterCarried);
}

// Stairs are found as their cell, with the way they lead and where to
static void StairsFindsBothWays(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &down = snapshot.cells[1 * 4 + 1];
  down.kind = AutoMapCellStairs;
  down.stairsLeadUp = false;
  down.stairsTargetLevel = 4;
  AutoMapCellRecord &up = snapshot.cells[1 * 4 + 2];
  up.kind = AutoMapCellStairs;
  up.stairsLeadUp = true;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, NoFog(explored, visibleNow));
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(drawList, "stairs", WaterItemName, NULL);
  CHECK(hits.size() == 2);
  if (hits.size() != 2) return;
  CHECK(hits[0].name == "STAIRS DOWN" && hits[0].x == 1 && hits[0].y == 1);
  CHECK(hits[0].kind == AutoMapHitFeature && hits[0].stairsTarget == 4);
  CHECK(hits[1].name == "STAIRS UP" && hits[1].x == 2);
  CHECK(AutoMapScene_SearchItems(drawList, "stairs up", WaterItemName, NULL).size() == 1);
}

static void WaterFindsFlasksAndFountains(void)
{
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(
      CorridorWithFeature(AutoMapWallFeatureFountain), "water", WaterItemName, NULL);
  CHECK(hits.size() == 2);
}

static void AltarAndViFindTheAltar(void)
{
  AutoMapDrawList drawList = CorridorWithFeature(AutoMapWallFeatureViAltar);
  const char *queries[2] = {"altar", "vi"};
  for (int i = 0; i < 2; i++)
  {
    std::vector<AutoMapSearchHit> hits =
        AutoMapScene_SearchItems(drawList, queries[i], WaterItemName, NULL);
    CHECK(hits.size() == 1);
    CHECK(hits.size() == 1 && hits[0].name == "ALTAR OF VI");
    CHECK(hits.size() == 1 && hits[0].wallFeature == AutoMapWallFeatureViAltar);
  }
  CHECK(AutoMapScene_SearchItems(drawList, "fountain", WaterItemName, NULL).empty());
}

static void APlainFaceIsNoFeature(void)
{
  CHECK(AutoMapScene_SearchItems(CorridorWithFeature(AutoMapWallFeatureNone), "fountain",
                                 WaterItemName, NULL).empty());
}

// A recruited champion's mirror loses its portrait (the engine turns the actuator off); the
// champion was still seen and viewed
static void RecruitedChampionStaysSeenAndViewed(void)
{
  AutoMapWallSide mirror;
  mirror.portrait = AUTOMAP_NO_PICTURE;
  mirror.hasWriting = false;
  AutoMapWallCounts counts = {0, 0, 0};
  AutoMapScene_CountWallSide(mirror, true, true, &counts);
  CHECK(counts.portraitsSeen == 1);
  CHECK(counts.portraitsViewed == 1);
  CHECK(counts.textsRead == 0);
}

static void MirrorOnlySeenIsNotViewed(void)
{
  AutoMapWallSide mirror;
  mirror.portrait = 7;
  mirror.hasWriting = false;
  AutoMapWallCounts counts = {0, 0, 0};
  AutoMapScene_CountWallSide(mirror, true, false, &counts);
  CHECK(counts.portraitsSeen == 1);
  CHECK(counts.portraitsViewed == 0);
  AutoMapScene_CountWallSide(mirror, false, false, &counts);
  CHECK(counts.portraitsSeen == 1);
}

static void PlainWallCountsNothing(void)
{
  AutoMapWallSide wall;
  wall.portrait = AUTOMAP_NO_PICTURE;
  wall.hasWriting = true;
  AutoMapWallCounts counts = {0, 0, 0};
  AutoMapScene_CountWallSide(wall, false, false, &counts);
  CHECK(counts.textsRead == 0 && counts.portraitsSeen == 0 && counts.portraitsViewed == 0);
  AutoMapScene_CountWallSide(wall, true, false, &counts);
  CHECK(counts.textsRead == 1 && counts.portraitsSeen == 0);
}

// Stepping to another level centres the view there only once that level is on screen: the
// level still shown keeps its own view until then
static void PendingCentreWaitsForItsLevel(void)
{
  AutoMapPendingCentre pending = AutoMapScene_NoPendingCentre();
  AutoMapScene_RequestCentre(&pending, 1, 17.0f, 10.0f);
  float x = -1, y = -1;
  CHECK(!AutoMapScene_TakeCentre(&pending, 0, &x, &y));
  CHECK(x == -1 && y == -1);
  CHECK(AutoMapScene_TakeCentre(&pending, 1, &x, &y));
  CHECK(x == 17.0f && y == 10.0f);
  // Taken once
  CHECK(!AutoMapScene_TakeCentre(&pending, 1, &x, &y));
}

static void NewerRequestReplacesThePendingCentre(void)
{
  AutoMapPendingCentre pending = AutoMapScene_NoPendingCentre();
  AutoMapScene_RequestCentre(&pending, 1, 17.0f, 10.0f);
  AutoMapScene_RequestCentre(&pending, 2, 3.0f, 4.0f);
  float x = -1, y = -1;
  CHECK(!AutoMapScene_TakeCentre(&pending, 1, &x, &y));
  CHECK(AutoMapScene_TakeCentre(&pending, 2, &x, &y));
  CHECK(x == 3.0f && y == 4.0f);
}

// The tooltip names the group under the pointer the way the player reads it, with its size
static void MonsterNameLineNamesTheGroup(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  AutoMapMonsterRecord group = MummyAt(2, 1);
  group.memberCount = 4;
  group.name = "Trolin";
  AutoMapMonsterRecord elemental = MummyAt(1, 1);
  elemental.name = "Water Elemental";
  corridor.monsters.push_back(group);
  corridor.monsters.push_back(elemental);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  CHECK(AutoMapScene_MonsterNameLine(drawList, 2, 1) == "TROLIN X4");
  CHECK(AutoMapScene_MonsterNameLine(drawList, 1, 1) == "WATER ELEMENTAL");
  CHECK(AutoMapScene_MonsterNameLine(drawList, 0, 0) == "");
  // Under the fog a group the party does not see right now is not on the map, so not named
  AutoMapDrawList fogged = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_MonsterNameLine(fogged, 2, 1) == "");
}

// A false wall says what it is, and that a switch operates it when one does
static void SwitchedFalseWallSaysSo(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  corridor.cells[0 * 4 + 2].kind = AutoMapCellFalseWall;
  corridor.cells[0 * 4 + 3].kind = AutoMapCellFalseWall;
  AutoMapFloorActuator pad = {1, 1, AutoMapActuatorPad};
  corridor.actuators.push_back(pad);
  AutoMapLink toWall = {1, 1, 2, 0, false, -1};
  corridor.links.push_back(toWall);
  AutoMapCell linked = {LEVEL, 2, 0};
  corridor.linkedCells.push_back(linked);
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  std::vector<std::string> switched = Texts(AutoMapScene_RelayLines(drawList, 2, 0));
  std::vector<std::string> plain = Texts(AutoMapScene_RelayLines(drawList, 3, 0));
  CHECK(switched.size() == 2 && switched[0] == "FALSE WALL" && switched[1] == "  SWITCHED");
  CHECK(plain.size() == 1 && plain[0] == "FALSE WALL");
  CHECK(AutoMapScene_RelayLines(drawList, 3, 0)[0].style == AutoMapTipHeader);
}

// Hovering a teleporter's destination shows where the party would arrive from: the firing
// teleporters of this level whose target the fog lets the map name
static void TeleportsIntoACell(void)
{
  AutoMapLevelSnapshot corridor = CorridorWithTeleporter();
  corridor.cells[0 * 4 + 2].kind = AutoMapCellTeleporter;
  corridor.cells[0 * 4 + 2].hasTeleporterTarget = true;
  corridor.cells[0 * 4 + 2].teleporterDestLevel = LEVEL;
  corridor.cells[0 * 4 + 2].teleporterDestX = 2;
  corridor.cells[0 * 4 + 2].teleporterDestY = 1;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  std::vector<AutoMapLink> into = AutoMapScene_TeleportsInto(drawList, 2, 1);
  CHECK(into.size() == 2);
  CHECK(into.size() == 2 && into[0].dstX == 2 && into[0].dstY == 1);
  CHECK(AutoMapScene_TeleportsInto(drawList, 1, 1).empty());
  // Under the fog with the destination unexplored, the map may not name where they lead
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapDrawList fogged = AutoMapScene_BuildDrawList(corridor, FogOver(explored, visibleNow));
  CHECK(AutoMapScene_TeleportsInto(fogged, 2, 1).empty());
}

// Stairs lead to the map at the next depth that covers the same spot, as the engine moves the
// party (IncrementLevel): in a dungeon with two maps at one depth that need not be level + 1
static void StairsLeadToTheMapAtTheNextDepth(void)
{
  std::vector<AutoMapLevelArea> areas;
  AutoMapLevelArea wide = {1, 0, 0, 30, 10};
  AutoMapLevelArea west = {2, 0, 0, 10, 10};
  AutoMapLevelArea east = {2, 20, 0, 10, 10};
  AutoMapLevelArea above = {0, 0, 0, 30, 10};
  areas.push_back(wide);
  areas.push_back(west);
  areas.push_back(east);
  areas.push_back(above);
  CHECK(AutoMapScene_StairsTarget(areas, 0, false, 5, 5) == 1);
  CHECK(AutoMapScene_StairsTarget(areas, 0, false, 25, 5) == 2);
  CHECK(AutoMapScene_StairsTarget(areas, 0, false, 15, 5) == AUTOMAP_NO_LEVEL);
  CHECK(AutoMapScene_StairsTarget(areas, 2, true, 25, 5) == 0);
  CHECK(AutoMapScene_StairsTarget(areas, 0, true, 5, 5) == 3);
  CHECK(AutoMapScene_StairsTarget(areas, 9, true, 5, 5) == AUTOMAP_NO_LEVEL);
}

// A stack of the same item reads as one line with its count after the name
static void StackNameLine(void)
{
  CHECK(AutoMapScene_StackLine("COPPER COIN", 5) == "COPPER COIN X5");
  CHECK(AutoMapScene_StackLine("DAGGER", 1) == "DAGGER");
}

// The bar's arrows and its dropdown mark are in the map's 3x5 font
static void FontHasTheBarsMarks(void)
{
  bool anyLess = false, anyTilde = false;
  for (int row = 0; row < 5; row++)
    for (int col = 0; col < 3; col++)
    {
      anyLess = anyLess || AutoMapScene_FontPixel('<', row, col);
      anyTilde = anyTilde || AutoMapScene_FontPixel('~', row, col);
    }
  CHECK(anyLess);
  CHECK(anyTilde);
  CHECK(AutoMapScene_FontPixel('<', 2, 0) && !AutoMapScene_FontPixel('<', 2, 2));
}

static AutoMapLevelSnapshot CorridorWithTrolins(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  AutoMapMonsterRecord group = MummyAt(2, 1);
  group.memberCount = 3;
  group.name = "Trolin";
  corridor.monsters.push_back(group);
  AutoMapFloorActuator generator = {1, 1, AutoMapActuatorGenerator};
  generator.hasGeneratedMonster = true;
  generator.generatedMonsterName = "Trolin";
  corridor.actuators.push_back(generator);
  return corridor;
}

// Monsters are found by name, generators by what they create; GEN lists every generator and
// three letters of MONSTER every group
static void SearchFindsMonstersAndGenerators(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(CorridorWithTrolins(), NoFog(explored, visibleNow));
  std::vector<AutoMapSearchHit> trolins = AutoMapScene_SearchItems(drawList, "trolin", SearchItemName, NULL);
  CHECK(trolins.size() == 2);
  bool monster = false, generator = false;
  for (size_t i = 0; i < trolins.size(); i++)
  {
    if (trolins[i].kind == AutoMapHitMonster && trolins[i].count == 3 && trolins[i].x == 2) monster = true;
    if (trolins[i].kind == AutoMapHitGenerator && trolins[i].x == 1) generator = true;
  }
  CHECK(monster && generator);
  std::vector<AutoMapSearchHit> gen = AutoMapScene_SearchItems(drawList, "GEN", SearchItemName, NULL);
  CHECK(gen.size() == 1 && gen[0].kind == AutoMapHitGenerator && gen[0].name == "TROLIN");
  CHECK(AutoMapScene_SearchItems(drawList, "generator", SearchItemName, NULL).size() == 1);
  std::vector<AutoMapSearchHit> mon = AutoMapScene_SearchItems(drawList, "MON", SearchItemName, NULL);
  CHECK(mon.size() == 1 && mon[0].kind == AutoMapHitMonster);
  CHECK(AutoMapScene_SearchItems(drawList, "MO", SearchItemName, NULL).empty());
}

// Under the fog the search finds only what the map shows: nothing on cells never explored
static void SearchKeepsTheFog(void)
{
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList fogged = AutoMapScene_BuildDrawList(CorridorWithTrolins(), FogOver(explored, visibleNow));
  CHECK(AutoMapScene_SearchItems(fogged, "GEN", SearchItemName, NULL).empty());
  CHECK(AutoMapScene_SearchItems(fogged, "trolin", SearchItemName, NULL).empty());
}

static AutoMapSearchHit HitOn(int level, int x, int y)
{
  AutoMapSearchHit hit;
  hit.level = level;
  hit.x = x;
  hit.y = y;
  return hit;
}

// The results group by level, the shown level first, then the others in order; within a level in
// reading order; a folded level keeps only its heading
static void SearchRowsGroupByLevel(void)
{
  std::vector<AutoMapSearchHit> hits;
  hits.push_back(HitOn(3, 5, 9));
  hits.push_back(HitOn(1, 2, 2));
  hits.push_back(HitOn(3, 7, 4));
  std::set<int> folded;
  std::vector<AutoMapSearchRow> rows = AutoMapScene_SearchRows(hits, 3, folded);
  CHECK(rows.size() == 5);
  CHECK(rows.size() == 5 && rows[0].heading && rows[0].level == 3 && rows[0].hits == 2);
  CHECK(rows.size() == 5 && !rows[1].heading && hits[(size_t)rows[1].hit].y == 4);
  CHECK(rows.size() == 5 && !rows[2].heading && hits[(size_t)rows[2].hit].y == 9);
  CHECK(rows.size() == 5 && rows[3].heading && rows[3].level == 1 && rows[3].hits == 1);
  folded.insert(1);
  rows = AutoMapScene_SearchRows(hits, 3, folded);
  CHECK(rows.size() == 4 && rows[3].heading && rows[3].level == 1);
}

// After a new query the selection starts on the first hit the panel lists, not on the best-ranked
// hit of the lowest level
static void SearchSelectionStartsOnTheFirstListedHit(void)
{
  std::vector<AutoMapSearchHit> hits;
  hits.push_back(HitOn(1, 2, 2));
  hits.push_back(HitOn(5, 7, 4));
  hits.push_back(HitOn(5, 3, 1));
  std::set<int> folded;
  std::vector<AutoMapSearchRow> rows = AutoMapScene_SearchRows(hits, 5, folded);
  CHECK(AutoMapScene_FirstListedHit(rows) == 2);
  folded.insert(5);
  folded.insert(1);
  CHECK(AutoMapScene_FirstListedHit(AutoMapScene_SearchRows(hits, 5, folded)) == -1);
}

// A monster hit carries its group, so a selected group can be found again after it moved
static void MonsterHitsKnowTheirGroup(void)
{
  AutoMapLevelSnapshot corridor = CorridorWithTrolins();
  corridor.monsters[0].record = 0x40011u;
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(drawList, "MON", SearchItemName, NULL);
  CHECK(hits.size() == 1 && hits[0].group == 0x40011u);
}

static void AGroupKeepsWhereItsMembersStandAndItsSize(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  AutoMapMonsterRecord group = MummyAt(2, 1);
  group.memberCount = 2;
  group.kind.size = 1;
  group.positions = 1 | (3 << 2);
  corridor.monsters.push_back(group);
  corridor.monsters.push_back(MummyAt(1, 1));
  AutoMapExplored explored = NothingExplored();
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, NoFog(explored, visibleNow));
  CHECK(drawList.monsters.size() == 2);
  const AutoMapShownMonster &pair = drawList.monsters[0].x == 2 ? drawList.monsters[0] : drawList.monsters[1];
  const AutoMapShownMonster &lone = drawList.monsters[0].x == 2 ? drawList.monsters[1] : drawList.monsters[0];
  CHECK(pair.size == 1);
  CHECK(pair.positions == (1 | (3 << 2)));
  CHECK(lone.size == 0);
  CHECK(lone.positions == AUTOMAP_CENTRE_POSITIONS);
}

static void TipGlyphsFindEveryInlineIcon(void)
{
  std::string text = std::string("TRIGGER: \x01\x01 PARTY \x02\x02 MONSTER ") + AUTOMAP_GLYPH_ITEM + '1'
                     + " APPLE " + AUTOMAP_GLYPH_FACING + '1';
  std::vector<AutoMapTipGlyph> glyphs = AutoMapScene_TipGlyphs(text);
  CHECK(glyphs.size() == 4);
  CHECK(glyphs[0].column == 9 && glyphs[0].kind == 1);
  CHECK(glyphs[1].kind == 2);
  CHECK(glyphs[2].kind == 4 && glyphs[2].argument == 1);
  CHECK(glyphs[3].kind == 5 && glyphs[3].argument == 1);
  CHECK(AutoMapScene_TipGlyphs("PLAIN TEXT").empty());
}

static void TipTextWithoutGlyphsBlanksTheArgumentCharacters(void)
{
  std::string text = std::string("A ") + AUTOMAP_GLYPH_ITEM + '1' + " B " + AUTOMAP_GLYPH_FACING + '2';
  CHECK(AutoMapScene_TipTextWithoutGlyphs(text) == "A    B   ");
  CHECK(AutoMapScene_TipTextWithoutGlyphs("PLAIN 12") == "PLAIN 12");
}

void TestAutoMapScene(void)
{
  TipTextWithoutGlyphsBlanksTheArgumentCharacters();
  TipGlyphsFindEveryInlineIcon();
  AGroupKeepsWhereItsMembersStandAndItsSize();
  SearchSelectionStartsOnTheFirstListedHit();
  MonsterHitsKnowTheirGroup();
  SearchFindsMonstersAndGenerators();
  SearchKeepsTheFog();
  SearchRowsGroupByLevel();
  FontHasTheBarsMarks();
  StairsLeadToTheMapAtTheNextDepth();
  StackNameLine();
  TeleportsIntoACell();
  SwitchedFalseWallSaysSo();
  MonsterNameLineNamesTheGroup();
  AsManyMonstersAsFitShareACell();
  AQuarterCellMonsterCanTakeAnyQuadrantOrTheCentre();
  AHalfCellMonsterFillsTheHalfItIsSpawnedIn();
  AWholeCellMonsterStandsInTheCentre();
  CellLooksChangedOnDoorPitWallPicture();
  LookOfReadsTheLookFields();
  ApplyLookOverwritesOnlyThose();
  CellLooksChangedSeesConcealment();
  VisibleLookKeepsAnUnknownConcealedPitsState();
  SeenToChangeIgnoresAnUnknownConcealedPit();
  RecordIdsDefaultToNone();
  BoundaryMarginKeepsFogAndGlobalsAligned();
  MonsterFacingSouthShowsItsFront();
  MonsterFacingNorthShowsItsBack();
  MonsterFacingEastShowsItsSide();
  MonsterFacingWestShowsItsSideMirrored();
  BackPictureFollowsTheFrontWhenThereIsNoSidePicture();
  MonsterWithoutSideAndBackPicturesAlwaysShowsItsFront();
  MonsterWithSideButNoBackPictureShowsItsFrontFromBehind();
  WithoutFogTheWholeLevelIsShown();
  UnderFogOnlyExploredCellsAreShown();
  UnderFogWithNothingExploredOnlyThePartyIsShown();
  UnderFogAMonsterIsShownOnlyOnACellThePartySeesNow();
  EveryMemberOfAGroupGetsThePictureOfItsOwnFacing();
  UnderFogAWallSideIsShownOnlyWhenTheFloorInFrontOfItIsExplored();
  WithoutFogAnExploredFloorCellIsHighlighted();
  WithoutFogAnUnexploredFloorCellIsNotHighlighted();
  AnExploredWallIsNotHighlighted();
  NothingIsHighlightedUnlessAskedFor();
  UnderFogNothingIsHighlighted();
  UpToThreeItemsAreShownWithoutAMark();
  AFourthItemTurnsIntoTheMoreItemsMark();
  TooltipKeepsTheFullPileWhenTheCellIsTruncated();
  TooltipGroupsIdenticalPicturesInFirstSeenOrder();
  TooltipKeepsApartItemsSharingTheirWorldPicture();
  TooltipSlotsKeepIconAndName();
  TooltipOnUnexploredCellIsEmpty();
  TooltipOnFoggedVisitedCellShowsItsItems();
  TooltipKeepsEachChestInItsOwnSlot();
  TooltipShowsChestPastTheDisplayTruncation();
  TooltipOnUnexploredChestCellIsEmpty();
  TooltipShowsMonsterPossessionsNested();
  PossessionsHideWithTheirMonster();
  TooltipShowsMonsterDropsByTier();
  DropsHideWithTheirMonster();
  TooltipShowsMonsterHealthPerMember();
  HealthHidesWithItsMonster();
  WholeLevelIsFittedAndCentred();
  WindowWithoutRoomShowsNothing();
  ZoomStepsStayWithinTheirRange();
  ZoomReachesAThirdOfTheView();
  ZoomStepZeroIsTheWholeLevel();
  ZoomedInTheViewIsCentredOnTheParty();
  ZoomedInTheViewStopsAtTheEdgesOfTheLevel();
  AFractionalCentreMovesTheZoomedView();
  FarCentresStopAtTheEdgesOfTheLevel();
  ZoomKeepsTheSameSpotCentred();
  ZoomKeepsThePointUnderThePointerStill();
  OnlyTheFittingZoomShowsTheWholeLevel();
  LabelStepKeepsNumbersApart();
  VisibleRangeCoversTheCellsTouchingTheMap();
  DigitsAreThreeByFive();
  FontMatchesTheDigits();
  RecruitedChampionStaysSeenAndViewed();
  MirrorOnlySeenIsNotViewed();
  PlainWallCountsNothing();
  PendingCentreWaitsForItsLevel();
  NewerRequestReplacesThePendingCentre();
  GlobalOffsetsReachTheDrawList();
  GlobalPositionAddsTheLevelOffset();
  WithoutFogMissilesAndCloudsAreShown();
  UnderFogMissilesAndCloudsShowOnlyWhereThePartySees();
  SpellFireShrinksWithItsRange();
  AFalseWallIsNotHighlighted();
  HiddenCellKindsSurviveTheDrawList();
  HiddenCellsFollowTheFog();
  WithoutFogActuatorsAndLinksAreShown();
  UnderFogActuatorsShowOnlyOnExploredCells();
  LinksSurviveWithoutFog();
  LinkHiddenWhenTargetUnexplored();
  LinkHiddenWhenSourceUnexplored();
  ReverseLookupFindsAllSources();
  WallSideSurvivesTheDrawList();
  PadOperatorsSurviveTheDrawList();
  PadOperatorsFollowTheFog();
  LinkSignalsSurviveTheDrawList();
  DoorPreviewFollowsTheSignal();
  TeleporterTargetSurvivesWithoutFog();
  TeleporterTargetHiddenWhenDestinationUnexplored();
  CrossLevelTargetFollowsTheFog();
  DormantTeleporterShowsNoTarget();
  DormantSpinnerSurvivesTheDrawList();
  TeleporterScopeSurvivesTheDrawList();
  SpinnerRotationSurvivesTheDrawList();
  GeneratedMonsterSurvivesTheDrawList();
  DoorBreakabilitySurvivesTheDrawList();
  WallTierFlagsSurviveTheDrawList();
  UnderFogATierFlagGoesWithItsHiddenSide();
  TooltipShowsAlcoveContentsWithoutFog();
  TooltipShowsWallLockWithoutFog();
  UnderFogAlcoveAndLockHideWithTheirSide();
  AlcoveAndLockStaySeparateFromFloorItems();
  RequiredNeverMergesWithPresent();
  SwapLockNeedsItsKeyAlongsideStockedAlcove();
  SameKeyOnDifferentSidesStaysSeparate();
  StackedActuatorsBehindOneKeyholeShowOneNeed();
  PadCarryNeedShowsAsRequired();
  PadCarryNeedShowsAlongsideIdenticalFloorItem();
  PadObjectNeedShowsWhenMissing();
  PadObjectNeedSilentWhenSatisfied();
  PadNeedsFoldDuplicatesButKeepCarryApartFromObject();
  PadNeedWithoutPictureStaysOut();
  PadNeedHidesWithUnexploredActuator();
  PlainHolderShowsItemWhenEmpty();
  PlainHolderSilentWhenStocked();
  DoorLockShowsInTooltip();
  DoorLockFollowsSideVisibility();
  ExcerptCollapsesWhitespaceAndCapsLength();
  ScrollTextsPersistLikeItemsUnderFog();
  WallTextsHideWithTheirSide();
  AlcoveAndChestScrollTextsShowWithoutFog();
  AlcoveScrollTextHidesWithItsSide();
  MonsterCarriedScrollTextFollowsItsMonster();
  TriggerLinesNameEveryOperator();
  TriggerLinesNameRequiredObjects();
  TriggerLinesFoldDuplicatesAndNameContraptions();
  TriggerLinesStaySilentWithoutTheCell();
  EachPadOfACellIsItsOwnPress();
  SameTriggerPadsAreOnePress();
  PadsReachingOneDoorGetOneBadge();
  GeneratorHealthRangeFollowsEngineFormula();
  GeneratorHealthLinesNameEachValueOnItsOwnLine();
  TriggerLinesShowGeneratorHealth();
  CounterCogCountsOnceAtAll();
  RelayLinesDescribeWallCounterChain();
  RelayHoverLinesNameEachCounterSourceCount();
  PressLinesNameBothTriggersOnOneTarget();
  RelayLinesAtAllKeepOnlyCellFacts();
  RelayLinesBelowAllUnchanged();
  PressLinesDescribeGateBitsAndSwitches();
  RelayHoverLinesKeepIdenticalGates();
  RelayHoverLinesNameLocalGatesAsMessageDriven();
  RelayLinesDescribeLaunchers();
  RelayLinesStaySilentWithoutTheCell();
  PressEffectsReachEveryCounterInTheCell();
  PressEffectsFollowACounterThatReachesZero();
  PressEffectsWalkLaterFirings();
  PressEffectsGateHonoursEdgeAndNormallyClosed();
  PressEffectsCounterRules();
  PressEffectsPressureSourceSendsSet();
  PressEffectsNormallyClosedPressureSourceSendsClear();
  PressLinesNameClosedSwitchablePits();
  PressEffectsMarkReceiversAfterADSA();
  PressEffectsStopAtLoops();
  PressEffectsReachARelayTwiceWithoutALoop();
  PressEffectsStaySilentUnderFogAndBelowAll();
  PressEffectsGroupBySource();
  PressEffectsLocalAction();
  PressEffectsFindTheGoalBehindTheRelays();
  PressLinesNameBareTargets();
  PressEffectsDeliverToTheWholeCellFirst();
  PressEffectsSpendOnceOnlyRelays();
  PressEffectsStopAtTheBudget();
  PressEffectsLeaveARelayCellUnpressed();
  PressEffectsFireEveryActuatorOfTheFace();
  GoalDistanceCountsDownACounter();
  GoalDistanceThroughAGate();
  GoalDistanceInvertedGateNeedsOthers();
  GoalDistanceEdgeModeGate();
  GoalDistanceCounterBehindGate();
  GoalDistanceCounterBehindCounter();
  GoalDistanceBlocked();
  GoalDistanceOtherStatuses();
  PressChainLinksCoverTheTree();
  GoalBadgeWords();
  PressLinesCompact();
  PressLinesTree();
  WhenNoteWords();
  RouteCounterThenGoal();
  RouteDeadCounterIsBlocked();
  HeldSourceAgreesInRouteResultAndBadge();
  RouteAfterADSANeverSaysThisPress();
  RouteHeldStepReachedLaterDoesNotSayEveryPress();
  RouteHeldCounterSaysWhatThisCountSends();
  RouteStopsAtADSA();
  RouteMarksStepsAfterADSA();
  RouteNamesTheLoop();
  RouteNamesBareTargetsAndLocalActions();
  ResultNamesItsDelay();
  ShiftHintOnlyWithStepsBetween();
  ShiftOnACappedDirectListShowsEveryResult();
  PressLinesWordTheGoalDistance();
  RouteStepMarks();
  PressLinesFoldIdenticalReceiversAndCap();
  PressLinesGroupSources();
  RelayHoverLinesLetterRelays();
  RelayHoverLinesStaySilent();
  PressLinesCutShortSurvivesTheCap();
  RelayHoverLinesCapTheTargets();
  PressLinesFullChainIsUncapped();
  RelayHoverLinesFullIsUncapped();
  GlyphGreaterThanDraws();
  SealedDoorsNeedNoSwitchLinkOrBash();
  RelayLinesNameSealedDoors();
  RelayLinesNameTheForceThatBreaksADoor();
  TeleporterLinesDescribeScopeDestinationAndTurn();
  TeleporterLinesDescribeAbsoluteFacing();
  DestinationPrintsGlobalCoordinates();
  SpinnerLinesDescribeAbsoluteAndRelativeSpin();
  DormantSpinnerLinesNameHypotheticalSpin();
  SpinnerGeometrySeparatesAbsoluteFromRelative();
  DormantTeleporterLineNamesNoEffect();
  PitLinesNameOpenAndHiddenState();
  DormantTeleporterNamesWhatItWouldDo();
  DormantDestinationHidesWithItsCell();
  LiveDestinationHidesWithItsCell();
  MachineryGroupsTeleporterAndPad();
  SameTriggerPadsShareOnePadGroup();
  DormantTeleporterKeepsItsWouldLines();
  TriggerIconsForEveryPadKind();
  BelowAllTheMachineryShowsNoRouteOrRelays();
  MachineryHeadsPitsDoorsAndGenerators();
  SearchFindsFloorItemsBySubstring();
  SearchRanksExactBeforeSubstring();
  SearchRespectsFog();
  SearchSkipsUnknownNames();
  SearchHitsCarryTheirLevel();
  SearchFindsMonsterPossessions();
  SearchPossessionsHideWithTheirMonster();
  SearchRankIsShared();
  FoodFindsEveryFoodItem();
  FoodWordMatchesByPrefix();
  AFoodItemIsListedOnceWhenItsNameMatchesToo();
  FoodNamesStillMatch();
  ANullFoodTestFindsNoFoodByTheWord();
  HitsOnOneCellKeepOneOrder();
  SortingHitsIgnoresTheirOrderBefore();
  FountainFindsTheFountainFace();
  StairsFindsBothWays();
  WaterFindsFlasksAndFountains();
  AltarAndViFindTheAltar();
  APlainFaceIsNoFeature();
  PlacementTakesAnEmptyFloor();
  PlacementRefusesWallsAndTheEdgeOfTheLevel();
  PlacementRefusesAClosedFalseWallButTakesAnOpenOne();
  PlacementTakesAFalseWallThePartyWalksThrough();
  PlacementTakesADoorOnlyWhenItLetsThePartyThrough();
  PlacementRefusesACellWithAMonster();
  PlacementRefusesThePartyCellOnlyOnThePartyLevel();
  PlacementNamesWhyACellIsRefused();
  SkillLevelDoublesItsExperienceForEveryLevel();
  ExperienceToNextLevelReachesExactlyTheNextLevel();
  ExperienceToNextLevelIsZeroWhenTheCapForbidsIt();
  NearestSidePicksTheEdge();
  ItemKindFollowsTheObjectTypeRanges();
  ItemsOfKindsKeepsThePileOrder();
  OnlyFlavourFloorPicturesCountAsDecoration();
  FoodAndKeysAreKindsOfTheirOwn();
}
