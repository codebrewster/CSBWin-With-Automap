// HeroStatsWindow.cpp : the Statistics window, a second OS window beside the map's that draws
// the hero statistics with Dear ImGui in a context of its own.
#include "stdafx.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "HeroStatsExport.h"
#include "HeroStatsHooks.h"
#include "HeroStatsPages.h"
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
static const ImU32 SECTION_RGB = 0xe8c66a;
static const ImU32 MELEE_RGB = 0xc9764a;
static const ImU32 RANGED_RGB = 0x5a9bd8;
static const ImU32 MAGIC_RGB = 0xa77ad8;
static const ImU32 BAD_RGB = 0xe07a6a;
static const ImU32 KILLS_RGB = 0xc9a24a;
static const ImU32 HEALED_RGB = 0x4ab0a0;
static const ImU32 GIVEN_RGB = 0xe0c060;
static const ImU32 GENERATED_RGB = 0x4a9a6a;
static const ImU32 TAKEN_RGB = 0x888888;
static const ImU32 DOOR_RGB = 0xb0a070;
static const ImU32 PIT_RGB = 0x7a7a7a;
static const ImU32 OTHER_RGB = 0x555555;
static const ImU32 HERO_RGB[4] = {0xc9764a, 0x5aa86a, 0x5a9bd8, 0xa77ad8};

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
// The numbers of the pages are kept this long: every query scans the whole store, and the
// numbers move only as the game plays
static const Uint32 CACHE_LIFETIME = 500;
static bool pageDataCached = false;
static bool byLevelCached = false;
static bool heroPageCached = false;
// The level the pages show: HERO_STATS_WHOLE_RUN or a level index
static int shownLevel = HERO_STATS_WHOLE_RUN;
// The hero page a click on a name asked for; HERO_STATS_PARTY for the party
static bool heroPageOpen = false;
// What the By level page adds up (HeroStatsMetric); kept while the program runs, not in csb.cfg
static int shownMetric = HeroMetricDealt;
// The open sections of the hero pages: 1 Attacks, 2 Spells, 4 Damage taken, 8 Healing, 16 Hardship
static const int DEFAULT_OPEN_SECTIONS = 1 | 16;
static int openSections = DEFAULT_OPEN_SECTIONS;
// stats_separator as csb.cfg has it ("comma", "dot", "space", "apostrophe", "none"), written back
// as it was read
static std::string separatorSetting = "comma";
static HeroStatsChampion heroPageWho = HERO_STATS_PARTY;

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
  openSections = csb_get_config_int(SETTINGS_SECTION, "stats_open", DEFAULT_OPEN_SECTIONS) & 31;
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
  csb_set_config_int(SETTINGS_SECTION, "stats_page", shownPage);
  csb_set_config_int(SETTINGS_SECTION, "stats_open", openSections);
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
  style.Colors[ImGuiCol_TableHeaderBg] = Colour(BACKGROUND_RGB);
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
  pageDataCached = false;
  byLevelCached = false;
  heroPageCached = false;
}

void HeroStatsWindow_Forget(void)
{
  pageDataCached = false;
  byLevelCached = false;
  heroPageCached = false;
}

void HeroStatsWindow_ShowAfterWin(void)
{
  shownLevel = HERO_STATS_WHOLE_RUN;
  heroPageOpen = false;
  pageDataCached = false;
  byLevelCached = false;
  heroPageCached = false;
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

static const float GUTTER = 8;

static ImVec4 DimmedColour(void)
{
  return ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
}

static float ButtonWidth(const char *label)
{
  return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
}

static void ExportStatistics(void);

// The strip at the top, as the mockup has it: the window's name on blue, and the export
static void DrawTitle(void)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float width = ImGui::GetContentRegionAvail().x;
  float height = ImGui::GetTextLineHeight() + 6;
  drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), PackedColour(TITLE_RGB));
  drawList->AddText(ImVec2(origin.x + 8, origin.y + 3), PackedColour(TITLE_TEXT_RGB), "Statistics");
  const char *exportLabel = "Export as HTML...";
  ImGui::SetCursorScreenPos(ImVec2(origin.x + width - ButtonWidth(exportLabel) - 4, origin.y + 1));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 1));
  if (ImGui::Button(exportLabel)) ExportStatistics();
  ImGui::PopStyleVar();
  ImGui::SetCursorScreenPos(origin);
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

// ---------------------------------------------------------------------------------------------
// The numbers of the visible page

// Who the columns are, as text, to see that they changed
static std::string ColumnsKey(const std::vector<HeroStatsColumn> &columns)
{
  std::string key;
  for (size_t i = 0; i < columns.size(); i++)
    key += std::to_string(columns[i].id) + (columns[i].hasLeft ? "L," : ",");
  return key;
}

// The page data for level, rebuilt when the level or the columns change and at most twice a second
static const HeroStatsOverview &CachedOverview(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level)
{
  static HeroStatsOverview data;
  static int cachedLevel;
  static std::string cachedColumns;
  static Uint32 cachedAt;
  std::string key = ColumnsKey(columns);
  Uint32 now = SDL_GetTicks();
  if (!pageDataCached || level != cachedLevel || key != cachedColumns || now - cachedAt >= CACHE_LIFETIME)
  {
    data = HeroStatsPages_Overview(stats, columns, level);
    cachedLevel = level;
    cachedColumns = key;
    cachedAt = now;
    pageDataCached = true;
  }
  return data;
}

// ---------------------------------------------------------------------------------------------
// Sortable tables

// The sort the player clicked in the table being drawn: column -1 when there is none
struct TableSort
{
  int column;
  bool descending;
};

static TableSort CurrentSort(void)
{
  TableSort sort = {-1, false};
  ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs();
  if (specs != NULL && specs->SpecsCount > 0)
  {
    sort.column = specs->Specs[0].ColumnIndex;
    sort.descending = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
    specs->SpecsDirty = false;
  }
  return sort;
}

static int Compare(double a, double b)
{
  return a < b ? -1 : a > b ? 1 : 0;
}

// compare(a, b, column) < 0 when a comes first in ascending order; ties keep their order
template <typename Row, typename Comparison>
static void SortRows(std::vector<Row> *rows, const TableSort &sort, Comparison compare)
{
  if (sort.column < 0) return;
  std::stable_sort(rows->begin(), rows->end(), [&](const Row &a, const Row &b) {
    int c = compare(a, b, sort.column);
    return sort.descending ? c > 0 : c < 0;
  });
}

