// HeroStatsWindow.cpp : the Statistics window, a second OS window beside the map's that draws
// the hero statistics with Dear ImGui in a context of its own.
#include "stdafx.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
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

// The colours of the mockup
static const ImU32 BACKGROUND_RGB = 0x1e1f24;
static const ImU32 TITLE_RGB = 0x294a7a;
static const ImU32 TITLE_TEXT_RGB = 0xffffff;
static const ImU32 TEXT_RGB = 0xd8d8d8;
static const ImU32 BUTTON_RGB = 0x2c3e5c;
static const ImU32 BUTTON_HOVERED_RGB = 0x36507a;
static const ImU32 BUTTON_ON_RGB = 0x4272b8;
static const ImU32 BORDER_RGB = 0x3a3d46;
static const ImU32 BAR_LINE_RGB = 0x33363e;
static const ImU32 FRAME_RGB = 0x2a2c33;

// The front pages, as stats_page keeps them
enum StatsPage
{
  StatsPageTable,
  StatsPageBars,
  StatsPageByLevel,
  StatsPageMonsters,
  STATS_PAGE_COUNT
};
static const char *const PAGE_LABELS[STATS_PAGE_COUNT] = {"Table", "Bars", "By level", "Monsters"};

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
static int shownPage = StatsPageTable;
static bool pointerIsOverWindow = false;
static int pointerStateOfTheGame = SDL_ENABLE;
static Uint32 lastFrame = 0;
static bool hasPainted = false;

static ImVec4 Colour(ImU32 rgb, float alpha = 1.0f)
{
  return ImVec4(((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f, (rgb & 0xff) / 255.0f, alpha);
}

static ImU32 PackedColour(ImU32 rgb)
{
  return ImGui::GetColorU32(Colour(rgb));
}

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
  shownPage = csb_get_config_int(SETTINGS_SECTION, "stats_page", StatsPageTable);
  if (shownPage < 0 || shownPage >= STATS_PAGE_COUNT) shownPage = StatsPageTable;
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
  csb_set_config_int(SETTINGS_SECTION, "stats_page", shownPage);
}

void HeroStatsWindow_RestoreVisibility(void)
{
  if (wasShownWhenSettingsWereRead) HeroStatsWindow_Show();
}

// ---------------------------------------------------------------------------------------------
// The ImGui context

static void ApplyStyle(void)
{
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(0, 0);
  style.WindowBorderSize = 0;
  style.WindowRounding = 0;
  style.FramePadding = ImVec2(6, 2);
  style.FrameRounding = 3;
  style.ItemSpacing = ImVec2(4, 4);
  style.Colors[ImGuiCol_WindowBg] = Colour(BACKGROUND_RGB);
  style.Colors[ImGuiCol_ChildBg] = Colour(BACKGROUND_RGB);
  style.Colors[ImGuiCol_PopupBg] = Colour(FRAME_RGB);
  style.Colors[ImGuiCol_Text] = Colour(TEXT_RGB);
  style.Colors[ImGuiCol_Border] = Colour(BORDER_RGB);
  style.Colors[ImGuiCol_Separator] = Colour(BORDER_RGB);
  style.Colors[ImGuiCol_Button] = Colour(BUTTON_RGB);
  style.Colors[ImGuiCol_ButtonHovered] = Colour(BUTTON_HOVERED_RGB);
  style.Colors[ImGuiCol_ButtonActive] = Colour(BUTTON_ON_RGB);
  style.Colors[ImGuiCol_FrameBg] = Colour(FRAME_RGB);
  style.Colors[ImGuiCol_FrameBgHovered] = Colour(BUTTON_HOVERED_RGB);
  style.Colors[ImGuiCol_FrameBgActive] = Colour(BUTTON_ON_RGB);
  style.Colors[ImGuiCol_Header] = Colour(BUTTON_RGB);
  style.Colors[ImGuiCol_HeaderHovered] = Colour(BUTTON_HOVERED_RGB);
  style.Colors[ImGuiCol_HeaderActive] = Colour(BUTTON_ON_RGB);
}

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
  ApplyStyle();
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

// The strip at the top, as the mockup has it: the window's name on blue
static void DrawTitle(void)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float width = ImGui::GetContentRegionAvail().x;
  float height = ImGui::GetTextLineHeight() + 6;
  drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), PackedColour(TITLE_RGB));
  drawList->AddText(ImVec2(origin.x + 8, origin.y + 3), PackedColour(TITLE_TEXT_RGB), "Statistics");
  ImGui::Dummy(ImVec2(width, height));
}

static bool PageButton(const char *label, bool isOn)
{
  if (isOn)
  {
    ImGui::PushStyleColor(ImGuiCol_Button, Colour(BUTTON_ON_RGB));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colour(BUTTON_ON_RGB));
    ImGui::PushStyleColor(ImGuiCol_Text, Colour(TITLE_TEXT_RGB));
  }
  bool clicked = ImGui::Button(label);
  if (isOn) ImGui::PopStyleColor(3);
  return clicked;
}

// The front-page choice on the left; the level picker on the right comes with the pages
static void DrawTopBar(void)
{
  const float GUTTER = 8;
  const float BAR_PADDING = 6;
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + BAR_PADDING));
  for (int page = 0; page < STATS_PAGE_COUNT; page++)
  {
    if (page > 0) ImGui::SameLine();
    if (PageButton(PAGE_LABELS[page], page == shownPage)) shownPage = page;
  }
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + BAR_PADDING - ImGui::GetStyle().ItemSpacing.y);
  ImVec2 lineStart = ImGui::GetCursorScreenPos();
  lineStart.x = ImGui::GetWindowPos().x;
  float width = ImGui::GetWindowWidth();
  ImGui::GetWindowDrawList()->AddLine(lineStart, ImVec2(lineStart.x + width, lineStart.y), PackedColour(BAR_LINE_RGB));
  ImGui::Dummy(ImVec2(width, 1));
}

static void DrawWindowContents(void)
{
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (ImGui::Begin("##statistics", NULL, flags))
  {
    DrawTitle();
    DrawTopBar();
  }
  ImGui::End();
}

void HeroStatsWindow_Paint(void)
{
  if (!isShown) return;
  Uint32 now = SDL_GetTicks();
  if (hasPainted && now - lastFrame < SMALLEST_FRAME_GAP) return;
  lastFrame = now;
  hasPainted = true;
  StatsContextScope scope;
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  DrawWindowContents();
  ImGui::Render();
  ImGuiIO &io = ImGui::GetIO();
  SDL_RenderSetScale(statsRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
  ImVec4 background = Colour(BACKGROUND_RGB);
  SDL_SetRenderDrawColor(statsRenderer, (Uint8)(background.x * 255), (Uint8)(background.y * 255),
                         (Uint8)(background.z * 255), 0xff);
  SDL_RenderClear(statsRenderer);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), statsRenderer);
  SDL_RenderPresent(statsRenderer);
}
