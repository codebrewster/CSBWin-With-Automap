// HeroStatsWindow.cpp : the Statistics window, a second OS window beside the map's that draws
// the hero statistics with Dear ImGui in a context of its own. The pages are HeroStatsDraw.cpp's.
#include "stdafx.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "HeroStatsCatalogue.h"
#include "HeroStatsDraw.h"
#include "HeroStatsExport.h"
#include "HeroStatsHooks.h"
#include "HeroStatsView.h"
#include "HeroStatsWindow.h"

#include "confile.h"

extern char pwd[]; // LinCSBUI.cpp

static const char *SETTINGS_SECTION = "settings";
static const int DEFAULT_WIDTH = 900;
static const int DEFAULT_HEIGHT = 640;
static const int SMALLEST_WIDTH = 600;
static const int SMALLEST_HEIGHT = 400;
// About 30 frames a second: the window has nothing that moves quickly, and the game loop
// calls Paint after every event
static const Uint32 SMALLEST_FRAME_GAP = 33;
static const float FONT_SIZE = 13.0f;
static const Uint8 BACKGROUND[3] = {0x1e, 0x1f, 0x24};
// stats_page keeps the topic since stats_layout 2; before, it kept the front page
static const int SETTINGS_LAYOUT = 2;

struct WindowPlacement
{
  int x;
  int y;
  int width;
  int height;
};

static SDL_Window *statsWindow = NULL;
static SDL_Renderer *statsRenderer = NULL;
static Uint32 statsWindowId = 0;
static ImGuiContext *statsContext = NULL;
static WindowPlacement placement = {SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DEFAULT_WIDTH, DEFAULT_HEIGHT};
static bool wasShownWhenSettingsWereRead = false;
static bool isShown = false;
static bool pointerIsOverWindow = false;
static int pointerStateOfTheGame = SDL_ENABLE;
static Uint32 lastFrame = 0;
static bool hasPainted = false;
// Who, the topic, the level, Per level and the folded sections. The topic and the folded
// sections are kept in csb.cfg; the rest only while the program runs.
static HeroStatsDrawState drawState = HeroStatsDraw_InitialState();
// stats_separator as csb.cfg has it ("comma", "dot", "space", "apostrophe", "none"), written back
// as it was read
static std::string separatorSetting = "comma";

// Makes the statistics context current for as long as it lives, and the one that was current
// before (the game's) again when it goes, on every path out of the function
struct StatsContextScope
{
  ImGuiContext *saved;
  StatsContextScope() : saved(ImGui::GetCurrentContext()) { ImGui::SetCurrentContext(statsContext); }
  ~StatsContextScope() { ImGui::SetCurrentContext(saved); }
};

// ---------------------------------------------------------------------------------------------
// Settings

void HeroStatsWindow_ReadSettings(void)
{
  wasShownWhenSettingsWereRead = csb_get_config_int(SETTINGS_SECTION, "stats_window", 0) != 0;
  placement.x = csb_get_config_int(SETTINGS_SECTION, "stats_x", SDL_WINDOWPOS_UNDEFINED);
  placement.y = csb_get_config_int(SETTINGS_SECTION, "stats_y", SDL_WINDOWPOS_UNDEFINED);
  placement.width = std::max(SMALLEST_WIDTH, csb_get_config_int(SETTINGS_SECTION, "stats_width", DEFAULT_WIDTH));
  placement.height = std::max(SMALLEST_HEIGHT, csb_get_config_int(SETTINGS_SECTION, "stats_height", DEFAULT_HEIGHT));
  int page = csb_get_config_int(SETTINGS_SECTION, "stats_page", HeroTopicOverview);
  if (csb_get_config_int(SETTINGS_SECTION, "stats_layout", 0) < SETTINGS_LAYOUT)
    drawState.topic = HeroStatsCatalogue_TopicOfOldPage(page);
  else
    drawState.topic = page >= 0 && page < HERO_TOPIC_COUNT ? page : HeroTopicOverview;
  drawState.closedSections = csb_get_config_int(SETTINGS_SECTION, "stats_closed", 0);
  char defaultSeparator[] = "comma";
  separatorSetting = csb_get_config_string(SETTINGS_SECTION, "stats_separator", defaultSeparator);
  HeroStatsView_SetThousandsSeparator(HeroStatsView_SeparatorOfSetting(separatorSetting));
}

