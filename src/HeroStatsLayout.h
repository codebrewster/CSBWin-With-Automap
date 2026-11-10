// HeroStatsLayout.h : how the pages of the Statistics window follow its width: what a table gives
// up when it does not fit, how many cards or tiles stand in a row, and when the sections of a
// page stand side by side. The window draws by these; the HTML export follows the same rules in
// its own script. All widths are at 100 % zoom. No engine, no ImGui.
#ifndef HEROSTATSLAYOUT_H
#define HEROSTATSLAYOUT_H

#include <vector>

// A bar column (Share, Who, To next rank) takes this share of its table, but at least the smallest
const float HERO_LAYOUT_BAR_SMALLEST = 110;
const float HERO_LAYOUT_BAR_SHARE = 0.26f;
// A label that wraps keeps at least this
const float HERO_LAYOUT_LABEL_SMALLEST = 120;
// The Champions tab: the champion's column stops growing here, so no bar gets long; a Party card
const float HERO_LAYOUT_CHAMPION_WIDEST = 460;
const float HERO_LAYOUT_CARD_SMALLEST = 290;
const float HERO_LAYOUT_CARD_WIDEST = 480;
const float HERO_LAYOUT_TILE_SMALLEST = 150;

// What a table needs: label: its widest label on one line; numbers: the number columns that
// always show; derived: the columns worked out from others (Rate, Avg), 0 when none. All with
// their cell padding.
struct HeroStatsTableNeeds { float label, numbers, derived; bool hasBar; };
// What it gets at a width: the bar and the derived columns shown or not, the label wrapping, the
// table scrolling sideways on its own. tableWidth is more than what was available only when
// scroll is set.
struct HeroStatsTableFit { bool bar, derived, wrap, scroll; float labelWidth, barWidth, tableWidth; };
// A table too wide gives up, in this order: the bar, the derived columns, the label's single
// line, and at last it scrolls
HeroStatsTableFit HeroStatsLayout_FitTable(const HeroStatsTableNeeds &needs, float available);
// label + numbers + derived + the smallest bar: what the table needs to show everything
float HeroStatsLayout_FullWidth(const HeroStatsTableNeeds &needs);
// How many of count equal boxes stand in a row: as many as fit, but four boxes never three and one
int HeroStatsLayout_Across(int count, float available, float smallest, float gap);
// True when there are at least two sections and each fits in half the page
bool HeroStatsLayout_TwoColumns(const std::vector<float> &fullWidths, float available, float gap);
// The column (0 left, 1 right) of each section: in order, each into the column shorter so far
std::vector<int> HeroStatsLayout_Flow(const std::vector<float> &heights);

#endif
