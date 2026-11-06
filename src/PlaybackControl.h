#ifndef PLAYBACKCONTROL_H
#define PLAYBACKCONTROL_H

#include <cstdint>

// The pace of a replay (a playfile.log or RecordNNNNN.log played back): how fast it runs against
// the game's own speed setting, and whether it stands still. A replay takes its input from the
// file by game tick, so how many vertical blanks come in a second changes only how fast it goes,
// never what happens. No engine, no SDL.

// 0.5x, 1x, 2x, 4x, 8x, 16x, 32x, 64x and max. Max is the engine's quick play (NoSpeedLimit):
// no clock at all, as many ticks as the machine manages (PLAYBACK_MAX_MS_PER_TIMER)
const int PLAYBACK_SPEED_COUNT = 9;
const int PLAYBACK_SPEED_NORMAL = 1; // the index of 1x
const int PLAYBACK_SPEED_MAX = 8;    // the index of max

// At max a timer message runs ticks for this long, then the window gets its turn to draw and
// to take keys (pause, slower), so the replay stays answerable however fast it runs
const int PLAYBACK_MAX_MS_PER_TIMER = 30;
// and draws the game's window no more often than this
const int PLAYBACK_MAX_FRAMES_PER_SECOND = 30;

int PlaybackControl_SpeedIndex(void);
void PlaybackControl_SetSpeedIndex(int index); // clamped to the list
void PlaybackControl_Faster(void);
void PlaybackControl_Slower(void);
bool PlaybackControl_IsMax(void);
// 0.5 .. 64 for the chosen speed; max has no factor, it counts as 64 (no sound)
double PlaybackControl_Factor(void);
double PlaybackControl_FactorOf(int index);
const char *PlaybackControl_LabelOf(int index); // "0.5x", "1x", ... "64x", "max"

bool PlaybackControl_IsPaused(void);
void PlaybackControl_SetPaused(bool paused);
void PlaybackControl_TogglePause(void);

// A new replay starts at 1x and running, without breakpoints; a stopped one loses them too
void PlaybackControl_Reset(void);

// Breakpoints: cells (level and the level's x, y, as the party's position) set on the automap.
// When the party steps onto one, that breakpoint goes (the others stay) and a pause is due.
// Stepping onto means coming from another cell: a breakpoint set under the party waits until it
// leaves and comes back.
void PlaybackControl_ToggleBreakpoint(int level, int x, int y);
bool PlaybackControl_HasBreakpoint(int level, int x, int y);
int PlaybackControl_BreakpointCount(void);
// Every tick of a replay with the party's position: true when it stepped onto a breakpoint.
// The replay is not paused yet: in the middle of a tick the engine may still wait for a vertical
// blank (WaitForButtonFlash), which a paused replay never gives, so it would hang.
bool PlaybackControl_PartyAt(int level, int x, int y);
// The platform layer's loop calls this before each of its own checks for a vertical blank,
// between ticks, where a pause from the menu or Space takes effect too
void PlaybackControl_ApplyDuePause(void);

// From 4x on the sounds would only pile up: they are dropped, as at quick play
bool PlaybackControl_MutesSound(void);

// How many times the platform layer checks for a due vertical blank per timer message:
// base at 1x and below, base times the factor above, so 64x gets enough chances. Max takes as
// many as fit into PLAYBACK_MAX_MS_PER_TIMER, which the platform layer measures itself
int PlaybackControl_ChecksPerTimer(int base);

// The vertical blanks of a replay at a speed other than 1x, or paused. The platform layer asks
// Due for every check with its millisecond clock; each true is one blank. Time runs at factor
// times real time; a paused clock gathers none, and a stall of a second or more is not made up.
// What it cannot catch up (a slow machine at 64x) it lets go, so it never comes in a burst.
class PlaybackClock
{
public:
  PlaybackClock(void) : m_last(0), m_started(false), m_budget(0.0) {}
  bool Due(uint64_t nowMs, double factor, bool paused, double msPerBlank);
  // The next Due measures from its own call: after a pause, or after 1x ran on the game's clock
  void Restart(void) { m_started = false; m_budget = 0.0; }

  static const uint64_t STALL_MS = 1000;
  // At most this many blanks plus two per unit of factor wait their turn
  static const int BACKLOG = 10;

private:
  uint64_t m_last;
  bool m_started;
  double m_budget;
};

#endif
