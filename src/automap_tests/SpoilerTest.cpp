#include "TestSupport.h"
#include "../AutoMapScene.h"
#include "../AutoMapCellMemory.h"
#include "../AutoMapMonsterDamage.h"
#include "../AutoMapPlayerNotes.h"

#include <cstddef>
#include <string>

static const int LEVEL = 0;
static const int LEVER_PICTURE = 148;
static const int PIT_PICTURE = 77;
static const AutoMapRecordId SWITCH_RECORD = 7;
static const AutoMapRecordId LOCK_RECORD = 5;

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

// A corridor of two floor cells inside walls, as in SceneTest.cpp:
//
//        x=0 x=1 x=2 x=3
//   y=0   #   L   #   #      L: lever on the south side
//   y=1   #   .   .   #
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

// The lever cell (1,0): a lever side whose switch is SWITCH_RECORD
static void MakeLeverFunctional(AutoMapLevelSnapshot *snapshot)
{
  AutoMapWallSide &side = At(snapshot, 1, 0).wallSides[AutoMapSouth];
  side.functional = true;
  side.actuators.push_back(SWITCH_RECORD);
}

static AutoMapExplored Explored(void)
{
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(1, AutoMapLevelSize{4, 3}));
  return explored;
}

static AutoMapFog Fog(bool isOn, AutoMapSpoilerLevel spoilers, const AutoMapExplored &explored,
                      const AutoMapVisibleCells &visibleNow, const AutoMapKnowledge *knowledge)
{
  AutoMapFog fog = {isOn, &explored, &visibleNow, false, spoilers, knowledge};
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

// Fog off, so every cell is shown; the spoiler level and the knowledge are the variables
static AutoMapDrawList Build(const AutoMapLevelSnapshot &snapshot, AutoMapSpoilerLevel spoilers,
                             const AutoMapKnowledge *knowledge)
{
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  return AutoMapScene_BuildDrawList(snapshot, Fog(false, spoilers, explored, visibleNow, knowledge));
}

static void Learn(AutoMapKnowledge *knowledge, AutoMapFactKind kind, int x, int y)
{
  AutoMapCell cell = {LEVEL, x, y};
  knowledge->Learn(AutoMapFact_OfCell(kind, cell));
}

static bool HasLineStartingWith(const std::vector<std::string> &lines, const char *start)
{
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (lines[i].compare(0, std::string(start).size(), start) == 0) return true;
  }
  return false;
}

static bool HasLine(const std::vector<std::string> &lines, const char *line)
{
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (lines[i] == line) return true;
  }
  return false;
}

// Floor cell (1,1) is a hidden pit, the wall at (3,1) a false wall, (2,1) a breakable door;
// the lever is functional and (0,0) holds a counter
static AutoMapLevelSnapshot AllTheSecrets(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  MakeLeverFunctional(&snapshot);
  At(&snapshot, 0, 0).wallRelays.push_back(CounterRelay(3));
  At(&snapshot, 3, 1).kind = AutoMapCellFalseWall;
  AutoMapCellRecord &pit = At(&snapshot, 1, 1);
  pit.kind = AutoMapCellPit;
  pit.pitIsHidden = true;
  pit.floorPicture = PIT_PICTURE;
  AutoMapCellRecord &door = At(&snapshot, 2, 1);
  door.kind = AutoMapCellDoor;
  door.doorBreakableWithAxe = true;
  door.doorBreakForce = 40;
  return snapshot;
}

static void CheckEverythingUnchanged(const AutoMapDrawList &drawList)
{
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].functional);
  CHECK(FindCell(drawList, 0, 0)->cell.wallRelays.size() == 1);
  CHECK(FindCell(drawList, 3, 1)->cell.kind == AutoMapCellFalseWall);
  CHECK(FindCell(drawList, 1, 1)->cell.pitIsHidden);
  CHECK(FindCell(drawList, 2, 1)->cell.doorBreakForce == 40);
}

static void AllKeepsTodaysMap(void)
{
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.Mark(AutoMapCell{LEVEL, 0, 0});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  explored.Mark(AutoMapCell{LEVEL, 3, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(
      AllTheSecrets(), Fog(true, AutoMapSpoilersAll, explored, visibleNow, NULL));
  CHECK(drawList.spoilers == AutoMapSpoilersAll);
  CheckEverythingUnchanged(drawList);
}

static void AllNeedsNoKnowledge(void)
{
  AutoMapDrawList drawList = Build(AllTheSecrets(), AutoMapSpoilersAll, NULL);
  CheckEverythingUnchanged(drawList);
}

static void UnseenFaceHidesItsDecorationUnderFog(void)
{
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  AutoMapDrawList before = AutoMapScene_BuildDrawList(Corridor(), fog);
  CHECK(FindCell(before, 1, 0)->cell.wallSides[AutoMapSouth].picture == AUTOMAP_NO_PICTURE);
  explored.MarkFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapDrawList after = AutoMapScene_BuildDrawList(Corridor(), fog);
  CHECK(FindCell(after, 1, 0)->cell.wallSides[AutoMapSouth].picture == LEVER_PICTURE);
}

static const int PUDDLE_PICTURE = 247 + 6 * 8 + 5;

// A puddle on (2,1): exploring the cell shows a strip of its floor, the puddle only once the 3D
// view showed enough of it. Pits keep their picture, the cell itself; At all shows every one.
static void AFloorPictureShowsOnceItWasNoticed(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  At(&corridor, 2, 1).floorPicture = PUDDLE_PICTURE;
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, fog), 2, 1)->cell.floorPicture == AUTOMAP_NO_PICTURE);
  AutoMapFog all = Fog(true, AutoMapSpoilersAll, explored, visibleNow, &knowledge);
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, all), 2, 1)->cell.floorPicture == PUDDLE_PICTURE);
  AutoMapFog whole = Fog(false, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, whole), 2, 1)->cell.floorPicture == PUDDLE_PICTURE);
  explored.MarkFloorSeen(AutoMapCell{LEVEL, 2, 1});
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, fog), 2, 1)->cell.floorPicture == PUDDLE_PICTURE);
  AutoMapLevelSnapshot pit = Corridor();
  At(&pit, 1, 1).kind = AutoMapCellPit;
  At(&pit, 1, 1).floorPicture = 54;
  CHECK(FindCell(AutoMapScene_BuildDrawList(pit, fog), 1, 1)->cell.floorPicture == 54);
}

// A champion's mirror on the lever wall's face: from afar the 3D view draws the frame only, the
// face when the party stands before it, which marks the face read like wall text
static void APortraitShowsItsFaceOnceSeenUpClose(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  At(&corridor, 1, 0).wallSides[AutoMapSouth].portrait = 5;
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.MarkFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, fog);
  const AutoMapWallSide &fromAfar = FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth];
  CHECK(fromAfar.portrait == AUTOMAP_NO_PICTURE);
  CHECK(fromAfar.picture == LEVER_PICTURE);
  AutoMapFog all = Fog(true, AutoMapSpoilersAll, explored, visibleNow, &knowledge);
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, all), 1, 0)->cell.wallSides[AutoMapSouth].portrait == 5);
  explored.MarkTextRead(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, fog), 1, 0)->cell.wallSides[AutoMapSouth].portrait == 5);
}

// Two items in the alcove of the lever wall's face: only the one that stood out in the 3D view
// shows. A face looked into as a whole (a portrait clicked, an older map) shows both.
static void AnAlcoveShowsOnlyTheItemsNoticedInIt(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  AutoMapCellRecord &wall = At(&corridor, 1, 0);
  wall.wallSides[AutoMapSouth].isAlcove = true;
  AutoMapAlcoveEntry cheese;
  cheese.item.worldPicture = 360;
  cheese.side = AutoMapSouth;
  cheese.record = 0x000a0001u;
  AutoMapAlcoveEntry bread = cheese;
  bread.item.worldPicture = 361;
  bread.record = 0x000a0002u;
  wall.alcoveContents.push_back(cheese);
  wall.alcoveContents.push_back(bread);
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.MarkFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapItemMemory items;
  // A description the level knows, so the cheese could be drawn even from memory alone
  corridor.itemOfDescIndex[160] = cheese.item;
  AutoMapRememberedItem seenCheese = {cheese.record, 160, AutoMapCell{LEVEL, 1, 0}, false};
  items.See(seenCheese);
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fog.items = &items;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(corridor, fog);
  const std::vector<AutoMapAlcoveEntry> &shown = FindCell(drawList, 1, 0)->cell.alcoveContents;
  CHECK(shown.size() == 1);
  CHECK(shown.size() == 1 && shown[0].record == cheese.record);
  // Remembered on the wall cell, the cheese is no floor item there
  CHECK(FindCell(drawList, 1, 0)->cell.itemPictures.empty());
  explored.MarkAlcoveLookedInto(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, fog), 1, 0)->cell.alcoveContents.size() == 2);
}

// Stairs on (2,1): a strip of the tile past a wall corner explores it, the stairs show once the
// 3D view showed enough of them; until then the tile reads as floor. At All they show.
static void StairsShowOnceTheyWereNoticed(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  At(&corridor, 2, 1).kind = AutoMapCellStairs;
  At(&corridor, 2, 1).stairsLeadUp = true;
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, fog), 2, 1)->cell.kind == AutoMapCellFloor);
  AutoMapFog all = Fog(true, AutoMapSpoilersAll, explored, visibleNow, &knowledge);
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, all), 2, 1)->cell.kind == AutoMapCellStairs);
  explored.MarkFloorSeen(AutoMapCell{LEVEL, 2, 1});
  CHECK(FindCell(AutoMapScene_BuildDrawList(corridor, fog), 2, 1)->cell.kind == AutoMapCellStairs);
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

// The lever on (1,0) glimpsed in the dark from (1,1): a "?" on that side, the wall still in the fog
static void AGlimpsedFaceShowsAQuestionMarkOnAnUnexploredWall(void)
{
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.MarkGlimpsedFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(Corridor(), fog);
  CHECK(FindCell(drawList, 1, 0) == NULL);
  CHECK(HasGlimpsedFace(drawList, 1, 0, AutoMapSouth));
  AutoMapFog all = Fog(true, AutoMapSpoilersAll, explored, visibleNow, &knowledge);
  CHECK(AutoMapScene_BuildDrawList(Corridor(), all).glimpsedFaces.empty());
}

// Writing on the lever wall's face, seen from afar: the map shows that it is there, the words
// only once the party stood in front of it
static void WallTextShowsOnlyOnceRead(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  At(&snapshot, 1, 0).wallSides[AutoMapSouth].hasWriting = true;
  At(&snapshot, 1, 0).wallSides[AutoMapSouth].text = "HALL OF CHAMPIONS";
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.MarkFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersSeen, explored, visibleNow, &knowledge);
  AutoMapDrawList unread = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(unread, 1, 0)->cell.wallSides[AutoMapSouth].hasWriting);
  CHECK(AutoMapScene_WallTexts(unread, 1, 0).empty());
  explored.MarkTextRead(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapDrawList read = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(AutoMapScene_WallTexts(read, 1, 0).size() == 1);
  // All shows the words of every face it shows, read or not
  AutoMapExplored unreadAll = Explored();
  unreadAll.Mark(AutoMapCell{LEVEL, 1, 1});
  unreadAll.Mark(AutoMapCell{LEVEL, 1, 0});
  AutoMapFog all = Fog(true, AutoMapSpoilersAll, unreadAll, visibleNow, &knowledge);
  CHECK(AutoMapScene_WallTexts(AutoMapScene_BuildDrawList(snapshot, all), 1, 0).size() == 1);
}

static void FogOffShowsEveryFace(void)
{
  AutoMapKnowledge knowledge;
  AutoMapDrawList drawList = Build(Corridor(), AutoMapSpoilersSeen, &knowledge);
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].picture == LEVER_PICTURE);
}

static void UnusedSwitchLooksLikeDressingAndAUsedOneWorks(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  MakeLeverFunctional(&snapshot);
  AutoMapKnowledge knowledge;
  AutoMapDrawList unused = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(!FindCell(unused, 1, 0)->cell.wallSides[AutoMapSouth].functional);
  CHECK(FindCell(unused, 1, 0)->cell.wallSides[AutoMapSouth].picture == LEVER_PICTURE);
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, SWITCH_RECORD));
  AutoMapDrawList used = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(used, 1, 0)->cell.wallSides[AutoMapSouth].functional);
}

static void SeenIgnoresKnowledge(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  MakeLeverFunctional(&snapshot);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, SWITCH_RECORD));
  AutoMapDrawList drawList = Build(snapshot, AutoMapSpoilersSeen, &knowledge);
  CHECK(!FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].functional);
  AutoMapDrawList withoutKnowledge = Build(snapshot, AutoMapSpoilersSeen, NULL);
  CHECK(!FindCell(withoutKnowledge, 1, 0)->cell.wallSides[AutoMapSouth].functional);
}