// A column whose header sorts the table; numbers sort biggest first on the first click
static void SortColumn(const std::string &label, float width, bool isNumber, bool sortsFirst = false)
{
  ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_WidthFixed;
  if (isNumber) flags |= ImGuiTableColumnFlags_PreferSortDescending;
  if (sortsFirst) flags |= ImGuiTableColumnFlags_DefaultSort;
  ImGui::TableSetupColumn(label.c_str(), flags, width);
}

// The header row; dimmed[i] greys the header of column i (a champion who left)
static void SortHeaders(const std::vector<bool> &dimmed)
{
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  int count = ImGui::TableGetColumnCount();
  for (int i = 0; i < count; i++)
  {
    if (!ImGui::TableSetColumnIndex(i)) continue;
    bool dim = i < (int)dimmed.size() && dimmed[i];
    if (dim) ImGui::PushStyleColor(ImGuiCol_Text, DimmedColour());
    ImGui::TableHeader(ImGui::TableGetColumnName(i));
    if (dim) ImGui::PopStyleColor();
  }
}

// ---------------------------------------------------------------------------------------------
// The top bar

// The automap shows the level index as it is; it has no names of its own for levels
static std::string LevelLabel(int level)
{
  return "Lv " + std::to_string(level);
}

static float PickerWidth(const HeroStatsPicker &picker)
{
  float spacing = ImGui::GetStyle().ItemSpacing.x;
  float width = ButtonWidth("Whole run");
  if (picker.stepper) return width + spacing * 3 + ImGui::GetFrameHeight() * 2 + 170;
  for (size_t i = 0; i < picker.levels.size(); i++)
    width += spacing + ButtonWidth(std::to_string(picker.levels[i]).c_str());
  return width;
}

static void DrawPicker(const HeroStatsPicker &picker)
{
  if (PageButton("Whole run", shownLevel == HERO_STATS_WHOLE_RUN)) shownLevel = HERO_STATS_WHOLE_RUN;
  if (!picker.stepper)
  {
    for (size_t i = 0; i < picker.levels.size(); i++)
    {
      ImGui::SameLine();
      ImGui::PushID((int)i);
      if (PageButton(std::to_string(picker.levels[i]).c_str(), shownLevel == picker.levels[i]))
        shownLevel = picker.levels[i];
      ImGui::PopID();
    }
    return;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(picker.previous == -2);
  if (ImGui::ArrowButton("##previous", ImGuiDir_Left)) shownLevel = picker.previous;
  ImGui::EndDisabled();
  ImGui::SameLine();
  std::string current = shownLevel == HERO_STATS_WHOLE_RUN
                            ? "All " + std::to_string(picker.levels.size()) + " levels"
                            : LevelLabel(shownLevel);
  ImGui::SetNextItemWidth(170);
  if (ImGui::BeginCombo("##level", current.c_str()))
  {
    if (ImGui::Selectable("Whole run", shownLevel == HERO_STATS_WHOLE_RUN)) shownLevel = HERO_STATS_WHOLE_RUN;
    for (size_t i = 0; i < picker.levels.size(); i++)
      if (ImGui::Selectable(LevelLabel(picker.levels[i]).c_str(), shownLevel == picker.levels[i]))
        shownLevel = picker.levels[i];
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(picker.next == -2);
  if (ImGui::ArrowButton("##next", ImGuiDir_Right)) shownLevel = picker.next;
  ImGui::EndDisabled();
}

// The front-page choice on the left (on a hero page: back to the overview, the champions and
// the party), the level picker on the right
static void DrawTopBar(const HeroStatsPicker &picker, bool showPicker, const std::vector<HeroStatsColumn> &columns)
{
  const float BAR_PADDING = 6;
  float spacing = ImGui::GetStyle().ItemSpacing.x;
  float pagesEnd = GUTTER;
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + BAR_PADDING));
  if (!heroPageOpen)
  {
    for (int page = 0; page < STATS_PAGE_COUNT; page++)
    {
      if (page > 0) ImGui::SameLine();
      if (PageButton(PAGE_LABELS[page], page == shownPage)) shownPage = page;
      pagesEnd += ButtonWidth(PAGE_LABELS[page]) + (page > 0 ? spacing : 0);
    }
  }
  else
  {
    const char *back = "< Overview";
    if (PageButton(back, false)) heroPageOpen = false;
    pagesEnd += ButtonWidth(back);
    for (size_t i = 0; i <= columns.size(); i++)
    {
      bool isParty = i == columns.size();
      HeroStatsChampion id = isParty ? HERO_STATS_PARTY : columns[i].id;
      const char *label = isParty ? "Party" : columns[i].name.c_str();
      bool left = !isParty && columns[i].hasLeft;
      ImGui::SameLine();
      ImGui::PushID((int)i);
      if (left) ImGui::PushStyleColor(ImGuiCol_Text, DimmedColour());
      if (PageButton(label, heroPageWho == id)) heroPageWho = id;
      if (left) ImGui::PopStyleColor();
      ImGui::PopID();
      pagesEnd += spacing + ButtonWidth(label);
    }
  }
  if (showPicker)
  {
    float left = ImGui::GetWindowWidth() - GUTTER - PickerWidth(picker);
    if (left > pagesEnd + 2 * spacing)
    {
      ImGui::SameLine();
      ImGui::SetCursorPosX(left);
    }
    else ImGui::SetCursorPosX(GUTTER);  // too narrow to sit beside the page buttons: its own line
    DrawPicker(picker);
  }
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + BAR_PADDING - ImGui::GetStyle().ItemSpacing.y);
  ImVec2 lineStart = ImGui::GetCursorScreenPos();
  lineStart.x = ImGui::GetWindowPos().x;
  float width = ImGui::GetWindowWidth();
  ImGui::GetWindowDrawList()->AddLine(lineStart, ImVec2(lineStart.x + width, lineStart.y), PackedColour(BAR_LINE_RGB));
  ImGui::Dummy(ImVec2(width, 1));
}

// ---------------------------------------------------------------------------------------------
// Table

static void RightText(const std::string &text, const ImVec4 &colour)
{
  float room = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text.c_str()).x;
  if (room > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room);
  ImGui::TextColored(colour, "%s", text.c_str());
}

static void OpenHeroPage(HeroStatsChampion who)
{
  heroPageOpen = true;
  heroPageWho = who;
}

