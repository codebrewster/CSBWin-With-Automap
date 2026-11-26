#include "TestSupport.h"
#include "../AutoMapLinkWatch.h"

#include <cstddef>

static const int LEVEL = 0;
static const int LEVER_PICTURE = 148;
static const AutoMapRecordId SWITCH_RECORD = 7;

static AutoMapCellRecord &At(AutoMapLevelSnapshot *snapshot, int x, int y)
{
  return snapshot->cells[y * snapshot->width + x];
}

// Lever (source 7) on wall (1,0) south links to a counter wall (3,0), which links to the
// door (2,1); the party stands at (1,1). The same shape as in SpoilerTest.cpp.
static AutoMapLevelSnapshot LeverCounterDoor(int firstDelay, int secondDelay)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 4;
  snapshot.height = 3;
  snapshot.cells.assign(12, AutoMapCellRecord());
  At(&snapshot, 1, 1).kind = AutoMapCellFloor;
  At(&snapshot, 2, 1).kind = AutoMapCellDoor;
  At(&snapshot, 2, 1).doorState = AutoMapDoorAlmostClosed;
  At(&snapshot, 1, 0).wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  At(&snapshot, 3, 0).wallSides[AutoMapSouth].picture = LEVER_PICTURE;
  snapshot.party.x = 1;
  snapshot.party.y = 1;
  AutoMapLink toCounter(1, 0, 3, 0, true, AutoMapSouth);
  toCounter.source = SWITCH_RECORD;
  toCounter.delay = firstDelay;
  AutoMapLink toDoor(3, 0, 2, 1, true, AutoMapSouth, AutoMapSignalSet, false, secondDelay, true);
  toDoor.source = 8;
  snapshot.links.push_back(toCounter);
  snapshot.links.push_back(toDoor);
  return snapshot;
}

static AutoMapLevelSnapshot DoorOpened(const AutoMapLevelSnapshot &snapshot)
{
  AutoMapLevelSnapshot changed = snapshot;
  At(&changed, 2, 1).doorState = AutoMapDoorOpen;
  return changed;
}

static void ChangeInSightTeachesTheLink(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(3, 0);
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  CHECK(watch.IsWatching());
  AutoMapVisibleCells seenNow;
  seenNow.Add(2, 1);
  std::vector<AutoMapFact> facts = watch.Observe(DoorOpened(s), seenNow, NULL, 103);
  CHECK(facts.size() == 1);
  if (facts.size() == 1)
  {
    AutoMapFact expected = AutoMapFact_OfLink(SWITCH_RECORD, AutoMapCell{LEVEL, 2, 1});
    CHECK(!(facts[0] < expected) && !(expected < facts[0]));
  }
  // The learned cell is no longer watched; the unchanged counter wall still is
  CHECK(watch.IsWatching());
  CHECK(watch.Observe(DoorOpened(s), seenNow, NULL, 104).empty());
}

static void ChangeOutOfSightTeachesNothing(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(3, 0);
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  AutoMapVisibleCells seenNow;
  CHECK(watch.Observe(DoorOpened(s), seenNow, NULL, 103).empty());
  CHECK(watch.IsWatching());
  // Seen later, still changed: learned then
  seenNow.Add(2, 1);
  CHECK(watch.Observe(DoorOpened(s), seenNow, NULL, 110).size() == 1);
}

static void PartyCellCountsAsInSight(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(3, 0);
  At(&s, 2, 1).kind = AutoMapCellFloor;
  At(&s, 2, 1).doorState = AutoMapDoorOpen;
  s.party.x = 2;
  s.party.y = 1;
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  AutoMapLevelSnapshot changed = s;
  At(&changed, 2, 1).floorPicture = 77;
  AutoMapVisibleCells seenNow;
  CHECK(watch.Observe(changed, seenNow, NULL, 101).size() == 1);
}

static void DeadlineFollowsTheDelays(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(5, 4);
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  AutoMapVisibleCells seenNow;
  seenNow.Add(2, 1);
  // The door is last on the path: 100 + 5 + 4 + 12 = 121
  AutoMapLevelSnapshot changed = DoorOpened(s);
  CHECK(watch.Observe(s, seenNow, NULL, 121).empty());
  CHECK(watch.IsWatching());
  CHECK(watch.Observe(changed, seenNow, NULL, 122).empty());
  CHECK(!watch.IsWatching());
}