static void RememberPlacement(void)
{
  if (statsWindow == NULL || !isShown) return;
  SDL_GetWindowPosition(statsWindow, &placement.x, &placement.y);
  SDL_GetWindowSize(statsWindow, &placement.width, &placement.height);
}

void HeroStatsWindow_WriteSettings(void)
{
  RememberPlacement();
  csb_set_config_int(SETTINGS_SECTION, "stats_window", isShown ? 1 : 0);
  csb_set_config_int(SETTINGS_SECTION, "stats_x", placement.x);
  csb_set_config_int(SETTINGS_SECTION, "stats_y", placement.y);
  csb_set_config_int(SETTINGS_SECTION, "stats_width", placement.width);
  csb_set_config_int(SETTINGS_SECTION, "stats_height", placement.height);
  csb_set_config_int(SETTINGS_SECTION, "stats_page", drawState.topic);
  csb_set_config_int(SETTINGS_SECTION, "stats_layout", SETTINGS_LAYOUT);
  csb_set_config_int(SETTINGS_SECTION, "stats_closed", drawState.closedSections);
  std::vector<char> separator(separatorSetting.begin(), separatorSetting.end());
  separator.push_back(0);
  csb_set_config_string(SETTINGS_SECTION, "stats_separator", separator.data());
}

void HeroStatsWindow_RestoreVisibility(void)
{
  if (wasShownWhenSettingsWereRead) HeroStatsWindow_Show();
}

// ---------------------------------------------------------------------------------------------
// The ImGui context


// Vera Sans Mono beside the game's Vera.ttf when it is there; otherwise ImGui's own font,
// which is monospaced as well
static void LoadFont(void)
{
  std::string path = std::string(pwd) + "/fonts/VeraMono.ttf";
  FILE *file = fopen(path.c_str(), "rb");
  if (file == NULL) return;
  fclose(file);
  ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), FONT_SIZE);
}

// With the window and its renderer made: a context of its own, so that the game's menu bar and
// this window keep apart what the pointer, the keys and the fonts do. The backends keep their
// data per context. What they do with SDL's global state is held back here:
// - the system pointer: the game hides it over its own window, and this context would show it
//   again on every frame, so it leaves the pointer alone (NoMouseCursorChange) and the window
//   takes and gives back the pointer as the map does;
// - mouse capture: each context would capture or release it on every frame by its own buttons,
//   undoing the other's, so this one does not capture;
// - game controllers: only the game's context opens them;
// - imgui.ini: the game's context owns that file, and this window's layout is fixed anyway.
// Clipboard, IME and text input (SDL_StartTextInput is only called on Android) need nothing.
static bool CreateStatsContext(void)
{
  ImGuiContext *saved = ImGui::GetCurrentContext();
  statsContext = ImGui::CreateContext();
  ImGui::SetCurrentContext(statsContext);
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = NULL;
  io.LogFilename = NULL;
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
  HeroStatsDraw_ApplyStyle();
  bool ok = ImGui_ImplSDL2_InitForSDLRenderer(statsWindow, statsRenderer);
  if (ok)
  {
    ImGui_ImplSDL2_SetMouseCaptureMode(ImGui_ImplSDL2_MouseCaptureMode_Disabled);
    ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode_Manual, NULL, 0);
    ok = ImGui_ImplSDLRenderer2_Init(statsRenderer);
    if (!ok) ImGui_ImplSDL2_Shutdown();
  }
  if (ok) LoadFont();
  else
  {
    ImGui::DestroyContext(statsContext);
    statsContext = NULL;
  }
  ImGui::SetCurrentContext(saved);
  return ok;
}

// ---------------------------------------------------------------------------------------------
// Showing and hiding

static void ReportThatTheWindowCannotBeCreated(void)
{
  std::string message = std::string("The statistics window cannot be opened:\n") + SDL_GetError();
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Statistics", message.c_str(), NULL);
}

static void DestroyStatsWindow(void)
{
  if (statsRenderer != NULL) SDL_DestroyRenderer(statsRenderer);
  if (statsWindow != NULL) SDL_DestroyWindow(statsWindow);
  statsRenderer = NULL;
  statsWindow = NULL;
  statsWindowId = 0;
}

