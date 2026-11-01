#ifndef AUTOMAPLAYOUT_H
#define AUTOMAPLAYOUT_H

// AutoMapLayout : the map window's geometry and drawing decisions, free of SDL and of the engine,
// so "make test" can check them: sizes that grow with the cell, wall slots, badges, line ends,
// the tooltip's side and grid, the bar and the level list. It carries structures: include it
// before CSB.h, which leaves #pragma pack(1) switched on.
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

#endif
