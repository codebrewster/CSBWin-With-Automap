#ifndef AUTOMAPBAR_H
#define AUTOMAPBAR_H

#include <string>
#include "AutoMapLayout.h"

// AutoMapBar : the strip above the map that holds every map setting (level, follow, vision,
// spoilers, show, search, controls help, debug), and the panels it opens over the map: the SHOW
// dropdown, the controls help, the level list and the export dialog. Window pixels throughout;
// the map below the bar is the window's. It names AutoMapRect and std::string: include it after
// AutoMapLayout.h and before CSB.h.

int AutoMapBar_Height(void);
void AutoMapBar_Draw(int width);
// The panels the bar opened, drawn over the map (window pixels, the whole window)
void AutoMapBar_DrawPanels(int width, int height);
// A left press at window pixel (x, y): true when the bar or one of its panels took it. An open
// panel closes on a press outside it, and takes that press too.
bool AutoMapBar_HandlePress(int x, int y, int width);
// The wheel at window pixel (x, y): true when it scrolled the bar or a panel
bool AutoMapBar_HandleWheel(int x, int y, int notches);
// A key while a panel is open: true when the panel took it (Escape closes; the level list types)
bool AutoMapBar_HandleKey(int sym);
bool AutoMapBar_PanelIsOpen(void);
bool AutoMapBar_LevelListIsOpen(void);
void AutoMapBar_ClosePanels(void);
// Where the / button sits, in window pixels, for the search panel to open below it
int AutoMapBar_SearchButtonX(void);
// A button and the well a group of buttons sits in, in the bar's look, for the export dialog
void AutoMapBar_DrawButton(const AutoMapRect &r, const std::string &label, bool on);
void AutoMapBar_DrawWell(const AutoMapRect &r);
int AutoMapBar_ButtonWidth(const std::string &label);
int AutoMapBar_ButtonHeight(void);
// The bar's look, map_bar_style in csb.cfg: stone (the default), segmented, cyan, brass or
// classic; any other name gives stone
void AutoMapBar_SetStyle(const char *name);
const char *AutoMapBar_StyleName(void);

#endif
