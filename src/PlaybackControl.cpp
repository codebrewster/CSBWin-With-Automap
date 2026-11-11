#include "PlaybackControl.h"

#include <map>
#include <set>
#include <tuple>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
const double FACTORS[PLAYBACK_SPEED_COUNT] = {0.5, 1, 2, 4, 8, 16, 32, 64, 64};
const char *const LABELS[PLAYBACK_SPEED_COUNT] = {"0.5x", "1x", "2x", "4x", "8x", "16x", "32x", "64x", "max"};

int speedIndex = PLAYBACK_SPEED_NORMAL;
bool paused = false;

typedef std::tuple<int, int, int> Cell;
std::set<Cell> breakpoints;
bool hasLastCell = false;
bool pauseIsDue = false;

bool soundsWhenFast = true;
int soundGapMs = PLAYBACK_SOUND_GAP_MS;
// When each sound last started in a fast replay
std::map<uint64_t, uint64_t> soundStarted;
Cell lastCell;

int totalTicks = -1;
int skipTarget = -1;
int skipSpeed = PLAYBACK_SPEED_NORMAL;
bool skipWasPaused = false;
int pauseTarget = -1;
bool speedSyncDue = false;
bool skipIsRewind = false;
// A rewind waiting for the engine to put a snapshot back, and what it goes back to after
int rewindTarget = -1;
int rewindSpeed = PLAYBACK_SPEED_NORMAL;
bool rewindWasPaused = false;

int Clamped(int index)
{
  if (index < 0) return 0;
  if (index >= PLAYBACK_SPEED_COUNT) return PLAYBACK_SPEED_COUNT - 1;
  return index;
}
}

int PlaybackControl_SpeedIndex(void) { return speedIndex; }
// Choosing a speed by hand ends a skip
void PlaybackControl_SetSpeedIndex(int index) { speedIndex = Clamped(index); skipTarget = -1; skipIsRewind = false; }
void PlaybackControl_Faster(void) { PlaybackControl_SetSpeedIndex(speedIndex + 1); }
void PlaybackControl_Slower(void) { PlaybackControl_SetSpeedIndex(speedIndex - 1); }
bool PlaybackControl_IsMax(void) { return speedIndex == PLAYBACK_SPEED_MAX; }
double PlaybackControl_Factor(void) { return FACTORS[speedIndex]; }
double PlaybackControl_FactorOf(int index) { return FACTORS[Clamped(index)]; }
const char *PlaybackControl_LabelOf(int index) { return LABELS[Clamped(index)]; }

bool PlaybackControl_IsPaused(void) { return paused; }
void PlaybackControl_SetPaused(bool value) { paused = value; }
void PlaybackControl_TogglePause(void) { paused = !paused; }

void PlaybackControl_Reset(void)
{
  speedIndex = PLAYBACK_SPEED_NORMAL;
  paused = false;
  breakpoints.clear();
  hasLastCell = false;
  pauseIsDue = false;
  totalTicks = -1;
  skipTarget = -1;
  skipIsRewind = false;
  pauseTarget = -1;
  speedSyncDue = false;
  rewindTarget = -1;
}

void PlaybackControl_SetTotalTicks(int ticks) { totalTicks = ticks; }
int PlaybackControl_TotalTicks(void) { return totalTicks; }

