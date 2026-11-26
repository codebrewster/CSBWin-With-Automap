#include "TestSupport.h"
#include "../AutoMapItemMemory.h"

static AutoMapRememberedItem Club(int x, int y)
{
  AutoMapRememberedItem item = {0x00050012u, 27, {3, x, y}};
  return item;
}

static void SeenItemIsRemembered(void)
{
  AutoMapItemMemory memory;
  memory.See(Club(4, 5));
  CHECK(memory.Remembers(Club(4, 5)));
  CHECK(memory.ItemsOn(AutoMapCell{3, 4, 5}).size() == 1);
}

static void SeenElsewhereMovesIt(void)
{
  AutoMapItemMemory memory;
  memory.See(Club(4, 5));
  memory.See(Club(6, 5));
  CHECK(memory.ItemsOn(AutoMapCell{3, 4, 5}).empty());
  CHECK(memory.Remembers(Club(6, 5)));
  CHECK(memory.Items().size() == 1);
}

static void GoneFromASeenCellIsForgotten(void)
{
  AutoMapItemMemory memory;
  memory.See(Club(4, 5));
  memory.KeepOnlyItemsStillOn(AutoMapCell{3, 4, 5}, std::vector<AutoMapFloorItemTag>());
  CHECK(memory.Items().empty());
}

static void StillThereIsKept(void)
{
  AutoMapItemMemory memory;
  memory.See(Club(4, 5));
  AutoMapFloorItemTag there = {0x00050012u, 27};
  memory.KeepOnlyItemsStillOn(AutoMapCell{3, 4, 5}, std::vector<AutoMapFloorItemTag>(1, there));
  CHECK(memory.Remembers(Club(4, 5)));
  // Another cell seen empty leaves it alone
  memory.KeepOnlyItemsStillOn(AutoMapCell{3, 6, 5}, std::vector<AutoMapFloorItemTag>());
  CHECK(memory.Remembers(Club(4, 5)));
}

static void ReusedRecordIsForgotten(void)
{
  AutoMapItemMemory memory;
  memory.See(Club(4, 5));
  AutoMapFloorItemTag apple = {0x00050012u, 120};
  memory.KeepOnlyItemsStillOn(AutoMapCell{3, 4, 5}, std::vector<AutoMapFloorItemTag>(1, apple));
  CHECK(memory.Items().empty());
}

static void PickedUpIsForgottenAndOrderIsKept(void)
{
  AutoMapItemMemory memory;
  AutoMapRememberedItem dagger = {0x00050020u, 8, {3, 4, 5}};
  memory.See(Club(4, 5));
  memory.See(dagger);
  CHECK(memory.ItemsOn(AutoMapCell{3, 4, 5})[0].record == 0x00050012u);
  memory.ForgetItem(0x00050012u);
  CHECK(memory.ItemsOn(AutoMapCell{3, 4, 5}).size() == 1);
  memory.ForgetLevel(3);
  CHECK(memory.Items().empty());
}

// Something seen in the dark, on a cell the light does not reach: the map shows a "?"
static AutoMapRememberedItem Glimpsed(AutoMapRememberedItem item)
{
  item.isGlimpsed = true;
  return item;
}

static void AGlimpseIsRememberedButNotIdentified(void)
{
  AutoMapItemMemory memory;
  memory.See(Glimpsed(Club(4, 5)));
  CHECK(memory.Remembers(Club(4, 5)));
  CHECK(!memory.Identifies(Club(4, 5)));
  CHECK(memory.ItemsOn(AutoMapCell{3, 4, 5})[0].isGlimpsed);
}

static void SeenInTheLightAGlimpseIsIdentified(void)
{
  AutoMapItemMemory memory;
  memory.See(Glimpsed(Club(4, 5)));
  memory.See(Club(4, 5));
  CHECK(memory.Identifies(Club(4, 5)));
  CHECK(!memory.ItemsOn(AutoMapCell{3, 4, 5})[0].isGlimpsed);
}

// The party knows what lies there: a later glimpse in the dark does not make it a "?" again
static void AGlimpseLeavesAnIdentifiedItemIdentified(void)
{
  AutoMapItemMemory memory;
  memory.See(Club(4, 5));
  memory.See(Glimpsed(Club(4, 5)));
  CHECK(memory.Identifies(Club(4, 5)));
}

