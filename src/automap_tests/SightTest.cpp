#include "TestSupport.h"
#include "../AutoMapSight.h"

#include <string>

static const int NOT_IN_THE_CONE = -1;

static int CellAt(int stepsForward, int stepsRight)
{
  for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++)
  {
    AutoMapSightOffset offset = AutoMapSight_OffsetOfCell(cell);
    if (offset.stepsForward == stepsForward && offset.stepsRight == stepsRight) return cell;
  }
  return NOT_IN_THE_CONE;
}

// A piece of dungeon as the party sees it, far row first. '#' cannot be seen through, '.' can,
// 'D' is a shut door facing the party. The party stands in the middle of the last row. The five
// columns are side -2..+2.
struct Surroundings
{
  bool canSeeThrough[AUTOMAP_SIGHT_CELL_COUNT];
  bool isShutDoor[AUTOMAP_SIGHT_CELL_COUNT];
  bool isVisible[AUTOMAP_SIGHT_CELL_COUNT];
  bool isExplored[AUTOMAP_SIGHT_CELL_COUNT];
};

static Surroundings Look(const char *rowsFromFarToNear[5])
{
  Surroundings surroundings;
  for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++)
  {
    AutoMapSightOffset offset = AutoMapSight_OffsetOfCell(cell);
    char symbol = rowsFromFarToNear[4 - offset.stepsForward][offset.stepsRight + 2];
    surroundings.canSeeThrough[cell] = symbol == '.';
    surroundings.isShutDoor[cell] = symbol == 'D';
  }
  AutoMapSight_FindVisibleCells(surroundings.canSeeThrough, surroundings.isShutDoor,
                                surroundings.isVisible);
  AutoMapSight_FindExploredCells(surroundings.canSeeThrough, surroundings.isShutDoor,
                                 surroundings.isExplored);
  return surroundings;
}

static bool Sees(const Surroundings &surroundings, int stepsForward, int stepsRight)
{
  int cell = CellAt(stepsForward, stepsRight);
  return cell != NOT_IN_THE_CONE && surroundings.isVisible[cell];
}

static bool Explores(const Surroundings &surroundings, int stepsForward, int stepsRight)
{
  int cell = CellAt(stepsForward, stepsRight);
  return cell != NOT_IN_THE_CONE && surroundings.isExplored[cell];
}

static void CellNumbersFollowTheViewportTables(void)
{
  CHECK(CellAt(0, 0) == AUTOMAP_SIGHT_PARTY_CELL);
  CHECK(CellAt(1, 0) == 17);
  CHECK(CellAt(4, -2) == 0);
  CHECK(CellAt(4, 2) == 1);
  CHECK(CellAt(2, -2) == 10);
  CHECK(CellAt(1, -2) == NOT_IN_THE_CONE);
  CHECK(CellAt(0, 2) == NOT_IN_THE_CONE);
}

static void ThePartyCellIsAlwaysVisible(void)
{
  const char *wallsAllAround[5] = {"#####", "#####", "#####", "#####", "#####"};
  Surroundings seen = Look(wallsAllAround);
  CHECK(Sees(seen, 0, 0));
}

// The view never shows more than three tiles ahead or more than one tile to either side, so
// nothing on the far row or two tiles to a side is ever seen however open the dungeon is.
static void TheViewSeesNoFarRowAndNoCellTwoTilesToTheSide(void)
{
  const char *wideOpen[5] = {".....", ".....", ".....", ".....", "....."};
  Surroundings seen = Look(wideOpen);
  for (int stepsForward = 0; stepsForward <= 3; stepsForward++)
  {
    CHECK(Sees(seen, stepsForward, 0));
    CHECK(Sees(seen, stepsForward, -1));
    CHECK(Sees(seen, stepsForward, 1));
  }
  for (int stepsRight = -2; stepsRight <= 2; stepsRight++) CHECK(!Sees(seen, 4, stepsRight));
  for (int stepsForward = 0; stepsForward <= 3; stepsForward++)
  {
    CHECK(!Sees(seen, stepsForward, -2));
    CHECK(!Sees(seen, stepsForward, 2));
  }
}

// A wall straight ahead and walls beside the party: the wall two ahead is seen but the far
// corner two tiles to the side at three ahead is not.
static void AWallTwoTilesAheadDoesNotRevealTheFarCorner(void)
{
  const char *junction[5] = {".....", ".....", "..#..", ".....", "##.##"};
  Surroundings seen = Look(junction);
  CHECK(Sees(seen, 2, 0));
  CHECK(!Sees(seen, 3, -2));
  CHECK(!Sees(seen, 3, 2));
}

