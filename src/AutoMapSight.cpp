// AutoMapSight.cpp : the line-of-sight rule over the cells the 3D view draws in front of the party.
#include "AutoMapSight.h"

#include <algorithm>
#include <cstddef>

// The 3D view's own cell numbers, in the order Viewport.cpp draws them (relativeX/relativeY)
static const AutoMapSightOffset OFFSETS[AUTOMAP_SIGHT_CELL_COUNT] =
{
  {4, -2}, {4, +2}, {3, -2}, {3, +2}, {4, -1}, {4, +1}, {4, 0},
  {3, -1}, {3, +1}, {3, 0},
  {2, -2}, {2, +2}, {2, -1}, {2, +1}, {2, 0},
  {1, -1}, {1, +1}, {1, 0},
  {0, -1}, {0, +1}, {0, 0}
};

// The cells the 3D view can show. The party never sees more than three tiles ahead or more
// than one tile to either side, but for the stone three ahead and two to a side: the far row
// draws nothing at all, the two-ahead cells two to a side draw nothing, and the three-ahead ones
// (2 and 3) a wall picture at the screen's edge when they are stone (StdDrawF3L2Stone,
// StdDrawF3R2Stone) and nothing else, so no floor, door or object on them is ever seen. Cells
// 4,5,6 draw objects over a bare background with no floor or wall to stand on. The party cell,
// 20, is always drawn.
static const bool CELL_IS_DRAWN[AUTOMAP_SIGHT_CELL_COUNT] =
{
  false, false, true,  true,  false, false, false, true,  true,  true,
  false, false, true,  true,  true,  true,  true,  true,  true,  true,
  true
};

static bool CellShowsOnlyStone(int cell)
{
  return cell == 2 || cell == 3;
}

AutoMapSightOffset AutoMapSight_OffsetOfCell(int cellNumber)
{
  return OFFSETS[cellNumber];
}

// The rule the engine uses for what a monster sees through a door (Monster.cpp)
bool AutoMapSight_CanSeeThroughDoor(const AutoMapSightDoor &door)
{
  bool isShut = door.state == AutoMapDoorClosed || door.state == AutoMapDoorAlmostClosed;
  return !isShut || door.isSeeThroughType;
}

struct AutoMapSightRectangle
{
  double forwardLow;
  double forwardHigh;
  double sideLow;
  double sideHigh;
};

static AutoMapSightRectangle CellRectangle(const AutoMapSightOffset &offset)
{
  AutoMapSightRectangle rectangle;
  rectangle.forwardLow  = offset.stepsForward - 0.5;
  rectangle.forwardHigh = offset.stepsForward + 0.5;
  rectangle.sideLow     = offset.stepsRight - 0.5;
  rectangle.sideHigh    = offset.stepsRight + 0.5;
  return rectangle;
}

// Liang-Barsky clipping of the segment (ax,ay)-(bx,by) against a rectangle. Returns false when
// they do not meet at all; otherwise tEnter/tExit bound the overlap within [0,1].
static bool SegmentMeetsRectangle(double ax, double ay, double bx, double by,
                                  const AutoMapSightRectangle &rectangle,
                                  double *tEnter, double *tExit)
{
  double lowerT = 0.0;
  double upperT = 1.0;
  double dx = bx - ax;
  double dy = by - ay;
  const double p[4] = {-dx, dx, -dy, dy};
  const double q[4] = {ax - rectangle.forwardLow, rectangle.forwardHigh - ax,
                       ay - rectangle.sideLow,    rectangle.sideHigh - ay};
  for (int i = 0; i < 4; i++)
  {
    if (p[i] == 0.0)
    {
      if (q[i] < 0.0) return false;
    }
    else
    {
      double t = q[i] / p[i];
      if (p[i] < 0.0)
      {
        if (t > lowerT) lowerT = t;
      }
      else
      {
        if (t < upperT) upperT = t;
      }
      if (lowerT > upperT) return false;
    }
  }
  *tEnter = lowerT;
  *tExit = upperT;
  return true;
}

