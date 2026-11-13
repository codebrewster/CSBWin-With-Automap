#include "stdafx.h"

#include "UI.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#if defined(__linux__)
#include <unistd.h>
#endif
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"
#include "ReplaySnapshot.h"
#include "ReplaySnapshotParts.h"
#include "PlaybackControl.h"
#include "miniz/miniz.h"

bool IsPlayFileOpen(void);

extern DATABASES db;
extern i32 dbEntrySizes[16];
extern ui16 *dataIndexMap;
extern ui8 *dataTypeMap;
extern i32 dataMapLength;
extern ui32 *globalVariables;
extern i32 numGlobalVariables;
extern ui32 cellflagArraySize;
extern ui32 numRandomCalls;
extern i32 totalMoveCount;
extern MouseQueueEnt MouseQueue[MOUSEQLEN];
extern MouseQueueEnt *pMouseQueue;
extern ui8 LScreenBase[320][210];
extern i32 NoSpeedLimit;
extern ui32 graphicRandom;
extern ui32 viewportRandom;
extern i16 globalPalette[16];
extern bool GameIsComplete;
extern bool endSequencePlaying; // CSBlinux.cpp
i32 StartCSB_State(void);          // CSBCode.cpp
void StartCSB_SetState(i32 state);
void MainLoop_WaitForMessage(void);
bool PlayfileCanGoBack(void);       // Recording.cpp
int PlayfileEndTick(void);
bool PlayfileReopenForRewind(void);
void PlayfileAwaitRestore(void);
void PlayfileCloseAgain(void);

const int SNAPSHOT_EVERY = REPLAY_SNAPSHOT_EVERY;

// A block on the heap goes in by its length and content. On the way back the block keeps its
// address when the content fits (what the engine has now is at least `have` bytes long) and
// is made larger otherwise.
static void PutBlock(SnapshotBytes &b, const void *p, size_t n)
{
  SnapshotPut(b, &n, sizeof n);
  if (p != NULL) SnapshotPut(b, p, n);
}

template <class T>
static void GetBlock(SnapshotReader &r, T *&p, size_t have, ui32 mallocId)
{
  size_t n;
  r.Get(&n, sizeof n);
  if (n == 0) return;
  if (p == NULL || have < n) p = (T *)UI_realloc(p, (i32)n, mallocId);
  r.Get(p, n);
}

// The databases of the dungeon's objects: each grows by a hundred entries when full
struct DatabasesSnapshot
{
  static void Save(SnapshotBytes &b)
  {
    SnapshotPut(b, &db.m_noEnlarge, sizeof db.m_noEnlarge);
    for (int i = 0; i < 16; i++)
    {
      SnapshotPut(b, &db.m_numEnt[i], sizeof db.m_numEnt[i]);
      PutBlock(b, db.m_Address[i], db.m_Address[i] == NULL ? 0 : (size_t)db.m_numEnt[i] * dbEntrySizes[i]);
    }
  }
  static void Restore(SnapshotReader &r)
  {
    r.Get(&db.m_noEnlarge, sizeof db.m_noEnlarge);
    for (int i = 0; i < 16; i++)
    {
      i32 n;
      r.Get(&n, sizeof n);
      size_t have = db.m_Address[i] == NULL ? 0 : (size_t)db.m_numEnt[i] * dbEntrySizes[i];
      GetBlock(r, db.m_Address[i], have, MALLOC047);
      db.m_numEnt[i] = n;
    }
  }
};

// The timers: three arrays of m_maxTimers entries each
struct GameTimersSnapshot
{
  static void Save(SnapshotBytes &b)
  {
    GameTimers &t = gameTimers;
    SnapshotPut(b, &t.m_numTimer, sizeof t.m_numTimer);
    SnapshotPut(b, &t.m_maxTimers, sizeof t.m_maxTimers);
    SnapshotPut(b, &t.m_firstAvailTimer, sizeof t.m_firstAvailTimer);
    SnapshotPut(b, &t.m_timerSequence, sizeof t.m_timerSequence);
    SnapshotPut(b, &t.m_searchActive, sizeof t.m_searchActive);
    // The queue as far as it is used; the search list is built anew for each search
    PutBlock(b, t.m_timerQueue, t.m_timerQueue == NULL ? 0 : t.m_numTimer * sizeof t.m_timerQueue[0]);
    PutBlock(b, t.m_timers, t.m_timers == NULL ? 0 : t.m_maxTimers * sizeof t.m_timers[0]);
  }
  static void Restore(SnapshotReader &r)
  {
    GameTimers &t = gameTimers;
    size_t max = t.m_maxTimers;
    r.Get(&t.m_numTimer, sizeof t.m_numTimer);
    r.Get(&t.m_maxTimers, sizeof t.m_maxTimers);
    r.Get(&t.m_firstAvailTimer, sizeof t.m_firstAvailTimer);
    r.Get(&t.m_timerSequence, sizeof t.m_timerSequence);
    r.Get(&t.m_searchActive, sizeof t.m_searchActive);
    GetBlock(r, t.m_timerQueue, max * sizeof t.m_timerQueue[0], MALLOC110);
    GetBlock(r, t.m_timers, max * sizeof t.m_timers[0], MALLOC110);
    if (t.m_maxTimers > max)
      t.m_searchList = (HTIMER *)UI_realloc(t.m_searchList, t.m_maxTimers * sizeof t.m_searchList[0], MALLOC110);
  }
};