static void LockRequirementOnlyAfterAnItemOpenedIt(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapWallLock lock;
  lock.side = AutoMapSouth;
  lock.required = true;
  lock.actuator = LOCK_RECORD;
  At(&snapshot, 1, 0).wallLocks.push_back(lock);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, LOCK_RECORD));
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 1, 0)->cell.wallLocks.empty());
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactLockRequirement, LOCK_RECORD));
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(after, 1, 0)->cell.wallLocks.size() == 1);
}

// The 3D view records faces of stone only, so a door's lock counts as seen, as at All, once
// the floor cell in front of its side is explored
static void DoorLockShowsUnderFogOnceTheCellInFrontIsExplored(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &door = At(&snapshot, 2, 1);
  door.kind = AutoMapCellDoor;
  AutoMapWallLock west;
  west.side = AutoMapWest;
  west.required = true;
  west.actuator = LOCK_RECORD;
  AutoMapWallLock east = west;
  east.side = AutoMapEast;
  door.wallLocks.push_back(west);
  door.wallLocks.push_back(east);
  AutoMapAlcoveEntry coin;
  coin.item.worldPicture = 9;
  coin.side = AutoMapWest;
  door.alcoveContents.push_back(coin);
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactLockRequirement, LOCK_RECORD));
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  AutoMapDrawList before = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(before, 2, 1)->cell.wallLocks.empty());
  CHECK(FindCell(before, 2, 1)->cell.alcoveContents.empty());
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapDrawList after = AutoMapScene_BuildDrawList(snapshot, fog);
  const std::vector<AutoMapWallLock> &locks = FindCell(after, 2, 1)->cell.wallLocks;
  CHECK(locks.size() == 1 && locks[0].side == AutoMapWest);
  // Seen from the side, the 3D view draws no alcove contents
  CHECK(FindCell(after, 2, 1)->cell.alcoveContents.empty());
  explored.MarkAlcoveLookedInto(AutoMapCell{LEVEL, 2, 1}, AutoMapWest);
  AutoMapDrawList lookedInto = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(lookedInto, 2, 1)->cell.alcoveContents.size() == 1);
}

static void PortraitGoodsShowOnceThePortraitWasViewed(void)
{
  // A champion portrait in the Hall of Champions (DM L0) holds the champion's gear like a
  // niche, but the 3D view shows only the portrait: the gear is learned by clicking the
  // portrait, which marks its face looked into (AutoMap_OnPortraitViewed)
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &wall = At(&snapshot, 1, 0);
  wall.wallSides[AutoMapSouth].picture = AUTOMAP_NO_PICTURE;
  wall.wallSides[AutoMapSouth].portrait = 3;
  wall.wallSides[AutoMapSouth].functional = true;
  AutoMapAlcoveEntry sword;
  sword.item.worldPicture = 9;
  sword.side = AutoMapSouth;
  wall.alcoveContents.push_back(sword);
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.MarkFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  AutoMapDrawList before = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(before, 1, 0) != NULL && FindCell(before, 1, 0)->cell.alcoveContents.empty());
  explored.MarkAlcoveLookedInto(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapDrawList viewed = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(viewed, 1, 0) != NULL && FindCell(viewed, 1, 0)->cell.alcoveContents.size() == 1);
}

static void LauncherAndHiddenFlagShowOnlyOnceTheLauncherFired(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapLauncher launcher;
  launcher.side = AutoMapSouth;
  launcher.actuator = SWITCH_RECORD;
  At(&snapshot, 1, 0).wallLaunchers.push_back(launcher);
  At(&snapshot, 1, 0).wallSides[AutoMapSouth].hidden = true;
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 1, 0)->cell.wallLaunchers.empty());
  CHECK(!FindCell(before, 1, 0)->cell.wallSides[AutoMapSouth].hidden);
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, SWITCH_RECORD));
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(after, 1, 0)->cell.wallLaunchers.size() == 1);
  CHECK(FindCell(after, 1, 0)->cell.wallSides[AutoMapSouth].hidden);
}

static void RelayMachineryNeverShowsBelowAll(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &wall = At(&snapshot, 3, 0);
  wall.wallRelays.push_back(CounterRelay(2));
  AutoMapRelay dsa;
  dsa.kind = AutoMapRelayDSA;
  wall.wallRelays.push_back(dsa);
  wall.wallRelays.push_back(GateRelay(0x31, false));
  wall.wallRelays.push_back(GateRelay(0x31, true));
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, SWITCH_RECORD));
  AutoMapDrawList drawList = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(drawList, 3, 0)->cell.wallRelays.empty());
  std::vector<std::string> lines = AutoMapScene_RelayLines(drawList, 3, 0);
  CHECK(!HasLineStartingWith(lines, "COUNT"));
  CHECK(!HasLineStartingWith(lines, "GATE"));
  CHECK(!HasLineStartingWith(lines, "LOCAL GATE"));
  CHECK(!HasLineStartingWith(lines, "DSA"));
  // At All the same snapshot still reads its machinery
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(HasLineStartingWith(AutoMapScene_RelayHoverLines(all, 3, 0, false), "STONE:"));
}

static void BreakableDoorLooksLikeAPlainDoor(void)
{
  AutoMapLevelSnapshot snapshot = AllTheSecrets();
  // A closed door with no switch, no link and no way to break it is sealed at All only
  At(&snapshot, 2, 1).doorState = AutoMapDoorClosed;
  At(&snapshot, 2, 1).doorBreakableWithAxe = false;
  At(&snapshot, 2, 1).doorBreakForce = -1;
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(HasLine(AutoMapScene_RelayLines(all, 2, 1), "SEALED"));
  At(&snapshot, 2, 1).doorBreakableWithAxe = true;
  At(&snapshot, 2, 1).doorBreakForce = 40;
  AutoMapKnowledge knowledge;
  AutoMapDrawList drawList = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  const AutoMapCellRecord &door = FindCell(drawList, 2, 1)->cell;
  CHECK(door.doorBreakForce == -1);
  CHECK(!door.doorBreakableWithAxe);
  CHECK(!door.doorBreakableWithFireball);
  CHECK(!HasLine(AutoMapScene_RelayLines(drawList, 2, 1), "SEALED"));
  CHECK(!HasLineStartingWith(AutoMapScene_RelayLines(drawList, 2, 1), "BREAK FORCE"));
}

static void FalseWallIsAWallUntilKnown(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  At(&snapshot, 3, 1).kind = AutoMapCellFalseWall;
  At(&snapshot, 2, 2).kind = AutoMapCellOpenFalseWall;
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 3, 1)->cell.kind == AutoMapCellWall);
  CHECK(FindCell(before, 2, 2)->cell.kind == AutoMapCellFloor);
  Learn(&knowledge, AutoMapFactFalseWall, 3, 1);
  Learn(&knowledge, AutoMapFactFalseWall, 2, 2);
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(after, 3, 1)->cell.kind == AutoMapCellFalseWall);
  CHECK(FindCell(after, 2, 2)->cell.kind == AutoMapCellOpenFalseWall);
  // Seen never restores anything, whatever the knowledge holds
  AutoMapDrawList seen = Build(snapshot, AutoMapSpoilersSeen, &knowledge);
  CHECK(FindCell(seen, 3, 1)->cell.kind == AutoMapCellWall);
}

static void HiddenPitIsFloorUntilKnown(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &closed = At(&snapshot, 1, 1);
  closed.kind = AutoMapCellPit;
  closed.pitIsHidden = true;
  closed.floorPicture = PIT_PICTURE;
  AutoMapCellRecord &open = At(&snapshot, 2, 1);
  open.kind = AutoMapCellPit;
  open.pitIsHidden = true;
  open.pitIsOpen = true;
  open.floorPicture = PIT_PICTURE;
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 1, 1)->cell.kind == AutoMapCellFloor);
  CHECK(FindCell(before, 1, 1)->cell.floorPicture == AUTOMAP_NO_PICTURE);
  CHECK(AutoMapScene_PitLines(before, 1, 1).empty());
  // Open, it still reads as floor in the 3D view
  CHECK(FindCell(before, 2, 1)->cell.kind == AutoMapCellFloor);
  CHECK(FindCell(before, 2, 1)->cell.floorPicture == AUTOMAP_NO_PICTURE);
  CHECK(AutoMapScene_PitLines(before, 2, 1).empty());
  Learn(&knowledge, AutoMapFactPit, 1, 1);
  Learn(&knowledge, AutoMapFactPit, 2, 1);
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(after, 1, 1)->cell.kind == AutoMapCellPit);
  CHECK(FindCell(after, 1, 1)->cell.pitIsHidden);
  CHECK(HasLine(AutoMapScene_PitLines(after, 1, 1), "HIDDEN PIT CLOSED"));
  CHECK(FindCell(after, 2, 1)->cell.kind == AutoMapCellPit);
  CHECK(HasLine(AutoMapScene_PitLines(after, 2, 1), "HIDDEN PIT OPEN"));
}

static AutoMapLevelSnapshot WithTeleporterAt11(AutoMapCellKind kind)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &teleporter = At(&snapshot, 1, 1);
  teleporter.kind = kind;
  teleporter.hasTeleporterTarget = true;
  teleporter.teleporterDestLevel = LEVEL;
  teleporter.teleporterDestX = 2;
  teleporter.teleporterDestY = 1;
  teleporter.teleporterDestGlobalX = 2;
  teleporter.teleporterDestGlobalY = 1;
  teleporter.teleporterScope = 3;
  teleporter.teleporterRotation = 1;
  teleporter.teleporterFacingMode = 0;
  return snapshot;
}

static void InvisibleTeleporterIsFloorUntilUsed(void)
{
  AutoMapLevelSnapshot snapshot = WithTeleporterAt11(AutoMapCellInvisibleTeleporter);
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 1, 1)->cell.kind == AutoMapCellFloor);
  CHECK(AutoMapScene_TeleporterLines(before, 1, 1).empty());
  Learn(&knowledge, AutoMapFactTeleporter, 1, 1);
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(after, 1, 1)->cell.kind == AutoMapCellInvisibleTeleporter);
  std::vector<std::string> lines = AutoMapScene_TeleporterLines(after, 1, 1);
  CHECK(HasLineStartingWith(lines, "DEST:"));
  CHECK(!HasLineStartingWith(lines, "TAKES:"));
}

static void VisibleTeleporterNamesNoDestinationUntilUsed(void)
{
  AutoMapLevelSnapshot snapshot = WithTeleporterAt11(AutoMapCellTeleporter);
  AutoMapKnowledge knowledge;
  AutoMapDrawList drawList = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(drawList, 1, 1)->cell.kind == AutoMapCellTeleporter);
  CHECK(!FindCell(drawList, 1, 1)->cell.hasTeleporterTarget);
  std::vector<std::string> lines = AutoMapScene_TeleporterLines(drawList, 1, 1);
  CHECK(!HasLineStartingWith(lines, "DEST:"));
  CHECK(!HasLineStartingWith(lines, "TAKES:"));
}

static void SpinnerIsFloorUntilUsedAndKeepsItsTurnAfterwards(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &spinner = At(&snapshot, 1, 1);
  spinner.kind = AutoMapCellSpinner;
  spinner.spinnerRotation = 1;
  spinner.spinnerFacing = -1;
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 1, 1)->cell.kind == AutoMapCellFloor);
  CHECK(FindCell(before, 1, 1)->cell.spinnerRotation == -1);
  Learn(&knowledge, AutoMapFactTeleporter, 1, 1);
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(after, 1, 1)->cell.kind == AutoMapCellSpinner);
  CHECK(FindCell(after, 1, 1)->cell.spinnerRotation == 1);
}

static void VisibleSpinnerLooksLikeATeleporterUntilUsed(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &spinner = At(&snapshot, 1, 1);
  spinner.kind = AutoMapCellSpinner;
  spinner.spinnerRotation = 1;
  spinner.spinnerIsVisible = true;
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 1, 1)->cell.kind == AutoMapCellTeleporter);
  CHECK(FindCell(before, 1, 1)->cell.spinnerRotation == -1);
  CHECK(!FindCell(before, 1, 1)->cell.hasTeleporterTarget);
  Learn(&knowledge, AutoMapFactTeleporter, 1, 1);
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(after, 1, 1)->cell.kind == AutoMapCellSpinner);
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(FindCell(all, 1, 1)->cell.kind == AutoMapCellSpinner);
}

static void DormantTeleporterNeverShowsBelowAll(void)
{
  AutoMapLevelSnapshot snapshot = WithTeleporterAt11(AutoMapCellDormantTeleporter);
  AutoMapKnowledge knowledge;
  Learn(&knowledge, AutoMapFactTeleporter, 1, 1);
  AutoMapDrawList drawList = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(drawList, 1, 1)->cell.kind == AutoMapCellFloor);
  CHECK(AutoMapScene_TeleporterLines(drawList, 1, 1).empty());
}

