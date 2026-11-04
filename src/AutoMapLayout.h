#ifndef AUTOMAPLAYOUT_H
#define AUTOMAPLAYOUT_H

// AutoMapLayout : the map window's geometry and drawing decisions, free of SDL and of the engine,
// so "make test" can check them: sizes that grow with the cell, wall slots, badges, line ends,
// the tooltip's side and grid, the bar and the level list. It carries structures: include it
// before CSB.h, which leaves #pragma pack(1) switched on.
#include <string>
#include <vector>
#include "AutoMapScene.h"

// A rectangle in pixels; empty when w or h is not positive
struct AutoMapRect
{
  int x;
  int y;
  int w;
  int h;
};

// True when a and b share at least one pixel
bool AutoMapLayout_Overlap(const AutoMapRect &a, const AutoMapRect &b);

// The "who sets it off" badge: a third of the cell high, none below cells of 20 pixels
int AutoMapLayout_BadgeSize(int cellSize);
// The pixel of the 3x5 font on plates inside a cell (stairs, level plates, pile counts)
int AutoMapLayout_PlatePixel(int cellSize);
// Links and teleport lines: the coloured brush, without its dark rim
int AutoMapLayout_LineWidth(int cellSize);
// Arrowheads of links and teleport lines
int AutoMapLayout_HeadSize(int cellSize);

// The square a decoration, writing or portrait takes on face side (0 north .. 3 west) of a wall
// cell, relative to the cell's top left: flush against the face, inset by the face line. 56 % of
// the cell for a face of its own, 44 % slid away from the shared corner when one neighbouring face
// is decorated too, and a third of the room inside the faces when both neighbours are; never deeper
// than half that room when the opposite face is decorated. Decorated slots never overlap.
AutoMapRect AutoMapLayout_WallSlot(const bool decorated[4], int side, int cellSize);
// The corner of a slot for a small item icon: lower right (right) for what lies in an alcove,
// lower left for the key a lock wants; half the slot, at least 8 pixels, never past the slot
AutoMapRect AutoMapLayout_SlotCorner(const AutoMapRect &slot, bool right);
// The thickness of the line along a decorated face, also the slot's inset from the face
int AutoMapLayout_FaceLine(int cellSize);

// The marks of the "who sets it off" badge of pads and teleporters
enum AutoMapBadgeGlyph
{
  AutoMapGlyphParty,
  AutoMapGlyphClaw,
  AutoMapGlyphBox,
  AutoMapGlyphItem,
  AutoMapGlyphGen
};

struct AutoMapBadge
{
  std::vector<AutoMapBadgeGlyph> glyphs;
  // Where the party arrow points, 0 north .. 3 west
  int facing;
  // A facing pad: the badge's frame is gold
  bool gold;
};

AutoMapBadge AutoMapLayout_PadBadge(AutoMapPadOperator op, int requiredFacing);
// Scope as the engine keeps it: 0 objects, 1 monsters, 2 party and objects, 3 anything
AutoMapBadge AutoMapLayout_TeleporterBadge(int scope);

struct AutoMapPointF
{
  float x;
  float y;
};

// The part of its tile (relative to the tile's top left) in which member `member` of a group
// stands. `positions` is the group's stored byte: four 2-bit corners in group order (0 north-west,
// 1 north-east, 2 south-east, 3 south-west), the whole byte 255 meaning the centre. Size 0 (small)
// takes the quarter of its corner; size 1 (medium) the half that holds the corner and runs along
// member 0's facing, as OrdinalOfMonsterAtPosition (Code11f52.cpp) decides who stands where: facing
// east/west splits north from south, north/south splits west from east. Size 2 (large), the byte
// 255 and any other size take the whole tile. The member count plays no part.
AutoMapRect AutoMapLayout_MemberArea(int positions, int member, int monsterSize, int member0Facing, int cellSize);
// Where the feet of a figure drawn in `area` stand: the middle of the area, near its bottom (a bit
// above the bottom of a whole tile so a centred figure sits between the back and front rows)
AutoMapPointF AutoMapLayout_FigureFeet(const AutoMapRect &area, int cellSize);