namespace
{
// One piece of the game's state: how it goes into a snapshot and comes back
struct Part
{
  const char *name;
  void (*save)(SnapshotBytes &b);
  void (*restore)(SnapshotReader &r);
};

// The index from an object's number to its database entry; it grows one entry at a time
void SaveDataMap(SnapshotBytes &b)
{
  SnapshotPut(b, &dataMapLength, sizeof dataMapLength);
  PutBlock(b, dataIndexMap, dataIndexMap == NULL ? 0 : 2 * (size_t)dataMapLength);
  PutBlock(b, dataTypeMap, dataTypeMap == NULL ? 0 : (size_t)dataMapLength);
}

void RestoreDataMap(SnapshotReader &r)
{
  size_t have = dataMapLength;
  r.Get(&dataMapLength, sizeof dataMapLength);
  GetBlock(r, dataIndexMap, 2 * have, MALLOC063);
  GetBlock(r, dataTypeMap, have, MALLOC062);
}

void SaveGlobalVariables(SnapshotBytes &b)
{
  SnapshotPut(b, &numGlobalVariables, sizeof numGlobalVariables);
  PutBlock(b, globalVariables, globalVariables == NULL ? 0 : 4 * (size_t)numGlobalVariables);
}

void RestoreGlobalVariables(SnapshotReader &r)
{
  size_t have = 4 * (size_t)numGlobalVariables;
  r.Get(&numGlobalVariables, sizeof numGlobalVariables);
  GetBlock(r, globalVariables, have, MALLOC105);
}

// The monsters active on the party's level, the list of objects in each cell and the text
// index. Their blocks are only named in d, which comes back after them and keeps them.
size_t ObjectListBytes(void)
{
  return d.dungeonDatIndex == NULL ? 0 : 2 * (size_t)d.dungeonDatIndex->ObjectListLength();
}

size_t IndirectTextBytes(void)
{
  return d.dungeonDatIndex == NULL ? 0 : 4 * (size_t)d.dungeonDatIndex->NumWordsInTextArray();
}

void SaveHeapOfD(SnapshotBytes &b)
{
  PutBlock(b, d.Item16, d.Item16 == NULL ? 0 : 16 * (size_t)d.MaxITEM16);
  PutBlock(b, d.objectList, d.objectList == NULL ? 0 : ObjectListBytes());
  PutBlock(b, d.indirectTextIndex, d.indirectTextIndex == NULL ? 0 : IndirectTextBytes());
  PutBlock(b, d.compressedText, d.compressedText == NULL ? 0 : 2 * (size_t)d.sizeOfCompressedText);
}

// Before d and the dungeon's index come back: what the blocks hold now is from the current state
void RestoreHeapOfD(SnapshotReader &r)
{
  GetBlock(r, d.Item16, d.Item16 == NULL ? 0 : 16 * (size_t)d.MaxITEM16, MALLOC009);
  GetBlock(r, d.objectList, ObjectListBytes(), MALLOC096);
  GetBlock(r, d.indirectTextIndex, IndirectTextBytes(), MALLOC039);
  GetBlock(r, d.compressedText, 2 * (size_t)d.sizeOfCompressedText, MALLOC039);
}

// In the engine's own memory pool, at addresses that never change: the dungeon's header, the
// levels, the index of each column's objects and the cells
size_t NumLevel(void) { return d.dungeonDatIndex == NULL ? 0 : d.dungeonDatIndex->NumLevel(); }

void PutFixed(SnapshotBytes &b, const void *p, size_t n)
{
  if (p == NULL) n = 0;
  SnapshotPut(b, &n, sizeof n);
  if (n != 0) SnapshotPut(b, p, n);
}

void GetFixed(SnapshotReader &r, void *p)
{
  size_t n;
  r.Get(&n, sizeof n);
  if (n != 0) r.Get(p, n);
}

void SaveDungeon(SnapshotBytes &b)
{
  PutFixed(b, d.dungeonDatIndex, sizeof *d.dungeonDatIndex);
  PutFixed(b, d.pLevelDescriptors, 16 * NumLevel());
  PutFixed(b, d.objectLevelIndex, 2 * NumLevel());
  PutFixed(b, d.objectListIndex, 2 * (size_t)d.numColumnPointers);
  PutFixed(b, d.cellFlagArray, cellflagArraySize);
}

void RestoreDungeon(SnapshotReader &r)
{
  GetFixed(r, d.dungeonDatIndex);
  GetFixed(r, d.pLevelDescriptors);
  GetFixed(r, d.objectLevelIndex);
  GetFixed(r, d.objectListIndex);
  GetFixed(r, d.cellFlagArray);
}

// d as it was, apart from what belongs to the screen and to the engine's own memory: the
// heap blocks it has now (RestoreHeapOfD put them right), the memory pool's bookkeeping, the
// graphics unpacker and cache, the cursor and the file being read. Those stay as they are.
// A pointer into the active monsters moves with them.
struct KeptRange
{
  size_t from, to;
};

// Where a field of d lies in it (DBank has private members, so not offsetof)
#define D_OFFSET(field) ((size_t)((const char *)&d.field - (const char *)&d))
const KeptRange keptOfD[] =
{
  {D_OFFSET(Pointer23322), D_OFFSET(pKeyXlate1)},           // memory pool
  {D_OFFSET(pEndOfAvailMemory), D_OFFSET(Pointer22968)},    // memory pool
  {D_OFFSET(LZWrepeatChar), D_OFFSET(Pointer16822)},        // unpacker, graphics cache, cursor
  {D_OFFSET(ClusterInCache), D_OFFSET(fill23240)},          // graphics file
  {D_OFFSET(CursorWidth), D_OFFSET(PressingEyeOrMouth)},    // cursor
  {D_OFFSET(inStreamLeft), D_OFFSET(Word22598)},            // file being read
};
#undef D_OFFSET

// What is kept goes in as zeros: it never comes back, and so two snapshots of the same state
// are the same bytes
void SaveD(SnapshotBytes &b)
{
  size_t at = b.size();
  SnapshotPut(b, &d, sizeof d);
  for (size_t i = 0; i < sizeof keptOfD / sizeof keptOfD[0]; i++)
    memset(&b[at + keptOfD[i].from], 0, keptOfD[i].to - keptOfD[i].from);
}

void RestoreD(SnapshotReader &r)
{
  static unsigned char now[sizeof(DBank)];
  memcpy(now, &d, sizeof d);
  ITEM16 *item16 = d.Item16;
  RN *objectList = d.objectList;
  ui32 *indirectTextIndex = d.indirectTextIndex;
  ui16 *compressedText = d.compressedText;
  r.Get(&d, sizeof d);
  for (size_t i = 0; i < sizeof keptOfD / sizeof keptOfD[0]; i++)
    memcpy((unsigned char *)&d + keptOfD[i].from, now + keptOfD[i].from, keptOfD[i].to - keptOfD[i].from);
  if (d.pI1613134 != NULL && d.Item16 != NULL && item16 != d.Item16
      && d.pI1613134 >= d.Item16 && d.pI1613134 < d.Item16 + d.MaxITEM16)
    d.pI1613134 = item16 + (d.pI1613134 - d.Item16);
  d.Item16 = item16;
  d.objectList = objectList;
  d.indirectTextIndex = indirectTextIndex;
  d.compressedText = compressedText;
}

// The picture as the engine drew it: a snapshot put back shows its own tick at once, with the
// inventory, the spells and the messages as they were
const size_t VIEWPORT_BMP_BYTES = 15232; // CSBCode.cpp: allocateMemory(15232, 1)

void SaveScreen(SnapshotBytes &b)
{
  SnapshotPut(b, &videoMode, sizeof videoMode);
  SnapshotPut(b, globalPalette, 32);
  SnapshotPut(b, LScreenBase, sizeof LScreenBase);
  PutFixed(b, d.pViewportBMP, VIEWPORT_BMP_BYTES);
}

void RestoreScreen(SnapshotReader &r)
{
  r.Get(&videoMode, sizeof videoMode);
  r.Get(globalPalette, 32); // the screens after a win fade to palettes of their own
  r.Get(LScreenBase, sizeof LScreenBase);
  GetFixed(r, d.pViewportBMP);
}

void SaveMisc(SnapshotBytes &b)
{
  SnapshotPut(b, &numRandomCalls, sizeof numRandomCalls);
  SnapshotPut(b, &totalMoveCount, sizeof totalMoveCount);
  SnapshotPut(b, &GameTime, sizeof GameTime);
  SnapshotPut(b, MouseQueue, sizeof MouseQueue);
  SnapshotPut(b, &pMouseQueue, sizeof pMouseQueue);
  SnapshotPut(b, &graphicRandom, sizeof graphicRandom);
  SnapshotPut(b, &viewportRandom, sizeof viewportRandom);
}

void RestoreMisc(SnapshotReader &r)
{
  r.Get(&numRandomCalls, sizeof numRandomCalls);
  r.Get(&totalMoveCount, sizeof totalMoveCount);
  r.Get(&GameTime, sizeof GameTime);
  r.Get(MouseQueue, sizeof MouseQueue);
  r.Get(&pMouseQueue, sizeof pMouseQueue);
  r.Get(&graphicRandom, sizeof graphicRandom);
  r.Get(&viewportRandom, sizeof viewportRandom);
}

// In this order: the heap blocks named in d and the dungeon's sizes before d itself
const Part parts[] =
{
  {"databases", DatabasesSnapshot::Save, DatabasesSnapshot::Restore},
  {"timers", GameTimersSnapshot::Save, GameTimersSnapshot::Restore},
  {"dataMap", SaveDataMap, RestoreDataMap},
  {"globalVariables", SaveGlobalVariables, RestoreGlobalVariables},
  {"heapOfD", SaveHeapOfD, RestoreHeapOfD},
  {"dungeon", SaveDungeon, RestoreDungeon},
  {"d", SaveD, RestoreD},
  {"misc", SaveMisc, RestoreMisc},
  {"playFile", PlayFile_SaveState, PlayFile_RestoreState},
  {"smartDiscard", SmartDiscard_SaveState, SmartDiscard_RestoreState},
  {"screen", SaveScreen, RestoreScreen},
  {"messages", Messages_SaveState, Messages_RestoreState},
  // The stores of the automap and the hero statistics, which read the dungeon put back before
  {"automap", AutoMap_SaveState, AutoMap_RestoreState},
};
const int PART_COUNT = (int)(sizeof parts / sizeof parts[0]);

// What is kept as a copy: put back after the parts
struct KeptPart
{
  const char *name;
  SnapshotKeptRef (*keep)(void);
  void (*restore)(const SnapshotKept &kept);
};

const KeptPart keptParts[] =
{
  {"automap transient", AutoMap_KeepTransient, AutoMap_RestoreTransient},
  {"statistics transient", HeroStats_KeepTransient, HeroStats_RestoreTransient},
};
const int KEPT_COUNT = (int)(sizeof keptParts / sizeof keptParts[0]);

struct Snapshot
{
  int tick;
  std::vector<SnapshotBytes> data; // one per part
  std::vector<SnapshotKeptRef> kept; // one per kept part
};

void Take(Snapshot &s)
{
  s.tick = (int)d.Time;
  s.data.resize(PART_COUNT);
  for (int i = 0; i < PART_COUNT; i++)
  {
    s.data[i].clear();
    parts[i].save(s.data[i]);
  }
  s.kept.resize(KEPT_COUNT);
  for (int i = 0; i < KEPT_COUNT; i++) s.kept[i] = keptParts[i].keep();
}

// Set when the engine came from the screens after a win: they used the memory the level's wall
// and floor bitmaps lived in, which d still points to
bool graphicsLost = false;

void Restore(const Snapshot &s)
{
  i32 levelBefore = d.partyLevel;
  for (int i = 0; i < PART_COUNT; i++)
  {
    SnapshotReader r(s.data[i]);
    parts[i].restore(r);
  }
  for (int i = 0; i < KEPT_COUNT && i < (int)s.kept.size(); i++) keptParts[i].restore(*s.kept[i]);
  // The level's walls and decorations, as on the way to another level (LoadPartyLevel)
  if (graphicsLost)
  {
    // ReadGraphicsForLevel keeps the bitmaps it reads in the same buffers each time, and reads
    // the walls and the floor again only for another set than the current one
    d.CurrentWallGraphic = -1;
    d.CurrentFloorAndCeilingGraphic = -1;
  }
  if (d.partyLevel != levelBefore || graphicsLost) ReadGraphicsForLevel();
  graphicsLost = false;
  ForceScreenDraw();
}

// ------------------------------------------------------------------------------ the store --
// The snapshots of the replay that runs, by tick: one every SNAPSHOT_EVERY ticks and one where
// it began. Each part is kept compressed; a part that has not changed since the snapshot before
// is the same block. The kept parts are kept as they are.
struct StoredPart
{
  size_t rawSize;
  std::vector<unsigned char> z;
};

typedef std::shared_ptr<const StoredPart> StoredPartRef;

struct StoredSnapshot
{
  std::vector<StoredPartRef> parts;
  std::vector<SnapshotKeptRef> kept;
};

std::map<int, StoredSnapshot> store;
size_t storeBytes = 0;
// The engine's call stack between two ticks, where the snapshots are taken: the same every time.
// From the screens after a win or a death it is put back to this, so that the main loop runs
// again and comes to a tick boundary, where the snapshot is put back.
SnapshotBytes mainLoopStack;
i32 mainLoopStartCSB = 0;
// The parts of the last snapshot taken, uncompressed, to see which are unchanged
Snapshot lastTaken;
std::vector<StoredPartRef> lastStored;

StoredPartRef Compress(const SnapshotBytes &raw)
{
  std::shared_ptr<StoredPart> p(new StoredPart);
  p->rawSize = raw.size();
  mz_ulong n = mz_compressBound((mz_ulong)raw.size());
  p->z.resize(n);
  if (mz_compress2(&p->z[0], &n, raw.empty() ? NULL : &raw[0], (mz_ulong)raw.size(), 1) != MZ_OK)
    n = 0;
  p->z.resize(n);
  p->z.shrink_to_fit();
  storeBytes += p->z.size();
  return p;
}

void Expand(const StoredPart &p, SnapshotBytes &raw)
{
  raw.resize(p.rawSize);
  mz_ulong n = (mz_ulong)p.rawSize;
  if (p.rawSize != 0) mz_uncompress(&raw[0], &n, &p.z[0], (mz_ulong)p.z.size());
}

void Forget(void)
{
  store.clear();
  storeBytes = 0;
  mainLoopStack.clear();
  lastTaken.data.clear();
  lastStored.clear();
}

void TakeStored(void)
{
  Snapshot s;
  Take(s);
  std::vector<StoredPartRef> parts(PART_COUNT);
  for (int i = 0; i < PART_COUNT; i++)
  {
    if (!lastStored.empty() && lastTaken.data[i] == s.data[i]) parts[i] = lastStored[i];
    else parts[i] = Compress(s.data[i]);
  }
  CallStack_Take(mainLoopStack);
  mainLoopStartCSB = StartCSB_State();
  store[s.tick].parts = parts;
  store[s.tick].kept = s.kept;
  lastTaken = s;
  lastStored = parts;
}

// The latest snapshot at or before the target, put back; false when there is none
bool RestoreStored(int target, int *restored)
{
  std::map<int, StoredSnapshot>::iterator it = store.upper_bound(target);
  if (it == store.begin()) return false;
  --it;
  Snapshot s;
  s.tick = it->first;
  s.data.resize(PART_COUNT);
  for (int i = 0; i < PART_COUNT; i++) Expand(*it->second.parts[i], s.data[i]);
  s.kept = it->second.kept;
  Restore(s);
  *restored = s.tick;
  return true;
}

// --rewind-check: going back at the given ticks as Down or Page Down would, and at the end a
// fingerprint of the game's state, to compare with a run that did not go back
bool checking = false;
std::vector<std::pair<int, int> > checkPresses; // tick, ticks back
size_t nextCheckPress = 0;
bool checkSawReplay = false;
// "end:2500": at the end of the replay (or the win) go back that far, as the buttons do there,
// and let it run to its end again
int checkEndBack = 0;
// "shot:135000": a fingerprint of the screen at that tick, every time the replay is there
int checkShotTick = -1;
bool checkEndPressDue = false;
bool checkEndPressDone = false;

void CheckEnd(void)
{
  Snapshot s;
  Take(s);
  printf("rewind-check: replay ended at tick %d, %d snapshots, %zu bytes\n",
         (int)d.Time, (int)store.size(), storeBytes);
  for (int i = 0; i < PART_COUNT; i++)
  {
    uint64_t h = 1469598103934665603ULL;
    for (size_t j = 0; j < s.data[i].size(); j++) h = (h ^ s.data[i][j]) * 1099511628211ULL;
    printf("rewind-check: part %-16s %016llx\n", parts[i].name, (unsigned long long)h);
    // and the part itself, to see where two runs differ (cmp -l)
    char name[64];
    snprintf(name, sizeof name, "rewind-check-%s.bin", parts[i].name);
    FILE *f = fopen(name, "wb");
    if (f != NULL)
    {
      if (!s.data[i].empty()) fwrite(&s.data[i][0], 1, s.data[i].size(), f);
      fclose(f);
    }
  }
  fflush(stdout);
  exit(0);
}

void CheckAtTickBoundary(void)
{
  if (!IsPlayFileOpen())
  {
    if (checkSawReplay && !checkEndPressDue) CheckEnd();
    return;
  }
  checkSawReplay = true;
  int tick = (int)d.Time;
  if (tick == checkShotTick)
  {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < sizeof LScreenBase; i++) h = (h ^ ((const unsigned char *)LScreenBase)[i]) * 1099511628211ULL;
    printf("rewind-check: shot at tick %d: %016llx\n", tick, (unsigned long long)h);
    fflush(stdout);
  }
  if (nextCheckPress < checkPresses.size() && tick >= checkPresses[nextCheckPress].first
      && !PlaybackControl_RewindPending() && !PlaybackControl_IsSkipping())
  {
    printf("rewind-check: at tick %d back %d\n", tick, checkPresses[nextCheckPress].second);
    fflush(stdout);
    // At the speed it runs: --quick is max. A negative count runs ahead (Up, Page Up, Right)
    int n = checkPresses[nextCheckPress].second;
    int speed = NoSpeedLimit != 0 ? PLAYBACK_SPEED_MAX : PlaybackControl_SpeedIndex();
    if (n < 0) PlaybackControl_SkipAhead(tick, -n, speed);
    else PlaybackControl_RewindBy(tick, n, speed);
    nextCheckPress++;
  }
}

