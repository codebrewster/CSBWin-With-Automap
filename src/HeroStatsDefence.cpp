// HeroStatsDefence.cpp : what took damage off a hit on a champion
#include "HeroStatsDefence.h"
#include <math.h>

// Shares total out by weights (summing to sum > 0) in whole numbers that add up to total: each
// share rounded down, the rest one by one to the largest fractions
static void ShareOut(long long total, const double *weights, double sum, int count, int *out)
{
  double exact[HERO_DEFENCE_PROTECTION_PARTS];
  long long given = 0;
  for (int k = 0; k < count; k++)
  {
    exact[k] = (double)total * weights[k] / sum;
    out[k] = (int)floor(exact[k]);
    given += out[k];
  }
  for (long long left = total - given; left > 0; left--)
  {
    int best = -1;
    for (int k = 0; k < count; k++)
      if (weights[k] != 0 && (best < 0 || exact[k] - out[k] > exact[best] - out[best])) best = k;
    if (best < 0) break;
    out[best]++;
  }
}

static void SplitProtection(const std::vector<HeroProtectionPart> &parts, long long taken, int *out)
{
  double weights[HERO_DEFENCE_PROTECTION_PARTS] = {};
  double sum = 0;
  for (size_t p = 0; p < parts.size(); p++)
  {
    long long points = 0;
    for (int k = 0; k < HERO_DEFENCE_PROTECTION_PARTS; k++) points += parts[p].points[k];
    // No protection on this part, or a sum the engine wrapped in 16 bits: nothing to share
    if (parts[p].value <= 0 || points <= 0) continue;
    for (int k = 0; k < HERO_DEFENCE_PROTECTION_PARTS; k++)
      weights[k] += (double)parts[p].value * parts[p].points[k] / (double)points;
    sum += parts[p].value;
  }
  if (sum <= 0 || taken <= 0) return;
  ShareOut(taken, weights, sum, HERO_DEFENCE_PROTECTION_PARTS, out);
}

void HeroDefence_Split(const HeroDamageSteps &steps, int out[HERO_DEFENCE_PART_COUNT])
{
  for (int k = 0; k < HERO_DEFENCE_PART_COUNT; k++) out[k] = 0;
  if (steps.raw <= 0) return;
  long long scale = steps.scale;
  long long afterShield = steps.afterShield > 0 ? steps.afterShield : 0;
  long long final = steps.final > 0 ? steps.final : 0;
  out[HeroDefenceResistance] = (int)((steps.noResistance - (long long)steps.afterResistance) * scale);
  out[HeroDefenceShieldSpell] = (int)((steps.afterResistance - afterShield) * scale);
  long long last = afterShield * scale - final * 64;
  if (steps.scale == HeroScaleWisdom) out[HeroDefenceWisdom] = (int)last;
  else if (steps.scale == HeroScaleProtection) SplitProtection(steps.parts, last, out + HERO_DEFENCE_FIRST_PROTECTION);
}