static void ObservingAnotherLevelDropsTheWatch(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(3, 0);
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  AutoMapLevelSnapshot other = DoorOpened(s);
  other.level = 1;
  AutoMapVisibleCells seenNow;
  seenNow.Add(2, 1);
  CHECK(watch.Observe(other, seenNow, NULL, 101).empty());
  CHECK(!watch.IsWatching());
  CHECK(watch.Observe(DoorOpened(s), seenNow, NULL, 102).empty());
}

static void WatchUsesLevelCoordinates(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(3, 0);
  AutoMapLevelSnapshot grown;
  grown.level = LEVEL;
  grown.width = 5;
  grown.height = 4;
  grown.boundaryMarginX = 1;
  grown.boundaryMarginY = 1;
  grown.cells.assign(20, AutoMapCellRecord());
  for (int y = 0; y < s.height; y++)
  {
    for (int x = 0; x < s.width; x++) At(&grown, x + 1, y + 1) = At(&s, x, y);
  }
  AutoMapLink toCounter(2, 1, 4, 1, true, AutoMapSouth);
  toCounter.source = SWITCH_RECORD;
  AutoMapLink toDoor(4, 1, 3, 2, true, AutoMapSouth);
  toDoor.source = 8;
  grown.links.push_back(toCounter);
  grown.links.push_back(toDoor);
  grown.party.x = 2;
  grown.party.y = 2;
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, grown, 100);
  AutoMapLevelSnapshot changed = grown;
  At(&changed, 3, 2).doorState = AutoMapDoorOpen;
  AutoMapVisibleCells wrongCell;
  wrongCell.Add(3, 2);
  CHECK(watch.Observe(changed, wrongCell, NULL, 101).empty());
  AutoMapVisibleCells seenNow;
  seenNow.Add(2, 1);
  std::vector<AutoMapFact> facts = watch.Observe(changed, seenNow, NULL, 102);
  CHECK(facts.size() == 1);
  if (facts.size() == 1)
  {
    CHECK(facts[0].kind == AutoMapFactLink);
    CHECK(facts[0].cell.level == LEVEL && facts[0].cell.x == 2 && facts[0].cell.y == 1);
  }
}

static void CyclesEndTheReach(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(1, 1);
  AutoMapLink back(2, 1, 3, 0, true, AutoMapSouth);
  back.source = 9;
  s.links.push_back(back);
  AutoMapLink loop(3, 0, 3, 0, true, AutoMapSouth);
  loop.source = 9;
  s.links.push_back(loop);
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  CHECK(watch.IsWatching());
  AutoMapVisibleCells seenNow;
  seenNow.Add(2, 1);
  // 100 + 1 + 1 + 12: nothing is left once the deadline passed
  CHECK(watch.Observe(s, seenNow, NULL, 115).empty());
  CHECK(!watch.IsWatching());
}

static void LearnsWhatTheFrameShows(void)
{
  // DM L1: the pit front-left of the party is plain in the 3D view, but the ray to its centre
  // grazes the switch wall, so the monster rule would not count it. The frame does.
  AutoMapLevelSnapshot s = LeverCounterDoor(3, 0);
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  AutoMapVisibleCells monstersSee;
  AutoMapVisibleCells seenNow;
  seenNow.Add(2, 1);
  CHECK(!monstersSee.Contains(2, 1));
  std::vector<AutoMapFact> facts = watch.Observe(DoorOpened(s), seenNow, NULL, 103);
  CHECK(facts.size() == 1);
}

// A pad (source 7) on (0,1) toggles the concealed pit (2,1); the party stands at (1,1)
static AutoMapLevelSnapshot PadAndConcealedPit(bool pitIsOpen)
{
  AutoMapLevelSnapshot snapshot;
  snapshot.level = LEVEL;
  snapshot.width = 4;
  snapshot.height = 3;
  snapshot.cells.assign(12, AutoMapCellRecord());
  At(&snapshot, 0, 1).kind = AutoMapCellFloor;
  At(&snapshot, 1, 1).kind = AutoMapCellFloor;
  AutoMapCellRecord &pit = At(&snapshot, 2, 1);
  pit.kind = AutoMapCellPit;
  pit.pitIsHidden = true;
  pit.pitIsOpen = pitIsOpen;
  snapshot.party.x = 1;
  snapshot.party.y = 1;
  AutoMapLink toPit(0, 1, 2, 1, false, -1, AutoMapSignalToggle);
  toPit.source = SWITCH_RECORD;
  snapshot.links.push_back(toPit);
  return snapshot;
}