void AtTickBoundary(void)
{
  if (checking) CheckAtTickBoundary();
  // The snapshots of a replay that ended stay: the game can go back in it (PlayfileCanGoBack)
  // until another game starts (Cleanup) or another replay opens
  if (!IsPlayFileOpen()) return;
  if (PlaybackControl_RewindPending())
  {
    int restored;
    if (RestoreStored(PlaybackControl_RewindTarget(), &restored))
    {
      if (checking) printf("rewind-check: restored tick %d for target %d\n", restored, PlaybackControl_RewindTarget());
      PlaybackControl_RewindStart(restored);
    }
    else PlaybackControl_CancelRewind();
    return;
  }
  int tick = (int)d.Time;
  // A fast forward over ticks the replay has been at before: on from the latest snapshot on the way
  int limit = PlaybackControl_JumpLimit();
  if (limit > tick)
  {
    std::map<int, StoredSnapshot>::iterator it = store.upper_bound(limit);
    if (it != store.begin() && (--it)->first > tick)
    {
      int restored;
      if (RestoreStored(it->first, &restored))
      {
        if (checking) printf("rewind-check: at tick %d jumped to %d\n", tick, restored);
        PlaybackControl_JumpedTo(restored);
        return;
      }
    }
  }
  if ((store.empty() || tick % SNAPSHOT_EVERY == 0) && store.find(tick) == store.end()) TakeStored();
}

