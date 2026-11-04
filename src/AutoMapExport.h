#ifndef AUTOMAPEXPORT_H
#define AUTOMAPEXPORT_H

// What the map shows, written to files for the reference mockup (docs/automap-mockup): set
// CSB_MAP_EXPORT=<dir> and run csb --visibility-test with a case file naming a save. Include
// it before CSB.h, which leaves #pragma pack(1) switched on: it holds std::string and std::vector.

#include <string>
#include <vector>

class AutoMapRecorder;

bool AutoMapExport_IsRequested(void);
// CSB_MAP_EXPORT=<dir>: writes index.json, one draw list per level, vision and spoiler level
// (L<level>_v<vision>_s<spoilers>.json) and the game's pictures (.bmp) into dir.
// CSB_MAP_RENDER=<out.bmp>,<level>,<vision>,<spoilers>,<cell>,<x>,<y>[,shift]: the map window as it
// would look, drawn off screen into out.bmp (see AutoMapExport.cpp)
// CSB_MAP_HTML=<out.html>,<max vision>,<max spoilers>[,<levels>]: the page of the map window's
// EXPORT (see AutoMapExport.cpp)
void AutoMapExport_Run(AutoMapRecorder &recorder, int partyLevel);

// The map window's EXPORT: one web page with the page code of docs/automap-mockup, the chosen
// levels' draw lists and their pictures. Begin, then Step once per level (the window shows
// the progress between steps), then Page. Spoilers are numbered as in csb.cfg (0 seen, 1
// learned, 2 all); vision 0 fog, 1 explored live, 2 whole level.
struct AutoMapHtmlChoice
{
  std::vector<int> levels;
  // The page holds every vision and spoiler level up to these
  int maxVision;
  int maxSpoilers;
  // What the page starts with: the map window's own, held inside the limits above
  int vision;
  int spoilers;
  int level;
  std::string saveName;
  std::string title;
  // The page's bar: stone, segmented, cyan, brass or classic, as map_bar_style
  std::string barStyle;
};
// After CSB.h the struct would be packed in one file and not in the next: its strings would be
// read at the wrong places
static_assert(alignof(AutoMapHtmlChoice) == alignof(std::string), "include AutoMapExport.h before CSB.h");

void AutoMapExport_HtmlBegin(AutoMapRecorder &recorder, int partyLevel, const AutoMapHtmlChoice &choice);
// One chosen level; true when every level is done
bool AutoMapExport_HtmlStep(void);
int AutoMapExport_HtmlLevelsDone(void);
int AutoMapExport_HtmlLevelsToDo(void);
// The level the next Step works on; -1 when none is left
int AutoMapExport_HtmlNextLevel(void);
// The whole page (doing any steps left first); the export is over afterwards
std::string AutoMapExport_HtmlPage(void);
void AutoMapExport_HtmlCancel(void);
// The same with the game's own map memory and party (AutoMapHooks.cpp)
void AutoMapExport_HtmlBeginForTheGame(const AutoMapHtmlChoice &choice);
// The file of the saved game the party came from, as the engine found it; empty before the
// game was first saved (AutoMapHooks.cpp)
std::string AutoMapExport_SavedGamePath(void);

#endif
