#include "AutoMapCompletion.h"

bool AutoMapCompletion_LevelIsStale(bool cached, long long readVersion, long long version, bool partyOnLevel,
                                    unsigned msSinceRead)
{
  if (!cached) return true;
  if (version == readVersion) return false;
  return !partyOnLevel || msSinceRead >= 2000;
}
