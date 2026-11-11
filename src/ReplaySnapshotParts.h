#ifndef REPLAYSNAPSHOTPARTS_H
#define REPLAYSNAPSHOTPARTS_H

// How the engine's units hand their game state to a replay snapshot (ReplaySnapshot.h): each
// writes its part into bytes and reads it back the same way. Plain memory goes in as it is;
// what points into the heap goes in by content, and on the way back keeps the block it has now
// (made larger when the snapshot's content does not fit).

#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>

typedef std::vector<unsigned char> SnapshotBytes;

inline void SnapshotPut(SnapshotBytes &b, const void *p, size_t n)
{
  const unsigned char *c = (const unsigned char *)p;
  b.insert(b.end(), c, c + n);
}

class SnapshotReader
{
public:
  SnapshotReader(const SnapshotBytes &b) : m_b(b), m_at(0) {}
  void Get(void *p, size_t n)
  {
    if (n == 0) return;
    memcpy(p, &m_b[m_at], n);
    m_at += n;
  }
  const unsigned char *Skip(size_t n)
  {
    const unsigned char *p = &m_b[m_at];
    m_at += n;
    return p;
  }

private:
  const SnapshotBytes &m_b;
  size_t m_at;
};

// The playfile reader (Recording.cpp): where it stands in the file and what it has read so far
void PlayFile_SaveState(SnapshotBytes &b);
void PlayFile_RestoreState(SnapshotReader &r);

// Where Smart Discard stands (SmartDiscard.cpp, with SmartDiscardLegacy.cpp)
void SmartDiscard_SaveState(SnapshotBytes &b);
void SmartDiscard_RestoreState(SnapshotReader &r);

// The lines of text at the bottom of the screen and those waiting to be printed (CSBCode.cpp)
void Messages_SaveState(SnapshotBytes &b);
void Messages_RestoreState(SnapshotReader &r);

// State held in C++ objects (containers) that is small and lasts from one tick to the next:
// a snapshot keeps a copy of it rather than bytes
class SnapshotKept
{
public:
  virtual ~SnapshotKept(void) {}
};
typedef std::shared_ptr<const SnapshotKept> SnapshotKeptRef;

// The automap and the hero statistics (AutoMapHooks.cpp): what their .map file holds, as bytes.
// The player's own notes on the map stay as they are.
void AutoMap_SaveState(SnapshotBytes &b);
void AutoMap_RestoreState(SnapshotReader &r);
// What the automap keeps track of between ticks: the last frame, the links it watches
SnapshotKeptRef AutoMap_KeepTransient(void);
void AutoMap_RestoreTransient(const SnapshotKept &kept);
// What the hero statistics keep track of between ticks (HeroStatsHooks.cpp): the blows in
// progress, the damage not yet applied, the doors whose bash timer runs
SnapshotKeptRef HeroStats_KeepTransient(void);
void HeroStats_RestoreTransient(const SnapshotKept &kept);

#endif
