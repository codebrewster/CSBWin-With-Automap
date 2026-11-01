#ifndef AUTOMAPLAYOUT_H
#define AUTOMAPLAYOUT_H

// AutoMapLayout : the map window's geometry and drawing decisions, free of SDL and of the engine,
// so "make test" can check them: sizes that grow with the cell, wall slots, badges, line ends,
// the tooltip's side and grid, the bar and the level list.

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

#endif
