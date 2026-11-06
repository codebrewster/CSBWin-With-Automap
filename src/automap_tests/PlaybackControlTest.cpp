#include "TestSupport.h"
#include "../PlaybackControl.h"

#include <cstring>

// Blanks a clock gives in ms milliseconds, asked every step milliseconds (several asks per step,
// as the platform layer asks several times per timer message)
static int BlanksIn(PlaybackClock &clock, uint64_t &now, uint64_t ms, double factor, bool paused,
                    int asksPerStep = 64, uint64_t step = 10)
{
  int blanks = 0;
  for (uint64_t t = 0; t < ms; t += step)
  {
    now += step;
    for (int i = 0; i < asksPerStep; i++)
      if (clock.Due(now, factor, paused, 17.0)) blanks++;
  }
  return blanks;
}

static void TheSpeedsGoFromHalfTo64AndMax(void)
{
  PlaybackControl_Reset();
  CHECK(PlaybackControl_Factor() == 1.0);
  CHECK(!PlaybackControl_IsMax());
  CHECK(std::strcmp(PlaybackControl_LabelOf(0), "0.5x") == 0);
  CHECK(std::strcmp(PlaybackControl_LabelOf(PLAYBACK_SPEED_MAX - 1), "64x") == 0);
  CHECK(std::strcmp(PlaybackControl_LabelOf(PLAYBACK_SPEED_COUNT - 1), "max") == 0);
  for (int i = 0; i < 20; i++) PlaybackControl_Faster();
  CHECK(PlaybackControl_IsMax());
  CHECK(PlaybackControl_IsFastForSound());
  PlaybackControl_Slower();
  CHECK(!PlaybackControl_IsMax());
  CHECK(PlaybackControl_Factor() == 64.0);
  for (int i = 0; i < 20; i++) PlaybackControl_Slower();
  CHECK(PlaybackControl_Factor() == 0.5);
  PlaybackControl_SetSpeedIndex(99);
  CHECK(PlaybackControl_SpeedIndex() == PLAYBACK_SPEED_COUNT - 1);
  PlaybackControl_SetPaused(true);
  PlaybackControl_Reset();
  CHECK(!PlaybackControl_IsPaused());
  CHECK(PlaybackControl_SpeedIndex() == PLAYBACK_SPEED_NORMAL);
}

// Below 4x every sound plays; from 4x on, max included, the same sound waits for the gap and
// other sounds do not; replay_sounds 0 drops them all when fast
static void SoundsThinOutFrom4x(void)
{
  PlaybackControl_Reset();
  PlaybackControl_SetSoundsWhenFast(true, 100);
  PlaybackControl_SetSpeedIndex(2); // 2x
  CHECK(!PlaybackControl_IsFastForSound());
  CHECK(PlaybackControl_SoundMayPlay(1, 1000) && PlaybackControl_SoundMayPlay(1, 1001));
  PlaybackControl_Faster();         // 4x
  CHECK(PlaybackControl_IsFastForSound());
  CHECK(PlaybackControl_SoundMayPlay(1, 2000));
  CHECK(!PlaybackControl_SoundMayPlay(1, 2050));   // the same sound too soon
  CHECK(PlaybackControl_SoundMayPlay(2, 2050));    // another one
  CHECK(PlaybackControl_SoundMayPlay(1, 2100));    // the gap has passed
  CHECK(PlaybackControl_SoundMayPlay(1, 50));      // a clock that went back
  PlaybackControl_SetSpeedIndex(PLAYBACK_SPEED_MAX);
  CHECK(PlaybackControl_IsFastForSound() && !PlaybackControl_SoundMayPlay(1, 60));
  PlaybackControl_SetSoundsWhenFast(false, 100);
  CHECK(!PlaybackControl_SoundMayPlay(3, 5000));
  PlaybackControl_SetSpeedIndex(PLAYBACK_SPEED_NORMAL);
  CHECK(PlaybackControl_SoundMayPlay(3, 5000));
  PlaybackControl_SetSoundsWhenFast(true, -5);
  CHECK(PlaybackControl_SoundGap() == 0);
  PlaybackControl_SetSoundsWhenFast(true, PLAYBACK_SOUND_GAP_MS);
  PlaybackControl_Reset();
}

static void EnoughChecksForEverySpeed(void)
{
  PlaybackControl_SetSpeedIndex(0);
  CHECK(PlaybackControl_ChecksPerTimer(10) == 10);
  PlaybackControl_SetSpeedIndex(PLAYBACK_SPEED_NORMAL);
  CHECK(PlaybackControl_ChecksPerTimer(10) == 10);
  PlaybackControl_SetSpeedIndex(PLAYBACK_SPEED_COUNT - 1);
  CHECK(PlaybackControl_ChecksPerTimer(10) == 640);
  PlaybackControl_Reset();
}