// ----------------------------------------------------------------------------------- test --

bool testing = false;
bool testSawReplay = false;

enum TestPhase {TEST_NONE, TEST_FIRST, TEST_SECOND};
TestPhase phase = TEST_NONE;
Snapshot testSnapshot;
std::vector<unsigned char> imageFirst;
Snapshot endOfFirst;
std::vector<unsigned char> imageAtSnapshot;
int stretches = 0, differingStretches = 0;
std::map<std::string, int> differCount; // symbol -> stretches in which it differed

// The whole of the program's variables, compared byte by byte. Not under AddressSanitizer,
// whose guard zones between them may not be read.
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__)
extern "C" char __data_start[];
extern "C" char _end[];

struct Symbol
{
  uintptr_t address;
  size_t size;
  std::string name;
  bool operator<(const Symbol &o) const { return address < o.address; }
};
std::vector<Symbol> symbols;

void LoadSymbols(void)
{
  char exe[2048];
  ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
  if (n <= 0) return;
  exe[n] = 0;
  std::string command = std::string("nm -C -S --defined-only '") + exe + "' 2>/dev/null";
  FILE *p = popen(command.c_str(), "r");
  if (p == NULL) return;
  char line[4096];
  uintptr_t dAt = 0;
  while (fgets(line, sizeof line, p) != NULL)
  {
    unsigned long long a, sz;
    char type;
    int used = 0;
    if (sscanf(line, "%llx %llx %c %n", &a, &sz, &type, &used) != 3) continue;
    if (strchr("bBdD", type) == NULL) continue;
    std::string name(line + used);
    while (!name.empty() && (name.back() == '\n' || name.back() == '\r')) name.pop_back();
    Symbol s = {(uintptr_t)a, (size_t)sz, name};
    symbols.push_back(s);
    if (name == "d") dAt = (uintptr_t)a;
  }
  pclose(p);
  uintptr_t base = (uintptr_t)&d - dAt;
  for (size_t i = 0; i < symbols.size(); i++) symbols[i].address += base;
  std::sort(symbols.begin(), symbols.end());
}

