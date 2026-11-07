#include "TestSupport.h"
#include "../HeroStats.h"
#include "../HeroStatsRecords.h"

static HeroRecordEntry Entry(HeroStatsChampion who, int level, uint32_t time, int value)
{
  HeroRecordEntry entry = HeroRecordEntry_Empty();
  entry.who = who;
  entry.level = level;
  entry.time = time;
  entry.value = value;
  return entry;
}

static int Value(const HeroStatsRecords &records, HeroRecordKind kind, HeroStatsChampion who, int level)
{
  HeroRecordEntry entry;
  return records.Best(kind, who, level, &entry) ? entry.value : -1;
}

static void BiggerReplaces(void)
{
  HeroStatsRecords records;
  records.Offer(HeroRecBiggestHit, Entry(7, 1, 10, 90));
  records.Offer(HeroRecBiggestHit, Entry(7, 1, 20, 120));
  records.Offer(HeroRecBiggestHit, Entry(7, 1, 30, 100));
  CHECK(Value(records, HeroRecBiggestHit, 7, 1) == 120);
  CHECK(Value(records, HeroRecBiggestHit, 7, 2) == -1);
}

static void TieKeepsTheFirst(void)
{
  HeroStatsRecords records;
  records.Offer(HeroRecBiggestHit, Entry(7, 1, 80, 120));
  records.Offer(HeroRecBiggestHit, Entry(7, 2, 50, 120));
  records.Offer(HeroRecBiggestHit, Entry(7, 2, 90, 120));
  HeroRecordEntry best;
  CHECK(records.Best(HeroRecBiggestHit, 7, HERO_STATS_WHOLE_RUN, &best) && best.level == 2 && best.time == 50);
}

static void PartyIsTheBestChampion(void)
{
  HeroStatsRecords records;
  records.Offer(HeroRecBiggestHit, Entry(7, 1, 10, 120));
  records.Offer(HeroRecBiggestHit, Entry(9, 1, 20, 150));
  HeroRecordEntry best;
  CHECK(records.Best(HeroRecBiggestHit, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, &best) && best.who == 9 && best.value == 150);
}

static void ClosestCallNeedsSurvival(void)
{
  HeroStatsRecords records;
  HeroRecordEntry best;
  CHECK(!records.Best(HeroRecClosestCall, 7, HERO_STATS_WHOLE_RUN, &best));
  records.Offer(HeroRecClosestCall, Entry(7, 1, 10, 9));
  records.Offer(HeroRecClosestCall, Entry(7, 1, 20, 2));
  records.Offer(HeroRecClosestCall, Entry(7, 1, 30, 0));  // dead: no closest call
  CHECK(Value(records, HeroRecClosestCall, 7, 1) == 2);
  CHECK(HeroRecord_LowerIsBetter(HeroRecClosestCall) && !HeroRecord_LowerIsBetter(HeroRecBiggestHit));
  CHECK(HeroRecord_PartyOnly(HeroRecLongestSleep) && !HeroRecord_PartyOnly(HeroRecKillStreak));
}