// A monster stands at the centre of its cell: the ray from the party's cell centre to it is
// stopped by any blocking cell it touches, even at a corner
static bool TheRayReachesTheCentre(int targetCell, const bool *canSeeThrough)
{
  const AutoMapSightOffset &target = OFFSETS[targetCell];
  for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++)
  {
    if (cell == targetCell) continue;
    if (cell == AUTOMAP_SIGHT_PARTY_CELL) continue;
    if (!CELL_IS_DRAWN[cell]) continue;
    if (canSeeThrough[cell]) continue;
    double tEnter, tExit;
    if (SegmentMeetsRectangle(0.0, 0.0, target.stepsForward, target.stepsRight,
                              CellRectangle(OFFSETS[cell]), &tEnter, &tExit))
    {
      return false;
    }
  }
  return true;
}

// The screen columns (first, last) the 3D view paints a cell's wall in: the size rectangles of
// the wall pictures (Data.h wallRectangles, the game's graphic 558), F3L1 0..83 to F0R1 192..223.
// A floor cell takes the columns its wall would: its own floor and what stands on it lie within.
// The viewport is 224 columns wide; the never drawn cells have none.
static const int SCREEN_COLUMNS[AUTOMAP_SIGHT_CELL_COUNT][2] =
{
  {0, -1}, {0, -1}, {0, 15}, {208, 223}, {0, -1}, {0, -1}, {0, -1},
  {0, 83}, {139, 223}, {74, 149},
  {0, -1}, {0, -1}, {0, 74}, {149, 223}, {60, 163},
  {0, 63}, {160, 223}, {32, 191},
  {0, 31}, {192, 223}, {0, 223}
};
// The columns a shut door facing the party is painted in (Data.h DoorRectsF3L1 .. DoorRectsF1R1,
// the closed door). Straight ahead its frame fills the rest of the wall's columns; beside the way
// no frame is drawn, or one the strip of stone three ahead shows through; beside the party the
// 3D view draws no door at all.
static const int DOOR_SCREEN_COLUMNS[AUTOMAP_SIGHT_CELL_COUNT][2] =
{
  {0, -1}, {0, -1}, {0, 15}, {208, 223}, {0, -1}, {0, -1}, {0, -1},
  {24, 71}, {150, 197}, {74, 149},
  {0, -1}, {0, -1}, {0, 63}, {160, 223}, {60, 163},
  {0, 31}, {192, 223}, {32, 191},
  {0, -1}, {0, -1}, {0, 223}
};
static const int VIEWPORT_COLUMNS = 224;

// The 3D view paints its cells in the order of their numbers, far rows first and in each row the
// sides before the middle, and a wall covers its columns from floor to ceiling. So a cell is on
// screen where no blocking cell painted after it covers its columns: past a wall straight ahead
// the strip of the column beside it shows, and past a wall beside the party the inner edge of
// the tile in front of it.
static int ColumnsOnScreen(int targetCell, const bool *canSeeThrough, const bool *isShutDoor)
{
  bool covered[VIEWPORT_COLUMNS];
  for (int column = 0; column < VIEWPORT_COLUMNS; column++) covered[column] = false;
  for (int cell = targetCell + 1; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++)
  {
    if (cell == AUTOMAP_SIGHT_PARTY_CELL || !CELL_IS_DRAWN[cell] || canSeeThrough[cell]) continue;
    const int *columns = isShutDoor[cell] ? DOOR_SCREEN_COLUMNS[cell] : SCREEN_COLUMNS[cell];
    for (int column = columns[0]; column <= columns[1]; column++)
    {
      covered[column] = true;
    }
  }
  int shown = 0;
  for (int column = SCREEN_COLUMNS[targetCell][0]; column <= SCREEN_COLUMNS[targetCell][1]; column++)
  {
    if (!covered[column]) shown++;
  }
  return shown;
}

static bool TheCellIsOnScreen(int cell, const bool *canSeeThrough, const bool *isShutDoor)
{
  if (CellShowsOnlyStone(cell) && canSeeThrough[cell]) return false;
  return ColumnsOnScreen(cell, canSeeThrough, isShutDoor) >= AUTOMAP_SEEN_SCREEN_COLUMNS;
}

