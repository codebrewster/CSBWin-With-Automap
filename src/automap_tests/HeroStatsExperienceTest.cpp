#include "TestSupport.h"
#include "../HeroStats.h"
#include "../HeroStatsExperience.h"

static HeroXpSource Attack(int weapon, int option)
{
  HeroXpSource source = HeroXpSource_Other();
  source.kind = HeroXpAttack;
  source.weapon = weapon;
  source.option = option;
  return source;
}

static HeroXpSource Monster(int monsterType)
{
  HeroXpSource source = HeroXpSource_Other();
  source.kind = HeroXpMonster;
  source.monsterType = monsterType;
  return source;
}

static HeroRankUp RankUp(HeroStatsChampion who, int cls, int rank, uint32_t time)
{
  HeroRankUp up;
  up.who = who;
  up.level = 1;
  up.cls = cls;
  up.rank = rank;
  up.time = time;
  up.source = HeroXpSource_Other();
  return up;
}

// A hidden skill's gain is its class's too; a gain straight to the class counts for it alone
static void HiddenSkillsAddUpToTheirClass(void)
{
  HeroStatsExperience xp;
  xp.Credit(7, 1, 4, Attack(5, 1), 30);     // Swing
  xp.Credit(7, 1, 7, Monster(10), 10);      // Parry
  xp.Credit(7, 2, 0, HeroXpSource_Other(), 5);  // Fighter itself
  CHECK(xp.Gained(7, HERO_STATS_WHOLE_RUN, 0) == 45);
  CHECK(xp.Gained(7, 1, 0) == 40);
  CHECK(xp.SkillGained(7, HERO_STATS_WHOLE_RUN, 4) == 30);
  CHECK(xp.Gained(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, -1) == 45);
  CHECK(xp.Gained(7, HERO_STATS_WHOLE_RUN, 1) == 0);
  CHECK(HeroXp_ClassOf(19) == 3 && HeroXp_ClassOf(4) == 0 && HeroXp_ClassOf(2) == 2);
  CHECK(std::string(HeroXp_SkillName(7)) == "Parry" && std::string(HeroXp_SkillName(19)) == "Water");
  CHECK(std::string(HeroXp_ClassName(1)) == "Ninja");
}

static void RowsKeepTheirSource(void)
{
  HeroStatsExperience xp;
  xp.Credit(7, 1, 4, Attack(5, 1), 30);
  xp.Credit(7, 2, 4, Attack(5, 1), 12);
  xp.Credit(7, 1, 4, Attack(5, 2), 3);
  std::vector<HeroXpRow> rows = xp.Rows(7, HERO_STATS_WHOLE_RUN);
  CHECK(rows.size() == 2);
  if (rows.size() == 2)
  {
    CHECK(rows[0].skill == 4 && rows[0].source.option == 1 && rows[0].amount == 42);
    CHECK(rows[1].source.option == 2 && rows[1].amount == 3);
  }
  CHECK(xp.Rows(9, HERO_STATS_WHOLE_RUN).empty());
}

static void RankUpsAreNewestFirst(void)
{
  HeroStatsExperience xp;
  xp.CountRankUp(RankUp(7, 3, 5, 100));
  xp.CountRankUp(RankUp(7, 3, 6, 200));
  xp.CountRankUp(RankUp(9, 0, 4, 150));
  std::vector<HeroRankUp> ups = xp.RankUps(7);
  CHECK(ups.size() == 2 && ups[0].time == 200 && ups[1].time == 100);
  CHECK(xp.RankUps(HERO_STATS_PARTY).size() == 3 && xp.RankUps(HERO_STATS_PARTY)[0].who == 9);  // the last one counted
  CHECK(xp.RanksGained(7, HERO_STATS_WHOLE_RUN, 3) == 2);
  CHECK(xp.RanksGained(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, -1) == 3);
  CHECK(xp.RanksGained(7, 2, 3) == 0);
}

