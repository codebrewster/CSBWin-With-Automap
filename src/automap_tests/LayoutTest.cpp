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
  static const int SIZES[] = {12, 36, 96};
  for (int s = 0; s < 3; s++)
  {
    int cell = SIZES[s];
    for (int mask = 1; mask < 16; mask++)
    {
      bool decorated[4] = {(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0, (mask & 8) != 0};
      AutoMapRect slots[4];
      for (int side = 0; side < 4; side++)
      {
        if (!decorated[side]) continue;
        slots[side] = AutoMapLayout_WallSlot(decorated, side, cell);
        const AutoMapRect &slot = slots[side];
        CHECK(slot.w > 0 && slot.h > 0);
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

void TestAutoMapLayout(void)
{
  SlotsNeverOverlap();
  LoneSlotIsBig();
  LayoutAtTinyCells();
  PaintSizesGrowWithTheCell();
  RectanglesOverlapOnlyWhenTheyShareAPixel();
}
