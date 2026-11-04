// AutoMapExportDialog.cpp : see AutoMapExportDialog.h
#include "stdafx.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "AutoMapLayout.h"
#include "AutoMapPaint.h"
#include "AutoMapBar.h"
#include "AutoMapExport.h"
#include "AutoMapExportChoice.h"
#include "AutoMapExportDialog.h"
#include "AutoMapHooks.h"
#include "AutoMapWindow.h"
#include "tinyfiledialogs/tinyfiledialogs.h"

static const int PIXEL = 2;
static const int PAD = 12;
static const int ROW = 22;
static const int GAP = 2;
static const SDL_Color PANEL_FILL = {0x11, 0x11, 0x11, 0xff};
static const SDL_Color PANEL_FRAME = {0xff, 0xc8, 0x3a, 0xff};
static const SDL_Color WHITE = {0xff, 0xff, 0xff, 0xff};
static const SDL_Color GOLD = {0xff, 0xcc, 0x33, 0xff};
static const SDL_Color GREY = {0x8f, 0x86, 0x76, 0xff};
static const SDL_Color DIM = {0x55, 0x50, 0x48, 0xff};
static const SDL_Color WARN = {0xff, 0x8a, 0x5c, 0xff};
static const SDL_Color RED = {0xff, 0x4d, 0x4d, 0xff};
static const SDL_Color DIVIDER = {0x3a, 0x36, 0x2f, 0xff};
static const SDL_Color BLACK = {0x00, 0x00, 0x00, 0xff};

enum Stage
{
  StageClosed,
  StageForm,
  StageBusy,
  StageDone,
  StageFailed
};

enum Action
{
  DoNothing,
  DoLevels,      // arg: an AutoMapExportLevels
  DoVision,      // arg: the highest vision
  DoSpoilers,    // arg: the highest spoiler level
  DoBarLook,     // arg: an index into BAR_LOOKS
  DoToggleLevel, // arg: the level
  DoPickNone,
  DoPickAll,
  DoPickInvert,
  DoPickVisited,
  DoBrowse,
  DoOpenWhenDone,
  DoCancel,
  DoExport,
  DoStop,
  DoShowFolder,
  DoOpenPage,
  DoClose
};

struct Hit
{
  AutoMapRect r;
  Action action;
  int arg;
};

static Stage stage = StageClosed;
static AutoMapExportLevels levelsChoice = AutoMapExportAll;
static std::set<int> picked;
static bool pickedIsSet = false;
static int maxVision = 2;
static int maxSpoilers = 2;
// The page's bar look, an index into BAR_LOOKS; -1 takes the map window's own on first opening
static int barLook = -1;
static bool openWhenDone = true;
static std::string folder;
static std::string fileName;
static std::string nameIsFor;      // the saved game the name was made for
static bool replaceAsked = false;  // the file exists and the next EXPORT replaces it
static int browseWorks = -1;       // -1 not asked yet
static bool startedTextInput = false;
static std::string busyLine;
static int busyDone = 0;
static int busyToDo = 0;
static std::string resultPath;
static std::string resultLine;
static std::string failure;
static std::vector<Hit> hits;
static AutoMapRect stopButton = {0, 0, 0, 0};

static const char *const VISION_NAMES[3] = {"FOG", "EXPLORED", "ALL"};
static const char *const SPOILER_NAMES[3] = {"SEEN", "LEARNED", "ALL"};
static const int BAR_LOOK_COUNT = 5;
static const char *const BAR_LOOKS[BAR_LOOK_COUNT] = {"stone", "segmented", "cyan", "brass", "classic"};
static const char *const BAR_LOOK_NAMES[BAR_LOOK_COUNT] = {"STONE", "SEGMENTED", "CYAN", "BRASS", "CLASSIC"};

// ---------------- what is chosen ----------------

static std::vector<bool> VisitedLevels(void)
{
  std::vector<bool> visited;
  for (int level = 0; level < AutoMap_LevelCount(); level++) visited.push_back(AutoMap_LevelIsVisited(level));
  return visited;
}