std::string SymbolAt(uintptr_t a, size_t *offset)
{
  Symbol key = {a, 0, ""};
  std::vector<Symbol>::iterator it = std::upper_bound(symbols.begin(), symbols.end(), key);
  if (it != symbols.begin())
  {
    --it;
    if (a < it->address + (it->size ? it->size : 1))
    {
      *offset = a - it->address;
      return it->name;
    }
  }
  *offset = 0;
  char buf[32];
  snprintf(buf, sizeof buf, "?%lx", (unsigned long)a);
  return buf;
}

void Capture(std::vector<unsigned char> &image)
{
  image.assign(__data_start, _end);
}

// The variables that differ between the two images, each with its first differing offset
void Compare(const std::vector<unsigned char> &a, const std::vector<unsigned char> &b, int tick)
{
  std::map<std::string, size_t> differ;
  for (size_t i = 0; i < a.size(); i++)
  {
    if (a[i] == b[i]) continue;
    size_t offset;
    std::string name = SymbolAt((uintptr_t)(__data_start + i), &offset);
    if (differ.find(name) == differ.end()) differ[name] = offset;
  }
  stretches++;
  if (differ.empty()) return;
  differingStretches++;
  printf("rewind-test: tick %d:", tick);
  for (std::map<std::string, size_t>::iterator it = differ.begin(); it != differ.end(); ++it)
  {
    printf(" %s+%zu", it->first.c_str(), it->second);
    differCount[it->first]++;
  }
  printf("\n");
  fflush(stdout);
}
#else
void LoadSymbols(void) {}
void Capture(std::vector<unsigned char> &image) { image.clear(); }
void Compare(const std::vector<unsigned char> &, const std::vector<unsigned char> &, int) { stretches++; }
#endif