static void AStraightCorridorRevealsItsFloorAndWalls(void)
{
  const char *corridor[5] = {"##.##", "##.##", "##.##", "##.##", "##.##"};
  Surroundings seen = Look(corridor);
  for (int stepsForward = 0; stepsForward <= 3; stepsForward++)
  {
    CHECK(Sees(seen, stepsForward, 0));
    CHECK(Sees(seen, stepsForward, -1));
    CHECK(Sees(seen, stepsForward, 1));
  }
  CHECK(!Sees(seen, 3, -2));
  CHECK(!Sees(seen, 3, 2));
}

// A wall beside the party clips the centre of the diagonal tile: a monster standing on the
// front-left or front-right tile is hidden even though the tile straight ahead is seen. The
// tile itself is still explored, so a sword lying anywhere on it may show (below).
static void AWallBesideThePartyHidesTheDiagonalTile(void)
{
  const char *doorway[5] = {".....", ".....", ".....", ".....", "##.##"};
  Surroundings seen = Look(doorway);
  CHECK(Sees(seen, 1, 0));
  CHECK(!Sees(seen, 1, -1));
  CHECK(!Sees(seen, 1, 1));
}

// The floor cell behind a wall beside the party is not wholly hidden: its inner-forward sliver
// is in sight, so a sword lying there is explored, while the centre stays hidden (the monster
// above). This is the sword on the front-right tile the captures showed.
static void AWallBesideThePartyStillRevealsTheSliverOfTheDiagonalTile(void)
{
  const char *wallOnTheRight[5] = {".....", ".....", ".....", ".....", "...#."};
  Surroundings seen = Look(wallOnTheRight);
  CHECK(!Sees(seen, 1, 1));
  CHECK(Explores(seen, 1, 1));
  CHECK(Explores(seen, 1, -1));
}

static void AWallOnTheLeftRevealsTheSliverOfTheFrontLeftTile(void)
{
  const char *wallOnTheLeft[5] = {".....", ".....", ".....", ".....", ".#..."};
  Surroundings seen = Look(wallOnTheLeft);
  CHECK(!Sees(seen, 1, -1));
  CHECK(Explores(seen, 1, -1));
  CHECK(Explores(seen, 1, 1));
}

// The floor on the front-left tile is hidden by the wall beside the party, but the wall itself
// is seen where its face turns towards the party.
static void AWallIsSeenAtItsCornerWhereTheFloorBesideItIsNot(void)
{
  const char *wallOnTheLeft[5] = {".....", ".....", ".....", ".#...", ".#..."};
  Surroundings seen = Look(wallOnTheLeft);
  CHECK(Sees(seen, 1, -1));
}

static void AWallDirectlyAheadHidesTheCellsBehindIt(void)
{
  const char *deadEnd[5] = {"#####", "#####", "#####", "#####", ".#.#."};
  Surroundings seen = Look(deadEnd);
  CHECK(Sees(seen, 1, 0));
  CHECK(!Sees(seen, 2, 0));
  CHECK(!Sees(seen, 3, 0));
  CHECK(!Sees(seen, 4, 0));
}

// The wall straight ahead covers columns 32..191: past its edge the 3D view shows a strip of the
// column beside it, so a shut door two ahead and one to the right is seen (captured: party on
// 1,4,25 facing north, the door on 1,5,23).
static void AWallStraightAheadLeavesTheDiagonalBehindItInSight(void)
{
  const char *wallAhead[5] = {"#####", "#####", "#####", "###.#", "....."};
  Surroundings seen = Look(wallAhead);
  CHECK(Sees(seen, 2, 1));
}

// The same strip reaches three ahead: the floor two ahead and one to the right is explored and
// the wall behind it seen (captured: party on 0,6,2 facing east, the floor 0,8,3, the wall 0,9,3)
static void AWallStraightAheadLeavesTheFarDiagonalInSight(void)
{
  const char *wallAhead[5] = {"#####", "#####", "###.#", "###.#", "....."};
  Surroundings seen = Look(wallAhead);
  CHECK(Explores(seen, 2, 1));
  CHECK(Sees(seen, 3, 1));
  CHECK(!Sees(seen, 2, 0));
}

// Past a wall on the left one ahead, the 3D view shows the inner edge of the floor two ahead on
// the left (captured: party on 0,1,5 facing south, the floor on 0,2,7)
static void AWallBesideTheWayLeavesTheEdgeOfTheTileBehindItInSight(void)
{
  const char *wallOnTheLeft[5] = {"#####", "#####", "#..##", "##.##", "##.##"};
  Surroundings seen = Look(wallOnTheLeft);
  CHECK(Explores(seen, 2, -1));
}

