#ifndef AUTOMAPCOMPLETION_H
#define AUTOMAPCOMPLETION_H

#include <map>
#include <utility>
#include <vector>
#include "AutoMapExplored.h"

// How much of a level the party can reach, for the Statistics window's Profile: the cells a
// closed-off area holds are no part of what there is to explore. No engine.

// One cell of a level as the counting sees it. Walkable: floor, pit, stairs, a door in any
// state, a teleporter that leaves the party be, a false wall of either kind. Solid: wall and the
// reader's added rock. Passed on: a teleporter that takes the party, which moves it away the moment
// it steps on, so the cell is reached but leads on only along its jumps, not to its neighbours.
enum AutoMapReachKind { AutoMapReachSolid, AutoMapReachWalkable, AutoMapReachPassedOn };
struct AutoMapReachGrid { int width, height; std::vector<AutoMapReachKind> cells; };  // row by row

// A way off a cell that walking does not take: stairs, a teleporter that takes the party, a pit.
// from is the cell's index in its level's grid; the target is a cell of level toLevel's grid.
struct AutoMapReachJump { int from; int toLevel; int toX; int toY; };
struct AutoMapReachLevel { AutoMapReachGrid grid; std::vector<AutoMapReachJump> jumps; };

// The cells of every level the party can get to from the seeds (level, grid x, grid y): through
// the four sides of a walkable cell and along the jumps of each cell reached, onto any level, so
// a pit several levels up or a teleporter on another level counts only when the party can get
// to it. One pass over all levels. A mask per level, parallel to its grid's cells; seeds and
// targets outside a grid, on a solid cell or on a level not given are ignored.
std::map<int, std::vector<bool> > AutoMapCompletion_ReachableAcross(const std::map<int, AutoMapReachLevel> &levels,
                                                                   const std::vector<AutoMapCell> &seeds);

// Whether a level read for the Profile is read again: never read yet, or the party has spent time
// on it since it was read (version, the game ticks spent there, moved from readVersion), whether
// anyone was looking or not. The party's own level changes all the time: at most every 2 seconds.
bool AutoMapCompletion_LevelIsStale(bool cached, long long readVersion, long long version, bool partyOnLevel,
                                    unsigned msSinceRead);

#endif
