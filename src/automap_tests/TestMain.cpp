// TestMain.cpp : runs the tests of the units that need neither the engine nor SDL.
#include "TestSupport.h"

int failedCheckCount = 0;

int main(void)
{
  TestAutoMapExplored();
  TestAutoMapKnowledge();
  TestAutoMapSight();
  TestAutoMapFile();
  TestAutoMapScene();
  TestAutoMapSpoilers();
  TestAutoMapLinkWatch();
  TestAutoMapDrawTally();
  TestAutoMapItemMemory();
  TestAutoMapCellMemory();
  TestAutoMapPlayerNotes();
  TestAutoMapMonsterDamage();
  TestAutoMapVision();
  TestAutoMapVisibilityCases();
  TestShotMissiles();
  TestHeroStats();
  TestHeroStatsOrigins();
  TestHeroStatsPending();
  if (failedCheckCount != 0)
  {
    std::printf("%d checks failed\n", failedCheckCount);
    return 1;
  }
  std::printf("All tests passed\n");
  return 0;
}