// Two ahead the same: the floor three ahead on the left past a wall two ahead on the left
// (captured: party on 0,1,7 facing east, the floor on 0,4,6)
static void AWallTwoAheadBesideTheWayLeavesTheEdgeOfTheTileBehindItInSight(void)
{
  const char *wallOnTheLeft[5] = {"#####", "#..##", "##.##", "##.##", "##.##"};
  Surroundings seen = Look(wallOnTheLeft);
  CHECK(Explores(seen, 3, -1));
  CHECK(!Explores(seen, 3, -2));
}

// A corridor opening on both sides two ahead (captured: party on 0,4,2 facing east, the floors on
// 0,6,1, 0,6,3 and 0,7,3)
static void ACorridorShowsTheTilesOnBothSidesWhereItWidens(void)
{
  const char *widening[5] = {"#####", "###.#", "#...#", "##.##", "##.##"};
  Surroundings seen = Look(widening);
  CHECK(Explores(seen, 2, -1));
  CHECK(Explores(seen, 2, 1));
  CHECK(Explores(seen, 3, 1));
}

// Three ahead and two to the side the 3D view draws stone at the screen's edge, when nothing
// nearer covers it (captured: party on 0,1,6 facing east, the wall on 0,4,8). A floor there it
// does not draw.
static void StoneThreeAheadTwoToTheSideIsSeenAtTheEdge(void)
{
  const char *openRight[5] = {"#####", "##..#", "##...", "##...", "##..."};
  Surroundings seen = Look(openRight);
  CHECK(Sees(seen, 3, 2));
  CHECK(Explores(seen, 3, 2));
  const char *floorThere[5] = {"#####", "##...", "##...", "##...", "##..."};
  Surroundings floor = Look(floorThere);
  CHECK(!Explores(floor, 3, 2));
  const char *coveredByTheWallBeside[5] = {"#####", "##.##", "##...", "##...", "##..."};
  CHECK(!Sees(Look(coveredByTheWallBeside), 3, 2));
}

// A portcullis two ahead at light 5: a grid of 5 single pixels and 20 wedges of three, no patch
// of four, is noticed; a few stray pixels of a dark wooden door are not
static void ADoorIsSeenByAllItsPixelsThatStandOut(void)
{
  AutoMapItemShape portcullisGrid = {400, 5 * 1 + 20 * 3, 3};
  CHECK(AutoMapSight_DoorIsSeen(portcullisGrid));
  AutoMapItemShape strayPixels = {400, 3, 3};
  CHECK(!AutoMapSight_DoorIsSeen(strayPixels));
}

static void ShutDoorCannotBeSeenThroughUnlessItIsAGrate(void)
{
  AutoMapSightDoor closedSolidDoor = {AutoMapDoorClosed, false};
  AutoMapSightDoor almostClosedSolidDoor = {AutoMapDoorAlmostClosed, false};
  AutoMapSightDoor closedGrate = {AutoMapDoorClosed, true};
  AutoMapSightDoor almostClosedGrate = {AutoMapDoorAlmostClosed, true};
  CHECK(!AutoMapSight_CanSeeThroughDoor(closedSolidDoor));
  CHECK(!AutoMapSight_CanSeeThroughDoor(almostClosedSolidDoor));
  CHECK(AutoMapSight_CanSeeThroughDoor(closedGrate));
  CHECK(AutoMapSight_CanSeeThroughDoor(almostClosedGrate));
}

static void DoorThatIsNotShutCanBeSeenThrough(void)
{
  AutoMapSightDoor openDoor = {AutoMapDoorOpen, false};
  AutoMapSightDoor almostOpenDoor = {AutoMapDoorAlmostOpen, false};
  AutoMapSightDoor halfOpenDoor = {AutoMapDoorHalfOpen, false};
  AutoMapSightDoor destroyedDoor = {AutoMapDoorDestroyed, false};
  CHECK(AutoMapSight_CanSeeThroughDoor(openDoor));
  CHECK(AutoMapSight_CanSeeThroughDoor(almostOpenDoor));
  CHECK(AutoMapSight_CanSeeThroughDoor(halfOpenDoor));
  CHECK(AutoMapSight_CanSeeThroughDoor(destroyedDoor));
}

static void AShutDoorIsSeenButHidesTheCorridorBehindIt(void)
{
  const char *withAShutDoor[5] = {".....", ".....", "..#..", ".....", "....."};
  Surroundings seen = Look(withAShutDoor);
  CHECK(Sees(seen, 2, 0));
  CHECK(!Sees(seen, 3, 0));
}

// A wooden door three ahead and one to the left hides only the columns it is painted in: the
// strip of stone at the screen's edge beside it stays in sight (captured: party on 0,2,17 facing
// north, the door on 0,1,14, the stone on 0,0,14). As stone the same cell hides the strip.
static void AShutDoorBesideTheWayLeavesTheStoneBesideItInSight(void)
{
  const char *doorAheadLeft[5] = {"#####", "#D#..", "#..#.", "#.#..", "#...#"};
  CHECK(Explores(Look(doorAheadLeft), 3, -2));
  const char *stoneAheadLeft[5] = {"#####", "###..", "#..#.", "#.#..", "#...#"};
  CHECK(!Explores(Look(stoneAheadLeft), 3, -2));
}

