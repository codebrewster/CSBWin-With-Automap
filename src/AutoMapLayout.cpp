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
