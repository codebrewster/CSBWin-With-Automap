// LayoutTest.cpp : the map's geometry and drawing decisions, without SDL
#include "TestSupport.h"
#include "../AutoMapLayout.h"

// At the whole level of a big dungeon a cell is a few pixels: badges drop out, plates and lines
// keep their smallest sizes
static void LayoutAtTinyCells(void)
{
  CHECK(AutoMapLayout_BadgeSize(12) == 0);
  CHECK(AutoMapLayout_BadgeSize(0) == 0);
  CHECK(AutoMapLayout_PlatePixel(0) == 1);
  CHECK(AutoMapLayout_LineWidth(0) == 2);
  CHECK(AutoMapLayout_HeadSize(0) == 7);
}

static void PaintSizesGrowWithTheCell(void)
{
  CHECK(AutoMapLayout_BadgeSize(20) == 10);
  CHECK(AutoMapLayout_BadgeSize(40) == 14);
  CHECK(AutoMapLayout_PlatePixel(10) == 1);
  CHECK(AutoMapLayout_PlatePixel(72) == 4);
  CHECK(AutoMapLayout_LineWidth(8) == 2);
  CHECK(AutoMapLayout_LineWidth(60) == 5);
  CHECK(AutoMapLayout_HeadSize(100) == 26);
}

static void RectanglesOverlapOnlyWhenTheyShareAPixel(void)
{
  AutoMapRect a = {0, 0, 4, 4};
  AutoMapRect b = {4, 0, 4, 4};
  AutoMapRect c = {3, 3, 4, 4};
  AutoMapRect empty = {1, 1, 0, 5};
  CHECK(!AutoMapLayout_Overlap(a, b));
  CHECK(AutoMapLayout_Overlap(a, c));
  CHECK(!AutoMapLayout_Overlap(a, empty));
}