// Straight ahead the door's frame fills the doorway: nothing behind it shows
static void AShutDoorStraightAheadHidesAllBehindIt(void)
{
  const char *doorAhead[5] = {".....", ".....", "..D..", ".....", "....."};
  Surroundings seen = Look(doorAhead);
  CHECK(Explores(seen, 2, 0));
  CHECK(!Explores(seen, 3, 0));
}

// The faces of a view cell that turn towards the party, as summary indices: 0 right, 1 front, 2 left
static bool ShowsFaces(int cellNumber, bool right, bool front, bool left)
{
  bool shown[3] = {true, true, true};
  AutoMapSight_FacesShownOfCell(cellNumber, shown);
  return shown[0] == right && shown[1] == front && shown[2] == left;
}

// Only the faces the 3D view draws: the front faces of 15 and 16 and the sides of the cells
// beside the party are never on screen
static void FacesShownFollowTheView(void)
{
  CHECK(ShowsFaces(15, true, false, false));
  CHECK(ShowsFaces(16, false, false, true));
  CHECK(ShowsFaces(18, false, false, false));
  CHECK(ShowsFaces(19, false, false, false));
  CHECK(ShowsFaces(AUTOMAP_SIGHT_PARTY_CELL, false, false, false));
  CHECK(ShowsFaces(7, true, true, false));
  CHECK(ShowsFaces(12, true, true, false));
  CHECK(ShowsFaces(8, false, true, true));
  CHECK(ShowsFaces(13, false, true, true));
  CHECK(ShowsFaces(9, false, true, false));
  CHECK(ShowsFaces(14, false, true, false));
  CHECK(ShowsFaces(17, false, true, false));
}

static void DecorationPlacementsMatchTheViewport(void)
{
  const int expected[13][2] =
  {
    {7, 0}, {8, 2}, {7, 1}, {9, 1}, {8, 1}, {12, 0}, {13, 2},
    {12, 1}, {14, 1}, {13, 1}, {15, 0}, {16, 2}, {17, 1}
  };
  for (int drawLocation = 0; drawLocation < 13; drawLocation++)
  {
    int cellNumber = -1;
    int summaryIndex = -1;
    CHECK(AutoMapSight_DecorationPlacement(drawLocation, &cellNumber, &summaryIndex));
    CHECK(cellNumber == expected[drawLocation][0]);
    CHECK(summaryIndex == expected[drawLocation][1]);
  }
  int cellNumber = 0;
  int summaryIndex = 0;
  CHECK(!AutoMapSight_DecorationPlacement(-1, &cellNumber, &summaryIndex));
  CHECK(!AutoMapSight_DecorationPlacement(13, &cellNumber, &summaryIndex));
  CHECK(AutoMapSight_SideOfSummaryIndex(0, 1) == 2);
  CHECK(AutoMapSight_SideOfSummaryIndex(1, 0) == 2);
  CHECK(AutoMapSight_SideOfSummaryIndex(3, 2) == 2);
}

// The brightest and darkest palettes of Dungeon Master's graphics.dat, 0x0RGB
static const short BRIGHT_PALETTE[16] = {0x000, 0x333, 0x444, 0x310, 0x066, 0x420, 0x040, 0x060,
                                         0x700, 0x750, 0x643, 0x770, 0x222, 0x555, 0x007, 0x777};
static const short DARK_PALETTE[16] = {0x000, 0x000, 0x000, 0x000, 0x066, 0x000, 0x000, 0x010,
                                       0x200, 0x200, 0x100, 0x320, 0x000, 0x000, 0x002, 0x222};
static const int GREY_333 = 1;
static const int BLACK = 0;
static const int BROWN_310 = 3;
static const int BROWN_420 = 5;

static std::vector<AutoMapPixelPair> Pixels(int behind, int drawn, int count)
{
  AutoMapPixelPair pair = {behind, drawn, count};
  return std::vector<AutoMapPixelPair>(1, pair);
}

static void APixelOneBrightnessStepFromTheWallIsVisible(void)
{
  CHECK(AutoMapSight_VisiblePixels(Pixels(GREY_333, BLACK, 7), BRIGHT_PALETTE) == 7);
  CHECK(AutoMapSight_VisiblePixels(Pixels(BROWN_310, BLACK, 7), BRIGHT_PALETTE) == 7);
}

static void APixelLessThanAStepFromTheWallIsNotVisible(void)
{
  CHECK(AutoMapSight_VisiblePixels(Pixels(BROWN_310, BROWN_420, 7), BRIGHT_PALETTE) == 0);
}