static std::vector<int> ChosenLevels(void)
{
  return AutoMapExportChoice_Levels(levelsChoice, AutoMap_LevelCount(), AutoMap_ShownLevel(), VisitedLevels(), picked);
}

static std::string Upper(const std::string &text)
{
  std::string upper = text;
  for (size_t i = 0; i < upper.size(); i++)
    if (upper[i] >= 'a' && upper[i] <= 'z') upper[i] = (char)(upper[i] - 'a' + 'A');
  return upper;
}

static std::string Path(void)
{
  return AutoMapExportChoice_Join(folder, fileName);
}

static std::string AbsoluteFolder(const std::string &relative)
{
  std::string dir = relative.empty() ? "." : relative;
#ifdef _WIN32
  char full[_MAX_PATH];
  if (_fullpath(full, dir.c_str(), sizeof(full)) != NULL) return full;
#else
  char *full = realpath(dir.c_str(), NULL);
  if (full != NULL)
  {
    std::string absolute = full;
    free(full);
    return absolute;
  }
#endif
  return dir;
}

static bool FileExists(const std::string &path)
{
  FILE *f = fopen(path.c_str(), "rb");
  if (f == NULL) return false;
  fclose(f);
  return true;
}

// Beside the saved game, named after it; kept as typed until another game is loaded
static void TakeNameFromTheSave(void)
{
  std::string save = AutoMapExport_SavedGamePath();
  if (!fileName.empty() && save == nameIsFor) return;
  nameIsFor = save;
  std::string saveFolder, saveName;
  AutoMapExportChoice_SplitPath(save, &saveFolder, &saveName);
  folder = AbsoluteFolder(saveFolder);
  fileName = AutoMapExportChoice_PageName(save);
}

// The real file dialogs only: tinyfiledialogs falls back to text boxes and consoles otherwise
static bool BrowseWorks(void)
{
  if (browseWorks >= 0) return browseWorks != 0;
  browseWorks = 0;
  if (tinyfd_saveFileDialog("tinyfd_query", "", 0, NULL, NULL) == NULL) return false;
  static const char *const DIALOGS[] = {"windows", "windows_wchar", "applescript", "kdialog", "zenity", "zenity3",
                                        "yad", "matedialog", "shellementary", "qarma", "python2-tkinter",
                                        "python3-tkinter"};
  for (size_t i = 0; i < sizeof(DIALOGS) / sizeof(DIALOGS[0]); i++)
    if (strcmp(tinyfd_response, DIALOGS[i]) == 0) browseWorks = 1;
  return browseWorks != 0;
}

// ---------------- input that arrived while the dialog was busy ----------------

static bool IsMapInput(const SDL_Event &e, Uint32 map)
{
  switch (e.type)
  {
  case SDL_MOUSEBUTTONDOWN: return e.button.windowID == map;
  case SDL_MOUSEMOTION: return e.motion.windowID == map;
  case SDL_MOUSEWHEEL: return e.wheel.windowID == map;
  case SDL_KEYDOWN: return e.key.windowID == map;
  case SDL_TEXTINPUT: return e.text.windowID == map;
  default: return false;
  }
}

// Presses and keys on the map while the page was written, or while the system's dialog was open,
// were not meant for what the dialog shows afterwards. Everything else stays queued, in order.
static void DropMapInput(void)
{
  Uint32 map = AutoMapWindow_WindowId();
  SDL_PumpEvents();
  std::vector<SDL_Event> kept;
  SDL_Event e;
  while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 1)
    if (!IsMapInput(e, map)) kept.push_back(e);
  for (size_t i = 0; i < kept.size(); i++) SDL_PeepEvents(&kept[i], 1, SDL_ADDEVENT, 0, 0);
}

// STOP or Escape while the page is written: read from the state, so that no event is taken away
static bool StopIsPressed(void)
{
  SDL_PumpEvents();
  Uint32 map = AutoMapWindow_WindowId();
  SDL_Window *keys = SDL_GetKeyboardFocus();
  if (keys != NULL && SDL_GetWindowID(keys) == map && SDL_GetKeyboardState(NULL)[SDL_SCANCODE_ESCAPE]) return true;
  SDL_Window *mouse = SDL_GetMouseFocus();
  int x = 0, y = 0;
  Uint32 buttons = SDL_GetMouseState(&x, &y);
  return mouse != NULL && SDL_GetWindowID(mouse) == map && (buttons & SDL_BUTTON_LMASK) && x >= stopButton.x &&
         x < stopButton.x + stopButton.w && y >= stopButton.y && y < stopButton.y + stopButton.h;
}