// A name that opens the hero's page when clicked
static void ClickableName(const std::string &name, bool dimmed, HeroStatsChampion who)
{
  float room = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(name.c_str()).x;
  if (room > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room);
  ImVec2 at = ImGui::GetCursorScreenPos();
  ImVec2 size = ImGui::CalcTextSize(name.c_str());
  bool hovered = ImGui::IsMouseHoveringRect(at, ImVec2(at.x + size.x, at.y + size.y)) && ImGui::IsWindowHovered();
  ImVec4 colour = hovered ? Colour(TITLE_TEXT_RGB) : dimmed ? DimmedColour() : Colour(SECTION_RGB);
  ImGui::TextColored(colour, "%s", name.c_str());
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) OpenHeroPage(who);
}

typedef int HeroStatsCell::*CellField;

static void RowLabel(const char *label, bool indent)
{
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  if (indent) ImGui::Indent(10);
  ImGui::TextUnformatted(label);
  if (indent) ImGui::Unindent(10);
}

static void NumberRow(const char *label, const HeroStatsOverview &data, CellField field, ImU32 colour,
                      bool isBad, bool indent)
{
  RowLabel(label, indent);
  for (size_t i = 0; i < data.cells.size(); i++)
  {
    ImGui::TableSetColumnIndex((int)i + 1);
    int value = data.cells[i].*field;
    bool greyed = i < data.columns.size() && data.columns[i].hasLeft;
    ImVec4 tint = Colour(TEXT_RGB);
    if (greyed) tint = DimmedColour();
    else if (isBad && value > 0) tint = Colour(BAD_RGB);
    else if (colour != 0) tint = Colour(colour);
    RightText(HeroStatsView_Number(value), tint);
  }
}

static void GroupRow(const char *title)
{
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Dummy(ImVec2(0, 2));
  ImGui::TextColored(Colour(SECTION_RGB), "%s", title);
}

// A row with a number in the Party column only
static void PartyOnlyRow(const char *label, int columnCount, int value, bool isBad)
{
  RowLabel(label, true);
  ImGui::TableSetColumnIndex(columnCount - 1);
  RightText(HeroStatsView_Number(value), isBad && value > 0 ? Colour(BAD_RGB) : Colour(TEXT_RGB));
}

static void DrawTable(const HeroStatsOverview &data)
{
  int columnCount = (int)data.cells.size() + 1;
  ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
  if (!ImGui::BeginTable("##table", columnCount, flags, ImGui::GetContentRegionAvail())) return;
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 230);
  for (int i = 1; i < columnCount; i++) ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 92);
  ImGui::TableSetupScrollFreeze(1, 1);
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  for (size_t i = 0; i < data.cells.size(); i++)
  {
    ImGui::TableSetColumnIndex((int)i + 1);
    if (i < data.columns.size()) ClickableName(data.columns[i].name, data.columns[i].hasLeft, data.columns[i].id);
    else ClickableName("Party", false, HERO_STATS_PARTY);
  }
  const HeroStatsCell &party = data.cells.back();

  GroupRow("Damage dealt");
  NumberRow("Melee", data, &HeroStatsCell::melee, MELEE_RGB, false, true);
  NumberRow("Ranged", data, &HeroStatsCell::ranged, RANGED_RGB, false, true);
  NumberRow("Magic", data, &HeroStatsCell::magic, MAGIC_RGB, false, true);
  NumberRow("Total", data, &HeroStatsCell::total, 0, false, false);
  RowLabel("share of party damage", true);
  for (size_t i = 0; i < data.cells.size(); i++)
  {
    ImGui::TableSetColumnIndex((int)i + 1);
    RightText(HeroStatsView_Percent(data.cells[i].total, party.total), DimmedColour());
  }
  NumberRow("Kills", data, &HeroStatsCell::kills, KILLS_RGB, false, false);

  GroupRow("Damage taken");
  NumberRow("Total", data, &HeroStatsCell::taken, 0, false, false);
  NumberRow("Friendly fire caused", data, &HeroStatsCell::friendlyFire, 0, true, false);

  GroupRow("Healing");
  NumberRow("Healed", data, &HeroStatsCell::healed, HEALED_RGB, false, true);
  NumberRow("Healing given (potions brewed)", data, &HeroStatsCell::given, GIVEN_RGB, false, true);

  GroupRow("Hardship");
  NumberRow("Deaths", data, &HeroStatsCell::deaths, 0, true, true);
  if (shownLevel == HERO_STATS_WHOLE_RUN)
  {
    PartyOnlyRow("Party wiped out", columnCount, data.wipes, true);
    PartyOnlyRow("Reloads", columnCount, data.reloads, false);
  }

  GroupRow("Mana");
  NumberRow("Generated", data, &HeroStatsCell::generated, GENERATED_RGB, false, true);
  NumberRow("Spent", data, &HeroStatsCell::spent, 0, false, true);
  NumberRow("Wasted on failures", data, &HeroStatsCell::wasted, 0, true, true);

  GroupRow("Movement");
  for (int c = 0; c < HERO_MOVE_COLUMN_COUNT; c++)
    PartyOnlyRow(HERO_STATS_MOVEMENT_LABELS[c], columnCount, HeroStatsPages_MovementValue(data.movement, c), false);
  ImGui::EndTable();
}

// ---------------------------------------------------------------------------------------------
// Bars

struct BarSegment { int value; ImU32 rgb; };

static const float NAME_WIDTH = 100;
static const float BAR_SCALE_WIDE = 210;
static const float BAR_SCALE_NARROW = 150;

static void BarSectionTitle(const char *title)
{
  ImGui::Dummy(ImVec2(0, 4));
  ImGui::TextColored(Colour(SECTION_RGB), "%s", title);
}

static void LegendEntry(ImU32 rgb, const char *text, bool first)
{
  if (!first) ImGui::SameLine(0, 14);
  ImVec2 at = ImGui::GetCursorScreenPos();
  float h = ImGui::GetTextLineHeight();
  ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(at.x, at.y + 3), ImVec2(at.x + 10, at.y + h - 3), PackedColour(rgb));
  ImGui::Dummy(ImVec2(10, h));
  ImGui::SameLine(0, 4);
  ImGui::TextColored(DimmedColour(), "%s", text);
}