static void ADarkPaletteCanTurnBothColoursBlack(void)
{
  CHECK(AutoMapSight_VisiblePixels(Pixels(GREY_333, BLACK, 7), DARK_PALETTE) == 0);
}

// A view rows high, one string per row: '.' grey wall, 'o' grey floor, 'b' black floor; an
// item pixel in upper case: 'B' brown, 'G' grey, 'K' black, over the floor of the lower case letter
// before it ('oB': brown over grey floor takes two characters, one pixel)
static const short GREY = 0x444;
static const short DARK = 0x000;
static const short BROWN = 0x200;

static short ColourOf(char c)
{
  if (c == '.' || c == 'o' || c == 'G') return GREY;
  if (c == 'B') return BROWN;
  return DARK;
}

static AutoMapDrawView ViewOf(const char *const *rows, int height)
{
  AutoMapDrawView view;
  view.height = height;
  view.width = 0;
  for (const char *c = rows[0]; *c != 0; c++) if (*c == '.' || *c == 'o' || *c == 'b') view.width++;
  for (int y = 0; y < height; y++)
  {
    for (const char *c = rows[y]; *c != 0; c++)
    {
      if (*c >= 'A' && *c <= 'Z') continue;
      bool isItem = c[1] >= 'A' && c[1] <= 'Z';
      view.background.push_back(ColourOf(*c));
      view.screen.push_back(isItem ? ColourOf(c[1]) : ColourOf(*c));
      view.isDrawn.push_back(isItem);
    }
  }
  return view;
}

// The bread under a wall: its grey pixels look like the stone around them
static void AnItemPixelInTheColourAroundItIsCamouflaged(void)
{
  const char *rows[] = {"......", ".oGoGoB..", ".oG.oG..", "......"};
  AutoMapItemShape shape = AutoMapSight_ShapeOfItem(ViewOf(rows, 4), NULL);
  CHECK(shape.drawn == 5 && shape.visible == 1 && shape.largestPatch == 1);
}

// A black club on the black floor, between the floor's grey specks, only hides specks
static void ABlackItemOnABlackFloorIsCamouflaged(void)
{
  const char *rows[] = {"bobob", "bbKoKbKb", "bobob"};
  CHECK(AutoMapSight_ShapeOfItem(ViewOf(rows, 3), NULL).visible == 0);
}

// The corn in the dark: brown on black stands out, and its touching pixels make one patch
static void TouchingPixelsThatStandOutMakeAPatch(void)
{
  const char *rows[] = {"bbbbbb", "bbBbBbbbB", "bbbBbbb", "bbbbbb"};
  std::vector<bool> standsOut;
  AutoMapItemShape shape = AutoMapSight_ShapeOfItem(ViewOf(rows, 4), &standsOut);
  CHECK(shape.visible == 4 && shape.largestPatch == 3);
  CHECK(standsOut.size() == 24 && standsOut[8] && !standsOut[0]);
}

// A row of pixels: count drawn ones of letter drawn over the wall of letter wall
static std::string Row(char wall, const char *drawn, int count, int wallAfter)
{
  std::string row(2, wall);
  for (int i = 0; i < count; i++) { row += wall; row += drawn; }
  return row + std::string((size_t)wallAfter, wall);
}

// A black ring on the grey wall in full light, four steps darker than the stone around it
static void ADarkRingOnALitWallStandsOut(void)
{
  std::string ring = Row('.', "K", 8, 2);
  std::string wall = Row('.', "", 0, 10);
  const char *rows[] = {wall.c_str(), ring.c_str(), wall.c_str()};
  CHECK(AutoMapSight_ContrastingPixels(ViewOf(rows, 3), NULL) == 8);
  CHECK(AutoMapSight_DecorationIsSeen(ViewOf(rows, 3)));
}

// The same ring in the dark: the wall is black but for a few lit specks the ring covers
static void ADarkRingOnADarkWallDoesNotStandOut(void)
{
  const char *rows[] = {"bbbbbbbbbb", "bbKoKbKbKoKbKbKbb", "bbbobbbbbb"};
  CHECK(AutoMapSight_ContrastingPixels(ViewOf(rows, 3), NULL) == 0);
}

// A switch in the wall's own grey is no darker or brighter than the stone
static void AGreySwitchOnAGreyWallDoesNotStandOut(void)
{
  std::string grey = Row('.', "G", 8, 2);
  std::string wall = Row('.', "", 0, 10);
  const char *rows[] = {wall.c_str(), grey.c_str(), wall.c_str()};
  CHECK(AutoMapSight_ContrastingPixels(ViewOf(rows, 3), NULL) == 0);
}