// ---------------- the export ----------------

static void OpenUrlOf(const std::string &path)
{
  SDL_OpenURL(AutoMapExportChoice_FileUrl(path).c_str());
}

static void WritePage(void)
{
  std::vector<int> levels = ChosenLevels();
  if (levels.empty() || fileName.empty()) return;
  std::string path = Path();
  if (!replaceAsked && FileExists(path))
  {
    replaceAsked = true;
    return;
  }
  replaceAsked = false;
  AutoMapHtmlChoice choice;
  choice.levels = levels;
  choice.maxVision = maxVision;
  choice.maxSpoilers = maxSpoilers;
  choice.vision = AutoMap_Vision();
  choice.spoilers = AutoMap_SpoilerLevel();
  choice.level = AutoMap_ShownLevel();
  std::string saveFolder;
  AutoMapExportChoice_SplitPath(nameIsFor, &saveFolder, &choice.saveName);
  if (choice.saveName.empty()) choice.saveName = "csb";
  choice.title = "CSB map: " + choice.saveName;
  choice.barStyle = BAR_LOOKS[barLook >= 0 && barLook < BAR_LOOK_COUNT ? barLook : 0];
  stage = StageBusy;
  AutoMapExport_HtmlBeginForTheGame(choice);
  busyToDo = AutoMapExport_HtmlLevelsToDo();
  for (;;)
  {
    int next = AutoMapExport_HtmlNextLevel();
    busyDone = AutoMapExport_HtmlLevelsDone();
    char line[96];
    if (next >= 0) sprintf(line, "LEVEL %d (%d OF %d)", next, busyDone + 1, busyToDo);
    else sprintf(line, "PUTTING THE PAGE TOGETHER");
    busyLine = line;
    AutoMapWindow_PaintNow();
    if (StopIsPressed())
    {
      AutoMapExport_HtmlCancel();
      DropMapInput();
      stage = StageForm;
      AutoMapWindow_Repaint();
      return;
    }
    if (next < 0) break;
    AutoMapExport_HtmlStep();
  }
  std::string page = AutoMapExport_HtmlPage();
  DropMapInput();
  FILE *f = fopen(path.c_str(), "wb");
  bool written = f != NULL && fwrite(page.data(), 1, page.size(), f) == page.size();
  int error = errno;
  if (f != NULL && fclose(f) != 0) written = false;
  AutoMapWindow_Repaint();
  if (!written)
  {
    failure = Upper(path) + ": " + Upper(strerror(error));
    stage = StageFailed;
    return;
  }
  resultPath = path;
  char line[160];
  sprintf(line, "%s, %d %s, VISION UP TO %s, SPOILERS UP TO %s", AutoMapExportChoice_SizeText((long)page.size()).c_str(),
          (int)levels.size(), levels.size() == 1 ? "LEVEL" : "LEVELS", VISION_NAMES[maxVision], SPOILER_NAMES[maxSpoilers]);
  resultLine = line;
  stage = StageDone;
  if (openWhenDone) OpenUrlOf(path);
}

static void Browse(void)
{
  if (!BrowseWorks()) return;
  const char *patterns[1] = {"*.html"};
  const char *chosen = tinyfd_saveFileDialog("Export the map", Path().c_str(), 1, patterns, "Web page");
  DropMapInput();
  AutoMapWindow_Repaint();
  if (chosen == NULL || chosen[0] == '\0') return;
  std::string newFolder, newName;
  AutoMapExportChoice_SplitPath(chosen, &newFolder, &newName);
  if (newName.empty()) return;
  folder = AbsoluteFolder(newFolder);
  fileName = AutoMapExportChoice_WithHtmlExtension(newName);
  // The system's dialog has asked already whether to replace the file
  replaceAsked = FileExists(Path());
}

