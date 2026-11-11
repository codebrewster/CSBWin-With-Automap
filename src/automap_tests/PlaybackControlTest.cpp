#include "TestSupport.h"
#include "../PlaybackControl.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

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

// A fast forward runs at max, then goes back to the speed and the pause it came from
static void ASkipRunsAtMaxAndGoesBack(void)
{
  PlaybackControl_Reset();
  PlaybackControl_SetSpeedIndex(2);
  PlaybackControl_SkipAhead(100, 250, 2);
  CHECK(PlaybackControl_IsMax() && PlaybackControl_IsSkipping() && PlaybackControl_SkipTarget() == 350);
  CHECK(PlaybackControl_TakeSpeedSync() && !PlaybackControl_TakeSpeedSync());
  PlaybackControl_SkipAhead(200, 2500, 2);           // adds on
  CHECK(PlaybackControl_SkipTarget() == 2850);
  PlaybackControl_AtTick(2849);
  CHECK(PlaybackControl_IsMax());
  PlaybackControl_AtTick(2850);
  CHECK(!PlaybackControl_IsSkipping() && PlaybackControl_SpeedIndex() == 2 && PlaybackControl_TakeSpeedSync());
  PlaybackControl_ApplyDuePause();
  CHECK(!PlaybackControl_IsPaused());
  // From a pause: back to it
  PlaybackControl_SetPaused(true);
  PlaybackControl_SkipAhead(10, 5, 3);
  CHECK(!PlaybackControl_IsPaused());
  PlaybackControl_AtTick(15);
  PlaybackControl_ApplyDuePause();
  CHECK(PlaybackControl_IsPaused() && PlaybackControl_SpeedIndex() == 3);
  // A speed chosen by hand ends it
  PlaybackControl_SetPaused(false);
  PlaybackControl_SkipAhead(0, 100, 1);
  PlaybackControl_SetSpeedIndex(4);
  CHECK(!PlaybackControl_IsSkipping());
}

// Going back: the engine puts a snapshot back, runs at max to the tick, then as before
static void ARewindRunsFromTheSnapshotToTheTick(void)
{
  PlaybackControl_Reset();
  PlaybackControl_SetSpeedIndex(2);
  CHECK(PlaybackControl_RewindBy(1000, 250, 2));
  CHECK(PlaybackControl_RewindPending() && PlaybackControl_RewindTarget() == 750);
  CHECK(PlaybackControl_RewindBy(1000, 250, 2));          // again before it happened: further
  CHECK(PlaybackControl_RewindTarget() == 500);
  PlaybackControl_RewindStart(250);
  CHECK(!PlaybackControl_RewindPending());
  CHECK(PlaybackControl_IsMax() && PlaybackControl_SkipIsRewind() && PlaybackControl_SkipTarget() == 500);
  CHECK(PlaybackControl_TakeSpeedSync());
  PlaybackControl_AtTick(500);
  PlaybackControl_ApplyDuePause();
  CHECK(!PlaybackControl_IsSkipping() && PlaybackControl_SpeedIndex() == 2 && !PlaybackControl_IsPaused());
  // Paused, to a snapshot's own tick: no run, still paused
  PlaybackControl_SetPaused(true);
  CHECK(PlaybackControl_RewindBy(1000, 250, 2));
  PlaybackControl_RewindStart(750);
  CHECK(!PlaybackControl_IsSkipping() && PlaybackControl_IsPaused() && PlaybackControl_SpeedIndex() == 2);
  // Paused, between snapshots: the run ends in the pause
  CHECK(PlaybackControl_RewindBy(1000, 100, 2));
  PlaybackControl_RewindStart(750);
  CHECK(!PlaybackControl_IsPaused() && PlaybackControl_SkipTarget() == 900);
  PlaybackControl_AtTick(900);
  PlaybackControl_ApplyDuePause();
  CHECK(PlaybackControl_IsPaused());
  // Not before the start
  PlaybackControl_SetPaused(false);
  CHECK(PlaybackControl_RewindBy(100, 250, 1) && PlaybackControl_RewindTarget() == 0);
  PlaybackControl_CancelRewind();
  CHECK(!PlaybackControl_RewindPending());
  // A new replay forgets a rewind that waits
  CHECK(PlaybackControl_RewindBy(1000, 250, 1));
  PlaybackControl_Reset();
  CHECK(!PlaybackControl_RewindPending());
}