static void ADecorationIsSeenFromEightContrastingPixels(void)
{
  std::string seven = Row('.', "K", 7, 3);
  std::string wall = Row('.', "", 0, 10);
  const char *rows[] = {wall.c_str(), seven.c_str(), wall.c_str()};
  std::vector<bool> contrasts;
  CHECK(AutoMapSight_ContrastingPixels(ViewOf(rows, 3), &contrasts) == 7);
  CHECK(contrasts.size() == 36 && contrasts[14] && !contrasts[13]);
  CHECK(!AutoMapSight_DecorationIsSeen(ViewOf(rows, 3)));
}

// Writing seen from the side is drawn as scribbles in the wall's own greys: a thin line two
// ahead (18 contrasting pixels) reads as nothing, a decoration of as many pixels as a ring would
static void ScribblesNeedTwentyFourContrastingPixels(void)
{
  // Each row of a view is as wide as the first: the wall rows match the drawn row
  std::string twentyThree = Row('.', "K", 23, 3);
  std::string wallOfTwentyThree = Row('.', "", 0, 23 + 3);
  const char *fewer[] = {wallOfTwentyThree.c_str(), twentyThree.c_str(), wallOfTwentyThree.c_str()};
  CHECK(AutoMapSight_DecorationIsSeen(ViewOf(fewer, 3)));
  CHECK(!AutoMapSight_ScribblesAreSeen(ViewOf(fewer, 3)));
  std::string twentyFour = Row('.', "K", 24, 3);
  std::string wallOfTwentyFour = Row('.', "", 0, 24 + 3);
  const char *enough[] = {wallOfTwentyFour.c_str(), twentyFour.c_str(), wallOfTwentyFour.c_str()};
  CHECK(AutoMapSight_ScribblesAreSeen(ViewOf(enough, 3)));
}

// A square decoration of side pixels in colour board, drawn in the middle of a wall of colour wall
static AutoMapDrawView BoardOnAWall(int side, short board, short wall)
{
  AutoMapDrawView view;
  view.width = side + 10;
  view.height = side + 10;
  for (int y = 0; y < view.height; y++)
  {
    for (int x = 0; x < view.width; x++)
    {
      bool isBoard = x >= 5 && x < 5 + side && y >= 5 && y < 5 + side;
      view.screen.push_back(isBoard ? board : wall);
      view.background.push_back(wall);
      view.isDrawn.push_back(isBoard);
    }
  }
  return view;
}

// The wooden board at light 4: straight ahead black on a wall of 1,1,1 (captured: party on 0,6,4
// facing west, the board on 0,5,4), from the side dark red on a wall gone black (party on 0,6,3
// facing south). Neither has a pixel two steps off the wall, yet both show; a few pixels do not,
// and at light 5, where all of it is black, nothing does.
static void ADecorationInAColourTheWallDoesNotShowIsSeen(void)
{
  const short DARK_GREY_111 = 0x111;
  const short DARK_RED_100 = 0x100;
  const short BLACK_000 = 0x000;
  AutoMapDrawView board = BoardOnAWall(14, BLACK_000, DARK_GREY_111);
  CHECK(AutoMapSight_ContrastingPixels(board, NULL) == 0);
  CHECK(AutoMapSight_DecorationIsSeen(board));
  AutoMapDrawView fromTheSide = BoardOnAWall(6, DARK_RED_100, BLACK_000);
  CHECK(AutoMapSight_ContrastingPixels(fromTheSide, NULL) == 0);
  CHECK(AutoMapSight_DecorationIsSeen(fromTheSide));
  // A crack at the foot of a wall at light 4 (captured: party on 2,8,40 facing north): a patch
  // of 14, under the eight pixels two steps off
  CHECK(AutoMapSight_DecorationIsSeen(BoardOnAWall(4, DARK_RED_100, BLACK_000)));
  CHECK(!AutoMapSight_DecorationIsSeen(BoardOnAWall(2, DARK_RED_100, BLACK_000)));
  CHECK(!AutoMapSight_DecorationIsSeen(BoardOnAWall(14, BLACK_000, BLACK_000)));
}

// A black slot of 86 pixels on the speckled wall ahead-right at light 3 (captured: party on
// 0,4,15 facing south, the slot on 0,3,16 east face): each black pixel has a black speck beside
// it, but the specks are gone where it lies. specksEvery: one wall pixel in so many of each row
// is black, the others a step up.
static AutoMapDrawView SlotOnASpeckledWall(int side, int specksEvery)
{
  const short BLACK_000 = 0x000;
  const short STEP_111 = 0x111;
  AutoMapDrawView view;
  view.width = side + 10;
  view.height = side + 10;
  for (int y = 0; y < view.height; y++)
  {
    for (int x = 0; x < view.width; x++)
    {
      bool isSlot = x >= 5 && x < 5 + side && y >= 5 && y < 5 + side;
      short wall = (x + 2 * y) % specksEvery == 0 ? BLACK_000 : STEP_111;
      view.screen.push_back(isSlot ? BLACK_000 : wall);
      view.background.push_back(wall);
      view.isDrawn.push_back(isSlot);
    }
  }
  return view;
}

