#ifndef AUTOMAPEXPORTCHOICE_H
#define AUTOMAPEXPORTCHOICE_H

#include <set>
#include <string>
#include <vector>

// The rules of the map window's EXPORT dialog that need neither the engine nor SDL: which levels
// a choice takes, the PICK buttons, the page's file name and URL, the size the page will have
// about, and typing a file name.

enum AutoMapExportLevels
{
  AutoMapExportThisLevel,
  AutoMapExportVisited,
  AutoMapExportAll,
  AutoMapExportPick
};

// The levels in order; visited[level] says whether the party explored any cell of it
std::vector<int> AutoMapExportChoice_Levels(AutoMapExportLevels which, int levelCount, int shownLevel,
                                            const std::vector<bool> &visited, const std::set<int> &picked);
// PICK's buttons: CLEAR, ALL, INVERT and VISITED
std::set<int> AutoMapExportChoice_PickAll(int levelCount);
std::set<int> AutoMapExportChoice_PickInverted(const std::set<int> &picked, int levelCount);
std::set<int> AutoMapExportChoice_PickVisited(const std::vector<bool> &visited);

// "csb-BeforeWin" gives "csb-BeforeWin-map.html", "csbgame.dat" "csbgame-map.html"; no save
// gives "csb-map.html". The folder of savePath is left out.
std::string AutoMapExportChoice_PageName(const std::string &savePath);
// name with ".html" added when it has no extension of its own
std::string AutoMapExportChoice_WithHtmlExtension(const std::string &name);
void AutoMapExportChoice_SplitPath(const std::string &path, std::string *folder, std::string *name);
// folder and name with the separator of the system (\ on Windows, / elsewhere) between them
std::string AutoMapExportChoice_Join(const std::string &folder, const std::string &name);
// A file:// URL of an absolute path, with every byte outside A-Z a-z 0-9 - . _ ~ / : escaped;
// a Windows path (C:\x) gets its third slash and forward slashes
std::string AutoMapExportChoice_FileUrl(const std::string &absolutePath);

// About how big the page comes out: measured on the Chaos Strikes Back dungeon (14 levels, all
// 9 views: 1.07 MB, the pictures about 350 KB of it)
long AutoMapExportChoice_EstimatedBytes(int levelCount, int maxVision, int maxSpoilers);
// "350 KB", "1.1 MB"
std::string AutoMapExportChoice_SizeText(long bytes);

// The name after typing utf8: letters a file name cannot hold (/ \ : * ? " < > | and control
// characters) are left out
std::string AutoMapExportChoice_Typed(const std::string &name, const char *utf8);
// The name after Backspace: one whole character off its end
std::string AutoMapExportChoice_Erased(const std::string &name);

#endif