void TestEnd(void)
{
  printf("rewind-test: replay ended at tick %d; %d stretches, %d with differences\n",
         (int)d.Time, stretches, differingStretches);
  for (std::map<std::string, int>::iterator it = differCount.begin(); it != differCount.end(); ++it)
    printf("rewind-test:   %6d  %s\n", it->second, it->first.c_str());
  for (int i = 0; i < PART_COUNT && i < (int)testSnapshot.data.size(); i++)
  {
    StoredPartRef z = Compress(testSnapshot.data[i]);
    printf("rewind-test: part %-16s %8zu bytes, %8zu compressed\n", parts[i].name, testSnapshot.data[i].size(), z->z.size());
  }
  fflush(stdout);
  exit(0);
}

void TestAtTickBoundary(void)
{
  if (!IsPlayFileOpen())
  {
    if (testSawReplay) TestEnd();
    return;
  }
  testSawReplay = true;
  int tick = (int)d.Time;
  static int lastReport = -1;
  if (tick / 5000 != lastReport)
  {
    lastReport = tick / 5000;
    printf("rewind-test: at tick %d (phase %d)\n", tick, (int)phase);
    fflush(stdout);
  }
  if (phase == TEST_NONE)
  {
    if (tick % SNAPSHOT_EVERY != 0) return;
    Take(testSnapshot);
    Capture(imageAtSnapshot);
    phase = TEST_FIRST;
    return;
  }
  if (tick < testSnapshot.tick + SNAPSHOT_EVERY) return;
  if (phase == TEST_FIRST)
  {
    // The end of the stretch: once more from its start
    Capture(imageFirst);
    Take(endOfFirst);
    Restore(testSnapshot);
    {
      // What the restore left as it was at the end of the stretch
      std::vector<unsigned char> restored;
      Capture(restored);
      printf("rewind-test: not restored ");
      Compare(imageAtSnapshot, restored, testSnapshot.tick);
    }
    phase = TEST_SECOND;
    return;
  }
  std::vector<unsigned char> imageSecond;
  Capture(imageSecond);
  Compare(imageFirst, imageSecond, tick);
  {
    // The game's state as the snapshot holds it: the same after both runs?
    Snapshot endOfSecond;
    Take(endOfSecond);
    for (int i = 0; i < PART_COUNT; i++)
    {
      const SnapshotBytes &a = endOfFirst.data[i], &b = endOfSecond.data[i];
      if (a == b) continue;
      size_t at = 0;
      while (at < a.size() && at < b.size() && a[at] == b[at]) at++;
      printf("rewind-test: tick %d: part %s differs at byte %zu (sizes %zu, %zu)\n",
             tick, parts[i].name, at, a.size(), b.size());
      differCount[std::string("part ") + parts[i].name]++;
      // Both, to find out what differs
      for (int run = 0; run < 2; run++)
      {
        char name[96];
        snprintf(name, sizeof name, "rewind-test-%s-%d-%c.bin", parts[i].name, tick, run == 0 ? 'a' : 'b');
        FILE *f = fopen(name, "wb");
        if (f == NULL) continue;
        const SnapshotBytes &bytes = run == 0 ? a : b;
        if (!bytes.empty()) fwrite(&bytes[0], 1, bytes.size(), f);
        fclose(f);
      }
    }
    fflush(stdout);
  }
  if (stretches % 20 == 0)
  {
    printf("rewind-test: tick %d\n", tick);
    fflush(stdout);
  }
  Take(testSnapshot);
  Capture(imageAtSnapshot);
  phase = TEST_FIRST;
}
}

