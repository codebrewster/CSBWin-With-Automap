// AutoMapTooltip.h : the machinery part of the tile tooltip, grouped by what is on the tile.
// Engine-free, so the tests build it.
#ifndef AUTOMAPTOOLTIP_H
#define AUTOMAPTOOLTIP_H

#include <vector>

#include "AutoMapScene.h"

// The machinery of cell (x, y) in tooltip order: one group per thing, a header and its lines
// indented two spaces. shift: the route / uncapped lists. Empty when the cell has none.
// Group order: teleporter or spinner, pit, door / false wall / launcher, pads, switches,
// generators, then the relay hover lines. hint, when not NULL, receives the tile-wide hint
// the press blocks ask for (SHIFT: FULL CHAIN), to go last in the tooltip.
std::vector<AutoMapTipLine> AutoMapTooltip_Machinery(const AutoMapDrawList &drawList, int x, int y,
                                                     bool shift, AutoMapItemNameFn nameForIndex,
                                                     std::vector<AutoMapTipLine> *hint = NULL);

#endif