void AutoMapSight_FindVisibleCells(const bool *canSeeThrough, const bool *isShutDoor, bool *isVisible)
{
  for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++) isVisible[cell] = false;
  isVisible[AUTOMAP_SIGHT_PARTY_CELL] = true;
  for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++)
  {
    if (cell == AUTOMAP_SIGHT_PARTY_CELL || !CELL_IS_DRAWN[cell]) continue;
    bool onScreen = TheCellIsOnScreen(cell, canSeeThrough, isShutDoor);
    isVisible[cell] = canSeeThrough[cell] ? onScreen && TheRayReachesTheCentre(cell, canSeeThrough)
                                          : onScreen;
  }
}

void AutoMapSight_FindExploredCells(const bool *canSeeThrough, const bool *isShutDoor, bool *isExplored)
{
  for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++) isExplored[cell] = false;
  isExplored[AUTOMAP_SIGHT_PARTY_CELL] = true;
  for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++)
  {
    if (cell == AUTOMAP_SIGHT_PARTY_CELL || !CELL_IS_DRAWN[cell]) continue;
    isExplored[cell] = TheCellIsOnScreen(cell, canSeeThrough, isShutDoor);
  }
}

// Read from the 25 DrawWallDecoration call sites in Viewport.cpp
static const int DECORATION_PLACEMENTS[13][2] =
{
  {7, 0}, {8, 2}, {7, 1}, {9, 1}, {8, 1}, {12, 0}, {13, 2},
  {12, 1}, {14, 1}, {13, 1}, {15, 0}, {16, 2}, {17, 1}
};

bool AutoMapSight_DecorationPlacement(int drawLocation, int *cellNumber, int *summaryIndex)
{
  if (drawLocation < 0 || drawLocation > 12) return false;
  *cellNumber = DECORATION_PLACEMENTS[drawLocation][0];
  *summaryIndex = DECORATION_PLACEMENTS[drawLocation][1];
  return true;
}

int AutoMapSight_SideOfSummaryIndex(int partyFacing, int summaryIndex)
{
  return (partyFacing + summaryIndex + 1) % 4;
}

// A face is shown when the view draws it: the same table the decorations are placed by, so a
// face the 3D view never puts on screen (the front of 15 and 16, the sides beside the party)
// never refreshes
void AutoMapSight_FacesShownOfCell(int cellNumber, bool shown[3])
{
  for (int summaryIndex = 0; summaryIndex < 3; summaryIndex++) shown[summaryIndex] = false;
  for (int drawLocation = 0; drawLocation <= 12; drawLocation++)
  {
    if (DECORATION_PLACEMENTS[drawLocation][0] != cellNumber) continue;
    shown[DECORATION_PLACEMENTS[drawLocation][1]] = true;
  }
}

// Brightness in hundredths of a palette step: 0.30 red, 0.59 green, 0.11 blue
static int BrightnessOf(short colour)
{
  int red = (colour >> 8) & 7;
  int green = (colour >> 4) & 7;
  int blue = colour & 7;
  return 30 * red + 59 * green + 11 * blue;
}

static const int ONE_BRIGHTNESS_STEP = 100;

int AutoMapSight_VisiblePixels(const std::vector<AutoMapPixelPair> &pixels, const short *palette)
{
  int visible = 0;
  for (size_t i = 0; i < pixels.size(); i++)
  {
    const AutoMapPixelPair &pair = pixels[i];
    int contrast = BrightnessOf(palette[pair.drawn & 15]) - BrightnessOf(palette[pair.behind & 15]);
    if (contrast < 0) contrast = -contrast;
    if (contrast >= ONE_BRIGHTNESS_STEP) visible += pair.count;
  }
  return visible;
}

