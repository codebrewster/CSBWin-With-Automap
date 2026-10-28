#include "TestSupport.h"
#include "../AutoMapDrawTally.h"

// A 4 x 1 screen the tests paint
static int screen[4];
static int ScreenPixel(int x, int y) { return y == 0 && x >= 0 && x < 4 ? screen[x] : -1; }

static AutoMapDrawKey Item(AutoMapRecordId record)
{
  AutoMapDrawKey key = {AutoMapDrawItem, -1, record, 7, 2, 3};
  return key;
}

static void OnlyPixelsStillDrawnCount(void)
{
  AutoMapDrawTally tally;
  tally.StartFrame();
  std::vector<AutoMapDrawnPixel> pixels;
  for (int x = 0; x < 4; x++) { AutoMapDrawnPixel p = {x, 0, 0, 5}; pixels.push_back(p); }
  tally.AddDraw(Item(1), pixels);
  screen[0] = 5; screen[1] = 5; screen[2] = 9; screen[3] = 5;   // a later draw covered x = 2
  std::vector<AutoMapDrawOnScreen> drawn = tally.FinishFrame(ScreenPixel);
  CHECK(drawn.size() == 1);
  CHECK(drawn[0].kept.size() == 1);
  CHECK(drawn[0].kept[0].behind == 0 && drawn[0].kept[0].drawn == 5 && drawn[0].kept[0].count == 3);
}

static void EqualKeysAddUp(void)
{
  AutoMapDrawTally tally;
  tally.StartFrame();
  AutoMapDrawnPixel left = {0, 0, 0, 5};
  AutoMapDrawnPixel right = {1, 0, 0, 5};
  AutoMapDrawKey group = {AutoMapDrawMonster, -1, 42, -1, -1, -1};
  tally.AddDraw(group, std::vector<AutoMapDrawnPixel>(1, left));
  tally.AddDraw(group, std::vector<AutoMapDrawnPixel>(1, right));
  tally.AddDraw(Item(2), std::vector<AutoMapDrawnPixel>(1, left));
  screen[0] = 5; screen[1] = 5;
  std::vector<AutoMapDrawOnScreen> drawn = tally.FinishFrame(ScreenPixel);
  CHECK(drawn.size() == 2);
  CHECK(drawn[0].key == group && drawn[0].kept[0].count == 2);
}

// A 4 x 1 record of the blit that wrote each pixel last
static int stamps[4];
static int WriteStamp(int x, int y) { return y == 0 && x >= 0 && x < 4 ? stamps[x] : -1; }

static void PixelsDrawnOverInTheSameColourDoNotCount(void)
{
  AutoMapDrawTally tally;
  tally.StartFrame();
  std::vector<AutoMapDrawnPixel> pixels;
  for (int x = 0; x < 4; x++) { AutoMapDrawnPixel p = {x, 0, 0, 5, 1}; pixels.push_back(p); }
  tally.AddDraw(Item(1), pixels);
  screen[0] = 5; screen[1] = 5; screen[2] = 5; screen[3] = 5;
  stamps[0] = 1; stamps[1] = 2; stamps[2] = 1; stamps[3] = 2;   // a wall in colour 5 covered x = 1, 3
  std::vector<AutoMapDrawOnScreen> drawn = tally.FinishFrame(ScreenPixel, WriteStamp);
  CHECK(drawn.size() == 1);
  CHECK(drawn[0].kept.size() == 1 && drawn[0].kept[0].count == 2);
}

// A pile: the gem (6) over the club (5) on the floor (0), a wall (5) over the right end. Without
// the items the floor shows where they lie, the wall where it covers them.
static void TheFrameWithoutItemsShowsTheFloorUnderAPile(void)
{
  AutoMapDrawTally tally;
  tally.StartFrame();
  std::vector<AutoMapDrawnPixel> club;
  for (int x = 0; x < 3; x++) { AutoMapDrawnPixel p = {x, 0, 0, 5, 1}; club.push_back(p); }
  tally.AddDraw(Item(1), club);
  std::vector<AutoMapDrawnPixel> gem;
  for (int x = 1; x < 3; x++) { AutoMapDrawnPixel p = {x, 0, 5, 6, 2}; gem.push_back(p); }
  tally.AddDraw(Item(2), gem);
  screen[0] = 5; screen[1] = 6; screen[2] = 5; screen[3] = 0;
  stamps[0] = 1; stamps[1] = 2; stamps[2] = 3; stamps[3] = 0;
  std::vector<int> itemless = tally.FrameWithoutItems(4, 1, ScreenPixel, WriteStamp);
  CHECK(itemless.size() == 4);
  CHECK(itemless[0] == 0 && itemless[1] == 0 && itemless[2] == 5 && itemless[3] == 0);
}

static void StartFrameForgetsTheLastFrame(void)
{
  AutoMapDrawTally tally;
  tally.StartFrame();
  AutoMapDrawnPixel pixel = {0, 0, 0, 5};
  tally.AddDraw(Item(1), std::vector<AutoMapDrawnPixel>(1, pixel));
  tally.StartFrame();
  CHECK(tally.FinishFrame(ScreenPixel).empty());
}

void TestAutoMapDrawTally(void)
{
  OnlyPixelsStillDrawnCount();
  EqualKeysAddUp();
  StartFrameForgetsTheLastFrame();
  PixelsDrawnOverInTheSameColourDoNotCount();
  TheFrameWithoutItemsShowsTheFloorUnderAPile();
}
