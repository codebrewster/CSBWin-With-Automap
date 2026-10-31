#ifndef HEROSTATSDRAW_H
#define HEROSTATSDRAW_H

#include <vector>
#include "HeroStats.h"
#include "HeroStatsCatalogue.h"
#include "HeroStatsView.h"

// The pages of the Statistics window, drawn with Dear ImGui in the window's own context.
// HeroStatsWindow.cpp owns the OS window, the settings and the state below.

// What the window shows. who: a champion or HERO_STATS_PARTY; topic: HeroStatsTopic; level:
// HERO_STATS_WHOLE_RUN or a level; perLevel: the Per level table instead of one level;
// sideBySide: the Party's Per level shows one number per champion (gridNumber of the topic);
// closedSections: the folded sections, see HeroStatsDraw_SectionBit.
struct HeroStatsDrawState
{
  HeroStatsChampion who;
  int topic;
  int level;
  bool perLevel;
  bool sideBySide;
  HeroStatsNumber gridNumber[HERO_TOPIC_COUNT];
  int closedSections;
};
HeroStatsDrawState HeroStatsDraw_InitialState(void);

// The bit of a section in closedSections: a detail table (HeroStatsDetail), the Numbers table
// or the Overview's Key numbers
int HeroStatsDraw_SectionBit(int detail);
const int HERO_STATS_NUMBERS_SECTION = -1;
const int HERO_STATS_KEY_NUMBERS_SECTION = -2;

void HeroStatsDraw_ApplyStyle(void);
// The whole window: the title strip, who, the level, the topic tabs and the page
void HeroStatsDraw_Frame(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                         const HeroStatsPicker &picker, void (*exportStatistics)(void));
// The pages' numbers are rebuilt on the next frame (a game loaded, the window shown again)
void HeroStatsDraw_Forget(void);

#endif
