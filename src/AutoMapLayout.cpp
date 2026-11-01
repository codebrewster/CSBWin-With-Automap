// AutoMapLayout.cpp : see AutoMapLayout.h
#include <algorithm>
#include <cmath>

#include "AutoMapLayout.h"

static const int SMALLEST_CELL_WITH_BADGE = 20;

bool AutoMapLayout_Overlap(const AutoMapRect &a, const AutoMapRect &b)
{
  if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) return false;
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

int AutoMapLayout_BadgeSize(int cellSize)
{
  if (cellSize < SMALLEST_CELL_WITH_BADGE) return 0;
  return std::max(10, (int)std::lround(cellSize * 0.36));
}

int AutoMapLayout_PlatePixel(int cellSize)
{
  return std::max(1, cellSize / 18);
}

int AutoMapLayout_LineWidth(int cellSize)
{
  return std::max(2, cellSize / 12);
}

int AutoMapLayout_HeadSize(int cellSize)
{
  return std::max(7, (int)(cellSize * 0.26));
}

int AutoMapLayout_FaceLine(int cellSize)
{
  return std::max(2, cellSize / 14);
}

AutoMapRect AutoMapLayout_WallSlot(const bool decorated[4], int side, int cellSize)
{
  int inset = AutoMapLayout_FaceLine(cellSize);
  int room = std::max(1, cellSize - 2 * inset);
  int before = (side + 3) % 4;
  int after = (side + 1) % 4;
  bool besideBefore = decorated[before];
  bool besideAfter = decorated[after];
  int size;
  if (besideBefore && besideAfter) size = room / 3;
  else if (besideBefore || besideAfter) size = (int)std::lround(cellSize * 0.44);
  else size = (int)std::lround(cellSize * 0.56);
  if (decorated[(side + 2) % 4]) size = std::min(size, room / 2);
  size = std::max(1, std::min(size, room));
  // Along the face: centred, or slid to the end away from the one decorated neighbour
  int margin = (int)std::lround(cellSize * 0.04);
  int along = (cellSize - size) / 2;
  if (besideBefore != besideAfter)
  {
    // Clockwise order north, east, south, west: the neighbour "after" north is east, whose corner
    // lies at the high x end of the north face; for east "after" is south, at the high y end
    bool awayFromHighEnd = (side == 0 || side == 1) ? besideAfter : besideBefore;
    along = awayFromHighEnd ? margin : cellSize - margin - size;
  }
  AutoMapRect slot = {along, inset, size, size};
  if (side == 1) slot = {cellSize - inset - size, along, size, size};
  if (side == 2) slot = {along, cellSize - inset - size, size, size};
  if (side == 3) slot = {inset, along, size, size};
  return slot;
}

AutoMapRect AutoMapLayout_SlotCorner(const AutoMapRect &slot, bool right)
{
  int size = std::min(std::min(slot.w, slot.h), std::max(8, slot.w / 2));
  AutoMapRect corner = {right ? slot.x + slot.w - size : slot.x, slot.y + slot.h - size, size, size};
  return corner;
}
