#ifndef AUTOMAPLINKWATCH_H
#define AUTOMAPLINKWATCH_H

#include <cstddef>
#include <vector>

#include "AutoMapKnowledge.h"
#include "AutoMapScene.h"
#include "AutoMapSight.h"

// Ticks a watched cell gets beyond the delays of the links on its path, for the engine's own
// timer granularity
const int AUTOMAP_WATCH_SLACK_TICKS = 12;

// Learns which cells a fired actuator changes: watches every cell its messages can reach
class AutoMapLinkWatch
{
public:
  AutoMapLinkWatch(void);
  void Clear(void);
  // source fired on the snapshot's level at tick now. Every cell reachable from the links
  // whose source is `source`, then from links starting at each reached cell, is watched
  // until now + summed delays + AUTOMAP_WATCH_SLACK_TICKS, with its current record as the
  // baseline. A cell already watched for the same source keeps the later deadline.
  void Start(AutoMapRecordId source, const AutoMapLevelSnapshot &snapshot, int now);
  bool IsWatching(void) const;
  // A later look at the party's level. seenNow, in level coordinates, holds the cells the
  // party can see this tick (AutoMapRecorder::SeenThisTick). Returns one AutoMapFactLink
  // (level coordinates) per watched cell that is seen to change (AutoMapScene_SeenToChange)
  // and is in seenNow or is the party's cell; that cell is no longer watched. A concealed pit
  // counts as known when knowledge holds AutoMapFactPit for it; a NULL knowledge knows
  // nothing. startTicks, when given, is set to the tick each returned fact's watch started,
  // in parallel. Expired cells are dropped. A snapshot of another level than the one watched
  // drops every watch.
  std::vector<AutoMapFact> Observe(const AutoMapLevelSnapshot &snapshot,
                                   const AutoMapVisibleCells &seenNow,
                                   const AutoMapKnowledge *knowledge, int now,
                                   std::vector<int> *startTicks = NULL);

private:
  struct Watch
  {
    AutoMapRecordId source;
    // Snapshot coordinates
    int x;
    int y;
    int deadline;
    // The tick of the Start that last set the baseline, for the debug readout
    int startedAt;
    AutoMapCellRecord baseline;
  };
  int m_level;
  std::vector<Watch> m_watches;
};

// Learns false walls seen opening or closing: remembers the last state seen per cell
class AutoMapFalseWallWatch
{
public:
  void Clear(void);
  // isOpen: the cell's state now. True when a state seen before differs: the caller
  // learns AutoMapFactFalseWall
  bool Observe(const AutoMapCell &cell, bool isOpen);

private:
  struct Seen
  {
    AutoMapCell cell;
    bool isOpen;
  };
  std::vector<Seen> m_seen;
};

#endif
