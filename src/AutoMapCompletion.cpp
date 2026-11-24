#include "AutoMapCompletion.h"
#include <cstddef>

std::vector<bool> AutoMapCompletion_Reachable(const AutoMapReachGrid &grid, const std::vector<std::pair<int, int> > &seeds)
{
  size_t cellCount = grid.width > 0 && grid.height > 0 ? (size_t)grid.width * grid.height : 0;
  std::vector<bool> reached(grid.cells.size(), false);
  if (grid.cells.size() != cellCount) return reached;
  std::vector<int> queue;
  for (size_t i = 0; i < seeds.size(); i++)
  {
    int x = seeds[i].first, y = seeds[i].second;
    if (x < 0 || y < 0 || x >= grid.width || y >= grid.height) continue;
    int at = y * grid.width + x;
    if (grid.cells[at] != AutoMapReachWalkable || reached[at]) continue;
    reached[at] = true;
    queue.push_back(at);
  }
  static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
  for (size_t next = 0; next < queue.size(); next++)
  {
    int x = queue[next] % grid.width, y = queue[next] / grid.width;
    for (int side = 0; side < 4; side++)
    {
      int nx = x + DX[side], ny = y + DY[side];
      if (nx < 0 || ny < 0 || nx >= grid.width || ny >= grid.height) continue;
      int at = ny * grid.width + nx;
      if (grid.cells[at] != AutoMapReachWalkable || reached[at]) continue;
      reached[at] = true;
      queue.push_back(at);
    }
  }
  return reached;
}

std::pair<int, int> AutoMapCompletion_TargetSeed(int targetLevel, int targetX, int targetY, int sourceLevel, int level,
                                                 int marginX, int marginY)
{
  (void)targetLevel;
  if (sourceLevel == level) return std::make_pair(targetX, targetY);
  return std::make_pair(targetX + marginX, targetY + marginY);
}
