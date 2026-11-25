#include "AutoMapCompletion.h"
#include <cstddef>

// Marks the cell (x, y) of level reached and queues it, unless it is off the grid, solid or reached
static void Visit(const std::map<int, AutoMapReachLevel> &levels, std::map<int, std::vector<bool> > &reached,
                  const std::map<int, std::vector<int> > &firstJump, std::vector<AutoMapCell> &queue, int level, int x, int y)
{
  std::map<int, AutoMapReachLevel>::const_iterator found = levels.find(level);
  if (found == levels.end() || firstJump.count(level) == 0) return;
  const AutoMapReachGrid &grid = found->second.grid;
  if (x < 0 || y < 0 || x >= grid.width || y >= grid.height) return;
  int at = y * grid.width + x;
  std::vector<bool> &mask = reached[level];
  if (grid.cells[(size_t)at] == AutoMapReachSolid || mask[(size_t)at]) return;
  mask[(size_t)at] = true;
  queue.push_back(AutoMapCell{level, at, 0});
}

std::map<int, std::vector<bool> > AutoMapCompletion_ReachableAcross(const std::map<int, AutoMapReachLevel> &levels,
                                                                   const std::vector<AutoMapCell> &seeds)
{
  std::map<int, std::vector<bool> > reached;
  // The jumps of each cell, as a list threaded through firstJump and nextJump
  std::map<int, std::vector<int> > firstJump, nextJump;
  for (std::map<int, AutoMapReachLevel>::const_iterator at = levels.begin(); at != levels.end(); ++at)
  {
    const AutoMapReachGrid &grid = at->second.grid;
    size_t cellCount = grid.width > 0 && grid.height > 0 ? (size_t)grid.width * grid.height : 0;
    reached[at->first].assign(grid.cells.size(), false);
    if (grid.cells.size() != cellCount) continue;
    std::vector<int> &first = firstJump[at->first], &next = nextJump[at->first];
    first.assign(cellCount, -1);
    next.assign(at->second.jumps.size(), -1);
    for (size_t j = 0; j < at->second.jumps.size(); j++)
    {
      int from = at->second.jumps[j].from;
      if (from < 0 || (size_t)from >= cellCount) continue;
      next[j] = first[(size_t)from];
      first[(size_t)from] = (int)j;
    }
  }
  std::vector<AutoMapCell> queue;  // x holds the cell's index in its grid, y is unused
  for (size_t i = 0; i < seeds.size(); i++) Visit(levels, reached, firstJump, queue, seeds[i].level, seeds[i].x, seeds[i].y);
  static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
  for (size_t n = 0; n < queue.size(); n++)
  {
    int level = queue[n].level, at = queue[n].x;
    const AutoMapReachLevel &here = levels.find(level)->second;
    int x = at % here.grid.width, y = at / here.grid.width;
    if (here.grid.cells[(size_t)at] != AutoMapReachPassedOn)
      for (int side = 0; side < 4; side++) Visit(levels, reached, firstJump, queue, level, x + DX[side], y + DY[side]);
    const std::vector<int> &next = nextJump[level];
    for (int j = firstJump[level][(size_t)at]; j >= 0; j = next[(size_t)j])
    {
      const AutoMapReachJump &jump = here.jumps[(size_t)j];
      Visit(levels, reached, firstJump, queue, jump.toLevel, jump.toX, jump.toY);
    }
  }
  return reached;
}

bool AutoMapCompletion_LevelIsStale(bool cached, long long readVersion, long long version, bool partyOnLevel,
                                    unsigned msSinceRead)
{
  if (!cached) return true;
  if (version == readVersion) return false;
  return !partyOnLevel || msSinceRead >= 2000;
}