// One row: the champion's name (none on a continuation row), then the segments one after the
// other, then the text. scale is pixels per unit.
static void BarRow(const HeroStatsColumn *column, const BarSegment *segments, int segmentCount, float scale,
                   const std::string &text)
{
  ImVec2 at = ImGui::GetCursorScreenPos();
  float lineHeight = ImGui::GetTextLineHeight();
  if (column != NULL)
  {
    ImVec2 size = ImGui::CalcTextSize(column->name.c_str());
    bool hovered = ImGui::IsMouseHoveringRect(at, ImVec2(at.x + size.x, at.y + size.y)) && ImGui::IsWindowHovered();
    ImVec4 colour = hovered ? Colour(TITLE_TEXT_RGB) : column->hasLeft ? DimmedColour() : Colour(TEXT_RGB);
    ImGui::TextColored(colour, "%s", column->name.c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) OpenHeroPage(column->id);
  }
  else ImGui::Dummy(ImVec2(0, lineHeight));
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  float x = at.x + NAME_WIDTH;
  for (int i = 0; i < segmentCount; i++)
  {
    if (segments[i].value <= 0) continue;
    float width = std::max(2.0f, std::floor(segments[i].value * scale + 0.5f));
    drawList->AddRectFilled(ImVec2(x, at.y + 2), ImVec2(x + width, at.y + lineHeight - 2), PackedColour(segments[i].rgb));
    x += width;
  }
  drawList->AddText(ImVec2(x + 8, at.y), PackedColour(TEXT_RGB), text.c_str());
}

static float ScaleFor(float width, int largest)
{
  return width / (float)std::max(1, largest);
}

static void DrawBars(const HeroStatsOverview &data)
{
  ImGui::BeginChild("##bars", ImGui::GetContentRegionAvail(), false);
  ImGui::Indent(GUTTER);
  size_t n = data.columns.size();
  const std::vector<HeroStatsCell> &cells = data.cells;

  BarSectionTitle("Damage dealt");
  LegendEntry(MELEE_RGB, "melee", true);
  LegendEntry(RANGED_RGB, "ranged", false);
  LegendEntry(MAGIC_RGB, "magic", false);
  int largest = 0;
  for (size_t i = 0; i < n; i++) largest = std::max(largest, cells[i].total);
  for (size_t i = 0; i < n; i++)
  {
    BarSegment segs[3] = {{cells[i].melee, MELEE_RGB}, {cells[i].ranged, RANGED_RGB}, {cells[i].magic, MAGIC_RGB}};
    BarRow(&data.columns[i], segs, 3, ScaleFor(BAR_SCALE_WIDE, largest), HeroStatsView_Number(cells[i].total));
  }

  BarSectionTitle("Kills");
  largest = 0;
  for (size_t i = 0; i < n; i++) largest = std::max(largest, cells[i].kills);
  for (size_t i = 0; i < n; i++)
  {
    BarSegment seg = {cells[i].kills, KILLS_RGB};
    BarRow(&data.columns[i], &seg, 1, ScaleFor(BAR_SCALE_NARROW, largest), HeroStatsView_Number(cells[i].kills));
  }

  BarSectionTitle("Damage taken");
  LegendEntry(TAKEN_RGB, "taken", true);
  LegendEntry(BAD_RGB, "friendly fire caused", false);
  largest = 0;
  for (size_t i = 0; i < n; i++) largest = std::max(largest, cells[i].taken + cells[i].friendlyFire);
  for (size_t i = 0; i < n; i++)
  {
    BarSegment segs[2] = {{cells[i].taken, TAKEN_RGB}, {cells[i].friendlyFire, BAD_RGB}};
    BarRow(&data.columns[i], segs, 2, ScaleFor(BAR_SCALE_WIDE, largest), HeroStatsView_Number(cells[i].taken));
  }

  BarSectionTitle("Healing");
  LegendEntry(HEALED_RGB, "healed", true);
  LegendEntry(GIVEN_RGB, "healing given", false);
  largest = 0;
  for (size_t i = 0; i < n; i++) largest = std::max(largest, std::max(cells[i].healed, cells[i].given));
  for (size_t i = 0; i < n; i++)
  {
    float scale = ScaleFor(BAR_SCALE_WIDE, largest);
    BarSegment healed = {cells[i].healed, HEALED_RGB};
    BarRow(&data.columns[i], &healed, 1, scale, HeroStatsView_Number(cells[i].healed));
    if (cells[i].given > 0)
    {
      BarSegment given = {cells[i].given, GIVEN_RGB};
      BarRow(NULL, &given, 1, scale, HeroStatsView_Number(cells[i].given) + " given");
    }
  }

  BarSectionTitle("Mana");
  LegendEntry(GENERATED_RGB, "generated", true);
  LegendEntry(RANGED_RGB, "spent", false);
  LegendEntry(BAD_RGB, "wasted", false);
  largest = 0;
  for (size_t i = 0; i < n; i++) largest = std::max(largest, std::max(cells[i].generated, cells[i].spent));
  for (size_t i = 0; i < n; i++)
  {
    if (cells[i].generated + cells[i].spent <= 0) continue;
    float scale = ScaleFor(BAR_SCALE_NARROW, largest);
    BarSegment generated = {cells[i].generated, GENERATED_RGB};
    BarRow(&data.columns[i], &generated, 1, scale, HeroStatsView_Number(cells[i].generated));
    BarSegment segs[2] = {{std::max(0, cells[i].spent - cells[i].wasted), RANGED_RGB}, {cells[i].wasted, BAD_RGB}};
    BarRow(NULL, segs, 2, scale,
           HeroStatsView_Number(cells[i].spent) + " (" + HeroStatsView_Number(cells[i].wasted) + " wasted)");
  }
  ImGui::Unindent(GUTTER);
  ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------
// Share bars, By level, Monsters and Movement

// A bar of the segments side by side, as wide as the cell, each as wide as its share of total
static void ShareBar(const std::vector<BarSegment> &segments, int total)
{
  ImVec2 at = ImGui::GetCursorScreenPos();
  float width = std::max(10.0f, ImGui::GetContentRegionAvail().x - 6);
  float height = ImGui::GetTextLineHeight();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  if (total > 0)
  {
    float x = at.x;
    for (size_t i = 0; i < segments.size(); i++)
    {
      if (segments[i].value <= 0) continue;
      float w = std::max(2.0f, std::floor(width * segments[i].value / total + 0.5f));
      drawList->AddRectFilled(ImVec2(x, at.y + 2), ImVec2(x + w, at.y + height - 2), PackedColour(segments[i].rgb));
      x += w;
    }
  }
  ImGui::Dummy(ImVec2(width, height));
}

struct ByLevelRow
{
  int level;                // HERO_STATS_WHOLE_RUN for the last row
  std::vector<int> values;  // per column, then the party
};

static void DrawMetricButtons(const std::vector<HeroStatsColumn> &columns)
{
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 4));
  for (int m = 0; m < HERO_METRIC_COUNT; m++)
  {
    if (m > 0) ImGui::SameLine();
    if (PageButton(HERO_STATS_METRIC_LABELS[m], m == shownMetric)) shownMetric = m;
  }
  if (shownMetric == HeroMetricMovement) return;  // the party's: no champions to tell apart
  ImGui::SameLine(0, 30);
  for (size_t i = 0; i < columns.size(); i++) LegendEntry(HERO_RGB[i % 4], columns[i].name.c_str(), i == 0);
}