// Back while a fast forward runs: from where it heads
static void ARewindDuringASkipCountsFromItsTarget(void)
{
  PlaybackControl_Reset();
  PlaybackControl_SkipAhead(1000, 2500, 3);                // heads for 3500
  CHECK(!PlaybackControl_RewindBy(1200, 250, PLAYBACK_SPEED_MAX));
  CHECK(PlaybackControl_SkipTarget() == 3250 && !PlaybackControl_RewindPending());
  CHECK(!PlaybackControl_RewindBy(1200, 2050, PLAYBACK_SPEED_MAX)); // where it stands: it ends
  CHECK(!PlaybackControl_IsSkipping() && PlaybackControl_SpeedIndex() == 3);
  PlaybackControl_SkipAhead(1200, 250, 3);                 // heads for 1450
  CHECK(PlaybackControl_RewindBy(1300, 250, PLAYBACK_SPEED_MAX)); // 1200: behind
  PlaybackControl_RewindStart(1000);
  PlaybackControl_AtTick(1200);
  CHECK(PlaybackControl_SpeedIndex() == 3);                // the skip's own speed, not max
}

// On the way back no breakpoint pauses; after it they do again
static void ARewindPassesBreakpoints(void)
{
  PlaybackControl_Reset();
  PlaybackControl_ToggleBreakpoint(1, 2, 3);
  CHECK(PlaybackControl_RewindBy(1000, 250, 1));
  PlaybackControl_RewindStart(500);
  CHECK(!PlaybackControl_PartyAt(1, 2, 3));
  CHECK(PlaybackControl_HasBreakpoint(1, 2, 3));
  PlaybackControl_AtTick(750);
  PlaybackControl_PartyAt(1, 2, 4);
  CHECK(PlaybackControl_PartyAt(1, 2, 3));
}

static void APauseTargetPausesWhenReached(void)
{
  PlaybackControl_Reset();
  CHECK(!PlaybackControl_SetPauseTarget(500, 400));  // not ahead
  CHECK(PlaybackControl_SetPauseTarget(500, 20000) && PlaybackControl_PauseTarget() == 20000);
  PlaybackControl_AtTick(19999);
  PlaybackControl_ApplyDuePause();
  CHECK(!PlaybackControl_IsPaused());
  PlaybackControl_AtTick(20000);
  PlaybackControl_ApplyDuePause();
  CHECK(PlaybackControl_IsPaused() && PlaybackControl_PauseTarget() == -1);
  CHECK(PlaybackControl_SetPauseTarget(0, 50) && PlaybackControl_SetPauseTarget(0, 0));
  CHECK(PlaybackControl_PauseTarget() == -1);
}

static void TheLengthIsTheTimeOfTheLastRecord(void)
{
  char path[] = "/tmp/playbackcontroltest-XXXXXX";
  int fd = mkstemp(path);
  CHECK(fd >= 0);
  FILE *f = fdopen(fd, "w");
  fputs("#CSB for Windows Version 11.022\n#NoSpeedLimit\n"
        "00000010 0001 0002 0003 00000000\n"   // absolute: 16
        "+0a 0 0 5 0\n"                        // +10 = 26
        "+ff 7 1\n"                            // +255, three fields = 281
        "00000005 0000 1234\n"                // delta 5, xyf 0, random: 286
        "00000000 1234\n"                          // txyf 0 (delta 5), random: 291
        "00000000\n", f);                             // txyfr 0 -> txyf 0 (delta 5): 296
  fclose(f);
  CHECK(PlaybackControl_ScanTotalTicks(path) == 296);
  remove(path);
  CHECK(PlaybackControl_ScanTotalTicks("/nonexistent/x") == -1);
  char buf[32];
  PlaybackControl_FormatTime(3725.4, buf, sizeof buf);
  CHECK(std::strcmp(buf, "1:02:05") == 0);
}

void TestPlaybackControl(void)
{
  ASkipRunsAtMaxAndGoesBack();
  ARewindRunsFromTheSnapshotToTheTick();
  ARewindDuringASkipCountsFromItsTarget();
  ARewindPassesBreakpoints();
  APauseTargetPausesWhenReached();
  TheLengthIsTheTimeOfTheLastRecord();
  ABreakpointPausesOnceWhenSteppedOnto();
  TheSpeedsGoFromHalfTo64AndMax();
  SoundsThinOutFrom4x();
  EnoughChecksForEverySpeed();
  TheClockRunsAtTheFactor();
  APauseGivesNoneAndNoBurstAfter();
  AStallIsNotMadeUp();
  ASlowMachineLetsTheBacklogGo();
}