// The reader grows the snapshot by a margin; facts and faces are in level coordinates
static void MarginKeepsFacesAndFactsAligned(void)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 5;
  snapshot.height = 4;
  snapshot.boundaryMarginX = 1;
  snapshot.boundaryMarginY = 1;
  snapshot.cells.assign(20, AutoMapCellRecord());
  At(&snapshot, 2, 2).kind = AutoMapCellFalseWall;
  AutoMapKnowledge wrongCell;
  Learn(&wrongCell, AutoMapFactFalseWall, 2, 2);
  AutoMapDrawList notRestored = Build(snapshot, AutoMapSpoilersLearned, &wrongCell);
  CHECK(FindCell(notRestored, 2, 2)->cell.kind == AutoMapCellWall);
  AutoMapKnowledge rightCell;
  Learn(&rightCell, AutoMapFactFalseWall, 1, 1);
  AutoMapDrawList restored = Build(snapshot, AutoMapSpoilersLearned, &rightCell);
  CHECK(FindCell(restored, 2, 2)->cell.kind == AutoMapCellFalseWall);

  // The face of snapshot cell (2,1) is level cell (1,0)
  At(&snapshot, 2, 1).wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  AutoMapVisibleCells visibleNow;
  AutoMapFog fog = Fog(true, AutoMapSpoilersSeen, explored, visibleNow, NULL);
  // A face marked at the unshifted cell is another wall's face
  explored.MarkFace(AutoMapCell{LEVEL, 2, 1}, AutoMapSouth);
  AutoMapDrawList hidden = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(hidden, 2, 1)->cell.wallSides[AutoMapSouth].picture == AUTOMAP_NO_PICTURE);
  explored.MarkFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapDrawList shown = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(shown, 2, 1)->cell.wallSides[AutoMapSouth].picture == LEVER_PICTURE);
}

// Regression pin: the debug placement text must never name a false wall
static void PlacementWordingNamesNoFalseWall(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  At(&snapshot, 3, 1).kind = AutoMapCellFalseWall;
  const char *falseWall =
      AutoMapScene_DescribePlacement(AutoMapScene_Placement(snapshot, 3, 1));
  const char *plainWall =
      AutoMapScene_DescribePlacement(AutoMapScene_Placement(snapshot, 3, 2));
  CHECK(std::string(falseWall) == plainWall);
}

static const char *FakeName(int nameIndex);

static AutoMapFloorActuator PadAt11(AutoMapRecordId record)
{
  AutoMapFloorActuator pad(1, 1, AutoMapActuatorPad);
  pad.record = record;
  return pad;
}

static void HiddenPadAppearsOnceKnownWithoutItsCondition(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator pad = PadAt11(3);
  pad.op = AutoMapPadPossession;
  pad.object.worldPicture = 40;
  pad.requiredFacing = 1;
  snapshot.actuators.push_back(pad);
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(before.actuators.empty());
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, 3));
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(after.actuators.size() == 1);
  CHECK(after.actuators[0].op == AutoMapPadAnything);
  CHECK(after.actuators[0].requiredFacing == -1);
  CHECK(after.actuators[0].object.worldPicture == AUTOMAP_NO_PICTURE);
  CHECK(!HasLineStartingWith(AutoMapScene_TriggerLines(after, 1, 1, FakeName), "TRIGGER"));
  // At All the pad keeps its condition
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(all.actuators[0].op == AutoMapPadPossession);
  CHECK(HasLineStartingWith(AutoMapScene_TriggerLines(all, 1, 1, FakeName), "TRIGGER: CARRY"));
}

// The pad on (1,1) opens the door on (2,1); both cells are explored, but the party looks away
static AutoMapLevelSnapshot PadOpeningTheDoor(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.actuators.push_back(PadAt11(3));
  At(&snapshot, 2, 1).kind = AutoMapCellDoor;
  AutoMapLink toDoor(1, 1, 2, 1, false, -1);
  toDoor.source = 3;
  snapshot.links.push_back(toDoor);
  snapshot.linkedCells.push_back(AutoMapCell{LEVEL, 2, 1});
  return snapshot;
}

static AutoMapDrawList BuildLookingAway(const AutoMapLevelSnapshot &snapshot,
                                        AutoMapSpoilerLevel spoilers,
                                        const AutoMapKnowledge *knowledge, bool padIsExplored)
{
  AutoMapExplored explored = Explored();
  if (padIsExplored) explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapVisibleCells nothingInSight;
  return AutoMapScene_BuildDrawList(snapshot,
                                    Fog(true, spoilers, explored, nothingInSight, knowledge));
}

// A pad does not move: once learned it stays on the map, with its link, like the door it opens;
// at All every pad on an explored cell does
static void LearnedPadStaysWhenThePartyLooksAway(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, 3));
  knowledge.Learn(AutoMapFact_OfLink(3, AutoMapCell{LEVEL, 2, 1}));
  AutoMapDrawList learned = BuildLookingAway(PadOpeningTheDoor(), AutoMapSpoilersLearned,
                                             &knowledge, true);
  CHECK(learned.actuators.size() == 1);
  CHECK(learned.links.size() == 1);
  // Not on a cell the party never explored
  AutoMapDrawList unexplored = BuildLookingAway(PadOpeningTheDoor(), AutoMapSpoilersLearned,
                                                &knowledge, false);
  CHECK(unexplored.actuators.empty());
  CHECK(unexplored.links.empty());
  // An unknown pad stays hidden, and Seen shows no pad at all
  AutoMapKnowledge nothingKnown;
  CHECK(BuildLookingAway(PadOpeningTheDoor(), AutoMapSpoilersLearned, &nothingKnown, true)
            .actuators.empty());
  CHECK(BuildLookingAway(PadOpeningTheDoor(), AutoMapSpoilersSeen, &knowledge, true)
            .actuators.empty());
  // All shows at least what Learned shows: every pad on an explored cell, with its link
  AutoMapDrawList all = BuildLookingAway(PadOpeningTheDoor(), AutoMapSpoilersAll, NULL, true);
  CHECK(all.actuators.size() == 1);
  CHECK(all.links.size() == 1);
  CHECK(BuildLookingAway(PadOpeningTheDoor(), AutoMapSpoilersAll, NULL, false).actuators.empty());
}

static void VisiblePressurePlateKeepsItsFloorPicture(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  At(&snapshot, 1, 1).floorPicture = 77;
  snapshot.actuators.push_back(PadAt11(3));
  AutoMapKnowledge knowledge;
  AutoMapDrawList drawList = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(drawList, 1, 1)->cell.floorPicture == 77);
  CHECK(drawList.actuators.empty());
}

static void CountersAndDormantPadsNeverShowBelowAll(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator cog(1, 1, AutoMapActuatorCog);
  cog.record = 4;
  cog.counterValue = 2;
  AutoMapFloorActuator dormant(2, 1, AutoMapActuatorDormantPad);
  dormant.record = 5;
  snapshot.actuators.push_back(cog);
  snapshot.actuators.push_back(dormant);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, 4));
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, 5));
  AutoMapDrawList drawList = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(drawList.actuators.empty());
  CHECK(Build(snapshot, AutoMapSpoilersAll, NULL).actuators.size() == 2);
}

static void KnownLauncherHasNoLaunchesLineBelowAll(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapLauncher launcher;
  launcher.side = AutoMapSouth;
  launcher.actuator = SWITCH_RECORD;
  launcher.actuatorType = 8;
  launcher.value = 0;
  At(&snapshot, 1, 0).wallLaunchers.push_back(launcher);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, SWITCH_RECORD));
  AutoMapDrawList learned = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(learned, 1, 0)->cell.wallLaunchers.size() == 1);
  CHECK(!HasLineStartingWith(AutoMapScene_RelayLines(learned, 1, 0), "LAUNCHES"));
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(HasLineStartingWith(AutoMapScene_RelayLines(all, 1, 0), "LAUNCHES"));
}

static void KnownSpentGeneratorShowsAsAGenerator(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapFloorActuator spent(1, 1, AutoMapActuatorSpentGenerator);
  spent.record = 6;
  spent.hasGeneratorHealth = true;
  spent.generatedCount = 3;
  spent.counterValue = 1;
  snapshot.actuators.push_back(spent);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, 6));
  AutoMapDrawList drawList = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(drawList.actuators.size() == 1);
  CHECK(drawList.actuators[0].kind == AutoMapActuatorGenerator);
  CHECK(!drawList.actuators[0].hasGeneratorHealth);
  CHECK(drawList.actuators[0].generatedCount == -1);
  CHECK(drawList.actuators[0].counterValue == -1);
}

// Lever (source 7) on wall (1,0) south links to a counter wall (3,0), which links to the
// door (2,1)
static AutoMapLevelSnapshot LeverCounterDoor(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  MakeLeverFunctional(&snapshot);
  At(&snapshot, 2, 1).kind = AutoMapCellDoor;
  At(&snapshot, 3, 0).wallRelays.push_back(CounterRelay(2));
  At(&snapshot, 3, 0).wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  AutoMapLink toCounter(1, 0, 3, 0, true, AutoMapSouth);
  toCounter.source = SWITCH_RECORD;
  toCounter.delay = 3;
  AutoMapLink toDoor(3, 0, 2, 1, true, AutoMapSouth, AutoMapSignalSet, false, 0, true);
  toDoor.source = 8;
  snapshot.links.push_back(toCounter);
  snapshot.links.push_back(toDoor);
  snapshot.linkedCells.push_back(AutoMapCell{LEVEL, 3, 0});
  snapshot.linkedCells.push_back(AutoMapCell{LEVEL, 2, 1});
  return snapshot;
}

static void KnownLinkSkipsAHiddenCounter(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, SWITCH_RECORD));
  knowledge.Learn(AutoMapFact_OfLink(SWITCH_RECORD, AutoMapCell{LEVEL, 2, 1}));
  AutoMapDrawList drawList = Build(LeverCounterDoor(), AutoMapSpoilersLearned, &knowledge);
  CHECK(drawList.links.size() == 1);
  CHECK(drawList.links[0].srcX == 1 && drawList.links[0].srcY == 0);
  CHECK(drawList.links[0].dstX == 2 && drawList.links[0].dstY == 1);
  CHECK(drawList.links[0].srcIsWall && drawList.links[0].srcSide == AutoMapSouth);
  CHECK(drawList.links[0].source == SWITCH_RECORD);
  CHECK(drawList.links[0].delay == 0 && !drawList.links[0].fromCounter);
  CHECK(drawList.linkedCells.size() == 1);
  std::vector<std::string> lines = AutoMapScene_RelayLines(drawList, 1, 0);
  CHECK(lines.size() == 1 && lines[0] == "TARGET: L0 (2, 1) DOOR");
  CHECK(AutoMapScene_RelayLines(drawList, 3, 0).empty());
  // At All the whole chain stays
  AutoMapDrawList all = Build(LeverCounterDoor(), AutoMapSpoilersAll, NULL);
  CHECK(all.links.size() == 2);
}

static void NoLinkWithoutAFact(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, SWITCH_RECORD));
  AutoMapDrawList drawList = Build(LeverCounterDoor(), AutoMapSpoilersLearned, &knowledge);
  CHECK(drawList.links.empty());
  CHECK(drawList.linkedCells.empty());
}

static void LinkOfADeletedActuatorDrawsNothing(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfLink(99, AutoMapCell{LEVEL, 2, 1}));
  AutoMapDrawList drawList = Build(LeverCounterDoor(), AutoMapSpoilersLearned, &knowledge);
  CHECK(drawList.links.empty());
}

// The reader grows the snapshot by a margin; the fact names the level cell
static void LinkTargetFollowsTheBoundaryMargin(void)
{
  AutoMapLevelSnapshot snapshot = LeverCounterDoor();
  AutoMapLevelSnapshot grown;
  grown.level = LEVEL;
  grown.width = 5;
  grown.height = 4;
  grown.boundaryMarginX = 1;
  grown.boundaryMarginY = 1;
  grown.cells.assign(20, AutoMapCellRecord());
  for (int y = 0; y < snapshot.height; y++)
  {
    for (int x = 0; x < snapshot.width; x++) At(&grown, x + 1, y + 1) = At(&snapshot, x, y);
  }
  AutoMapLink link(2, 1, 0, 0, true, AutoMapSouth);
  link.source = SWITCH_RECORD;
  grown.links.push_back(link);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfLink(SWITCH_RECORD, AutoMapCell{LEVEL, 2, 1}));
  AutoMapDrawList drawList = Build(grown, AutoMapSpoilersLearned, &knowledge);
  CHECK(drawList.links.size() == 1);
  CHECK(drawList.links[0].dstX == 3 && drawList.links[0].dstY == 2);
}

static const int SWORD = 11;
static const int CHEST = 10;
static const int SCROLL = 12;
static const AutoMapRecordId CHEST_RECORD = 11;
static const AutoMapRecordId SCROLL_RECORD = 21;