static void ConcealedPitTeachesOnlyOnceKnown(void)
{
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, PadAndConcealedPit(false), 100);
  AutoMapVisibleCells seenNow;
  seenNow.Add(2, 1);
  AutoMapKnowledge knowledge;
  // Open or closed it reads as floor: nothing the eye saw changed
  CHECK(watch.Observe(PadAndConcealedPit(true), seenNow, &knowledge, 101).empty());
  CHECK(watch.Observe(PadAndConcealedPit(true), seenNow, NULL, 102).empty());
  CHECK(watch.IsWatching());
  // The party fell through it: from now on it is a pit like any
  AutoMapCell pitCell = {LEVEL, 2, 1};
  knowledge.Learn(AutoMapFact_OfCell(AutoMapFactPit, pitCell));
  std::vector<AutoMapFact> facts = watch.Observe(PadAndConcealedPit(true), seenNow, &knowledge, 103);
  CHECK(facts.size() == 1);
  if (facts.size() == 1)
  {
    AutoMapFact expected = AutoMapFact_OfLink(SWITCH_RECORD, pitCell);
    CHECK(!(facts[0] < expected) && !(expected < facts[0]));
  }
}

static void ReportsStartTicks(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(3, 0);
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  AutoMapVisibleCells seenNow;
  seenNow.Add(2, 1);
  std::vector<int> startTicks(1, -5);
  std::vector<AutoMapFact> facts = watch.Observe(DoorOpened(s), seenNow, NULL, 103, &startTicks);
  CHECK(facts.size() == 1);
  CHECK(startTicks.size() == facts.size());
  if (startTicks.size() == 1) CHECK(startTicks[0] == 100);
}

static void FalseWallSeenChanging(void)
{
  AutoMapFalseWallWatch watch;
  AutoMapCell cell = {0, 1, 1};
  CHECK(!watch.Observe(cell, false));
  CHECK(!watch.Observe(cell, false));
  CHECK(watch.Observe(cell, true));
  AutoMapCell other = {0, 2, 1};
  CHECK(!watch.Observe(other, true));
}

// MightLearn is false only when Observe could learn nothing; DropExpired then does what it does
static void MightLearnOnlyWithAWatchedCellInSight(void)
{
  AutoMapLevelSnapshot s = LeverCounterDoor(5, 4);
  s.boundaryMarginX = 1;  // watches are kept in snapshot coordinates, the sight in level ones
  AutoMapLinkWatch watch;
  watch.Start(SWITCH_RECORD, s, 100);
  AutoMapVisibleCells nothing;
  CHECK(!watch.MightLearn(LEVEL, nothing, 9, 9, 101));
  AutoMapVisibleCells door;
  door.Add(1, 1);  // the door (2, 1) in level coordinates
  CHECK(watch.MightLearn(LEVEL, door, 9, 9, 101));
  CHECK(watch.MightLearn(LEVEL, nothing, 1, 1, 101));  // the party on the door's cell
  CHECK(watch.MightLearn(LEVEL + 1, nothing, 9, 9, 101));  // another level drops the watch
  // Past the deadline (100 + 5 + 4 + 12) the door counts no more, and DropExpired ends the watch
  CHECK(!watch.MightLearn(LEVEL, door, 9, 9, 122));
  watch.DropExpired(121);
  CHECK(watch.IsWatching());
  watch.DropExpired(122);
  CHECK(!watch.IsWatching());
}

// ObserveLooks, with the looks taken from a snapshot, learns what Observe on it learns
static AutoMapCellLook LookInSnapshot(void *context, int x, int y)
{
  const AutoMapLevelSnapshot *s = (const AutoMapLevelSnapshot *)context;
  return AutoMapScene_LookOf(s->cells[(y + s->boundaryMarginY) * s->width + x + s->boundaryMarginX]);
}

