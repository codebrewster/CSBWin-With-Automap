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

AutoMapRect AutoMapLayout_MemberArea(int positions, int member, int monsterSize, int member0Facing, int cellSize)
{
  AutoMapRect area = {0, 0, cellSize, cellSize};
  if (positions == AUTOMAP_CENTRE_POSITIONS || monsterSize < 0 || monsterSize > 1) return area;
  int corner = (positions >> (2 * member)) & 3;
  int half = cellSize / 2;
  int rest = cellSize - half;
  bool east = corner == 1 || corner == 2;
  bool south = corner == 2 || corner == 3;
  bool splitsNorthFromSouth = (member0Facing & 1) != 0;
  if (monsterSize == 0 || !splitsNorthFromSouth)
  {
    area.x = east ? half : 0;
    area.w = east ? rest : half;
  }
  if (monsterSize == 0 || splitsNorthFromSouth)
  {
    area.y = south ? half : 0;
    area.h = south ? rest : half;
  }
  return area;
}

AutoMapPointF AutoMapLayout_FigureFeet(const AutoMapRect &area, int cellSize)
{
  AutoMapPointF feet;
  feet.x = area.x + area.w * 0.5f;
  bool whole = area.w == cellSize && area.h == cellSize;
  feet.y = whole ? cellSize * 0.72f : area.y + area.h * 0.94f;
  return feet;
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

// Where a line runs, in cells: from its source's centre (a switch: the middle of its face) to its
// target's centre
static void LinePoints(const AutoMapLink &line, float *x0, float *y0, float *x1, float *y1)
{
  *x0 = line.srcX + 0.5f;
  *y0 = line.srcY + 0.5f;
  if (line.srcIsWall)
  {
    if (line.srcSide == AutoMapNorth) *y0 -= 0.5f;
    else if (line.srcSide == AutoMapSouth) *y0 += 0.5f;
    else if (line.srcSide == AutoMapEast) *x0 += 0.5f;
    else if (line.srcSide == AutoMapWest) *x0 -= 0.5f;
  }
  *x1 = line.dstX + 0.5f;
  *y1 = line.dstY + 0.5f;
}

// Two lines on one straight line that share a stretch, not just a point
static bool SharesStretch(const AutoMapLink &a, const AutoMapLink &b)
{
  float ax, ay, bx, by, cx, cy, dx2, dy2;
  LinePoints(a, &ax, &ay, &bx, &by);
  LinePoints(b, &cx, &cy, &dx2, &dy2);
  float dx = bx - ax, dy = by - ay, length = dx * dx + dy * dy;
  if (length <= 0.0f) return false;
  float offC = (cx - ax) * dy - (cy - ay) * dx, offD = (dx2 - ax) * dy - (dy2 - ay) * dx;
  if (std::fabs(offC) > 1e-4f || std::fabs(offD) > 1e-4f) return false;
  float t1 = ((cx - ax) * dx + (cy - ay) * dy) / length;
  float t2 = ((dx2 - ax) * dx + (dy2 - ay) * dy) / length;
  return std::min(1.0f, std::max(t1, t2)) - std::max(0.0f, std::min(t1, t2)) > 1e-4f;
}

static float LineLength(const AutoMapLink &line)
{
  float x0, y0, x1, y1;
  LinePoints(line, &x0, &y0, &x1, &y1);
  return std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
}

std::vector<float> AutoMapLayout_BundleLanes(const std::vector<AutoMapLink> &lines,
                                             const std::vector<bool> &isFeed)
{
  size_t n = lines.size();
  std::vector<float> lanes(n, 0.0f);
  std::vector<size_t> root(n);
  for (size_t i = 0; i < n; i++) root[i] = i;
  for (size_t i = 0; i < n; i++)
  {
    for (size_t j = i + 1; j < n; j++)
    {
      if (!SharesStretch(lines[i], lines[j])) continue;
      size_t a = i, b = j;
      while (root[a] != a) a = root[a];
      while (root[b] != b) b = root[b];
      root[a] = b;
    }
  }
  for (size_t r = 0; r < n; r++)
  {
    std::vector<size_t> bundle;
    for (size_t i = 0; i < n; i++)
    {
      size_t top = i;
      while (root[top] != top) top = root[top];
      if (top == r) bundle.push_back(i);
    }
    if (bundle.size() < 2) continue;
    // Gold before blue, then the shorter first: every bundle sorts alike whichever way it runs
    std::stable_sort(bundle.begin(), bundle.end(), [&](size_t p, size_t q) {
      bool feedP = p < isFeed.size() && isFeed[p], feedQ = q < isFeed.size() && isFeed[q];
      if (feedP != feedQ) return !feedP;
      return LineLength(lines[p]) < LineLength(lines[q]);
    });
    float step = bundle.size() == 2 ? 2.0f : 1.5f;
    for (size_t k = 0; k < bundle.size(); k++)
    {
      // One frame for the bundle: the axis pointing east (or south when upright); a line that
      // runs the other way has its right on the other side
      float x0, y0, x1, y1;
      LinePoints(lines[bundle[k]], &x0, &y0, &x1, &y1);
      float along = std::fabs(x1 - x0) > 1e-4f ? x1 - x0 : y1 - y0;
      float lane = (k - (bundle.size() - 1) / 2.0f) * step;
      lanes[bundle[k]] = along > 0.0f ? lane : -lane;
    }
  }
  return lanes;
}

// How much of a cell (and the tab on its top edge) a tooltip covers: 0, or 1 plus the share
static float CoverOf(const AutoMapRect &tip, int left, int top, int size, int tab)
{
  AutoMapRect cell = {left, top - tab, size, size + tab};
  int ox = std::min(cell.x + cell.w, tip.x + tip.w) - std::max(cell.x, tip.x);
  int oy = std::min(cell.y + cell.h, tip.y + tip.h) - std::max(cell.y, tip.y);
  if (ox <= 0 || oy <= 0) return 0.0f;
  return 1.0f + (float)(ox * oy) / (float)(cell.w * cell.h);
}

AutoMapRect AutoMapLayout_TooltipPlace(int hoverX, int hoverY, const std::vector<AutoMapLink> &lines,
                                       const std::vector<AutoMapLink> &goals, int width, int height,
                                       int originX, int originY, int cellSize, int tabHeight,
                                       const AutoMapRect &area)
{
  // What the hover lights up, by cell: 4 where lines start or end and goals, 1 where they run
  std::vector<int> cellX, cellY, weight;
  struct Add
  {
    std::vector<int> &xs, &ys, &ws;
    void operator()(int x, int y, int w)
    {
      for (size_t i = 0; i < xs.size(); i++)
      {
        if (xs[i] != x || ys[i] != y) continue;
        ws[i] = std::max(ws[i], w);
        return;
      }
      xs.push_back(x);
      ys.push_back(y);
      ws.push_back(w);
    }
  } add = {cellX, cellY, weight};
  for (size_t i = 0; i < lines.size(); i++)
  {
    const AutoMapLink &l = lines[i];
    int steps = std::max(1, 2 * std::max(std::abs(l.dstX - l.srcX), std::abs(l.dstY - l.srcY)));
    for (int s = 1; s < steps; s++)
    {
      add((int)std::lround(l.srcX + (l.dstX - l.srcX) * (double)s / steps),
          (int)std::lround(l.srcY + (l.dstY - l.srcY) * (double)s / steps), 1);
    }
  }
  for (size_t i = 0; i < lines.size(); i++)
  {
    add(lines[i].srcX, lines[i].srcY, 4);
    add(lines[i].dstX, lines[i].dstY, 4);
  }
  for (size_t i = 0; i < goals.size(); i++) add(goals[i].dstX, goals[i].dstY, 4);
  add(hoverX, hoverY, 1000);
  int minX = hoverX, maxX = hoverX, minY = hoverY, maxY = hoverY;
  for (size_t i = 0; i < cellX.size(); i++)
  {
    minX = std::min(minX, cellX[i]);
    maxX = std::max(maxX, cellX[i]);
    minY = std::min(minY, cellY[i]);
    maxY = std::max(maxY, cellY[i]);
  }
  // The old rule as the last tie-break: away from where the lines lead on the whole
  int sumX = 0, sumY = 0;
  for (size_t i = 0; i < lines.size(); i++)
  {
    sumX += lines[i].dstX + lines[i].srcX - 2 * hoverX;
    sumY += lines[i].dstY + lines[i].srcY - 2 * hoverY;
  }
  bool preferLeft = sumX > 0, preferUp = sumY > 0;
  int gap = 4, gapY = gap + tabHeight / 2; // above or below, the gap clears the goal tabs
  int cellLeft = originX + hoverX * cellSize, cellTop = originY + hoverY * cellSize;
  int lefts[2] = {cellLeft - width - gap, originX + minX * cellSize - width - gap};
  int rights[2] = {cellLeft + cellSize + gap, originX + (maxX + 1) * cellSize + gap};
  int ups[2] = {cellTop - height - gapY, originY + minY * cellSize - height - gapY};
  int downs[2] = {cellTop + cellSize + gapY, originY + (maxY + 1) * cellSize + gapY};
  int levels[2] = {cellTop, cellTop + cellSize - height};
  float centreX = cellLeft + cellSize / 2.0f, centreY = cellTop + cellSize / 2.0f;
  AutoMapRect best = {0, 0, width, height};
  float bestScore = 0.0f;
  bool found = false;
  for (int order = 0; order < 4; order++)
  {
    bool left = (order & 1) ? !preferLeft : preferLeft;
    bool up = (order & 2) ? !preferUp : preferUp;
    for (int xi = 0; xi < 2; xi++)
    {
      for (int yi = 0; yi < 4; yi++)
      {
        int x = left ? lefts[xi] : rights[xi];
        int y = yi < 2 ? (up ? ups[yi] : downs[yi]) : levels[yi - 2];
        x = std::max(area.x, std::min(x, area.x + area.w - width));
        y = std::max(area.y, std::min(y, area.y + area.h - height));
        AutoMapRect tip = {x, y, width, height};
        float score = 0.0f;
        for (size_t i = 0; i < cellX.size(); i++)
        {
          int tab = weight[i] >= 4 ? tabHeight / 2 : 0;
          score += weight[i] * CoverOf(tip, originX + cellX[i] * cellSize,
                                       originY + cellY[i] * cellSize, cellSize, tab);
        }
        // How far it is from the hovered cell's centre, far below one hit
        float dx = std::max(0.0f, std::max(x - centreX, centreX - (x + width)));
        float dy = std::max(0.0f, std::max(y - centreY, centreY - (y + height)));
        score += std::sqrt(dx * dx + dy * dy) / 1e6f;
        if (!found || score < bestScore - 1e-9f)
        {
          best = tip;
          bestScore = score;
          found = true;
        }
      }
    }
  }
  return best;
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
