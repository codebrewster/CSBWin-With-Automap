#include "TestSupport.h"
#include "../AutoMapExplored.h"

static AutoMapExplored TwoLevelDungeon(void)
{
  std::vector<AutoMapLevelSize> sizes;
  sizes.push_back(AutoMapLevelSize{5, 3});
  sizes.push_back(AutoMapLevelSize{32, 32});
  AutoMapExplored explored;
  explored.Reset(sizes);
  return explored;
}

static void NothingIsExploredAfterReset(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  CHECK(explored.LevelCount() == 2);
  CHECK(!explored.IsExplored(AutoMapCell{0, 0, 0}));
  CHECK(!explored.IsExplored(AutoMapCell{1, 31, 31}));
}

static void MarkedCellIsExploredAndItsNeighboursAreNot(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.Mark(AutoMapCell{0, 4, 2});
  CHECK(explored.IsExplored(AutoMapCell{0, 4, 2}));
  CHECK(!explored.IsExplored(AutoMapCell{0, 3, 2}));
  CHECK(!explored.IsExplored(AutoMapCell{0, 4, 1}));
  CHECK(!explored.IsExplored(AutoMapCell{1, 4, 2}));
}

static void LevelsOfDifferentSizesKeepTheirOwnCells(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.Mark(AutoMapCell{1, 31, 31});
  explored.Mark(AutoMapCell{1, 5, 0});
  CHECK(explored.IsExplored(AutoMapCell{1, 31, 31}));
  CHECK(explored.IsExplored(AutoMapCell{1, 5, 0}));
  CHECK(!explored.IsExplored(AutoMapCell{0, 0, 1}));
  CHECK(explored.SizeOfLevel(0).width == 5);
  CHECK(explored.SizeOfLevel(1).height == 32);
}

// The 3D view looks past the edge of a level, and the prison screen reports level 255
static void CellsOutsideTheDungeonAreIgnored(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.Mark(AutoMapCell{0, -2, 0});
  explored.Mark(AutoMapCell{0, 6, 0});
  explored.Mark(AutoMapCell{0, 0, 4});
  explored.Mark(AutoMapCell{0, 0, -2});
  explored.Mark(AutoMapCell{2, 0, 0});
  explored.Mark(AutoMapCell{255, 0, 0});
  explored.Mark(AutoMapCell{-1, 0, 0});
  for (int y = 0; y < 3; y++)
  {
    for (int x = 0; x < 5; x++) CHECK(!explored.IsExplored(AutoMapCell{0, x, y}));
  }
  CHECK(!explored.IsExplored(AutoMapCell{0, -2, 0}));
  CHECK(!explored.IsExplored(AutoMapCell{255, 0, 0}));
}

static void ResetForgetsEverything(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.Mark(AutoMapCell{0, 1, 1});
  explored.Reset(std::vector<AutoMapLevelSize>(1, AutoMapLevelSize{5, 3}));
  CHECK(explored.LevelCount() == 1);
  CHECK(!explored.IsExplored(AutoMapCell{0, 1, 1}));
}

static void BitsAreStoredRowByRowLowestBitFirst(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.Mark(AutoMapCell{0, 0, 0});
  explored.Mark(AutoMapCell{0, 4, 1});
  const std::vector<uint8_t> &bits = explored.BitsOfLevel(0);
  CHECK(bits.size() == 2);
  CHECK(bits[0] == 0x01);
  CHECK(bits[1] == 0x02);
}

// The save file stores these bytes as they are: four bits per cell, in the cell order of the
// explored bits, side by side lowest bit first
static void FaceBitsAreStoredFourPerCellLowestBitFirst(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkFace(AutoMapCell{0, 0, 0}, 0);
  explored.MarkFace(AutoMapCell{0, 1, 0}, 3);
  explored.MarkFace(AutoMapCell{0, 2, 0}, 1);
  const std::vector<uint8_t> &bits = explored.FaceBitsOfLevel(0);
  CHECK(bits.size() == 8);
  CHECK(bits[0] == 0x81);
  CHECK(bits[1] == 0x02);
  CHECK(AutoMapExplored::FaceByteCountFor(AutoMapLevelSize{5, 3}) == 8);
}

static void NoFaceIsSeenAfterReset(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  for (int level = 0; level < 2; level++)
  {
    for (int side = 0; side < 4; side++) CHECK(!explored.FaceIsSeen(AutoMapCell{level, 0, 0}, side));
  }
}

static void MarkedFaceIsSeenAndTheOtherSidesAreNot(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkFace(AutoMapCell{0, 4, 2}, 1);
  CHECK(explored.FaceIsSeen(AutoMapCell{0, 4, 2}, 1));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 4, 2}, 0));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 4, 2}, 2));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 4, 2}, 3));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 3, 2}, 1));
  CHECK(!explored.FaceIsSeen(AutoMapCell{1, 4, 2}, 1));
  CHECK(!explored.IsExplored(AutoMapCell{0, 4, 2}));
}