// ---------------- drawing ----------------

// Drawn only on the second pass; the first measures the height
static bool painting = false;

static void Text(int x, int y, const std::string &text, SDL_Color colour)
{
  if (painting) AutoMapPaint_Text(x, y, text, colour, PIXEL);
}

static int TextW(const std::string &text)
{
  return AutoMapPaint_TextWidth(text, PIXEL);
}

// The end of text that fits into width, with "..." in front when it was cut
static std::string Tail(const std::string &text, int width)
{
  if (TextW(text) <= width) return text;
  std::string tail = text;
  while (!tail.empty() && TextW("..." + tail) > width) tail.erase(0, 1);
  return "..." + tail;
}

// text broken into lines of at most width, at a / \ or space where it can
static std::vector<std::string> Wrapped(const std::string &text, int width)
{
  std::vector<std::string> lines;
  std::string rest = text;
  while (!rest.empty())
  {
    size_t fits = rest.size();
    while (fits > 1 && TextW(rest.substr(0, fits)) > width) fits--;
    if (fits < rest.size())
    {
      size_t at = rest.find_last_of("/\\ ", fits - 1);
      if (at != std::string::npos && at > 0) fits = at + 1;
    }
    lines.push_back(rest.substr(0, fits));
    rest.erase(0, fits);
  }
  return lines;
}

static void AddHit(const AutoMapRect &r, Action action, int arg)
{
  if (!painting) return;
  Hit hit = {r, action, arg};
  hits.push_back(hit);
}

// Buttons side by side in one well; on[i] lights button i, grey[i] (when given) dims it and
// takes no press. Returns the x after the well.
static int Buttons(int x, int y, const std::vector<std::string> &labels, const std::vector<bool> &on, Action action,
                   const std::vector<int> &args, const std::vector<bool> *grey = NULL)
{
  int h = AutoMapBar_ButtonHeight();
  int w = 0;
  for (size_t i = 0; i < labels.size(); i++) w += AutoMapBar_ButtonWidth(labels[i]) + (i ? GAP : 0);
  AutoMapRect well = {x, y, w, h};
  if (painting) AutoMapBar_DrawWell(well);
  int bx = x;
  for (size_t i = 0; i < labels.size(); i++)
  {
    AutoMapRect r = {bx, y, AutoMapBar_ButtonWidth(labels[i]), h};
    bool dim = grey != NULL && (*grey)[i];
    if (painting)
    {
      AutoMapBar_DrawButton(r, labels[i], on[i]);
      if (dim)
      {
        SDL_Color shade = {0x11, 0x11, 0x11, 0xff};
        AutoMapRect veil = {r.x, r.y, r.w, r.h};
        AutoMapPaint_Dither(veil, shade, 160, 1);
      }
    }
    if (!dim) AddHit(r, action, args[i]);
    bx += r.w + GAP;
  }
  return x + w;
}

static int OneButton(int x, int y, const std::string &label, bool on, Action action)
{
  return Buttons(x, y, std::vector<std::string>(1, label), std::vector<bool>(1, on), action, std::vector<int>(1, 0));
}

static int ChoiceRow(int x, int y, const char *const *names, int count, int chosen, Action action)
{
  std::vector<std::string> labels;
  std::vector<bool> on;
  std::vector<int> args;
  for (int i = 0; i < count; i++)
  {
    labels.push_back(names[i]);
    on.push_back(i == chosen);
    args.push_back(i);
  }
  return Buttons(x, y, labels, on, action, args);
}

static void Divider(int x, int y, int w)
{
  AutoMapRect line = {x, y, w, 1};
  if (painting) AutoMapPaint_Fill(line, DIVIDER);
}

static void CheckBox(int x, int y, const std::string &label, bool on, Action action)
{
  AutoMapRect frame = {x, y + 2, 10, 10};
  AutoMapRect tick = {x + 2, y + 4, 6, 6};
  if (painting)
  {
    AutoMapPaint_Frame(frame, GREY, 1);
    if (on) AutoMapPaint_Fill(tick, GOLD);
  }
  Text(x + 16, y + 3, label, on ? WHITE : GREY);
  AutoMapRect r = {x, y, 16 + TextW(label), 14};
  AddHit(r, action, 0);
}

