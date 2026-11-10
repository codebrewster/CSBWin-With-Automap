#include "TestSupport.h"
#include "../HeroStatsLayout.h"

#include <cmath>

static const HeroStatsTableNeeds NEEDS = {200, 300, 100, true};

static void AWideTableShowsEverything(void)
{
  HeroStatsTableFit fit = HeroStatsLayout_FitTable(NEEDS, 1000);
  CHECK(fit.bar && fit.derived && !fit.wrap && !fit.scroll);
  // 26 % of 1000 is not exact in a float
  CHECK(fabs(fit.barWidth - 260) < 0.01 && fabs(fit.labelWidth - 340) < 0.01 && fit.tableWidth == 1000);
}

static void TheBarGivesWayToTheLabelFirst(void)
{
  HeroStatsTableFit fit = HeroStatsLayout_FitTable(NEEDS, 720);  // 26 % would leave the label 133
  CHECK(fit.bar && fit.barWidth == 120 && fit.labelWidth == 200);
}

static void ThenTheBarGoesThenTheDerivedColumns(void)
{
  HeroStatsTableFit noBar = HeroStatsLayout_FitTable(NEEDS, 650);
  CHECK(!noBar.bar && noBar.derived && noBar.labelWidth == 250);
  HeroStatsTableFit noDerived = HeroStatsLayout_FitTable(NEEDS, 550);
  CHECK(!noDerived.bar && !noDerived.derived && !noDerived.wrap && noDerived.labelWidth == 250);
}

static void ThenTheLabelWraps(void)
{
  HeroStatsTableFit fit = HeroStatsLayout_FitTable(NEEDS, 450);
  CHECK(fit.wrap && !fit.scroll && fit.labelWidth == 150 && fit.tableWidth == 450);
}

static void ANameTooLongScrollsTheTable(void)
{
  HeroStatsTableFit fit = HeroStatsLayout_FitTable(NEEDS, 400);
  CHECK(fit.wrap && fit.scroll && fit.labelWidth == 120 && fit.tableWidth == 420);
}

static void NoRoomAtAllStillGivesWidths(void)
{
  HeroStatsTableFit fit = HeroStatsLayout_FitTable(NEEDS, 0);
  CHECK(fit.scroll && fit.labelWidth == 120 && fit.tableWidth == 420 && fit.barWidth == 0);
  HeroStatsTableNeeds plain = {80, 60, 0, false};
  CHECK(!HeroStatsLayout_FitTable(plain, 500).bar && HeroStatsLayout_FitTable(plain, 500).labelWidth == 440);
}

static void FullWidthIsEverythingWithTheSmallestBar(void)
{
  CHECK(HeroStatsLayout_FullWidth(NEEDS) == 710);
}

static void CardsNeverStandThreeAndOne(void)
{
  CHECK(HeroStatsLayout_Across(4, 1300, 290, 10) == 4);
  CHECK(HeroStatsLayout_Across(4, 940, 290, 10) == 2);   // three would fit
  CHECK(HeroStatsLayout_Across(4, 500, 290, 10) == 1);
  CHECK(HeroStatsLayout_Across(3, 940, 290, 10) == 3);
  CHECK(HeroStatsLayout_Across(1, 1300, 290, 10) == 1);
  CHECK(HeroStatsLayout_Across(4, 100, 290, 10) == 1);
}

static void SectionsStandSideBySideWhenEachFitsInHalf(void)
{
  std::vector<float> widths;
  widths.push_back(600);
  widths.push_back(704);
  CHECK(HeroStatsLayout_TwoColumns(widths, 1430, 22));
  CHECK(!HeroStatsLayout_TwoColumns(widths, 1400, 22));
  CHECK(!HeroStatsLayout_TwoColumns(std::vector<float>(1, 300), 2000, 22));
}

static void EachSectionGoesToTheShorterColumn(void)
{
  static const float HEIGHTS[4] = {300, 200, 150, 400};
  static const int COLUMNS[4] = {0, 1, 1, 0};
  CHECK(HeroStatsLayout_Flow(std::vector<float>(HEIGHTS, HEIGHTS + 4)) == std::vector<int>(COLUMNS, COLUMNS + 4));
}

void TestHeroStatsLayout(void)
{
  AWideTableShowsEverything();
  TheBarGivesWayToTheLabelFirst();
  ThenTheBarGoesThenTheDerivedColumns();
  ThenTheLabelWraps();
  ANameTooLongScrollsTheTable();
  NoRoomAtAllStillGivesWidths();
  FullWidthIsEverythingWithTheSmallestBar();
  CardsNeverStandThreeAndOne();
  SectionsStandSideBySideWhenEachFitsInHalf();
  EachSectionGoesToTheShorterColumn();
}