// Where a line from `from` (pixels, the level's top left at 0, 0) enters cell (targetX, targetY),
// inset 18 % of the cell: lines end at their target's edge, not its centre
AutoMapPointF AutoMapLayout_LineEnd(AutoMapPointF from, int targetX, int targetY, int cellSize);

// Lines that run on one straight line and share a stretch of it (not just a point) form a
// bundle and spread across lanes around that line: two a line width either side, three or more
// 1.5 line widths apart, so none hides another and all still leave their 3-line-wide source
// square. Gold before blue (isFeed false first), then the shorter first. A line runs from its
// source's centre (a switch: the middle of its face) to its target's centre. The result, per
// line, is its lane in line widths to the right of its own direction (screen y down); 0 alone.
std::vector<float> AutoMapLayout_BundleLanes(const std::vector<AutoMapLink> &lines,
                                             const std::vector<bool> &isFeed);

// Where the hovered cell's tooltip goes: the place that hides the least of what the hover lights
// up. Tried: left or right of the hovered cell or of the whole lit group, each above or below the
// hovered row or the lit group or level with the hovered row; a place that does not fit `area` is
// pushed into it. Any overlap is a hit: the cells lines start or end in and the goals (with the
// tab of tabHeight that sits on their top edge) weigh 4, cells a line runs through 1, each plus
// the part of it covered; the hovered cell weighs 1000. Of equal places the nearest to the
// hovered cell wins, then the side away from where the lines lead on the whole. Cell (x, y)
// covers originX + x * cellSize .. + cellSize, likewise for y; the result is the tooltip's rect.
AutoMapRect AutoMapLayout_TooltipPlace(int hoverX, int hoverY, const std::vector<AutoMapLink> &lines,
                                       const std::vector<AutoMapLink> &goals, int width, int height,
                                       int originX, int originY, int cellSize, int tabHeight,
                                       const AutoMapRect &area);

// The tooltip's icon grid for this many entries: six to a row, two rows at most; with more, the
// last tile is "+N" for the rest
struct AutoMapTooltipGrid
{
  int tiles;
  int columns;
  int rows;
  int plusMore;
};
AutoMapTooltipGrid AutoMapLayout_TooltipGrid(int entries);

// The bar above the map: one rectangle per label, left to right, 4 pixels from the top and 18 high.
// A label starting with '|' begins a group (a wider gap), one starting with '#' is a caption, not
// a button (a mark of either kind can be followed by the other); shownLabels gets the labels as drawn, without those marks. When the bar is wider than
// width the long labels shorten (EXPLORED to EXPL, LEARNED to LRND, FOLLOW to FOL, VISION to VIS,
// SPOILERS to SPOIL); if it is still too wide it reaches past width and scrolls. Nothing is dropped.
std::vector<AutoMapRect> AutoMapLayout_PackBar(const std::vector<std::string> &labels, int width, int pixel,
                                               std::vector<std::string> *shownLabels);
// The spacing of one bar style, in screen pixels: between the buttons of a group, between
// groups, before a group that belongs to the one in front (a label starting with '^', such as
// TINT after the vision choices), the space a caption gets around its text (0: bare text), and
// the frame a style draws around each group, which the gaps leave room for
struct AutoMapBarSpacing
{
  int itemGap;
  int groupGap;
  int nearGap;
  int captionPad;
  int groupFrame;
};
// The spacing of the classic bar, the one PackBar above uses
AutoMapBarSpacing AutoMapLayout_ClassicBarSpacing(int pixel);
std::vector<AutoMapRect> AutoMapLayout_PackBarSpaced(const std::vector<std::string> &labels, int width, int pixel,
                                                     const AutoMapBarSpacing &spacing,
                                                     std::vector<std::string> *shownLabels);
// The bar's height for a font pixel
int AutoMapLayout_BarHeight(int pixel);

// The level list on the bar: the first level shown when it opens on focusLevel (four rows down,
// within the dungeon), and the level typed digits pick (clamped to the last; -1 for none)
int AutoMapLayout_LevelListTop(int levels, int rows, int focusLevel);
int AutoMapLayout_TypedLevel(const std::string &digits, int levels);

// Whether a plate, count or badge of width x height fits inside a cell with a pixel to spare: what
// does not fit is left out instead of spilling into the neighbouring cells
bool AutoMapLayout_Fits(int width, int height, int cellSize);

#endif
