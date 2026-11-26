// AutoMapLinkWatch.cpp : learning which cells a fired actuator changes, by watching them.
#include "AutoMapLinkWatch.h"

#include <cstddef>

namespace
{
  struct Reach
  {
    int x;
    int y;
    int delaySum;
  };
}

static int FindReach(const std::vector<Reach> &reached, int x, int y)
{
  for (size_t i = 0; i < reached.size(); i++)
  {
    if (reached[i].x == x && reached[i].y == y) return (int)i;
  }
  return -1;
}

// Records that a cell was reached with a delay sum. A cell whose sum improves is walked
// again, so a cycle ends: its second lap only adds delay.
static void Reached(std::vector<Reach> *reached, std::vector<size_t> *queue,
                    int x, int y, int delaySum)
{
  int found = FindReach(*reached, x, y);
  if (found < 0)
  {
    Reach reach = {x, y, delaySum};
    reached->push_back(reach);
    queue->push_back(reached->size() - 1);
  }
  else if (delaySum < (*reached)[found].delaySum)
  {
    (*reached)[found].delaySum = delaySum;
    queue->push_back((size_t)found);
  }
}

// A breadth-first walk over the snapshot's link graph from the links of one source, keeping
// per cell the smallest delay sum
static std::vector<Reach> ReachFrom(AutoMapRecordId source, const AutoMapLevelSnapshot &snapshot)
{
  std::vector<Reach> reached;
  std::vector<size_t> queue;
  for (size_t i = 0; i < snapshot.links.size(); i++)
  {
    const AutoMapLink &link = snapshot.links[i];
    if (link.source == source) Reached(&reached, &queue, link.dstX, link.dstY, link.delay);
  }
  for (size_t head = 0; head < queue.size(); head++)
  {
    Reach from = reached[queue[head]];
    for (size_t i = 0; i < snapshot.links.size(); i++)
    {
      const AutoMapLink &link = snapshot.links[i];
      if (link.srcX != from.x || link.srcY != from.y) continue;
      Reached(&reached, &queue, link.dstX, link.dstY, from.delaySum + link.delay);
    }
  }
  return reached;
}

static bool IsInside(const AutoMapLevelSnapshot &snapshot, int x, int y)
{
  return x >= 0 && y >= 0 && x < snapshot.width && y < snapshot.height;
}

AutoMapLinkWatch::AutoMapLinkWatch(void)
  : m_level(AUTOMAP_NO_LEVEL), m_marginX(0), m_marginY(0)
{
}

void AutoMapLinkWatch::Clear(void)
{
  m_level = AUTOMAP_NO_LEVEL;
  m_watches.clear();
}

void AutoMapLinkWatch::Start(AutoMapRecordId source, const AutoMapLevelSnapshot &snapshot,
                             int now)
{
  if (snapshot.level != m_level) Clear();
  m_level = snapshot.level;
  m_marginX = snapshot.boundaryMarginX;
  m_marginY = snapshot.boundaryMarginY;
  std::vector<Reach> reached = ReachFrom(source, snapshot);
  for (size_t i = 0; i < reached.size(); i++)
  {
    if (!IsInside(snapshot, reached[i].x, reached[i].y)) continue;
    int deadline = now + reached[i].delaySum + AUTOMAP_WATCH_SLACK_TICKS;
    WatchCell(source, reached[i].x, reached[i].y, deadline, now,
              snapshot.cells[reached[i].y * snapshot.width + reached[i].x]);
  }
  if (m_watches.empty()) m_level = AUTOMAP_NO_LEVEL;
}

void AutoMapLinkWatch::StartLooks(AutoMapRecordId source, const AutoMapLevelSnapshot &links,
                                  LookAt lookAt, void *context, int now)
{
  if (links.level != m_level) Clear();
  m_level = links.level;
  m_marginX = links.boundaryMarginX;
  m_marginY = links.boundaryMarginY;
  std::vector<Reach> reached = ReachFrom(source, links);
  for (size_t i = 0; i < reached.size(); i++)
  {
    if (!IsInside(links, reached[i].x, reached[i].y)) continue;
    int deadline = now + reached[i].delaySum + AUTOMAP_WATCH_SLACK_TICKS;
    // Observe and ObserveLooks only ever compare the look of the baseline
    AutoMapCellRecord baseline;
    AutoMapScene_ApplyLook(lookAt(context, reached[i].x - m_marginX, reached[i].y - m_marginY),
                           &baseline);
    WatchCell(source, reached[i].x, reached[i].y, deadline, now, baseline);
  }
  if (m_watches.empty()) m_level = AUTOMAP_NO_LEVEL;
}

// Watches snapshot cell (x, y) for source until deadline; one watched already for the same
// source keeps the later deadline and takes the new baseline
void AutoMapLinkWatch::WatchCell(AutoMapRecordId source, int x, int y, int deadline, int now,
                                 const AutoMapCellRecord &baseline)
{
  bool isWatched = false;
  for (size_t w = 0; w < m_watches.size(); w++)
  {
    Watch &watch = m_watches[w];
    if (watch.source != source || watch.x != x || watch.y != y) continue;
    if (deadline > watch.deadline) watch.deadline = deadline;
    watch.baseline = baseline;
    watch.startedAt = now;
    isWatched = true;
  }
  if (isWatched) return;
  Watch watch = {source, x, y, deadline, now, baseline};
  m_watches.push_back(watch);
}

bool AutoMapLinkWatch::IsWatching(void) const
{
  return !m_watches.empty();
}

