#include "TestSupport.h"
#include "../AutoMapScene.h"
#include "../AutoMapTickRate.h"

#include <cmath>

static bool Near(double a, double b)
{
  return std::fabs(a - b) < 0.01;
}

static void NormalSpeedIsMeasured(void)
{
  AutoMapTickRate rate;
  CHECK(Near(rate.TicksPerSecond(9.0), 9.0));
  // 421 ticks in 100 seconds, as measured at Normal: one every 237.5 ms
  for (int i = 0; i < 421; i++) rate.OnTick(5000 + (uint32_t)(i * 237.5));
  CHECK(std::fabs(rate.TicksPerSecond(0.0) - 4.21) < 0.05);
}

static void TooShortAMeasureGivesTheFallback(void)
{
  AutoMapTickRate rate;
  for (int i = 0; i < 5; i++) rate.OnTick(1000 + i * 200);
  CHECK(Near(rate.TicksPerSecond(4.2), 4.2));
}

static void APauseStartsAgain(void)
{
  AutoMapTickRate rate;
  for (int i = 0; i < 40; i++) rate.OnTick(i * 100);
  CHECK(Near(rate.TicksPerSecond(0.0), 10.0));
  // A minute in the menu, then slow ticks: the old fast ones are gone
  for (int i = 0; i < 20; i++) rate.OnTick(63900 + i * 250);
  CHECK(Near(rate.TicksPerSecond(0.0), 4.0));
  rate.Interrupt();
  CHECK(Near(rate.TicksPerSecond(3.0), 3.0));
}

static void OnlyTheLastTenSecondsCount(void)
{
  AutoMapTickRate rate;
  for (int i = 0; i < 100; i++) rate.OnTick(i * 100);   // 10 a second for 10 s
  for (int i = 1; i <= 50; i++) rate.OnTick(9900 + i * 200); // then 5 a second for 10 s
  CHECK(Near(rate.TicksPerSecond(0.0), 5.0));
}

static void SpeedsGiveTheMeasuredRates(void)
{
  // Slow, Normal and Fast: 22, 15 and 11 vertical blanks a tick
  CHECK(std::fabs(AutoMapTickRate_OfSpeed(22) * 100 - 279) < 3);
  CHECK(std::fabs(AutoMapTickRate_OfSpeed(15) * 100 - 421) < 4);
  CHECK(std::fabs(AutoMapTickRate_OfSpeed(11) * 100 - 590) < 6);
  CHECK(AutoMapTickRate_OfSpeed(0) > 0);
  CHECK(AutoMapTickRate_OfSpeed(1000) > 0 && AutoMapTickRate_OfSpeed(1000) < 0.1);
}

static void WakeLineUsesTheRate(void)
{
  double before = AutoMapScene_TicksPerSecond();
  AutoMapScene_SetTicksPerSecond(4.21);
  CHECK(AutoMapScene_WakeLine(421) == "WAKES IN 421 TICKS, ABOUT 1:40");
  AutoMapScene_SetTicksPerSecond(0.0);
  CHECK(AutoMapScene_TicksPerSecond() > 0);
  AutoMapScene_SetTicksPerSecond(before);
}

void TestAutoMapTickRate(void)
{
  NormalSpeedIsMeasured();
  TooShortAMeasureGivesTheFallback();
  APauseStartsAgain();
  OnlyTheLastTenSecondsCount();
  SpeedsGiveTheMeasuredRates();
  WakeLineUsesTheRate();
}