static int FormHeight(int x, int top, int w)
{
  int labelW = TextW("SPOILERS UP TO") + 12;
  int cx = x + PAD + labelW;
  int y = top + PAD;
  int right = x + w - PAD;
  Text(x + PAD, y, "EXPORT THE MAP AS A WEB PAGE", GOLD);
  y += 18;

  // Levels
  int levelCount = AutoMap_LevelCount();
  std::vector<bool> visited = VisitedLevels();
  std::vector<int> chosen = ChosenLevels();
  static const char *const LEVEL_NAMES[4] = {"THIS LEVEL", "VISITED", "ALL", "PICK"};
  Text(x + PAD, y + 4, "LEVELS", GREY);
  ChoiceRow(cx, y, LEVEL_NAMES, 4, (int)levelsChoice, DoLevels);
  y += ROW;
  if (levelsChoice == AutoMapExportPick)
  {
    int chipW = TextW("L00") + 10, chipH = 16;
    int chipX = cx;
    for (int level = 0; level < levelCount; level++)
    {
      if (chipX + chipW > right)
      {
        chipX = cx;
        y += chipH + 3;
      }
      AutoMapRect chip = {chipX, y, chipW, chipH};
      bool on = picked.count(level) != 0;
      if (painting)
      {
        AutoMapPaint_Fill(chip, BLACK);
        AutoMapPaint_Frame(chip, on ? GOLD : DIVIDER, 1);
      }
      std::string name = "L" + std::to_string(level);
      SDL_Color colour = level == AutoMap_PartyLevel() ? RED : on ? WHITE : (visited[level] ? GREY : DIM);
      Text(chipX + (chipW - TextW(name)) / 2, y + 3, name, colour);
      AddHit(chip, DoToggleLevel, level);
      chipX += chipW + 3;
    }
    y += chipH + 6;
    std::vector<std::string> tools;
    tools.push_back("CLEAR");
    tools.push_back("ALL");
    tools.push_back("INVERT");
    int tx = cx;
    // One well, three actions: each button gets its own
    int h = AutoMapBar_ButtonHeight();
    int wellW = 0;
    for (size_t i = 0; i < tools.size(); i++) wellW += AutoMapBar_ButtonWidth(tools[i]) + (i ? GAP : 0);
    AutoMapRect well = {tx, y, wellW, h};
    if (painting) AutoMapBar_DrawWell(well);
    static const Action TOOL_ACTIONS[3] = {DoPickNone, DoPickAll, DoPickInvert};
    for (size_t i = 0; i < tools.size(); i++)
    {
      AutoMapRect r = {tx, y, AutoMapBar_ButtonWidth(tools[i]), h};
      if (painting) AutoMapBar_DrawButton(r, tools[i], false);
      AddHit(r, TOOL_ACTIONS[i], 0);
      tx += r.w + GAP;
    }
    OneButton(tx + 8, y, "VISITED", false, DoPickVisited);
    y += ROW;
  }
  char line[128];
  if (levelsChoice == AutoMapExportThisLevel) sprintf(line, "L%d, THE LEVEL ON SCREEN", AutoMap_ShownLevel());
  else sprintf(line, "%d OF %d LEVELS", (int)chosen.size(), levelCount);
  Text(cx, y, line, chosen.empty() ? WARN : GREY);
  bool unvisitedTaken = false;
  for (size_t i = 0; i < chosen.size(); i++) unvisitedTaken = unvisitedTaken || !visited[chosen[i]];
  if (unvisitedTaken && maxVision < 2) Text(cx + TextW(line) + 12, y, "UNVISITED ONES STAY DARK", GREY);
  y += 18;

  // Vision and spoilers
  Text(x + PAD, y + 4, "VISION UP TO", GREY);
  ChoiceRow(cx, y, VISION_NAMES, 3, maxVision, DoVision);
  y += ROW;
  if (maxVision == 2) Text(cx, y, "THE UNEXPLORED PARTS OF EACH LEVEL ARE IN THE FILE", WARN);
  else
  {
    std::string can = "THE READER CAN SWITCH";
    for (int v = 0; v <= maxVision; v++) can += std::string(v ? " / " : " ") + VISION_NAMES[v];
    Text(cx, y, can, GREY);
  }
  y += 18;
  Text(x + PAD, y + 4, "SPOILERS UP TO", GREY);
  ChoiceRow(cx, y, SPOILER_NAMES, 3, maxSpoilers, DoSpoilers);
  y += ROW;
  if (maxSpoilers == 2) Text(cx, y, "ALL SHOWS WHAT EVERY SWITCH DOES AND EVERY SECRET", WARN);
  else
  {
    std::string can = "THE READER CAN SWITCH";
    for (int s = 0; s <= maxSpoilers; s++) can += std::string(s ? " / " : " ") + SPOILER_NAMES[s];
    Text(cx, y, can, GREY);
  }
  y += 18;
  Text(x + PAD, y + 4, "BAR LOOK", GREY);
  ChoiceRow(cx, y, BAR_LOOK_NAMES, BAR_LOOK_COUNT, barLook, DoBarLook);
  y += ROW + 4;
  Divider(x + PAD, y, w - 2 * PAD);
  y += 8;

  // The file
  Text(x + PAD, y + 4, "FILE", GREY);
  int browseW = AutoMapBar_ButtonWidth("BROWSE...");
  AutoMapRect field = {cx, y, right - cx - browseW - 8, AutoMapBar_ButtonHeight()};
  if (painting)
  {
    AutoMapPaint_Fill(field, BLACK);
    AutoMapPaint_Frame(field, GOLD, 1);
    std::string shown = Tail(Upper(fileName), field.w - 14);
    AutoMapPaint_Text(field.x + 5, field.y + 4, shown, WHITE, PIXEL);
    // The caret: typing always goes to the end of the name
    AutoMapRect caret = {field.x + 5 + TextW(shown) + 2, field.y + 3, PIXEL, 12};
    AutoMapPaint_Fill(caret, GOLD);
  }
  std::vector<bool> greyBrowse(1, !BrowseWorks());
  Buttons(right - browseW, y, std::vector<std::string>(1, "BROWSE..."), std::vector<bool>(1, false), DoBrowse,
          std::vector<int>(1, 0), &greyBrowse);
  y += ROW;
  Text(cx, y, Tail("IN " + Upper(folder), right - cx), GREY);
  y += 18;
  CheckBox(cx, y, "OPEN IN THE BROWSER WHEN DONE", openWhenDone, DoOpenWhenDone);
  y += 22;

  // The foot
  int exportW = AutoMapBar_ButtonWidth(replaceAsked ? "REPLACE" : "EXPORT");
  int cancelW = AutoMapBar_ButtonWidth("CANCEL");
  int footY = y + 2;
  int exportX = right - exportW;
  int cancelX = exportX - 10 - cancelW;
  if (replaceAsked)
  {
    std::vector<std::string> warn = Wrapped(Upper(fileName) + " EXISTS. EXPORT AGAIN TO REPLACE IT.", cancelX - 10 - (x + PAD));
    for (size_t i = 0; i < warn.size(); i++) Text(x + PAD, footY + 4 + (int)i * 12, warn[i], WARN);
  }
  else
  {
    long bytes = AutoMapExportChoice_EstimatedBytes((int)chosen.size(), maxVision, maxSpoilers);
    Text(x + PAD, footY + 4, chosen.empty() ? "PICK AT LEAST ONE LEVEL" : "ABOUT " + AutoMapExportChoice_SizeText(bytes),
         chosen.empty() ? WARN : GREY);
  }
  OneButton(cancelX, footY, "CANCEL", false, DoCancel);
  bool canExport = !chosen.empty() && !fileName.empty();
  std::vector<bool> greyExport(1, !canExport);
  Buttons(exportX, footY, std::vector<std::string>(1, replaceAsked ? "REPLACE" : "EXPORT"), std::vector<bool>(1, true),
          DoExport, std::vector<int>(1, 0), &greyExport);
  y = footY + AutoMapBar_ButtonHeight() + PAD;
  return y - top;
}

