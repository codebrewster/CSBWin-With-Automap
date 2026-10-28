// ShotMissiles.cpp : remembers which flying missiles a champion shot with a launcher.
#include "ShotMissiles.h"

#include <set>

static bool shotIsExpected = false;
static std::set<int> shotMissileRecords;

void ExpectShotMissile(void)
{
  shotIsExpected = true;
}

bool TakeExpectedShotMissile(void)
{
  const bool wasExpected = shotIsExpected;
  shotIsExpected = false;
  return wasExpected;
}

void RememberShotMissile(int recordIndex)
{
  shotMissileRecords.insert(recordIndex);
}

bool IsShotMissile(int recordIndex)
{
  return shotMissileRecords.count(recordIndex) != 0;
}

void ForgetShotMissile(int recordIndex)
{
  shotMissileRecords.erase(recordIndex);
}

void ForgetAllShotMissiles(void)
{
  shotIsExpected = false;
  shotMissileRecords.clear();
}