static AutoMapItem ItemOf(int picture, int nameIndex)
{
  AutoMapItem item;
  item.worldPicture = picture;
  item.nameIndex = nameIndex;
  return item;
}

static AutoMapLevelSnapshot WithChestAt11(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &cell = At(&snapshot, 1, 1);
  cell.itemPictures.push_back(ItemOf(CHEST, 1));
  AutoMapChestContents contents;
  contents.item = 0;
  contents.chest = CHEST_RECORD;
  contents.pictures.push_back(ItemOf(SWORD, 2));
  contents.pictures.push_back(ItemOf(SCROLL, 3));
  contents.pictureTexts.push_back("");
  contents.pictureTexts.push_back("SECRET");
  contents.pictureScrolls.push_back(AUTOMAP_NO_RECORD);
  contents.pictureScrolls.push_back(SCROLL_RECORD);
  cell.chestContents.push_back(contents);
  return snapshot;
}

static void ChestContentsOnlyOnceOpened(void)
{
  AutoMapLevelSnapshot snapshot = WithChestAt11();
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 1, 1)->cell.chestContents.empty());
  CHECK(FindCell(before, 1, 1)->cell.itemPictures.size() == 1);
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactChestOpened, CHEST_RECORD));
  AutoMapDrawList opened = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(opened, 1, 1)->cell.chestContents.size() == 1);
  const AutoMapChestContents &contents = FindCell(opened, 1, 1)->cell.chestContents[0];
  CHECK(contents.pictures.size() == 2);
  CHECK(contents.pictureTexts[1] == "");
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactScrollRead, SCROLL_RECORD));
  AutoMapDrawList read = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(read, 1, 1)->cell.chestContents[0].pictureTexts[1] == "SECRET");
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(FindCell(all, 1, 1)->cell.chestContents[0].pictureTexts[1] == "SECRET");
}

static void ScrollTextOnlyOnceRead(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &cell = At(&snapshot, 1, 1);
  cell.scrollTexts.push_back("HELLO");
  cell.scrollRecords.push_back(12);
  AutoMapAlcoveEntry entry;
  entry.item = ItemOf(SCROLL, 3);
  entry.side = AutoMapSouth;
  entry.text = "ALCOVE";
  entry.scroll = 13;
  At(&snapshot, 1, 0).wallSides[AutoMapSouth].isAlcove = true;
  At(&snapshot, 1, 0).alcoveContents.push_back(entry);
  AutoMapKnowledge knowledge;
  AutoMapDrawList before = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(before, 1, 1)->cell.scrollTexts.empty());
  CHECK(FindCell(before, 1, 1)->cell.scrollRecords.empty());
  CHECK(FindCell(before, 1, 0)->cell.alcoveContents.size() == 1);
  CHECK(FindCell(before, 1, 0)->cell.alcoveContents[0].text == "");
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactScrollRead, 12));
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactScrollRead, 13));
  AutoMapDrawList after = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(after, 1, 1)->cell.scrollTexts.size() == 1);
  CHECK(FindCell(after, 1, 1)->cell.scrollTexts[0] == "HELLO");
  CHECK(FindCell(after, 1, 1)->cell.scrollRecords.size() == 1);
  CHECK(FindCell(after, 1, 0)->cell.alcoveContents[0].text == "ALCOVE");
}

// Whole level below All: the 3D view draws the goods in a wall only on an alcove face, so the
// coins in a coin slot (CSB L10 (36,36)) stay hidden until the slot turns into an alcove or the
// party looked into it
static void WholeLevelShowsWallGoodsOnlyOnAnAlcove(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapAlcoveEntry coin;
  coin.item = ItemOf(SWORD, 2);
  coin.side = AutoMapSouth;
  At(&snapshot, 1, 0).alcoveContents.push_back(coin);
  AutoMapKnowledge knowledge;
  AutoMapDrawList slot = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(slot, 1, 0)->cell.alcoveContents.empty());
  CHECK(AutoMapScene_SearchItems(slot, "sword", FakeName, NULL).empty());
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(FindCell(all, 1, 0)->cell.alcoveContents.size() == 1);
  AutoMapExplored explored = Explored();
  explored.MarkAlcoveLookedInto(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList lookedInto = AutoMapScene_BuildDrawList(
      snapshot, Fog(false, AutoMapSpoilersLearned, explored, visibleNow, &knowledge));
  CHECK(FindCell(lookedInto, 1, 0)->cell.alcoveContents.size() == 1);
  At(&snapshot, 1, 0).wallSides[AutoMapSouth].isAlcove = true;
  AutoMapDrawList alcove = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(alcove, 1, 0)->cell.alcoveContents.size() == 1);
}

// The reader flattens a chest in an alcove into the alcove's entries: the chest, then each item
// it holds tagged with the chest
static AutoMapLevelSnapshot WithChestInTheAlcove(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapAlcoveEntry chest;
  chest.item = ItemOf(CHEST, 1);
  chest.side = AutoMapSouth;
  AutoMapAlcoveEntry sword;
  sword.item = ItemOf(SWORD, 2);
  sword.side = AutoMapSouth;
  sword.inChest = CHEST_RECORD;
  At(&snapshot, 1, 0).wallSides[AutoMapSouth].isAlcove = true;
  At(&snapshot, 1, 0).alcoveContents.push_back(chest);
  At(&snapshot, 1, 0).alcoveContents.push_back(sword);
  return snapshot;
}

static void ChestInAnAlcoveShowsItsContentsOnlyOnceOpened(void)
{
  AutoMapLevelSnapshot snapshot = WithChestInTheAlcove();
  AutoMapKnowledge knowledge;
  AutoMapDrawList closed = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(closed, 1, 0)->cell.alcoveContents.size() == 1);
  CHECK(FindCell(closed, 1, 0)->cell.alcoveContents[0].item.worldPicture == CHEST);
  CHECK(AutoMapScene_SearchItems(closed, "sword", FakeName, NULL).empty());
  CHECK(AutoMapScene_SearchItems(closed, "chest", FakeName, NULL).size() == 1);
  AutoMapDrawList seen = Build(snapshot, AutoMapSpoilersSeen, NULL);
  CHECK(FindCell(seen, 1, 0)->cell.alcoveContents.size() == 1);
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactChestOpened, CHEST_RECORD));
  AutoMapDrawList opened = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(FindCell(opened, 1, 0)->cell.alcoveContents.size() == 2);
  CHECK(AutoMapScene_SearchItems(opened, "sword", FakeName, NULL).size() == 1);
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(FindCell(all, 1, 0)->cell.alcoveContents.size() == 2);
  CHECK(AutoMapScene_SearchItems(all, "sword", FakeName, NULL).size() == 1);
}

static AutoMapLevelSnapshot WithMummyAt11(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapMonsterRecord mummy = AutoMapMonsterRecord();
  mummy.x = 1;
  mummy.y = 1;
  mummy.memberCount = 1;
  mummy.possessionPictures.push_back(ItemOf(30, 4));
  mummy.possessionTexts.push_back("");
  mummy.residuePictures.push_back(ItemOf(31, 5));
  mummy.chanceResiduePictures.push_back(ItemOf(32, 6));
  snapshot.monsters.push_back(mummy);
  return snapshot;
}

static const AutoMapRecordId MUMMIES_RECORD = 0x40002;

// Two mummies with 42 and 35 hit points on (1, 1)
static AutoMapLevelSnapshot WithHurtMummiesAt11(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapMonsterRecord mummies = AutoMapMonsterRecord();
  mummies.x = 1;
  mummies.y = 1;
  mummies.memberCount = 2;
  mummies.record = MUMMIES_RECORD;
  mummies.memberHealth.push_back(42);
  mummies.memberHealth.push_back(35);
  snapshot.monsters.push_back(mummies);
  return snapshot;
}

// Fog off, like Build, with the damage store the hooks keep
static AutoMapDrawList BuildWithDamage(const AutoMapLevelSnapshot &snapshot,
                                       AutoMapSpoilerLevel spoilers,
                                       const AutoMapMonsterDamage *damage)
{
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = {false, &explored, &visibleNow, false, spoilers, &knowledge,
                    NULL, NULL, NULL, NULL, damage};
  return AutoMapScene_BuildDrawList(snapshot, fog);
}

static void BelowAllNoHitPointsAreShown(void)
{
  AutoMapMonsterDamage damage;
  AutoMapSpoilerLevel levels[2] = {AutoMapSpoilersSeen, AutoMapSpoilersLearned};
  for (int i = 0; i < 2; i++)
  {
    AutoMapDrawList drawList = BuildWithDamage(WithHurtMummiesAt11(), levels[i], &damage);
    CHECK(drawList.monsters.size() == 1);
    CHECK(drawList.monsters[0].memberHealth.empty());
    CHECK(AutoMapScene_MonsterHealthLines(drawList, 1, 1).empty());
  }
}

static void BelowAllEachHitMemberShowsItsDamage(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(MUMMIES_RECORD, 1, 17);
  AutoMapDrawList drawList = BuildWithDamage(WithHurtMummiesAt11(), AutoMapSpoilersSeen, &damage);
  std::vector<std::string> lines = AutoMapScene_MonsterHealthLines(drawList, 1, 1);
  CHECK(lines.size() == 1);
  CHECK(lines.size() == 1 && lines[0] == "DMG 17");
  CHECK(drawList.monsters[0].memberDamage.size() == 2);
  CHECK(drawList.monsters[0].memberDamage.size() == 2 && drawList.monsters[0].memberDamage[0] == 0
        && drawList.monsters[0].memberDamage[1] == 17);
}

static void AtAllHitPointsShowAndDamageDoesNot(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(MUMMIES_RECORD, 1, 17);
  AutoMapDrawList drawList = BuildWithDamage(WithHurtMummiesAt11(), AutoMapSpoilersAll, &damage);
  std::vector<std::string> lines = AutoMapScene_MonsterHealthLines(drawList, 1, 1);
  CHECK(lines.size() == 2);
  CHECK(lines.size() == 2 && lines[0] == "HP 42" && lines[1] == "HP 35");
  CHECK(drawList.monsters[0].memberDamage.empty());
}

static void NoDamageStoreShowsNoDamage(void)
{
  AutoMapDrawList drawList = BuildWithDamage(WithHurtMummiesAt11(), AutoMapSpoilersLearned, NULL);
  CHECK(AutoMapScene_MonsterHealthLines(drawList, 1, 1).empty());
}

static const char *NoName(int)
{
  return "";
}

static void AnUnseenFountainIsNotFound(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  At(&snapshot, 1, 0).wallSides[AutoMapSouth].feature = AutoMapWallFeatureFountain;
  // The 3D view explores the wall it shows as well as the floor in front of it
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList unseen =
      AutoMapScene_BuildDrawList(snapshot, Fog(true, AutoMapSpoilersSeen, explored, visibleNow, NULL));
  CHECK(AutoMapScene_SearchItems(unseen, "fountain", NoName, NULL).empty());
  AutoMapDrawList all =
      AutoMapScene_BuildDrawList(snapshot, Fog(true, AutoMapSpoilersAll, explored, visibleNow, NULL));
  CHECK(AutoMapScene_SearchItems(all, "fountain", NoName, NULL).size() == 1);
  explored.MarkFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapDrawList seen =
      AutoMapScene_BuildDrawList(snapshot, Fog(true, AutoMapSpoilersSeen, explored, visibleNow, NULL));
  CHECK(AutoMapScene_SearchItems(seen, "fountain", NoName, NULL).size() == 1);
}

static void MonsterCarriesNothingBelowAll(void)
{
  AutoMapLevelSnapshot snapshot = WithMummyAt11();
  AutoMapKnowledge knowledge;
  AutoMapDrawList hidden = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(hidden.monsters.size() == 1);
  CHECK(hidden.monsters[0].possessionPictures.empty());
  CHECK(hidden.monsters[0].possessionTexts.empty());
  CHECK(hidden.monsters[0].residuePictures.empty());
  CHECK(hidden.monsters[0].chanceResiduePictures.empty());
  AutoMapDrawList seen = Build(snapshot, AutoMapSpoilersSeen, NULL);
  CHECK(seen.monsters[0].possessionPictures.empty());
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(all.monsters[0].possessionPictures.size() == 1);
  CHECK(all.monsters[0].possessionTexts.size() == 1);
  CHECK(all.monsters[0].residuePictures.size() == 1);
  CHECK(all.monsters[0].chanceResiduePictures.size() == 1);
}

static const AutoMapRecordId GROUP_RECORD = 77;

// Party on (1,1) facing east, one monster group (record 77) on (2,1)
static AutoMapLevelSnapshot WithGroupAt21(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapMonsterRecord monster = AutoMapMonsterRecord();
  monster.x = 2;
  monster.y = 1;
  monster.memberCount = 1;
  monster.record = GROUP_RECORD;
  snapshot.monsters.push_back(monster);
  return snapshot;
}