void PlaybackControl_FormatTime(double seconds, char *buf, int size)
{
  long s = seconds < 0 ? 0 : (long)(seconds + 0.5);
  snprintf(buf, size, "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
}

// The same reading of the lines as PLAYFILE::Play (Recording.cpp), for the times only
int PlaybackControl_ScanTotalTicks(const char *path)
{
  FILE *f = fopen(path, "r");
  if (f == NULL) return -1;
  char line[256];
  long oldTime = 0, time = -1;
  std::vector<long> txyf;                     // delta, xyf index
  std::vector<std::pair<long, long> > txyfr;  // txyf index, random
  bool first = true;
  while (fgets(line, sizeof line, f) != NULL)
  {
    if (first) { first = false; continue; } // the version line
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == 0) continue;
    long delta = -1;
    if (line[0] == '+')
    {
      unsigned d, a, b, c, r;
      int n = sscanf(line + 1, "%x %x %x %x %x", &d, &a, &b, &c, &r);
      if (n != 5 && n != 3) continue;
      delta = d;
    }
    else
    {
      unsigned f1, f2, f3, f4, f5;
      int n = sscanf(line, "%08x %04x %04x %04x %08x", &f1, &f2, &f3, &f4, &f5);
      if (n == 5) { time = f1; oldTime = time; continue; }
      if (n == 3)
      {
        delta = f1;
        txyf.push_back(delta);
        txyf.push_back(f2);
      }
      else if (n == 2)
      {
        if ((size_t)f1 * 2 >= txyf.size()) continue;
        delta = txyf[(size_t)f1 * 2];
        txyfr.push_back(std::make_pair((long)f1, (long)f2));
      }
      else if (n == 1)
      {
        if ((size_t)f1 >= txyfr.size()) continue;
        long t = txyfr[f1].first;
        if ((size_t)t * 2 >= txyf.size()) continue;
        delta = txyf[(size_t)t * 2];
      }
      else continue;
    }
    time = oldTime + delta;
    oldTime = time;
  }
  fclose(f);
  return (int)time;
}

void PlaybackControl_SkipAhead(int now, int n, int speedBefore)
{
  if (n <= 0) return;
  if (skipTarget < 0)
  {
    skipTarget = now + n;
    skipSpeed = Clamped(speedBefore);
    skipWasPaused = paused;
    skipIsRewind = false;
  }
  else skipTarget += n;
  if (totalTicks > 0 && skipTarget > totalTicks) skipTarget = totalTicks;
  // At max, and running (the pause after it is the one it came from)
  speedIndex = PLAYBACK_SPEED_MAX;
  paused = false;
  speedSyncDue = true;
}

// The end of a skip, where it heads or because it is no longer needed: the speed and the pause
// it came from
static void EndSkip(void)
{
  skipTarget = -1;
  skipIsRewind = false;
  speedIndex = skipSpeed;
  speedSyncDue = true;
  if (skipWasPaused) pauseIsDue = true;
}

bool PlaybackControl_RewindBy(int now, int n, int speedBefore)
{
  if (n <= 0) return rewindTarget >= 0;
  int from = rewindTarget >= 0 ? rewindTarget : skipTarget >= 0 ? skipTarget : now;
  int target = from - n;
  if (target < 0) target = 0;
  if (rewindTarget < 0 && skipTarget >= 0 && target >= now)
  {
    // Still ahead of where the skip stands
    skipTarget = target;
    if (target == now) EndSkip();
    return false;
  }
  if (rewindTarget < 0)
  {
    // From a skip: the speed and the pause it would have gone back to
    rewindSpeed = skipTarget >= 0 ? skipSpeed : Clamped(speedBefore);
    rewindWasPaused = skipTarget >= 0 ? skipWasPaused : paused;
  }
  rewindTarget = target;
  return true;
}

bool PlaybackControl_RewindPending(void) { return rewindTarget >= 0; }
int PlaybackControl_RewindTarget(void) { return rewindTarget; }

void PlaybackControl_RewindStart(int restored)
{
  if (rewindTarget < 0) return;
  hasLastCell = false;
  pauseIsDue = false;
  if (restored < rewindTarget)
  {
    skipTarget = rewindTarget;
    skipSpeed = rewindSpeed;
    skipWasPaused = rewindWasPaused;
    skipIsRewind = true;
    speedIndex = PLAYBACK_SPEED_MAX;
    paused = false;
  }
  else
  {
    skipTarget = -1;
    skipIsRewind = false;
    speedIndex = rewindSpeed;
    paused = rewindWasPaused;
  }
  speedSyncDue = true;
  rewindTarget = -1;
}

void PlaybackControl_CancelRewind(void) { rewindTarget = -1; }

