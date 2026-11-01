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
  int room = cellSize - 2 * inset;
  // Too small a cell for slots: draw none rather than let them pile up or leave the cell
  if (room < 3)
  {
    AutoMapRect none = {0, 0, 0, 0};
    return none;
  }
  int before = (side + 3) % 4;
  int after = (side + 1) % 4;
  bool besideBefore = decorated[before];
  bool besideAfter = decorated[after];
  int size;
  int margin = (int)std::lround(cellSize * 0.04);
  if (besideBefore && besideAfter) size = room / 3;
  // Slid apart, a slot and its neighbour's must fit side by side between the far end's margin and
  // the neighbour's inset
  else if (besideBefore || besideAfter) size = std::min((int)std::lround(cellSize * 0.44), (cellSize - inset - margin) / 2);
  else size = (int)std::lround(cellSize * 0.56);
  if (decorated[(side + 2) % 4]) size = std::min(size, room / 2);
  size = std::max(1, std::min(size, room));
  // Along the face: centred, or slid to the end away from the one decorated neighbour
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

AutoMapTooltipGrid AutoMapLayout_TooltipGrid(int entries)
{
  static const int COLUMNS = 6;
  static const int MOST = 12;
  AutoMapTooltipGrid grid = {0, 0, 0, 0};
  if (entries <= 0) return grid;
  grid.tiles = entries > MOST ? MOST - 1 : entries;
  grid.plusMore = entries > MOST ? entries - grid.tiles : 0;
  int shown = grid.tiles + (grid.plusMore > 0 ? 1 : 0);
  grid.columns = std::min(COLUMNS, shown);
  grid.rows = (shown + COLUMNS - 1) / COLUMNS;
  return grid;
}

int AutoMapLayout_BarHeight(int pixel)
{
  return 9 * pixel + 8;
}

static std::vector<AutoMapRect> PackBarAs(const std::vector<std::string> &labels, int pixel, bool shorten,
                                          std::vector<std::string> *shownLabels)
{
  static const char *const LONG[] = {"EXPLORED", "LEARNED", "FOLLOW", "VISION", "SPOILERS"};
  static const char *const SHORT[] = {"EXPL", "LRND", "FOL", "VIS", "SPOIL"};
  std::vector<AutoMapRect> rects;
  shownLabels->clear();
  int x = 3 * pixel;
  for (size_t i = 0; i < labels.size(); i++)
  {
    std::string label = labels[i];
    bool group = !label.empty() && label[0] == '|';
    if (group) label.erase(0, 1);
    bool caption = !label.empty() && label[0] == '#';
    if (caption) label.erase(0, 1);
    for (int k = 0; shorten && k < 5; k++)
    {
      if (label == LONG[k]) label = SHORT[k];
    }
    if (i > 0) x += group ? 7 * pixel : pixel;
    int textWidth = label.empty() ? 0 : ((int)label.size() * 4 - 1) * pixel;
    int w = caption ? textWidth : textWidth + 5 * pixel;
    AutoMapRect rect = {x, 2 * pixel, w, 9 * pixel};
    rects.push_back(rect);
    shownLabels->push_back(label);
    x += w;
  }
  return rects;
}

std::vector<AutoMapRect> AutoMapLayout_PackBar(const std::vector<std::string> &labels, int width, int pixel,
                                               std::vector<std::string> *shownLabels)
{
  std::vector<AutoMapRect> rects = PackBarAs(labels, pixel, false, shownLabels);
  if (rects.empty() || rects.back().x + rects.back().w <= width) return rects;
  return PackBarAs(labels, pixel, true, shownLabels);
}

int AutoMapLayout_LevelListTop(int levels, int rows, int focusLevel)
{
  int top = focusLevel - 4;
  top = std::min(top, levels - rows);
  return std::max(0, top);
}

int AutoMapLayout_TypedLevel(const std::string &digits, int levels)
{
  if (digits.empty() || levels <= 0) return -1;
  int level = 0;
  for (size_t i = 0; i < digits.size() && i < 4; i++)
  {
    if (digits[i] < '0' || digits[i] > '9') return -1;
    level = level * 10 + (digits[i] - '0');
  }
  return std::min(level, levels - 1);
}

bool AutoMapLayout_Fits(int width, int height, int cellSize)
{
  return width > 0 && height > 0 && width <= cellSize - 2 && height <= cellSize - 2;
}