static void StreakEndsWithHarm(void)
{
  HeroStatsRecords records;
  records.CountKill(7, 3, 100, 10);
  records.CountKill(7, 3, 110, 10);
  records.CountKill(9, 4, 120, 11);
  records.CountHarm(9);
  HeroRecordEntry party;
  CHECK(records.Best(HeroRecKillStreak, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, &party));
  CHECK(party.who == HERO_STATS_PARTY && party.value == 3 && party.level == 4 && party.firstLevel == 3 && party.firstTime == 100);
  CHECK(party.kinds[0][0] == 10 && party.kinds[0][1] == 2 && party.kinds[1][0] == 11 && party.kinds[1][1] == 1);
  // 7 was not hurt: their own two kills go on, and 9's one is over
  CHECK(Value(records, HeroRecKillStreak, 7, HERO_STATS_WHOLE_RUN) == 2);
  CHECK(Value(records, HeroRecKillStreak, 9, HERO_STATS_WHOLE_RUN) == 1);
  records.CountKill(7, 3, 130, 10);
  records.CountHarm(7);
  records.CountKill(7, 3, 140, 10);
  CHECK(Value(records, HeroRecKillStreak, 7, HERO_STATS_WHOLE_RUN) == 3);
  // The party's new streak started after 9 was hurt: 2 kills, less than its record
  CHECK(Value(records, HeroRecKillStreak, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 3);
}

static void ChainsEndWithAMove(void)
{
  HeroStatsRecords records;
  records.CountPitFall(7, 100);
  records.CountPitFall(8, 101);
  records.CountPitFall(9, 102);
  records.CountMove();
  records.CountPitFall(2, 200);
  HeroRecordEntry fall;
  CHECK(records.Best(HeroRecLongestFall, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, &fall));
  CHECK(fall.value == 3 && fall.firstLevel == 7 && fall.level == 9 && fall.who == HERO_STATS_PARTY);
  records.CountTeleport(1, 10);
  records.CountTeleport(1, 11);
  records.CountMove();
  CHECK(Value(records, HeroRecLongestTeleportChain, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 2);
  for (int t = 0; t < 5; t++) records.CountSleepTick(2, 300 + t, true);
  records.CountSleepTick(2, 305, false);
  records.CountSleepTick(2, 306, true);
  CHECK(Value(records, HeroRecLongestSleep, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN) == 5);
}

static void RunningStreakSurvivesTheFile(void)
{
  HeroStats stats;
  stats.Records().CountKill(7, 1, 10, 4);
  stats.Records().CountKill(7, 1, 20, 4);
  stats.Records().Offer(HeroRecBiggestHit, Entry(7, 1, 20, 77));
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(Value(read.Records(), HeroRecBiggestHit, 7, 1) == 77);
  read.Records().CountKill(7, 1, 30, 4);
  read.Records().CountHarm(7);
  CHECK(Value(read.Records(), HeroRecKillStreak, 7, HERO_STATS_WHOLE_RUN) == 3);
  CHECK(!read.Deserialize(bytes.data(), bytes.size() - 1));
}

// Records are no hardship: a reload takes the file's
static void RecordsGoBackWithTakeFromFile(void)
{
  HeroStats memory, file;
  memory.Records().Offer(HeroRecBiggestHit, Entry(7, 1, 10, 200));
  file.Records().Offer(HeroRecBiggestHit, Entry(7, 1, 10, 120));
  memory.TakeFromFile(file);
  CHECK(Value(memory.Records(), HeroRecBiggestHit, 7, HERO_STATS_WHOLE_RUN) == 120);
  memory.Forget();
  CHECK(Value(memory.Records(), HeroRecBiggestHit, 7, HERO_STATS_WHOLE_RUN) == -1);
}

// A stretch of poison runs from the first dose to the end of the last, with its damage and doses;
// what still runs counts, and a cure or a death ends it
static void PoisonStretchesAreRecords(void)
{
  HeroStats stats;
  HeroStatsRecords &records = stats.Records();
  records.CountPoison(7, 1, 100, 40, true);
  records.CountPoisonDamage(7, 2);
  records.CountPoison(7, 1, 130, 90, true);       // a second dose: 40 + 50
  records.CountPoison(7, 1, 166, 80, false);      // the next tick of a dose
  records.CountPoisonDamage(7, 3);
  HeroRecordEntry entry;
  // Running: 66 ticks long, two doses, 5 damage
  CHECK(records.Best(HeroRecLongestPoisoned, 7, HERO_STATS_WHOLE_RUN, &entry));
  CHECK(entry.value == 66 && entry.extra == 5 && entry.count == 2 && entry.firstTime == 100);
  CHECK(Value(records, HeroRecMostPoisonings, 7, HERO_STATS_WHOLE_RUN) == 2);
  CHECK(Value(records, HeroRecMostPoison, 7, HERO_STATS_WHOLE_RUN) == 90);
  // The file keeps what runs
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(Value(read.Records(), HeroRecLongestPoisoned, 7, HERO_STATS_WHOLE_RUN) == 66);
  // Cured or dead: the stretch is over and a shorter one later does not beat it
  records.EndPoison(7, 1, 200);
  CHECK(Value(records, HeroRecLongestPoisoned, 7, HERO_STATS_WHOLE_RUN) == 100);
  records.CountPoison(7, 2, 300, 10, true);
  records.EndPoison(7, 2, 320);
  CHECK(Value(records, HeroRecLongestPoisoned, 7, HERO_STATS_WHOLE_RUN) == 100);
  CHECK(Value(records, HeroRecMostPoisonings, 7, HERO_STATS_WHOLE_RUN) == 2);
  records.EndPoison(7, 2, 330);   // nothing runs: nothing happens
  CHECK(Value(records, HeroRecLongestPoisoned, 9, HERO_STATS_WHOLE_RUN) == -1);
}

static void WoundRecordsAndCounts(void)
{
  HeroStats stats;
  stats.Records().CountWounds(7, 1, 50, 2, 2);
  stats.Records().CountWounds(7, 1, 60, 3, 1);
  CHECK(Value(stats.Records(), HeroRecMostWoundsAtOnce, 7, HERO_STATS_WHOLE_RUN) == 3);
  CHECK(Value(stats.Records(), HeroRecMostWoundsInOneBlow, 7, HERO_STATS_WHOLE_RUN) == 2);
  stats.CountEvent(7, 1, HeroEventWounded, 3);
  stats.CountEvent(7, 2, HeroEventPoisoned);
  stats.CountEvent(9, 2, HeroEventPoisoned);
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.Events(7, HERO_STATS_WHOLE_RUN, HeroEventWounded) == 3 && read.Events(7, 1, HeroEventPoisoned) == 0);
  CHECK(read.Events(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, HeroEventPoisoned) == 2 && read.Events(9, 2, HeroEventPoisoned) == 1);
}

void TestHeroStatsRecords(void)
{
  BiggerReplaces();
  TieKeepsTheFirst();
  PartyIsTheBestChampion();
  ClosestCallNeedsSurvival();
  StreakEndsWithHarm();
  ChainsEndWithAMove();
  RunningStreakSurvivesTheFile();
  RecordsGoBackWithTakeFromFile();
  PoisonStretchesAreRecords();
  WoundRecordsAndCounts();
}