// Moved in the dark, nobody can tell it is the same item
static void AGlimpseElsewhereMovesItAsAGlimpse(void)
{
  AutoMapItemMemory memory;
  memory.See(Club(4, 5));
  memory.See(Glimpsed(Club(6, 5)));
  CHECK(memory.ItemsOn(AutoMapCell{3, 4, 5}).empty());
  CHECK(memory.ItemsOn(AutoMapCell{3, 6, 5})[0].isGlimpsed);
}

// Explored, live asks this of a live item wherever it lies now
static void AnItemSeenAnywhereHasBeenSeen(void)
{
  AutoMapItemMemory memory;
  AutoMapFloorItemTag club = {0x00050012u, 27};
  CHECK(!memory.HasSeen(club));
  memory.See(Club(4, 5));
  CHECK(memory.HasSeen(club));
  AutoMapFloorItemTag otherDescription = {0x00050012u, 8};
  CHECK(!memory.HasSeen(otherDescription));
  AutoMapFloorItemTag otherRecord = {0x00050013u, 27};
  CHECK(!memory.HasSeen(otherRecord));
}

static void AGlimpseHasNotBeenSeen(void)
{
  AutoMapItemMemory memory;
  AutoMapRememberedItem glimpse = Club(4, 5);
  glimpse.isGlimpsed = true;
  memory.See(glimpse);
  AutoMapFloorItemTag club = {0x00050012u, 27};
  CHECK(!memory.HasSeen(club));
}

// The items of a cell are found through an index: every change since the last question shows
static void EachCellAnswersAfterEveryChange(void)
{
  AutoMapItemMemory memory;
  AutoMapRememberedItem gem = {0x00060020u, 9, {3, 4, 5}};
  memory.See(Club(4, 5));
  memory.See(gem);
  std::vector<AutoMapRememberedItem> two = memory.ItemsOn(AutoMapCell{3, 4, 5});
  CHECK(two.size() == 2 && two[0].record == 0x00050012u && two[1].record == 0x00060020u);
  CHECK(memory.Identifies(gem) && memory.ItemsOn(AutoMapCell{3, 6, 5}).empty());
  // Moved: gone from the old cell, on the new one
  memory.See(Club(6, 5));
  CHECK(memory.ItemsOn(AutoMapCell{3, 4, 5}).size() == 1 && memory.Remembers(Club(6, 5)));
  CHECK(!memory.Remembers(Club(4, 5)));
  // Picked up
  memory.ForgetItem(0x00060020u);
  CHECK(memory.ItemsOn(AutoMapCell{3, 4, 5}).empty() && !memory.Identifies(gem));
  // Seen again on its cell, with the same description: kept in place
  AutoMapFloorItemTag there = {0x00050012u, 27};
  memory.KeepOnlyItemsStillOn(AutoMapCell{3, 6, 5}, std::vector<AutoMapFloorItemTag>(1, there));
  CHECK(memory.ItemsOn(AutoMapCell{3, 6, 5}).size() == 1);
  memory.KeepOnlyItemsStillOn(AutoMapCell{3, 6, 5}, std::vector<AutoMapFloorItemTag>());
  CHECK(memory.ItemsOn(AutoMapCell{3, 6, 5}).empty() && memory.Items().empty());
  // A level forgotten, and everything
  memory.See(Club(1, 1));
  memory.ForgetLevel(3);
  CHECK(memory.ItemsOn(AutoMapCell{3, 1, 1}).empty());
  memory.See(Club(2, 2));
  memory.Forget();
  CHECK(!memory.Remembers(Club(2, 2)));
}

void TestAutoMapItemMemory(void)
{
  EachCellAnswersAfterEveryChange();
  AGlimpseIsRememberedButNotIdentified();
  SeenInTheLightAGlimpseIsIdentified();
  AGlimpseLeavesAnIdentifiedItemIdentified();
  AGlimpseElsewhereMovesItAsAGlimpse();
  SeenItemIsRemembered();
  SeenElsewhereMovesIt();
  GoneFromASeenCellIsForgotten();
  StillThereIsKept();
  ReusedRecordIsForgotten();
  PickedUpIsForgottenAndOrderIsKept();
  AnItemSeenAnywhereHasBeenSeen();
  AGlimpseHasNotBeenSeen();
}