static int BusyHeight(int x, int top, int w)
{
  int y = top + PAD;
  Text(x + PAD, y, "WRITING THE PAGE", GOLD);
  y += 18;
  Text(x + PAD, y, busyLine, WHITE);
  y += 16;
  AutoMapRect bar = {x + PAD, y, w - 2 * PAD, 10};
  if (painting)
  {
    AutoMapPaint_Fill(bar, BLACK);
    AutoMapPaint_Frame(bar, GREY, 1);
    int fill = busyToDo > 0 ? (bar.w - 4) * busyDone / busyToDo : 0;
    AutoMapRect done = {bar.x + 2, bar.y + 2, fill, bar.h - 4};
    AutoMapPaint_Fill(done, GOLD);
  }
  y += 18;
  Text(x + PAD, y + 4, "THE GAME WAITS UNTIL THE PAGE IS WRITTEN.", GREY);
  int stopW = AutoMapBar_ButtonWidth("STOP");
  AutoMapRect stop = {x + w - PAD - stopW, y, stopW, AutoMapBar_ButtonHeight()};
  OneButton(stop.x, stop.y, "STOP", false, DoStop);
  if (painting) stopButton = stop;
  y += AutoMapBar_ButtonHeight() + PAD;
  return y - top;
}

static int ResultHeight(int x, int top, int w)
{
  int y = top + PAD;
  bool done = stage == StageDone;
  Text(x + PAD, y, done ? "THE PAGE IS WRITTEN" : "THE PAGE COULD NOT BE WRITTEN", done ? GOLD : WARN);
  y += 18;
  std::vector<std::string> lines = Wrapped(done ? Upper(resultPath) : failure, w - 2 * PAD);
  for (size_t i = 0; i < lines.size(); i++)
  {
    Text(x + PAD, y, lines[i], WHITE);
    y += 12;
  }
  y += 4;
  if (done)
  {
    std::vector<std::string> info = Wrapped(resultLine + (openWhenDone ? ". OPENED IN THE BROWSER." : "."), w - 2 * PAD);
    for (size_t i = 0; i < info.size(); i++)
    {
      Text(x + PAD, y, info[i], GREY);
      y += 12;
    }
  }
  y += 8;
  int right = x + w - PAD;
  int closeW = AutoMapBar_ButtonWidth("CLOSE");
  int bx = right - closeW;
  OneButton(bx, y, "CLOSE", true, DoClose);
  if (done)
  {
    int openW = AutoMapBar_ButtonWidth("OPEN");
    bx -= 10 + openW;
    OneButton(bx, y, "OPEN", false, DoOpenPage);
    int folderW = AutoMapBar_ButtonWidth("SHOW FOLDER");
    bx -= 10 + folderW;
    OneButton(bx, y, "SHOW FOLDER", false, DoShowFolder);
  }
  y += AutoMapBar_ButtonHeight() + PAD;
  return y - top;
}

