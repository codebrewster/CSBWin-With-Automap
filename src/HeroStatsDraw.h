#ifndef HEROSTATSDRAW_H
#define HEROSTATSDRAW_H

#include <vector>
#include "imgui.h"
#include "HeroStats.h"
#include "HeroStatsCatalogue.h"
#include "HeroStatsLive.h"
#include "HeroStatsView.h"

// The pages of the Statistics window, drawn with Dear ImGui in the window's own context.
// HeroStatsWindow.cpp owns the OS window, the settings and the state below.

// What the window shows. who: a champion or HERO_STATS_PARTY; topic: HeroStatsTopic; level:
// HERO_STATS_WHOLE_RUN or a level; perLevel: the Per level table instead of one level;
// sideBySide: the Party's Per level shows one number per champion (gridNumber of the topic);
// closedSections: the folded sections, see HeroStatsDraw_SectionBit.
// The tab after the topics: the champions as they are now (HeroStatsLive.h), not a topic of the
// statistics, so the topics' tables never see it
const int HERO_STATS_CHAMPIONS_TAB = HERO_TOPIC_COUNT;

struct HeroStatsDrawState
{
  HeroStatsChampion who;
  int topic;
  int level;
  bool perLevel;
  bool sideBySide;
  HeroStatsNumber gridNumber[HERO_TOPIC_COUNT];
  int closedSections;
  bool inventoryList;  // the Champions tab shows a champion's inventory as a list, not as the picture
};
HeroStatsDrawState HeroStatsDraw_InitialState(void);

// The bit of a section in closedSections: a detail table (HeroStatsDetail), the Numbers table
// or the Overview's Key numbers
int HeroStatsDraw_SectionBit(int detail);
const int HERO_STATS_NUMBERS_SECTION = -1;
const int HERO_STATS_KEY_NUMBERS_SECTION = -2;
// The sections of a champion's Champions page: bits 16384, 32768 and 65536
const int HERO_STATS_ATTRIBUTES_SECTION = -3;
const int HERO_STATS_RANKS_SECTION = -4;
const int HERO_STATS_INVENTORY_SECTION = -5;

// The pictures of the Champions tab as textures of the window's renderer; 0 when there is none.
// icon: an item's inventory icon (HeroStatsLiveItem::iconGraphic and iconTile), 16 x 16.
// portrait: a champion's, with its size.
struct HeroStatsDrawPictures
{
  ImTextureID (*icon)(int graphic, int tile);
  ImTextureID (*portrait)(HeroStatsChampion who, int *width, int *height);
};

void HeroStatsDraw_ApplyStyle(void);
// The whole window: the title strip, who, the level, the topic tabs and the page. live is the
// party now, for the Champions tab.
void HeroStatsDraw_Frame(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                         const HeroStatsPicker &picker, const HeroStatsLiveParty &live, const HeroStatsDrawPictures &pictures,
                         void (*exportStatistics)(void));
// True when the frame just drawn was laid out with widths that are now known to be stale: a table
// shown for the first time, numbers that grew wider, another page, another window width. The
// window then draws again before it shows anything, so the page never visibly jumps into place.
bool HeroStatsDraw_WantsAnotherPass(void);
// The pages' numbers are rebuilt on the next frame (a game loaded, the window shown again)
void HeroStatsDraw_Forget(void);

#endif