static void StartRankIsTheFirst(void)
{
  HeroStatsExperience xp;
  CHECK(xp.StartRank(7, 2) == -1);
  xp.SetStartRank(7, 2, 3);
  xp.SetStartRank(7, 2, 5);
  CHECK(xp.StartRank(7, 2) == 3);
}

static void ExperienceSurvivesTheFile(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MarkLevelVisited(1);
  stats.Experience().Credit(7, 1, 16, Attack(-2, 3), 77);
  stats.Experience().CountRankUp(RankUp(7, 3, 6, 4321));
  stats.Experience().SetStartRank(7, 3, 5);
  std::vector<uint8_t> bytes;
  stats.Serialize(&bytes);
  HeroStats read;
  CHECK(read.Deserialize(bytes.data(), bytes.size()));
  CHECK(read.Experience().Gained(7, 1, 3) == 77);
  CHECK(read.Experience().RankUps(7).size() == 1 && read.Experience().RankUps(7)[0].time == 4321);
  CHECK(read.Experience().StartRank(7, 3) == 5);
  // A short file is refused
  CHECK(!read.Deserialize(bytes.data(), bytes.size() - 1));

  // Layout 22 has no experience: the bytes before it read, nothing after them
  HeroStats old;
  old.MeetChampion(7, "HALK");
  std::vector<uint8_t> oldBytes;
  old.Serialize(&oldBytes);
  oldBytes.resize(oldBytes.size() - HeroStatsExperience().SerializedSize());
  CHECK(read.Deserialize(oldBytes.data(), oldBytes.size(), 22));
  CHECK(read.Experience().Gained(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, -1) == 0);
}

// Experience is no hardship: loading takes the file's
static void TakeFromFileTakesExperience(void)
{
  HeroStats memory, file;
  memory.Experience().Credit(7, 1, 4, Attack(5, 1), 50);
  file.Experience().Credit(7, 1, 4, Attack(5, 1), 20);
  memory.TakeFromFile(file);
  CHECK(memory.Experience().Gained(7, HERO_STATS_WHOLE_RUN, 0) == 20);
  memory.Forget();
  CHECK(memory.Experience().Gained(HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN, -1) == 0);
}

// The game shows a class's mastery m as Pointer16770[m]: 1 shows no rank, 2 is Neophyte
static void RankNamesAreTheGames(void)
{
  CHECK(HeroXp_RankName(1) == "");
  CHECK(HeroXp_RankName(2) == "Neophyte");
  CHECK(HeroXp_RankName(9) == "Expert");
  CHECK(HeroXp_RankName(10) == "Lo Master");
  CHECK(HeroXp_RankName(15) == "Mon Master");
  CHECK(HeroXp_RankName(16) == "Archmaster");
  CHECK(HeroXp_RankName(17) == "Archmaster");  // the game caps the mastery it shows at 16
  CHECK(HeroXp_RankName(0) == "");
}

// As DetermineMastery counts it without the items held: 1 below 500, one more for each doubling
static void MasteryFollowsTheGame(void)
{
  int percent = -1;
  CHECK(HeroXp_Mastery(0, &percent) == 1 && percent == 0);
  CHECK(HeroXp_Mastery(250, &percent) == 1 && percent == 50);
  CHECK(HeroXp_Mastery(500, &percent) == 2 && percent == 0);
  CHECK(HeroXp_Mastery(750, &percent) == 2 && percent == 50);
  CHECK(HeroXp_Mastery(1000, &percent) == 3);
  CHECK(HeroXp_Mastery(500 << 14, &percent) == 16);
  CHECK(HeroXp_Mastery(999, NULL) == 2);
}

void TestHeroStatsExperience(void)
{
  MasteryFollowsTheGame();
  HiddenSkillsAddUpToTheirClass();
  RowsKeepTheirSource();
  RankUpsAreNewestFirst();
  StartRankIsTheFirst();
  ExperienceSurvivesTheFile();
  TakeFromFileTakesExperience();
  RankNamesAreTheGames();
}