static bool CreateStatsWindow(void)
{
  Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
  statsWindow = SDL_CreateWindow("Statistics", placement.x, placement.y, placement.width, placement.height, flags);
  if (statsWindow != NULL) statsRenderer = SDL_CreateRenderer(statsWindow, -1, 0);
  if (statsRenderer == NULL || !CreateStatsContext())
  {
    ReportThatTheWindowCannotBeCreated();
    DestroyStatsWindow();
    return false;
  }
  statsWindowId = SDL_GetWindowID(statsWindow);
  SDL_SetWindowMinimumSize(statsWindow, SMALLEST_WIDTH, SMALLEST_HEIGHT);
  return true;
}

bool HeroStatsWindow_IsShown(void)
{
  return isShown;
}

void HeroStatsWindow_Show(void)
{
  if (isShown) return;
  if (statsWindow == NULL && !CreateStatsWindow()) return;
  SDL_Window *windowWithKeyboard = SDL_GetKeyboardFocus();
  SDL_ShowWindow(statsWindow);
  // The keys keep steering the game
  if (windowWithKeyboard != NULL) SDL_RaiseWindow(windowWithKeyboard);
  isShown = true;
  hasPainted = false;
  HeroStatsDraw_Forget();
}

void HeroStatsWindow_Forget(void)
{
  HeroStatsDraw_Forget();
}

void HeroStatsWindow_ShowAfterWin(void)
{
  drawState.who = HERO_STATS_PARTY;
  drawState.level = HERO_STATS_WHOLE_RUN;
  drawState.perLevel = false;
  HeroStatsDraw_Forget();
  HeroStatsWindow_Show();
}

static void GiveThePointerBackToTheGame(void)
{
  if (!pointerIsOverWindow) return;
  pointerIsOverWindow = false;
  SDL_ShowCursor(pointerStateOfTheGame);
}

static void TakeThePointerFromTheGame(void)
{
  if (pointerIsOverWindow) return;
  pointerIsOverWindow = true;
  pointerStateOfTheGame = SDL_ShowCursor(SDL_QUERY);
  SDL_ShowCursor(SDL_ENABLE);
}

void HeroStatsWindow_Hide(void)
{
  if (!isShown) return;
  RememberPlacement();
  GiveThePointerBackToTheGame();
  {
    // Shown again, the window must not find a button still held or the pointer still on it
    StatsContextScope scope;
    ImGuiIO &io = ImGui::GetIO();
    io.AddFocusEvent(false);
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
  }
  SDL_HideWindow(statsWindow);
  isShown = false;
}

bool HeroStatsWindow_PointerIsOver(void)
{
  return pointerIsOverWindow;
}

// ---------------------------------------------------------------------------------------------
// Events

// The window an event was sent to, 0 for events that belong to no window
static Uint32 WindowIdOfEvent(const SDL_Event &event)
{
  switch (event.type)
  {
  case SDL_MOUSEMOTION: return event.motion.windowID;
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP: return event.button.windowID;
  case SDL_MOUSEWHEEL: return event.wheel.windowID;
  case SDL_KEYDOWN:
  case SDL_KEYUP: return event.key.windowID;
  case SDL_TEXTINPUT: return event.text.windowID;
  case SDL_TEXTEDITING: return event.edit.windowID;
  case SDL_WINDOWEVENT: return event.window.windowID;
  default: return 0;
  }
}

bool HeroStatsWindow_HandleEvent(const SDL_Event &event)
{
  if (statsWindow == NULL || statsContext == NULL) return false;
  // No keyboard input here: the keys go on steering the game, as when the Map window has focus
  if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTINPUT ||
      event.type == SDL_TEXTEDITING)
    return false;
  if (WindowIdOfEvent(event) != statsWindowId) return false;
  if (event.type == SDL_WINDOWEVENT)
  {
    switch (event.window.event)
    {
    case SDL_WINDOWEVENT_CLOSE: HeroStatsWindow_Hide(); return true;
    case SDL_WINDOWEVENT_ENTER: TakeThePointerFromTheGame(); break;
    case SDL_WINDOWEVENT_LEAVE: GiveThePointerBackToTheGame(); break;
    default: break;
    }
  }
  StatsContextScope scope;
  ImGui_ImplSDL2_ProcessEvent(&event);
  return true;
}

// ---------------------------------------------------------------------------------------------
// Drawing

static void ExportStatistics(void);

