#include "TestSupport.h"
#include "../AutoMapScene.h"
#include "../AutoMapCellMemory.h"

#include <cstddef>

// Explored, live: the fog on with isLive. Explored cells are drawn as they are now, not as the
// party last saw them, and what moves on them shows on every explored cell.

static const int LEVEL = 0;
static const int LEVER_PICTURE = 148;
static const int CLUB_PICTURE = 11;

//         x=0 x=1 x=2 x=3
//   y=0   #   L   #   #      L: lever on the south side
//   y=1   #   .   .   .
//   y=2   #   #   #   #
static AutoMapLevelSnapshot Corridor(void)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 4;
  snapshot.height = 3;
  snapshot.cells.assign(12, AutoMapCellRecord());
  snapshot.cells[1 * 4 + 1].kind = AutoMapCellFloor;
  snapshot.cells[1 * 4 + 2].kind = AutoMapCellFloor;
  snapshot.cells[1 * 4 + 3].kind = AutoMapCellFloor;
  snapshot.cells[0 * 4 + 1].wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  snapshot.party.x = 1;
  snapshot.party.y = 1;
  snapshot.party.facing = AutoMapEast;
  return snapshot;
}

static AutoMapCellRecord &At(AutoMapLevelSnapshot *snapshot, int x, int y)
{
  return snapshot->cells[(size_t)y * snapshot->width + x];
}

// A live club (record 0x10, description 27) on (2,1)
static AutoMapLevelSnapshot WithLiveClub(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapItem club;
  club.worldPicture = CLUB_PICTURE;
  At(&snapshot, 2, 1).itemPictures.push_back(club);
  AutoMapFloorItemTag tag = {0x10u, 27};
  At(&snapshot, 2, 1).itemTags.push_back(tag);
  snapshot.itemOfDescIndex[27].worldPicture = CLUB_PICTURE;
  return snapshot;
}

// (1,1), the wall north of it and (2,1) explored; (3,1) not
static AutoMapExplored Explored(void)
{
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(1, AutoMapLevelSize{4, 3}));
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  return explored;
}

static AutoMapFog Fog(bool isLive, AutoMapSpoilerLevel spoilers, const AutoMapExplored &explored,
                      const AutoMapVisibleCells &visibleNow, const AutoMapKnowledge *knowledge)
{
  AutoMapFog fog = {true, &explored, &visibleNow, false, spoilers, knowledge};
  fog.isLive = isLive;
  return fog;
}

static const AutoMapShownCell *FindCell(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.cells.size(); i++)
  {
    if (drawList.cells[i].x == x && drawList.cells[i].y == y) return &drawList.cells[i];
  }
  return NULL;
}

static bool HasGlimpseAt(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.glimpsedItems.size(); i++)
  {
    if (drawList.glimpsedItems[i].x == x && drawList.glimpsedItems[i].y == y) return true;
  }
  return false;
}

static bool HasGlimpsedFace(const AutoMapDrawList &drawList, int x, int y, int side)
{
  for (size_t i = 0; i < drawList.glimpsedFaces.size(); i++)
  {
    const AutoMapGlimpsedFace &face = drawList.glimpsedFaces[i];
    if (face.x == x && face.y == y && face.side == side) return true;
  }
  return false;
}

