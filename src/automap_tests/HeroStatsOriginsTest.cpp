#include "TestSupport.h"
#include "../HeroStatsOrigins.h"

static HeroStatsBlow ChampionBlow(HeroStatsChampion who)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = who;
  blow.kind = HeroDamageRanged;
  return blow;
}

static void ExpectedMissileIsTakenOnce(void)
{
  HeroStatsOrigins origins;
  HeroStatsBlow blow;
  origins.ExpectMissile(ChampionBlow(7));
  origins.TakeExpectedMissile(4);
  CHECK(origins.MissileOrigin(4, &blow) && blow.champion == 7);
  origins.TakeExpectedMissile(5);
  CHECK(!origins.MissileOrigin(5, &blow));
  origins.ForgetMissile(4);
  CHECK(!origins.MissileOrigin(4, &blow));
}

static void ExpectationIsReplacedAndRecordsAreReused(void)
{
  HeroStatsOrigins origins;
  HeroStatsBlow blow;
  origins.ExpectMissile(ChampionBlow(7));
  origins.TakeExpectedMissile(4);
  origins.ExpectMissile(ChampionBlow(9));
  origins.TakeExpectedMissile(4);
  CHECK(origins.MissileOrigin(4, &blow) && blow.champion == 9);
}

static void CloudsAndBrewers(void)
{
  HeroStatsOrigins origins;
  HeroStatsBlow blow;
  CHECK(origins.Brewer(3) == HERO_STATS_NOBODY);
  CHECK(!origins.CloudOrigin(2, &blow));
  origins.SetCloudOrigin(2, ChampionBlow(8));
  origins.SetBrewer(3, 5);
  CHECK(origins.CloudOrigin(2, &blow) && blow.champion == 8);
  CHECK(origins.Brewer(3) == 5);
  origins.ForgetCloud(2);
  origins.ForgetPotion(3);
  CHECK(!origins.CloudOrigin(2, &blow));
  CHECK(origins.Brewer(3) == HERO_STATS_NOBODY);
}

static void ForgetDropsAllTablesAndRestoreReplaces(void)
{
  HeroStatsOrigins origins;
  HeroStatsBlow blow;
  origins.ExpectMissile(ChampionBlow(7));
  origins.TakeExpectedMissile(1);
  origins.SetCloudOrigin(2, ChampionBlow(8));
  origins.SetBrewer(3, 5);
  HeroStatsOrigins copy;
  copy.Restore(origins.Missiles(), origins.Clouds(), origins.Brewers());
  CHECK(copy.MissileOrigin(1, &blow) && copy.CloudOrigin(2, &blow) && copy.Brewer(3) == 5);
  origins.Forget();
  CHECK(origins.Missiles().empty() && origins.Clouds().empty() && origins.Brewers().empty());
  CHECK(!origins.MissileOrigin(1, &blow));
}

static void ForgetDropsExpectation(void)
{
  HeroStatsOrigins origins;
  HeroStatsBlow blow;
  origins.ExpectMissile(ChampionBlow(7));
  origins.Forget();
  origins.TakeExpectedMissile(1);
  CHECK(!origins.MissileOrigin(1, &blow));
}

void TestHeroStatsOrigins(void)
{
  ExpectedMissileIsTakenOnce();
  ExpectationIsReplacedAndRecordsAreReused();
  CloudsAndBrewers();
  ForgetDropsAllTablesAndRestoreReplaces();
  ForgetDropsExpectation();
}
