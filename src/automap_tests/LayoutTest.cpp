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

void TestAutoMapLayout(void)
{
  LayoutAtTinyCells();
  PaintSizesGrowWithTheCell();
  RectanglesOverlapOnlyWhenTheyShareAPixel();
}