// Fog on; the party sees (2,1) in the ray when inRay. seen is what the last frame measured,
// NULL for a fog that was not given a measurement
static size_t MonstersShown(AutoMapSpoilerLevel spoilers, bool inRay,
                            const std::vector<AutoMapRecordId> *seen)
{
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  if (inRay) visibleNow.Add(2, 1);
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, spoilers, explored, visibleNow, &knowledge);
  fog.items = NULL;
  fog.monstersSeenNow = seen;
  return AutoMapScene_BuildDrawList(WithGroupAt21(), fog).monsters.size();
}

static void MeasuredMonsterIsShown(void)
{
  std::vector<AutoMapRecordId> seen(1, GROUP_RECORD);
  CHECK(MonstersShown(AutoMapSpoilersLearned, true, &seen) == 1);
}

static void UnmeasuredMonsterInTheRayIsNotShown(void)
{
  std::vector<AutoMapRecordId> seen;
  CHECK(MonstersShown(AutoMapSpoilersLearned, true, &seen) == 0);
  CHECK(MonstersShown(AutoMapSpoilersSeen, true, &seen) == 0);
}

static void MeasuredMonsterOutsideTheRayIsShown(void)
{
  std::vector<AutoMapRecordId> seen(1, GROUP_RECORD);
  CHECK(MonstersShown(AutoMapSpoilersLearned, false, &seen) == 1);
}

static void AllKeepsTheRay(void)
{
  std::vector<AutoMapRecordId> seen;
  CHECK(MonstersShown(AutoMapSpoilersAll, true, &seen) == 1);
  CHECK(MonstersShown(AutoMapSpoilersAll, false, &seen) == 0);
}

// A snapshot of another level gets an empty list from ShowLastSnapshot: nothing is seen there
static void NoMonsterSeenWithoutAFrame(void)
{
  AutoMapLevelSnapshot snapshot = WithGroupAt21();
  snapshot.level = LEVEL + 3;
  snapshot.party.x = 3;
  std::vector<AutoMapRecordId> seen;
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  // In the ray, so the old ray rule would show the group
  visibleNow.Add(2, 1);
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fog.monstersSeenNow = &seen;
  CHECK(AutoMapScene_BuildDrawList(snapshot, fog).monsters.empty());
}

static const char *FakeName(int nameIndex)
{
  switch (nameIndex)
  {
  case 1: return "CHEST";
  case 2: return "SWORD";
  case 4: return "CLUB";
  default: return "";
  }
}

// The search lists a chest, not what is inside it, so the chest's own sword is not a hit at
// any level; a monster's club is a hit at All only.
static void SearchFindsNoHiddenItems(void)
{
  AutoMapLevelSnapshot snapshot = WithMummyAt11();
  AutoMapLevelSnapshot withChest = WithChestAt11();
  At(&snapshot, 2, 1) = At(&withChest, 1, 1);
  AutoMapKnowledge knowledge;
  AutoMapDrawList hidden = Build(snapshot, AutoMapSpoilersLearned, &knowledge);
  CHECK(AutoMapScene_SearchItems(hidden, "club", FakeName, NULL).empty());
  CHECK(AutoMapScene_SearchItems(hidden, "sword", FakeName, NULL).empty());
  CHECK(AutoMapScene_SearchItems(hidden, "chest", FakeName, NULL).size() == 1);
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(AutoMapScene_SearchItems(all, "club", FakeName, NULL).size() == 1);
  CHECK(AutoMapScene_SearchItems(all, "chest", FakeName, NULL).size() == 1);
}

// A live club (record 0x10, description 27) on (2,1); descriptions 27 and 8 have pictures 11 and 12
static const int CLUB_PICTURE = 11;
static const int OTHER_PICTURE = 12;
// What the description of the club draws; the live club has its own picture
static const int CLUB_DESC_PICTURE = 21;

static AutoMapLevelSnapshot WithLiveClub(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapItem club;
  club.worldPicture = CLUB_PICTURE;
  At(&snapshot, 2, 1).itemPictures.push_back(club);
  AutoMapFloorItemTag tag = {0x10u, 27};
  At(&snapshot, 2, 1).itemTags.push_back(tag);
  snapshot.itemOfDescIndex[27].worldPicture = CLUB_DESC_PICTURE;
  snapshot.itemOfDescIndex[8].worldPicture = OTHER_PICTURE;
  return snapshot;
}

static void Remember(AutoMapItemMemory *memory, AutoMapRecordId record, int descIndex, int x, int y)
{
  AutoMapRememberedItem item = {record, descIndex, {LEVEL, x, y}};
  memory->See(item);
}

// Fog on, the party sees the whole corridor; the explored cells are the whole corridor
static AutoMapDrawList BuildRemembering(const AutoMapLevelSnapshot &snapshot,
                                        AutoMapSpoilerLevel spoilers,
                                        const AutoMapItemMemory &memory)
{
  AutoMapExplored explored = Explored();
  explored.Mark({LEVEL, 1, 1});
  explored.Mark({LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, spoilers, explored, visibleNow, &knowledge);
  fog.items = &memory;
  return AutoMapScene_BuildDrawList(snapshot, fog);
}

static void UnseenLiveItemIsNotShown(void)
{
  AutoMapItemMemory memory;
  AutoMapDrawList drawList = BuildRemembering(WithLiveClub(), AutoMapSpoilersLearned, memory);
  CHECK(FindCell(drawList, 2, 1) != NULL);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.empty());
}

static void RememberedItemShowsWhereItWasSeen(void)
{
  AutoMapItemMemory memory;
  Remember(&memory, 0x10u, 27, 2, 1);
  AutoMapDrawList drawList = BuildRemembering(WithLiveClub(), AutoMapSpoilersLearned, memory);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.size() == 1);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures[0].worldPicture == CLUB_PICTURE);
}

static void RememberedItemGoneFromThePileStillShows(void)
{
  AutoMapItemMemory memory;
  Remember(&memory, 0x20u, 8, 2, 1);
  AutoMapDrawList drawList = BuildRemembering(WithLiveClub(), AutoMapSpoilersSeen, memory);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.size() == 1);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures[0].worldPicture == OTHER_PICTURE);
}

// Only (1,1) explored: the club on (2,1) was glimpsed in the dark, beyond the light
static AutoMapDrawList BuildWithAGlimpse(AutoMapSpoilerLevel spoilers, bool isExplored)
{
  AutoMapExplored explored = Explored();
  explored.Mark({LEVEL, 1, 1});
  if (isExplored) explored.Mark({LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapItemMemory memory;
  AutoMapRememberedItem glimpse = {0x10u, 27, {LEVEL, 2, 1}};
  glimpse.isGlimpsed = true;
  memory.See(glimpse);
  AutoMapFog fog = Fog(true, spoilers, explored, visibleNow, &knowledge);
  fog.items = &memory;
  return AutoMapScene_BuildDrawList(WithLiveClub(), fog);
}

static bool HasGlimpseAt(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.glimpsedItems.size(); i++)
  {
    if (drawList.glimpsedItems[i].x == x && drawList.glimpsedItems[i].y == y) return true;
  }
  return false;
}

// The cell stays in the fog: the "?" says that something lies there, not what, nor on what floor
static void AGlimpseOnAnUnexploredCellShowsOnlyAQuestionMark(void)
{
  AutoMapDrawList drawList = BuildWithAGlimpse(AutoMapSpoilersLearned, false);
  CHECK(FindCell(drawList, 2, 1) == NULL);
  CHECK(HasGlimpseAt(drawList, 2, 1));
}

static void AGlimpseOnAnExploredCellShowsNoPicture(void)
{
  AutoMapDrawList drawList = BuildWithAGlimpse(AutoMapSpoilersSeen, true);
  CHECK(FindCell(drawList, 2, 1) != NULL);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.empty());
  CHECK(HasGlimpseAt(drawList, 2, 1));
}

static void AllShowsTheItemNotAGlimpse(void)
{
  AutoMapDrawList drawList = BuildWithAGlimpse(AutoMapSpoilersAll, true);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.size() == 1);
  CHECK(drawList.glimpsedItems.empty());
}

static void AllAndFogOffShowTheLivePile(void)
{
  AutoMapItemMemory memory;
  AutoMapDrawList all = BuildRemembering(WithLiveClub(), AutoMapSpoilersAll, memory);
  CHECK(FindCell(all, 2, 1)->cell.itemPictures.size() == 1);
  AutoMapExplored explored = Explored();
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(false, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fog.items = &memory;
  AutoMapDrawList fogOff = AutoMapScene_BuildDrawList(WithLiveClub(), fog);
  CHECK(FindCell(fogOff, 2, 1)->cell.itemPictures.size() == 1);
}

static void UnopenedChestStillHidesWhatItHolds(void)
{
  AutoMapLevelSnapshot snapshot = WithLiveClub();
  AutoMapChestContents contents;
  contents.item = 0;
  contents.chest = 0x10u;
  contents.pictures.push_back(AutoMapItem());
  contents.pictureTexts.push_back("");
  contents.pictureScrolls.push_back(AUTOMAP_NO_RECORD);
  At(&snapshot, 2, 1).chestContents.push_back(contents);
  AutoMapItemMemory memory;
  Remember(&memory, 0x10u, 27, 2, 1);
  AutoMapDrawList drawList = BuildRemembering(snapshot, AutoMapSpoilersLearned, memory);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.size() == 1);
  CHECK(FindCell(drawList, 2, 1)->cell.chestContents.empty());
}

static void RememberedItemsShowOnAnotherLevel(void)
{
  AutoMapLevelSnapshot snapshot = WithLiveClub();
  snapshot.level = 4;
  snapshot.party.x = 0;
  snapshot.partyIsOnShownLevel = false;
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(5, AutoMapLevelSize{4, 3}));
  explored.Mark({4, 2, 1});
  AutoMapVisibleCells visibleNow;   // the party is elsewhere and sees nothing of this level
  AutoMapKnowledge knowledge;
  AutoMapItemMemory memory;
  AutoMapRememberedItem item = {0x10u, 27, {4, 2, 1}};
  memory.See(item);
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fog.items = &memory;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(drawList, 2, 1) != NULL);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.size() == 1);
}

static void ExploredItemsAreRememberedFromAnOldMap(void)
{
  AutoMapLevelSnapshot snapshot = WithLiveClub();
  AutoMapItem club;
  club.worldPicture = CLUB_PICTURE;
  At(&snapshot, 3, 1).itemPictures.push_back(club);
  AutoMapFloorItemTag tag = {0x11u, 27};
  At(&snapshot, 3, 1).itemTags.push_back(tag);
  AutoMapExplored explored = Explored();
  explored.Mark({LEVEL, 2, 1});
  AutoMapItemMemory memory;
  AutoMapScene_RememberExploredItems(snapshot, explored, &memory);
  AutoMapCell cell21 = {LEVEL, 2, 1};
  AutoMapCell cell31 = {LEVEL, 3, 1};
  CHECK(memory.ItemsOn(cell21).size() == 1);
  CHECK(memory.ItemsOn(cell31).empty());
}

static AutoMapItem Picture(int picture)
{
  AutoMapItem item;
  item.worldPicture = picture;
  return item;
}

static void PutLive(AutoMapLevelSnapshot *snapshot, AutoMapRecordId record, int descIndex, int picture)
{
  AutoMapFloorItemTag tag = {record, descIndex};
  At(snapshot, 2, 1).itemPictures.push_back(Picture(picture));
  At(snapshot, 2, 1).itemTags.push_back(tag);
}

static void LiveEntryWithAnotherDescriptionIsNotUsed(void)
{
  AutoMapItemMemory memory;
  Remember(&memory, 0x10u, 8, 2, 1);   // the live club is record 0x10 with description 27
  AutoMapDrawList drawList = BuildRemembering(WithLiveClub(), AutoMapSpoilersLearned, memory);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.size() == 1);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures[0].worldPicture == OTHER_PICTURE);
}

static void UnknownDescriptionIsLeftOut(void)
{
  AutoMapItemMemory memory;
  Remember(&memory, 0x30u, 99, 2, 1);
  Remember(&memory, 0x10u, 8, 2, 1);
  AutoMapDrawList drawList = BuildRemembering(WithLiveClub(), AutoMapSpoilersLearned, memory);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures.size() == 1);
  CHECK(FindCell(drawList, 2, 1)->cell.itemPictures[0].worldPicture == OTHER_PICTURE);
}

static AutoMapChestContents ChestHolding(AutoMapRecordId chest, size_t item, int picture)
{
  AutoMapChestContents contents;
  contents.item = item;
  contents.chest = chest;
  contents.pictures.push_back(Picture(picture));
  contents.pictureTexts.push_back("");
  contents.pictureScrolls.push_back(AUTOMAP_NO_RECORD);
  return contents;
}