// Every combination of decorated faces, at a tiny, a normal and a big cell: each slot stays inside
// the cell, no two slots share a pixel, each corner icon stays inside its slot
static void SlotsNeverOverlap(void)
{
  for (int cell = 1; cell <= 200; cell++)
  {
    for (int mask = 1; mask < 16; mask++)
    {
      bool decorated[4] = {(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0, (mask & 8) != 0};
      AutoMapRect slots[4];
      for (int side = 0; side < 4; side++)
      {
        if (!decorated[side]) continue;
        slots[side] = AutoMapLayout_WallSlot(decorated, side, cell);
        const AutoMapRect &slot = slots[side];
        // Too small a cell draws no slot at all
        if (slot.w <= 0 || slot.h <= 0) continue;
        CHECK(slot.x >= 0 && slot.y >= 0 && slot.x + slot.w <= cell && slot.y + slot.h <= cell);
        for (int right = 0; right < 2; right++)
        {
          AutoMapRect corner = AutoMapLayout_SlotCorner(slot, right != 0);
          CHECK(corner.x >= slot.x && corner.y >= slot.y);
          CHECK(corner.x + corner.w <= slot.x + slot.w && corner.y + corner.h <= slot.y + slot.h);
        }
        for (int other = 0; other < side; other++)
        {
          if (decorated[other]) CHECK(!AutoMapLayout_Overlap(slots[side], slots[other]));
        }
      }
    }
  }
}

// A face of its own gets a big slot, flush against the face and centred along it
static void LoneSlotIsBig(void)
{
  bool north[4] = {true, false, false, false};
  AutoMapRect slot = AutoMapLayout_WallSlot(north, 0, 100);
  CHECK(slot.x == 22 && slot.y == 7 && slot.w == 56 && slot.h == 56);
  bool east[4] = {false, true, false, false};
  AutoMapRect onEast = AutoMapLayout_WallSlot(east, 1, 100);
  CHECK(onEast.x == 100 - 7 - 56 && onEast.y == 22);
}

static bool GlyphsAre(const AutoMapBadge &badge, int count, AutoMapBadgeGlyph first, AutoMapBadgeGlyph second)
{
  if ((int)badge.glyphs.size() != count) return false;
  if (count >= 1 && badge.glyphs[0] != first) return false;
  if (count >= 2 && badge.glyphs[1] != second) return false;
  return true;
}

// Pads and teleporters tell who sets them off with the same few glyphs
static void BadgesSpeakOneVocabulary(void)
{
  CHECK(GlyphsAre(AutoMapLayout_PadBadge(AutoMapPadPartyOrMonster, -1), 2, AutoMapGlyphParty, AutoMapGlyphClaw));
  AutoMapBadge party = AutoMapLayout_PadBadge(AutoMapPadParty, -1);
  CHECK(GlyphsAre(party, 1, AutoMapGlyphParty, AutoMapGlyphParty));
  CHECK(party.facing == 0 && !party.gold);
  AutoMapBadge facing = AutoMapLayout_PadBadge(AutoMapPadPartyFacing, 2);
  CHECK(GlyphsAre(facing, 1, AutoMapGlyphParty, AutoMapGlyphParty));
  CHECK(facing.facing == 2 && facing.gold);
  CHECK(AutoMapLayout_PadBadge(AutoMapPadPartyFacing, 9).facing == 0);
  CHECK(GlyphsAre(AutoMapLayout_PadBadge(AutoMapPadMonster, -1), 1, AutoMapGlyphClaw, AutoMapGlyphClaw));
  CHECK(GlyphsAre(AutoMapLayout_PadBadge(AutoMapPadObject, -1), 1, AutoMapGlyphItem, AutoMapGlyphItem));
  CHECK(GlyphsAre(AutoMapLayout_PadBadge(AutoMapPadPossession, -1), 2, AutoMapGlyphParty, AutoMapGlyphItem));
  CHECK(AutoMapLayout_PadBadge(AutoMapPadAnything, -1).glyphs.empty());
  CHECK(GlyphsAre(AutoMapLayout_TeleporterBadge(0), 1, AutoMapGlyphBox, AutoMapGlyphBox));
  CHECK(GlyphsAre(AutoMapLayout_TeleporterBadge(1), 1, AutoMapGlyphClaw, AutoMapGlyphClaw));
  CHECK(GlyphsAre(AutoMapLayout_TeleporterBadge(2), 2, AutoMapGlyphParty, AutoMapGlyphBox));
  CHECK(AutoMapLayout_TeleporterBadge(3).glyphs.empty());
  CHECK(AutoMapLayout_TeleporterBadge(7).glyphs.empty());
}

// A line ends where it enters the target cell, inset a little, not at its centre
static void LineStopsAtTheTargetEdge(void)
{
  AutoMapPointF from = {20.0f, 20.0f};
  AutoMapPointF end = AutoMapLayout_LineEnd(from, 3, 0, 40);
  CHECK(end.x > 127.1f && end.x < 127.3f);
  CHECK(end.y > 19.9f && end.y < 20.1f);
  AutoMapPointF diagonal = AutoMapLayout_LineEnd(from, 2, 2, 40);
  CHECK(diagonal.x > 87.1f && diagonal.x < 87.3f && diagonal.y > 87.1f && diagonal.y < 87.3f);
}

static AutoMapRect TipPlace(int hoverX, int hoverY, const std::vector<AutoMapLink> &lines,
                            const std::vector<AutoMapLink> &goals, int w, int h, int areaW, int areaH)
{
  AutoMapRect area = {0, 0, areaW, areaH};
  return AutoMapLayout_TooltipPlace(hoverX, hoverY, lines, goals, w, h, 0, 0, 40, 16, area);
}

// Cells of 40 pixels from 0, 0: does the rect cover any pixel of cell (x, y)?
static bool Covers(const AutoMapRect &r, int x, int y)
{
  AutoMapRect cell = {x * 40, y * 40, 40, 40};
  return AutoMapLayout_Overlap(r, cell);
}

// With room all round, the tooltip opens away from the lines' other ends, off the hovered cell
static void TooltipOpensAwayFromTheLines(void)
{
  std::vector<AutoMapLink> none;
  // Nothing lit: right beside the cell, level with it (the nearest free place)
  AutoMapRect free = TipPlace(5, 5, none, none, 100, 60, 1000, 1000);
  CHECK(free.x == 244 && free.y == 200);
  std::vector<AutoMapLink> lines;
  lines.push_back(AutoMapLink(5, 5, 9, 5));
  lines.push_back(AutoMapLink(5, 5, 6, 8));
  AutoMapRect away = TipPlace(5, 5, lines, none, 100, 60, 1000, 1000);
  CHECK(away.x + away.w <= 200); // to the left, away from the lines
  CHECK(!Covers(away, 5, 5) && !Covers(away, 9, 5) && !Covers(away, 6, 8));
}

// Lines left, right and down (L4 (21, 6)): the sum of the directions leaves only "down", but the
// free corner is up and left, and no goal or its tab is covered
static void TooltipFindsTheFreeCorner(void)
{
  std::vector<AutoMapLink> lines;
  lines.push_back(AutoMapLink(5, 5, 4, 5));
  lines.push_back(AutoMapLink(5, 5, 6, 5));
  lines.push_back(AutoMapLink(5, 5, 5, 6));
  AutoMapRect place = TipPlace(5, 5, lines, lines, 100, 60, 1000, 1000);
  for (size_t i = 0; i < lines.size(); i++) CHECK(!Covers(place, lines[i].dstX, lines[i].dstY));
  CHECK(!Covers(place, 5, 5));
  // The tab of (4, 5) sticks 8 pixels above its row: the tooltip ends above it
  CHECK(place.y + place.h <= 5 * 40 - 8);
}

// No room above or below (three rows shown): the tooltip goes beside the lit group, not over it
static void TooltipGoesBesideTheLitGroup(void)
{
  std::vector<AutoMapLink> lines;
  lines.push_back(AutoMapLink(5, 1, 4, 1));
  lines.push_back(AutoMapLink(5, 1, 6, 1));
  lines.push_back(AutoMapLink(5, 1, 5, 2));
  AutoMapRect place = TipPlace(5, 1, lines, lines, 100, 60, 1000, 120);
  CHECK(place.y >= 0 && place.y + place.h <= 120);
  for (size_t i = 0; i < lines.size(); i++) CHECK(!Covers(place, lines[i].dstX, lines[i].dstY));
  CHECK(!Covers(place, 5, 1));
}

// Lines on one straight line spread across lanes; lines that only meet stay on their axis
static void BundlesSpreadOverlappingLines(void)
{
  // A gold line (0,0)->(1,0) and a blue one (2,0)->(0,0) over the same stretch: a line width
  // either side, gold first. Seen along the axis pointing east (+ is south): gold north, blue
  // south. In each line's own terms (to the right of its direction) both are -1: gold runs east
  // and north is its left; blue runs west and south is its left.
  std::vector<AutoMapLink> pair;
  pair.push_back(AutoMapLink(0, 0, 1, 0));
  pair.push_back(AutoMapLink(2, 0, 0, 0));
  std::vector<bool> feed;
  feed.push_back(false);
  feed.push_back(true);
  std::vector<float> lanes = AutoMapLayout_BundleLanes(pair, feed);
  CHECK(lanes.size() == 2 && lanes[0] == -1.0f && lanes[1] == -1.0f);
  // Three on one line: 1.5 apart, the shortest first
  std::vector<AutoMapLink> three;
  three.push_back(AutoMapLink(0, 0, 3, 0));
  three.push_back(AutoMapLink(1, 0, 3, 0));
  three.push_back(AutoMapLink(2, 0, 3, 0));
  lanes = AutoMapLayout_BundleLanes(three, std::vector<bool>(3, false));
  CHECK(lanes.size() == 3 && lanes[2] == -1.5f && lanes[1] == 0.0f && lanes[0] == 1.5f);
  // Meeting at a point only, or crossing: no lanes
  std::vector<AutoMapLink> apart;
  apart.push_back(AutoMapLink(0, 0, 1, 0));
  apart.push_back(AutoMapLink(1, 0, 2, 0));
  apart.push_back(AutoMapLink(1, 1, 1, -1));
  lanes = AutoMapLayout_BundleLanes(apart, std::vector<bool>(3, false));
  CHECK(lanes.size() == 3 && lanes[0] == 0.0f && lanes[1] == 0.0f && lanes[2] == 0.0f);
}

// The tooltip's icon grid: six to a row, two rows; with more kinds the twelfth tile reads +N
static void TooltipGridHasTwoRowsOfSix(void)
{
  AutoMapTooltipGrid five = AutoMapLayout_TooltipGrid(5);
  CHECK(five.tiles == 5 && five.columns == 5 && five.rows == 1 && five.plusMore == 0);
  AutoMapTooltipGrid twelve = AutoMapLayout_TooltipGrid(12);
  CHECK(twelve.tiles == 12 && twelve.columns == 6 && twelve.rows == 2 && twelve.plusMore == 0);
  AutoMapTooltipGrid fourteen = AutoMapLayout_TooltipGrid(14);
  CHECK(fourteen.tiles == 11 && fourteen.columns == 6 && fourteen.rows == 2 && fourteen.plusMore == 3);
  AutoMapTooltipGrid none = AutoMapLayout_TooltipGrid(0);
  CHECK(none.tiles == 0 && none.columns == 0 && none.rows == 0 && none.plusMore == 0);
}

static std::vector<std::string> BarLabels(void)
{
  const char *labels[] = {"<", "L10 ~", ">", "|FOLLOW", "|#VISION", "FOG", "EXPLORED", "ALL", "TINT",
                          "|#SPOILERS", "SEEN", "LEARNED", "ALL", "|SHOW", "|/", "?", "+"};
  return std::vector<std::string>(labels, labels + sizeof(labels) / sizeof(labels[0]));
}

static bool PackedInOrderWithoutOverlap(const std::vector<AutoMapRect> &rects)
{
  for (size_t i = 1; i < rects.size(); i++)
  {
    if (rects[i].x < rects[i - 1].x + rects[i - 1].w) return false;
    if (AutoMapLayout_Overlap(rects[i], rects[i - 1])) return false;
  }
  return true;
}

// The bar never drops an item: a narrow window shortens the long labels first, then the bar
// reaches past the window and scrolls
static void BarFitsNarrowWindow(void)
{
  std::vector<std::string> shown;
  std::vector<AutoMapRect> wide = AutoMapLayout_PackBar(BarLabels(), 1200, 2, &shown);
  CHECK(wide.size() == BarLabels().size() && shown.size() == wide.size());
  CHECK(PackedInOrderWithoutOverlap(wide));
  CHECK(shown[6] == "EXPLORED" && shown[4] == "VISION" && shown[3] == "FOLLOW");
  CHECK(wide.back().x + wide.back().w <= 1200);
  std::vector<AutoMapRect> narrow = AutoMapLayout_PackBar(BarLabels(), 300, 2, &shown);
  CHECK(narrow.size() == BarLabels().size() && shown.size() == narrow.size());
  CHECK(PackedInOrderWithoutOverlap(narrow));
  CHECK(shown[6] == "EXPL" && shown[11] == "LRND" && shown[3] == "FOL");
  CHECK(narrow.back().x + narrow.back().w < wide.back().x + wide.back().w);
}

// The level list opens with the shown level four rows down, and stays within the dungeon; typing
// a level beyond the last goes to the last
static void LevelListClamps(void)
{
  CHECK(AutoMapLayout_LevelListTop(64, 12, 10) == 6);
  CHECK(AutoMapLayout_LevelListTop(64, 12, 62) == 52);
  CHECK(AutoMapLayout_LevelListTop(64, 12, 2) == 0);
  CHECK(AutoMapLayout_LevelListTop(1, 12, 0) == 0);
  CHECK(AutoMapLayout_LevelListTop(14, 12, 13) == 2);
  CHECK(AutoMapLayout_TypedLevel("99", 14) == 13);
  CHECK(AutoMapLayout_TypedLevel("4", 14) == 4);
  CHECK(AutoMapLayout_TypedLevel("07", 14) == 7);
  CHECK(AutoMapLayout_TypedLevel("", 14) == -1);
  CHECK(AutoMapLayout_TypedLevel("3", 0) == -1);
}

// A plate, count or badge that does not fit inside its cell is left out rather than spill over
static void PlatesKeepInsideTheirCell(void)
{
  CHECK(AutoMapLayout_Fits(17, 9, 40));
  CHECK(!AutoMapLayout_Fits(17, 9, 12));
  CHECK(!AutoMapLayout_Fits(10, 9, 10));
  CHECK(AutoMapLayout_Fits(8, 8, 10));
  CHECK(AutoMapLayout_Fits(0, 0, 0) == false);
}

static bool SameRect(const AutoMapRect &a, int x, int y, int w, int h)
{
  return a.x == x && a.y == y && a.w == w && a.h == h;
}

// Small monsters stand in the quarter of their own two bits; 255 is the centre
static void SmallMonstersStandInTheirQuarter(void)
{
  int positions = 0 | (1 << 2) | (2 << 4) | (3 << 6);
  CHECK(SameRect(AutoMapLayout_MemberArea(positions, 0, 0, 0, 40), 0, 0, 20, 20));
  CHECK(SameRect(AutoMapLayout_MemberArea(positions, 1, 0, 0, 40), 20, 0, 20, 20));
  CHECK(SameRect(AutoMapLayout_MemberArea(positions, 2, 0, 0, 40), 20, 20, 20, 20));
  CHECK(SameRect(AutoMapLayout_MemberArea(positions, 3, 0, 0, 40), 0, 20, 20, 20));
  // Two on the west side: north-west and south-west
  int west = 0 | (3 << 2);
  CHECK(SameRect(AutoMapLayout_MemberArea(west, 1, 0, 0, 40), 0, 20, 20, 20));
  // A lone one the AI moved to the south-east stays there
  CHECK(SameRect(AutoMapLayout_MemberArea(2, 0, 0, 0, 40), 20, 20, 20, 20));
  CHECK(SameRect(AutoMapLayout_MemberArea(AUTOMAP_CENTRE_POSITIONS, 0, 0, 0, 40), 0, 0, 40, 40));
}

// A medium monster's half holds its corner and runs along member 0's facing (OrdinalOfMonsterAtPosition)
static void MediumMonstersTakeTheHalfAlongTheirFacing(void)
{
  // facing east (1) or west (3): corners 0,1 north, 2,3 south
  CHECK(SameRect(AutoMapLayout_MemberArea(0, 0, 1, 1, 40), 0, 0, 40, 20));
  CHECK(SameRect(AutoMapLayout_MemberArea(1, 0, 1, 3, 40), 0, 0, 40, 20));
  CHECK(SameRect(AutoMapLayout_MemberArea(2, 0, 1, 1, 40), 0, 20, 40, 20));
  CHECK(SameRect(AutoMapLayout_MemberArea(3, 0, 1, 3, 40), 0, 20, 40, 20));
  // facing north (0) or south (2): corners 0,3 west, 1,2 east
  CHECK(SameRect(AutoMapLayout_MemberArea(0, 0, 1, 0, 40), 0, 0, 20, 40));
  CHECK(SameRect(AutoMapLayout_MemberArea(3, 0, 1, 2, 40), 0, 0, 20, 40));
  CHECK(SameRect(AutoMapLayout_MemberArea(1, 0, 1, 0, 40), 20, 0, 20, 40));
  CHECK(SameRect(AutoMapLayout_MemberArea(2, 0, 1, 2, 40), 20, 0, 20, 40));
}

// A later member's half also runs along member 0's facing (the facing passed in), not its own
static void LaterMediumMembersUseMemberZerosFacing(void)
{
  CHECK(SameRect(AutoMapLayout_MemberArea(0 | (2 << 2), 1, 1, 1, 40), 0, 20, 40, 20));
}

static void LargeAndOddSizesFillTheCell(void)
{
  CHECK(SameRect(AutoMapLayout_MemberArea(1, 0, 2, 0, 40), 0, 0, 40, 40));
  CHECK(SameRect(AutoMapLayout_MemberArea(1, 0, 3, 0, 40), 0, 0, 40, 40));
  CHECK(SameRect(AutoMapLayout_MemberArea(1, 0, -1, 0, 40), 0, 0, 40, 40));
}

// Feet: back row above front row, the centre between them, x in the middle of the area
static void FeetStandAtTheBottomOfTheArea(void)
{
  AutoMapPointF back = AutoMapLayout_FigureFeet(AutoMapLayout_MemberArea(0, 0, 0, 0, 40), 40);
  AutoMapPointF front = AutoMapLayout_FigureFeet(AutoMapLayout_MemberArea(3, 0, 0, 0, 40), 40);
  AutoMapPointF centre = AutoMapLayout_FigureFeet(AutoMapLayout_MemberArea(255, 0, 0, 0, 40), 40);
  CHECK(back.x == 10.0f && front.x == 10.0f && centre.x == 20.0f);
  CHECK(back.y < centre.y && centre.y < front.y);
  CHECK(back.y <= 20.0f && front.y <= 40.0f);
}

void TestAutoMapLayout(void)
{
  PlatesKeepInsideTheirCell();
  LevelListClamps();
  BarFitsNarrowWindow();
  TooltipGridHasTwoRowsOfSix();
  LineStopsAtTheTargetEdge();
  TooltipOpensAwayFromTheLines();
  TooltipFindsTheFreeCorner();
  TooltipGoesBesideTheLitGroup();
  BundlesSpreadOverlappingLines();
  BadgesSpeakOneVocabulary();
  SlotsNeverOverlap();
  LoneSlotIsBig();
  LayoutAtTinyCells();
  PaintSizesGrowWithTheCell();
  RectanglesOverlapOnlyWhenTheyShareAPixel();
  SmallMonstersStandInTheirQuarter();
  MediumMonstersTakeTheHalfAlongTheirFacing();
  LaterMediumMembersUseMemberZerosFacing();
  LargeAndOddSizesFillTheCell();
  FeetStandAtTheBottomOfTheArea();
}
