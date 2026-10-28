#include "TestSupport.h"
#include "../AutoMapCellMemory.h"

static AutoMapCellLook DoorLook(AutoMapDoorState state)
{
  AutoMapCellLook look = AutoMapScene_LookOf(AutoMapCellRecord());
  look.kind = AutoMapCellDoor;
  look.doorState = state;
  return look;
}

static AutoMapCellLook WallLook(int side1Picture)
{
  AutoMapCellLook look = AutoMapScene_LookOf(AutoMapCellRecord());
  look.kind = AutoMapCellWall;
  look.sidePictures[1] = side1Picture;
  return look;
}

static void SeenCellIsRemembered(void)
{
  AutoMapCellMemory memory;
  AutoMapCellLook found;
  CHECK(!memory.LookOf(AutoMapCell{3, 4, 5}, &found));
  memory.See(AutoMapCell{3, 4, 5}, DoorLook(AutoMapDoorClosed));
  CHECK(memory.LookOf(AutoMapCell{3, 4, 5}, &found));
  CHECK(found.kind == AutoMapCellDoor);
  CHECK(found.doorState == AutoMapDoorClosed);
}

static void SeeKeepsSidesOfAKnownCell(void)
{
  AutoMapCellMemory memory;
  AutoMapCell cell = {3, 4, 5};
  AutoMapCellLook found;
  memory.See(cell, WallLook(148));
  memory.See(cell, WallLook(149));
  CHECK(memory.LookOf(cell, &found));
  CHECK(found.sidePictures[1] == 148);
  memory.SeeSide(cell, 1, 149);
  CHECK(memory.LookOf(cell, &found));
  CHECK(found.sidePictures[1] == 149);
  AutoMapCell other = {3, 9, 9};
  memory.SeeSide(other, 1, 149);
  CHECK(!memory.LookOf(other, &found));
}

static void SeeRefreshesTheCellFields(void)
{
  AutoMapCellMemory memory;
  AutoMapCell cell = {3, 4, 5};
  AutoMapCellLook found;
  memory.See(cell, DoorLook(AutoMapDoorClosed));
  memory.See(cell, DoorLook(AutoMapDoorOpen));
  CHECK(memory.LookOf(cell, &found));
  CHECK(found.doorState == AutoMapDoorOpen);
}

static void RememberIfNewKeepsTheFirstImpression(void)
{
  AutoMapCellMemory memory;
  AutoMapCell cell = {3, 4, 5};
  AutoMapCellLook found;
  memory.RememberIfNew(cell, DoorLook(AutoMapDoorClosed));
  memory.RememberIfNew(cell, DoorLook(AutoMapDoorOpen));
  CHECK(memory.LookOf(cell, &found));
  CHECK(found.doorState == AutoMapDoorClosed);
  memory.Restore(cell, DoorLook(AutoMapDoorOpen));
  CHECK(memory.LookOf(cell, &found));
  CHECK(found.doorState == AutoMapDoorOpen);
}

static void ForgetEmptiesIt(void)
{
  AutoMapCellMemory memory;
  memory.See(AutoMapCell{3, 4, 5}, DoorLook(AutoMapDoorClosed));
  memory.Forget();
  AutoMapCellLook found;
  CHECK(!memory.LookOf(AutoMapCell{3, 4, 5}, &found));
  CHECK(memory.Looks().empty());
}

static void ForgetLevelKeepsTheOtherLevels(void)
{
  AutoMapCellMemory memory;
  memory.See(AutoMapCell{1, 4, 5}, DoorLook(AutoMapDoorClosed));
  memory.See(AutoMapCell{2, 4, 5}, DoorLook(AutoMapDoorClosed));
  memory.See(AutoMapCell{3, 0, 0}, DoorLook(AutoMapDoorClosed));
  memory.ForgetLevel(2);
  AutoMapCellLook found;
  CHECK(memory.LookOf(AutoMapCell{1, 4, 5}, &found));
  CHECK(!memory.LookOf(AutoMapCell{2, 4, 5}, &found));
  CHECK(memory.LookOf(AutoMapCell{3, 0, 0}, &found));
  CHECK(memory.Looks().size() == 2);
}

static void LooksAreOrdered(void)
{
  AutoMapCellMemory memory;
  memory.See(AutoMapCell{2, 1, 1}, DoorLook(AutoMapDoorClosed));
  memory.See(AutoMapCell{1, 5, 2}, DoorLook(AutoMapDoorClosed));
  memory.See(AutoMapCell{1, 3, 2}, DoorLook(AutoMapDoorClosed));
  memory.See(AutoMapCell{1, 7, 0}, DoorLook(AutoMapDoorClosed));
  std::vector<std::pair<AutoMapCell, AutoMapCellLook> > looks = memory.Looks();
  CHECK(looks.size() == 4);
  CHECK(looks[0].first.level == 1 && looks[0].first.y == 0 && looks[0].first.x == 7);
  CHECK(looks[1].first.level == 1 && looks[1].first.y == 2 && looks[1].first.x == 3);
  CHECK(looks[2].first.level == 1 && looks[2].first.y == 2 && looks[2].first.x == 5);
  CHECK(looks[3].first.level == 2);
}

void TestAutoMapCellMemory(void)
{
  SeenCellIsRemembered();
  SeeKeepsSidesOfAKnownCell();
  SeeRefreshesTheCellFields();
  RememberIfNewKeepsTheFirstImpression();
  ForgetEmptiesIt();
  ForgetLevelKeepsTheOtherLevels();
  LooksAreOrdered();
}