static void ChestContentsFollowTheirChestToTheNewPosition(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  PutLive(&snapshot, 0x40u, 5, 30);   // dagger
  PutLive(&snapshot, 0x41u, 6, 31);   // chest A
  PutLive(&snapshot, 0x42u, 6, 32);   // chest B
  At(&snapshot, 2, 1).chestContents.push_back(ChestHolding(0x41u, 1, 50));
  At(&snapshot, 2, 1).chestContents.push_back(ChestHolding(0x42u, 2, 51));
  AutoMapItemMemory memory;
  Remember(&memory, 0x42u, 6, 2, 1);
  Remember(&memory, 0x40u, 5, 2, 1);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactChestOpened, 0x42u));
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactChestOpened, 0x41u));
  AutoMapExplored explored = Explored();
  explored.Mark({LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fog.items = &memory;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, fog);
  const AutoMapCellRecord &cell = FindCell(drawList, 2, 1)->cell;
  CHECK(cell.itemPictures.size() == 2);
  CHECK(cell.itemPictures[0].worldPicture == 32 && cell.itemPictures[1].worldPicture == 30);
  CHECK(cell.chestContents.size() == 1);
  CHECK(cell.chestContents[0].chest == 0x42u && cell.chestContents[0].item == 0);
  CHECK(cell.chestContents[0].pictures[0].worldPicture == 51);
}

static void ScrollTextsStayParallelToTheRememberedScrolls(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  PutLive(&snapshot, 0x50u, 7, 40);
  PutLive(&snapshot, 0x51u, 7, 40);
  AutoMapCellRecord &cell = At(&snapshot, 2, 1);
  cell.scrollTexts.push_back("first");
  cell.scrollRecords.push_back(0x50u);
  cell.scrollTexts.push_back("second");
  cell.scrollRecords.push_back(0x51u);
  AutoMapItemMemory memory;
  Remember(&memory, 0x51u, 7, 2, 1);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactScrollRead, 0x50u));
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactScrollRead, 0x51u));
  AutoMapExplored explored = Explored();
  explored.Mark({LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fog.items = &memory;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, fog);
  const AutoMapCellRecord &shown = FindCell(drawList, 2, 1)->cell;
  CHECK(shown.scrollTexts.size() == 1 && shown.scrollRecords.size() == 1);
  CHECK(shown.scrollTexts[0] == "second" && shown.scrollRecords[0] == 0x51u);
}

static void RememberedPileKeepsItsOrderAndTruncates(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  for (int i = 0; i < 4; i++) snapshot.itemOfDescIndex[100 + i] = Picture(60 + i);
  AutoMapItemMemory memory;
  for (int i = 0; i < 4; i++) Remember(&memory, 0x60u + i, 100 + i, 2, 1);
  AutoMapDrawList drawList = BuildRemembering(snapshot, AutoMapSpoilersLearned, memory);
  const AutoMapShownCell *shown = FindCell(drawList, 2, 1);
  CHECK(shown->cell.itemPictures.size() == 3);
  CHECK(shown->cell.itemPictures[0].worldPicture == 60 && shown->cell.itemPictures[2].worldPicture == 62);
  CHECK(shown->hasMoreItems);
  CHECK(shown->allItemPictures.size() == 4);
}

// The door at (2,1) is live open; the party last saw it closed
static AutoMapLevelSnapshot WithLiveOpenDoor(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &door = At(&snapshot, 2, 1);
  door.kind = AutoMapCellDoor;
  door.doorState = AutoMapDoorOpen;
  return snapshot;
}

static void RememberLook(AutoMapCellMemory *memory, int x, int y, const AutoMapCellLook &look)
{
  AutoMapCell cell = {LEVEL, x, y};
  memory->RememberIfNew(cell, look);
}

static AutoMapDrawList BuildWithLooks(const AutoMapLevelSnapshot &snapshot, bool fogIsOn,
                                      AutoMapSpoilerLevel spoilers, const AutoMapCellMemory *cells)
{
  AutoMapExplored explored = Explored();
  explored.Mark({LEVEL, 1, 1});
  explored.Mark({LEVEL, 2, 1});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(fogIsOn, spoilers, explored, visibleNow, &knowledge);
  fog.cells = cells;
  return AutoMapScene_BuildDrawList(snapshot, fog);
}

static void RememberedDoorDrawsAsSeen(void)
{
  AutoMapLevelSnapshot snapshot = WithLiveOpenDoor();
  AutoMapCellLook look = AutoMapScene_LookOf(At(&snapshot, 2, 1));
  look.doorState = AutoMapDoorClosed;
  AutoMapCellMemory memory;
  RememberLook(&memory, 2, 1, look);
  AutoMapSpoilerLevel below[2] = {AutoMapSpoilersLearned, AutoMapSpoilersSeen};
  for (int i = 0; i < 2; i++)
  {
    AutoMapDrawList drawList = BuildWithLooks(snapshot, true, below[i], &memory);
    CHECK(FindCell(drawList, 2, 1)->cell.doorState == AutoMapDoorClosed);
  }
  AutoMapDrawList all = BuildWithLooks(snapshot, true, AutoMapSpoilersAll, &memory);
  CHECK(FindCell(all, 2, 1)->cell.doorState == AutoMapDoorOpen);
  AutoMapDrawList fogOff = BuildWithLooks(snapshot, false, AutoMapSpoilersLearned, &memory);
  CHECK(FindCell(fogOff, 2, 1)->cell.doorState == AutoMapDoorOpen);
  AutoMapDrawList none = BuildWithLooks(snapshot, true, AutoMapSpoilersLearned, NULL);
  CHECK(FindCell(none, 2, 1)->cell.doorState == AutoMapDoorOpen);
}

static void RememberedLookKeepsTheSecretRules(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &pit = At(&snapshot, 2, 1);
  pit.kind = AutoMapCellPit;
  pit.pitIsHidden = true;
  pit.floorPicture = PIT_PICTURE;
  AutoMapCellMemory memory;
  RememberLook(&memory, 2, 1, AutoMapScene_LookOf(pit));
  // The pit has opened since, in the live level
  pit.pitIsOpen = true;
  AutoMapDrawList drawList = BuildWithLooks(snapshot, true, AutoMapSpoilersLearned, &memory);
  CHECK(FindCell(drawList, 2, 1)->cell.kind == AutoMapCellFloor);
  CHECK(FindCell(drawList, 2, 1)->cell.floorPicture == AUTOMAP_NO_PICTURE);
}

// An unknown concealed pit remembered open is floor below All, as the eye saw it; at All the
// live pit shows
static void UnknownConcealedPitRememberedOpenIsFloor(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &pit = At(&snapshot, 2, 1);
  pit.kind = AutoMapCellPit;
  pit.pitIsHidden = true;
  pit.pitIsOpen = true;
  pit.floorPicture = PIT_PICTURE;
  AutoMapCellMemory memory;
  RememberLook(&memory, 2, 1, AutoMapScene_LookOf(pit));
  AutoMapSpoilerLevel below[2] = {AutoMapSpoilersLearned, AutoMapSpoilersSeen};
  for (int i = 0; i < 2; i++)
  {
    AutoMapDrawList drawList = BuildWithLooks(snapshot, true, below[i], &memory);
    CHECK(FindCell(drawList, 2, 1)->cell.kind == AutoMapCellFloor);
    CHECK(FindCell(drawList, 2, 1)->cell.floorPicture == AUTOMAP_NO_PICTURE);
    CHECK(AutoMapScene_PitLines(drawList, 2, 1).empty());
  }
  AutoMapDrawList all = BuildWithLooks(snapshot, true, AutoMapSpoilersAll, &memory);
  CHECK(FindCell(all, 2, 1)->cell.kind == AutoMapCellPit);
  CHECK(FindCell(all, 2, 1)->cell.pitIsOpen);
  CHECK(HasLine(AutoMapScene_PitLines(all, 2, 1), "HIDDEN PIT OPEN"));
}

static void RememberedSideOverridesOnlyThatSide(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &wall = At(&snapshot, 1, 0);
  wall.wallSides[AutoMapEast].picture = 90;
  AutoMapCellLook look = AutoMapScene_LookOf(wall);
  look.sidePictures[AutoMapSouth] = 150;
  AutoMapCellMemory memory;
  RememberLook(&memory, 1, 0, look);
  // Since then the lever picture changed and so did the east side
  wall.wallSides[AutoMapSouth].picture = 151;
  wall.wallSides[AutoMapEast].picture = 92;
  AutoMapExplored explored = Explored();
  explored.Mark({LEVEL, 1, 0});
  explored.MarkFace({LEVEL, 1, 0}, AutoMapSouth);
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, AutoMapSpoilersLearned, explored, visibleNow, &knowledge);
  fog.cells = &memory;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, fog);
  // The seen south face shows the remembered picture
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].picture == 150);
  // The remembered east picture is of a face never seen: still blank, never the live 92
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapEast].picture == AUTOMAP_NO_PICTURE);
  // The same cell without a memory keeps the live picture on the seen face
  fog.cells = NULL;
  drawList = AutoMapScene_BuildDrawList(snapshot, fog);
  CHECK(FindCell(drawList, 1, 0)->cell.wallSides[AutoMapSouth].picture == 151);
}

static void UnrememberedCellDrawsLive(void)
{
  AutoMapLevelSnapshot snapshot = WithLiveOpenDoor();
  AutoMapCellMemory memory;
  AutoMapDrawList drawList = BuildWithLooks(snapshot, true, AutoMapSpoilersLearned, &memory);
  CHECK(FindCell(drawList, 2, 1)->cell.doorState == AutoMapDoorOpen);
}

static void RememberedDestroyedDoorDrawsWithoutLiveRemnants(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &door = At(&snapshot, 2, 1);
  door.kind = AutoMapCellDoor;
  door.doorState = AutoMapDoorClosed;
  door.doorBreakableWithAxe = true;
  door.doorBreakForce = 40;
  AutoMapCellLook look = AutoMapScene_LookOf(door);
  look.doorState = AutoMapDoorDestroyed;
  AutoMapCellMemory memory;
  RememberLook(&memory, 2, 1, look);
  AutoMapDrawList drawList = BuildWithLooks(snapshot, true, AutoMapSpoilersLearned, &memory);
  CHECK(FindCell(drawList, 2, 1)->cell.doorState == AutoMapDoorDestroyed);
  CHECK(!HasLineStartingWith(AutoMapScene_RelayLines(drawList, 2, 1), "BREAK FORCE"));
}

static void RememberExploredLooksTakesOnlyNewExploredCells(void)
{
  AutoMapLevelSnapshot snapshot = WithLiveOpenDoor();
  AutoMapExplored explored = Explored();
  explored.Mark({LEVEL, 1, 1});
  explored.Mark({LEVEL, 2, 1});
  AutoMapCellLook old = AutoMapScene_LookOf(At(&snapshot, 2, 1));
  old.doorState = AutoMapDoorClosed;
  AutoMapCellMemory memory;
  RememberLook(&memory, 2, 1, old);
  AutoMapScene_RememberExploredLooks(snapshot, explored, &memory);
  AutoMapCellLook got;
  AutoMapCell c11 = {LEVEL, 1, 1};
  AutoMapCell c21 = {LEVEL, 2, 1};
  AutoMapCell c31 = {LEVEL, 3, 1};
  CHECK(memory.LookOf(c11, &got));
  CHECK(got.kind == AutoMapCellFloor);
  CHECK(memory.LookOf(c21, &got));
  CHECK(got.doorState == AutoMapDoorClosed);
  CHECK(!memory.LookOf(c31, &got));
  CHECK(memory.Looks().size() == 2);
}

// A spinner switched on out of sight: the party last saw it dormant, so no field shows
static void RememberedDormantSpinnerHidesTheLiveField(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &spinner = At(&snapshot, 2, 1);
  spinner.kind = AutoMapCellDormantSpinner;
  spinner.spinnerRotation = 1;
  spinner.spinnerIsVisible = false;
  AutoMapCellMemory memory;
  RememberLook(&memory, 2, 1, AutoMapScene_LookOf(spinner));
  spinner.kind = AutoMapCellSpinner;
  spinner.spinnerIsVisible = true;
  AutoMapDrawList drawList = BuildWithLooks(snapshot, true, AutoMapSpoilersLearned, &memory);
  CHECK(FindCell(drawList, 2, 1)->cell.kind != AutoMapCellTeleporter);
  CHECK(FindCell(drawList, 2, 1)->cell.kind == AutoMapCellFloor);
  AutoMapDrawList all = BuildWithLooks(snapshot, true, AutoMapSpoilersAll, &memory);
  CHECK(FindCell(all, 2, 1)->cell.kind == AutoMapCellSpinner);
  AutoMapDrawList fogOff = BuildWithLooks(snapshot, false, AutoMapSpoilersLearned, &memory);
  CHECK(FindCell(fogOff, 2, 1)->cell.kind == AutoMapCellTeleporter);
}

