#include "TestSupport.h"
#include "../AutoMapCompletion.h"

// Two rooms of 2x3 cells, split by the middle column: a wall, or a door when withDoor
static AutoMapReachGrid TwoRooms(bool withDoor)
{
  AutoMapReachGrid grid;
  grid.width = 5;
  grid.height = 3;
  grid.cells.assign(15, AutoMapReachWalkable);
  for (int y = 0; y < 3; y++) grid.cells[y * 5 + 2] = AutoMapReachSolid;
  if (withDoor) grid.cells[1 * 5 + 2] = AutoMapReachWalkable;
  return grid;
}

static int CountReached(const std::vector<bool> &mask)
{
  int count = 0;
  for (size_t i = 0; i < mask.size(); i++) count += mask[i] ? 1 : 0;
  return count;
}

static void ASeedReachesItsOwnRoomOnly(void)
{
  std::vector<std::pair<int, int> > seeds(1, std::make_pair(0, 0));
  std::vector<bool> reached = AutoMapCompletion_Reachable(TwoRooms(false), seeds);
  CHECK(reached.size() == 15);
  CHECK(CountReached(reached) == 6);
  CHECK(reached[0 * 5 + 1]);
  CHECK(reached[2 * 5 + 0]);
  CHECK(!reached[0 * 5 + 3]);
  CHECK(!reached[1 * 5 + 2]);
}

static void ADoorJoinsTheRooms(void)
{
  std::vector<std::pair<int, int> > seeds(1, std::make_pair(0, 0));
  std::vector<bool> reached = AutoMapCompletion_Reachable(TwoRooms(true), seeds);
  CHECK(CountReached(reached) == 13);
  CHECK(reached[1 * 5 + 2]);
  CHECK(reached[2 * 5 + 4]);
}

static void SeedsOnRockOrOutsideReachNothing(void)
{
  std::vector<std::pair<int, int> > seeds;
  seeds.push_back(std::make_pair(2, 0));
  seeds.push_back(std::make_pair(-1, 0));
  seeds.push_back(std::make_pair(5, 1));
  seeds.push_back(std::make_pair(0, 3));
  CHECK(CountReached(AutoMapCompletion_Reachable(TwoRooms(false), seeds)) == 0);
}

static void ASeedInEachRoomReachesBoth(void)
{
  std::vector<std::pair<int, int> > seeds;
  seeds.push_back(std::make_pair(0, 0));
  seeds.push_back(std::make_pair(4, 2));
  CHECK(CountReached(AutoMapCompletion_Reachable(TwoRooms(false), seeds)) == 12);
}

// A teleporter's target as a seed of the level read: a target on another level names that level's
// own cell, so the read level's margins move it; one on the same level the snapshot already moved
static void TeleporterTargetsMoveWithTheMarginsOnce(void)
{
  std::pair<int, int> fromElsewhere = AutoMapCompletion_TargetSeed(3, 5, 7, 2, 3, 1, 1);
  CHECK(fromElsewhere.first == 6 && fromElsewhere.second == 8);
  std::pair<int, int> fromHere = AutoMapCompletion_TargetSeed(3, 5, 7, 3, 3, 1, 1);
  CHECK(fromHere.first == 5 && fromHere.second == 7);
}

void TestAutoMapCompletion(void)
{
  TeleporterTargetsMoveWithTheMarginsOnce();
  ASeedReachesItsOwnRoomOnly();
  ADoorJoinsTheRooms();
  SeedsOnRockOrOutsideReachNothing();
  ASeedInEachRoomReachesBoth();
}
