#ifndef AUTOMAPWINDOW_H
#define AUTOMAPWINDOW_H

#include <SDL.h>

// The second OS window that shows the map. This header declares functions only, so that it can
// be included after CSB.h, which leaves #pragma pack(1) switched on.

struct AutoMapDrawList;

// Reads and writes the window's entries of csb.cfg; the caller has selected the file
void AutoMapWindow_ReadSettings(void);
void AutoMapWindow_WriteSettings(void);
// Opens the window if the settings say it was open when the game was last left
void AutoMapWindow_RestoreVisibility(void);

bool AutoMapWindow_IsShown(void);
// Creates the window on first use. Tells the player and stays hidden when that fails.
void AutoMapWindow_Show(void);
void AutoMapWindow_Hide(void);

// True when the window is shown and has painted the draw list it was given last
bool AutoMapWindow_WantsNewDrawList(void);
void AutoMapWindow_ShowDrawList(const AutoMapDrawList &drawList);
void AutoMapWindow_PaintIfDirty(void);
// The fog or the spoiler level changed what every level shows: an open search is run anew
// over the whole dungeon, not just the shown level
void AutoMapWindow_RefreshSearch(void);

// What the map draws, each switched on and off from the menu and kept in csb.cfg. Hidden things
// still answer the search and the tooltip. The numbers are the bits of map_hide: new entries go
// last so that a saved csb.cfg keeps its meaning.
enum AutoMapShown
{
  AutoMapShowScrolls,
  AutoMapShowChests,
  AutoMapShowPotions,
  AutoMapShowWeapons,
  AutoMapShowArmour,
  AutoMapShowOtherItems,
  AutoMapShowMonsters,
  AutoMapShowWallDecorations,
  AutoMapShowWallWriting,
  AutoMapShowFloorDecorations,
  AutoMapShowFood,
  AutoMapShowKeys,
  AUTOMAP_SHOWN_COUNT
};
bool AutoMapWindow_Shows(AutoMapShown what);
void AutoMapWindow_SetShows(AutoMapShown what, bool isShown);

// Whether map_timing was set while the settings were read
bool AutoMapWindow_TimingReportsOn(void);
// Whether map_debug was set while the settings were read
bool AutoMapWindow_DebugIsOn(void);
// Hands the view back to the party without touching which level is shown
void AutoMapWindow_RecenterView(void);
// Both return true when the event belongs to the map window and must go no further
bool AutoMapWindow_HandleWindowEvent(const SDL_Event &event);
bool AutoMapWindow_HandlePointerEvent(const SDL_Event &event);
// Key and text input for the item search: true when the map consumed the event
// and the game must not see it (search open, or '/' / Ctrl+F opening it)
bool AutoMapWindow_HandleKeyEvent(const SDL_Event &event);
// Page Up / Page Down show the level above or below; true when the key was one of them and
// the map is open. The game window calls it too, for keys the player has not bound.
bool AutoMapWindow_HandleLevelKey(SDL_Keycode sym);
// The game hides the system pointer because it draws its own; over the map there is none
bool AutoMapWindow_PointerIsOverMap(void);

#endif
