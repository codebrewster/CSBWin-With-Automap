#ifndef AUTOMAPWINDOWTOOLTIP_H
#define AUTOMAPWINDOWTOOLTIP_H

// The tile tooltip as the map window builds it, for the export. This header carries structures,
// so it must be included before CSB.h, which leaves #pragma pack(1) switched on.
#include <string>
#include <vector>
#include "AutoMapScene.h"

// One tile of the tooltip's icon grid
struct AutoMapTooltipEntry
{
  AutoMapItem item;
  int count;
  bool nested;
  bool predicted;
  bool required;
  bool carried;
  int side;
};

// The lines (POS first) and the icon grid of the tooltip of cell (x, y) of list; both stay empty
// when the cell has nothing to tell. fullChain: as with Shift held.
void AutoMapWindow_TooltipFor(const AutoMapDrawList &list, int x, int y, bool fullChain,
                              std::vector<std::string> *lines,
                              std::vector<AutoMapTooltipEntry> *icons);

// Draws list as the map window would, at a fixed cellSize centred on (cellX, cellY), with that cell
// hovered (and Shift held when shift), into a BMP of width x height; false when it fails
bool AutoMapWindow_RenderToFile(const AutoMapDrawList &list, int width, int height, int cellSize,
                                int cellX, int cellY, bool shift, const char *path);

#endif
