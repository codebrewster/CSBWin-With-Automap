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

// The one level 0 of grid, filled from the seeds (x, y) on it
static std::vector<bool> ReachedOnOneLevel(const AutoMapReachGrid &grid, const std::vector<std::pair<int, int> > &seeds)
{
  std::map<int, AutoMapReachLevel> levels;
  levels[0].grid = grid;
  std::vector<AutoMapCell> cells;
  for (size_t i = 0; i < seeds.size(); i++) cells.push_back(AutoMapCell{0, seeds[i].first, seeds[i].second});
  return AutoMapCompletion_ReachableAcross(levels, cells)[0];
}

static void ASeedReachesItsOwnRoomOnly(void)
{
  std::vector<std::pair<int, int> > seeds(1, std::make_pair(0, 0));
  std::vector<bool> reached = ReachedOnOneLevel(TwoRooms(false), seeds);
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
  std::vector<bool> reached = ReachedOnOneLevel(TwoRooms(true), seeds);
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
  CHECK(CountReached(ReachedOnOneLevel(TwoRooms(false), seeds)) == 0);
}

static void ASeedInEachRoomReachesBoth(void)
{
  std::vector<std::pair<int, int> > seeds;
  seeds.push_back(std::make_pair(0, 0));
  seeds.push_back(std::make_pair(4, 2));
  CHECK(CountReached(ReachedOnOneLevel(TwoRooms(false), seeds)) == 12);
}

// Three levels of two rooms each; the party stood only in the west room of level 0
static std::map<int, AutoMapReachLevel> ThreeLevels(void)
{
  std::map<int, AutoMapReachLevel> levels;
  for (int level = 0; level < 3; level++) levels[level].grid = TwoRooms(false);
  return levels;
}

static std::vector<AutoMapCell> WestOfLevel0(void)
{
  return std::vector<AutoMapCell>(1, AutoMapCell{0, 0, 0});
}

// A pit in the west room drops into the east room of the level below; a pit there drops further:
// the rooms under the first pit count, even two levels down
static void PitsLeadDownSeveralLevels(void)
{
  std::map<int, AutoMapReachLevel> levels = ThreeLevels();
  levels[0].jumps.push_back(AutoMapReachJump{1 * 5 + 1, 1, 3, 1});
  levels[1].jumps.push_back(AutoMapReachJump{0 * 5 + 4, 2, 4, 0});
  std::map<int, std::vector<bool> > reached = AutoMapCompletion_ReachableAcross(levels, WestOfLevel0());
  CHECK(CountReached(reached[0]) == 6);
  CHECK(CountReached(reached[1]) == 6);
  CHECK(reached[1][1 * 5 + 3] && !reached[1][0]);
  CHECK(CountReached(reached[2]) == 6);
  CHECK(reached[2][0 * 5 + 4] && !reached[2][0]);
}

// A pit, stairs or teleporter in a room the party cannot get to leads nowhere it can be
static void AJumpFromAClosedOffRoomCountsNothing(void)
{
  std::map<int, AutoMapReachLevel> levels = ThreeLevels();
  levels[0].jumps.push_back(AutoMapReachJump{1 * 5 + 4, 1, 0, 0});
  std::map<int, std::vector<bool> > reached = AutoMapCompletion_ReachableAcross(levels, WestOfLevel0());
  CHECK(CountReached(reached[0]) == 6);
  CHECK(CountReached(reached[1]) == 0);
  CHECK(CountReached(reached[2]) == 0);
}

// Stairs to level 1, stairs on to level 2, and a teleporter there back into level 0's east room;
// jumps onto rock, off the grid or to a level not given lead nowhere
static void JumpsChainAcrossLevelsAndBack(void)
{
  std::map<int, AutoMapReachLevel> levels = ThreeLevels();
  levels[0].jumps.push_back(AutoMapReachJump{0, 1, 0, 0});
  levels[1].jumps.push_back(AutoMapReachJump{2 * 5 + 1, 2, 1, 2});
  levels[2].jumps.push_back(AutoMapReachJump{2 * 5 + 1, 0, 3, 0});
  levels[2].jumps.push_back(AutoMapReachJump{0, 1, 2, 1});
  levels[2].jumps.push_back(AutoMapReachJump{0, 1, 9, 9});
  levels[2].jumps.push_back(AutoMapReachJump{0, 7, 0, 0});
  levels[2].jumps.push_back(AutoMapReachJump{0, -1, 0, 0});
  std::map<int, std::vector<bool> > reached = AutoMapCompletion_ReachableAcross(levels, WestOfLevel0());
  CHECK(CountReached(reached[0]) == 12);
  CHECK(CountReached(reached[1]) == 6);
  CHECK(CountReached(reached[2]) == 6);
}

// A teleporter that takes the party moves it away as it steps on: the room behind it is not
// reached by walking across it, only where it leads
static void ATeleporterSquareLeadsOnlyWhereItSends(void)
{
  std::map<int, AutoMapReachLevel> levels = ThreeLevels();
  levels[0].grid.cells[1 * 5 + 2] = AutoMapReachPassedOn;
  levels[0].jumps.push_back(AutoMapReachJump{1 * 5 + 2, 1, 0, 0});
  std::map<int, std::vector<bool> > reached = AutoMapCompletion_ReachableAcross(levels, WestOfLevel0());
  CHECK(CountReached(reached[0]) == 7);
  CHECK(reached[0][1 * 5 + 2] && !reached[0][1 * 5 + 3]);
  CHECK(CountReached(reached[1]) == 6);
}

// A level read before is read again once the party has spent time on it since (its version, the
// ticks spent there, moved), whether the window was open or not; the party's own level at most
// every 2 seconds
static void ALevelIsReadAgainWhenThePartyWasThereSince(void)
{
  CHECK(AutoMapCompletion_LevelIsStale(false, 0, 0, false, 0));        // never read
  CHECK(!AutoMapCompletion_LevelIsStale(true, 500, 500, false, 60000)); // nobody was there since
  CHECK(AutoMapCompletion_LevelIsStale(true, 500, 900, false, 10));     // cleared and left while not looking
  CHECK(!AutoMapCompletion_LevelIsStale(true, 500, 900, true, 1000));   // the party is there: not yet
  CHECK(AutoMapCompletion_LevelIsStale(true, 500, 900, true, 2000));
}

void TestAutoMapCompletion(void)
{
  ALevelIsReadAgainWhenThePartyWasThereSince();
  ASeedReachesItsOwnRoomOnly();
  ADoorJoinsTheRooms();
  SeedsOnRockOrOutsideReachNothing();
  ASeedInEachRoomReachesBoth();
  PitsLeadDownSeveralLevels();
  AJumpFromAClosedOffRoomCountsNothing();
  JumpsChainAcrossLevelsAndBack();
  ATeleporterSquareLeadsOnlyWhereItSends();
}