static void DrawWindowContents(void)
{
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (ImGui::Begin("##statistics", NULL, flags))
  {
    HeroStats_RefreshMapCounts(false);
    const HeroStats &stats = HeroStats_Store();
    HeroStatsPicker picker = HeroStatsView_Picker(stats, drawState.level);
    // A level of the game before is not in this game's list
    if (drawState.level != HERO_STATS_WHOLE_RUN &&
        std::find(picker.levels.begin(), picker.levels.end(), drawState.level) == picker.levels.end())
    {
      drawState.level = HERO_STATS_WHOLE_RUN;
      picker = HeroStatsView_Picker(stats, drawState.level);
    }
    std::vector<HeroStatsColumn> columns = HeroStatsView_Columns(stats, HeroStats_PartyNow());
    // A champion of a game before is not in this game's list
    drawState.who = HeroStatsView_ValidWho(columns, drawState.who);
    HeroStatsDraw_Frame(&drawState, stats, columns, picker, ExportStatistics);
  }
  ImGui::End();
}

// ---------------------------------------------------------------------------------------------
// Export

// ---------------------------------------------------------------------------------------------
// Export

// file:///home/... or, for a Windows path, file:///C:/...
static std::string FileUrl(std::string path)
{
  for (size_t i = 0; i < path.size(); i++)
    if (path[i] == '\\') path[i] = '/';
  return path.compare(0, 1, "/") == 0 ? "file://" + path : "file:///" + path;
}

// statistics-2026-10-31-153000.html in the game folder, beside csb.cfg and the saved games
static void ExportStatistics(void)
{
  time_t now = time(NULL);
  char stamp[32] = "";
  char date[32] = "";
  const struct tm *local = localtime(&now);
  if (local != NULL)
  {
    strftime(stamp, sizeof stamp, "%Y-%m-%d-%H%M%S", local);
    strftime(date, sizeof date, "%Y-%m-%d %H:%M", local);
  }
  std::string path = std::string(pwd) + "/statistics-" + stamp + ".html";
  HeroStats_RefreshMapCounts(true);
  const HeroStats &stats = HeroStats_Store();
  HeroStatsExportInfo info = {HeroStats_DungeonInPlay(), date};
  std::string html = HeroStatsExport_Html(stats, HeroStatsView_Columns(stats, HeroStats_PartyNow()),
                                          HeroStats_NamesOfTheGame(), info);
  FILE *file = fopen(path.c_str(), "wb");
  bool written = file != NULL && fwrite(html.data(), 1, html.size(), file) == html.size();
  if (file != NULL && fclose(file) != 0) written = false;
  if (!written)
  {
    std::string message = "The statistics could not be written to\n" + path;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Statistics", message.c_str(), statsWindow);
    return;
  }
  std::string message = "The statistics were written to\n" + path +
                        "\n\nThe file holds every page and level and opens in any web browser.";
  const SDL_MessageBoxButtonData buttons[2] = {
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "OK"},
      {0, 1, "Open"}};
  SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION, statsWindow, "Statistics", message.c_str(), 2, buttons, NULL};
  int pressed = 0;
  if (SDL_ShowMessageBox(&box, &pressed) == 0 && pressed == 1) SDL_OpenURL(FileUrl(path).c_str());
}

void HeroStatsWindow_Paint(void)
{
  if (!isShown) return;
  Uint32 now = SDL_GetTicks();
  if (hasPainted && now - lastFrame < SMALLEST_FRAME_GAP) return;
  lastFrame = now;
  hasPainted = true;
  StatsContextScope scope;
  // Without capture a release outside the window is never delivered: let go of what SDL sees up
  {
    ImGuiIO &mouseIo = ImGui::GetIO();
    Uint32 down = SDL_GetGlobalMouseState(NULL, NULL);
    const Uint32 masks[3] = {SDL_BUTTON_LMASK, SDL_BUTTON_RMASK, SDL_BUTTON_MMASK};
    for (int b = 0; b < 3; b++)
      if (mouseIo.MouseDown[b] && !(down & masks[b])) mouseIo.AddMouseButtonEvent(b, false);
  }
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  DrawWindowContents();
  ImGui::Render();
  ImGuiIO &io = ImGui::GetIO();
  SDL_RenderSetScale(statsRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
  SDL_SetRenderDrawColor(statsRenderer, BACKGROUND[0], BACKGROUND[1], BACKGROUND[2], 0xff);
  SDL_RenderClear(statsRenderer);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), statsRenderer);
  SDL_RenderPresent(statsRenderer);
}