// The door on (2,1) was seen shut; it opened out of sight
static void ExploredLiveShowsTheLiveLookNotTheRemembered(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &door = At(&snapshot, 2, 1);
  door.kind = AutoMapCellDoor;
  door.doorState = AutoMapDoorOpen;
  AutoMapCellLook shut = AutoMapScene_LookOf(door);
  shut.doorState = AutoMapDoorClosed;
  AutoMapCellMemory cells;
  cells.See(AutoMapCell{LEVEL, 2, 1}, shut);
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog live = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  live.cells = &cells;
  AutoMapFog fogOfWar = Fog(false, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fogOfWar.cells = &cells;
  CHECK(FindCell(AutoMapScene_BuildDrawList(snapshot, live), 2, 1)->cell.doorState == AutoMapDoorOpen);
  CHECK(FindCell(AutoMapScene_BuildDrawList(snapshot, fogOfWar), 2, 1)->cell.doorState ==
        AutoMapDoorClosed);
}

// The club was never seen, so the item memory is empty
// The party saw the club on (1,1); out of sight it moved to (2,1)
static void ExploredLiveShowsASeenItemWhereItIsNow(void)
{
  AutoMapItemMemory items;
  AutoMapRememberedItem seen = {0x10u, 27, {LEVEL, 1, 1}};
  items.See(seen);
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog live = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  live.items = &items;
  AutoMapFog fogOfWar = Fog(false, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fogOfWar.items = &items;
  AutoMapDrawList liveList = AutoMapScene_BuildDrawList(WithLiveClub(), live);
  const std::vector<AutoMapItem> &pile = FindCell(liveList, 2, 1)->cell.itemPictures;
  CHECK(pile.size() == 1 && pile[0].worldPicture == CLUB_PICTURE);
  CHECK(FindCell(liveList, 1, 1)->cell.itemPictures.empty());
  AutoMapDrawList fogList = AutoMapScene_BuildDrawList(WithLiveClub(), fogOfWar);
  CHECK(FindCell(fogList, 2, 1)->cell.itemPictures.empty());
  CHECK(FindCell(fogList, 1, 1)->cell.itemPictures.size() == 1);
}

// The key under the ashes: on an explored cell, but the 3D view never showed it
static void ExploredLiveHidesAnItemNeverSeenBelowAll(void)
{
  AutoMapItemMemory items;
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog learned = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  learned.items = &items;
  CHECK(FindCell(AutoMapScene_BuildDrawList(WithLiveClub(), learned), 2, 1)->cell.itemPictures.empty());
  AutoMapFog all = Fog(true, AutoMapSpoilersAll, explored, visibleNow, &knowledge);
  all.items = &items;
  CHECK(FindCell(AutoMapScene_BuildDrawList(WithLiveClub(), all), 2, 1)->cell.itemPictures.size() == 1);
}

static void ExploredLiveDrawsNoUnexploredCell(void)
{
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapDrawList live = AutoMapScene_BuildDrawList(
      Corridor(), Fog(true, AutoMapSpoilersAll, explored, visibleNow, &knowledge));
  CHECK(FindCell(live, 3, 1) == NULL);
  CHECK(FindCell(live, 0, 1) == NULL);
  CHECK(FindCell(live, 2, 1) != NULL);
}

// A glimpsed item was never seen, so the live pile does not draw it: the "?" stays, explored
// cell or not, as under the fog of war
static void ExploredLiveKeepsTheGlimpsesOfTheFogOfWar(void)
{
  AutoMapItemMemory items;
  AutoMapRememberedItem onExplored = {0x10u, 27, {LEVEL, 2, 1}};
  onExplored.isGlimpsed = true;
  items.See(onExplored);
  AutoMapRememberedItem onUnexplored = {0x11u, 27, {LEVEL, 3, 1}};
  onUnexplored.isGlimpsed = true;
  items.See(onUnexplored);
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog live = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  live.items = &items;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(WithLiveClub(), live);
  CHECK(HasGlimpseAt(drawList, 2, 1));
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.empty());
  CHECK(HasGlimpseAt(drawList, 3, 1));
}

static void ExploredLiveKeepsAGlimpsedFace(void)
{
  AutoMapExplored explored = Explored();
  explored.MarkGlimpsedFace(AutoMapCell{LEVEL, 3, 0}, AutoMapSouth);
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapDrawList live = AutoMapScene_BuildDrawList(
      Corridor(), Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge));
  CHECK(HasGlimpsedFace(live, 3, 0, AutoMapSouth));
}

// Live vision tells what is there now, not its secrets: the lever's face was never shown by
// the 3D view, and the chest on (2,1) was never opened
static void ExploredLiveStillHidesSecretsBelowAll(void)
{
  AutoMapLevelSnapshot snapshot = WithLiveClub();
  AutoMapChestContents contents;
  contents.item = 0;
  contents.chest = 0x10u;
  contents.pictures.push_back(AutoMapItem());
  contents.pictureTexts.push_back("");
  contents.pictureScrolls.push_back(AUTOMAP_NO_RECORD);
  At(&snapshot, 2, 1).chestContents.push_back(contents);
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapItemMemory items;
  AutoMapRememberedItem chest = {0x10u, 27, {LEVEL, 2, 1}};
  items.See(chest);
  AutoMapFog live = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  live.items = &items;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, live);
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].picture == AUTOMAP_NO_PICTURE);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.size() == 1);
  CHECK(FindCell(drawList, 2, 1)->cell.chestContents.empty());
}

static AutoMapMonsterRecord GroupAt(int x, int y)
{
  AutoMapMonsterRecord monster;
  monster.x = x;
  monster.y = y;
  monster.kind.frontPicture = 300;
  monster.kind.hasSidePicture = false;
  monster.kind.hasBackPicture = false;
  monster.kind.transparentColour = 4;
  monster.memberCount = 1;
  for (int member = 0; member < AUTOMAP_LARGEST_GROUP; member++) monster.memberFacings[member] = AutoMapWest;
  return monster;
}

// Nothing is in sight and the last frame measured no group: under the fog of war none of these
// would show
static AutoMapDrawList BuildLive(const AutoMapLevelSnapshot &snapshot, AutoMapSpoilerLevel spoilers,
                                 const AutoMapExplored &explored)
{
  static const std::vector<AutoMapRecordId> noneSeen;
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog live = Fog(true, spoilers, explored, visibleNow, &knowledge);
  live.monstersSeenNow = &noneSeen;
  return AutoMapScene_BuildDrawList(snapshot, live);
}