// The reader adds a column of rock west of the level (boundaryMarginX = 1). Snapshot x=0 is the
// rim, x=1 and x=2 are level cells 0 and 1:
//
//        x=0  x=1  x=2
//   y=0   R    .    #      R: rim rock
static AutoMapLevelSnapshot RimLevel(void)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 3;
  snapshot.height = 1;
  snapshot.boundaryMarginX = 1;
  snapshot.cells.assign(3, AutoMapCellRecord());
  snapshot.cells[0].isOutsideLevel = true;
  snapshot.cells[1].kind = AutoMapCellFloor;
  return snapshot;
}

// rimPainted: the 3D view painted the rim cell, which explored it
static bool RimIsShown(AutoMapSpoilerLevel spoilers, bool exploreFloor, bool exploreStone,
                       bool rimPainted = false)
{
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(1, AutoMapLevelSize{2, 1}));
  if (exploreFloor) explored.Mark(AutoMapCell{LEVEL, 0, 0});
  if (exploreStone) explored.Mark(AutoMapCell{LEVEL, 1, 0});
  if (rimPainted) explored.Mark(AutoMapCell{LEVEL, -1, 0});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(
      RimLevel(), Fog(true, spoilers, explored, visibleNow, &knowledge));
  return FindCell(drawList, 0, 0) != NULL;
}

// At All the rim shows beside explored floor; explored stone beside it (the floor stays
// unexplored) is no reason to show it
static void RimRockShowsBesideExploredFloorAtAll(void)
{
  CHECK(RimIsShown(AutoMapSpoilersAll, true, false));
  CHECK(!RimIsShown(AutoMapSpoilersAll, false, false));
  CHECK(!RimIsShown(AutoMapSpoilersAll, false, true));
}

// Below All the rim shows once the 3D view painted it, whatever is explored beside it: the floor
// beside it may be explored while the rim never was on screen, two tiles to the side of the
// party, which the 3D view never draws (captured: party on 0,1,4 facing south, the floor 0,0,5
// explored, the rim 0,-1,5 not shown)
static void RimRockShowsOnlyOnceTheViewPaintedIt(void)
{
  CHECK(!RimIsShown(AutoMapSpoilersLearned, true, false));
  CHECK(!RimIsShown(AutoMapSpoilersSeen, true, false));
  CHECK(RimIsShown(AutoMapSpoilersSeen, true, false, true));
  CHECK(RimIsShown(AutoMapSpoilersLearned, false, false, true));
}

// A shut false wall hides the rim behind it from the 3D view, so below All the rim, shown only
// once painted, cannot give the wall away; at All it shows beside it
static bool RimBesideFalseWallIsShown(AutoMapSpoilerLevel spoilers, bool knowsIt)
{
  AutoMapLevelSnapshot snapshot = RimLevel();
  snapshot.cells[1].kind = AutoMapCellFalseWall;
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(1, AutoMapLevelSize{2, 1}));
  explored.Mark(AutoMapCell{LEVEL, 0, 0});
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  if (knowsIt) Learn(&knowledge, AutoMapFactFalseWall, 0, 0);
  AutoMapDrawList drawList = AutoMapScene_BuildDrawList(
      snapshot, Fog(true, spoilers, explored, visibleNow, &knowledge));
  return FindCell(drawList, 0, 0) != NULL;
}

static void RimRockKeepsAnUnknownFalseWallSecret(void)
{
  CHECK(!RimBesideFalseWallIsShown(AutoMapSpoilersLearned, false));
  CHECK(!RimBesideFalseWallIsShown(AutoMapSpoilersLearned, true));
  CHECK(RimBesideFalseWallIsShown(AutoMapSpoilersAll, false));
}

// A decoration on the rim is kept like one on the level's own walls: the face the 3D view showed
static void ADecorationOnTheRimShowsOnceItsFaceIsSeen(void)
{
  AutoMapLevelSnapshot snapshot = RimLevel();
  snapshot.cells[0].kind = AutoMapCellWall;
  snapshot.cells[0].wallSides[AutoMapEast].picture = 7;
  AutoMapExplored explored;
  explored.Reset(std::vector<AutoMapLevelSize>(1, AutoMapLevelSize{2, 1}));
  explored.Mark(AutoMapCell{LEVEL, 0, 0});
  explored.Mark(AutoMapCell{LEVEL, -1, 0});
  AutoMapVisibleCells visibleNow;
  AutoMapDrawList unseen = AutoMapScene_BuildDrawList(
      snapshot, Fog(true, AutoMapSpoilersSeen, explored, visibleNow, NULL));
  CHECK(FindCell(unseen, 0, 0) != NULL);
  CHECK(FindCell(unseen, 0, 0)->cell.wallSides[AutoMapEast].picture != 7);
  explored.MarkFace(AutoMapCell{LEVEL, -1, 0}, AutoMapEast);
  AutoMapDrawList seen = AutoMapScene_BuildDrawList(
      snapshot, Fog(true, AutoMapSpoilersSeen, explored, visibleNow, NULL));
  CHECK(FindCell(seen, 0, 0)->cell.wallSides[AutoMapEast].picture == 7);
}

// A visible spinner switched off out of sight: the party last saw its field
static void RememberedVisibleSpinnerKeepsItsField(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapCellRecord &spinner = At(&snapshot, 2, 1);
  spinner.kind = AutoMapCellSpinner;
  spinner.spinnerRotation = 1;
  spinner.spinnerIsVisible = true;
  AutoMapCellMemory memory;
  RememberLook(&memory, 2, 1, AutoMapScene_LookOf(spinner));
  spinner.kind = AutoMapCellDormantSpinner;
  spinner.spinnerIsVisible = false;
  AutoMapDrawList drawList = BuildWithLooks(snapshot, true, AutoMapSpoilersLearned, &memory);
  CHECK(FindCell(drawList, 2, 1)->cell.kind == AutoMapCellTeleporter);
  CHECK(FindCell(drawList, 2, 1)->cell.spinnerRotation == -1);
  AutoMapDrawList all = BuildWithLooks(snapshot, true, AutoMapSpoilersAll, &memory);
  CHECK(FindCell(all, 2, 1)->cell.kind == AutoMapCellDormantSpinner);
}

static void MemoryOfAnotherLevelApplies(void)
{
  AutoMapLevelSnapshot snapshot = WithLiveOpenDoor();
  snapshot.partyIsOnShownLevel = false;
  AutoMapCellLook look = AutoMapScene_LookOf(At(&snapshot, 2, 1));
  look.doorState = AutoMapDoorClosed;
  AutoMapCellMemory memory;
  RememberLook(&memory, 2, 1, look);
  AutoMapDrawList drawList = BuildWithLooks(snapshot, true, AutoMapSpoilersLearned, &memory);
  CHECK(FindCell(drawList, 2, 1)->cell.doorState == AutoMapDoorClosed);
}

// The player's notes: Build with the fog on and the notes store in the fog
static AutoMapDrawList BuildWithNotes(const AutoMapLevelSnapshot &snapshot,
                                      AutoMapSpoilerLevel spoilers, const AutoMapExplored &explored,
                                      const AutoMapPlayerNotes *notes)
{
  AutoMapVisibleCells visibleNow;
  AutoMapKnowledge knowledge;
  AutoMapFog fog = Fog(true, spoilers, explored, visibleNow, &knowledge);
  fog.notes = notes;
  return AutoMapScene_BuildDrawList(snapshot, fog);
}

static void PlayerPitRidesOnTheShownCell(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapPlayerNotes notes;
  notes.SetPit(AutoMapCell{LEVEL, 1, 1}, true);
  notes.SetPit(AutoMapCell{LEVEL, 2, 1}, false);
  notes.SetPit(AutoMapCell{LEVEL, 3, 1}, true);
  AutoMapSpoilerLevel levels[2] = {AutoMapSpoilersLearned, AutoMapSpoilersAll};
  for (int i = 0; i < 2; i++)
  {
    AutoMapDrawList drawList = BuildWithNotes(snapshot, levels[i], explored, &notes);
    CHECK(FindCell(drawList, 1, 1)->playerPit == 1);
    CHECK(FindCell(drawList, 2, 1)->playerPit == 0);
    // The mark never changes what the cell is
    CHECK(FindCell(drawList, 1, 1)->cell.kind == AutoMapCellFloor);
    // An unexplored cell is not shown, so its mark is not either
    CHECK(FindCell(drawList, 3, 1) == NULL);
  }
}

static void NoNotesWithoutTheStore(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapDrawList drawList = BuildWithNotes(snapshot, AutoMapSpoilersLearned, explored, NULL);
  CHECK(FindCell(drawList, 1, 1)->playerPit == -1);
  CHECK(drawList.playerLinks.empty());
  CHECK(AutoMapScene_PlayerNoteLines(drawList, 1, 1).empty());
}

static void PlayerLinksNeedBothEndsShown(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapPlayerNotes notes;
  AutoMapLinkSource wall = {{LEVEL, 1, 1}, AutoMapNorth};
  AutoMapLinkSource floor = {{LEVEL, 1, 1}, -1};
  notes.ToggleLinkTarget(wall, AutoMapCell{LEVEL, 2, 1});
  notes.ToggleLinkTarget(floor, AutoMapCell{LEVEL, 2, 1});
  AutoMapDrawList drawList = BuildWithNotes(snapshot, AutoMapSpoilersLearned, explored, &notes);
  CHECK(drawList.playerLinks.empty());
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  drawList = BuildWithNotes(snapshot, AutoMapSpoilersLearned, explored, &notes);
  CHECK(drawList.playerLinks.size() == 2);
  for (size_t i = 0; i < drawList.playerLinks.size(); i++)
  {
    const AutoMapLink &link = drawList.playerLinks[i];
    CHECK(link.srcX == 1 && link.srcY == 1 && link.dstX == 2 && link.dstY == 1);
    CHECK(link.source == AUTOMAP_NO_RECORD);
    CHECK(link.srcIsWall == (link.srcSide >= 0));
  }
}

static bool SameLinks(const std::vector<AutoMapLink> &a, const std::vector<AutoMapLink> &b)
{
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
  {
    if (a[i].srcX != b[i].srcX || a[i].srcY != b[i].srcY || a[i].dstX != b[i].dstX
        || a[i].dstY != b[i].dstY || a[i].srcIsWall != b[i].srcIsWall
        || a[i].srcSide != b[i].srcSide || a[i].source != b[i].source)
    {
      return false;
    }
  }
  return true;
}

static bool SameCells(const std::vector<AutoMapCell> &a, const std::vector<AutoMapCell> &b)
{
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
  {
    if (a[i].level != b[i].level || a[i].x != b[i].x || a[i].y != b[i].y) return false;
  }
  return true;
}

// BuildWithNotes with the knowledge given
static AutoMapDrawList BuildWithNotesKnowing(const AutoMapLevelSnapshot &snapshot,
                                             AutoMapSpoilerLevel spoilers,
                                             const AutoMapExplored &explored,
                                             const AutoMapKnowledge *knowledge,
                                             const AutoMapPlayerNotes *notes)
{
  AutoMapVisibleCells visibleNow;
  AutoMapFog fog = Fog(true, spoilers, explored, visibleNow, knowledge);
  fog.notes = notes;
  return AutoMapScene_BuildDrawList(snapshot, fog);
}

// The player draws a link on the very edge the map has learned (the lever (1,0) south to the
// door (2,1)): the learned links, badges and press block are the same with it as without, at
// Learned and at All
static void PlayerLinksAreNotLearnedLinks(void)
{
  AutoMapLevelSnapshot snapshot = LeverCounterDoor();
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 0});
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  explored.Mark(AutoMapCell{LEVEL, 3, 0});
  explored.MarkFace(AutoMapCell{LEVEL, 1, 0}, AutoMapSouth);
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, SWITCH_RECORD));
  knowledge.Learn(AutoMapFact_OfLink(SWITCH_RECORD, AutoMapCell{LEVEL, 2, 1}));
  AutoMapPlayerNotes notes;
  AutoMapLinkSource lever = {{LEVEL, 1, 0}, AutoMapSouth};
  notes.ToggleLinkTarget(lever, AutoMapCell{LEVEL, 2, 1});
  AutoMapSpoilerLevel levels[2] = {AutoMapSpoilersLearned, AutoMapSpoilersAll};
  for (int i = 0; i < 2; i++)
  {
    bool atAll = levels[i] == AutoMapSpoilersAll;
    AutoMapDrawList without =
        BuildWithNotesKnowing(snapshot, levels[i], explored, &knowledge, NULL);
    AutoMapDrawList with = BuildWithNotesKnowing(snapshot, levels[i], explored, &knowledge, &notes);
    CHECK(without.playerLinks.empty());
    CHECK(with.playerLinks.size() == 1);
    // Something to compare: the learned edge (the whole chain at All) is drawn
    CHECK(!without.links.empty());
    CHECK(!without.linkedCells.empty());
    CHECK(SameLinks(with.links, without.links));
    CHECK(SameCells(with.linkedCells, without.linkedCells));
    std::vector<AutoMapPressSource> pressWithout = AutoMapScene_PressEffects(without, 1, 0);
    std::vector<AutoMapPressSource> pressWith = AutoMapScene_PressEffects(with, 1, 0);
    // A press source at All, none below
    CHECK(pressWithout.empty() == !atAll);
    CHECK(pressWith.size() == pressWithout.size());
    for (size_t s = 0; s < pressWith.size() && s < pressWithout.size(); s++)
    {
      CHECK(pressWith[s].source == pressWithout[s].source);
      CHECK(pressWith[s].srcIsWall == pressWithout[s].srcIsWall);
      CHECK(pressWith[s].srcSide == pressWithout[s].srcSide);
      CHECK(pressWith[s].effects.size() == pressWithout[s].effects.size());
    }
    for (int full = 0; full < 2; full++)
    {
      CHECK(AutoMapScene_PressLines(with, pressWith, full != 0)
            == AutoMapScene_PressLines(without, pressWithout, full != 0));
    }
    CHECK(AutoMapScene_PressLines(without, pressWithout, true).empty() == !atAll);
  }
}

