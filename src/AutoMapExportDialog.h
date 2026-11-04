#ifndef AUTOMAPEXPORTDIALOG_H
#define AUTOMAPEXPORTDIALOG_H

#include <SDL.h>
#include <string>

// The map window's EXPORT dialog: which levels, how much vision and spoilers the page may show,
// and the file; then it writes the page (AutoMapExport.cpp) while the window shows the progress,
// and offers to open it. It is drawn over the map below the bar and takes every press, key and
// typed letter of the map window while it is open. It names std::string: include it before CSB.h.

bool AutoMapExportDialog_IsOpen(void);
void AutoMapExportDialog_Open(void);
void AutoMapExportDialog_Close(void);
// Window pixels, the whole window
void AutoMapExportDialog_Draw(int width, int height);
// A left press below the bar; always true while the dialog is open
bool AutoMapExportDialog_HandlePress(int x, int y);
void AutoMapExportDialog_HandleKey(SDL_Keycode sym, Uint16 mod);
void AutoMapExportDialog_HandleText(const char *utf8);
// For pictures of the dialog (CSB_MAP_RENDER): open it as "form", "pick", "busy" or "done"
void AutoMapExportDialog_OpenForPicture(const std::string &how);

#endif
