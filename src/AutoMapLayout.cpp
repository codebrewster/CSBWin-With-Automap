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

static AutoMapBadge Badge(void)
{
  AutoMapBadge badge;
  badge.facing = 0;
  badge.gold = false;
  return badge;
}

AutoMapBadge AutoMapLayout_PadBadge(AutoMapPadOperator op, int requiredFacing)
{
  AutoMapBadge badge = Badge();
  switch (op)
  {
  case AutoMapPadPartyOrMonster:
    badge.glyphs.push_back(AutoMapGlyphParty);
    badge.glyphs.push_back(AutoMapGlyphClaw);
    break;
  case AutoMapPadParty: badge.glyphs.push_back(AutoMapGlyphParty); break;
  case AutoMapPadPartyFacing:
    badge.glyphs.push_back(AutoMapGlyphParty);
    badge.facing = requiredFacing >= 0 && requiredFacing <= 3 ? requiredFacing : 0;
    badge.gold = true;
    break;
  case AutoMapPadMonster: badge.glyphs.push_back(AutoMapGlyphClaw); break;
  case AutoMapPadObject: badge.glyphs.push_back(AutoMapGlyphItem); break;
  case AutoMapPadPossession:
    badge.glyphs.push_back(AutoMapGlyphParty);
    badge.glyphs.push_back(AutoMapGlyphItem);
    break;
  default: break;
  }
  return badge;
}

AutoMapBadge AutoMapLayout_TeleporterBadge(int scope)
{
  AutoMapBadge badge = Badge();
  if (scope == 0) badge.glyphs.push_back(AutoMapGlyphBox);
  if (scope == 1) badge.glyphs.push_back(AutoMapGlyphClaw);
  if (scope == 2)
  {
    badge.glyphs.push_back(AutoMapGlyphParty);
    badge.glyphs.push_back(AutoMapGlyphBox);
  }
  return badge;
}

AutoMapPointF AutoMapLayout_LineEnd(AutoMapPointF from, int targetX, int targetY, int cellSize)
{
  float centreX = targetX * cellSize + cellSize / 2.0f;
  float centreY = targetY * cellSize + cellSize / 2.0f;
  float half = cellSize / 2.0f - cellSize * 0.18f;
  float dx = from.x - centreX;
  float dy = from.y - centreY;
  float reach = 1.0f;
  if (std::fabs(dx) > 0.0f) reach = std::min(reach, half / std::fabs(dx));
  if (std::fabs(dy) > 0.0f) reach = std::min(reach, half / std::fabs(dy));
  AutoMapPointF end = {centreX + dx * reach, centreY + dy * reach};
  return end;
}

int AutoMapLayout_TooltipSide(int hoverX, int hoverY, const std::vector<AutoMapLink> &lines)
{
  int sumX = 0;
  int sumY = 0;
  for (size_t i = 0; i < lines.size(); i++)
  {
    sumX += lines[i].dstX + lines[i].srcX - 2 * hoverX;
    sumY += lines[i].dstY + lines[i].srcY - 2 * hoverY;
  }
  return (sumX > 0 ? AUTOMAP_TOOLTIP_LEFT : 0) | (sumY > 0 ? AUTOMAP_TOOLTIP_UP : 0);
}