static int StageHeight(int x, int top, int w)
{
  switch (stage)
  {
  case StageForm: return FormHeight(x, top, w);
  case StageBusy: return BusyHeight(x, top, w);
  default: return ResultHeight(x, top, w);
  }
}

void AutoMapExportDialog_Draw(int width, int height)
{
  (void)height;
  if (stage == StageClosed) return;
  hits.clear();
  int w = std::min(width - 16, 640);
  int x = (width - w) / 2;
  int top = AutoMapBar_Height() + 8;
  painting = false;
  int h = StageHeight(x, top, w);
  AutoMapRect panel = {x, top, w, h};
  AutoMapPaint_Fill(panel, PANEL_FILL);
  AutoMapPaint_Frame(panel, PANEL_FRAME, 1);
  painting = true;
  StageHeight(x, top, w);
  painting = false;
}

// ---------------- input ----------------

bool AutoMapExportDialog_IsOpen(void)
{
  return stage != StageClosed;
}

void AutoMapExportDialog_Open(void)
{
  TakeNameFromTheSave();
  if (barLook < 0)
  {
    barLook = 0;
    for (int i = 0; i < BAR_LOOK_COUNT; i++)
      if (strcmp(AutoMapBar_StyleName(), BAR_LOOKS[i]) == 0) barLook = i;
  }
  if (!pickedIsSet)
  {
    picked = AutoMapExportChoice_PickVisited(VisitedLevels());
    pickedIsSet = true;
  }
  replaceAsked = false;
  stage = StageForm;
  startedTextInput = !SDL_IsTextInputActive();
  if (startedTextInput) SDL_StartTextInput();
  AutoMapWindow_Repaint();
}

