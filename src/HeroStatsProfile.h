#ifndef HEROSTATSPROFILE_H
#define HEROSTATSPROFILE_H

#include <string>
#include <utility>
#include <vector>
#include "HeroStats.h"

// The Statistics window's Profile: the run as star plots, a bar per level and, for Per level, a
// table of the completion. Every value on a star is a ratio of two counts of the same thing, a
// share of a whole the axes split, a rank, or a share of what the dungeon holds. No engine, no
// ImGui: the window draws it, the HTML export writes it as SVG.

#pragma pack(push, 8)

// One shape: a champion, or HERO_STATS_PARTY. values runs parallel to the star's axes, -1 for a
// value with nothing to work it out from. texts: each value written short ("62%", "Expert", "-"),
// details: what is behind it, for the hover ("31 of 50").
struct HeroStatsStarShape
{
  HeroStatsChampion who;
  std::vector<double> values;
  std::vector<std::string> texts, details;
  bool faint;   // drawn behind, for comparison
};

struct HeroStatsStar
{
  std::string title, caption;
  std::vector<std::string> axes;
  std::vector<HeroStatsStarShape> shapes;               // drawing order: faint ones first
  double outer;                                         // the value at the edge
  std::vector<std::pair<double, std::string> > rings;   // value and label ("50%", "Expert")
  double evenRing;                                      // Roles: 1 / champions shown; else -1
  double centre;                                        // the value at the centre (Trained: rank 1)
};

struct HeroStatsLevelBar { int level; double explored; };   // -1: nothing reachable known
struct HeroStatsCompletionRow { int level; double values[HERO_COMPLETION_AXIS_COUNT]; };  // -1: nothing there

struct HeroStatsProfile
{
  HeroStatsStar completion, played, trained, mana, roles;
  bool showRoles;                                // two champions or more with any of its values
  std::vector<HeroStatsLevelBar> levelBars;      // the visited levels, ascending
  std::vector<HeroStatsCompletionRow> perLevel;  // the visited levels, then the whole run (HERO_STATS_WHOLE_RUN)
};

// who: HERO_STATS_PARTY or a champion (bright, the others faint); level: one, or
// HERO_STATS_WHOLE_RUN. Completion and the bars are always the party's.
HeroStatsProfile HeroStatsProfile_Make(const HeroStats &stats, HeroStatsChampion who, int level);

#pragma pack(pop)

#endif