static void DrawMovement(const HeroStats &stats, const HeroStatsPicker &picker);

// A level's row of a table that lists the levels: clicking it shows the level in the Table
static void LevelCell(int level, bool isRun)
{
  std::string label = isRun ? "Whole run" : LevelLabel(level);
  if (isRun) ImGui::TextColored(Colour(TITLE_TEXT_RGB), "%s", label.c_str());
  else if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
  {
    shownLevel = level;
    shownPage = StatsPageTable;
  }
}

static void DrawByLevel(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                        const HeroStatsPicker &picker)
{
  DrawMetricButtons(columns);
  if (shownMetric == HeroMetricMovement)
  {
    DrawMovement(stats, picker);
    return;
  }
  size_t n = columns.size();
  // One query per level, column and metric, but only when the metric, the columns or the levels
  // changed, and at most twice a second; the last row is the whole run
  static std::vector<ByLevelRow> rows;
  static int cachedMetric;
  static std::string cachedKey;
  static Uint32 cachedAt;
  std::string key = ColumnsKey(columns) + "|";
  for (size_t r = 0; r < picker.levels.size(); r++) key += std::to_string(picker.levels[r]) + ",";
  Uint32 now = SDL_GetTicks();
  if (!byLevelCached || cachedMetric != shownMetric || key != cachedKey || now - cachedAt >= CACHE_LIFETIME)
  {
    rows.clear();
    for (size_t r = 0; r <= picker.levels.size(); r++)
    {
      ByLevelRow row;
      row.level = r < picker.levels.size() ? picker.levels[r] : HERO_STATS_WHOLE_RUN;
      for (size_t i = 0; i < n; i++) row.values.push_back(HeroStatsPages_Metric(stats, columns[i].id, row.level, shownMetric));
      row.values.push_back(HeroStatsPages_Metric(stats, HERO_STATS_PARTY, row.level, shownMetric));
      rows.push_back(row);
    }
    cachedMetric = shownMetric;
    cachedKey = key;
    cachedAt = now;
    byLevelCached = true;
  }
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 4));
  int columnCount = (int)n + 3;
  ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_Sortable;
  if (!ImGui::BeginTable("##bylevel", columnCount, flags, ImGui::GetContentRegionAvail())) return;
  SortColumn("Level", 100, false, true);
  std::vector<bool> dimmed(1, false);
  for (size_t i = 0; i < n; i++)
  {
    SortColumn(columns[i].name + "##" + std::to_string(i), 92, true);
    dimmed.push_back(columns[i].hasLeft);
  }
  SortColumn("Party", 92, true);
  SortColumn("Share", 240, true);
  ImGui::TableSetupScrollFreeze(1, 1);
  SortHeaders(dimmed);
  std::vector<ByLevelRow> levels(rows.begin(), rows.end() - 1);
  SortRows(&levels, CurrentSort(), [n](const ByLevelRow &a, const ByLevelRow &b, int column) {
    if (column == 0) return Compare(a.level, b.level);
    size_t value = std::min((size_t)column - 1, n);  // the Share column sorts by the party's value
    return Compare(a.values[value], b.values[value]);
  });
  levels.push_back(rows.back());
  for (size_t r = 0; r < levels.size(); r++)
  {
    const ByLevelRow &row = levels[r];
    bool isRun = row.level == HERO_STATS_WHOLE_RUN;
    ImGui::TableNextRow();
    if (isRun) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, PackedColour(FRAME_RGB));
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(row.level);
    LevelCell(row.level, isRun);
    ImGui::PopID();
    std::vector<BarSegment> segments;
    for (size_t i = 0; i <= n; i++)
    {
      ImGui::TableSetColumnIndex((int)i + 1);
      bool greyed = i < n && columns[i].hasLeft;
      RightText(HeroStatsView_Number(row.values[i]), greyed ? DimmedColour() : Colour(TEXT_RGB));
      if (i < n) segments.push_back({row.values[i], HERO_RGB[i % 4]});
    }
    ImGui::TableSetColumnIndex(columnCount - 1);
    ShareBar(segments, row.values[n]);
  }
  ImGui::EndTable();
}

static void MonsterCount(int value, bool greyed)
{
  if (value == 0) RightText("\xc2\xb7", Colour(OTHER_RGB));
  else RightText(HeroStatsView_Number(value), greyed ? DimmedColour() : Colour(TEXT_RGB));
}