static void ExploredLiveShowsAMonsterOutOfSightAtEverySpoilerLevel(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.monsters.push_back(GroupAt(2, 1));
  snapshot.monsters.push_back(GroupAt(3, 1));
  const AutoMapSpoilerLevel levels[3] = {AutoMapSpoilersAll, AutoMapSpoilersLearned, AutoMapSpoilersSeen};
  for (int i = 0; i < 3; i++)
  {
    AutoMapDrawList live = BuildLive(snapshot, levels[i], Explored());
    CHECK(live.monsters.size() == 1 && live.monsters[0].x == 2);
  }
}

static void ExploredLiveShowsMissilesAndCloudsOnExploredCellsOnly(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  for (int x = 2; x <= 3; x++)
  {
    AutoMapMissileRecord missile = {x, 1, AutoMapItem(), 5, false};
    snapshot.missiles.push_back(missile);
    AutoMapCloudRecord cloud = {x, 1, 600};
    snapshot.clouds.push_back(cloud);
  }
  AutoMapDrawList live = BuildLive(snapshot, AutoMapSpoilersLearned, Explored());
  CHECK(live.missiles.size() == 1 && live.missiles[0].x == 2);
  CHECK(live.clouds.size() == 1 && live.clouds[0].x == 2);
}

static void ExploredLiveShowsFloorActuatorsOnExploredCellsAtAll(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(AutoMapFloorActuator(2, 1));
  snapshot.actuators.push_back(AutoMapFloorActuator(3, 1));
  AutoMapDrawList live = BuildLive(snapshot, AutoMapSpoilersAll, Explored());
  CHECK(live.actuators.size() == 1 && live.actuators[0].x == 2);
}

// Another level: the party sees nothing there and no frame measured its groups
static void ExploredLiveShowsMonstersOnAnotherLevel(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.level = LEVEL + 1;
  snapshot.party.x = 0;
  snapshot.partyIsOnShownLevel = false;
  snapshot.monsters.push_back(GroupAt(2, 1));
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(2, AutoMapLevelSize{4, 3}));
  explored.Mark(AutoMapCell{LEVEL + 1, 2, 1});
  AutoMapDrawList live = BuildLive(snapshot, AutoMapSpoilersLearned, explored);
  CHECK(live.monsters.size() == 1);
}

// A snapshot grown by a margin of 1: snapshot (2,2) is level cell (1,1), explored; snapshot
// (1,1) is level cell (0,0), not explored
static void ExploredLiveTestsTheCellUnderTheMonsterAcrossTheMargin(void)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 5;
  snapshot.height = 4;
  snapshot.boundaryMarginX = 1;
  snapshot.boundaryMarginY = 1;
  snapshot.cells.assign(20, AutoMapCellRecord());
  At(&snapshot, 2, 2).kind = AutoMapCellFloor;
  snapshot.party.x = 2;
  snapshot.party.y = 2;
  snapshot.monsters.push_back(GroupAt(2, 2));
  snapshot.monsters.push_back(GroupAt(1, 1));
  AutoMapDrawList live = BuildLive(snapshot, AutoMapSpoilersAll, Explored());
  CHECK(live.monsters.size() == 1 && live.monsters[0].x == 2);
}

// A group may stand in a closed false wall. Below All the map draws that wall as stone until the
// party walked through it, so a group drawn there would give the secret away; at All it shows.
static void ExploredLiveHidesWhatMovesInsideAnUnknownFalseWall(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  At(&snapshot, 2, 0).kind = AutoMapCellFalseWall;
  snapshot.monsters.push_back(GroupAt(2, 0));
  AutoMapMissileRecord missile = {2, 0, AutoMapItem(), 5, false};
  snapshot.missiles.push_back(missile);
  AutoMapCloudRecord cloud = {2, 0, 600};
  snapshot.clouds.push_back(cloud);
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 2, 0});
  AutoMapDrawList learned = BuildLive(snapshot, AutoMapSpoilersLearned, explored);
  CHECK(learned.monsters.empty());
  CHECK(learned.missiles.empty());
  CHECK(learned.clouds.empty());
  AutoMapDrawList all = BuildLive(snapshot, AutoMapSpoilersAll, explored);
  CHECK(all.monsters.size() == 1);
  CHECK(all.missiles.size() == 1);
  CHECK(all.clouds.size() == 1);
}

void TestAutoMapVision(void)
{
  ExploredLiveShowsTheLiveLookNotTheRemembered();
  ExploredLiveShowsASeenItemWhereItIsNow();
  ExploredLiveHidesAnItemNeverSeenBelowAll();
  ExploredLiveDrawsNoUnexploredCell();
  ExploredLiveKeepsTheGlimpsesOfTheFogOfWar();
  ExploredLiveKeepsAGlimpsedFace();
  ExploredLiveStillHidesSecretsBelowAll();
  ExploredLiveShowsAMonsterOutOfSightAtEverySpoilerLevel();
  ExploredLiveShowsMissilesAndCloudsOnExploredCellsOnly();
  ExploredLiveShowsFloorActuatorsOnExploredCellsAtAll();
  ExploredLiveShowsMonstersOnAnotherLevel();
  ExploredLiveTestsTheCellUnderTheMonsterAcrossTheMargin();
  ExploredLiveHidesWhatMovesInsideAnUnknownFalseWall();
}