// The most pixels of open touching, sideways or diagonally; width: of the view, row by row
static int LargestPatch(std::vector<bool> open, int width)
{
  int largest = 0;
  // Each pixel is taken out of open once its patch counts it
  std::vector<size_t> next;
  for (size_t start = 0; start < open.size(); start++)
  {
    if (!open[start]) continue;
    open[start] = false;
    next.push_back(start);
    int patch = 0;
    while (!next.empty())
    {
      size_t at = next.back();
      next.pop_back();
      patch++;
      int x = (int)(at % (size_t)width);
      int y = (int)(at / (size_t)width);
      int height = (int)(open.size() / (size_t)width);
      for (int ny = y - 1; ny <= y + 1; ny++)
      {
        for (int nx = x - 1; nx <= x + 1; nx++)
        {
          if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
          size_t neighbour = (size_t)(ny * width + nx);
          if (!open[neighbour]) continue;
          open[neighbour] = false;
          next.push_back(neighbour);
        }
      }
    }
    if (patch > largest) largest = patch;
  }
  return largest;
}

// The median brightness of the background within AUTOMAP_CAMOUFLAGE_RADIUS of (x, y)
static int WallBrightnessAround(const AutoMapDrawView &view, int x, int y)
{
  std::vector<int> around;
  for (int ny = y - AUTOMAP_CAMOUFLAGE_RADIUS; ny <= y + AUTOMAP_CAMOUFLAGE_RADIUS; ny++)
  {
    for (int nx = x - AUTOMAP_CAMOUFLAGE_RADIUS; nx <= x + AUTOMAP_CAMOUFLAGE_RADIUS; nx++)
    {
      if (nx < 0 || ny < 0 || nx >= view.width || ny >= view.height) continue;
      around.push_back(BrightnessOf(view.background[(size_t)(ny * view.width + nx)]));
    }
  }
  std::sort(around.begin(), around.end());
  return around[around.size() / 2];
}

int AutoMapSight_ContrastingPixels(const AutoMapDrawView &view, std::vector<bool> *contrasts)
{
  size_t size = (size_t)(view.width * view.height);
  if (contrasts != NULL) contrasts->assign(size, false);
  int count = 0;
  for (int y = 0; y < view.height; y++)
  {
    for (int x = 0; x < view.width; x++)
    {
      size_t at = (size_t)(y * view.width + x);
      if (!view.isDrawn[at]) continue;
      int contrast = BrightnessOf(view.screen[at]) - WallBrightnessAround(view, x, y);
      if (contrast < 0) contrast = -contrast;
      if (contrast < AUTOMAP_DECORATION_CONTRAST_STEPS * ONE_BRIGHTNESS_STEP) continue;
      count++;
      if (contrasts != NULL) (*contrasts)[at] = true;
    }
  }
  return count;
}

bool AutoMapSight_DoorIsSeen(const AutoMapItemShape &shape)
{
  return shape.visible >= AUTOMAP_SEEN_DOOR_PIXELS;
}

int AutoMapSight_DecorationDarkening(const AutoMapDrawView &view)
{
  long long difference = 0;
  int pixels = 0;
  for (size_t at = 0; at < view.isDrawn.size(); at++)
  {
    if (!view.isDrawn[at]) continue;
    difference += BrightnessOf(view.background[at]) - BrightnessOf(view.screen[at]);
    pixels++;
  }
  if (pixels == 0) return 0;
  if (difference < 0) difference = -difference;
  return (int)(difference / pixels);
}

bool AutoMapSight_DecorationIsSeen(const AutoMapDrawView &view)
{
  if (AutoMapSight_ContrastingPixels(view, NULL) >= AUTOMAP_SEEN_DECORATION_PIXELS) return true;
  if (AutoMapSight_ShapeOfItem(view, NULL).largestPatch >= AUTOMAP_SEEN_DECORATION_PATCH) return true;
  if (AutoMapSight_DecorationTintPatch(view) >= AUTOMAP_SEEN_DECORATION_TINT_PATCH) return true;
  int pixels = (int)std::count(view.isDrawn.begin(), view.isDrawn.end(), true);
  return pixels >= AUTOMAP_SEEN_BLOT_PIXELS
         && AutoMapSight_DecorationDarkening(view) >= AUTOMAP_SEEN_DECORATION_DARKENING;
}