void ReplaySnapshot_EnableTest(void)
{
  testing = true;
  LoadSymbols();
}

bool ReplaySnapshot_TestIsRunning(void)
{
  return testing || checking;
}

void ReplaySnapshot_ReplayOver(void)
{
  if (testing && testSawReplay) TestEnd();
  if (checking && checkSawReplay)
  {
    // The end: the press comes between two messages (BetweenMessages), the engine is in the middle of its code here
    if (checkEndBack > 0 && !checkEndPressDone) checkEndPressDue = true;
    else CheckEnd();
  }
}

// How far back from where a replay ended or stopped: its snapshots reach (what ReplayBack asks)
bool ReplaySnapshot_BackFromEnd(int n, int speedBefore)
{
  bool reopened = false;
  if (!IsPlayFileOpen())
  {
    if (!PlayfileCanGoBack() || store.empty()) return false;
    reopened = true;
  }
  int from = reopened ? PlayfileEndTick() : (int)d.Time;
  int first = store.begin()->first;
  if (n > from - first) n = from - first; // no snapshot reaches further back
  if (n <= 0) return false;
  if (reopened && !PlayfileReopenForRewind()) return false;
  return PlaybackControl_RewindBy(from, n, speedBefore);
}

// The screens after a win or a death, and the end sequence of a win, wait in calls the main loop
// never comes back to; and with them its tick boundary, where a snapshot is put back. A rewind
// asked for there is given the call stack of the main loop again, which runs on from its wait for
// a message to the next tick boundary.
bool EndScreenIsUp(void)
{
  return CallStack_Contains(st_ShowCredits) || CallStack_Contains(st_FusionSequence);
}