static void DrawMonsters(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns)
{
  std::vector<HeroStatsMonsterLine> lines = HeroStatsPages_Monsters(stats, columns, shownLevel, HeroStats_NamesOfTheGame());
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 4));
  if (lines.empty())
  {
    ImGui::TextColored(DimmedColour(), "No monsters killed yet.");
    return;
  }
  size_t n = columns.size();
  HeroStatsMonsterLine all = HeroStatsPages_AllMonsters(lines, n);
  // Door, Pit and Other only when something was killed that way
  const char *extraLabel[3] = {"Door", "Pit", "Other"};
  int extraTotal[3] = {all.door, all.pit, all.other};
  std::vector<int> shownExtra;
  for (int e = 0; e < 3; e++)
    if (extraTotal[e] > 0) shownExtra.push_back(e);
  int columnCount = 1 + (int)n + (int)shownExtra.size() + 2;
  ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_Sortable;
  ImVec2 size = ImGui::GetContentRegionAvail();
  size.y = std::max(40.0f, size.y - ImGui::GetTextLineHeightWithSpacing() - 4);  // room for the note
  if (!ImGui::BeginTable("##monsters", columnCount, flags, size)) return;
  SortColumn("Monster", 190, false);
  std::vector<bool> dimmed(1, false);
  for (size_t i = 0; i < n; i++)
  {
    SortColumn(columns[i].name + "##" + std::to_string(i), 72, true);
    dimmed.push_back(columns[i].hasLeft);
  }
  for (size_t e = 0; e < shownExtra.size(); e++) SortColumn(extraLabel[shownExtra[e]], 72, true);
  SortColumn("All causes", 72, true, true);
  SortColumn("Killed by", 240, true);
  ImGui::TableSetupScrollFreeze(1, 1);
  SortHeaders(dimmed);
  // The value of a column: the champions, the shown extras, then the total (twice: the bar)
  auto valueOf = [&](const HeroStatsMonsterLine &line, int column) {
    if (column <= (int)n) return line.byColumn[column - 1];
    size_t e = column - 1 - n;
    if (e < shownExtra.size())
    {
      int extra[3] = {line.door, line.pit, line.other};
      return extra[shownExtra[e]];
    }
    return line.total;
  };
  SortRows(&lines, CurrentSort(), [&](const HeroStatsMonsterLine &a, const HeroStatsMonsterLine &b, int column) {
    if (column == 0) return a.name.compare(b.name);
    return Compare(valueOf(a, column), valueOf(b, column));
  });
  for (size_t l = 0; l <= lines.size(); l++)
  {
    bool isAll = l == lines.size();
    const HeroStatsMonsterLine &line = isAll ? all : lines[l];
    ImGui::TableNextRow();
    if (isAll) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, PackedColour(FRAME_RGB));
    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(isAll ? Colour(TITLE_TEXT_RGB) : Colour(TEXT_RGB), "%s", line.name.c_str());
    std::vector<BarSegment> segments;
    for (size_t i = 0; i < n; i++)
    {
      ImGui::TableSetColumnIndex((int)i + 1);
      MonsterCount(line.byColumn[i], columns[i].hasLeft);
      segments.push_back({line.byColumn[i], HERO_RGB[i % 4]});
    }
    int lineExtra[3] = {line.door, line.pit, line.other};
    ImU32 extraRgb[3] = {DOOR_RGB, PIT_RGB, OTHER_RGB};
    for (size_t e = 0; e < shownExtra.size(); e++)
    {
      ImGui::TableSetColumnIndex((int)(n + 1 + e));
      MonsterCount(lineExtra[shownExtra[e]], false);
    }
    for (int e = 0; e < 3; e++) segments.push_back({lineExtra[e], extraRgb[e]});
    ImGui::TableSetColumnIndex(columnCount - 2);
    RightText(HeroStatsView_Number(line.total), Colour(KILLS_RGB));
    ImGui::TableSetColumnIndex(columnCount - 1);
    ShareBar(segments, line.total);
  }
  ImGui::EndTable();
  ImGui::SetCursorPosX(GUTTER);
  for (size_t i = 0; i < n; i++) LegendEntry(HERO_RGB[i % 4], columns[i].name.c_str(), i == 0);
  for (size_t e = 0; e < shownExtra.size(); e++)
  {
    ImU32 rgb[3] = {DOOR_RGB, PIT_RGB, OTHER_RGB};
    LegendEntry(rgb[shownExtra[e]], extraLabel[shownExtra[e]], n == 0 && e == 0);
  }
  ImGui::SameLine(0, 14);
  ImGui::TextColored(DimmedColour(), "(Other = clouds, traps and monsters)");
}

struct MovementRow
{
  int level;  // HERO_STATS_WHOLE_RUN for the last row
  HeroStatsMovement movement;
};

// By level's Movement: the party's steps, turns, bumps, teleports, pits and time on each visited
// level and in the whole run
static void DrawMovement(const HeroStats &stats, const HeroStatsPicker &picker)
{
  std::vector<MovementRow> rows;
  for (size_t r = 0; r < picker.levels.size(); r++)
  {
    MovementRow row = {picker.levels[r], stats.Movement(picker.levels[r])};
    rows.push_back(row);
  }
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 4));
  ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_Sortable;
  int columnCount = 1 + HERO_MOVE_COLUMN_COUNT;
  if (!ImGui::BeginTable("##movement", columnCount, flags, ImGui::GetContentRegionAvail())) return;
  SortColumn("Level", 100, false, true);
  for (int c = 0; c < HERO_MOVE_COLUMN_COUNT; c++) SortColumn(HERO_STATS_MOVEMENT_LABELS[c], c >= HeroMoveTicks ? 110 : 84, true);
  ImGui::TableSetupScrollFreeze(1, 1);
  SortHeaders(std::vector<bool>());
  SortRows(&rows, CurrentSort(), [](const MovementRow &a, const MovementRow &b, int column) {
    if (column == 0) return Compare(a.level, b.level);
    return Compare(HeroStatsPages_MovementValue(a.movement, column - 1), HeroStatsPages_MovementValue(b.movement, column - 1));
  });
  MovementRow run = {HERO_STATS_WHOLE_RUN, stats.Movement(HERO_STATS_WHOLE_RUN)};
  rows.push_back(run);
  for (size_t r = 0; r < rows.size(); r++)
  {
    bool isRun = rows[r].level == HERO_STATS_WHOLE_RUN;
    ImGui::TableNextRow();
    if (isRun) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, PackedColour(FRAME_RGB));
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(rows[r].level);
    LevelCell(rows[r].level, isRun);
    ImGui::PopID();
    for (int column = 1; column < columnCount; column++)
    {
      ImGui::TableSetColumnIndex(column);
      MonsterCount(HeroStatsPages_MovementValue(rows[r].movement, column - 1), false);
    }
  }
  ImGui::EndTable();
}

// ---------------------------------------------------------------------------------------------
// Hero and Party pages

static const HeroStatsColumn *ColumnOf(const std::vector<HeroStatsColumn> &columns, HeroStatsChampion id)
{
  for (size_t i = 0; i < columns.size(); i++)
    if (columns[i].id == id) return &columns[i];
  return NULL;
}

static const HeroStatsHeroPage &CachedHeroPage(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                               HeroStatsChampion who, int level)
{
  static HeroStatsHeroPage data;
  static HeroStatsChampion cachedWho;
  static int cachedLevel;
  static std::string cachedColumns;
  static Uint32 cachedAt;
  std::string key = ColumnsKey(columns);
  Uint32 now = SDL_GetTicks();
  if (!heroPageCached || who != cachedWho || level != cachedLevel || key != cachedColumns ||
      now - cachedAt >= CACHE_LIFETIME)
  {
    data = HeroStatsPages_Hero(stats, columns, who, level, HeroStats_NamesOfTheGame());
    heroPageCached = true;
    cachedWho = who;
    cachedLevel = level;
    cachedColumns = key;
    cachedAt = now;
  }
  return data;
}