bool PlaybackControl_SkipIsRewind(void) { return skipIsRewind && skipTarget >= 0; }

bool PlaybackControl_IsSkipping(void) { return skipTarget >= 0; }
int PlaybackControl_SkipTarget(void) { return skipTarget; }

bool PlaybackControl_SetPauseTarget(int now, int tick)
{
  if (tick <= 0) { pauseTarget = -1; return true; }
  if (tick <= now) return false;
  pauseTarget = tick;
  return true;
}

int PlaybackControl_PauseTarget(void) { return pauseTarget; }

void PlaybackControl_AtTick(int tickAfter)
{
  if (skipTarget >= 0 && tickAfter >= skipTarget) EndSkip();
  if (pauseTarget >= 0 && tickAfter >= pauseTarget)
  {
    pauseTarget = -1;
    pauseIsDue = true;
  }
}

bool PlaybackControl_TakeSpeedSync(void)
{
  bool due = speedSyncDue;
  speedSyncDue = false;
  return due;
}

void PlaybackControl_ToggleBreakpoint(int level, int x, int y)
{
  Cell cell(level, x, y);
  if (!breakpoints.erase(cell)) breakpoints.insert(cell);
}

bool PlaybackControl_HasBreakpoint(int level, int x, int y)
{
  return breakpoints.count(Cell(level, x, y)) != 0;
}

int PlaybackControl_BreakpointCount(void) { return (int)breakpoints.size(); }

bool PlaybackControl_PartyAt(int level, int x, int y)
{
  Cell cell(level, x, y);
  bool stepped = !hasLastCell || cell != lastCell;
  hasLastCell = true;
  lastCell = cell;
  // On the way back to a tick the party walks where it walked before
  if (PlaybackControl_SkipIsRewind()) return false;
  if (!stepped || !breakpoints.erase(cell)) return false;
  pauseIsDue = true;
  return true;
}

void PlaybackControl_ApplyDuePause(void)
{
  if (!pauseIsDue) return;
  pauseIsDue = false;
  paused = true;
}

bool PlaybackControl_HoldsTicks(void) { return pauseIsDue || paused; }

bool PlaybackControl_IsFastForSound(void) { return PlaybackControl_Factor() >= PLAYBACK_FAST_FOR_SOUND; }

void PlaybackControl_SetSoundsWhenFast(bool on, int gapMs)
{
  soundsWhenFast = on;
  soundGapMs = gapMs < 0 ? 0 : gapMs;
}

bool PlaybackControl_SoundsWhenFast(void) { return soundsWhenFast; }
int PlaybackControl_SoundGap(void) { return soundGapMs; }

bool PlaybackControl_SoundMayPlay(uint64_t key, uint64_t nowMs)
{
  if (!PlaybackControl_IsFastForSound()) return true;
  if (!soundsWhenFast) return false;
  std::map<uint64_t, uint64_t>::iterator last = soundStarted.find(key);
  // A clock that went back starts afresh
  if (last != soundStarted.end() && nowMs >= last->second && nowMs - last->second < (uint64_t)soundGapMs) return false;
  soundStarted[key] = nowMs;
  return true;
}

int PlaybackControl_ChecksPerTimer(int base)
{
  double factor = PlaybackControl_Factor();
  if (factor <= 1) return base;
  return base * (int)(factor + 0.999);
}

bool PlaybackClock::Due(uint64_t nowMs, double factor, bool isPaused, double msPerBlank)
{
  if (!m_started || nowMs < m_last)
  {
    m_started = true;
    m_last = nowMs;
    m_budget = 0.0;
    return false;
  }
  uint64_t delta = nowMs - m_last;
  m_last = nowMs;
  if (isPaused)
  {
    m_budget = 0.0;
    return false;
  }
  if (delta < STALL_MS && msPerBlank > 0) m_budget += (double)delta * factor / msPerBlank;
  double most = BACKLOG + 2 * factor;
  if (m_budget > most) m_budget = most;
  if (m_budget < 1.0) return false;
  m_budget -= 1.0;
  return true;
}
