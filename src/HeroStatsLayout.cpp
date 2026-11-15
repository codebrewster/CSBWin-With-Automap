// HeroStatsLayout.cpp : see HeroStatsLayout.h
#include "HeroStatsLayout.h"

#include <algorithm>

// The narrowest the table's bar may be: the smallest bar, or its header when that is wider
static float SmallestBar(const HeroStatsTableNeeds &needs)
{
  return std::max(HERO_LAYOUT_BAR_SMALLEST, needs.barHeader);
}

float HeroStatsLayout_FullWidth(const HeroStatsTableNeeds &needs)
{
  return needs.label + needs.numbers + needs.derived + (needs.hasBar ? SmallestBar(needs) : 0);
}

HeroStatsTableFit HeroStatsLayout_FitTable(const HeroStatsTableNeeds &needs, float available)
{
  HeroStatsTableFit fit = {false, false, false, false, needs.label, 0, available, true, true, false};
  float numbers = needs.numbers;
  float withDerived = needs.label + numbers + needs.derived;
  if (needs.hasBar && withDerived + SmallestBar(needs) <= available)
  {
    // The bar's share, but never at the cost of the label's single line
    fit.bar = true;
    fit.derived = needs.derived > 0;
    fit.barWidth = std::min(std::max(SmallestBar(needs), HERO_LAYOUT_BAR_SHARE * available), available - withDerived);
    fit.labelWidth = available - needs.numbers - needs.derived - fit.barWidth;
    return fit;
  }
  // The champions' names, then half the padding
  if (withDerived > available && needs.names > 0)
  {
    fit.names = false;
    numbers -= needs.names;
  }
  if (needs.label + numbers + needs.derived > available && needs.padding > 0)
  {
    fit.fullPadding = false;
    numbers -= needs.padding;
  }
  if (needs.derived > 0 && needs.label + numbers + needs.derived <= available)
  {
    fit.derived = true;
    fit.labelWidth = available - numbers - needs.derived;
    return fit;
  }
  fit.labelWidth = available - numbers;
  if (fit.labelWidth >= needs.label) return fit;
  fit.wrap = true;
  if (fit.labelWidth >= HERO_LAYOUT_LABEL_SMALLEST) return fit;
  if (needs.shortNumbers > 0)
  {
    fit.shortNumbers = true;
    // Short numbers never stand under a champion's name: a table whose names saved nothing while
    // its numbers were long would cut the names now
    fit.names = false;
    numbers -= needs.shortNumbers;
    fit.labelWidth = available - numbers;
    fit.wrap = fit.labelWidth < needs.label;
    if (fit.labelWidth >= std::min(needs.label, HERO_LAYOUT_LABEL_SMALLEST)) return fit;
    fit.wrap = true;
  }
  fit.scroll = true;
  fit.labelWidth = std::min(needs.label, HERO_LAYOUT_LABEL_SMALLEST);
  fit.tableWidth = fit.labelWidth + numbers;
  return fit;
}

int HeroStatsLayout_Across(int count, float available, float smallest, float gap)
{
  int fits = 1;
  while (fits < count && (fits + 1) * smallest + fits * gap <= available) fits++;
  if (count == 4 && fits == 3) return 2;
  return fits;
}

bool HeroStatsLayout_TwoColumns(const std::vector<float> &fullWidths, float available, float gap)
{
  if (fullWidths.size() < 2) return false;
  float half = (available - gap) / 2;
  for (size_t i = 0; i < fullWidths.size(); i++)
    if (fullWidths[i] > half) return false;
  return true;
}

std::vector<int> HeroStatsLayout_Flow(const std::vector<float> &heights)
{
  std::vector<int> columns;
  float left = 0, right = 0;
  for (size_t i = 0; i < heights.size(); i++)
  {
    bool goesRight = right < left;
    columns.push_back(goesRight ? 1 : 0);
    (goesRight ? right : left) += heights[i];
  }
  return columns;
}

HeroStatsListValues HeroStatsLayout_ListValues(float needed, float kinetic, float values, float available)
{
  if (needed <= available) return HeroListAllValues;
  if (needed - kinetic <= available) return HeroListDamageAndArmor;
  return HeroListNoValues;
}

HeroStatsPartyStep HeroStatsLayout_PartyStep(int count, int across, float withBoth, float withInventory, float available)
{
  if (count <= 0 || across != count) return HeroPartyPlain;
  if (withBoth <= available) return HeroPartyAttributesAndInventory;
  if (withInventory <= available) return HeroPartyInventory;
  return HeroPartyPlain;
}