// The Corridor grown by a rim of added rock, 1 west and 2 north (and 1 east and south): snapshot
// cell (x + 1, y + 2) is level cell (x, y). The level's global offset is (10, 20).
static AutoMapLevelSnapshot CorridorInARim(void)
{
  AutoMapLevelSnapshot corridor = Corridor();
  AutoMapLevelSnapshot grown;
  grown.level = LEVEL;
  grown.width = 6;
  grown.height = 6;
  grown.boundaryMarginX = 1;
  grown.boundaryMarginY = 2;
  grown.offsetX = 10 - 1;
  grown.offsetY = 20 - 2;
  grown.cells.assign(36, AutoMapCellRecord());
  for (int y = 0; y < grown.height; y++)
  {
    for (int x = 0; x < grown.width; x++)
    {
      bool inside = x >= 1 && x < 1 + corridor.width && y >= 2 && y < 2 + corridor.height;
      if (inside) At(&grown, x, y) = At(&corridor, x - 1, y - 2);
      else At(&grown, x, y).isOutsideLevel = true;
    }
  }
  grown.party.x = corridor.party.x + 1;
  grown.party.y = corridor.party.y + 2;
  grown.party.facing = corridor.party.facing;
  return grown;
}

// A hit lists the level's own cell, as the rulers number it: the rim of added rock moves the
// snapshot's cells but not the hit's global position
static void ASearchHitInARimNamesTheLevelsOwnCell(void)
{
  AutoMapLevelSnapshot snapshot = CorridorInARim();
  AutoMapAlcoveEntry chest;
  chest.item = ItemOf(CHEST, 1);
  chest.side = AutoMapSouth;
  At(&snapshot, 1 + 1, 0 + 2).alcoveContents.push_back(chest);
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  std::vector<AutoMapSearchHit> hits = AutoMapScene_SearchItems(all, "chest", FakeName, NULL);
  CHECK(hits.size() == 1);
  if (hits.size() != 1) return;
  CHECK(hits[0].x == 2 && hits[0].y == 2);
  CHECK(hits[0].globalX == 10 + 1 && hits[0].globalY == 20 + 0);
}

static void RimRockTakesNoNotes(void)
{
  AutoMapLevelSnapshot snapshot = CorridorInARim();
  // Fog off: the rim is shown, and still takes no note
  AutoMapDrawList all = Build(snapshot, AutoMapSpoilersAll, NULL);
  CHECK(FindCell(all, 0, 0) != NULL);
  CHECK(!AutoMapScene_CellTakesNotes(all, 0, 0));
  CHECK(!AutoMapScene_CellTakesNotes(all, 0, 3));
  CHECK(!AutoMapScene_CellTakesNotes(all, 5, 5));
  CHECK(!AutoMapScene_CellTakesNotes(all, 2, 1));
  // The level's own cells take them, walls as well as floors
  CHECK(AutoMapScene_CellTakesNotes(all, 2, 3));
  CHECK(AutoMapScene_CellTakesNotes(all, 1, 2));
  // Outside the list altogether
  CHECK(!AutoMapScene_CellTakesNotes(all, -1, 0));
  CHECK(!AutoMapScene_CellTakesNotes(all, 6, 0));
  // Under the fog an unexplored cell is not shown, so it takes none either
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  AutoMapDrawList fogged = BuildWithNotes(snapshot, AutoMapSpoilersLearned, explored, NULL);
  CHECK(AutoMapScene_CellTakesNotes(fogged, 2, 3));
  CHECK(FindCell(fogged, 4, 4) == NULL);
  CHECK(!AutoMapScene_CellTakesNotes(fogged, 4, 4));
}

// Notes are kept under level cells; the draw list shows them shifted by the margins
static void NotesFollowTheBoundaryMargin(void)
{
  AutoMapLevelSnapshot snapshot = CorridorInARim();
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapPlayerNotes notes;
  notes.SetPit(AutoMapCell{LEVEL, 1, 1}, true);
  AutoMapLinkSource floor = {{LEVEL, 1, 1}, -1};
  notes.ToggleLinkTarget(floor, AutoMapCell{LEVEL, 2, 1});
  AutoMapDrawList drawList = BuildWithNotes(snapshot, AutoMapSpoilersLearned, explored, &notes);
  CHECK(FindCell(drawList, 2, 3) != NULL && FindCell(drawList, 2, 3)->playerPit == 1);
  CHECK(FindCell(drawList, 3, 3) != NULL && FindCell(drawList, 3, 3)->playerPit == -1);
  CHECK(FindCell(drawList, 1, 1) == NULL || FindCell(drawList, 1, 1)->playerPit == -1);
  CHECK(drawList.playerLinks.size() == 1);
  if (drawList.playerLinks.size() == 1)
  {
    const AutoMapLink &link = drawList.playerLinks[0];
    CHECK(link.srcX == 2 && link.srcY == 3 && link.dstX == 3 && link.dstY == 3);
  }
  std::vector<std::string> lines = AutoMapScene_PlayerNoteLines(drawList, 2, 3);
  CHECK(lines.size() == 2);
  CHECK(HasLine(lines, "YOUR MARK: HIDDEN PIT OPEN"));
  CHECK(HasLine(lines, "YOUR LINK: L0 (12, 21)"));
}

static void PlayerNoteLinesWording(void)
{
  AutoMapLevelSnapshot snapshot = Corridor();
  snapshot.offsetX = 10;
  snapshot.offsetY = 20;
  AutoMapExplored explored = Explored();
  explored.Mark(AutoMapCell{LEVEL, 1, 1});
  explored.Mark(AutoMapCell{LEVEL, 2, 1});
  AutoMapPlayerNotes notes;
  notes.SetPit(AutoMapCell{LEVEL, 1, 1}, true);
  notes.SetPit(AutoMapCell{LEVEL, 2, 1}, false);
  AutoMapLinkSource floor = {{LEVEL, 1, 1}, -1};
  notes.ToggleLinkTarget(floor, AutoMapCell{LEVEL, 2, 1});
  AutoMapDrawList drawList = BuildWithNotes(snapshot, AutoMapSpoilersLearned, explored, &notes);
  std::vector<std::string> open = AutoMapScene_PlayerNoteLines(drawList, 1, 1);
  CHECK(open.size() == 2);
  CHECK(HasLine(open, "YOUR MARK: HIDDEN PIT OPEN"));
  CHECK(HasLine(open, "YOUR LINK: L0 (12, 21)"));
  std::vector<std::string> closed = AutoMapScene_PlayerNoteLines(drawList, 2, 1);
  CHECK(closed.size() == 1);
  CHECK(HasLine(closed, "YOUR MARK: HIDDEN PIT CLOSED"));
  CHECK(AutoMapScene_PlayerNoteLines(drawList, 3, 1).empty());
}

void TestAutoMapSpoilers(void)
{
  AFloorPictureShowsOnceItWasNoticed();
  APortraitShowsItsFaceOnceSeenUpClose();
  AnAlcoveShowsOnlyTheItemsNoticedInIt();
  StairsShowOnceTheyWereNoticed();
  BelowAllNoHitPointsAreShown();
  BelowAllEachHitMemberShowsItsDamage();
  AtAllHitPointsShowAndDamageDoesNot();
  NoDamageStoreShowsNoDamage();
  AnUnseenFountainIsNotFound();
  MeasuredMonsterIsShown();
  UnmeasuredMonsterInTheRayIsNotShown();
  MeasuredMonsterOutsideTheRayIsShown();
  AllKeepsTheRay();
  NoMonsterSeenWithoutAFrame();
  AllKeepsTodaysMap();
  AllNeedsNoKnowledge();
  UnseenFaceHidesItsDecorationUnderFog();
  AGlimpsedFaceShowsAQuestionMarkOnAnUnexploredWall();
  WallTextShowsOnlyOnceRead();
  FogOffShowsEveryFace();
  UnusedSwitchLooksLikeDressingAndAUsedOneWorks();
  SeenIgnoresKnowledge();
  LockRequirementOnlyAfterAnItemOpenedIt();
  DoorLockShowsUnderFogOnceTheCellInFrontIsExplored();
  PortraitGoodsShowOnceThePortraitWasViewed();
  LauncherAndHiddenFlagShowOnlyOnceTheLauncherFired();
  RelayMachineryNeverShowsBelowAll();
  BreakableDoorLooksLikeAPlainDoor();
  FalseWallIsAWallUntilKnown();
  HiddenPitIsFloorUntilKnown();
  InvisibleTeleporterIsFloorUntilUsed();
  VisibleTeleporterNamesNoDestinationUntilUsed();
  SpinnerIsFloorUntilUsedAndKeepsItsTurnAfterwards();
  VisibleSpinnerLooksLikeATeleporterUntilUsed();
  DormantTeleporterNeverShowsBelowAll();
  MarginKeepsFacesAndFactsAligned();
  PlacementWordingNamesNoFalseWall();
  HiddenPadAppearsOnceKnownWithoutItsCondition();
  LearnedPadStaysWhenThePartyLooksAway();
  VisiblePressurePlateKeepsItsFloorPicture();
  CountersAndDormantPadsNeverShowBelowAll();
  KnownSpentGeneratorShowsAsAGenerator();
  KnownLauncherHasNoLaunchesLineBelowAll();
  KnownLinkSkipsAHiddenCounter();
  NoLinkWithoutAFact();
  LinkOfADeletedActuatorDrawsNothing();
  LinkTargetFollowsTheBoundaryMargin();
  ChestContentsOnlyOnceOpened();
  ScrollTextOnlyOnceRead();
  MonsterCarriesNothingBelowAll();
  SearchFindsNoHiddenItems();
  ChestInAnAlcoveShowsItsContentsOnlyOnceOpened();
  WholeLevelShowsWallGoodsOnlyOnAnAlcove();
  ASearchHitInARimNamesTheLevelsOwnCell();
  UnseenLiveItemIsNotShown();
  RememberedItemShowsWhereItWasSeen();
  AGlimpseOnAnUnexploredCellShowsOnlyAQuestionMark();
  AGlimpseOnAnExploredCellShowsNoPicture();
  AllShowsTheItemNotAGlimpse();
  RememberedItemGoneFromThePileStillShows();
  AllAndFogOffShowTheLivePile();
  UnopenedChestStillHidesWhatItHolds();
  RememberedItemsShowOnAnotherLevel();
  ExploredItemsAreRememberedFromAnOldMap();
  LiveEntryWithAnotherDescriptionIsNotUsed();
  UnknownDescriptionIsLeftOut();
  ChestContentsFollowTheirChestToTheNewPosition();
  ScrollTextsStayParallelToTheRememberedScrolls();
  RememberedPileKeepsItsOrderAndTruncates();
  RememberedDoorDrawsAsSeen();
  RememberedLookKeepsTheSecretRules();
  UnknownConcealedPitRememberedOpenIsFloor();
  RememberedSideOverridesOnlyThatSide();
  UnrememberedCellDrawsLive();
  RememberedDestroyedDoorDrawsWithoutLiveRemnants();
  RememberExploredLooksTakesOnlyNewExploredCells();
  MemoryOfAnotherLevelApplies();
  RememberedDormantSpinnerHidesTheLiveField();
  RememberedVisibleSpinnerKeepsItsField();
  RimRockShowsBesideExploredFloorAtAll();
  ADecorationOnTheRimShowsOnceItsFaceIsSeen();
  RimRockKeepsAnUnknownFalseWallSecret();
  RimRockShowsOnlyOnceTheViewPaintedIt();
  PlayerPitRidesOnTheShownCell();
  NoNotesWithoutTheStore();
  PlayerLinksNeedBothEndsShown();
  PlayerLinksAreNotLearnedLinks();
  PlayerNoteLinesWording();
  RimRockTakesNoNotes();
  NotesFollowTheBoundaryMargin();
}