static void FacesOutsideTheDungeonOrSideAreIgnored(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkFace(AutoMapCell{0, -2, 0}, 0);
  explored.MarkFace(AutoMapCell{0, 6, 0}, 0);
  explored.MarkFace(AutoMapCell{2, 0, 0}, 0);
  explored.MarkFace(AutoMapCell{0, 0, 0}, -1);
  explored.MarkFace(AutoMapCell{0, 0, 0}, 4);
  for (int level = 0; level < 2; level++)
  {
    for (size_t i = 0; i < explored.FaceBitsOfLevel(level).size(); i++)
    {
      CHECK(explored.FaceBitsOfLevel(level)[i] == 0);
    }
  }
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, -2, 0}, 0));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 6, 0}, 0));
  CHECK(!explored.FaceIsSeen(AutoMapCell{2, 0, 0}, 0));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 0, 0}, -1));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 0, 0}, 4));
}

static void FaceBitsRoundTrip(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkFace(AutoMapCell{1, 31, 31}, 3);
  CHECK(explored.FaceBitsOfLevel(1).size() == AutoMapExplored::FaceByteCountFor(AutoMapLevelSize{32, 32}));
  CHECK(explored.FaceBitsOfLevel(1).size() == 512);
  AutoMapExplored copy = TwoLevelDungeon();
  copy.ReplaceFaceBitsOfLevel(1, explored.FaceBitsOfLevel(1));
  CHECK(copy.FaceIsSeen(AutoMapCell{1, 31, 31}, 3));
  CHECK(!copy.FaceIsSeen(AutoMapCell{1, 31, 31}, 2));
}

static void OldRuleMarksFacesTowardsExploredNeighbours(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.Mark(AutoMapCell{0, 2, 1});
  explored.MarkFacesTowardsExploredCells();
  CHECK(explored.FaceIsSeen(AutoMapCell{0, 2, 0}, 2));
  CHECK(explored.FaceIsSeen(AutoMapCell{0, 3, 1}, 3));
  CHECK(explored.FaceIsSeen(AutoMapCell{0, 2, 2}, 0));
  CHECK(explored.FaceIsSeen(AutoMapCell{0, 1, 1}, 1));
  for (int side = 0; side < 4; side++) CHECK(!explored.FaceIsSeen(AutoMapCell{0, 2, 1}, side));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 3, 1}, 1));
}

// The map_debug panel clears one level so its cells can be looked at afresh
static void ForgettingALevelLeavesTheOthers(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.Mark(AutoMapCell{0, 4, 2});
  explored.MarkFace(AutoMapCell{0, 4, 2}, 1);
  explored.Mark(AutoMapCell{1, 7, 7});
  explored.MarkFace(AutoMapCell{1, 7, 7}, 0);
  explored.ForgetLevel(0);
  CHECK(!explored.IsExplored(AutoMapCell{0, 4, 2}));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 4, 2}, 1));
  CHECK(explored.IsExplored(AutoMapCell{1, 7, 7}));
  CHECK(explored.FaceIsSeen(AutoMapCell{1, 7, 7}, 0));
  CHECK(explored.SizeOfLevel(0).width == 5);
  explored.ForgetLevel(2);
  explored.ForgetLevel(-1);
  CHECK(explored.IsExplored(AutoMapCell{1, 7, 7}));
}

// Something on a wall in the dark: the map shows a "?" on that side, not what it is
static void AGlimpsedFaceIsNotSeen(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkGlimpsedFace(AutoMapCell{0, 4, 2}, 1);
  CHECK(explored.FaceIsGlimpsed(AutoMapCell{0, 4, 2}, 1));
  CHECK(!explored.FaceIsGlimpsed(AutoMapCell{0, 4, 2}, 0));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 4, 2}, 1));
}

static void ASeenFaceIsNoLongerAGlimpse(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkGlimpsedFace(AutoMapCell{0, 4, 2}, 1);
  explored.MarkFace(AutoMapCell{0, 4, 2}, 1);
  CHECK(!explored.FaceIsGlimpsed(AutoMapCell{0, 4, 2}, 1));
  explored.MarkGlimpsedFace(AutoMapCell{0, 4, 2}, 1);
  CHECK(!explored.FaceIsGlimpsed(AutoMapCell{0, 4, 2}, 1));
}

static void ForgettingALevelForgetsItsGlimpses(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkGlimpsedFace(AutoMapCell{0, 4, 2}, 1);
  explored.MarkGlimpsedFace(AutoMapCell{1, 4, 2}, 1);
  explored.ForgetLevel(0);
  CHECK(!explored.FaceIsGlimpsed(AutoMapCell{0, 4, 2}, 1));
  CHECK(explored.FaceIsGlimpsed(AutoMapCell{1, 4, 2}, 1));
}

// Wall text is read standing in front of it, not when its face is seen from afar
static void ASeenFaceIsNotRead(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkFace(AutoMapCell{0, 4, 2}, 1);
  CHECK(!explored.TextIsRead(AutoMapCell{0, 4, 2}, 1));
  explored.MarkTextRead(AutoMapCell{0, 4, 2}, 1);
  CHECK(explored.TextIsRead(AutoMapCell{0, 4, 2}, 1));
  CHECK(!explored.TextIsRead(AutoMapCell{0, 4, 2}, 0));
  explored.ForgetLevel(0);
  CHECK(!explored.TextIsRead(AutoMapCell{0, 4, 2}, 1));
}

