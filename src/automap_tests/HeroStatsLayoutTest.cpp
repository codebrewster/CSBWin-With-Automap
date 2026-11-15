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

// A bar is never narrower than its header ("Share of party", "Who: Experience")
static void ABarKeepsRoomForItsHeader(void)
{
  HeroStatsTableNeeds needs = {200, 300, 100, true, 150};
  CHECK(HeroStatsLayout_FullWidth(needs) == 750);
  CHECK(!HeroStatsLayout_FitTable(needs, 720).bar);          // 110 would fit, the header's 150 does not
  HeroStatsTableFit fit = HeroStatsLayout_FitTable(needs, 760);
  CHECK(fit.bar && fit.barWidth == 160 && fit.labelWidth == 200);
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

// A table too wide gives up, in this order: the bar, the champions' names, half its padding, the
// worked-out columns, the label's single line, the long form of big numbers; then it scrolls
static void ATableGivesUpOneThingAfterTheOther(void)
{
  HeroStatsTableNeeds needs = {160, 300, 80, true, 60, 90, 40, 70};
  CHECK(HeroStatsLayout_FullWidth(needs) == 650);
  HeroStatsTableFit f = HeroStatsLayout_FitTable(needs, 650);
  CHECK(f.bar && f.names && f.fullPadding && f.derived && !f.wrap && !f.shortNumbers && !f.scroll);
  f = HeroStatsLayout_FitTable(needs, 540);   // 160 + 300 + 80: the bar goes
  CHECK(!f.bar && f.names && f.fullPadding && f.derived && !f.wrap);
  f = HeroStatsLayout_FitTable(needs, 450);   // - 90: the names go
  CHECK(!f.bar && !f.names && f.fullPadding && f.derived && !f.wrap);
  f = HeroStatsLayout_FitTable(needs, 410);   // - 40: the padding goes
  CHECK(!f.names && !f.fullPadding && f.derived && !f.wrap && f.labelWidth == 160);
  f = HeroStatsLayout_FitTable(needs, 330);   // - 80: the derived columns go
  CHECK(!f.derived && !f.wrap && f.labelWidth == 160);
  f = HeroStatsLayout_FitTable(needs, 300);   // the label wraps: 300 - 170 = 130, not below 120
  CHECK(f.wrap && !f.shortNumbers && !f.scroll && f.labelWidth == 130);
  f = HeroStatsLayout_FitTable(needs, 230);   // 230 - 170 is below 120: the numbers get short, 230 - 100 = 130
  CHECK(f.wrap && f.shortNumbers && !f.scroll && f.labelWidth == 130 && f.tableWidth == 230);
  f = HeroStatsLayout_FitTable(needs, 200);   // 200 - 100 is below 120: it scrolls
  CHECK(f.scroll && f.shortNumbers && !f.names && !f.fullPadding && f.labelWidth == 120 && f.tableWidth == 220);
  // Short numbers that make room for the whole label: it no longer wraps
  HeroStatsTableNeeds roomy = {160, 300, 0, false, 0, 0, 0, 200};
  f = HeroStatsLayout_FitTable(roomy, 300);
  CHECK(f.shortNumbers && !f.wrap && !f.scroll && f.labelWidth == 200);
  // Short numbers never stand under a champion's name: the header is the dot then, also in a
  // table whose names saved nothing while its numbers were long
  CHECK(!f.names);
  // A table with nothing to give up scrolls as before, and says it keeps what it has
  HeroStatsTableNeeds plain = {160, 300, 0, false, 0, 0, 0, 0};
  f = HeroStatsLayout_FitTable(plain, 400);
  CHECK(f.scroll && f.names && f.fullPadding && !f.shortNumbers);
}

// Before the inventory list would scroll it gives up Kinetic, then Damage and Armor
static void TheListGivesUpKineticThenDamageAndArmor(void)
{
  CHECK(HeroStatsLayout_ListValues(680, 70, 120, 700) == HeroListAllValues);
  CHECK(HeroStatsLayout_ListValues(680, 70, 120, 680) == HeroListAllValues);
  CHECK(HeroStatsLayout_ListValues(680, 70, 120, 679) == HeroListDamageAndArmor);
  CHECK(HeroStatsLayout_ListValues(680, 70, 120, 610) == HeroListDamageAndArmor);
  CHECK(HeroStatsLayout_ListValues(680, 70, 120, 609) == HeroListNoValues);
  CHECK(HeroStatsLayout_ListValues(680, 70, 120, 100) == HeroListNoValues);
}

// A Party card shows the most that fits in the height, and only when the cards stand in one row
static void APartyCardShowsTheMostThatFits(void)
{
  CHECK(HeroStatsLayout_PartyStep(4, 4, 882, 742, 900, 300, 236) == HeroPartyAttributesAndInventory);
  CHECK(HeroStatsLayout_PartyStep(4, 4, 882, 742, 882, 300, 236) == HeroPartyAttributesAndInventory);
  CHECK(HeroStatsLayout_PartyStep(4, 4, 882, 742, 881, 300, 236) == HeroPartyInventory);
  CHECK(HeroStatsLayout_PartyStep(4, 4, 882, 742, 742, 300, 236) == HeroPartyInventory);
  CHECK(HeroStatsLayout_PartyStep(4, 4, 882, 742, 741, 300, 236) == HeroPartyPlain);
  CHECK(HeroStatsLayout_PartyStep(4, 2, 882, 742, 2000, 300, 236) == HeroPartyPlain);
  CHECK(HeroStatsLayout_PartyStep(2, 2, 882, 742, 900, 300, 236) == HeroPartyAttributesAndInventory);
  CHECK(HeroStatsLayout_PartyStep(1, 1, 882, 742, 800, 300, 236) == HeroPartyInventory);
  CHECK(HeroStatsLayout_PartyStep(0, 0, 882, 742, 2000, 300, 236) == HeroPartyPlain);
  // A card narrower than the inventory's picture stays plain, however tall the page
  CHECK(HeroStatsLayout_PartyStep(1, 1, 882, 742, 2000, 235, 236) == HeroPartyPlain);
  CHECK(HeroStatsLayout_PartyStep(1, 1, 882, 742, 2000, 236, 236) == HeroPartyAttributesAndInventory);
}

void TestHeroStatsLayout(void)
{
  TheListGivesUpKineticThenDamageAndArmor();
  APartyCardShowsTheMostThatFits();
  ATableGivesUpOneThingAfterTheOther();
  AWideTableShowsEverything();
  TheBarGivesWayToTheLabelFirst();
  ThenTheBarGoesThenTheDerivedColumns();
  ThenTheLabelWraps();
  ANameTooLongScrollsTheTable();
  NoRoomAtAllStillGivesWidths();
  ABarKeepsRoomForItsHeader();
  FullWidthIsEverythingWithTheSmallestBar();
  CardsNeverStandThreeAndOne();
  SectionsStandSideBySideWhenEachFitsInHalf();
  EachSectionGoesToTheShorterColumn();
}