// A section header that folds; the summary sits right-aligned on the header line
static bool SectionHeader(int bit, const char *title, const std::string &summary, ImU32 summaryRgb)
{
  ImGui::SetNextItemOpen((openSections & bit) != 0, ImGuiCond_Always);
  ImGui::PushStyleColor(ImGuiCol_Text, Colour(SECTION_RGB));
  bool open = ImGui::CollapsingHeader(title);
  ImGui::PopStyleColor();
  if (open) openSections |= bit;
  else openSections &= ~bit;
  ImVec2 min = ImGui::GetItemRectMin();
  ImVec2 max = ImGui::GetItemRectMax();
  float width = ImGui::CalcTextSize(summary.c_str()).x;
  float titleEnd = min.x + ImGui::CalcTextSize(title).x + ImGui::GetTreeNodeToLabelSpacing() + 20;
  float x = max.x - width - 10;
  if (x > titleEnd)
    ImGui::GetWindowDrawList()->AddText(ImVec2(x, min.y + ImGui::GetStyle().FramePadding.y), PackedColour(summaryRgb),
                                        summary.c_str());
  return open;
}

static void SectionGap(void)
{
  ImGui::Dummy(ImVec2(0, 2));
}

static void NumberCell(int column, int value, const ImVec4 &colour)
{
  ImGui::TableSetColumnIndex(column);
  RightText(HeroStatsView_Number(value), colour);
}

static void TextCell(int column, const std::string &text, const ImVec4 &colour)
{
  ImGui::TableSetColumnIndex(column);
  RightText(text, colour);
}

// Tries shows the blows and missiles and the actions that cannot hit; a line of such actions
// only (War Cry ...) has "-" for what only a hit can have. Rate is hits per blow or missile, Avg
// the damage per hit.
template <typename Line>
static void AttackCells(const Line &line, const ImVec4 &colour)
{
  NumberCell(1, line.tries + line.uses, colour);
  if (line.tries == 0 && line.uses > 0)
  {
    for (int column = 2; column <= 6; column++) TextCell(column, "-", DimmedColour());
    return;
  }
  NumberCell(2, line.hits, colour);
  TextCell(3, HeroStatsView_Percent(line.hits, line.tries), colour);
  TextCell(4, line.hits > 0 ? HeroStatsView_Number((line.damage + line.hits / 2) / line.hits) : "-", colour);
  NumberCell(5, line.damage, colour);
  NumberCell(6, line.kills, colour);
}

static double Ratio(int part, int whole)
{
  return whole > 0 ? (double)part / whole : -1;
}

// The columns of the Attacks table: name, tries, hits, rate, avg, damage, kills
template <typename Line>
static int CompareAttacks(const Line &a, const Line &b, int column)
{
  switch (column)
  {
  case 0: return a.name.compare(b.name);
  case 1: return Compare(a.tries + a.uses, b.tries + b.uses);
  case 2: return Compare(a.hits, b.hits);
  case 3: return Compare(Ratio(a.hits, a.tries), Ratio(b.hits, b.tries));
  case 4: return Compare(Ratio(a.damage, a.hits), Ratio(b.damage, b.hits));
  case 5: return Compare(a.damage, b.damage);
  default: return Compare(a.kills, b.kills);
  }
}

