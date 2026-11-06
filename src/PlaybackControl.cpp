#include "PlaybackControl.h"

#include <map>
#include <set>
#include <tuple>

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

int Clamped(int index)
{
  if (index < 0) return 0;
  if (index >= PLAYBACK_SPEED_COUNT) return PLAYBACK_SPEED_COUNT - 1;
  return index;
}
}

int PlaybackControl_SpeedIndex(void) { return speedIndex; }
void PlaybackControl_SetSpeedIndex(int index) { speedIndex = Clamped(index); }
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