void ReplaySnapshot_BetweenMessages(void)
{
  if (checking && checkEndPressDue)
  {
    checkEndPressDue = false;
    checkEndPressDone = true;
    printf("rewind-check: end at tick %d back %d%s\n", IsPlayFileOpen() ? (int)d.Time : PlayfileEndTick(), checkEndBack,
           IsPlayFileOpen() ? "" : " (the replay's file was closed)");
    fflush(stdout);
    if (!ReplaySnapshot_BackFromEnd(checkEndBack, PLAYBACK_SPEED_MAX)) { printf("rewind-check: nothing to go back to\n"); CheckEnd(); }
    PlaybackControl_SetSpeedIndex(PLAYBACK_SPEED_MAX);
    NoSpeedLimit = 0x20000000;
  }
  if (!PlaybackControl_RewindPending() || !IsPlayFileOpen() || mainLoopStack.empty()) return;
  if (!EndScreenIsUp()) return;
  CallStack_Put(mainLoopStack);
  StartCSB_SetState(mainLoopStartCSB);
  MainLoop_WaitForMessage();
  PlayfileAwaitRestore();
  graphicsLost = true;
  GameIsComplete = false;
  endSequencePlaying = false;
}

bool ReplaySnapshot_EndScreenWaits(void)
{
  return IsPlayFileOpen() && PlaybackControl_RewindPending();
}

void ReplaySnapshot_AtTickBoundary(void)
{
  // A test or check runs at max: --quick alone leaves the replay's own pace at 1x
  if ((testing || checking) && IsPlayFileOpen() && !PlaybackControl_IsMax()
      && !PlaybackControl_IsSkipping() && !PlaybackControl_RewindPending())
  {
    PlaybackControl_SetSpeedIndex(PLAYBACK_SPEED_MAX);
    NoSpeedLimit = 0x20000000;
  }
  if (testing) TestAtTickBoundary();
  else AtTickBoundary();
}

void ReplaySnapshot_EnableCheck(const char *presses)
{
  checking = true;
  int tick, back, used;
  const char *p = presses;
  for (;;)
  {
    if (strncmp(p, "shot:", 5) == 0 && sscanf(p + 5, "%d%n", &back, &used) == 1)
    {
      checkShotTick = back;
      p += 5 + used;
      if (*p == ',') p++;
      continue;
    }
    if (strncmp(p, "end:", 4) == 0 && sscanf(p + 4, "%d%n", &back, &used) == 1)
    {
      checkEndBack = back;
      p += 4 + used;
      if (*p == ',') p++;
      continue;
    }
    if (sscanf(p, "%d:%d%n", &tick, &back, &used) != 2) break;
    checkPresses.push_back(std::make_pair(tick, back));
    p += used;
    if (*p == ',') p++;
  }
}

void ReplaySnapshot_Forget(void)
{
  Forget();
}

int ReplaySnapshot_Count(void)
{
  return (int)store.size();
}

size_t ReplaySnapshot_Bytes(void)
{
  return storeBytes;
}

int ReplaySnapshot_First(void)
{
  return store.empty() ? -1 : store.begin()->first;
}
