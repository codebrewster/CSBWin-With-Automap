// ExportHtml.cpp : see ExportHtml.h. Include AutoMapExport.h before CSB.h (it holds std::string).
#include "AutoMapExport.h"
#include "stdafx.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/stat.h>
#include "AutoMapHooks.h"
#include "AutoMapBar.h"
#include "ExportHtml.h"
#include "HeroStatsHooks.h"
#include "HeroStatsWindow.h"
#include "CSB.h"
#include "Data.h"

extern ui32 numRandomCalls;
extern bool GameIsComplete;

static bool on = false;
static std::string outDir;
static bool replayEnded = false;
static int replayEndedTick = 0;
// The game's ending after the replay's last line takes about 6000 ticks (the fusion, the texts);
// more than this means the game is not heading for a victory screen
static const int TICKS_AFTER_THE_END_AT_MOST = 40000;

bool ExportHtml_IsOn(void)
{
  return on;
}

void ExportHtml_Enable(const char *dir)
{
  on = true;
  outDir = dir;
  while (outDir.size() > 1 && outDir[outDir.size() - 1] == '/') outDir.erase(outDir.size() - 1);
  // mkdir -p
  for (size_t i = 1; i <= outDir.size(); i++)
    if (i == outDir.size() || outDir[i] == '/') mkdir(outDir.substr(0, i).c_str(), 0777);
  setenv("SDL_VIDEODRIVER", "dummy", 1);
}

static bool WriteFile(const std::string &path, const std::string &bytes)
{
  FILE *f = fopen(path.c_str(), "wb");
  bool ok = f != NULL && fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
  if (f != NULL && fclose(f) != 0) ok = false;
  return ok;
}

static std::string Basename(const std::string &path)
{
  size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

static void WriteMap(void)
{
  AutoMapHtmlChoice choice;
  for (int level = 0; level < AutoMap_LevelCount(); level++) choice.levels.push_back(level);
  choice.maxVision = 2;   // whole level
  choice.maxSpoilers = 2; // all
  choice.vision = AutoMap_Vision();
  choice.spoilers = AutoMap_SpoilerLevel();
  choice.level = d.partyLevel;
  choice.saveName = Basename(outDir);
  if (choice.saveName.empty() || choice.saveName == ".") choice.saveName = "csb";
  choice.title = "CSB map: " + choice.saveName;
  choice.barStyle = AutoMapBar_StyleName();
  AutoMapExport_HtmlBeginForTheGame(choice);
  while (AutoMapExport_HtmlNextLevel() >= 0) AutoMapExport_HtmlStep();
  std::string page = AutoMapExport_HtmlPage();
  std::string path = outDir + "/map.html";
  if (!WriteFile(path, page)) { printf("export-html: cannot write %s\n", path.c_str()); fflush(stdout); _Exit(4); }
}

static void Finish(const char *how)
{
  WriteMap();
  std::string path = outDir + "/statistics.html";
  if (!HeroStatsWindow_WriteExport(path)) { printf("export-html: cannot write %s\n", path.c_str()); fflush(stdout); _Exit(4); }
  printf("export-html: %s at tick %d (statistics count %d), written to %s\n", how, (int)d.Time,
         HeroStats_Store().Movement(HERO_STATS_WHOLE_RUN).ticks, outDir.c_str());
  fflush(stdout);
  _Exit(0);
}

void ExportHtml_OnReplayEnd(void)
{
  if (!on) return;
  replayEnded = true;
  replayEndedTick = (int)d.Time;
  // In the ending (the fusion, the end of game actuator) the victory screen comes by itself
  if (d.Word11694 == 0) Finish("replay ended");
}

void ExportHtml_OnGameEnd(void)
{
  if (on) Finish("game ended");
}

void ExportHtml_OnMessageBox(const char *msg)
{
  if (!on) return;
  std::string text = msg != NULL ? msg : "";
  for (size_t i = 0; i < text.size(); i++) if (text[i] == '\n') text[i] = ' ';
  printf("export-html: message box at tick %d: %s\n", (int)d.Time, text.c_str());
  fflush(stdout);
  _Exit(2);
}

void ExportHtml_AtTick(void)
{
  if (on && replayEnded && (int)d.Time - replayEndedTick > TICKS_AFTER_THE_END_AT_MOST) Finish("gave up waiting for the ending");
}
