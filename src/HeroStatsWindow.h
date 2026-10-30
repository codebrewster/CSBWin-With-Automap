#ifndef HEROSTATSWINDOW_H
#define HEROSTATSWINDOW_H

#include <SDL.h>

// The second OS window, beside the map's, that shows the hero statistics. It draws with Dear
// ImGui in a context of its own; every function here leaves the game's ImGui context current.
// This header declares functions only, so that it can be included after CSB.h, which leaves
// #pragma pack(1) switched on.

// Reads and writes the window's entries of csb.cfg; the caller has selected the file
void HeroStatsWindow_ReadSettings(void);
void HeroStatsWindow_WriteSettings(void);
// Opens the window if the settings say it was open when the game was last left
void HeroStatsWindow_RestoreVisibility(void);

bool HeroStatsWindow_IsShown(void);
// Creates the window and its ImGui context on first use. Tells the player and stays hidden
// when that fails.
void HeroStatsWindow_Show(void);
void HeroStatsWindow_Hide(void);

// True when the event was for this window: it then went to the window's own ImGui context and
// must go no further, neither to the map nor to the game
bool HeroStatsWindow_HandleEvent(const SDL_Event &event);
// Draws a frame if the window is shown, at most about 30 a second; restores the game's ImGui
// context
void HeroStatsWindow_Paint(void);
// The game hides the system pointer because it draws its own; over this window there is none
bool HeroStatsWindow_PointerIsOver(void);

#endif