// What lies in an alcove shows on the wall facing the party, not when the face is seen from aside
static void ASeenFaceIsNotLookedInto(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkFace(AutoMapCell{0, 4, 2}, 1);
  CHECK(!explored.AlcoveIsLookedInto(AutoMapCell{0, 4, 2}, 1));
  explored.MarkAlcoveLookedInto(AutoMapCell{0, 4, 2}, 1);
  CHECK(explored.AlcoveIsLookedInto(AutoMapCell{0, 4, 2}, 1));
  CHECK(!explored.AlcoveIsLookedInto(AutoMapCell{0, 4, 2}, 0));
  explored.ForgetLevel(0);
  CHECK(!explored.AlcoveIsLookedInto(AutoMapCell{0, 4, 2}, 1));
}

// The ring one step outside a level, where the engine makes up rock: kept apart from the level's
// own bits, corners included, and forgotten with the level
static void TheRimAroundALevelIsKept(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.Mark(AutoMapCell{0, -1, -1});
  explored.Mark(AutoMapCell{0, 5, 3});
  explored.MarkFace(AutoMapCell{0, 2, -1}, 2);
  explored.MarkTextRead(AutoMapCell{0, -1, 1}, 1);
  explored.MarkAlcoveLookedInto(AutoMapCell{0, 5, 1}, 3);
  CHECK(explored.IsExplored(AutoMapCell{0, -1, -1}));
  CHECK(explored.IsExplored(AutoMapCell{0, 5, 3}));
  CHECK(!explored.IsExplored(AutoMapCell{0, 0, -1}));
  CHECK(explored.FaceIsSeen(AutoMapCell{0, 2, -1}, 2));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 2, 0}, 2));
  CHECK(explored.TextIsRead(AutoMapCell{0, -1, 1}, 1));
  CHECK(explored.AlcoveIsLookedInto(AutoMapCell{0, 5, 1}, 3));
  CHECK(!explored.IsExplored(AutoMapCell{1, -1, -1}));
  for (size_t i = 0; i < explored.BitsOfLevel(0).size(); i++) CHECK(explored.BitsOfLevel(0)[i] == 0);
  // Row above (7 cells), row below (7), then the left and right columns (3 each)
  CHECK(AutoMapExplored::RimByteCountFor(AutoMapLevelSize{5, 3}, AutoMapExploredBits) == 3);
  CHECK(explored.RimBitsOfLevel(0, AutoMapExploredBits)[0] == 1);
  CHECK(explored.RimBitsOfLevel(0, AutoMapExploredBits)[1] == 0x20);
  explored.ForgetLevel(0);
  CHECK(!explored.IsExplored(AutoMapCell{0, -1, -1}));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 2, -1}, 2));
}

// What a version-10 file kept on the floor beside a painted rim cell
static void RimMarksOnTheEdgeBecomeExploredRimCells(void)
{
  AutoMapExplored explored = TwoLevelDungeon();
  explored.MarkFace(AutoMapCell{0, 0, 1}, 3);
  explored.MarkFace(AutoMapCell{0, 4, 2}, 2);
  explored.MarkFace(AutoMapCell{0, 2, 1}, 3);
  explored.TakeRimMarksFromTheEdge();
  CHECK(explored.IsExplored(AutoMapCell{0, -1, 1}));
  CHECK(explored.IsExplored(AutoMapCell{0, 4, 3}));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 0, 1}, 3));
  CHECK(!explored.FaceIsSeen(AutoMapCell{0, 4, 2}, 2));
  CHECK(explored.FaceIsSeen(AutoMapCell{0, 2, 1}, 3));
  CHECK(!explored.IsExplored(AutoMapCell{0, 1, 1}));
}

void TestAutoMapExplored(void)
{
  TheRimAroundALevelIsKept();
  RimMarksOnTheEdgeBecomeExploredRimCells();
  ASeenFaceIsNotRead();
  ASeenFaceIsNotLookedInto();
  AGlimpsedFaceIsNotSeen();
  ASeenFaceIsNoLongerAGlimpse();
  ForgettingALevelForgetsItsGlimpses();
  NothingIsExploredAfterReset();
  MarkedCellIsExploredAndItsNeighboursAreNot();
  LevelsOfDifferentSizesKeepTheirOwnCells();
  CellsOutsideTheDungeonAreIgnored();
  ResetForgetsEverything();
  BitsAreStoredRowByRowLowestBitFirst();
  FaceBitsAreStoredFourPerCellLowestBitFirst();
  NoFaceIsSeenAfterReset();
  MarkedFaceIsSeenAndTheOtherSidesAreNot();
  FacesOutsideTheDungeonOrSideAreIgnored();
  FaceBitsRoundTrip();
  OldRuleMarksFacesTowardsExploredNeighbours();
  ForgettingALevelLeavesTheOthers();
}