void AutoMapExportDialog_Close(void)
{
  stage = StageClosed;
  hits.clear();
  if (startedTextInput) SDL_StopTextInput();
  startedTextInput = false;
  AutoMapWindow_Repaint();
}

static void Carry(Action action, int arg)
{
  int levelCount = AutoMap_LevelCount();
  if (action != DoExport && action != DoNothing) replaceAsked = false;
  switch (action)
  {
  case DoLevels: levelsChoice = (AutoMapExportLevels)arg; break;
  case DoVision: maxVision = arg; break;
  case DoSpoilers: maxSpoilers = arg; break;
  case DoBarLook: barLook = arg; break;
  case DoToggleLevel:
    if (picked.count(arg)) picked.erase(arg);
    else picked.insert(arg);
    break;
  case DoPickNone: picked.clear(); break;
  case DoPickAll: picked = AutoMapExportChoice_PickAll(levelCount); break;
  case DoPickInvert: picked = AutoMapExportChoice_PickInverted(picked, levelCount); break;
  case DoPickVisited: picked = AutoMapExportChoice_PickVisited(VisitedLevels()); break;
  case DoBrowse: Browse(); break;
  case DoOpenWhenDone: openWhenDone = !openWhenDone; break;
  case DoCancel:
  case DoClose: AutoMapExportDialog_Close(); break;
  case DoExport: WritePage(); break;
  case DoShowFolder:
  {
    std::string resultFolder, resultName;
    AutoMapExportChoice_SplitPath(resultPath, &resultFolder, &resultName);
    OpenUrlOf(resultFolder);
    break;
  }
  case DoOpenPage: OpenUrlOf(resultPath); break;
  default: break;
  }
  AutoMapWindow_Repaint();
}

void AutoMapExportDialog_OpenForPicture(const std::string &how)
{
  AutoMapExportDialog_Open();
  if (how == "pick")
  {
    levelsChoice = AutoMapExportPick;
    maxSpoilers = 1;
  }
  if (how == "busy")
  {
    stage = StageBusy;
    busyLine = "LEVEL 4 (5 OF 14)";
    busyDone = 4;
    busyToDo = 14;
  }
  if (how == "done")
  {
    stage = StageDone;
    resultPath = Path();
    resultLine = "1.0 MB, 14 LEVELS, VISION UP TO ALL, SPOILERS UP TO ALL";
  }
}

bool AutoMapExportDialog_HandlePress(int x, int y)
{
  if (stage == StageClosed) return false;
  // Copied first: an action may draw the dialog again, which rebuilds the list
  std::vector<Hit> now = hits;
  for (size_t i = 0; i < now.size(); i++)
  {
    const AutoMapRect &r = now[i].r;
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
    {
      Carry(now[i].action, now[i].arg);
      return true;
    }
  }
  return true;
}

void AutoMapExportDialog_HandleKey(SDL_Keycode sym, Uint16 mod)
{
  (void)mod;
  if (sym == SDLK_ESCAPE)
  {
    AutoMapExportDialog_Close();
    return;
  }
  if (stage != StageForm)
  {
    if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) AutoMapExportDialog_Close();
    return;
  }
  if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER)
  {
    WritePage();
    return;
  }
  if (sym == SDLK_BACKSPACE)
  {
    fileName = AutoMapExportChoice_Erased(fileName);
    replaceAsked = false;
  }
}

void AutoMapExportDialog_HandleText(const char *utf8)
{
  if (stage != StageForm) return;
  fileName = AutoMapExportChoice_Typed(fileName, utf8);
  replaceAsked = false;
}
