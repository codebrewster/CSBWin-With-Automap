#include "TestSupport.h"
#include "../ContinueLog.h"

#include <string>

static const char *HEADER =
  "#CSB Version Nov 11 2026\n"
  "#NoFountainDrink\n"
  "00000010 00e9 0071 0003 11111111\n";

// The game was saved and quit from the inventory's disk menu: Save and Quit is the second
// option (00d3), the file the first (00d2)
static void SaveAndQuitAtTheEndBecomesA2223Line(void)
{
  std::string log = std::string(HEADER) +
    "00000100 00a1 0079 0053 22222222\n"
    "00000101 0000 0000 0add 33333333\n"
    "00000105 00b1 0029 008c 44444444\n"
    "00000105 0000 0000 0add 44444444\n"
    "00000105 008c 0070 00d3 44444444\n"
    "00000105 0000 0000 0add 44444444\n"
    "00000105 0091 006b 00d2 44444444\n"
    "00000105 0000 0000 0add 44444444\n"
    "00000105 001f 0d4a 6667 44444444\n"
    "00000105 f2e5 643b 6668 55555555\n";
  bool changed = false;
  std::string prepared = ContinueLog_Prepare(log, &changed);
  CHECK(changed);
  std::string expected = std::string(HEADER) +
    "00000100 00a1 0079 0053 22222222\n"
    "00000101 0000 0000 0add 33333333\n"
    "#00000105 00b1 0029 008c 44444444\n"
    "#00000105 0000 0000 0add 44444444\n"
    "#00000105 008c 0070 00d3 44444444\n"
    "#00000105 0000 0000 0add 44444444\n"
    "#00000105 0091 006b 00d2 44444444\n"
    "#00000105 0000 0000 0add 44444444\n"
    "00000105 001f 0d4a 6667 44444444\n"
    "00000105 0000 0000 2223 55555555\n"
    "00000105 f2e5 643b 6668 55555555\n";
  CHECK(prepared == expected);
  // Prepared once, it is left as it is
  std::string again = ContinueLog_Prepare(prepared, &changed);
  CHECK(!changed);
  CHECK(again == prepared);
}

// Save and Play from the disk menu: the replay saves as the game did and plays on
static void SaveAndPlayIsLeftAlone(void)
{
  std::string log = std::string(HEADER) +
    "00000105 00b1 0029 008c 44444444\n"
    "00000105 008c 0070 00d2 44444444\n"
    "00000105 0091 006b 00d2 44444444\n"
    "00000105 f2e5 643b 6668 55555555\n";
  bool changed = true;
  CHECK(ContinueLog_Prepare(log, &changed) == log);
  CHECK(!changed);
}

// File > Save game writes 2223 itself
static void MenuSaveIsLeftAlone(void)
{
  std::string log = std::string(HEADER) +
    "00000105 f2e5 643b 6668 55555555\n"
    "00000105 0000 0000 2223 55555555\n";
  bool changed = true;
  CHECK(ContinueLog_Prepare(log, &changed) == log);
  CHECK(!changed);
}

// The game went on after a Save and Quit (a new game, say): nothing to undo at the end
static void PlayAfterTheDiskMenuIsLeftAlone(void)
{
  std::string log = std::string(HEADER) +
    "00000105 00b1 0029 008c 44444444\n"
    "00000105 008c 0070 00d3 44444444\n"
    "00000105 f2e5 643b 6668 55555555\n"
    "00000140 00e9 0071 0002 66666666\n";
  bool changed = true;
  CHECK(ContinueLog_Prepare(log, &changed) == log);
  CHECK(!changed);
}

// Cancel in the disk menu, or no save written: no 6668, nothing to replace
static void DiskMenuWithoutSaveIsLeftAlone(void)
{
  std::string log = std::string(HEADER) +
    "00000105 00b1 0029 008c 44444444\n"
    "00000105 008c 0070 00d3 44444444\n";
  bool changed = true;
  CHECK(ContinueLog_Prepare(log, &changed) == log);
  CHECK(!changed);
}

static void LogWithoutDiskMenuIsLeftAlone(void)
{
  bool changed = true;
  CHECK(ContinueLog_Prepare(HEADER, &changed) == HEADER);
  CHECK(!changed);
}

// Windows line ends are kept as they are
static void CarriageReturnsAreKept(void)
{
  std::string log =
    "#CSB Version Nov 11 2026\r\n"
    "00000105 00b1 0029 008c 44444444\r\n"
    "00000105 008c 0070 00d3 44444444\r\n"
    "00000105 f2e5 643b 6668 55555555\r\n";
  bool changed = false;
  std::string prepared = ContinueLog_Prepare(log, &changed);
  CHECK(changed);
  CHECK(prepared ==
    "#CSB Version Nov 11 2026\r\n"
    "#00000105 00b1 0029 008c 44444444\r\n"
    "#00000105 008c 0070 00d3 44444444\r\n"
    "00000105 0000 0000 2223 55555555\r\n"
    "00000105 f2e5 643b 6668 55555555\r\n");
}

void TestContinueLog(void)
{
  SaveAndQuitAtTheEndBecomesA2223Line();
  SaveAndPlayIsLeftAlone();
  MenuSaveIsLeftAlone();
  PlayAfterTheDiskMenuIsLeftAlone();
  DiskMenuWithoutSaveIsLeftAlone();
  LogWithoutDiskMenuIsLeftAlone();
  CarriageReturnsAreKept();
}
