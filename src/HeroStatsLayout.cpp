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
  HeroStatsTableFit fit = {false, false, false, false, needs.label, 0, available};
  float withDerived = needs.label + needs.numbers + needs.derived;
  if (needs.hasBar && withDerived + SmallestBar(needs) <= available)
  {
    // The bar's share, but never at the cost of the label's single line
    fit.bar = true;
    fit.derived = needs.derived > 0;
    fit.barWidth = std::min(std::max(SmallestBar(needs), HERO_LAYOUT_BAR_SHARE * available), available - withDerived);
    fit.labelWidth = available - needs.numbers - needs.derived - fit.barWidth;
    return fit;
  }
  if (needs.derived > 0 && withDerived <= available)
  {
    fit.derived = true;
    fit.labelWidth = available - needs.numbers - needs.derived;
    return fit;
  }
  fit.labelWidth = available - needs.numbers;
  if (fit.labelWidth >= needs.label) return fit;
  fit.wrap = true;
  if (fit.labelWidth >= HERO_LAYOUT_LABEL_SMALLEST) return fit;
  fit.scroll = true;
  fit.labelWidth = std::min(needs.label, HERO_LAYOUT_LABEL_SMALLEST);
  fit.tableWidth = fit.labelWidth + needs.numbers;
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
