#include "PlaybackControl.h"

namespace
{
const double FACTORS[PLAYBACK_SPEED_COUNT] = {0.5, 1, 2, 4, 8, 16, 32, 64};
const char *const LABELS[PLAYBACK_SPEED_COUNT] = {"0.5x", "1x", "2x", "4x", "8x", "16x", "32x", "64x"};

int speedIndex = PLAYBACK_SPEED_NORMAL;
bool paused = false;

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
}

bool PlaybackControl_MutesSound(void) { return PlaybackControl_Factor() >= 4; }

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
