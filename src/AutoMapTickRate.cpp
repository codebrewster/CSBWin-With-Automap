// AutoMapTickRate.cpp : see AutoMapTickRate.h
#include "AutoMapTickRate.h"

void AutoMapTickRate::OnTick(uint32_t nowMs)
{
  if (!m_ticks.empty() && nowMs - m_ticks.back() > GAP_MS) m_ticks.clear();
  m_ticks.push_back(nowMs);
  while (m_ticks.size() > 1 && nowMs - m_ticks.front() > WINDOW_MS) m_ticks.pop_front();
}

void AutoMapTickRate::Interrupt(void)
{
  m_ticks.clear();
}

double AutoMapTickRate::TicksPerSecond(double fallback) const
{
  if (m_ticks.size() < 2) return fallback;
  uint32_t span = m_ticks.back() - m_ticks.front();
  if (span < MEASURED_MS) return fallback;
  // n ticks mark n - 1 intervals
  return (m_ticks.size() - 1) * 1000.0 / span;
}

static const double MS_PER_VBLANK = 17.2;
static const double VBLANKS_SAVED_PER_TICK = 1.2;

double AutoMapTickRate_OfSpeed(int vblanksPerTick)
{
  double blanks = vblanksPerTick - VBLANKS_SAVED_PER_TICK;
  // FTL (5) and faster stay above a blank a tick
  if (blanks < 1.0) blanks = 1.0;
  return 1000.0 / (blanks * MS_PER_VBLANK);
}