static void DrawAttacks(const HeroStatsHeroPage &page)
{
  if (page.weapons.empty())
  {
    ImGui::TextColored(DimmedColour(), "No attacks yet.");
    return;
  }
  if (!ImGui::BeginTable("##attacks", 7, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable)) return;
  SortColumn("Weapon / option", 230, false);
  SortColumn("Tries", 70, true);
  SortColumn("Hits", 70, true);
  SortColumn("Rate", 70, true);
  SortColumn("Avg", 70, true);
  SortColumn("Damage", 78, true, true);
  SortColumn("Kills", 70, true);
  SortHeaders(std::vector<bool>());
  TableSort sort = CurrentSort();
  std::vector<HeroStatsWeaponLine> weapons = page.weapons;
  SortRows(&weapons, sort, CompareAttacks<HeroStatsWeaponLine>);
  for (size_t w = 0; w < weapons.size(); w++)
  {
    HeroStatsWeaponLine &weapon = weapons[w];
    SortRows(&weapon.options, sort, CompareAttacks<HeroStatsOptionLine>);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    // Open or folded is ImGui's own, per weapon and per hero, for as long as the program runs
    ImGui::PushID((int)heroPageWho);
    ImGui::PushID(weapon.id);
    ImGui::PushStyleColor(ImGuiCol_Text, Colour(TITLE_TEXT_RGB));
    bool open = ImGui::TreeNodeEx(weapon.name.c_str(), ImGuiTreeNodeFlags_None);
    ImGui::PopStyleColor();
    AttackCells(weapon, Colour(TEXT_RGB));
    if (open)
    {
      for (size_t o = 0; o < weapon.options.size(); o++)
      {
        const HeroStatsOptionLine &option = weapon.options[o];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(Colour(0xaaaaaa), "%s", option.name.c_str());
        AttackCells(option, Colour(0xaaaaaa));
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
    ImGui::PopID();
  }
  ImGui::EndTable();
}

// The columns of the Spells table: name, casts, successful, failed, mana, wasted, damage, kills
static int SpellValue(const HeroStatsSpellLine &line, int column)
{
  switch (column)
  {
  case 1: return line.casts;
  case 2: return line.successful;
  case 3: return line.failed;
  case 4: return line.mana;
  case 5: return line.wasted;
  case 6: return line.damage;
  default: return line.kills;
  }
}

static void DrawSpells(const HeroStatsHeroPage &page)
{
  if (page.spells.empty())
  {
    ImGui::TextColored(DimmedColour(), "No spells cast.");
    return;
  }
  if (!ImGui::BeginTable("##spells", 8, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable)) return;
  SortColumn("Spell", 230, false);
  const char *heads[7] = {"Casts", "Successful", "Failed", "Mana", "Wasted", "Damage", "Kills"};
  for (int i = 0; i < 7; i++) SortColumn(heads[i], i == 1 ? 84 : 66, true, i == 3);
  SortHeaders(std::vector<bool>());
  std::vector<HeroStatsSpellLine> spells = page.spells;
  SortRows(&spells, CurrentSort(), [](const HeroStatsSpellLine &a, const HeroStatsSpellLine &b, int column) {
    if (column == 0) return a.name.compare(b.name);
    return Compare(SpellValue(a, column), SpellValue(b, column));
  });
  for (size_t i = 0; i < spells.size(); i++)
  {
    const HeroStatsSpellLine &line = spells[i];
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(line.name.c_str());
    // Every value is a number, 0 included (a Light spell does 0 damage)
    for (int column = 1; column < 8; column++)
      NumberCell(column, SpellValue(line, column),
                 column == 5 && line.wasted > 0 ? Colour(BAD_RGB) : Colour(TEXT_RGB));
  }
  ImGui::EndTable();
}

// A label on the left and a value right-aligned in a column of fixed width
static void ValueLine(const std::string &label, const std::string &value, const ImVec4 &labelColour,
                      const ImVec4 &valueColour, float indent)
{
  const float VALUE_RIGHT = 230 + 78;
  float x = ImGui::GetCursorPosX();
  ImGui::Indent(indent);
  ImGui::TextColored(labelColour, "%s", label.c_str());
  ImGui::Unindent(indent);
  if (!value.empty())
  {
    ImGui::SameLine();
    ImGui::SetCursorPosX(x + VALUE_RIGHT - ImGui::CalcTextSize(value.c_str()).x);
    ImGui::TextColored(valueColour, "%s", value.c_str());
  }
}

static void DrawHeroPage(const HeroStatsHeroPage &page, HeroStatsChampion who)
{
  ImGui::BeginChild("##hero", ImGui::GetContentRegionAvail(), false);
  ImGui::Indent(GUTTER);
  ImGui::Dummy(ImVec2(0, 2));
  ImVec4 text = Colour(TEXT_RGB), bad = Colour(BAD_RGB);

  if (SectionHeader(1, "Attacks", page.summaries.attacks, TAKEN_RGB))
  {
    DrawAttacks(page);
    SectionGap();
  }
  if (SectionHeader(2, "Spells", page.summaries.spells, TAKEN_RGB))
  {
    DrawSpells(page);
    SectionGap();
  }
  if (SectionHeader(4, "Damage taken", page.summaries.harm, TAKEN_RGB))
  {
    if (page.taken.empty()) ImGui::TextColored(DimmedColour(), "No damage taken.");
    for (size_t i = 0; i < page.taken.size(); i++)
      ValueLine(page.taken[i].text, HeroStatsView_Number(page.taken[i].amount),
                page.taken[i].isBad ? bad : text, page.taken[i].isBad ? bad : text, 0);
    SectionGap();
  }
  if (SectionHeader(8, "Healing", page.summaries.healing, HEALED_RGB))
  {
    ValueLine("Healed", HeroStatsView_Number(page.healedTotal), text, Colour(HEALED_RGB), 0);
    for (size_t i = 0; i < page.healing.size(); i++)
      ValueLine(page.healing[i].text, HeroStatsView_Number(page.healing[i].amount), DimmedColour(), text, 12);
    if (page.given != 0)
      ValueLine("Healing given (potions brewed)", HeroStatsView_Number(page.given), text, Colour(GIVEN_RGB), 0);
    if (page.resurrected != 0)
      ValueLine("Resurrected", std::to_string(page.resurrected) + "\xc3\x97", text, text, 0);
    SectionGap();
  }
  bool hurt = !page.deathLines.empty() || page.wipes != 0 || page.reloads != 0;
  if (SectionHeader(16, "Hardship", page.summaries.hardship, hurt ? BAD_RGB : TAKEN_RGB))
  {
    if (!hurt) ImGui::TextColored(DimmedColour(), "Never died.");
    if (!page.deathLines.empty())
    {
      ValueLine(who == HERO_STATS_PARTY ? "Champion deaths" : "Died",
                std::to_string(page.deathLines.size()) + "\xc3\x97", text, bad, 0);
      for (size_t i = 0; i < page.deathLines.size(); i++)
        ValueLine(page.deathLines[i], "", DimmedColour(), text, 12);
    }
    if (page.wipes != 0) ValueLine("Party wiped out", std::to_string(page.wipes) + "\xc3\x97", text, bad, 0);
    if (page.reloads != 0) ValueLine("Reloads", std::to_string(page.reloads) + "\xc3\x97", text, text, 0);
    SectionGap();
  }
  ImGui::TextColored(DimmedColour(),
                     "Click a section or a weapon to fold it, a column header to sort. Which sections are open is remembered.");
  ImGui::Unindent(GUTTER);
  ImGui::EndChild();
}

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

// ---------------------------------------------------------------------------------------------

static void DrawWindowContents(void)
{
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (ImGui::Begin("##statistics", NULL, flags))
  {
    const HeroStats &stats = HeroStats_Store();
    HeroStatsPicker picker = HeroStatsView_Picker(stats, shownLevel);
    // A level of the game before is not in this game's list
    if (shownLevel != HERO_STATS_WHOLE_RUN &&
        std::find(picker.levels.begin(), picker.levels.end(), shownLevel) == picker.levels.end())
    {
      shownLevel = HERO_STATS_WHOLE_RUN;
      picker = HeroStatsView_Picker(stats, shownLevel);
    }
    DrawTitle();
    std::vector<HeroStatsColumn> columns = HeroStatsView_Columns(stats, HeroStats_PartyNow());
    // A champion of a game before is not in this game's list
    if (heroPageOpen && heroPageWho != HERO_STATS_PARTY && ColumnOf(columns, heroPageWho) == NULL)
      heroPageWho = HERO_STATS_PARTY;
    // By level lists every level
    bool showPicker = heroPageOpen || shownPage != StatsPageByLevel;
    DrawTopBar(picker, showPicker, columns);
    if (stats.Champions().empty())
    {
      ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 8));
      ImGui::TextColored(DimmedColour(), "No statistics yet.");
    }
    else if (heroPageOpen)
      DrawHeroPage(CachedHeroPage(stats, columns, heroPageWho, shownLevel), heroPageWho);
    else if (shownPage == StatsPageTable || shownPage == StatsPageBars)
    {
      const HeroStatsOverview &data = CachedOverview(stats, columns, shownLevel);
      ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 4));
      if (shownPage == StatsPageTable) DrawTable(data);
      else DrawBars(data);
    }
    else if (shownPage == StatsPageByLevel)
      DrawByLevel(stats, columns, picker);
    else if (shownPage == StatsPageMonsters)
      DrawMonsters(stats, columns);

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
  ImVec4 background = Colour(BACKGROUND_RGB);
  SDL_SetRenderDrawColor(statsRenderer, (Uint8)(background.x * 255), (Uint8)(background.y * 255),
                         (Uint8)(background.z * 255), 0xff);
  SDL_RenderClear(statsRenderer);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), statsRenderer);
  SDL_RenderPresent(statsRenderer);
}
