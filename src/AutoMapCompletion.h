#ifndef AUTOMAPCOMPLETION_H
#define AUTOMAPCOMPLETION_H

#include <utility>
#include <vector>

// How much of a level the party can reach, for the Statistics window's Profile: the cells a
// closed-off area holds are no part of what there is to explore. No engine.

// One cell of a level as the counting sees it. Walkable: floor, pit, stairs, a door in any
// state, a teleporter, a false wall of either kind. Solid: wall and the reader's added rock.
enum AutoMapReachKind { AutoMapReachSolid, AutoMapReachWalkable };
struct AutoMapReachGrid { int width, height; std::vector<AutoMapReachKind> cells; };  // row by row

// The cells of the walkable areas that hold a seed (x, y), through the four sides of a cell.
// Seeds outside the grid or on a solid cell are ignored. The mask runs parallel to grid.cells.
std::vector<bool> AutoMapCompletion_Reachable(const AutoMapReachGrid &grid, const std::vector<std::pair<int, int> > &seeds);

#endif