bool AutoMapLinkWatch::MightLearn(int level, const AutoMapVisibleCells &seenNow, int partyX, int partyY,
                                  int now) const
{
  if (level != m_level) return true;  // Observe drops every watch
  for (size_t i = 0; i < m_watches.size(); i++)
  {
    const Watch &watch = m_watches[i];
    if (now > watch.deadline) continue;
    int levelX = watch.x - m_marginX;
    int levelY = watch.y - m_marginY;
    if (seenNow.Contains(levelX, levelY) || (levelX == partyX && levelY == partyY)) return true;
  }
  return false;
}

void AutoMapLinkWatch::DropExpired(int now)
{
  std::vector<Watch> kept;
  for (size_t i = 0; i < m_watches.size(); i++)
    if (now <= m_watches[i].deadline) kept.push_back(m_watches[i]);
  m_watches.swap(kept);
  if (m_watches.empty()) m_level = AUTOMAP_NO_LEVEL;
}

std::vector<AutoMapFact> AutoMapLinkWatch::Observe(const AutoMapLevelSnapshot &snapshot,
                                                   const AutoMapVisibleCells &seenNow,
                                                   const AutoMapKnowledge *knowledge, int now,
                                                   std::vector<int> *startTicks)
{
  std::vector<AutoMapFact> facts;
  if (startTicks != NULL) startTicks->clear();
  if (snapshot.level != m_level)
  {
    Clear();
    return facts;
  }
  std::vector<Watch> kept;
  for (size_t i = 0; i < m_watches.size(); i++)
  {
    const Watch &watch = m_watches[i];
    if (now > watch.deadline || !IsInside(snapshot, watch.x, watch.y)) continue;
    // The seen cells are level cells; the snapshot may carry rock on its west and north
    int levelX = watch.x - snapshot.boundaryMarginX;
    int levelY = watch.y - snapshot.boundaryMarginY;
    bool isSeen = seenNow.Contains(levelX, levelY) ||
                  (watch.x == snapshot.party.x && watch.y == snapshot.party.y);
    AutoMapCell target = {snapshot.level, levelX, levelY};
    bool pitIsKnown = knowledge != NULL &&
                      knowledge->Knows(AutoMapFact_OfCell(AutoMapFactPit, target));
    const AutoMapCellRecord &record = snapshot.cells[watch.y * snapshot.width + watch.x];
    // A concealed pit toggling reads as floor both ways: nothing was seen to happen
    if (isSeen && AutoMapScene_SeenToChange(watch.baseline, record, pitIsKnown))
    {
      facts.push_back(AutoMapFact_OfLink(watch.source, target));
      if (startTicks != NULL) startTicks->push_back(watch.startedAt);
      continue;
    }
    kept.push_back(watch);
  }
  m_watches.swap(kept);
  if (m_watches.empty()) m_level = AUTOMAP_NO_LEVEL;
  return facts;
}

std::vector<AutoMapFact> AutoMapLinkWatch::ObserveLooks(int level, int width, int height, LookAt lookAt,
                                                        void *context, const AutoMapVisibleCells &seenNow,
                                                        int partyX, int partyY,
                                                        const AutoMapKnowledge *knowledge, int now,
                                                        std::vector<int> *startTicks)
{
  std::vector<AutoMapFact> facts;
  if (startTicks != NULL) startTicks->clear();
  if (level != m_level)
  {
    Clear();
    return facts;
  }
  std::vector<Watch> kept;
  for (size_t i = 0; i < m_watches.size(); i++)
  {
    const Watch &watch = m_watches[i];
    // Every watch lies inside its level's snapshot: Start took it from one of the same size
    if (now > watch.deadline) continue;
    int levelX = watch.x - m_marginX;
    int levelY = watch.y - m_marginY;
    bool isSeen = seenNow.Contains(levelX, levelY) || (levelX == partyX && levelY == partyY);
    if (isSeen)
    {
      AutoMapCell target = {level, levelX, levelY};
      bool pitIsKnown = knowledge != NULL &&
                        knowledge->Knows(AutoMapFact_OfCell(AutoMapFactPit, target));
      AutoMapCellLook was = AutoMapScene_LookOf(watch.baseline);
      // The rim around the level is made-up rock, the same in every snapshot
      bool isOnLevel = levelX >= 0 && levelY >= 0 && levelX < width && levelY < height;
      AutoMapCellLook look = isOnLevel ? lookAt(context, levelX, levelY) : was;
      if (AutoMapScene_LooksDiffer(was, AutoMapScene_VisibleLook(look, &was, pitIsKnown)))
      {
        facts.push_back(AutoMapFact_OfLink(watch.source, target));
        if (startTicks != NULL) startTicks->push_back(watch.startedAt);
        continue;
      }
    }
    kept.push_back(watch);
  }
  m_watches.swap(kept);
  if (m_watches.empty()) m_level = AUTOMAP_NO_LEVEL;
  return facts;
}

void AutoMapFalseWallWatch::Clear(void)
{
  m_seen.clear();
}

bool AutoMapFalseWallWatch::Observe(const AutoMapCell &cell, bool isOpen)
{
  for (size_t i = 0; i < m_seen.size(); i++)
  {
    const AutoMapCell &known = m_seen[i].cell;
    if (known.level != cell.level || known.x != cell.x || known.y != cell.y) continue;
    bool hasChanged = m_seen[i].isOpen != isOpen;
    m_seen[i].isOpen = isOpen;
    return hasChanged;
  }
  Seen seen = {cell, isOpen};
  m_seen.push_back(seen);
  return false;
}
