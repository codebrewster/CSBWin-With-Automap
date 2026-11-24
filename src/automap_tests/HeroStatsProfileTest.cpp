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

// Every axis says what it counts, for the hover
static void EveryAxisIsExplained(void)
{
  HeroStats stats = OneChampion();
  stats.MeetChampion(ELIJA, "ELIJA");
  stats.CreditMonsterDamage(1, BlowOf(ZED), 30);
  stats.CreditMonsterDamage(1, BlowOf(ELIJA), 10);
  for (int level = HERO_STATS_WHOLE_RUN; level <= 1; level += 2)
  {
    HeroStatsProfile profile = HeroStatsProfile_Make(stats, HERO_STATS_PARTY, level);
    const HeroStatsStar *stars[5] = {&profile.completion, &profile.played, &profile.trained, &profile.mana, &profile.roles};
    for (int i = 0; i < 5; i++)
    {
      CHECK(stars[i]->helps.size() == stars[i]->axes.size());
      for (size_t a = 0; a < stars[i]->helps.size(); a++) CHECK(stars[i]->helps[a].size() > 20);
    }
    CHECK(profile.played.helps[6].find("full") != std::string::npos);  // Mana used: lost to full mana
  }
}

// The whole run is the whole dungeon: a level the party has not been to counts too
static void TheWholeRunCountsLevelsNotVisited(void)
{
  HeroStats stats = OneChampion();
  std::map<int, HeroStatsDungeonCounts> perLevel;
  HeroStatsDungeonCounts here = {100, 100, 0, 0, 0, 0, 0, 0}, notYet = {300, 0, 0, 0, 0, 0, 0, 0};
  perLevel[1] = here;
  perLevel[2] = notYet;
  stats.SetDungeonCounts(perLevel);
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(Near(profile.completion.shapes[0].values[HeroCompletionExplored], 0.25));
  CHECK(profile.levelBars.size() == 1);   // the bars are the visited levels
}

// The hover text of an axis: what it counts, then a line per shape in the window's tooltip style
static void AnAxisTipExplainsThenListsTheShapes(void)
{
  HeroStats stats = OneChampion();
  stats.MeetChampion(ELIJA, "ELIJA");
  for (int i = 0; i < 4; i++) stats.CountAttackTry(1, BlowOf(ZED));
  for (int i = 0; i < 3; i++) stats.CountAttackHit(1, BlowOf(ZED));
  stats.CreditMonsterDamage(1, BlowOf(ZED), 30);
  stats.CreditMonsterDamage(1, BlowOf(ELIJA), 10);
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, ZED, HERO_STATS_WHOLE_RUN);
  const HeroStatsStar &played = profile.played;
  CHECK(played.tips.size() == played.axes.size());
  CHECK(played.tips[0].find(played.helps[0]) == 0);
  CHECK(played.tips[0].find("\n\nZED  75% \xc2\xb7 3 of 4") != std::string::npos);  // the bright shape first
  CHECK(played.tips[0].find("\nParty  75% \xc2\xb7 3 of 4") != std::string::npos);
  // Short names on the star, so it keeps its size in a narrow card; the hovertext says it all
  for (size_t a = 0; a < played.axes.size(); a++) CHECK(played.axes[a].size() <= 14);
  CHECK(played.axes[4] == "Defences" && played.axes[6] == "Mana regen" && played.axes[7] == "Food & water");
  const HeroStatsStar &roles = profile.roles;
  for (size_t a = 0; a < roles.axes.size(); a++) CHECK(roles.axes[a].size() <= 14);
  CHECK(roles.axes[0] == "Damage" && roles.axes[3] == "Hits taken" && roles.axes[4] == "Potions" && roles.axes[5] == "Experience");
  CHECK(roles.tips[0].find("\n\nZED  75% \xc2\xb7 30\nELIJA  25% \xc2\xb7 10") != std::string::npos);
  // The completion's own help says nothing of the level; its tip does
  CHECK(profile.completion.helps[1].find("whole dungeon") == std::string::npos);
  CHECK(profile.completion.tips[1].find("whole dungeon") != std::string::npos);
}

// The Monsters line tells the champions' kills apart from all the kills, as the Monsters tab does
static void TheMonstersLineSaysWhatTheChampionsKilled(void)
{
  HeroStats stats = OneChampion();
  stats.CountMonsterKill(1, 10, 0, BlowOf(ZED));
  stats.CountMonsterKill(1, 10, 0, BlowOf(ZED));
  stats.CountMonsterKill(1, 10, 0, HeroStatsBlow_Nobody(HeroBlowByDoor));
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(profile.completion.shapes[0].details[HeroCompletionMonsters] == "3 of 3 (2 by champions)");
  CHECK(profile.completion.shapes[0].details[HeroCompletionItems] == "0 of 0");
}

// Casts worked is of the real spells: a meaningless incantation is a wrong rune, not a failure
static void MeaninglessSpellsDoNotCountAgainstCastsWorked(void)
{
  HeroStats stats = OneChampion();
  uint32_t fireball = HeroStatsView_SpellKey("Ful Ir");
  stats.CountCast(ZED, 1, fireball, 10);
  stats.CountCast(ZED, 1, fireball, 10);
  stats.CountFailedCast(ZED, 1, fireball, 10);
  stats.CountFailedCast(ZED, 1, HeroStatsView_SpellKey("Gor Gor") | HERO_SPELL_NO_SUCH_SPELL, 5);
  HeroStatsProfile profile = HeroStatsProfile_Make(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(Near(profile.played.shapes[0].values[2], 2.0 / 3));
  CHECK(profile.played.shapes[0].details[2] == "2 of 3");
  CHECK(profile.played.helps[2].find("Meaningless") != std::string::npos);
}

void TestHeroStatsProfile(void)
{
  MeaninglessSpellsDoNotCountAgainstCastsWorked();
  TheMonstersLineSaysWhatTheChampionsKilled();
  AnAxisTipExplainsThenListsTheShapes();
  EveryAxisIsExplained();
  TheWholeRunCountsLevelsNotVisited();
  PlayedRatesAreRatiosOfCounts();
  ManaPurposesAddUp();
  RolesNeedTwoChampionsWithAnyValue();
  TrainedOuterRingIsTheBestRank();
  APickedChampionIsBrightTheOthersFaint();
  LevelBarsListTheVisitedLevels();
}
