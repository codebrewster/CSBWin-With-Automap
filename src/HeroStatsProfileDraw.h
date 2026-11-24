#ifndef HEROSTATSPROFILEDRAW_H
#define HEROSTATSPROFILEDRAW_H

#include <vector>
#include "HeroStatsDraw.h"

// The Profile tab's page under the tabs: the star plots and the bars of HeroStatsProfile, or on
// Per level the completion table. A click on a level picks it.
void HeroStatsProfileDraw_Page(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns);

#endif