// A black speck in every four pixels: three quarters of a step darker where the slot lies, seen;
// in every second, half a step, missed, as a slot too small to be told by its tone alone
static void ADecorationThatDarkensTheWallAsAWholeIsSeen(void)
{
  AutoMapDrawView quarterBlack = SlotOnASpeckledWall(8, 4);
  CHECK(AutoMapSight_ContrastingPixels(quarterBlack, NULL) == 0);
  CHECK(AutoMapSight_ShapeOfItem(quarterBlack, NULL).largestPatch == 0);
  CHECK(AutoMapSight_DecorationDarkening(quarterBlack) == 75);
  CHECK(AutoMapSight_DecorationIsSeen(quarterBlack));
  AutoMapDrawView halfBlack = SlotOnASpeckledWall(8, 2);
  CHECK(AutoMapSight_DecorationDarkening(halfBlack) == 50);
  CHECK(!AutoMapSight_DecorationIsSeen(halfBlack));
  CHECK(!AutoMapSight_DecorationIsSeen(SlotOnASpeckledWall(6, 4)));
}

// A vertical line of length pixels in colour line, one pixel wide, on a wall of colour wall
static AutoMapDrawView LineOnAWall(int length, short line, short wall)
{
  AutoMapDrawView view;
  view.width = 11;
  view.height = length + 10;
  for (int y = 0; y < view.height; y++)
  {
    for (int x = 0; x < view.width; x++)
    {
      bool isLine = x == 5 && y >= 5 && y < 5 + length;
      view.screen.push_back(isLine ? line : wall);
      view.background.push_back(wall);
      view.isDrawn.push_back(isLine);
    }
  }
  return view;
}

// A brown line on a black wall (captured: party on 0,4,17 facing north, the line on 0,3,14 east
// face): at light 2, (2,1,0), more than a step brighter than the wall, six pixels are seen; at
// light 3 the same line is (1,0,0), a third of a step, and missed. A grey line a step brighter
// (party on 0,4,15 facing south, the line on 0,5,17 west face, light 4) is missed too.
static void ATintedLineAStepOffTheWallIsSeen(void)
{
  const short BLACK_000 = 0x000;
  AutoMapDrawView light2 = LineOnAWall(6, 0x210, BLACK_000);
  CHECK(AutoMapSight_ShapeOfItem(light2, NULL).largestPatch == 6);
  CHECK(AutoMapSight_DecorationTintPatch(light2) == 6);
  CHECK(AutoMapSight_DecorationIsSeen(light2));
  AutoMapDrawView light3 = LineOnAWall(6, 0x100, BLACK_000);
  CHECK(AutoMapSight_ShapeOfItem(light3, NULL).largestPatch == 6);
  CHECK(AutoMapSight_DecorationTintPatch(light3) == 0);
  CHECK(!AutoMapSight_DecorationIsSeen(light3));
  CHECK(!AutoMapSight_DecorationIsSeen(LineOnAWall(3, 0x210, BLACK_000)));
  AutoMapDrawView grey = LineOnAWall(7, 0x111, BLACK_000);
  CHECK(AutoMapSight_DecorationTintPatch(grey) == 0);
  CHECK(!AutoMapSight_DecorationIsSeen(grey));
}

// Inside a large item no background shows: what it covers is its background
static void TheFloorUnderAnItemIsItsBackground(void)
{
  const char *rows[] = {"bBbBbB", "bBbBbB", "bBbBbB"};
  CHECK(AutoMapSight_ShapeOfItem(ViewOf(rows, 3), NULL).largestPatch == 9);
}


// Colour 1 is grey 0x333 (bright) and colour 0 black in BRIGHT_PALETTE; DARK_PALETTE has both black
static void LitPercentCountsPixelsAStepAboveBlack(void)
{
  int counts[16] = {0};
  counts[GREY_333] = 30;
  counts[BLACK] = 70;
  CHECK(AutoMapSight_LitPercent(counts, BRIGHT_PALETTE) == 30);
  CHECK(AutoMapSight_LitPercent(counts, DARK_PALETTE) == 0);
  int none[16] = {0};
  CHECK(AutoMapSight_LitPercent(none, BRIGHT_PALETTE) == 0);
}

