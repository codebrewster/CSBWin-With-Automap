#ifndef AUTOMAPTICKRATE_H
#define AUTOMAPTICKRATE_H

#include <cstdint>
#include <deque>

// How many game ticks pass in a second of real time, measured while the game runs. The speed
// setting (Slow, Normal, Fast, ...) only says how many vertical blanks make one tick (22, 15, 11,
// ...; CSBCode.cpp ticks once vblCount reaches it); how many vertical blanks a second come depends
// on the machine: the platform layer queues one every 10 ms, but only after the game has handled
// the last. With the Player clock on, waiting input brings a tick after 10 blanks already. No
// engine, no SDL.
class AutoMapTickRate
{
public:
  // One tick of the running game at nowMs, a millisecond clock
  void OnTick(uint32_t nowMs);
  // Ticks that do not run at the set speed (the party asleep) or a clock that stands still: the
  // measurement starts again with the next tick
  void Interrupt(void);
  // The measured rate once ticks span at least MEASURED_MS; fallback before that
  double TicksPerSecond(double fallback) const;

  // Measured over the ticks of the last WINDOW_MS; a gap longer than GAP_MS is a pause
  static const uint32_t WINDOW_MS = 10000;
  static const uint32_t MEASURED_MS = 2000;
  static const uint32_t GAP_MS = 1000;

private:
  std::deque<uint32_t> m_ticks;
};

// The rate before one is measured, for TicksPerSecond's fallback: a tick lasts (vblanksPerTick -
// 1.2) times 17.2 ms, fitted to 279, 421 and 590 ticks counted in 100 s at Slow (22), Normal (15)
// and Fast (11) on one machine; within 1 % of each
double AutoMapTickRate_OfSpeed(int vblanksPerTick);

#endif