static bool SameFacts(const std::vector<AutoMapFact> &a, const std::vector<AutoMapFact> &b)
{
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (a[i] < b[i] || b[i] < a[i]) return false;
  return true;
}

static void ObserveLooksLearnsWhatObserveLearns(void)
{
  AutoMapLevelSnapshot lever = LeverCounterDoor(3, 0);
  AutoMapLevelSnapshot grown;
  grown.level = LEVEL;
  grown.width = 5;
  grown.height = 4;
  grown.boundaryMarginX = 1;
  grown.boundaryMarginY = 1;
  grown.cells.assign(20, AutoMapCellRecord());
  for (int y = 0; y < lever.height; y++)
    for (int x = 0; x < lever.width; x++) At(&grown, x + 1, y + 1) = At(&lever, x, y);
  AutoMapLink toCounter(2, 1, 4, 1, true, AutoMapSouth);
  toCounter.source = SWITCH_RECORD;
  AutoMapLink toDoor(4, 1, 3, 2, true, AutoMapSouth);
  toDoor.source = 8;
  grown.links.push_back(toCounter);
  grown.links.push_back(toDoor);
  grown.party.x = 2;
  grown.party.y = 2;
  AutoMapLevelSnapshot grownOpened = grown;
  At(&grownOpened, 3, 2).doorState = AutoMapDoorOpen;
  struct Case { AutoMapLevelSnapshot before, after; int levelWidth, levelHeight; };
  Case cases[] = {
    {lever, DoorOpened(lever), 4, 3},
    {grown, grownOpened, 3, 3},
    {PadAndConcealedPit(false), PadAndConcealedPit(true), 4, 3},
  };
  for (size_t c = 0; c < sizeof cases / sizeof cases[0]; c++)
  {
    const Case &k = cases[c];
    AutoMapKnowledge knowledge;
    if (c == 2) knowledge.Learn(AutoMapFact_OfCell(AutoMapFactPit, AutoMapCell{LEVEL, 2, 1}));
    // Every single cell in sight, the party elsewhere and then on each cell, ticks to past the end
    for (int partyOn = -1; partyOn < k.levelWidth * k.levelHeight; partyOn++)
    {
      for (int seen = -1; seen < k.levelWidth * k.levelHeight; seen++)
      {
        AutoMapLinkWatch byLevel, byLooks;
        byLevel.Start(SWITCH_RECORD, k.before, 100);
        byLooks.Start(SWITCH_RECORD, k.before, 100);
        AutoMapVisibleCells seenNow;
        if (seen >= 0) seenNow.Add(seen % k.levelWidth, seen / k.levelWidth);
        int partyX = partyOn < 0 ? 9 : partyOn % k.levelWidth;
        int partyY = partyOn < 0 ? 9 : partyOn / k.levelWidth;
        AutoMapLevelSnapshot after = k.after;
        after.party.x = partyX + after.boundaryMarginX;
        after.party.y = partyY + after.boundaryMarginY;
        for (int now = 101; now <= 130; now += 7)
        {
          std::vector<int> ticksA, ticksB;
          std::vector<AutoMapFact> a = byLevel.Observe(after, seenNow, &knowledge, now, &ticksA);
          std::vector<AutoMapFact> b = byLooks.ObserveLooks(LEVEL, k.levelWidth, k.levelHeight, LookInSnapshot,
                                                            &after, seenNow, partyX, partyY, &knowledge, now, &ticksB);
          CHECK(SameFacts(a, b) && ticksA == ticksB);
          CHECK(byLevel.IsWatching() == byLooks.IsWatching());
        }
      }
    }
  }
}

void TestAutoMapLinkWatch(void)
{
  ObserveLooksLearnsWhatObserveLearns();
  ChangeInSightTeachesTheLink();
  ChangeOutOfSightTeachesNothing();
  PartyCellCountsAsInSight();
  DeadlineFollowsTheDelays();
  ObservingAnotherLevelDropsTheWatch();
  WatchUsesLevelCoordinates();
  CyclesEndTheReach();
  LearnsWhatTheFrameShows();
  ConcealedPitTeachesOnlyOnceKnown();
  ReportsStartTicks();
  FalseWallSeenChanging();
  MightLearnOnlyWithAWatchedCellInSight();
}
