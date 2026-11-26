#include "TestSupport.h"
#include "../AutoMapCompletion.h"

// A level read before is read again once the party has spent time on it since (its version, the
// ticks spent there, moved), whether the window was open or not; the party's own level at most
// every 2 seconds
static void ALevelIsReadAgainWhenThePartyWasThereSince(void)
{
  CHECK(AutoMapCompletion_LevelIsStale(false, 0, 0, false, 0));        // never read
  CHECK(!AutoMapCompletion_LevelIsStale(true, 500, 500, false, 60000)); // nobody was there since
  CHECK(AutoMapCompletion_LevelIsStale(true, 500, 900, false, 10));     // cleared and left while not looking
  CHECK(!AutoMapCompletion_LevelIsStale(true, 500, 900, true, 1000));   // the party is there: not yet
  CHECK(AutoMapCompletion_LevelIsStale(true, 500, 900, true, 2000));
}

void TestAutoMapCompletion(void)
{
  ALevelIsReadAgainWhenThePartyWasThereSince();
}
