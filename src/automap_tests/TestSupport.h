#ifndef AUTOMAP_TESTSUPPORT_H
#define AUTOMAP_TESTSUPPORT_H

#include <cstdio>

extern int failedCheckCount;

#define CHECK(condition) \
  do \
  { \
    if (!(condition)) \
    { \
      failedCheckCount++; \
      std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
  } while (0)

void TestAutoMapExplored(void);
void TestAutoMapKnowledge(void);
void TestAutoMapSight(void);
void TestAutoMapFile(void);
void TestAutoMapScene(void);
void TestAutoMapSpoilers(void);
void TestAutoMapLinkWatch(void);
void TestAutoMapDrawTally(void);
void TestAutoMapItemMemory(void);
void TestAutoMapCellMemory(void);
void TestAutoMapPlayerNotes(void);
void TestAutoMapMonsterDamage(void);
void TestAutoMapVision(void);
void TestAutoMapVisibilityCases(void);
void TestShotMissiles(void);
void TestHeroStats(void);
void TestHeroStatsOrigins(void);

#endif