static void ReachStopsAtTheFirstDarkRow(void)
{
  int rows[AUTOMAP_REACH_ROWS][16] = {{0}};
  rows[0][GREY_333] = 100;   // F1 lit
  rows[1][BLACK] = 100;      // F2 black
  rows[2][GREY_333] = 100;   // F3 lit, but behind a dark row
  CHECK(AutoMapSight_LightReach(rows, BRIGHT_PALETTE) == 1);
  rows[1][GREY_333] = 100;
  CHECK(AutoMapSight_LightReach(rows, BRIGHT_PALETTE) == 3);
  CHECK(AutoMapSight_LightReach(rows, DARK_PALETTE) == 0);
}

static void ReachNeedsTheShareOfLitPixels(void)
{
  int rows[AUTOMAP_REACH_ROWS][16] = {{0}};
  for (int row = 0; row < AUTOMAP_REACH_ROWS; row++)
  {
    rows[row][GREY_333] = AUTOMAP_REACHED_ROW_PERCENT;
    rows[row][BLACK] = 100 - AUTOMAP_REACHED_ROW_PERCENT;
  }
  CHECK(AutoMapSight_LightReach(rows, BRIGHT_PALETTE) == 3);
  rows[0][GREY_333] -= 1;
  rows[0][BLACK] += 1;
  CHECK(AutoMapSight_LightReach(rows, BRIGHT_PALETTE) == 0);
}

static void CutAtLightReachDropsTheFartherRows(void)
{
  bool cells[AUTOMAP_SIGHT_CELL_COUNT];
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++) cells[i] = true;
  AutoMapSight_CutAtLightReach(1, cells);
  CHECK(cells[15] && cells[16] && cells[17] && cells[18] && cells[19] && cells[20]);
  CHECK(!cells[7] && !cells[8] && !cells[9] && !cells[12] && !cells[13] && !cells[14]);
  AutoMapSight_CutAtLightReach(0, cells);
  CHECK(!cells[17] && cells[20]);
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++) cells[i] = true;
  AutoMapSight_CutAtLightReach(3, cells);
  CHECK(cells[7] && cells[9]);
}

void TestAutoMapSight(void)
{
  ScribblesNeedTwentyFourContrastingPixels();
  CellNumbersFollowTheViewportTables();
  ThePartyCellIsAlwaysVisible();
  TheViewSeesNoFarRowAndNoCellTwoTilesToTheSide();
  AStraightCorridorRevealsItsFloorAndWalls();
  AWallBesideThePartyHidesTheDiagonalTile();
  AWallBesideThePartyStillRevealsTheSliverOfTheDiagonalTile();
  AWallOnTheLeftRevealsTheSliverOfTheFrontLeftTile();
  AWallIsSeenAtItsCornerWhereTheFloorBesideItIsNot();
  AWallDirectlyAheadHidesTheCellsBehindIt();
  AWallStraightAheadLeavesTheDiagonalBehindItInSight();
  AWallStraightAheadLeavesTheFarDiagonalInSight();
  AWallBesideTheWayLeavesTheEdgeOfTheTileBehindItInSight();
  AWallTwoAheadBesideTheWayLeavesTheEdgeOfTheTileBehindItInSight();
  ACorridorShowsTheTilesOnBothSidesWhereItWidens();
  StoneThreeAheadTwoToTheSideIsSeenAtTheEdge();
  ADoorIsSeenByAllItsPixelsThatStandOut();
  ADecorationInAColourTheWallDoesNotShowIsSeen();
  AWallTwoTilesAheadDoesNotRevealTheFarCorner();
  ShutDoorCannotBeSeenThroughUnlessItIsAGrate();
  DoorThatIsNotShutCanBeSeenThrough();
  AShutDoorIsSeenButHidesTheCorridorBehindIt();
  AShutDoorBesideTheWayLeavesTheStoneBesideItInSight();
  AShutDoorStraightAheadHidesAllBehindIt();
  DecorationPlacementsMatchTheViewport();
  FacesShownFollowTheView();
  APixelOneBrightnessStepFromTheWallIsVisible();
  APixelLessThanAStepFromTheWallIsNotVisible();
  ADarkPaletteCanTurnBothColoursBlack();
  ADarkRingOnALitWallStandsOut();
  ADarkRingOnADarkWallDoesNotStandOut();
  AGreySwitchOnAGreyWallDoesNotStandOut();
  ADecorationIsSeenFromEightContrastingPixels();
  ADecorationThatDarkensTheWallAsAWholeIsSeen();
  ATintedLineAStepOffTheWallIsSeen();
  AnItemPixelInTheColourAroundItIsCamouflaged();
  ABlackItemOnABlackFloorIsCamouflaged();
  TouchingPixelsThatStandOutMakeAPatch();
  TheFloorUnderAnItemIsItsBackground();
  LitPercentCountsPixelsAStepAboveBlack();
  ReachStopsAtTheFirstDarkRow();
  ReachNeedsTheShareOfLitPixels();
  CutAtLightReachDropsTheFartherRows();
}
