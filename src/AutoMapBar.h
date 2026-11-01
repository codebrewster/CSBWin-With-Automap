#ifndef AUTOMAPBAR_H
#define AUTOMAPBAR_H

// AutoMapBar : the strip above the map that holds every map setting (level, follow, vision,
// spoilers, show, search, controls help, debug), and the panels it opens over the map: the SHOW
// dropdown, the controls help and the level list. Window pixels throughout; the map below the bar
// is the window's. Declarations only, so it can be included after CSB.h.

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
void AutoMapBar_ClosePanels(void);

#endif
