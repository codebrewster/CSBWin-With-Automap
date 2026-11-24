#include "TestSupport.h"
#include <cmath>
#include "../HeroStatsProfile.h"
#include "../HeroStatsView.h"

static const HeroStatsChampion ZED = 1, ELIJA = 2, SYRA = 3;

static bool Near(double a, double b)
{
  return std::fabs(a - b) < 1e-9;
}

static HeroStatsBlow BlowOf(HeroStatsChampion who)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = who;
  blow.kind = HeroDamageMelee;
  blow.weapon = 13;
  blow.option = 1;
  return blow;
}

static HeroStats OneChampion(void)
{
  HeroStats stats;
  stats.MeetChampion(ZED, "ZED");
  stats.MarkLevelVisited(1);
  return stats;
}

static void PlayedRatesAreRatiosOfCounts(void)
{
  HeroStats stats = OneChampion();
  for (int i = 0; i < 10; i++) stats.CountAttackTry(1, BlowOf(ZED));
  for (int i = 0; i < 6; i++) stats.CountAttackHit(1, BlowOf(ZED));
  stats.CreditManaGained(ZED, 1, HeroManaRegeneration, 300);
  stats.CreditManaGained(ZED, 1, HeroManaRegenLost, 100);
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(profile.played.axes.size() == 8);
  CHECK(profile.played.axes[0] == "Hit rate");
  CHECK(profile.played.shapes.size() == 1);
  const std::vector<double> &values = profile.played.shapes[0].values;
  CHECK(values.size() == 8);
  CHECK(Near(values[0], 0.6));
  CHECK(Near(values[2], -1));   // no casts: Casts worked is undefined
  CHECK(Near(values[6], 0.75)); // Mana used
}

static void ManaPurposesAddUp(void)
{
  HeroStats stats = OneChampion();
  stats.CountCast(ZED, 1, HeroStatsView_SpellKey("Ful Ir"), 30);
  stats.CountCast(ZED, 1, HeroStatsView_SpellKey("Vi"), 20);
  stats.CountCast(ZED, 1, HeroStatsView_SpellKey("Gor Gor"), 5);  // no spell the table knows
  stats.EnterRune(ZED, 1, 10);
  stats.ResolveRunes(ZED, HeroRuneFailed);
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(profile.mana.axes.size() == 6);
  CHECK(profile.mana.axes[0] == "Attack" && profile.mana.axes[5] == "Wasted");
  CHECK(profile.mana.shapes.size() == 1);
  const std::vector<double> &values = profile.mana.shapes[0].values;
  CHECK(Near(values[0], 30.0 / 65));  // Attack
  CHECK(Near(values[1], 20.0 / 65));  // Potions
  CHECK(Near(values[4], 5.0 / 65));   // Other
  CHECK(Near(values[5], 10.0 / 65));  // Wasted
  double sum = 0;
  for (size_t i = 0; i < values.size(); i++) sum += values[i];
  CHECK(Near(sum, 1));
}

static void RolesNeedTwoChampionsWithAnyValue(void)
{
  HeroStats solo = OneChampion();
  solo.CreditMonsterDamage(1, BlowOf(ZED), 40);
  CHECK(!HeroStatsProfile_Make(solo, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).showRoles);

  HeroStats three = OneChampion();
  three.MeetChampion(ELIJA, "ELIJA");
  three.MeetChampion(SYRA, "SYRA");  // does nothing at all
  three.CreditMonsterDamage(1, BlowOf(ZED), 30);
  three.CreditMonsterDamage(1, BlowOf(ELIJA), 10);
  HeroStatsProfile profile = HeroStatsProfile_Make(three, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(profile.showRoles);
  CHECK(profile.roles.shapes.size() == 2);
  CHECK(Near(profile.roles.evenRing, 0.5));
  CHECK(profile.roles.shapes[0].who == ZED && Near(profile.roles.shapes[0].values[0], 0.75));
}

static void TrainedOuterRingIsTheBestRank(void)
{
  HeroStats stats = OneChampion();
  HeroRankUp fighter = {ZED, 1, 0, 13, 100, HeroXpSource_Other()};
  HeroRankUp wizard = {ZED, 1, 3, 9, 200, HeroXpSource_Other()};
  stats.Experience().CountRankUp(fighter);
  stats.Experience().CountRankUp(wizard);
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(Near(profile.trained.outer, 13));
  CHECK(profile.trained.rings.size() == 3);
  CHECK(Near(profile.trained.rings[0].first, 5) && profile.trained.rings[0].second == HeroXp_RankName(5));
  CHECK(Near(profile.trained.rings[1].first, 9) && profile.trained.rings[1].second == HeroXp_RankName(9));
  CHECK(Near(profile.trained.rings[2].first, 11));
  CHECK(profile.trained.edgeLabel == HeroXp_RankName(13));
  CHECK(profile.trained.shapes.size() == 1);
  CHECK(Near(profile.trained.shapes[0].values[0], 13));
  CHECK(Near(profile.trained.shapes[0].values[1], 1));  // Ninja: no rank known
  CHECK(Near(profile.trained.shapes[0].values[3], 9));
}

static void APickedChampionIsBrightTheOthersFaint(void)
{
  HeroStats stats = OneChampion();
  stats.MeetChampion(ELIJA, "ELIJA");
  stats.CreditMonsterDamage(1, BlowOf(ZED), 30);
  stats.CreditMonsterDamage(1, BlowOf(ELIJA), 10);
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, ELIJA, HERO_STATS_WHOLE_RUN);
  CHECK(profile.roles.shapes.size() == 2);
  for (size_t i = 0; i < profile.roles.shapes.size(); i++)
    CHECK(profile.roles.shapes[i].faint == (profile.roles.shapes[i].who != ELIJA));
  CHECK(!profile.roles.shapes.back().faint);  // the bright one is drawn last
  CHECK(profile.played.shapes.size() == 2);
  CHECK(profile.played.shapes[0].faint && profile.played.shapes[0].who == HERO_STATS_PARTY);
  CHECK(!profile.played.shapes[1].faint && profile.played.shapes[1].who == ELIJA);
}

static void LevelBarsListTheVisitedLevels(void)
{
  HeroStats stats = OneChampion();
  stats.MarkLevelVisited(0);
  stats.MarkLevelVisited(5);
  std::map<int, HeroStatsDungeonCounts> perLevel;
  HeroStatsDungeonCounts half = {100, 50, 0, 0, 0, 0, 0, 0};
  perLevel[5] = half;
  stats.SetDungeonCounts(perLevel);
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(profile.levelBars.size() == 3);
  CHECK(profile.levelBars[0].level == 0 && profile.levelBars[1].level == 1 && profile.levelBars[2].level == 5);
  CHECK(Near(profile.levelBars[0].explored, -1));  // nothing reachable known
  CHECK(Near(profile.levelBars[2].explored, 0.5));
  CHECK(profile.perLevel.size() == 4);
  CHECK(profile.perLevel.back().level == HERO_STATS_WHOLE_RUN);
  CHECK(Near(profile.perLevel.back().values[HeroCompletionExplored], 0.5));
  CHECK(profile.completion.axes.size() == HERO_COMPLETION_AXIS_COUNT);
}

void TestHeroStatsProfile(void)
{
  PlayedRatesAreRatiosOfCounts();
  ManaPurposesAddUp();
  RolesNeedTwoChampionsWithAnyValue();
  TrainedOuterRingIsTheBestRank();
  APickedChampionIsBrightTheOthersFaint();
  LevelBarsListTheVisitedLevels();
}