static void TheClockRunsAtTheFactor(void)
{
  // 1000 ms / 17 ms = 58.8 blanks at 1x
  const double factors[] = {0.5, 2, 8, 64};
  for (double factor : factors)
  {
    PlaybackClock clock;
    uint64_t now = 5000;
    int blanks = BlanksIn(clock, now, 10000, factor, false, (int)(factor + 2));
    double expected = 10000.0 / 17.0 * factor;
    CHECK(blanks > expected * 0.97 && blanks < expected * 1.03);
  }
}

static void APauseGivesNoneAndNoBurstAfter(void)
{
  PlaybackClock clock;
  uint64_t now = 0;
  BlanksIn(clock, now, 500, 8, false);
  CHECK(BlanksIn(clock, now, 5000, 8, true) == 0);
  // Right after the pause no more than the one step's worth
  int first = BlanksIn(clock, now, 10, 8, false);
  CHECK(first <= 5);
}

static void AStallIsNotMadeUp(void)
{
  PlaybackClock clock;
  uint64_t now = 0;
  BlanksIn(clock, now, 100, 2, false);
  now += 3000; // the window was dragged, a message box stood
  int blanks = BlanksIn(clock, now, 10, 2, false);
  CHECK(blanks <= 2);
}

static void ASlowMachineLetsTheBacklogGo(void)
{
  // Asked once per 10 ms at 64x: 37 blanks are due each step, only one is taken
  PlaybackClock clock;
  uint64_t now = 0;
  BlanksIn(clock, now, 1000, 64, false, 1);
  // Then asked often: the backlog is at most BACKLOG + 2 * 64
  int burst = 0;
  for (int i = 0; i < 1000; i++)
    if (clock.Due(now, 64, false, 17.0)) burst++;
  CHECK(burst <= PlaybackClock::BACKLOG + 2 * 64);
}

// Stepping onto a breakpoint pauses the replay and removes it; standing on, or setting one under
// the party, does not; a reset forgets them all
static void ABreakpointPausesOnceWhenSteppedOnto(void)
{
  PlaybackControl_Reset();
  PlaybackControl_ToggleBreakpoint(1, 5, 6);
  PlaybackControl_ToggleBreakpoint(2, 3, 3);
  CHECK(PlaybackControl_HasBreakpoint(1, 5, 6) && PlaybackControl_BreakpointCount() == 2);
  CHECK(!PlaybackControl_PartyAt(1, 5, 5) && !PlaybackControl_IsPaused());
  CHECK(!PlaybackControl_PartyAt(1, 5, 5));
  // The pause waits for the loop, between ticks
  CHECK(PlaybackControl_PartyAt(1, 5, 6) && !PlaybackControl_IsPaused());
  PlaybackControl_ApplyDuePause();
  CHECK(PlaybackControl_IsPaused());
  PlaybackControl_SetPaused(false);
  PlaybackControl_ApplyDuePause();   // only once
  CHECK(!PlaybackControl_IsPaused());
  CHECK(!PlaybackControl_HasBreakpoint(1, 5, 6) && PlaybackControl_HasBreakpoint(2, 3, 3));
  PlaybackControl_SetPaused(false);
  // The same cell on another level is another cell
  CHECK(!PlaybackControl_PartyAt(2, 5, 6) && !PlaybackControl_IsPaused());
  // Set under the party: it waits until the party leaves and comes back
  PlaybackControl_ToggleBreakpoint(2, 5, 6);
  CHECK(!PlaybackControl_PartyAt(2, 5, 6) && !PlaybackControl_IsPaused());
  CHECK(!PlaybackControl_PartyAt(2, 5, 7));
  CHECK(PlaybackControl_PartyAt(2, 5, 6));
  PlaybackControl_ApplyDuePause();
  CHECK(PlaybackControl_IsPaused());
  // Toggling twice removes it
  PlaybackControl_ToggleBreakpoint(2, 3, 3);
  CHECK(PlaybackControl_BreakpointCount() == 0);
  PlaybackControl_ToggleBreakpoint(1, 1, 1);
  CHECK(PlaybackControl_PartyAt(1, 1, 1));
  PlaybackControl_Reset();
  PlaybackControl_ApplyDuePause();   // the reset dropped the due pause
  CHECK(PlaybackControl_BreakpointCount() == 0 && !PlaybackControl_IsPaused());
}

void TestPlaybackControl(void)
{
  ABreakpointPausesOnceWhenSteppedOnto();
  TheSpeedsGoFromHalfTo64AndMax();
  SoundsThinOutFrom4x();
  EnoughChecksForEverySpeed();
  TheClockRunsAtTheFactor();
  APauseGivesNoneAndNoBurstAfter();
  AStallIsNotMadeUp();
  ASlowMachineLetsTheBacklogGo();
}