static bool StandsOut(const AutoMapDrawView &view, int x, int y)
{
  short colour = view.screen[(size_t)(y * view.width + x)];
  for (int ny = y - AUTOMAP_CAMOUFLAGE_RADIUS; ny <= y + AUTOMAP_CAMOUFLAGE_RADIUS; ny++)
  {
    for (int nx = x - AUTOMAP_CAMOUFLAGE_RADIUS; nx <= x + AUTOMAP_CAMOUFLAGE_RADIUS; nx++)
    {
      if (nx < 0 || ny < 0 || nx >= view.width || ny >= view.height) continue;
      if (view.background[(size_t)(ny * view.width + nx)] == colour) return false;
    }
  }
  return true;
}

AutoMapItemShape AutoMapSight_ShapeOfItem(const AutoMapDrawView &view, std::vector<bool> *standsOut)
{
  AutoMapItemShape shape = {0, 0, 0};
  size_t size = (size_t)(view.width * view.height);
  std::vector<bool> open(size, false);
  for (int y = 0; y < view.height; y++)
  {
    for (int x = 0; x < view.width; x++)
    {
      if (!view.isDrawn[(size_t)(y * view.width + x)]) continue;
      shape.drawn++;
      if (!StandsOut(view, x, y)) continue;
      open[(size_t)(y * view.width + x)] = true;
      shape.visible++;
    }
  }
  if (standsOut != NULL) *standsOut = open;
  shape.largestPatch = LargestPatch(open, view.width);
  return shape;
}


static bool IsGrey(short colour)
{
  int red = (colour >> 8) & 7;
  return red == ((colour >> 4) & 7) && red == (colour & 7);
}

int AutoMapSight_DecorationTintPatch(const AutoMapDrawView &view)
{
  std::vector<bool> tinted;
  AutoMapSight_ShapeOfItem(view, &tinted);
  for (int y = 0; y < view.height; y++)
  {
    for (int x = 0; x < view.width; x++)
    {
      size_t at = (size_t)(y * view.width + x);
      if (!tinted[at]) continue;
      int step = BrightnessOf(view.screen[at]) - WallBrightnessAround(view, x, y);
      if (step < 0) step = -step;
      if (IsGrey(view.screen[at]) || step < ONE_BRIGHTNESS_STEP) tinted[at] = false;
    }
  }
  return LargestPatch(tinted, view.width);
}

int AutoMapSight_LitPercent(const int *colourCounts, const short *palette)
{
  int total = 0;
  int lit = 0;
  for (int colour = 0; colour < 16; colour++)
  {
    total += colourCounts[colour];
    if (BrightnessOf(palette[colour]) >= ONE_BRIGHTNESS_STEP) lit += colourCounts[colour];
  }
  if (total == 0) return 0;
  return static_cast<int>(static_cast<long long>(lit) * 100 / total);
}

int AutoMapSight_LightReach(const int colourCounts[AUTOMAP_REACH_ROWS][16], const short *palette)
{
  int reach = 0;
  while (reach < AUTOMAP_REACH_ROWS
         && AutoMapSight_LitPercent(colourCounts[reach], palette) >= AUTOMAP_REACHED_ROW_PERCENT)
  {
    reach++;
  }
  return reach;
}

void AutoMapSight_CutAtLightReach(int reach, bool *cells)
{
  for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++)
  {
    if (AutoMapSight_OffsetOfCell(cell).stepsForward > reach) cells[cell] = false;
  }
}

void AutoMapVisibleCells::Clear(void)
{
  m_positions.clear();
}

void AutoMapVisibleCells::Add(int x, int y)
{
  if (Contains(x, y)) return;
  Position position = {x, y};
  m_positions.push_back(position);
}

bool AutoMapVisibleCells::Contains(int x, int y) const
{
  for (size_t i = 0; i < m_positions.size(); i++)
  {
    if (m_positions[i].x == x && m_positions[i].y == y) return true;
  }
  return false;
}
