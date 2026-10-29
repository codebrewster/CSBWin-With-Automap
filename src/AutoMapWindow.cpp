// AutoMapWindow.cpp : the second window, which draws the map.
#include "stdafx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "AutoMapScene.h"
#include "AutoMapSprites.h"
#include "AutoMapWindow.h"
// Function declarations only, free of engine structures
#include "AutoMapHooks.h"
// Item names for the tooltip footer; the scene carries only the type id
#include "AutoMapLevelReader.h"

#include "confile.h"

static const char *SETTINGS_SECTION = "settings";
static const int DEFAULT_SIZE = 640;
static const int SMALLEST_SIZE = 200;

static const SDL_Color BACKGROUND = {0x12, 0x11, 0x10, 0xff};
static const SDL_Color WALL = {0x8d, 0x87, 0x7a, 0xff};
static const SDL_Color WALL_EDGE = {0xa3, 0x9c, 0x8d, 0xff};
static const SDL_Color FLOOR = {0x2e, 0x2b, 0x27, 0xff};
static const SDL_Color FLOOR_EDGE = {0x3f, 0x3b, 0x35, 0xff};
static const SDL_Color EXPLORED_HIGHLIGHT = {0x4c, 0xd0, 0x6a, 56};
static const SDL_Color DOOR = {0x8b, 0x5a, 0x2b, 0xff};
// A sealed door works like a wall, so it wears wall grey instead of door brown:
// quiet, but unmistakable next to a real door. The leaf shapes (and their gap)
// still read as a door, so it never merges with actual full-cell walls.
static const SDL_Color SEALED_DOOR = {0x8d, 0x87, 0x7a, 0xff};
static const SDL_Color DESTROYED_DOOR = {0x5a, 0x3a, 0x1c, 0xff};
static const SDL_Color STAIRS_UP_LIGHT = {0x9a, 0x94, 0x88, 0xff};
static const SDL_Color STAIRS_UP_DARK = {0x5e, 0x59, 0x4f, 0xff};
static const SDL_Color STAIRS_DOWN_LIGHT = {0x6d, 0x68, 0x5f, 0xff};
static const SDL_Color STAIRS_DOWN_DARK = {0x3a, 0x37, 0x31, 0xff};
static const SDL_Color TELEPORTER_HAZE = {80, 200, 255, 115};
static const SDL_Color TELEPORTER_EDGE = {150, 225, 255, 204};
static const SDL_Color PIT_MARKER = {0x00, 0x00, 0x00, 0xff};
static const SDL_Color PIT_MARKER_EDGE = {0x8a, 0x4a, 0x4a, 0xff};
static const SDL_Color PARTY = {0xff, 0x4d, 0x4d, 0xff};
static const SDL_Color WHITE = {0xff, 0xff, 0xff, 0xff};
static const SDL_Color RULER_BACKGROUND = {0x24, 0x22, 0x1f, 0xff};
static const SDL_Color RULER_TEXT = {0xb8, 0xb0, 0xa0, 0xff};
static const SDL_Color RULER_TICK = {0x6e, 0x68, 0x5c, 0xff};

// How much of a door's width stays open in each of the engine's six door positions
static const int DOOR_GAP_PERCENT[] = {70, 50, 30, 15, 0, 70};
static const int STAIR_STRIPES = 4;
static const SDL_Color FLOOR_FEATURE_MARKER = {0x7d, 0x77, 0x6b, 0xff};
static const SDL_Color WALL_FEATURE_BACKDROP = {0x3a, 0x2f, 0x27, 0xff};
static const SDL_Color WALL_FEATURE_MARKER = {0xff, 0x8a, 0x2b, 0xff};
static const SDL_Color WALL_SIDE_LINE = {0xe8, 0xe2, 0xd2, 0xff};
static const SDL_Color FLAVOR_TICK = {0x5a, 0x56, 0x4e, 0xff};
static const SDL_Color HIDDEN_SWITCH = {0xff, 0x8a, 0x2b, 0xff};
static const SDL_Color ITEM_MARKER = {0xf2, 0xd5, 0x44, 0xff};
static const SDL_Color MONSTER_MARKER = {0xc0, 0x50, 0x50, 0xff};
static const SDL_Color MONSTER_SHADOW = {0, 0, 0, 115};
static const SDL_Color CLOUD_HAZE = {80, 200, 120, 115};
static const SDL_Color CLOUD_EDGE = {150, 230, 170, 204};
static const SDL_Color MISSILE_MARKER = {0xff, 0x6a, 0x2b, 0xff};
static const SDL_Color FALSE_WALL = {0x4a, 0x38, 0x56, 0xff};
static const SDL_Color FALSE_WALL_EDGE = {0x8a, 0x6e, 0xa0, 0xff};
static const SDL_Color FALSE_WALL_STRIPE = {0x66, 0x4e, 0x7a, 0xff};
static const SDL_Color INVISIBLE_TELEPORTER = {0x8a, 0x8a, 0x9a, 0xff};
static const SDL_Color DORMANT_TELEPORTER = {0x5a, 0x5a, 0x66, 0xff};
static const SDL_Color SPINNER = {0x6a, 0xd8, 0xe8, 0xff};
static const SDL_Color DORMANT_SPINNER = {0x40, 0x6e, 0x76, 0xff};
static const SDL_Color PAD = {0xc8, 0x9a, 0x4a, 0xff};
static const SDL_Color PAD_DORMANT = {0x8a, 0x84, 0x7a, 0xff};
static const SDL_Color GENERATOR = {0xc0, 0x50, 0x50, 0xff};
static const SDL_Color GENERATOR_SPENT = {0x8a, 0x5a, 0x5a, 0xff};
static const SDL_Color COG = {0x9a, 0x94, 0x88, 0xff};
static const SDL_Color LINK_BADGE = {0xff, 0xc8, 0x3a, 0xff};
// Feeders of a hovered relay, and goals that still wait on something
static const SDL_Color LINK_INCOMING = {0x4a, 0x9e, 0xff, 0xff};
static const SDL_Color LINK_WARNING = {0xff, 0x6a, 0x2b, 0xff};
// The player's own notes: pit marks, player links and link mode. A violet apart from the link
// yellow, the feeder blue and the warning orange, and brighter than the false-wall purple.
static const SDL_Color PLAYER_NOTE = {0xc7, 0x7d, 0xff, 0xff};

// Features, items and pits are drawn by the engine with this palette entry left out
static const int TRANSPARENT_COLOUR_OF_THE_GAME = 10;
static const int WRITING_LINES = 3;
// A monster picture of this many pixels is as wide as a cell, and so is an item picture
static const int MONSTER_PIXELS_PER_CELL = 60;
static const int ITEM_PIXELS_PER_CELL = 64;

struct WindowPlacement
{
  int x;
  int y;
  int width;
  int height;
};

struct FloatPoint
{
  float x;
  float y;
};

static SDL_Window *mapWindow = NULL;
static SDL_Renderer *mapRenderer = NULL;
static AutoMapSprites sprites;
static AutoMapDrawList shownDrawList;
static Uint32 drawListSerial = 0;
// The unmoving layer (cells, items, clouds, pads, highlight) rendered once per draw list
// and zoom step: panning then only blits this texture and draws what moves
static SDL_Texture *staticCache = NULL;
static Uint32 staticSerial = 0;
static int staticCellSize = 0;
static WindowPlacement placement = {SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DEFAULT_SIZE, DEFAULT_SIZE};
static bool wasShownWhenSettingsWereRead = false;
static bool isShown = false;
static bool isDirty = false;
static bool drawListWasPainted = true;
static bool pointerIsOverMap = false;
static int pointerStateOfTheGame = SDL_ENABLE;
static int zoomSteps = 0;
// One bit per AutoMapShown; everything by default
static unsigned shownThings = (1u << AUTOMAP_SHOWN_COUNT) - 1;

// Set map_timing = 1 in the settings and the map reports its renderer, paint times,
// rebuild counts and cache use to the terminal. Off by default, costs nothing. The flag
// is read with the rest of the settings: outside ReadSettings no config file is selected
// and every lookup misses.
static bool timingReportsOn = false;

static bool MapTimingOn(void)
{
  return timingReportsOn;
}

bool AutoMapWindow_TimingReportsOn(void)
{
  return timingReportsOn;
}

static Uint64 paintMicroseconds = 0;
static int paintCount = 0;
static int staticRenders = 0;
static int staticBlits = 0;

static double MicrosecondsPerTick(void)
{
  return 1000000.0 / SDL_GetPerformanceFrequency();
}

// A ruler of global coordinates runs along each side of the window; the level is drawn
// in the map area between the rulers
static const int RULER = 30;
// The ruler numbers are drawn with the 3x5 bitmap font of AutoMapScene_DigitPixel
static const int DIGIT_SCALE = 2;
// A middle-drag pans the zoomed view: the exact centre (fractions included) freezes where
// the drag started and the view stops following the party until a middle-click without a
// drag recentres it. Fractions keep re-pans continuous and zooming centred on the same spot.
static bool viewFollowsParty = true;
static float viewCenterX = 0;
static float viewCenterY = 0;
static bool middleDragging = false;
static bool dragLeftTheStart = false;
static bool dragNeedsResync = false;
static int dragDistance = 0;
static const int DRAG_START_DISTANCE = 4;
static void CancelMiddleDrag(void);
// Hovered cell for action-link arrows: the cell under the pointer, or nothing when the
// pointer is off the level. Forward mode draws outgoing edges, reverse mode incoming ones.
static bool hoverValid = false;
static int hoverX = -1;
static int hoverY = -1;
// Under Shift the tooltip footer is uncapped and may outgrow the window: the wheel scrolls
// it. tooltipScrollLines is the footer lines scrolled off the top, tooltipScrollMax how far
// it can go (0 when the footer fits, which leaves the wheel to the zoom). The draw sets
// the max; a new hovered cell or a released Shift puts the scroll back to the top.
static int tooltipScrollLines = 0;
static int tooltipScrollMax = 0;
// Window pixel of the pointer when the hover was last updated: the icon tooltip
// parks next to the cursor, not on top of the hovered cell.
static int hoverPixelX = 0;
static int hoverPixelY = 0;
static void ClearHover(void);
// Legend overlay state and geometry, drawn with the legend below.
static int legendPage = 0;
static SDL_Rect LegendButtonRect(void);
static SDL_Rect LegendPanelRect(void);
static bool LegendPageHasRows(int page);
static bool PointInRect(int x, int y, const SDL_Rect &area);
// Item search state: the query the player typed, the hits across all levels
// (already fog-filtered, so unexplored never matches), the selected hit and
// the first shown row of the scrollable results panel.
static bool searchOpen = false;
static std::string searchQuery;
static std::vector<AutoMapSearchHit> searchHits;
static int searchSelected = 0;
static int searchTop = 0;
// SDL text input is one switch for the whole program, and the game's ImGui
// dialogs (the Save As name) need it on, so the search box hands it back the
// way it found it rather than switching it off.
static bool searchStartedTextInput = false;
static SDL_Rect SearchButtonRect(void);
static void SearchGiveBackTextInput(void);
static void SearchFullRefresh(void);
static void SearchRefreshCurrentLevel(void);
static void SearchCenterOnSelected(void);
static void SearchEnsureSelectedVisible(void);
static int SearchVisibleRows(void);
static bool BelongsToMapWindow(Uint32 windowId);
// A right-click on a cell the map shows opens one menu. Its first rows are the player's notes:
// a hidden-pit mark for a floor tile, LINK FROM HERE (link mode) for a floor tile or a wall
// face, and REMOVE LINKS for a cell a player link starts or ends on. Set map_debug = 1 in the settings and the testing rows follow behind a separator, on any
// cell: teleport the party there or spawn a monster there. A spawn asks for the monster, then how
// many (skipped when only one fits a cell), for one alone its spot on the cell (skipped when it
// fills the cell), and the facing. The menu rows hold the cell they act on; it closes with any
// click outside an enabled row, the wheel, Escape or a change of the shown level.
enum DebugMenuRowKind
{
  DebugRowTeleport, DebugRowMonster, DebugRowCount, DebugRowSpot, DebugRowFacing,
  // The player's notes; choice is the mark (-1 none, 0 closed, 1 open) or the link's side
  MenuRowPitMark, MenuRowLinkFromHere, MenuRowRemoveLinks,
  // A line between the note rows and the debug rows: never chosen
  MenuRowSeparator
};
struct DebugMenuRow
{
  DebugMenuRowKind kind;
  // The monster, the count, the spot of AutoMapScene_SpawnSpots or the facing (-1 any) the
  // row picks
  int choice;
  std::string label;
  bool isEnabled;
};
static bool debugMenuOn = false;
static bool debugMenuOpen = false;
static int debugMenuLevel = 0;
static int debugMenuX = 0;
static int debugMenuY = 0;
static int debugMenuPixelX = 0;
static int debugMenuPixelY = 0;
static std::string debugMenuTitle;
static std::vector<DebugMenuRow> debugMenuRows;
// The spawn picked so far: the monster, how many and the spot of one alone
static AutoMapMonsterChoice debugMenuMonster;
static int debugMenuCount = 1;
static AutoMapSpawnSpot debugMenuSpot;
static int debugMenuHovered = -1;
static void DebugMenuClose(void);
// Link mode, entered from LINK FROM HERE: each left-click on a shown cell adds it to the
// source's player links or takes it off again. The source is a level cell of the shown level and
// a side (-1 for a floor tile). Esc, a right-click or a change of the shown level ends it.
static bool linkModeOn = false;
static int linkModeLevel = 0;
static int linkModeX = 0;
static int linkModeY = 0;
static int linkModeSide = -1;
static void LinkModeEnd(void);

bool AutoMapWindow_DebugIsOn(void)
{
  return debugMenuOn;
}

// With map_debug the + button beside search opens a panel of testing aids: the champions'
// base skills (a click on a skill gives that champion one level in it), the light the 3D
// view is shown in, and forgetting what was explored or learned. Only one of the legend,
// search and this panel is open at a time.
static bool levelPanelOpen = false;
static int levelPanelHoveredRow = -1;
static int levelPanelHoveredColumn = -1;
static void LevelPanelClose(void);

// ---------------------------------------------------------------------------------------------
// Settings

void AutoMapWindow_ReadSettings(void)
{
  timingReportsOn = csb_get_config_int(SETTINGS_SECTION, "map_timing", 0) != 0;
  debugMenuOn = csb_get_config_int(SETTINGS_SECTION, "map_debug", 0) != 0;
  wasShownWhenSettingsWereRead = csb_get_config_int(SETTINGS_SECTION, "map_window", 0) != 0;
  placement.x = csb_get_config_int(SETTINGS_SECTION, "map_x", SDL_WINDOWPOS_UNDEFINED);
  placement.y = csb_get_config_int(SETTINGS_SECTION, "map_y", SDL_WINDOWPOS_UNDEFINED);
  placement.width = std::max(SMALLEST_SIZE, csb_get_config_int(SETTINGS_SECTION, "map_width", DEFAULT_SIZE));
  placement.height = std::max(SMALLEST_SIZE, csb_get_config_int(SETTINGS_SECTION, "map_height", DEFAULT_SIZE));
  // The hidden things are kept, so that whatever a later version adds starts out shown
  unsigned everything = (1u << AUTOMAP_SHOWN_COUNT) - 1;
  shownThings = everything & ~(unsigned)csb_get_config_int(SETTINGS_SECTION, "map_hide", 0);
}

static void RememberPlacement(void)
{
  if (mapWindow == NULL || !isShown) return;
  SDL_GetWindowPosition(mapWindow, &placement.x, &placement.y);
  SDL_GetWindowSize(mapWindow, &placement.width, &placement.height);
}

void AutoMapWindow_WriteSettings(void)
{
  RememberPlacement();
  csb_set_config_int(SETTINGS_SECTION, "map_window", isShown ? 1 : 0);
  csb_set_config_int(SETTINGS_SECTION, "map_x", placement.x);
  csb_set_config_int(SETTINGS_SECTION, "map_y", placement.y);
  csb_set_config_int(SETTINGS_SECTION, "map_width", placement.width);
  csb_set_config_int(SETTINGS_SECTION, "map_height", placement.height);
  unsigned everything = (1u << AUTOMAP_SHOWN_COUNT) - 1;
  csb_set_config_int(SETTINGS_SECTION, "map_hide", (int)(everything & ~shownThings));
}

static bool Shows(AutoMapShown what)
{
  return (shownThings & (1u << what)) != 0;
}

// The Show entry of each AutoMapItemKind
static AutoMapShown ShownEntryOfItemKind(AutoMapItemKind kind)
{
  switch (kind)
  {
  case AutoMapItemScroll: return AutoMapShowScrolls;
  case AutoMapItemChest: return AutoMapShowChests;
  case AutoMapItemPotion: return AutoMapShowPotions;
  case AutoMapItemWeapon: return AutoMapShowWeapons;
  case AutoMapItemArmour: return AutoMapShowArmour;
  case AutoMapItemFood: return AutoMapShowFood;
  case AutoMapItemKey: return AutoMapShowKeys;
  default: return AutoMapShowOtherItems;
  }
}

// The item kinds shown, as AutoMapScene_ItemsOfKinds takes them
static unsigned ShownItemKinds(void)
{
  unsigned kinds = 0;
  for (int kind = 0; kind < AUTOMAP_ITEM_KIND_COUNT; kind++)
  {
    if (Shows(ShownEntryOfItemKind((AutoMapItemKind)kind))) kinds |= 1u << kind;
  }
  return kinds;
}

bool AutoMapWindow_Shows(AutoMapShown what)
{
  return Shows(what);
}

void AutoMapWindow_SetShows(AutoMapShown what, bool isShown)
{
  if (isShown) shownThings |= 1u << what;
  else shownThings &= ~(1u << what);
  // The static layer is cached per draw list: a new serial paints it again
  drawListSerial++;
  isDirty = true;
}

void AutoMapWindow_RestoreVisibility(void)
{
  if (wasShownWhenSettingsWereRead) AutoMapWindow_Show();
}

// ---------------------------------------------------------------------------------------------
// Showing and hiding

static void ReportThatTheWindowCannotBeCreated(void)
{
  std::string message = std::string("The map window cannot be opened:\n") + SDL_GetError();
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Map", message.c_str(), NULL);
}

static void DestroyMapWindow(void)
{
  if (staticCache != NULL) SDL_DestroyTexture(staticCache);
  if (mapRenderer != NULL) SDL_DestroyRenderer(mapRenderer);
  if (mapWindow != NULL) SDL_DestroyWindow(mapWindow);
  staticCache = NULL;
  mapRenderer = NULL;
  mapWindow = NULL;
}

static bool CreateMapWindow(void)
{
  Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
  mapWindow = SDL_CreateWindow("Map", placement.x, placement.y, placement.width, placement.height, flags);
  if (mapWindow != NULL) mapRenderer = SDL_CreateRenderer(mapWindow, -1, 0);
  if (mapRenderer == NULL)
  {
    ReportThatTheWindowCannotBeCreated();
    DestroyMapWindow();
    return false;
  }
  SDL_SetWindowMinimumSize(mapWindow, SMALLEST_SIZE, SMALLEST_SIZE);
  SDL_SetRenderDrawBlendMode(mapRenderer, SDL_BLENDMODE_BLEND);
  sprites.UseRenderer(mapRenderer);
  if (MapTimingOn())
  {
    SDL_RendererInfo info;
    SDL_GetRendererInfo(mapRenderer, &info);
    printf("Automap timing: renderer %s flags 0x%x max texture %dx%d\n", info.name, info.flags,
           info.max_texture_width, info.max_texture_height);
  }
  return true;
}

bool AutoMapWindow_IsShown(void)
{
  return isShown;
}

void AutoMapWindow_Show(void)
{
  if (isShown) return;
  if (mapWindow == NULL && !CreateMapWindow()) return;
  SDL_Window *windowWithKeyboard = SDL_GetKeyboardFocus();
  SDL_ShowWindow(mapWindow);
  // The keys keep steering the game
  if (windowWithKeyboard != NULL) SDL_RaiseWindow(windowWithKeyboard);
  isShown = true;
  isDirty = true;
  drawListWasPainted = true;
}

static void GiveThePointerBackToTheGame(void)
{
  if (!pointerIsOverMap) return;
  pointerIsOverMap = false;
  SDL_ShowCursor(pointerStateOfTheGame);
}

void AutoMapWindow_Hide(void)
{
  if (!isShown) return;
  RememberPlacement();
  GiveThePointerBackToTheGame();
  CancelMiddleDrag();
  ClearHover();
  if (searchOpen)
  {
    searchOpen = false;
    searchQuery.clear();
    searchHits.clear();
    searchSelected = 0;
    searchTop = 0;
    SearchGiveBackTextInput();
  }
  SDL_HideWindow(mapWindow);
  isShown = false;
}

// ---------------------------------------------------------------------------------------------
// The draw list

bool AutoMapWindow_WantsNewDrawList(void)
{
  return isShown && drawListWasPainted;
}

static void ShowLevelInTitle(int level)
{
  if (mapWindow == NULL) return;
  std::string title = "Map";
  if (level != AUTOMAP_NO_LEVEL) title += ": Level " + std::to_string(level);
  SDL_SetWindowTitle(mapWindow, title.c_str());
}

static void ClearHover(void)
{
  if (!hoverValid) return;
  hoverValid = false;
  hoverX = -1;
  hoverY = -1;
  hoverPixelX = 0;
  hoverPixelY = 0;
  isDirty = true;
}

void AutoMapWindow_ShowDrawList(const AutoMapDrawList &drawList)
{
  if (drawList.level != shownDrawList.level)
  {
    sprites.ForgetAll();
    ShowLevelInTitle(drawList.level);
    ClearHover();
    DebugMenuClose();
    LinkModeEnd();
  }
  sprites.UseColourSchemes(drawList.paletteScheme9, drawList.paletteScheme10);
  shownDrawList = drawList;
  drawListSerial++;
  isDirty = true;
  drawListWasPainted = false;
  if (searchOpen) SearchRefreshCurrentLevel();
}

// ---------------------------------------------------------------------------------------------
// Item search

static SDL_Rect SearchButtonRect(void)
{
  SDL_Rect button = {30, 3, 24, 24};
  return button;
}

static SDL_Rect LevelButtonRect(void)
{
  SDL_Rect button = {57, 3, 24, 24};
  return button;
}

// The search and level panels start right of the last button
static int PanelLeft(void)
{
  return debugMenuOn ? 84 : 57;
}

static void SearchFullRefresh(void)
{
  searchHits.clear();
  searchSelected = 0;
  searchTop = 0;
  if (!searchOpen || searchQuery.empty()) return;
  searchHits = AutoMap_SearchDungeon(searchQuery);
  isDirty = true;
}

void AutoMapWindow_RefreshSearch(void)
{
  SearchFullRefresh();
}

// The per-tick rebuild brings a fresh draw list for the shown level: re-search
// just that level and swap its hits into the dungeon-wide list, keeping the
// selection when the cell is still there. Cheap (one level), so the live map
// and the results never disagree while the search stays open.
static void SearchRefreshCurrentLevel(void)
{
  if (!searchOpen || searchQuery.empty()) return;
  std::vector<AutoMapSearchHit> fresh = AutoMapScene_SearchItems(
      shownDrawList, searchQuery, AutoMapLevelReader_ItemName, AutoMapLevelReader_ItemIsFood);
  int selectedLevel = -1, selectedX = -1, selectedY = -1;
  std::string selectedName;
  bool selectedCarried = false;
  AutoMapWallFeature selectedFeature = AutoMapWallFeatureNone;
  int selectedSide = -1;
  // The refresh runs with every new draw list: it must not undo the wheel, so it keeps the
  // selection in view only when the selection was in view
  bool selectedWasShown = searchSelected >= searchTop && searchSelected < searchTop + SearchVisibleRows();
  if (!searchHits.empty() && searchSelected >= 0 && searchSelected < (int)searchHits.size())
  {
    const AutoMapSearchHit &old = searchHits[(size_t)searchSelected];
    selectedLevel = old.level;
    selectedX = old.x;
    selectedY = old.y;
    selectedName = old.name;
    selectedCarried = old.monsterCarried;
    selectedFeature = old.wallFeature;
    selectedSide = old.slot.side;
  }
  std::vector<AutoMapSearchHit> merged;
  for (size_t i = 0; i < searchHits.size(); i++)
  {
    if (searchHits[i].level != shownDrawList.level) merged.push_back(searchHits[i]);
  }
  merged.insert(merged.end(), fresh.begin(), fresh.end());
  AutoMapScene_SortSearchHits(&merged, searchQuery);
  searchHits = merged;
  searchSelected = 0;
  for (size_t i = 0; i < searchHits.size(); i++)
  {
    if (searchHits[i].level == selectedLevel && searchHits[i].x == selectedX
        && searchHits[i].y == selectedY && searchHits[i].name == selectedName
        && searchHits[i].monsterCarried == selectedCarried
        && searchHits[i].wallFeature == selectedFeature && searchHits[i].slot.side == selectedSide)
    {
      searchSelected = (int)i;
      break;
    }
  }
  if (selectedWasShown)
  {
    SearchEnsureSelectedVisible();
  }
  else
  {
    int maxTop = (int)searchHits.size() - SearchVisibleRows();
    if (searchTop > maxTop) searchTop = maxTop;
    if (searchTop < 0) searchTop = 0;
  }
  isDirty = true;
}

static void SearchGiveBackTextInput(void)
{
  if (searchStartedTextInput) SDL_StopTextInput();
  searchStartedTextInput = false;
}

static void SearchOpenBox(void)
{
  legendPage = 0;
  LevelPanelClose();
  searchOpen = true;
  searchQuery.clear();
  searchHits.clear();
  searchSelected = 0;
  searchTop = 0;
  searchStartedTextInput = !SDL_IsTextInputActive();
  if (searchStartedTextInput) SDL_StartTextInput();
  isDirty = true;
}

static void SearchCloseBox(void)
{
  searchOpen = false;
  searchQuery.clear();
  searchHits.clear();
  searchSelected = 0;
  searchTop = 0;
  SearchGiveBackTextInput();
  isDirty = true;
}

static void SearchCenterOnSelected(void)
{
  if (!searchOpen || searchHits.empty()) return;
  if (searchSelected < 0 || searchSelected >= (int)searchHits.size()) searchSelected = 0;
  const AutoMapSearchHit &hit = searchHits[(size_t)searchSelected];
  // A hit on another level switches the map there first; the per-tick rebuild
  // brings the level within a moment and the frozen centre lands on the cell.
  if (hit.level != AUTOMAP_NO_LEVEL && hit.level != shownDrawList.level) AutoMap_ShowLevel(hit.level);
  viewFollowsParty = false;
  viewCenterX = (float)hit.x;
  viewCenterY = (float)hit.y;
  isDirty = true;
}

static void SearchEnsureSelectedVisible(void)
{
  int rows = SearchVisibleRows();
  if (searchSelected < searchTop) searchTop = searchSelected;
  if (searchSelected >= searchTop + rows) searchTop = searchSelected - rows + 1;
  int maxTop = (int)searchHits.size() - rows;
  if (maxTop < 0) maxTop = 0;
  if (searchTop > maxTop) searchTop = maxTop;
  if (searchTop < 0) searchTop = 0;
}

static Uint32 KeyWindowOf(const SDL_Event &event)
{
  if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) return event.key.windowID;
  if (event.type == SDL_TEXTINPUT) return event.text.windowID;
  return 0;
}

static SDL_Rect SearchPanelRect(void);
static bool SearchHitAtPixel(int pixelX, int pixelY, int *hitIndex);

bool AutoMapWindow_HandleKeyEvent(const SDL_Event &event)
{
  if (event.type != SDL_KEYDOWN && event.type != SDL_TEXTINPUT) return false;
  if (!isShown || !BelongsToMapWindow(KeyWindowOf(event))) return false;
  if (event.type == SDL_TEXTINPUT)
  {
    if (!searchOpen) return false;
    searchQuery += event.text.text;
    SearchFullRefresh();
    return true;
  }
  SDL_Keycode sym = event.key.keysym.sym;
  bool ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;
  if (debugMenuOpen && sym == SDLK_ESCAPE)
  {
    DebugMenuClose();
    return true;
  }
  if (linkModeOn && sym == SDLK_ESCAPE)
  {
    LinkModeEnd();
    return true;
  }
  if (levelPanelOpen && sym == SDLK_ESCAPE)
  {
    LevelPanelClose();
    return true;
  }
  if (AutoMapWindow_HandleLevelKey(sym)) return true;
  if (!searchOpen)
  {
    if (sym == SDLK_SLASH || (ctrl && sym == SDLK_f))
    {
      SearchOpenBox();
      return true;
    }
    return false;
  }
  if (sym == SDLK_ESCAPE)
  {
    SearchCloseBox();
    return true;
  }
  if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER)
  {
    SearchCenterOnSelected();
    return true;
  }
  if (sym == SDLK_BACKSPACE)
  {
    if (!searchQuery.empty()) searchQuery.erase(searchQuery.size() - 1);
    SearchFullRefresh();
    return true;
  }
  if (sym == SDLK_UP)
  {
    if (!searchHits.empty())
    {
      searchSelected = (searchSelected - 1 + (int)searchHits.size()) % (int)searchHits.size();
      SearchEnsureSelectedVisible();
      SearchCenterOnSelected();
    }
    return true;
  }
  if (sym == SDLK_DOWN)
  {
    if (!searchHits.empty())
    {
      searchSelected = (searchSelected + 1) % (int)searchHits.size();
      SearchEnsureSelectedVisible();
      SearchCenterOnSelected();
    }
    return true;
  }
  // Printable keys arrive as TEXTINPUT; swallow KEYDOWN so the game does not
  // walk while the player types.
  return true;
}

// ---------------------------------------------------------------------------------------------
// Events

static bool BelongsToMapWindow(Uint32 windowId)
{
  return mapWindow != NULL && windowId == SDL_GetWindowID(mapWindow);
}

static void TakeThePointerFromTheGame(void)
{
  if (pointerIsOverMap) return;
  pointerIsOverMap = true;
  pointerStateOfTheGame = SDL_ShowCursor(SDL_QUERY);
  SDL_ShowCursor(SDL_ENABLE);
}

bool AutoMapWindow_HandleWindowEvent(const SDL_Event &event)
{
  if (event.type != SDL_WINDOWEVENT || !BelongsToMapWindow(event.window.windowID)) return false;
  switch (event.window.event)
  {
  case SDL_WINDOWEVENT_CLOSE: AutoMapWindow_Hide(); break;
  case SDL_WINDOWEVENT_ENTER:
    TakeThePointerFromTheGame();
    if (middleDragging) dragNeedsResync = true;
    break;
  case SDL_WINDOWEVENT_LEAVE:
    GiveThePointerBackToTheGame();
    ClearHover();
    if (middleDragging) dragNeedsResync = true;
    break;
  case SDL_WINDOWEVENT_FOCUS_LOST: CancelMiddleDrag(); ClearHover(); break;
  default: isDirty = true; break;
  }
  return true;
}

static bool IsPointerEventOfMapWindow(const SDL_Event &event)
{
  switch (event.type)
  {
  case SDL_MOUSEMOTION: return BelongsToMapWindow(event.motion.windowID);
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP: return BelongsToMapWindow(event.button.windowID);
  case SDL_MOUSEWHEEL: return BelongsToMapWindow(event.wheel.windowID);
  default: return false;
  }
}

static void RecenterViewOnParty(void)
{
  viewFollowsParty = true;
  isDirty = true;
}

void AutoMapWindow_RecenterView(void)
{
  RecenterViewOnParty();
}

static void MapAreaSize(int *mapWidth, int *mapHeight)
{
  int windowWidth = 0, windowHeight = 0;
  if (mapRenderer != NULL) SDL_GetRendererOutputSize(mapRenderer, &windowWidth, &windowHeight);
  *mapWidth = windowWidth - 2 * RULER;
  *mapHeight = windowHeight - 2 * RULER;
}

static AutoMapView ViewOfShownLevel(void);

// The exact view centre in cells, so that switching from the party to the pan — and any
// later re-pan or zoom step — keeps the view still
static void FreezeExactViewCenter(const AutoMapView &view)
{
  int mapWidth = 0, mapHeight = 0;
  MapAreaSize(&mapWidth, &mapHeight);
  if (view.cellSize <= 0 || mapWidth <= 0 || mapHeight <= 0)
  {
    viewCenterX = 0;
    viewCenterY = 0;
    return;
  }
  viewCenterX = (mapWidth / 2.0f - (view.originX - RULER)) / view.cellSize - 0.5f;
  viewCenterY = (mapHeight / 2.0f - (view.originY - RULER)) / view.cellSize - 0.5f;
}

static int gestureMotions = 0;

static void ReportGesture(const char *outcome)
{
  if (!MapTimingOn()) return;
  printf("Automap gesture: %s after %d motions, centre %.2f,%.2f, following %d\n", outcome,
         gestureMotions, viewCenterX, viewCenterY, viewFollowsParty ? 1 : 0);
}

// A drag that ends anywhere but on the map (release outside, focus lost, window hidden)
// ends silently: the view stays where the drag left it
static void LeaveRelativeMode(void)
{
  SDL_SetRelativeMouseMode(SDL_FALSE);
  SDL_CaptureMouse(SDL_FALSE);
}

static void CancelMiddleDrag(void)
{
  if (!middleDragging) return;
  ReportGesture("cancelled");
  middleDragging = false;
  LeaveRelativeMode();
}

static void UpdateHoverFromPixel(int pixelX, int pixelY);
static bool CellAtPixel(int pixelX, int pixelY, int *cellX, int *cellY);

static void StepLevelCenteredOn(int delta, int cellX, int cellY);

static void StartMiddleDrag(void)
{
  // A bouncing wheel button sends DOWN twice: never restart a live drag
  if (middleDragging)
  {
    if (MapTimingOn()) printf("Automap gesture: down ignored, already dragging\n");
    return;
  }
  ClearHover();
  middleDragging = true;
  dragLeftTheStart = false;
  dragNeedsResync = true;
  dragDistance = 0;
  gestureMotions = 0;
  // Relative deltas never pin at the screen edge the way an absolute cursor does:
  // circles stay circles even against the edge or in a corner. The cursor hides meanwhile.
  SDL_SetRelativeMouseMode(SDL_TRUE);
  // The cursor roams off the window mid-drag: keep the motion events flowing
  SDL_CaptureMouse(SDL_TRUE);
}

static void DragMiddle(int deltaX, int deltaY)
{
  if (!middleDragging) return;
  gestureMotions++;
  // A gap in the motion stream (re-enter without capture) carries a stale jump
  if (dragNeedsResync)
  {
    dragNeedsResync = false;
    return;
  }
  AutoMapView view = ViewOfShownLevel();
  if (view.cellSize <= 0) return;
  dragDistance += (deltaX < 0 ? -deltaX : deltaX) + (deltaY < 0 ? -deltaY : deltaY);
  if (!dragLeftTheStart && dragDistance < DRAG_START_DISTANCE) return;
  if (!dragLeftTheStart)
  {
    dragLeftTheStart = true;
    FreezeExactViewCenter(view);
    viewFollowsParty = false;
    // Stay on the level on screen while roaming elsewhere: panning locks the level,
    // a middle-click without a drag hands both back to the party
    if (shownDrawList.level != AUTOMAP_NO_LEVEL) AutoMap_ShowLevel(shownDrawList.level);
  }
  viewCenterX -= (float)deltaX / view.cellSize;
  viewCenterY -= (float)deltaY / view.cellSize;
  isDirty = true;
}

static void EndMiddleDrag(void)
{
  if (!middleDragging) return;
  ReportGesture(dragLeftTheStart ? "panned" : "reclicked");
  middleDragging = false;
  LeaveRelativeMode();
  if (!dragLeftTheStart)
  {
    RecenterViewOnParty();
    AutoMap_FollowParty();
  }
}

static void StepLevelCenteredOn(int delta, int cellX, int cellY)
{
  if (shownDrawList.level == AUTOMAP_NO_LEVEL) return;
  int levelCount = AutoMap_LevelCount();
  if (levelCount <= 0) return;
  int current = shownDrawList.level;
  int target = current + delta;
  if (target < 0) target = 0;
  if (target >= levelCount) target = levelCount - 1;
  int globalX = 0, globalY = 0;
  AutoMapScene_GlobalPosition(shownDrawList, cellX, cellY, &globalX, &globalY);
  // The target's draw list counts its cells from the rock the reader adds around it, so its
  // offset is the snapshot's, not the level's own (AutoMapScene_GlobalPosition)
  AutoMapLevelSnapshot targetSnapshot = AutoMapLevelReader_ReadLevel(target);
  viewFollowsParty = false;
  viewCenterX = (float)(globalX - targetSnapshot.offsetX);
  viewCenterY = (float)(globalY - targetSnapshot.offsetY);
  if (target != current) AutoMap_ShowLevel(target);
  isDirty = true;
  if (MapTimingOn())
  {
    printf("Automap gesture: level %d -> %d centred on (%d,%d)\n", current, target, cellX,
           cellY);
  }
}

// Page Up and Page Down step one level over the spot in the middle of the view, the way the
// side buttons do over the hovered tile
bool AutoMapWindow_HandleLevelKey(SDL_Keycode sym)
{
  if (sym != SDLK_PAGEUP && sym != SDLK_PAGEDOWN) return false;
  if (!isShown || shownDrawList.level == AUTOMAP_NO_LEVEL) return false;
  int cellX = shownDrawList.width / 2;
  int cellY = shownDrawList.height / 2;
  if (!viewFollowsParty)
  {
    cellX = (int)std::floor(viewCenterX + 0.5f);
    cellY = (int)std::floor(viewCenterY + 0.5f);
  }
  else if (shownDrawList.partyIsOnShownLevel)
  {
    cellX = shownDrawList.party.x;
    cellY = shownDrawList.party.y;
  }
  StepLevelCenteredOn(sym == SDLK_PAGEDOWN ? 1 : -1, cellX, cellY);
  return true;
}

static bool DebugMenuHandlePointerEvent(const SDL_Event &event);
static void LinkModeClickAt(int pixelX, int pixelY);
static bool LevelPanelHandlePointerEvent(const SDL_Event &event);

bool AutoMapWindow_HandlePointerEvent(const SDL_Event &event)
{
  // A release outside the map ends the drag silently and belongs to the game
  if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_MIDDLE &&
      !IsPointerEventOfMapWindow(event))
  {
    CancelMiddleDrag();
    return false;
  }
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_MIDDLE &&
      !IsPointerEventOfMapWindow(event) && !middleDragging && MapTimingOn())
  {
    printf("Automap gesture: down outside the map window\n");
  }
  if (!IsPointerEventOfMapWindow(event)) return false;
  // A right-click ends link mode and opens no menu
  if (linkModeOn && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT)
  {
    LinkModeEnd();
    return true;
  }
  if (DebugMenuHandlePointerEvent(event)) return true;
  if (LevelPanelHandlePointerEvent(event)) return true;
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
  {
    if (PointInRect(event.button.x, event.button.y, SearchButtonRect()))
    {
      if (searchOpen) SearchCloseBox();
      else SearchOpenBox();
      return true;
    }
    if (searchOpen)
    {
      int hitIndex = -1;
      if (SearchHitAtPixel(event.button.x, event.button.y, &hitIndex))
      {
        searchSelected = hitIndex;
        SearchCenterOnSelected();
        return true;
      }
      // A click into the open panel but off any hit stays in the panel
      if (PointInRect(event.button.x, event.button.y, SearchPanelRect())) return true;
    }
    // The legend ? button and the open panel share one behaviour: advance the page.
    bool onButton = PointInRect(event.button.x, event.button.y, LegendButtonRect());
    bool onPanel = legendPage != 0
                   && PointInRect(event.button.x, event.button.y, LegendPanelRect());
    // In link mode a click on the map picks a target; the buttons and panels keep theirs
    if (linkModeOn && !onButton && !onPanel)
    {
      LinkModeClickAt(event.button.x, event.button.y);
      return true;
    }
    if (onButton || onPanel)
    {
      if (legendPage == 0)
      {
        if (searchOpen) SearchCloseBox();
        LevelPanelClose();
      }
      legendPage = (legendPage + 1) % 7;
      // Skip pages the current spoiler level leaves empty; page 0 closes the legend
      while (legendPage != 0 && !LegendPageHasRows(legendPage)) legendPage = (legendPage + 1) % 7;
      isDirty = true;
    }
    return true;
  }
  if (event.type == SDL_MOUSEBUTTONDOWN
      && (event.button.button == SDL_BUTTON_X1 || event.button.button == SDL_BUTTON_X2))
  {
    // Side buttons step one level, centred on the hovered tile: back (X1) goes
    // down a level, forward (X2) goes up. At the top or bottom level the view
    // still recentres on the tile.
    if (!middleDragging)
    {
      UpdateHoverFromPixel(event.button.x, event.button.y);
      int cellX = -1, cellY = -1;
      if (hoverValid)
      {
        cellX = hoverX;
        cellY = hoverY;
      }
      else
      {
        CellAtPixel(event.button.x, event.button.y, &cellX, &cellY);
      }
      if (cellX >= 0 && cellY >= 0 && shownDrawList.level != AUTOMAP_NO_LEVEL)
      {
        int delta = event.button.button == SDL_BUTTON_X1 ? 1 : -1;
        StepLevelCenteredOn(delta, cellX, cellY);
      }
    }
    return true;
  }
  bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
  if (event.type == SDL_MOUSEWHEEL && (event.wheel.y != 0 || (shiftHeld && event.wheel.x != 0)))
  {
    // macOS sends Shift+wheel as a horizontal scroll: wheel.y 0, and wheel.x of the opposite sign
    // (SDL negates the Cocoa deltaX, not the deltaY), so -wheel.x stands in for wheel.y there
    int notches = event.wheel.y != 0 ? event.wheel.y : -event.wheel.x;
    // The wheel scrolls the results panel when the pointer sits over it,
    // zooms otherwise. Wheel events carry no pointer position of their own,
    // so the last hovered pixel decides.
    if (shiftHeld && hoverValid && tooltipScrollMax > 0)
    {
      // A tall tooltip under Shift: one footer line per notch, and no zoom
      tooltipScrollLines += notches > 0 ? -1 : 1;
      if (tooltipScrollLines < 0) tooltipScrollLines = 0;
      if (tooltipScrollLines > tooltipScrollMax) tooltipScrollLines = tooltipScrollMax;
      isDirty = true;
    }
    else if (event.wheel.y == 0)
    {
      // A sideways scroll only ever scrolls the tooltip
    }
    else if (searchOpen && PointInRect(hoverPixelX, hoverPixelY, SearchPanelRect()))
    {
      int rows = SearchVisibleRows();
      int maxTop = (int)searchHits.size() > rows ? (int)searchHits.size() - rows : 0;
      searchTop -= event.wheel.y;
      if (searchTop < 0) searchTop = 0;
      if (searchTop > maxTop) searchTop = maxTop;
      isDirty = true;
    }
    else
    {
      zoomSteps = AutoMapScene_ClampZoomSteps(zoomSteps + (event.wheel.y > 0 ? 1 : -1));
      isDirty = true;
    }
  }
  else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_MIDDLE)
  {
    StartMiddleDrag();
  }
  else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_MIDDLE)
  {
    EndMiddleDrag();
  }
  else if (event.type == SDL_MOUSEMOTION && middleDragging)
  {
    DragMiddle(event.motion.xrel, event.motion.yrel);
  }
  else if (event.type == SDL_MOUSEMOTION && !middleDragging)
  {
    UpdateHoverFromPixel(event.motion.x, event.motion.y);
  }
  return true;
}

bool AutoMapWindow_PointerIsOverMap(void)
{
  return pointerIsOverMap;
}

// ---------------------------------------------------------------------------------------------
// Drawing

static void UseColour(const SDL_Color &colour)
{
  SDL_SetRenderDrawColor(mapRenderer, colour.r, colour.g, colour.b, colour.a);
}

static void Fill(const SDL_Rect &area, const SDL_Color &colour)
{
  UseColour(colour);
  SDL_RenderFillRect(mapRenderer, &area);
}

static void Outline(const SDL_Rect &area, const SDL_Color &colour)
{
  UseColour(colour);
  SDL_RenderDrawRect(mapRenderer, &area);
}

static SDL_Rect Inset(const SDL_Rect &area, int percentOfWidth)
{
  int margin = area.w * percentOfWidth / 100;
  SDL_Rect inner = {area.x + margin, area.y + margin, area.w - 2 * margin, area.h - 2 * margin};
  return inner;
}

static SDL_Rect CellArea(const AutoMapView &view, int x, int y)
{
  SDL_Rect area = {view.originX + x * view.cellSize, view.originY + y * view.cellSize,
                   view.cellSize, view.cellSize};
  return area;
}

static float Lerp(float from, float to, float t)
{
  return from + (to - from) * t;
}

static float PartOfTheWay(float from, float to, float position)
{
  if (to == from) return 0;
  return std::max(0.0f, std::min((position - from) / (to - from), 1.0f));
}

// SDL_RenderGeometry needs SDL 2.0.18; horizontal lines work with every SDL2
static void FillTriangle(FloatPoint top, FloatPoint middle, FloatPoint bottom, const SDL_Color &colour)
{
  if (top.y > middle.y) std::swap(top, middle);
  if (middle.y > bottom.y) std::swap(middle, bottom);
  if (top.y > middle.y) std::swap(top, middle);
  UseColour(colour);
  for (int y = (int)top.y; y <= (int)bottom.y; y++)
  {
    bool isBelowMiddle = y > middle.y;
    FloatPoint from = isBelowMiddle ? middle : top;
    FloatPoint to = isBelowMiddle ? bottom : middle;
    float onLongEdge = Lerp(top.x, bottom.x, PartOfTheWay(top.y, bottom.y, (float)y));
    float onShortEdge = Lerp(from.x, to.x, PartOfTheWay(from.y, to.y, (float)y));
    SDL_RenderDrawLine(mapRenderer, (int)onLongEdge, y, (int)onShortEdge, y);
  }
}

// A point of a shape that is given looking east, turned to look in another direction
static FloatPoint TurnedPointInCell(const SDL_Rect &cell, const FloatPoint &whenLookingEast,
                                    AutoMapDirection direction)
{
  float fromCentreX = whenLookingEast.x - 0.5f;
  float fromCentreY = whenLookingEast.y - 0.5f;
  FloatPoint turned = {fromCentreX, fromCentreY};
  if (direction == AutoMapSouth) { turned.x = -fromCentreY; turned.y = fromCentreX; }
  if (direction == AutoMapWest) { turned.x = -fromCentreX; turned.y = -fromCentreY; }
  if (direction == AutoMapNorth) { turned.x = fromCentreY; turned.y = -fromCentreX; }
  FloatPoint onScreen = {cell.x + (turned.x + 0.5f) * cell.w, cell.y + (turned.y + 0.5f) * cell.h};
  return onScreen;
}

static void DrawFloor(const SDL_Rect &cell)
{
  Fill(cell, FLOOR);
  Outline(cell, FLOOR_EDGE);
}

// The bar lies across the direction of travel and opens from the middle
static void DoorLeafRects(const SDL_Rect &cell, bool runsNorthSouth, AutoMapDoorState state,
                          SDL_Rect *firstLeaf, SDL_Rect *secondLeaf)
{
  int thickness = cell.w / 5;
  int leafLength = cell.w * (100 - DOOR_GAP_PERCENT[state]) / 200;
  SDL_Rect first = {cell.x, cell.y + (cell.h - thickness) / 2, leafLength, thickness};
  SDL_Rect second = {cell.x + cell.w - leafLength, first.y, leafLength, thickness};
  if (!runsNorthSouth)
  {
    SDL_Rect upper = {cell.x + (cell.w - thickness) / 2, cell.y, thickness, leafLength};
    SDL_Rect lower = {upper.x, cell.y + cell.h - leafLength, thickness, leafLength};
    first = upper;
    second = lower;
  }
  *firstLeaf = first;
  *secondLeaf = second;
}

// Which tools break a door, bottom-left like every other annotation. Axe reads as a chop
// cross, fireball as a hollow orb; destroyed doors need no tools and stay unmarked.
static void DrawHollowCircle(float centreX, float centreY, float radius)
{
  if (radius < 2.0f) return;
  UseColour(WHITE);
  float prevX = centreX + radius;
  float prevY = centreY;
  for (int i = 1; i <= 12; i++)
  {
    float angle = 2.0f * 3.14159265f * i / 12;
    float x = centreX + radius * cos(angle);
    float y = centreY + radius * sin(angle);
    SDL_RenderDrawLine(mapRenderer, (int)prevX, (int)prevY, (int)x, (int)y);
    prevX = x;
    prevY = y;
  }
}

static void DrawBreakAxeMark(const SDL_Rect &box)
{
  UseColour(WHITE);
  SDL_RenderDrawLine(mapRenderer, box.x, box.y, box.x + box.w, box.y + box.h);
  SDL_RenderDrawLine(mapRenderer, box.x, box.y + box.h, box.x + box.w, box.y);
}

static void DrawDoorBreakMarks(const SDL_Rect &cell, const AutoMapCellRecord &door)
{
  if (door.doorState == AutoMapDoorDestroyed) return;
  bool axe = door.doorBreakableWithAxe;
  bool fire = door.doorBreakableWithFireball;
  if (!axe && !fire) return;
  SDL_Rect corner = {cell.x + cell.w * 8 / 100, cell.y + cell.h * 58 / 100,
                     cell.w * 30 / 100, cell.h * 34 / 100};
  if (axe && fire)
  {
    SDL_Rect axeBox = {corner.x, corner.y, corner.w * 45 / 100, corner.h};
    SDL_Rect fireBox = {corner.x + corner.w * 55 / 100, corner.y, corner.w * 45 / 100,
                        corner.h};
    DrawBreakAxeMark(axeBox);
    DrawHollowCircle(fireBox.x + fireBox.w / 2.0f, fireBox.y + fireBox.h / 2.0f,
                     std::min(fireBox.w, fireBox.h) / 2.0f - 1.0f);
    return;
  }
  if (axe)
  {
    DrawBreakAxeMark(corner);
    return;
  }
  DrawHollowCircle(corner.x + corner.w / 2.0f, corner.y + corner.h / 2.0f,
                   std::min(corner.w, corner.h) / 2.0f - 1.0f);
}

static void DrawDoor(const SDL_Rect &cell, const AutoMapCellRecord &door, bool sealed)
{
  DrawFloor(cell);
  const SDL_Color &colour = door.doorState == AutoMapDoorDestroyed
      ? DESTROYED_DOOR
      : (sealed ? SEALED_DOOR : DOOR);
  SDL_Rect firstLeaf, secondLeaf;
  DoorLeafRects(cell, door.runsNorthSouth, door.doorState, &firstLeaf, &secondLeaf);
  Fill(firstLeaf, colour);
  Fill(secondLeaf, colour);
}

// The steps lie across the direction of travel
static void DrawStairs(const SDL_Rect &cell, const AutoMapCellRecord &stairs)
{
  const SDL_Color &light = stairs.stairsLeadUp ? STAIRS_UP_LIGHT : STAIRS_DOWN_LIGHT;
  const SDL_Color &dark = stairs.stairsLeadUp ? STAIRS_UP_DARK : STAIRS_DOWN_DARK;
  for (int stripe = 0; stripe < STAIR_STRIPES; stripe++)
  {
    int start = cell.w * stripe / STAIR_STRIPES;
    int length = cell.w * (stripe + 1) / STAIR_STRIPES - start;
    SDL_Rect step = {cell.x, cell.y + start, cell.w, length};
    if (!stairs.runsNorthSouth)
    {
      SDL_Rect stepOfEastWestStairs = {cell.x + start, cell.y, length, cell.h};
      step = stepOfEastWestStairs;
    }
    Fill(step, stripe % 2 == 0 ? light : dark);
  }
}

static void DrawTeleporter(const SDL_Rect &cell)
{
  SDL_Rect haze = Inset(cell, 10);
  Fill(haze, TELEPORTER_HAZE);
  Outline(haze, TELEPORTER_EDGE);
}

static int NumberPixelWidth(int value);
static void DrawNumberCentred(int centreX, int y, int value);

// The destination level of a cross-level teleporter as a small centred plate,
// the same digits as the title bar. The static layer shows it on every firing
// cross-level teleporter whose target survived the fog filter; hovering draws
// the same plate over the arrow's source cell.
static void DrawLevelPlate(const SDL_Rect &cell, int level)
{
  int plateW = NumberPixelWidth(level) + 8;
  int plateH = 5 * DIGIT_SCALE + 8;
  SDL_Rect plate = {cell.x + (cell.w - plateW) / 2, cell.y + (cell.h - plateH) / 2, plateW, plateH};
  Fill(plate, BACKGROUND);
  Outline(plate, SPINNER);
  UseColour(SPINNER);
  DrawNumberCentred(cell.x + cell.w / 2, plate.y + 4, level);
}

static void DrawPitMarker(const SDL_Rect &cell)
{
  SDL_Rect marker = Inset(cell, 12);
  Fill(marker, PIT_MARKER);
  Outline(marker, PIT_MARKER_EDGE);
}

// The largest area with the proportions of the sprite that fits into the box, centred in it
static SDL_Rect FitInto(const SDL_Rect &box, const AutoMapSprite &sprite)
{
  float scale = std::min((float)box.w / sprite.width, (float)box.h / sprite.height);
  int width = (int)(sprite.width * scale);
  int height = (int)(sprite.height * scale);
  SDL_Rect area = {box.x + (box.w - width) / 2, box.y + (box.h - height) / 2, width, height};
  return area;
}

static const AutoMapSprite *SpriteOfTheGame(int picture)
{
  if (picture == AUTOMAP_NO_PICTURE) return NULL;
  AutoMapSpriteRequest request = {picture, TRANSPARENT_COLOUR_OF_THE_GAME};
  return sprites.Find(request);
}

static void DrawSprite(const AutoMapSprite &sprite, const SDL_Rect &area)
{
  SDL_RenderCopy(mapRenderer, sprite.texture, NULL, &area);
}

static void DrawSpriteTile(const AutoMapSprite &sprite, const SDL_Rect &area, const SDL_Rect &tile)
{
  SDL_RenderCopy(mapRenderer, sprite.texture, &tile, &area);
}

// The inventory tile of an item, the way the inventory draws objects: a 16x16
// tile of a basic graphic. NULL when the tile lies outside the decoded picture.
static const AutoMapSprite *SpriteOfIcon(const AutoMapItem &item, SDL_Rect *tile)
{
  if (!item.hasIcon) return NULL;
  tile->x = (item.iconTile & 15) * 16;
  tile->y = (item.iconTile >> 4) * 16;
  tile->w = 16;
  tile->h = 16;
  AutoMapSpriteRequest request = {item.iconGraphic, -1, true, tile->x, tile->y, tile->w, tile->h,
                                    true};
  const AutoMapSprite *sprite = sprites.Find(request);
  if (sprite == NULL) return NULL;
  if (tile->x + tile->w > sprite->width || tile->y + tile->h > sprite->height) return NULL;
  return sprite;
}

static void DrawFloorMarker(const SDL_Rect &cell, const AutoMapCellRecord &record)
{
  if (record.kind == AutoMapCellPit) DrawPitMarker(cell);
  else Fill(Inset(cell, 20), FLOOR_FEATURE_MARKER);
}

static void DrawFloorPicture(const SDL_Rect &cell, const AutoMapCellRecord &record)
{
  const AutoMapSprite *sprite = SpriteOfTheGame(record.floorPicture);
  if (sprite == NULL) DrawFloorMarker(cell, record);
  else DrawSprite(*sprite, FitInto(Inset(cell, 5), *sprite));
}

// Champion portrait tile (actuator type 127) the way the 3D view draws it:
// portraits 0..23 live in basic graphic 26 as 32x29 tiles, 8 per row
// (CSBCode.cpp DrawWallDecoration). Drawn with the shown level's palette like
// the viewport, transparent colour 1 like the 3D portrait pass. External
// portraits (>= 24, portrait files) have no file picture and read as frame
// only; the frame still shows.
static const int PORTRAIT_PICTURE = 26;
static const int PORTRAIT_TILE_W = 32;
static const int PORTRAIT_TILE_H = 29;
static const int PORTRAIT_TRANSPARENT = 1;
static const int PORTRAIT_COUNT_IN_GRAPHIC = 24;

static const AutoMapSprite *SpriteOfPortrait(int portrait, SDL_Rect *tile)
{
  if (portrait == AUTOMAP_NO_PICTURE) return NULL;
  if (portrait < 0 || portrait >= PORTRAIT_COUNT_IN_GRAPHIC) return NULL;
  tile->x = (portrait & 7) * PORTRAIT_TILE_W;
  tile->y = (portrait >> 3) * PORTRAIT_TILE_H;
  tile->w = PORTRAIT_TILE_W;
  tile->h = PORTRAIT_TILE_H;
  AutoMapSpriteRequest request = {PORTRAIT_PICTURE, PORTRAIT_TRANSPARENT, true,
                                  tile->x, tile->y, tile->w, tile->h, false};
  const AutoMapSprite *sprite = sprites.Find(request);
  if (sprite == NULL) return NULL;
  if (tile->x + tile->w > sprite->width || tile->y + tile->h > sprite->height) return NULL;
  return sprite;
}

// The face inside its frame, like the 3D view layers it over the wall
// decoration: a smaller centred tile so the frame stays visible around it.
static void DrawPortrait(const SDL_Rect &area, int portrait)
{
  SDL_Rect tile;
  const AutoMapSprite *sprite = SpriteOfPortrait(portrait, &tile);
  if (sprite == NULL) return;
  SDL_Rect box = Inset(area, 28);
  if (box.w <= 0 || box.h <= 0) return;
  float scale = std::min((float)box.w / tile.w, (float)box.h / tile.h);
  int width = (int)(tile.w * scale);
  int height = (int)(tile.h * scale);
  if (width <= 0 || height <= 0) return;
  SDL_Rect dst = {box.x + (box.w - width) / 2, box.y + (box.h - height) / 2, width, height};
  DrawSpriteTile(*sprite, dst, tile);
}

static bool HasFeature(const AutoMapWallSide &side)
{
  return side.picture != AUTOMAP_NO_PICTURE || side.hasWriting
      || side.portrait != AUTOMAP_NO_PICTURE;
}

static SDL_Rect HalfTowards(const SDL_Rect &cell, int side)
{
  SDL_Rect half = cell;
  if (side == AutoMapNorth || side == AutoMapSouth) half.h = cell.h / 2;
  if (side == AutoMapEast || side == AutoMapWest) half.w = cell.w / 2;
  if (side == AutoMapSouth) half.y = cell.y + cell.h - half.h;
  if (side == AutoMapEast) half.x = cell.x + cell.w - half.w;
  return half;
}

static int StripThickness(const SDL_Rect &cell)
{
  return std::max(2, cell.w / 15);
}

static SDL_Rect StripAlong(const SDL_Rect &cell, int side)
{
  int thickness = StripThickness(cell);
  SDL_Rect strip = {cell.x + thickness, cell.y, cell.w - 2 * thickness, thickness};
  if (side == AutoMapSouth) strip.y = cell.y + cell.h - thickness;
  if (side == AutoMapEast || side == AutoMapWest)
  {
    SDL_Rect upright = {cell.x, cell.y + thickness, thickness, cell.h - 2 * thickness};
    if (side == AutoMapEast) upright.x = cell.x + cell.w - thickness;
    strip = upright;
  }
  return strip;
}

// Writing has no picture of its own: a few lines stand for the text. The glyph lives in a
// square centred on its area so every side reads the same: halves towards north/south are
// wide and flat while halves towards east/west are narrow and tall, and insetting the half
// itself would collapse the north/south lines into one thin stroke. A full cell is already
// square and draws unchanged.
static SDL_Rect WritingSquare(const SDL_Rect &area)
{
  int side = std::min(area.w, area.h);
  SDL_Rect square = {area.x + (area.w - side) / 2, area.y + (area.h - side) / 2, side, side};
  return square;
}

// Where a decoration, a portrait or writing sits on its wall: a square half the cell wide, against
// the face it hangs on and just inside that face's strip. The stone around it still reads as wall,
// and on a pillar the square shows which face carries the decoration.
static SDL_Rect FeatureSquare(const SDL_Rect &cell, int side)
{
  SDL_Rect square = WritingSquare(HalfTowards(cell, side));
  int strip = StripThickness(cell);
  if (side == AutoMapNorth) square.y += strip;
  if (side == AutoMapSouth) square.y -= strip;
  if (side == AutoMapEast) square.x -= strip;
  if (side == AutoMapWest) square.x += strip;
  return square;
}

static int CountFeatures(const AutoMapCellRecord &wall)
{
  int count = 0;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) count += HasFeature(wall.wallSides[side]) ? 1 : 0;
  return count;
}

// At All every switch carries its lock ring and goods dot by its face, so the features keep the
// look they had there: a wall with one feature shows it across the whole cell, with several each
// gets the half on its own side. Below All a feature keeps to its square by the face.
static SDL_Rect FeatureArea(const SDL_Rect &cell, const AutoMapCellRecord &wall, int side,
                            bool allowWholeCell)
{
  if (shownDrawList.spoilers != AutoMapSpoilersAll) return FeatureSquare(cell, side);
  return CountFeatures(wall) == 1 && allowWholeCell ? cell : HalfTowards(cell, side);
}

static void DrawWriting(const SDL_Rect &area)
{
  SDL_Rect text = Inset(WritingSquare(area), 25);
  for (int line = 0; line < WRITING_LINES; line++)
  {
    SDL_Rect stroke = {text.x, text.y + text.h * (2 * line + 1) / (2 * WRITING_LINES),
                       text.w, std::max(1, text.h / 10)};
    Fill(stroke, WALL_SIDE_LINE);
  }
}

static void DrawDashedRect(const SDL_Rect &area, const SDL_Color &colour);

static void DrawHollowDiamond(const SDL_Rect &area, const SDL_Color &colour)
{
  int centreX = area.x + area.w / 2;
  int centreY = area.y + area.h / 2;
  int radius = std::min(area.w, area.h) / 2 - 2;
  if (radius < 2) return;
  UseColour(colour);
  SDL_RenderDrawLine(mapRenderer, centreX, centreY - radius, centreX + radius, centreY);
  SDL_RenderDrawLine(mapRenderer, centreX + radius, centreY, centreX, centreY + radius);
  SDL_RenderDrawLine(mapRenderer, centreX, centreY + radius, centreX - radius, centreY);
  SDL_RenderDrawLine(mapRenderer, centreX - radius, centreY, centreX, centreY - radius);
}

// Machinery the 3D view hides: a dashed frame and a hollow marker on the side's half,
// compact always so the wall itself stays visible
static void DrawHiddenSwitchMark(const SDL_Rect &cell, int side)
{
  SDL_Rect area = HalfTowards(cell, side);
  DrawDashedRect(Inset(area, 18), WALL_SIDE_LINE);
  DrawHollowDiamond(area, HIDDEN_SWITCH);
}

static void DrawWallSide(const SDL_Rect &cell, const AutoMapCellRecord &wall, int side,
                         bool allowWholeCell)
{
  const AutoMapWallSide &wallSide = wall.wallSides[side];
  bool drawsPicture = Shows(AutoMapShowWallDecorations)
      && (wallSide.picture != AUTOMAP_NO_PICTURE || wallSide.portrait != AUTOMAP_NO_PICTURE);
  bool drawsWriting = Shows(AutoMapShowWallWriting) && wallSide.hasWriting;
  if (wallSide.hidden)
  {
    DrawHiddenSwitchMark(cell, side);
    if (drawsPicture && wallSide.portrait != AUTOMAP_NO_PICTURE)
    {
      DrawPortrait(HalfTowards(cell, side), wallSide.portrait);
    }
    if (drawsWriting) DrawWriting(HalfTowards(cell, side));
    return;
  }
  if (!HasFeature(wallSide)) return;
  // Below all, the fog filter clears `functional` for sides whose switch the party has not
  // found out about; the picture still shows, so it is drawn like any other decoration
  bool plainDecoration = !wallSide.functional && shownDrawList.spoilers != AutoMapSpoilersAll;
  if (!wallSide.functional && !plainDecoration)
  {
    if (!drawsPicture && !drawsWriting) return;
    // Random dressing: a quiet tick where a switch would strip the wall
    Fill(StripAlong(cell, side), FLAVOR_TICK);
    if (drawsWriting) DrawWriting(FeatureArea(cell, wall, side, false));
    return;
  }
  if (!drawsPicture && !drawsWriting)
  {
    // A known switch keeps its strip with its picture hidden
    if (!plainDecoration) Fill(StripAlong(cell, side), WALL_SIDE_LINE);
    return;
  }
  SDL_Rect area = FeatureArea(cell, wall, side, allowWholeCell);
  Fill(Inset(area, 6), WALL_FEATURE_BACKDROP);
  if (drawsPicture)
  {
    const AutoMapSprite *sprite = SpriteOfTheGame(wallSide.picture);
    bool pictureIsMissing = sprite == NULL && wallSide.picture != AUTOMAP_NO_PICTURE;
    if (sprite != NULL) DrawSprite(*sprite, FitInto(Inset(area, 8), *sprite));
    if (pictureIsMissing) Fill(Inset(area, 30), WALL_FEATURE_MARKER);
    if (wallSide.portrait != AUTOMAP_NO_PICTURE) DrawPortrait(area, wallSide.portrait);
  }
  if (drawsWriting) DrawWriting(area);
  // The strip marks the face; only a switch the party knows about gets the bright one
  Fill(StripAlong(cell, side), plainDecoration ? FLAVOR_TICK : WALL_SIDE_LINE);
}

// Faint always-show markers for the machinery the hover tooltip details: a hollow
// gold ring on sides with a lock requirement, a filled item-yellow dot on sides
// holding alcove goods. The two yellows are close, so shape tells them apart: an
// alcove that wants an item and holds one shows a ring and a dot, never two squares. Dots sit on the side's edge midpoint, clear of every corner
// badge (link badge, break marks, more-items). Only functional (visible) wall
// sides mark; doors mark every surviving lock side, as doors carry no side
// graphics to gate on. Drawn in the static layer, fog-filtered like the tooltip.
// A dark rim around the dot keeps it readable on bright wall sprites at low zoom,
// where the bare yellow would drown in the picture behind it.
static void DrawSideDot(int centreX, int centreY, int size, const SDL_Color &colour)
{
  SDL_Rect rim = {centreX - size / 2 - 1, centreY - size / 2 - 1, size + 2, size + 2};
  Fill(rim, BACKGROUND);
  SDL_Rect dot = {centreX - size / 2, centreY - size / 2, size, size};
  Fill(dot, colour);
}

// The lock mark: the dot's rim and size, hollow, its frame thick enough to read at low zoom
static void DrawSideRing(int centreX, int centreY, int size, const SDL_Color &colour)
{
  DrawSideDot(centreX, centreY, size, colour);
  int frame = std::max(1, size / 4);
  if (size - 2 * frame < 1) return;
  SDL_Rect hole = {centreX - size / 2 + frame, centreY - size / 2 + frame, size - 2 * frame,
                   size - 2 * frame};
  Fill(hole, BACKGROUND);
}

static void DrawWallMarkers(const SDL_Rect &cell, const AutoMapCellRecord &record)
{
  bool isDoor = record.kind == AutoMapCellDoor;
  if (!isDoor && record.kind != AutoMapCellWall && record.kind != AutoMapCellFalseWall) return;
  bool locked[AUTOMAP_SIDE_COUNT] = {false, false, false, false};
  bool stocked[AUTOMAP_SIDE_COUNT] = {false, false, false, false};
  for (size_t i = 0; i < record.wallLocks.size(); i++)
  {
    const AutoMapWallLock &lock = record.wallLocks[i];
    if (!lock.required || lock.side < 0 || lock.side >= AUTOMAP_SIDE_COUNT) continue;
    if (!isDoor && !record.wallSides[lock.side].functional) continue;
    locked[lock.side] = true;
  }
  for (size_t i = 0; i < record.alcoveContents.size(); i++)
  {
    const AutoMapAlcoveEntry &entry = record.alcoveContents[i];
    if (entry.item.worldPicture == AUTOMAP_NO_PICTURE) continue;
    if (entry.side < 0 || entry.side >= AUTOMAP_SIDE_COUNT) continue;
    if (!Shows(ShownEntryOfItemKind(AutoMapScene_KindOfItem(entry.item)))) continue;
    // Below all, `functional` is cleared for unknown switches, but goods in an alcove show
    if (!isDoor && !record.wallSides[entry.side].functional
        && (shownDrawList.spoilers == AutoMapSpoilersAll || !record.wallSides[entry.side].isAlcove))
      continue;
    stocked[entry.side] = true;
  }
  int dotSize = std::max(6, cell.w / 7);
  int midX = cell.x + cell.w / 2;
  int midY = cell.y + cell.h / 2;
  // Both dots sit fully inside the cell: the lock dot rim-tight against the edge,
  // the stocked dot one diameter further in, so they never overlap each other or
  // bleed into the neighbouring cell.
  int lockOffset = dotSize / 2 + 1;
  int stockedOffset = lockOffset + dotSize + 2;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    if (!locked[side] && !stocked[side]) continue;
    int edgeX = midX, edgeY = midY;
    int inwardX = 0, inwardY = 0;
    if (side == AutoMapNorth) { edgeY = cell.y; inwardY = 1; }
    else if (side == AutoMapSouth) { edgeY = cell.y + cell.h; inwardY = -1; }
    else if (side == AutoMapWest) { edgeX = cell.x; inwardX = 1; }
    else { edgeX = cell.x + cell.w; inwardX = -1; }
    if (locked[side])
    {
      DrawSideRing(edgeX + inwardX * lockOffset, edgeY + inwardY * lockOffset, dotSize,
                   LINK_BADGE);
    }
    if (stocked[side])
    {
      DrawSideDot(edgeX + inwardX * stockedOffset, edgeY + inwardY * stockedOffset,
                  dotSize, ITEM_MARKER);
    }
  }
}

static void DrawWall(const SDL_Rect &cell, const AutoMapCellRecord &wall)
{
  Fill(cell, WALL);
  Outline(cell, WALL_EDGE);
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) DrawWallSide(cell, wall, side, true);
  DrawWallMarkers(cell, wall);
}

// An illusionary wall: the pinstripes tell it from stone at a glance
static void DrawFalseWall(const SDL_Rect &cell, const AutoMapCellRecord &wall)
{
  Fill(cell, FALSE_WALL);
  Outline(cell, FALSE_WALL_EDGE);
  UseColour(FALSE_WALL_STRIPE);
  for (int x = cell.x + 3; x < cell.x + cell.w; x += 4)
  {
    SDL_RenderDrawLine(mapRenderer, x, cell.y + 1, x, cell.y + cell.h - 2);
  }
  // Features keep to their own half so the stripes stay visible around them
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) DrawWallSide(cell, wall, side, false);
  DrawWallMarkers(cell, wall);
}

static void DrawDashedRect(const SDL_Rect &area, const SDL_Color &colour)
{
  static const int DASH = 4;
  static const int GAP = 3;
  // Inside the cell's own pixels: neighbours drawn later share the boundary rows
  SDL_Rect inner = {area.x + 1, area.y + 1, std::max(0, area.w - 2), std::max(0, area.h - 2)};
  UseColour(colour);
  for (int x = inner.x; x < inner.x + inner.w; x += DASH + GAP)
  {
    int endX = std::min(x + DASH, inner.x + inner.w);
    SDL_RenderDrawLine(mapRenderer, x, inner.y, endX, inner.y);
    SDL_RenderDrawLine(mapRenderer, x, inner.y + inner.h, endX, inner.y + inner.h);
  }
  for (int y = inner.y; y < inner.y + inner.h; y += DASH + GAP)
  {
    int endY = std::min(y + DASH, inner.y + inner.h);
    SDL_RenderDrawLine(mapRenderer, inner.x, y, inner.x, endY);
    SDL_RenderDrawLine(mapRenderer, inner.x + inner.w, y, inner.x + inner.w, endY);
  }
}

// A teleporter the 3D view hides: a dashed grey frame on the floor
static void DrawInvisibleTeleporterMark(const SDL_Rect &cell)
{
  SDL_Rect frame = Inset(cell, 10);
  DrawDashedRect(frame, INVISIBLE_TELEPORTER);
}

// A pit the 3D view hides (closed, or open with the concealed look): the bare
// sprite reads as floor at low zoom, so a dashed red frame round the whole
// cell keeps it visible at any zoom, the way open false walls frame theirs.
// An open one also gets a centre dot, drawn last like the dormant dot so
// items cannot bury it: the dot marks the dangerous state at a glance, so
// frame plus dot reads open (live hole), frame alone reads closed (latent).
static void DrawHiddenPitMark(const SDL_Rect &cell)
{
  DrawDashedRect(cell, PIT_MARKER_EDGE);
}

static void DrawHiddenPitDot(const SDL_Rect &cell)
{
  int size = std::max(2, cell.w / 12);
  SDL_Rect dot = {cell.x + (cell.w - size) / 2, cell.y + (cell.h - size) / 2, size, size};
  Fill(dot, PIT_MARKER_EDGE);
}

// A teleporter that fires nothing: a dimmer frame with a dot, present but dead. The frame
// lies under what the cell holds; the dot draws last so items cannot bury it.
static void DrawDormantTeleporterMark(const SDL_Rect &cell)
{
  SDL_Rect frame = Inset(cell, 10);
  DrawDashedRect(frame, DORMANT_TELEPORTER);
}

static void DrawDormantTeleporterDot(const SDL_Rect &cell)
{
  int size = std::max(2, cell.w / 12);
  SDL_Rect dot = {cell.x + (cell.w - size) / 2, cell.y + (cell.h - size) / 2, size, size};
  Fill(dot, DORMANT_TELEPORTER);
}

// A teleporter to its own cell: absolute spinners force an exit facing, so they
// draw a full ring with a straight radial arrow out to that facing; relative
// spinners turn the victim, so they draw only the arc of the turn itself with
// a tangential head (right runs clockwise, left counter-clockwise, around a
// half circle). A straight spin keeps the facing: a gapped ring with a dot
// and no head. Unknown rotations keep the old full ring with an east head.
static void DrawSpinnerArc(float centreX, float centreY, float radius,
                            float startAngle, float sweepAngle)
{
  static const float PI = 3.14159265f;
  int segments = (int)ceilf(16.0f * fabsf(sweepAngle) / (2.0f * PI));
  if (segments < 2) segments = 2;
  float previousX = centreX + radius * cos(startAngle);
  float previousY = centreY - radius * sin(startAngle);
  for (int i = 1; i <= segments; i++)
  {
    float angle = startAngle + sweepAngle * i / segments;
    float x = centreX + radius * cos(angle);
    float y = centreY - radius * sin(angle);
    SDL_RenderDrawLine(mapRenderer, (int)previousX, (int)previousY, (int)x, (int)y);
    previousX = x;
    previousY = y;
  }
}

static void DrawSpinnerTangentialHead(float headX, float headY, float headAngle,
                                      float radius, bool clockwise,
                                      const SDL_Color &colour)
{
  // Along is the counter-clockwise tangent; clockwise heads run against it
  float sign = clockwise ? -1.0f : 1.0f;
  float alongX = sign * -sin(headAngle);
  float alongY = sign * -cos(headAngle);
  float outX = cos(headAngle);
  float outY = -sin(headAngle);
  FloatPoint tip = {headX + alongX * radius * 0.45f, headY + alongY * radius * 0.45f};
  FloatPoint upper = {headX - alongX * radius * 0.15f + outX * radius * 0.30f,
                     headY - alongY * radius * 0.15f + outY * radius * 0.30f};
  FloatPoint lower = {headX - alongX * radius * 0.15f - outX * radius * 0.30f,
                     headY - alongY * radius * 0.15f - outY * radius * 0.30f};
  FillTriangle(tip, upper, lower, colour);
}

static void DrawSpinnerRadialHead(float centreX, float centreY,
                                  float headX, float headY, float headAngle,
                                  float radius, const SDL_Color &colour)
{
  SDL_RenderDrawLine(mapRenderer, (int)centreX, (int)centreY, (int)headX, (int)headY);
  float outX = cos(headAngle);
  float outY = -sin(headAngle);
  float perpX = -sin(headAngle);
  float perpY = -cos(headAngle);
  FloatPoint tip = {headX + outX * radius * 0.45f, headY + outY * radius * 0.45f};
  FloatPoint upper = {headX - outX * radius * 0.10f + perpX * radius * 0.30f,
                     headY - outY * radius * 0.10f + perpY * radius * 0.30f};
  FloatPoint lower = {headX - outX * radius * 0.10f - perpX * radius * 0.30f,
                     headY - outY * radius * 0.10f - perpY * radius * 0.30f};
  FillTriangle(tip, upper, lower, colour);
}

static void DrawSpinnerMark(const SDL_Rect &cell, int facing, int rotation,
                            const SDL_Color &colour = SPINNER)
{
  static const float PI = 3.14159265f;
  float centreX = cell.x + cell.w / 2.0f;
  float centreY = cell.y + cell.h / 2.0f;
  float radius = cell.w * 0.28f;
  UseColour(colour);
  AutoMapSpinnerGeometry geometry = AutoMapScene_SpinnerGeometry(facing, rotation);
  switch (geometry.glyph)
  {
  case AutoMapSpinnerAbsolute:
  {
    DrawSpinnerArc(centreX, centreY, radius, 0.0f, 2.0f * PI);
    float headX = centreX + radius * cos(geometry.headAngle);
    float headY = centreY - radius * sin(geometry.headAngle);
    DrawSpinnerRadialHead(centreX, centreY, headX, headY, geometry.headAngle, radius, colour);
    break;
  }
  case AutoMapSpinnerRight:
  case AutoMapSpinnerLeft:
  case AutoMapSpinnerAround:
  {
    DrawSpinnerArc(centreX, centreY, radius,
                   geometry.startAngle, geometry.sweepAngle);
    float headX = centreX + radius * cos(geometry.headAngle);
    float headY = centreY - radius * sin(geometry.headAngle);
    DrawSpinnerTangentialHead(headX, headY, geometry.headAngle, radius,
                              geometry.sweepAngle < 0.0f, colour);
    break;
  }
  case AutoMapSpinnerStraight:
  {
    // A gapped ring reads as "no turn" at any zoom: 60 degrees open at north
    static const float GAP = PI / 3.0f;
    DrawSpinnerArc(centreX, centreY, radius, PI / 2.0f + GAP / 2.0f,
                   2.0f * PI - GAP);
    int size = std::max(2, cell.w / 12);
    SDL_Rect dot = {(int)(centreX - size / 2.0f), (int)(centreY - size / 2.0f), size, size};
    Fill(dot, colour);
    break;
  }
  case AutoMapSpinnerUnknown:
  default:
  {
    DrawSpinnerArc(centreX, centreY, radius, 0.0f, 2.0f * PI);
    DrawSpinnerTangentialHead(centreX + radius, centreY, 0.0f, radius, false, colour);
    break;
  }
  }
}

static void DrawMoreItemsMark(const SDL_Rect &cell)
{
  int arm = std::max(3, cell.w / 5);
  int thickness = std::max(1, arm / 3);
  int centreX = cell.x + cell.w - arm;
  int centreY = cell.y + arm;
  SDL_Rect horizontal = {centreX - arm / 2, centreY - thickness / 2, arm, thickness};
  SDL_Rect vertical = {centreX - thickness / 2, centreY - arm / 2, thickness, arm};
  Fill(horizontal, ITEM_MARKER);
  Fill(vertical, ITEM_MARKER);
}

// Small items keep their size relative to a cell, large ones are fitted into their slot
static void DrawItem(const SDL_Rect &slot, int picture)
{
  const AutoMapSprite *sprite = SpriteOfTheGame(picture);
  if (sprite == NULL)
  {
    Fill(Inset(slot, 35), ITEM_MARKER);
    return;
  }
  float scale = std::min((float)slot.w / ITEM_PIXELS_PER_CELL, slot.w * 0.92f / sprite->width);
  int width = (int)(sprite->width * scale);
  int height = (int)(sprite->height * scale);
  SDL_Rect area = {slot.x + (slot.w - width) / 2, slot.y + (slot.h - height) / 2, width, height};
  DrawSprite(*sprite, area);
}

// Like DrawItem, but contained in both dimensions: tall figures (skeletons)
// would otherwise spill over the tooltip plate and the HP footer at high zoom,
// where slots grow with the cell size. Map cells can let sprites reach into the
// neighbour cell; the plate cannot. Items with a known type draw their inventory
// tile, the way the inventory draws them; the rest fall back to the world sprite.
static void DrawTooltipIcon(const SDL_Rect &slot, const AutoMapItem &item)
{
  SDL_Rect tile = {0, 0, 0, 0};
  const AutoMapSprite *icon = SpriteOfIcon(item, &tile);
  if (icon != NULL)
  {
    SDL_Rect box = Inset(slot, 8);
    float scale = std::min((float)box.w / tile.w, (float)box.h / tile.h);
    int width = (int)(tile.w * scale);
    int height = (int)(tile.h * scale);
    SDL_Rect area = {box.x + (box.w - width) / 2, box.y + (box.h - height) / 2, width, height};
    DrawSpriteTile(*icon, area, tile);
    return;
  }
  const AutoMapSprite *sprite = SpriteOfTheGame(item.worldPicture);
  if (sprite == NULL)
  {
    Fill(Inset(slot, 35), ITEM_MARKER);
    return;
  }
  DrawSprite(*sprite, FitInto(Inset(slot, 8), *sprite));
}

// Something glimpsed in the dark: a "?" in the font of the rulers, font pixels of pixel screen
// pixels, centred in area, on the cell or in the fog
static void DrawGlimpseMark(int pixel, const SDL_Rect &area)
{
  int left = area.x + (area.w - 3 * pixel) / 2;
  int top = area.y + (area.h - 5 * pixel) / 2;
  for (int row = 0; row < 5; row++)
  {
    for (int col = 0; col < 3; col++)
    {
      if (!AutoMapScene_FontPixel('?', row, col)) continue;
      SDL_Rect dot = {left + col * pixel, top + row * pixel, pixel, pixel};
      Fill(dot, ITEM_MARKER);
    }
  }
}

static void DrawGlimpses(const AutoMapView &view, const AutoMapDrawList &drawList)
{
  for (size_t i = 0; i < drawList.glimpsedItems.size(); i++)
  {
    SDL_Rect cell = CellArea(view, drawList.glimpsedItems[i].x, drawList.glimpsedItems[i].y);
    DrawGlimpseMark(std::max(1, cell.w / 9), cell);
  }
  // On a wall, a smaller "?" sits in the third of the cell towards the face
  for (size_t i = 0; i < drawList.glimpsedFaces.size(); i++)
  {
    const AutoMapGlimpsedFace &face = drawList.glimpsedFaces[i];
    SDL_Rect cell = CellArea(view, face.x, face.y);
    SDL_Rect area = cell;
    if (face.side == AutoMapNorth || face.side == AutoMapSouth) area.h = cell.h / 3;
    else area.w = cell.w / 3;
    if (face.side == AutoMapSouth) area.y = cell.y + cell.h - area.h;
    if (face.side == AutoMapEast) area.x = cell.x + cell.w - area.w;
    DrawGlimpseMark(std::max(1, cell.w / 16), area);
  }
}

static void DrawItems(const SDL_Rect &cell, const AutoMapShownCell &shown)
{
  std::vector<AutoMapItem> items = shown.cell.itemPictures;
  bool hasMoreItems = shown.hasMoreItems;
  unsigned kinds = ShownItemKinds();
  if (kinds != AUTOMAP_ALL_ITEM_KINDS)
  {
    // Only the kinds shown fill the slots, from the whole pile
    items = AutoMapScene_ItemsOfKinds(shown.allItemPictures, kinds);
    hasMoreItems = (int)items.size() > AUTOMAP_MOST_ITEMS_SHOWN;
    if (hasMoreItems) items.resize(AUTOMAP_MOST_ITEMS_SHOWN);
  }
  int count = (int)items.size();
  for (int i = 0; i < count; i++)
  {
    SDL_Rect slot = {cell.x + cell.w * i / count, cell.y, cell.w / count, cell.h};
    DrawItem(slot, items[i].worldPicture);
  }
  if (hasMoreItems) DrawMoreItemsMark(cell);
}

// A lingering cloud settles over the floor but under what lies on it
static void DrawClouds(const SDL_Rect &cell, const AutoMapShownCell &shown,
                       const AutoMapDrawList &drawList)
{
  for (size_t i = 0; i < drawList.clouds.size(); i++)
  {
    if (drawList.clouds[i].x != shown.x || drawList.clouds[i].y != shown.y) continue;
    const AutoMapSprite *sprite = SpriteOfTheGame(drawList.clouds[i].picture);
    if (sprite == NULL)
    {
      SDL_Rect haze = Inset(cell, 5);
      Fill(haze, CLOUD_HAZE);
      Outline(haze, CLOUD_EDGE);
    }
    else
    {
      DrawSprite(*sprite, FitInto(cell, *sprite));
    }
  }
}

static void DrawOperatorDot(int centreX, int centreY, int size)
{
  SDL_Rect dot = {centreX - size / 2, centreY - size / 2, size, size};
  Fill(dot, WHITE);
}

// A small triangle in annotation white: the party echo for party-operated pads,
// pointing the required facing for directed ones (north when unknown or plain)
static void DrawOperatorTriangle(const SDL_Rect &box, AutoMapDirection facing)
{
  static const FloatPoint TIP = {1.00f, 0.50f};
  static const FloatPoint UPPER_CORNER = {0.30f, 0.10f};
  static const FloatPoint LOWER_CORNER = {0.30f, 0.90f};
  FillTriangle(TurnedPointInCell(box, TIP, facing), TurnedPointInCell(box, UPPER_CORNER, facing),
               TurnedPointInCell(box, LOWER_CORNER, facing), WHITE);
}

// The fallback when a required object type resolves to no sprite: a keyhole where the
// miniature would sit. An unmarked pad would read as "anything goes", which is wrong.
static void DrawKeyholeMark(const SDL_Rect &box)
{
  int radius = std::min(box.w, box.h) / 4;
  if (radius < 2) return;
  float centreX = box.x + box.w / 2.0f;
  float centreY = box.y + box.h / 2.0f - radius / 2.0f;
  UseColour(WHITE);
  float prevX = centreX + radius;
  float prevY = centreY;
  for (int i = 1; i <= 12; i++)
  {
    float angle = 2.0f * 3.14159265f * i / 12;
    float x = centreX + radius * cos(angle);
    float y = centreY + radius * sin(angle);
    SDL_RenderDrawLine(mapRenderer, (int)prevX, (int)prevY, (int)x, (int)y);
    prevX = x;
    prevY = y;
  }
  int slotW = std::max(2, radius / 2);
  SDL_Rect slot = {(int)(centreX - slotW / 2), (int)centreY, slotW, radius + 2};
  Fill(slot, WHITE);
}

// Who operates the pad, bottom-left so the more-items mark (top-right) and the link badge
// (bottom-right) keep their corners. Triangles echo the party arrow, dots the monsters;
// anything-operated pads stay unmarked and quiet.
static void DrawPadOperatorMark(const SDL_Rect &cell, const AutoMapFloorActuator &pad)
{
  SDL_Rect corner = {cell.x + cell.w * 8 / 100, cell.y + cell.h * 58 / 100,
                     cell.w * 30 / 100, cell.h * 34 / 100};
  int dotSize = std::max(2, cell.w / 14);
  switch (pad.op)
  {
  case AutoMapPadPartyOrMonster:
  {
    SDL_Rect triangleHalf = {corner.x, corner.y, corner.w * 60 / 100, corner.h};
    DrawOperatorTriangle(triangleHalf, AutoMapNorth);
    int dotX = corner.x + corner.w * 80 / 100;
    DrawOperatorDot(dotX, corner.y + corner.h / 2, dotSize);
    break;
  }
  case AutoMapPadParty: DrawOperatorTriangle(corner, AutoMapNorth); break;
  case AutoMapPadPartyFacing:
  {
    AutoMapDirection facing = AutoMapNorth;
    if (pad.requiredFacing >= AutoMapNorth && pad.requiredFacing <= AutoMapWest)
    {
      facing = (AutoMapDirection)pad.requiredFacing;
    }
    DrawOperatorTriangle(corner, facing);
    break;
  }
  case AutoMapPadMonster:
    DrawOperatorDot(corner.x + corner.w * 25 / 100, corner.y + corner.h / 2, dotSize);
    DrawOperatorDot(corner.x + corner.w * 75 / 100, corner.y + corner.h / 2, dotSize);
    break;
  case AutoMapPadObject:
  case AutoMapPadPossession:
  {
    SDL_Rect miniature = Inset(cell, 30);
    if (pad.object.worldPicture == AUTOMAP_NO_PICTURE) DrawKeyholeMark(miniature);
    else DrawTooltipIcon(miniature, pad.object);
    // Held, not placed: the centered miniature plus a dot beneath it
    if (pad.op == AutoMapPadPossession)
    {
      int dotX = miniature.x + miniature.w / 2;
      int dotY = miniature.y + miniature.h + dotSize / 2 + 1;
      if (dotY < cell.y + cell.h - 1) DrawOperatorDot(dotX, dotY, dotSize);
    }
    break;
  }
  default: break;
  }
}

// Objects-only teleporters take anything inanimate: a hollow square, the last glyph of
// the operator vocabulary (triangle = party, dots = monsters, plain = anything)
static void DrawOperatorHollowSquare(const SDL_Rect &box)
{
  SDL_Rect square = Inset(box, 22);
  if (square.w < 2 || square.h < 2) return;
  Outline(square, WHITE);
}

// Who a firing teleporter takes, bottom-left like the pad marks and in the same glyphs.
// Dormant teleporters, spinners and plain floors carry no scope and stay unmarked.
static void DrawTeleporterScopeMark(const SDL_Rect &cell, const AutoMapCellRecord &record)
{
  if (record.teleporterScope < 0 || record.teleporterScope > 3) return;
  if (record.teleporterScope == 3) return;
  SDL_Rect corner = {cell.x + cell.w * 8 / 100, cell.y + cell.h * 58 / 100,
                     cell.w * 30 / 100, cell.h * 34 / 100};
  int dotSize = std::max(2, cell.w / 14);
  switch (record.teleporterScope)
  {
  case 2: DrawOperatorTriangle(corner, AutoMapNorth); break;
  case 1:
    DrawOperatorDot(corner.x + corner.w * 25 / 100, corner.y + corner.h / 2, dotSize);
    DrawOperatorDot(corner.x + corner.w * 75 / 100, corner.y + corner.h / 2, dotSize);
    break;
  case 0: DrawOperatorHollowSquare(corner); break;
  default: break;
  }
}

static void DrawPadMark(const SDL_Rect &cell, const AutoMapFloorActuator &pad)
{
  SDL_Rect plate = Inset(cell, 22);
  if (pad.kind == AutoMapActuatorDormantPad)
  {
    DrawDashedRect(plate, PAD_DORMANT);
    return;
  }
  Outline(plate, PAD);
  UseColour(PAD);
  SDL_RenderDrawLine(mapRenderer, plate.x, plate.y + plate.h / 2, plate.x + plate.w,
                     plate.y + plate.h / 2);
  DrawPadOperatorMark(cell, pad);
}

static void DrawGeneratorMark(const SDL_Rect &cell, bool spent)
{
  float centreX = cell.x + cell.w / 2.0f;
  float centreY = cell.y + cell.h / 2.0f;
  float radius = cell.w * 0.25f;
  FloatPoint top = {centreX, centreY - radius};
  FloatPoint right = {centreX + radius, centreY};
  FloatPoint bottom = {centreX, centreY + radius};
  FloatPoint left = {centreX - radius, centreY};
  if (spent)
  {
    UseColour(GENERATOR_SPENT);
    SDL_Point corners[] = {{(int)top.x, (int)top.y}, {(int)right.x, (int)right.y},
                           {(int)bottom.x, (int)bottom.y}, {(int)left.x, (int)left.y},
                           {(int)top.x, (int)top.y}};
    SDL_RenderDrawLines(mapRenderer, corners, 5);
    return;
  }
  FillTriangle(top, right, bottom, GENERATOR);
  FillTriangle(top, bottom, left, GENERATOR);
}

static void DrawCogMark(const SDL_Rect &cell)
{
  float centreX = cell.x + cell.w / 2.0f;
  float centreY = cell.y + cell.h / 2.0f;
  float radius = cell.w * 0.20f;
  UseColour(COG);
  float previousX = centreX + radius;
  float previousY = centreY;
  for (int i = 1; i <= 8; i++)
  {
    float angle = 2.0f * 3.14159265f * i / 8;
    float x = centreX + radius * cos(angle);
    float y = centreY + radius * sin(angle);
    SDL_RenderDrawLine(mapRenderer, (int)previousX, (int)previousY, (int)x, (int)y);
    previousX = x;
    previousY = y;
  }
  SDL_RenderDrawLine(mapRenderer, (int)(centreX - radius), (int)centreY,
                     (int)(centreX + radius), (int)centreY);
  SDL_RenderDrawLine(mapRenderer, (int)centreX, (int)(centreY - radius),
                     (int)centreX, (int)(centreY + radius));
}

// Which unit a live generator creates: the monster's front picture over the diamond's
// centre, the corners still reading as a generator. Missing sprites and spent
// generators fall back to the bare diamond.
static void DrawGeneratedMonster(const SDL_Rect &cell, const AutoMapFloorActuator &pad)
{
  if (!pad.hasGeneratedMonster) return;
  AutoMapSpriteRequest request = {pad.generatedKind.frontPicture,
                                  pad.generatedKind.transparentColour};
  const AutoMapSprite *sprite = sprites.Find(request);
  if (sprite == NULL) return;
  DrawSprite(*sprite, FitInto(Inset(cell, 30), *sprite));
}

static void DrawFloorActuators(const SDL_Rect &cell, const AutoMapShownCell &shown,
                               const AutoMapDrawList &drawList)
{
  for (size_t i = 0; i < drawList.actuators.size(); i++)
  {
    if (drawList.actuators[i].x != shown.x || drawList.actuators[i].y != shown.y) continue;
    switch (drawList.actuators[i].kind)
    {
    case AutoMapActuatorPad:
    case AutoMapActuatorDormantPad: DrawPadMark(cell, drawList.actuators[i]); break;
    case AutoMapActuatorGenerator:
      DrawGeneratorMark(cell, false);
      DrawGeneratedMonster(cell, drawList.actuators[i]);
      break;
    case AutoMapActuatorSpentGenerator: DrawGeneratorMark(cell, true); break;
    case AutoMapActuatorCog: DrawCogMark(cell); break;
    }
  }
}

static bool CellIsLinked(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.linkedCells.size(); i++)
  {
    if (drawList.linkedCells[i].x == x && drawList.linkedCells[i].y == y) return true;
  }
  return false;
}

// Something operates this wall or door: an actuator points at its cell
static void DrawLinkBadgeIfLinked(const SDL_Rect &cell, const AutoMapShownCell &shown,
                                  const AutoMapDrawList &drawList)
{
  if (!CellIsLinked(drawList, shown.x, shown.y)) return;
  int size = std::max(3, cell.w / 6);
  SDL_Rect dot = {cell.x + cell.w - size - 2, cell.y + cell.h - size - 2, size, size};
  Fill(dot, LINK_BADGE);
}

static void DrawMissile(const SDL_Rect &slot, const AutoMapShownMissile &missile)
{
  const AutoMapSprite *sprite = SpriteOfTheGame(missile.item.worldPicture);
  if (sprite == NULL)
  {
    Fill(Inset(slot, 35), MISSILE_MARKER);
    return;
  }
  float power = missile.sizeShowsRange ? AutoMapScene_MissileScale(missile.rangeRemaining) : 1.0f;
  float scale = std::min((float)slot.w / ITEM_PIXELS_PER_CELL, slot.w * 0.92f / sprite->width);
  int width = (int)(sprite->width * scale * power);
  int height = (int)(sprite->height * scale * power);
  SDL_Rect area = {slot.x + (slot.w - width) / 2, slot.y + (slot.h - height) / 2, width, height};
  DrawSprite(*sprite, area);
}

// What flies crosses the upper part of its cell, above what lies on the floor
static void DrawMissiles(const SDL_Rect &cell, const AutoMapShownCell &shown,
                         const AutoMapDrawList &drawList)
{
  int count = 0;
  for (size_t i = 0; i < drawList.missiles.size(); i++)
  {
    count += drawList.missiles[i].x == shown.x && drawList.missiles[i].y == shown.y ? 1 : 0;
  }
  if (count == 0) return;
  SDL_Rect air = {cell.x, cell.y, cell.w, cell.h * 3 / 5};
  int slot = 0;
  for (size_t i = 0; i < drawList.missiles.size(); i++)
  {
    if (drawList.missiles[i].x != shown.x || drawList.missiles[i].y != shown.y) continue;
    SDL_Rect place = {air.x + air.w * slot / count, air.y, air.w / count, air.h};
    DrawMissile(place, drawList.missiles[i]);
    slot++;
  }
}

static void DrawCellBase(const AutoMapView &view, const AutoMapShownCell &shown,
                         const AutoMapDrawList &drawList)
{
  SDL_Rect cell = CellArea(view, shown.x, shown.y);
  const AutoMapCellRecord &record = shown.cell;
  bool sealedDoor = record.kind == AutoMapCellDoor && drawList.spoilers == AutoMapSpoilersAll
      && AutoMapScene_DoorIsSealed(record, CellIsLinked(drawList, shown.x, shown.y));
  switch (record.kind)
  {
  case AutoMapCellWall: DrawWall(cell, record); return;
  case AutoMapCellFalseWall:
    DrawFalseWall(cell, record);
    DrawLinkBadgeIfLinked(cell, shown, drawList);
    return;
  case AutoMapCellDoor:
    DrawDoor(cell, record, sealedDoor);
    break;
  case AutoMapCellStairs: DrawStairs(cell, record); break;
  case AutoMapCellOpenFalseWall: DrawFloor(cell); DrawDashedRect(cell, FALSE_WALL_EDGE); break;
  case AutoMapCellInvisibleTeleporter: DrawFloor(cell); DrawInvisibleTeleporterMark(cell); break;
  case AutoMapCellDormantTeleporter: DrawFloor(cell); DrawDormantTeleporterMark(cell); break;
  case AutoMapCellSpinner: DrawFloor(cell); DrawSpinnerMark(cell, record.spinnerFacing, record.spinnerRotation); break;
  case AutoMapCellDormantSpinner:
    DrawFloor(cell);
    DrawSpinnerMark(cell, record.spinnerFacing, record.spinnerRotation, DORMANT_SPINNER);
    break;
  default: DrawFloor(cell); break;
  }
  bool hasFloorPicture = record.kind == AutoMapCellPit || record.floorPicture != AUTOMAP_NO_PICTURE;
  if (record.kind == AutoMapCellTeleporter) DrawTeleporter(cell);
  if (record.kind == AutoMapCellTeleporter || record.kind == AutoMapCellInvisibleTeleporter)
  {
    DrawTeleporterScopeMark(cell, record);
    // Cross-level targets name their level on the cell itself, so the link
    // reads without hovering. Same-level targets keep the hover arrow only.
    // teleporterTargetVisible is the fog filter's verdict, so unexplored
    // destinations stay blank.
    if (shown.teleporterTargetVisible && record.teleporterDestLevel != drawList.level)
    {
      DrawLevelPlate(cell, record.teleporterDestLevel);
    }
  }
  if (hasFloorPicture && (record.kind == AutoMapCellPit || Shows(AutoMapShowFloorDecorations)
                          || !AutoMapScene_FloorPictureIsFlavour(record.floorPicture)))
  {
    DrawFloorPicture(cell, record);
  }
  // A closed switchable pit keeps its floor look; the flag survives only at All
  if ((record.kind == AutoMapCellPit && record.pitIsHidden) || record.pitIsClosedSwitchable)
  {
    DrawHiddenPitMark(cell);
  }
  DrawClouds(cell, shown, drawList);
  if (shown.isHighlighted) Fill(cell, EXPLORED_HIGHLIGHT);
  DrawFloorActuators(cell, shown, drawList);
  DrawItems(cell, shown);
  if (record.kind == AutoMapCellDormantTeleporter) DrawDormantTeleporterDot(cell);
  if (record.kind == AutoMapCellPit && record.pitIsHidden && record.pitIsOpen)
  {
    DrawHiddenPitDot(cell);
  }
  if (record.kind == AutoMapCellDoor || record.kind == AutoMapCellOpenFalseWall)
  {
    DrawLinkBadgeIfLinked(cell, shown, drawList);
  }
  if (record.kind == AutoMapCellDoor) DrawDoorBreakMarks(cell, record);
  if (record.kind == AutoMapCellDoor) DrawWallMarkers(cell, record);
}

// ---------------------------------------------------------------------------------------------
// The player's notes on the static layer

static bool IsPlayerLinkTarget(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.playerLinks.size(); i++)
  {
    if (drawList.playerLinks[i].dstX == x && drawList.playerLinks[i].dstY == y) return true;
  }
  return false;
}

// Drawn over whatever the cell is, at every spoiler level: the mark is a guess and never replaces
// the drawn cell. The frame sits a little inside the cell, so at All a real hidden pit's frame
// still shows round it. A player link target gets the link badge in the player colour, in the
// corner across from the learned badge so both can show.
static void DrawPlayerNotesOfCell(const AutoMapView &view, const AutoMapShownCell &shown,
                                  const AutoMapDrawList &drawList)
{
  SDL_Rect cell = CellArea(view, shown.x, shown.y);
  if (shown.playerPit >= 0)
  {
    DrawDashedRect(Inset(cell, 6), PLAYER_NOTE);
    if (shown.playerPit == 1)
    {
      int size = std::max(2, cell.w / 12);
      SDL_Rect dot = {cell.x + (cell.w - size) / 2, cell.y + (cell.h - size) / 2, size, size};
      Fill(dot, PLAYER_NOTE);
    }
  }
  if (IsPlayerLinkTarget(drawList, shown.x, shown.y))
  {
    int size = std::max(3, cell.w / 6);
    SDL_Rect dot = {cell.x + 2, cell.y + cell.h - size - 2, size, size};
    Fill(dot, PLAYER_NOTE);
  }
}

static void DrawPlayerNotes(const AutoMapView &view, const AutoMapDrawList &drawList)
{
  for (size_t i = 0; i < drawList.cells.size(); i++)
  {
    DrawPlayerNotesOfCell(view, drawList.cells[i], drawList);
  }
}

static void DrawParty(const AutoMapView &view, const AutoMapParty &party)
{
  SDL_Rect cell = CellArea(view, party.x, party.y);
  static const FloatPoint TIP = {0.88f, 0.50f};
  static const FloatPoint UPPER_BARB = {0.18f, 0.16f};
  static const FloatPoint NOTCH = {0.36f, 0.50f};
  static const FloatPoint LOWER_BARB = {0.18f, 0.84f};
  FloatPoint tip = TurnedPointInCell(cell, TIP, party.facing);
  FloatPoint upperBarb = TurnedPointInCell(cell, UPPER_BARB, party.facing);
  FloatPoint notch = TurnedPointInCell(cell, NOTCH, party.facing);
  FloatPoint lowerBarb = TurnedPointInCell(cell, LOWER_BARB, party.facing);
  FillTriangle(tip, upperBarb, notch, PARTY);
  FillTriangle(tip, notch, lowerBarb, PARTY);
  SDL_Point outline[] = {{(int)tip.x, (int)tip.y}, {(int)upperBarb.x, (int)upperBarb.y},
                         {(int)notch.x, (int)notch.y}, {(int)lowerBarb.x, (int)lowerBarb.y},
                         {(int)tip.x, (int)tip.y}};
  UseColour(WHITE);
  SDL_RenderDrawLines(mapRenderer, outline, 5);
}

static void DrawFacingMark(const SDL_Rect &cell, AutoMapDirection facing)
{
  static const FloatPoint TIP = {1.00f, 0.50f};
  static const FloatPoint UPPER_CORNER = {0.82f, 0.36f};
  static const FloatPoint LOWER_CORNER = {0.82f, 0.64f};
  FillTriangle(TurnedPointInCell(cell, TIP, facing), TurnedPointInCell(cell, UPPER_CORNER, facing),
               TurnedPointInCell(cell, LOWER_CORNER, facing), WHITE);
}

static void DrawFacingMarks(const AutoMapView &view, const AutoMapShownMonster &monster)
{
  SDL_Rect cell = CellArea(view, monster.x, monster.y);
  for (int member = 0; member < monster.memberCount; member++) DrawFacingMark(cell, monster.memberFacings[member]);
}

static float FigureScale(int memberCount)
{
  if (memberCount == 1) return 1.0f;
  return memberCount == 2 ? 0.62f : 0.5f;
}

struct FigurePlace
{
  int centreX;
  int feetY;
  float scale;
};

// The figures of a group stand side by side on the lower edge of their cell
static FigurePlace PlaceOfFigure(const SDL_Rect &cell, int memberCount, int member)
{
  FigurePlace place;
  place.centreX = cell.x + cell.w * (2 * member + 1) / (2 * memberCount);
  place.feetY = cell.y + cell.h * 92 / 100;
  place.scale = FigureScale(memberCount) * cell.w / MONSTER_PIXELS_PER_CELL;
  return place;
}

static void DrawFigureMarker(const FigurePlace &place)
{
  int size = (int)(place.scale * MONSTER_PIXELS_PER_CELL / 2);
  SDL_Rect marker = {place.centreX - size / 2, place.feetY - size, size, size};
  Fill(marker, MONSTER_MARKER);
}

// A figure is a little taller than its cell and reaches into the cell above
static void DrawFigure(const FigurePlace &place, const AutoMapSprite &sprite, bool isMirrored)
{
  int width = (int)(sprite.width * place.scale);
  int height = (int)(sprite.height * place.scale);
  SDL_Rect shadow = {place.centreX - width * 38 / 100, place.feetY - 4, width * 76 / 100, 4};
  SDL_Rect figure = {place.centreX - width / 2, place.feetY - height, width, height};
  Fill(shadow, MONSTER_SHADOW);
  SDL_RendererFlip flip = isMirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
  SDL_RenderCopyEx(mapRenderer, sprite.texture, NULL, &figure, 0, NULL, flip);
}

static void DrawMonster(const AutoMapView &view, const AutoMapShownMonster &monster)
{
  SDL_Rect cell = CellArea(view, monster.x, monster.y);
  for (int member = 0; member < monster.memberCount; member++)
  {
    FigurePlace place = PlaceOfFigure(cell, monster.memberCount, member);
    const AutoMapMonsterPicture &picture = monster.memberPictures[member];
    AutoMapSpriteRequest request = {picture.picture, monster.transparentColour};
    const AutoMapSprite *sprite = sprites.Find(request);
    if (sprite == NULL) DrawFigureMarker(place);
    else DrawFigure(place, *sprite, picture.isMirrored);
  }
}

static bool StandsFurtherNorth(const AutoMapShownMonster &first, const AutoMapShownMonster &second)
{
  return first.y < second.y;
}

// From north to south, so that a figure reaching into the cell above covers what stands there
static void DrawMonsters(const AutoMapView &view, const AutoMapDrawList &drawList)
{
  std::vector<AutoMapShownMonster> fromNorthToSouth = drawList.monsters;
  std::stable_sort(fromNorthToSouth.begin(), fromNorthToSouth.end(), StandsFurtherNorth);
  for (size_t i = 0; i < fromNorthToSouth.size(); i++) DrawFacingMarks(view, fromNorthToSouth[i]);
  for (size_t i = 0; i < fromNorthToSouth.size(); i++) DrawMonster(view, fromNorthToSouth[i]);
}

static AutoMapView ViewOfShownLevel(void)
{
  AutoMapViewRequest request;
  request.levelSize.width = shownDrawList.width;
  request.levelSize.height = shownDrawList.height;
  MapAreaSize(&request.windowWidth, &request.windowHeight);
  request.zoomSteps = zoomSteps;
  if (viewFollowsParty)
  {
    if (shownDrawList.partyIsOnShownLevel)
    {
      request.centerX = shownDrawList.party.x;
      request.centerY = shownDrawList.party.y;
    }
    else
    {
      // No party to follow on this level: zoom into the middle of the level instead
      request.centerX = shownDrawList.width / 2;
      request.centerY = shownDrawList.height / 2;
    }
  }
  else
  {
    request.centerX = viewCenterX;
    request.centerY = viewCenterY;
  }
  AutoMapView view = AutoMapScene_PlaceLevel(request);
  view.originX += RULER;
  view.originY += RULER;
  return view;
}

// The level cell under a window pixel, if any: the inverse of ViewOfShownLevel
static bool CellAtPixel(int pixelX, int pixelY, int *cellX, int *cellY)
{
  AutoMapView view = ViewOfShownLevel();
  if (view.cellSize <= 0) return false;
  int localX = pixelX - view.originX;
  int localY = pixelY - view.originY;
  if (localX < 0 || localY < 0) return false;
  int x = localX / view.cellSize;
  int y = localY / view.cellSize;
  if (x < 0 || y < 0 || x >= shownDrawList.width || y >= shownDrawList.height) return false;
  *cellX = x;
  *cellY = y;
  return true;
}

static void UpdateHoverFromPixel(int pixelX, int pixelY)
{
  int x = -1, y = -1;
  if (!CellAtPixel(pixelX, pixelY, &x, &y))
  {
    ClearHover();
    return;
  }
  if (hoverValid && hoverX == x && hoverY == y)
  {
    // Same cell: the tooltip parks next to the cursor, so follow it.
    if (hoverPixelX != pixelX || hoverPixelY != pixelY)
    {
      hoverPixelX = pixelX;
      hoverPixelY = pixelY;
      isDirty = true;
    }
    return;
  }
  if (x != hoverX || y != hoverY)
  {
    tooltipScrollLines = 0;
    tooltipScrollMax = 0;
  }
  hoverValid = true;
  hoverX = x;
  hoverY = y;
  hoverPixelX = pixelX;
  hoverPixelY = pixelY;
  isDirty = true;
}

// Hover arrows: the directed edges behind the badge set, drawn in the dynamic layer so
// hovering never rebuilds the static cache. Forward mode (hover is a source) draws outgoing
// edges; reverse mode (hover is a target) draws every incoming edge. Both ends already
// survived the fog filter, so nothing secret leaks.
static FloatPoint LinkEndpoint(const AutoMapView &view, int x, int y, bool isWall, int side)
{
  SDL_Rect cell = CellArea(view, x, y);
  FloatPoint centre = {(float)cell.x + cell.w / 2.0f, (float)cell.y + cell.h / 2.0f};
  if (!isWall) return centre;
  FloatPoint edge = centre;
  if (side == AutoMapNorth) edge.y = (float)cell.y;
  else if (side == AutoMapSouth) edge.y = (float)(cell.y + cell.h);
  else if (side == AutoMapWest) edge.x = (float)cell.x;
  else if (side == AutoMapEast) edge.x = (float)(cell.x + cell.w);
  return edge;
}

static void DrawArrowShaft(FloatPoint from, FloatPoint to, const SDL_Color &colour = LINK_BADGE)
{
  // Keep the shaft visible when zoomed in without burying neighbouring cells
  UseColour(colour);
  SDL_RenderDrawLine(mapRenderer, (int)from.x, (int)from.y, (int)to.x, (int)to.y);
}

// A one-shot link reads as fragile: same shaft, broken. The pattern scales with the shaft:
// a short hop gets a single central break (fixed dashes would vanish into one blob),
// a long arrow gets a few long dashes with wide gaps (fixed dashes would buzz).
static void DrawDashedShaft(FloatPoint from, FloatPoint to, const SDL_Color &colour = LINK_BADGE)
{
  float dx = to.x - from.x;
  float dy = to.y - from.y;
  float length = sqrt(dx * dx + dy * dy);
  if (length < 1.0f) return;
  int gaps = std::max(1, std::min((int)(length / 18.0f), 4));
  float gap = std::max(3.0f, std::min(length * 0.12f, 7.0f));
  float dash = (length - gaps * gap) / (gaps + 1);
  UseColour(colour);
  for (int i = 0; i <= gaps; i++)
  {
    float fromT = (i * (dash + gap)) / length;
    float toT = std::min((i * (dash + gap) + dash) / length, 1.0f);
    SDL_RenderDrawLine(mapRenderer, (int)(from.x + dx * fromT), (int)(from.y + dy * fromT),
                       (int)(from.x + dx * toT), (int)(from.y + dy * toT));
  }
}

struct ArrowHeadPoints
{
  FloatPoint nose;
  FloatPoint upper;
  FloatPoint lower;
};

static ArrowHeadPoints HeadPoints(FloatPoint tip, float angle, float size)
{
  float acrossX = -sin(angle);
  float acrossY = -cos(angle);
  float alongX = cos(angle);
  float alongY = -sin(angle);
  ArrowHeadPoints head;
  head.nose = {tip.x, tip.y};
  head.upper = {tip.x - alongX * size * 0.6f + acrossX * size * 0.35f,
                tip.y - alongY * size * 0.6f + acrossY * size * 0.35f};
  head.lower = {tip.x - alongX * size * 0.6f - acrossX * size * 0.35f,
                tip.y - alongY * size * 0.6f - acrossY * size * 0.35f};
  return head;
}

// SET: a decisive filled head. CLEAR: the same head hollowed out
static void DrawArrowHead(FloatPoint tip, float angle, float size,
                          const SDL_Color &colour = LINK_BADGE)
{
  ArrowHeadPoints head = HeadPoints(tip, angle, size);
  FillTriangle(head.nose, head.upper, head.lower, colour);
}

// A learned link: the party saw the target change, not which signal did it. Two strokes
// with no base, so it reads as neither SET nor CLEAR
static void DrawOpenArrowHead(FloatPoint tip, float angle, float size,
                              const SDL_Color &colour = LINK_BADGE)
{
  ArrowHeadPoints head = HeadPoints(tip, angle, size);
  UseColour(colour);
  SDL_RenderDrawLine(mapRenderer, (int)head.nose.x, (int)head.nose.y, (int)head.upper.x,
                     (int)head.upper.y);
  SDL_RenderDrawLine(mapRenderer, (int)head.nose.x, (int)head.nose.y, (int)head.lower.x,
                     (int)head.lower.y);
}

static void DrawHollowArrowHead(FloatPoint tip, float angle, float size,
                                  const SDL_Color &colour = LINK_BADGE)
{
  ArrowHeadPoints head = HeadPoints(tip, angle, size);
  UseColour(colour);
  SDL_RenderDrawLine(mapRenderer, (int)head.nose.x, (int)head.nose.y, (int)head.upper.x,
                     (int)head.upper.y);
  SDL_RenderDrawLine(mapRenderer, (int)head.upper.x, (int)head.upper.y, (int)head.lower.x,
                     (int)head.lower.y);
  SDL_RenderDrawLine(mapRenderer, (int)head.lower.x, (int)head.lower.y, (int)head.nose.x,
                     (int)head.nose.y);
}

// CONSTPRESSURE holds its level instead of edging once: a crossbar mid-shaft
static void DrawPressureTick(FloatPoint from, FloatPoint to, const SDL_Color &colour = LINK_BADGE)
{
  float dx = to.x - from.x;
  float dy = to.y - from.y;
  float length = sqrt(dx * dx + dy * dy);
  if (length < 1.0f) return;
  float tickHalf = std::max(3.0f, length * 0.08f);
  FloatPoint middle = {from.x + dx / 2.0f, from.y + dy / 2.0f};
  FloatPoint upper = {middle.x - dy / length * tickHalf, middle.y + dx / length * tickHalf};
  FloatPoint lower = {middle.x + dy / length * tickHalf, middle.y - dx / length * tickHalf};
  UseColour(colour);
  SDL_RenderDrawLine(mapRenderer, (int)upper.x, (int)upper.y, (int)lower.x, (int)lower.y);
}

// isPlayerLink: the player's own link, which claims no signal at any spoiler level
static void DrawLinkEdge(const AutoMapView &view, const AutoMapLink &edge, float lateralOffset,
                         const SDL_Color &colour = LINK_BADGE, bool isPlayerLink = false)
{
  FloatPoint from = LinkEndpoint(view, edge.srcX, edge.srcY, edge.srcIsWall, edge.srcSide);
  FloatPoint to = LinkEndpoint(view, edge.dstX, edge.dstY, false, -1);
  float dx = to.x - from.x;
  float dy = to.y - from.y;
  float length = sqrt(dx * dx + dy * dy);
  if (length < 1.0f)
  {
    // Self-link: a ring instead of a zero-length line, dashed when one-shot
    SDL_Rect cell = CellArea(view, edge.dstX, edge.dstY);
    if (edge.onceOnly) DrawDashedRect(Inset(cell, 25), colour);
    else Outline(Inset(cell, 25), colour);
    return;
  }
  // Spread parallel edges sharing one target so N sources stay countable
  float perpX = -dy / length;
  float perpY = dx / length;
  from.x += perpX * lateralOffset;
  from.y += perpY * lateralOffset;
  to.x += perpX * lateralOffset;
  to.y += perpY * lateralOffset;
  // Stop the shaft short so the head stays inside the target cell
  float headSize = std::max(6.0f, view.cellSize * 0.25f);
  float backOff = headSize * 0.9f / length;
  FloatPoint tip = {to.x, to.y};
  FloatPoint shaftEnd = {to.x - dx * backOff, to.y - dy * backOff};
  if (edge.onceOnly) DrawDashedShaft(from, shaftEnd, colour);
  else DrawArrowShaft(from, shaftEnd, colour);
  float angle = atan2(dx, -dy);
  // Below All a link is a learned one, built with default fields: its signal claims nothing.
  // Neither does a player link, which is only the player's guess.
  if (isPlayerLink || shownDrawList.spoilers != AutoMapSpoilersAll)
  {
    DrawArrowShaft(shaftEnd, tip, colour);
    DrawOpenArrowHead(tip, angle, headSize, colour);
    return;
  }
  switch (edge.signal)
  {
  case AutoMapSignalClear: DrawHollowArrowHead(tip, angle, headSize, colour); break;
  case AutoMapSignalToggle:
    // Flips either way: a head on each end
    DrawArrowHead(tip, angle, headSize, colour);
    DrawArrowHead(from, angle + 3.14159265f, headSize * 0.7f, colour);
    break;
  case AutoMapSignalPressure:
    DrawArrowHead(tip, angle, headSize, colour);
    DrawPressureTick(from, shaftEnd, colour);
    break;
  default: DrawArrowHead(tip, angle, headSize, colour); break;
  }
}

static void HighlightLinkedCell(const AutoMapView &view, int x, int y,
                                const SDL_Color &colour = LINK_BADGE)
{
  SDL_Rect cell = CellArea(view, x, y);
  Outline(Inset(cell, 1), colour);
  Outline(Inset(cell, 3), colour);
}

static const AutoMapShownCell *ShownCellAt(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.cells.size(); i++)
  {
    if (drawList.cells[i].x == x && drawList.cells[i].y == y) return &drawList.cells[i];
  }
  return NULL;
}

// The ghost of what hovering this source would do to a door target: its leaves redrawn
// where the signal would leave them. Nothing draws for non-doors (their response is not
// statically knowable), for destroyed doors (signals are no-ops), or when the signal
// changes nothing (the arrow alone says "linked but no change").
static const SDL_Color LINK_GHOST = {0xff, 0xc8, 0x3a, 140};

static void DrawDoorPreview(const AutoMapView &view, const AutoMapLink &edge)
{
  const AutoMapShownCell *shown = ShownCellAt(shownDrawList, edge.dstX, edge.dstY);
  if (shown == NULL || shown->cell.kind != AutoMapCellDoor) return;
  int predicted = AutoMapScene_PredictDoorState(shown->cell.doorState, edge.signal);
  if (predicted < 0) return;
  SDL_Rect cell = CellArea(view, edge.dstX, edge.dstY);
  SDL_Rect firstLeaf, secondLeaf;
  DoorLeafRects(cell, shown->cell.runsNorthSouth, (AutoMapDoorState)predicted, &firstLeaf,
                &secondLeaf);
  Fill(firstLeaf, LINK_GHOST);
  Fill(secondLeaf, LINK_GHOST);
  Outline(firstLeaf, LINK_BADGE);
  Outline(secondLeaf, LINK_BADGE);
}

static void DrawTeleporterTarget(const AutoMapView &view);
static void DrawLegendText(int x, int y, const char *text, const SDL_Color &colour);

// The cyan destination arrow belongs to teleporters that fire: a dormant cell
// carries a hypothetical target for the tooltip text but must draw no arrow.
static bool IsFiringTeleporterCell(const AutoMapShownCell *shown)
{
  return shown != NULL
      && (shown->cell.kind == AutoMapCellTeleporter
          || shown->cell.kind == AutoMapCellInvisibleTeleporter)
      && shown->teleporterTargetVisible;
}

// Spread parallel edges sideways so N of them stay countable
static float ParallelOffset(const AutoMapView &view, int index, int count)
{
  return ((float)index - (float)(count - 1) / 2.0f) * std::max(2.0f, view.cellSize * 0.08f);
}

static const std::vector<AutoMapLink> NO_EDGES;

// How many of the first `before` edges run between the same two cells as edge
static int EdgesBetweenTheSameCells(const std::vector<AutoMapLink> &edges, const AutoMapLink &edge,
                                    size_t before)
{
  int count = 0;
  for (size_t j = 0; j < before && j < edges.size(); j++)
  {
    if (edges[j].srcX != edge.srcX || edges[j].srcY != edge.srcY
        || edges[j].dstX != edge.dstX || edges[j].dstY != edge.dstY) continue;
    count++;
  }
  return count;
}

// Edges between the same two cells are spread with ParallelOffset; a lone edge stays centred.
// The player's links between the same cells take their places after these (DrawPlayerEdgeGroups).
static void DrawEdgeGroups(const AutoMapView &view, const std::vector<AutoMapLink> &edges,
                           const SDL_Color &colour,
                           const std::vector<AutoMapLink> &playerEdges = NO_EDGES)
{
  for (size_t i = 0; i < edges.size(); i++)
  {
    int index = EdgesBetweenTheSameCells(edges, edges[i], i);
    int count = EdgesBetweenTheSameCells(edges, edges[i], edges.size())
              + EdgesBetweenTheSameCells(playerEdges, edges[i], playerEdges.size());
    DrawLinkEdge(view, edges[i], ParallelOffset(view, index, count), colour);
  }
}

// The player's links on hover, in the player colour, each beside the learned edges between the
// same two cells, so a guess that names a learned link shows both
static void DrawPlayerEdgeGroups(const AutoMapView &view, const std::vector<AutoMapLink> &playerEdges,
                                 const std::vector<AutoMapLink> &learnedEdges)
{
  for (size_t i = 0; i < playerEdges.size(); i++)
  {
    int learned = EdgesBetweenTheSameCells(learnedEdges, playerEdges[i], learnedEdges.size());
    int index = learned + EdgesBetweenTheSameCells(playerEdges, playerEdges[i], i);
    int count = learned + EdgesBetweenTheSameCells(playerEdges, playerEdges[i], playerEdges.size());
    DrawLinkEdge(view, playerEdges[i], ParallelOffset(view, index, count), PLAYER_NOTE, true);
  }
}

// The player's links of the hovered cell: the ones it starts, else the ones that end on it
static std::vector<AutoMapLink> PlayerLinksAtHover(bool *forward)
{
  std::vector<AutoMapLink> outgoing;
  std::vector<AutoMapLink> incoming;
  for (size_t i = 0; i < shownDrawList.playerLinks.size(); i++)
  {
    const AutoMapLink &link = shownDrawList.playerLinks[i];
    if (link.srcX == hoverX && link.srcY == hoverY) outgoing.push_back(link);
    else if (link.dstX == hoverX && link.dstY == hoverY) incoming.push_back(link);
  }
  *forward = !outgoing.empty();
  return *forward ? outgoing : incoming;
}

static void HighlightPlayerLinkEnds(const AutoMapView &view, const std::vector<AutoMapLink> &edges,
                                    bool forward)
{
  for (size_t i = 0; i < edges.size(); i++)
  {
    int otherX = forward ? edges[i].dstX : edges[i].srcX;
    int otherY = forward ? edges[i].dstY : edges[i].srcY;
    HighlightLinkedCell(view, otherX, otherY, PLAYER_NOTE);
  }
}

// Goals behind a press, depth first: the cells that get a badge. Relays in between get none.
static void CollectChainGoals(const std::vector<AutoMapEffect> &effects,
                              std::vector<const AutoMapEffect *> *goals)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    if (effects[i].kind == AutoMapEffectGoal) goals->push_back(&effects[i]);
    CollectChainGoals(effects[i].next, goals);
  }
}

// A goal's ring and badge. Yellow for what happens now or is done; the warning colour for
// what still waits on something, so a blocked door never reads like an open one.
static void DrawGoalBadge(const AutoMapView &view, const AutoMapEffect &goal)
{
  std::string badge = AutoMapScene_GoalBadge(goal.distance);
  if (badge.empty()) return;
  AutoMapGoalStatus status = goal.distance.status;
  bool good = status == AutoMapGoalOnThisPress || status == AutoMapGoalMore
           || status == AutoMapGoalAlreadyDone;
  const SDL_Color &colour = good ? LINK_BADGE : LINK_WARNING;
  SDL_Rect cell = CellArea(view, goal.x, goal.y);
  HighlightLinkedCell(view, goal.x, goal.y, colour);
  int width = AutoMapScene_TextWidth(badge.c_str()) * DIGIT_SCALE;
  SDL_Rect plate = {cell.x + (cell.w - width) / 2 - 2, cell.y + (cell.h - 5 * DIGIT_SCALE) / 2 - 2,
                    width + 4, 5 * DIGIT_SCALE + 4};
  Fill(plate, BACKGROUND);
  Outline(plate, colour);
  DrawLegendText(plate.x + 2, plate.y + 2, badge.c_str(), colour);
}

// At All a press lights its whole chain (every link down to the goals, a badge on each
// goal), and a relay shows its feeders in blue beside what it feeds. False when the
// hovered cell has neither, so the plain arrows run as they always did.
static bool DrawChainArrows(const AutoMapView &view, const std::vector<AutoMapLink> &playerEdges,
                            bool playerForward)
{
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(shownDrawList, hoverX, hoverY);
  bool relayCell = !AutoMapScene_RelayHoverLines(shownDrawList, hoverX, hoverY, false).empty();
  if (sources.empty() && !relayCell) return false;
  std::vector<AutoMapLink> chain = AutoMapScene_PressChainLinks(sources);
  std::vector<AutoMapLink> direct = AutoMapScene_LinksFrom(shownDrawList, hoverX, hoverY);
  std::vector<AutoMapLink> feeders;
  if (relayCell) feeders = AutoMapScene_LinksTo(shownDrawList, hoverX, hoverY);
  if (chain.empty()) chain = direct;
  if (chain.empty() && feeders.empty()) return false;
  HighlightLinkedCell(view, hoverX, hoverY);
  for (size_t i = 0; i < feeders.size(); i++)
  {
    HighlightLinkedCell(view, feeders[i].srcX, feeders[i].srcY, LINK_INCOMING);
  }
  DrawEdgeGroups(view, feeders, LINK_INCOMING, playerEdges);
  for (size_t i = 0; i < chain.size(); i++)
  {
    HighlightLinkedCell(view, chain[i].dstX, chain[i].dstY);
  }
  DrawEdgeGroups(view, chain, LINK_BADGE, playerEdges);
  std::vector<AutoMapLink> learned = feeders;
  learned.insert(learned.end(), chain.begin(), chain.end());
  HighlightPlayerLinkEnds(view, playerEdges, playerForward);
  DrawPlayerEdgeGroups(view, playerEdges, learned);
  // Door previews stay forward-only, and for the direct targets
  for (size_t i = 0; i < direct.size(); i++) DrawDoorPreview(view, direct[i]);
  std::vector<const AutoMapEffect *> goals;
  for (size_t i = 0; i < sources.size(); i++) CollectChainGoals(sources[i].effects, &goals);
  for (size_t i = 0; i < goals.size(); i++) DrawGoalBadge(view, *goals[i]);
  DrawTeleporterTarget(view);
  return true;
}

static void DrawLinkArrows(const AutoMapView &view)
{
  if (!hoverValid) return;
  // The player's links draw beside the learned ones at every spoiler level, All included
  bool playerForward = true;
  std::vector<AutoMapLink> playerEdges = PlayerLinksAtHover(&playerForward);
  if (shownDrawList.spoilers == AutoMapSpoilersAll
      && DrawChainArrows(view, playerEdges, playerForward)) return;
  std::vector<AutoMapLink> outgoing = AutoMapScene_LinksFrom(shownDrawList, hoverX, hoverY);
  std::vector<AutoMapLink> incoming = AutoMapScene_LinksTo(shownDrawList, hoverX, hoverY);
  std::vector<AutoMapLink> edges;
  if (!outgoing.empty()) edges = outgoing;
  else if (!incoming.empty()) edges = incoming;
  const AutoMapShownCell *hovered = ShownCellAt(shownDrawList, hoverX, hoverY);
  bool teleporter = IsFiringTeleporterCell(hovered);
  if (edges.empty() && !teleporter && playerEdges.empty()) return;
  bool hasLearned = !edges.empty() || teleporter;
  HighlightLinkedCell(view, hoverX, hoverY, hasLearned ? LINK_BADGE : PLAYER_NOTE);
  bool forward = !outgoing.empty();
  // One fan for both, the player's links last; with no player link it is the learned fan alone
  int fan = (int)(edges.size() + playerEdges.size());
  for (size_t i = 0; i < edges.size(); i++)
  {
    DrawLinkEdge(view, edges[i], ParallelOffset(view, (int)i, fan));
    int otherX = forward ? edges[i].dstX : edges[i].srcX;
    int otherY = forward ? edges[i].dstY : edges[i].srcY;
    HighlightLinkedCell(view, otherX, otherY);
  }
  HighlightPlayerLinkEnds(view, playerEdges, playerForward);
  for (size_t i = 0; i < playerEdges.size(); i++)
  {
    DrawLinkEdge(view, playerEdges[i], ParallelOffset(view, (int)(edges.size() + i), fan),
                 PLAYER_NOTE, true);
  }
  // Reverse hover shows every source but predicts nothing: with possibly conflicting
  // signals the effect is ambiguous by definition
  if (forward)
  {
    // A learned link has default fields: the preview would claim a door opens
    if (shownDrawList.spoilers == AutoMapSpoilersAll)
    {
      for (size_t i = 0; i < edges.size(); i++) DrawDoorPreview(view, edges[i]);
    }
  }
  DrawTeleporterTarget(view);
}

// A firing teleporter points at its destination: a cyan arrow when the victim lands on
// the shown level, the destination level's number (as in the title bar) when it lands
// elsewhere. Spinners need none of this: their circular arrow is the self-target.
static void DrawTeleporterTarget(const AutoMapView &view)
{
  const AutoMapShownCell *shown = ShownCellAt(shownDrawList, hoverX, hoverY);
  if (!IsFiringTeleporterCell(shown)) return;
  int dstLevel = shown->cell.teleporterDestLevel;
  int dstX = shown->cell.teleporterDestX;
  int dstY = shown->cell.teleporterDestY;
  SDL_Rect src = CellArea(view, shown->x, shown->y);
  if (dstLevel == shownDrawList.level)
  {
    SDL_Rect dst = CellArea(view, dstX, dstY);
    if (dstX == shown->x && dstY == shown->y)
    {
      Outline(Inset(dst, 25), SPINNER);
      return;
    }
    FloatPoint from = {(float)src.x + src.w / 2.0f, (float)src.y + src.h / 2.0f};
    FloatPoint to = {(float)dst.x + dst.w / 2.0f, (float)dst.y + dst.h / 2.0f};
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float length = sqrt(dx * dx + dy * dy);
    float headSize = std::max(6.0f, view.cellSize * 0.25f);
    float backOff = headSize * 0.9f / length;
    FloatPoint shaftEnd = {to.x - dx * backOff, to.y - dy * backOff};
    DrawArrowShaft(from, shaftEnd, SPINNER);
    DrawArrowHead(to, atan2(dx, -dy), headSize, SPINNER);
    HighlightLinkedCell(view, dstX, dstY, SPINNER);
    return;
  }
  DrawLevelPlate(src, dstLevel);
}

static int DigitAdvance(void)
{
  return (3 + 1) * DIGIT_SCALE;
}

static int DecimalDigits(int value)
{
  int digits = 1;
  while (value >= 10)
  {
    digits++;
    value /= 10;
  }
  return digits;
}

static int NumberPixelWidth(int value)
{
  return DecimalDigits(value) * DigitAdvance() - DIGIT_SCALE;
}

static void DrawDigit(int x, int y, int digit)
{
  for (int row = 0; row < 5; row++)
  {
    for (int col = 0; col < 3; col++)
    {
      if (!AutoMapScene_DigitPixel(digit, row, col)) continue;
      SDL_Rect pixel = {x + col * DIGIT_SCALE, y + row * DIGIT_SCALE, DIGIT_SCALE, DIGIT_SCALE};
      SDL_RenderFillRect(mapRenderer, &pixel);
    }
  }
}

// Global coordinates stay below 1000: the offsets are bytes and the levels are small
static void DrawNumberCentred(int centreX, int y, int value)
{
  int digits[4];
  int count = 0;
  do
  {
    digits[count++] = value % 10;
    value /= 10;
  } while (value > 0 && count < 4);
  int x = centreX - (count * DigitAdvance() - DIGIT_SCALE) / 2;
  for (int i = count - 1; i >= 0; i--)
  {
    DrawDigit(x, y, digits[i]);
    x += DigitAdvance();
  }
}

// Tile-contents tooltip: an icon grid of what the hovered cell shows. Each slot is
// sized like a map tile (a single stack reads exactly like a single-item cell).
// When the plate would outgrow the map area the slots shrink first, then narrow
// columns and finally the stack list give way (leftovers fold into the "+N" slot).
// Items come grouped from the full (untruncated) pile; pad requirements arrive
// as NEEDS slots from the scene, monsters, missiles and clouds join from the
// same filtered draw list, so the tooltip never shows what the map itself
// hides. Counts use the ruler digit font: no text backend needed.
static const int TOOLTIP_COLUMNS = 5;
static const int TOOLTIP_MAX_CELLS = 30;
static const int TOOLTIP_MIN_SLOT = 28;
static const int TOOLTIP_DEFAULT_SLOT = 40;
static const int TOOLTIP_PAD = 6;
static const int TOOLTIP_CURSOR_GAP = 18;

// One drawn grid cell: chest contents nest right after their chest, carried gear
// and death residue under the monster figures. Chance drops nest without an
// outline: predicted, not present. Lock and pad requirements keep their side
// and read as needs, never merging with present items.
struct TooltipCell
{
  AutoMapItem item;
  int count;
  bool nested;
  bool predicted;
  bool required;
  bool carried;
  int side;
};

static void MergeTooltipItem(std::vector<AutoMapTooltipSlot> *slots, const AutoMapItem &item,
                             int count)
{
  if (item.worldPicture == AUTOMAP_NO_PICTURE || count <= 0) return;
  for (size_t i = 0; i < slots->size(); i++)
  {
    if (!(*slots)[i].isChest && !(*slots)[i].required && (*slots)[i].item == item)
    {
      (*slots)[i].count += count;
      return;
    }
  }
  AutoMapTooltipSlot slot;
  slot.item = item;
  slot.count = count;
  slot.isChest = false;
  slots->push_back(slot);
}

static std::vector<AutoMapTooltipSlot> BuildTooltipSlots(int x, int y)
{
  std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(shownDrawList, x, y);
  // Group figures per monster (never with floor items: carried gear nests under
  // the group instead). The reader keeps one group per cell at most, so the
  // cell's possessions and death residue hang off the first figure slot.
  std::vector<AutoMapItemStack> possessions = AutoMapScene_MonsterPossessions(shownDrawList, x, y);
  AutoMapMonsterDrops drops = AutoMapScene_MonsterDrops(shownDrawList, x, y);
  bool monsterGearHung = possessions.empty() && drops.certain.empty() && drops.chance.empty();
  for (size_t i = 0; i < shownDrawList.monsters.size(); i++)
  {
    const AutoMapShownMonster &monster = shownDrawList.monsters[i];
    if (monster.x != x || monster.y != y) continue;
    size_t base = slots.size();
    for (int member = 0; member < monster.memberCount; member++)
    {
      int picture = monster.memberPictures[member].picture;
      if (picture == AUTOMAP_NO_PICTURE) continue;
      AutoMapItem figureItem;
      figureItem.worldPicture = picture;
      bool counted = false;
      for (size_t s = base; s < slots.size(); s++)
      {
        if (!slots[s].isChest && slots[s].item == figureItem)
        {
          slots[s].count++;
          counted = true;
          break;
        }
      }
      if (!counted)
      {
        AutoMapTooltipSlot figure;
        figure.item = figureItem;
        figure.count = 1;
        figure.isChest = false;
        slots.push_back(figure);
      }
    }
    if (!monsterGearHung && slots.size() > base)
    {
      slots[base].contents = possessions;
      slots[base].uncertainContents = drops.chance;
      // Guaranteed residue reads as carried: same nested slot, outlined.
      for (size_t d = 0; d < drops.certain.size(); d++)
      {
        slots[base].contents.push_back(drops.certain[d]);
      }
      monsterGearHung = true;
    }
  }
  if (!monsterGearHung)
  {
    // No figure slot to hang gear on: show it plain rather than dropping it.
    for (size_t p = 0; p < possessions.size(); p++)
    {
      MergeTooltipItem(&slots, possessions[p].item, possessions[p].count);
    }
    for (size_t p = 0; p < drops.certain.size(); p++)
    {
      MergeTooltipItem(&slots, drops.certain[p].item, drops.certain[p].count);
    }
    for (size_t p = 0; p < drops.chance.size(); p++)
    {
      MergeTooltipItem(&slots, drops.chance[p].item, drops.chance[p].count);
    }
  }
  for (size_t i = 0; i < shownDrawList.missiles.size(); i++)
  {
    if (shownDrawList.missiles[i].x == x && shownDrawList.missiles[i].y == y)
    {
      MergeTooltipItem(&slots, shownDrawList.missiles[i].item, 1);
    }
  }
  for (size_t i = 0; i < shownDrawList.clouds.size(); i++)
  {
    if (shownDrawList.clouds[i].x == x && shownDrawList.clouds[i].y == y)
    {
      AutoMapItem cloud;
      cloud.worldPicture = shownDrawList.clouds[i].picture;
      MergeTooltipItem(&slots, cloud, 1);
    }
  }
  // Pad requirements arrive as NEEDS slots from AutoMapScene_TooltipSlots
  // (CARRIED vs ON PAD), so nothing is merged here: monsters, missiles and
  // clouds join as plain items only.
  return slots;
}

// Flattened cells in draw order: each chest or figure followed by its nested contents.
static std::vector<TooltipCell> FlattenTooltipSlots(const std::vector<AutoMapTooltipSlot> &slots)
{
  std::vector<TooltipCell> cells;
  for (size_t i = 0; i < slots.size(); i++)
  {
    TooltipCell chest;
    chest.item = slots[i].item;
    chest.count = slots[i].count;
    chest.nested = false;
    chest.predicted = false;
    chest.required = slots[i].required;
    chest.carried = slots[i].carried;
    chest.side = slots[i].side;
    cells.push_back(chest);
    for (size_t c = 0; c < slots[i].contents.size(); c++)
    {
      TooltipCell held;
      held.item = slots[i].contents[c].item;
      held.count = slots[i].contents[c].count;
      held.nested = true;
      held.predicted = false;
      held.required = false;
      held.carried = false;
      held.side = -1;
      cells.push_back(held);
    }
    for (size_t c = 0; c < slots[i].uncertainContents.size(); c++)
    {
      TooltipCell maybe;
      maybe.item = slots[i].uncertainContents[c].item;
      maybe.count = slots[i].uncertainContents[c].count;
      maybe.nested = true;
      maybe.predicted = true;
      maybe.required = false;
      maybe.carried = false;
      maybe.side = -1;
      cells.push_back(maybe);
    }
  }
  return cells;
}

static void DrawCountBadge(const SDL_Rect &slot, int count)
{
  if (count <= 1) return;
  int badgeW = NumberPixelWidth(count) + 6;
  int badgeH = 5 * DIGIT_SCALE + 4;
  SDL_Rect badge = {slot.x + slot.w - badgeW - 2, slot.y + slot.h - badgeH - 2, badgeW, badgeH};
  Fill(badge, BACKGROUND);
  Outline(badge, ITEM_MARKER);
  UseColour(ITEM_MARKER);
  DrawNumberCentred(badge.x + badge.w / 2, badge.y + 2, count);
}

static void DrawNestedItem(const SDL_Rect &slot, const AutoMapItem &item, bool predicted)
{
  SDL_Rect inner = Inset(slot, 18);
  DrawTooltipIcon(inner, item);
  if (!predicted) Outline(slot, ITEM_MARKER);
}

// Footer lines per shown group member, in group order: the hit points at All, the damage the
// game showed below it. DrawLegendText renders them with the same 3x5 font as the legend.
static std::vector<std::string> BuildTooltipHealthLines(int x, int y)
{
  return AutoMapScene_MonsterHealthLines(shownDrawList, x, y);
}

// One footer line per named item stack, in tooltip order: "3X SKELETON KEY".
// Lock requirements read as needs on their side: "NEEDS SOLID KEY (S)".
// Pad requirements carry no side and name where the item must be:
// "NEEDS CARRIED FIRESTAFF" (in the inventory) vs "NEEDS ON PAD COIN"
// (lying on the pad).
// Names come from the engine's object names; the 3x5 font renders lowercase as
// uppercase and blanks what it has no glyph for. Lines are truncated to maxLineW.
static char SideInitial(int side)
{
  switch (side)
  {
  case AutoMapNorth: return 'N';
  case AutoMapEast: return 'E';
  case AutoMapSouth: return 'S';
  case AutoMapWest: return 'W';
  default: return '\0';
  }
}

static std::vector<std::string> BuildTooltipNameLines(const std::vector<TooltipCell> &cells,
                                                      int maxLineW)
{
  std::vector<std::string> lines;
  for (size_t i = 0; i < cells.size(); i++)
  {
    if (cells[i].item.nameIndex < 0) continue;
    const char *name = AutoMapLevelReader_ItemName(cells[i].item.nameIndex);
    if (name == NULL || name[0] == '\0') continue;
    char line[64];
    if (cells[i].required && cells[i].side == -1 && cells[i].count > 1)
    {
      if (cells[i].carried) std::snprintf(line, sizeof(line), "%dx NEEDS CARRIED %s", cells[i].count, name);
      else std::snprintf(line, sizeof(line), "%dx NEEDS ON PAD %s", cells[i].count, name);
    }
    else if (cells[i].required && cells[i].side == -1)
    {
      if (cells[i].carried) std::snprintf(line, sizeof(line), "NEEDS CARRIED %s", name);
      else std::snprintf(line, sizeof(line), "NEEDS ON PAD %s", name);
    }
    else if (cells[i].required && cells[i].count > 1)
    {
      std::snprintf(line, sizeof(line), "%dx NEEDS %s", cells[i].count, name);
    }
    else if (cells[i].required) std::snprintf(line, sizeof(line), "NEEDS %s", name);
    else if (cells[i].count > 1) std::snprintf(line, sizeof(line), "%dx %s", cells[i].count, name);
    else std::snprintf(line, sizeof(line), "%s", name);
    char sideInitial = SideInitial(cells[i].side);
    if (sideInitial != '\0')
    {
      size_t length = 0;
      while (line[length] != '\0') length++;
      if (length + 4 < sizeof(line))
      {
        line[length] = ' ';
        line[length + 1] = '(';
        line[length + 2] = sideInitial;
        line[length + 3] = ')';
        line[length + 4] = '\0';
      }
    }
    std::string shortened = line;
    while (!shortened.empty()
           && AutoMapScene_TextWidth(shortened.c_str()) * DIGIT_SCALE > maxLineW)
    {
      shortened.erase(shortened.size() - 1);
    }
    if (!shortened.empty()) lines.push_back(shortened);
  }
  return lines;
}

// Excerpt lines for the tile tooltip: scroll texts first, then wall texts with
// their side. Capped so one tile never grows the plate without bound.
static std::vector<std::string> BuildTooltipTextLines(
    const std::vector<std::string> &scrolls, const std::vector<AutoMapWallText> &walls,
    int maxLineW)
{
  static const size_t MAX_TEXT_LINES = 4;
  std::vector<std::string> full;
  for (size_t i = 0; i < scrolls.size(); i++) full.push_back("SCROLL: " + scrolls[i]);
  for (size_t i = 0; i < walls.size(); i++)
  {
    char sideInitial = SideInitial(walls[i].side);
    std::string line = "WALL";
    if (sideInitial != '\0')
    {
      line += " (";
      line += sideInitial;
      line += ")";
    }
    line += ": " + walls[i].text;
    full.push_back(line);
  }
  std::vector<std::string> lines;
  for (size_t i = 0; i < full.size() && lines.size() < MAX_TEXT_LINES; i++)
  {
    std::string shortened = full[i];
    while (!shortened.empty()
           && AutoMapScene_TextWidth(shortened.c_str()) * DIGIT_SCALE > maxLineW)
    {
      shortened.erase(shortened.size() - 1);
    }
    if (!shortened.empty()) lines.push_back(shortened);
  }
  return lines;
}

static void DrawTileTooltip(const AutoMapView &view)
{
  // Set again below once the footer is known; early returns must not leave a stale max
  // that would swallow the zoom
  tooltipScrollMax = 0;
  if (!hoverValid || middleDragging) return;
  std::vector<TooltipCell> cells = FlattenTooltipSlots(BuildTooltipSlots(hoverX, hoverY));
  std::vector<std::string> hpLines = BuildTooltipHealthLines(hoverX, hoverY);
  std::vector<std::string> scrolls = AutoMapScene_ScrollTexts(shownDrawList, hoverX, hoverY);
  std::vector<AutoMapWallText> walls = AutoMapScene_WallTexts(shownDrawList, hoverX, hoverY);
  std::vector<std::string> triggers =
      AutoMapScene_TriggerLines(shownDrawList, hoverX, hoverY, AutoMapLevelReader_ItemName);
  std::vector<std::string> teleporter = AutoMapScene_TeleporterLines(shownDrawList, hoverX, hoverY);
  std::vector<std::string> pit = AutoMapScene_PitLines(shownDrawList, hoverX, hoverY);
  std::vector<std::string> relay = AutoMapScene_RelayLines(shownDrawList, hoverX, hoverY);
  // Shift unfolds the compact press lines into the full chain; the tooltip redraws every
  // frame, so no event handler is needed.
  bool fullChain = (SDL_GetModState() & KMOD_SHIFT) != 0;
  if (!fullChain) tooltipScrollLines = 0;
  std::vector<std::string> press = AutoMapScene_PressLines(
      shownDrawList, AutoMapScene_PressEffects(shownDrawList, hoverX, hoverY), fullChain);
  std::vector<std::string> relayHover =
      AutoMapScene_RelayHoverLines(shownDrawList, hoverX, hoverY, fullChain);
  std::vector<std::string> notes = AutoMapScene_PlayerNoteLines(shownDrawList, hoverX, hoverY);
  if (cells.empty() && hpLines.empty() && scrolls.empty() && walls.empty() && triggers.empty()
      && teleporter.empty() && pit.empty() && relay.empty() && press.empty()
      && relayHover.empty() && notes.empty())
  {
    return;
  }
  int windowWidth = 0, windowHeight = 0;
  SDL_GetRendererOutputSize(mapRenderer, &windowWidth, &windowHeight);
  int maxW = windowWidth - 2 * RULER - 16;
  int maxH = windowHeight - 2 * RULER - 16;
  if (maxW < 60) maxW = 60;
  if (maxH < 60) maxH = 60;
  // Footer: position first, then HP lines, then one name line per named stack,
  // left-aligned below the icon grid. The footer keeps a fixed text scale; the icon grid
  // shrinks into what is left.
  std::vector<std::string> footerLines;
  {
    int gx = 0, gy = 0;
    AutoMapScene_GlobalPosition(shownDrawList, hoverX, hoverY, &gx, &gy);
    char pos[64];
    std::snprintf(pos, sizeof(pos), "POS: L%d (%d, %d)", shownDrawList.level, gx, gy);
    footerLines.push_back(pos);
  }
  footerLines.insert(footerLines.end(), hpLines.begin(), hpLines.end());
  std::vector<std::string> nameLines =
      BuildTooltipNameLines(cells, maxW - 2 * TOOLTIP_PAD);
  footerLines.insert(footerLines.end(), nameLines.begin(), nameLines.end());
  footerLines.insert(footerLines.end(), triggers.begin(), triggers.end());
  footerLines.insert(footerLines.end(), teleporter.begin(), teleporter.end());
  footerLines.insert(footerLines.end(), pit.begin(), pit.end());
  footerLines.insert(footerLines.end(), relay.begin(), relay.end());
  footerLines.insert(footerLines.end(), press.begin(), press.end());
  footerLines.insert(footerLines.end(), relayHover.begin(), relayHover.end());
  std::vector<std::string> textLines =
      BuildTooltipTextLines(scrolls, walls, maxW - 2 * TOOLTIP_PAD);
  footerLines.insert(footerLines.end(), textLines.begin(), textLines.end());
  // The player's own notes come last: they are not what the map knows
  footerLines.insert(footerLines.end(), notes.begin(), notes.end());
  int footerTextH = 5 * DIGIT_SCALE;
  int footerLineGap = 4;
  int footerTextW = 0;
  if (fullChain && !footerLines.empty())
  {
    // The uncapped footer may be taller than the window allows next to a minimal icon
    // grid. Show a window of it, scrollable by the wheel; the first and last visible line
    // say how many lines are hidden past that edge.
    int footerPad = cells.empty() ? 2 * TOOLTIP_PAD : TOOLTIP_PAD;
    int room = maxH - (cells.empty() ? 0 : 60) - footerPad;
    int visible = (room + footerLineGap) / (footerTextH + footerLineGap);
    if (visible < 1) visible = 1;
    int total = (int)footerLines.size();
    if (total > visible)
    {
      // The plate keeps its width while scrolling: measure every line and the widest notes
      for (size_t i = 0; i < footerLines.size(); i++)
      {
        int w = AutoMapScene_TextWidth(footerLines[i].c_str()) * DIGIT_SCALE;
        if (w > footerTextW) footerTextW = w;
      }
      char widest[32];
      std::snprintf(widest, sizeof(widest), "+%d MORE BELOW", total);
      int noteW = AutoMapScene_TextWidth(widest) * DIGIT_SCALE;
      if (noteW > footerTextW) footerTextW = noteW;
      std::snprintf(widest, sizeof(widest), "+%d MORE ABOVE", total);
      noteW = AutoMapScene_TextWidth(widest) * DIGIT_SCALE;
      if (noteW > footerTextW) footerTextW = noteW;
      tooltipScrollMax = total - visible;
      if (tooltipScrollLines > tooltipScrollMax) tooltipScrollLines = tooltipScrollMax;
      std::vector<std::string> window(footerLines.begin() + tooltipScrollLines,
                                      footerLines.begin() + tooltipScrollLines + visible);
      char note[32];
      int below = total - tooltipScrollLines - visible;
      // The notes replace content lines, so only a window of 3 or more takes them; a
      // smaller one shows content only and every line stays reachable by scrolling.
      if (visible >= 3 && below > 0)
      {
        std::snprintf(note, sizeof(note), "+%d MORE BELOW", below + 1);
        window.back() = note;
      }
      if (visible >= 3 && tooltipScrollLines > 0)
      {
        std::snprintf(note, sizeof(note), "+%d MORE ABOVE", tooltipScrollLines + 1);
        window.front() = note;
      }
      footerLines = window;
    }
  }
  for (size_t i = 0; i < footerLines.size(); i++)
  {
    int w = AutoMapScene_TextWidth(footerLines[i].c_str()) * DIGIT_SCALE;
    if (w > footerTextW) footerTextW = w;
  }
  int footerH = footerLines.empty() ? 0 : (int)footerLines.size() * footerTextH
      + ((int)footerLines.size() - 1) * footerLineGap + (cells.empty() ? 2 * TOOLTIP_PAD : TOOLTIP_PAD);
  int iconMaxH = maxH - footerH;
  if (iconMaxH < 60 && !cells.empty()) iconMaxH = 60;
  // Start at full map-tile size so one stack reads like a single-item cell at any
  // zoom. Oversized multi-stack plates shrink back down to the map area below.
  int slot = view.cellSize <= 0 ? TOOLTIP_DEFAULT_SLOT : view.cellSize;
  if (slot < TOOLTIP_DEFAULT_SLOT) slot = TOOLTIP_DEFAULT_SLOT;
  bool capped = (int)cells.size() > TOOLTIP_MAX_CELLS;
  size_t shown = capped ? (size_t)(TOOLTIP_MAX_CELLS - 1) : cells.size();
  int leftover = 0;
  if (capped)
  {
    for (size_t i = shown; i < cells.size(); i++) leftover += cells[i].count;
  }
  size_t slots = shown + (capped ? 1 : 0);
  int columns = 0;
  int rows = 0;
  if (!cells.empty())
  {
    columns = std::min(TOOLTIP_COLUMNS, (int)slots);
    rows = ((int)slots + columns - 1) / columns;
    // Too big: shrink the slots first.
    while ((columns * slot + 2 * TOOLTIP_PAD > maxW || rows * slot + 2 * TOOLTIP_PAD > iconMaxH)
           && slot > TOOLTIP_MIN_SLOT)
    {
      slot -= 2;
    }
    // Still too wide: narrow the columns.
    while (columns > 1 && columns * slot + 2 * TOOLTIP_PAD > maxW)
    {
      columns--;
      rows = ((int)slots + columns - 1) / columns;
    }
    // Still too tall: drop rows, folding them into the "+N" slot.
    int maxRows = (iconMaxH - 2 * TOOLTIP_PAD) / slot;
    if (maxRows < 1) maxRows = 1;
    if (rows > maxRows)
    {
      size_t capacity = (size_t)maxRows * (size_t)columns;
      if (capacity < 1) capacity = 1;
      size_t keep = capacity - 1;
      if (keep > shown) keep = shown;
      for (size_t i = keep; i < shown; i++) leftover += cells[i].count;
      shown = keep;
      capped = true;
      slots = shown + 1;
      rows = maxRows;
    }
  }
  bool truncated = capped;
  int iconW = cells.empty() ? 0 : columns * slot + 2 * TOOLTIP_PAD;
  int iconH = cells.empty() ? 0 : rows * slot + 2 * TOOLTIP_PAD;
  int plateW = iconW;
  if (!footerLines.empty() && footerTextW + 2 * TOOLTIP_PAD > plateW)
  {
    plateW = footerTextW + 2 * TOOLTIP_PAD;
  }
  int plateH = iconH + footerH;
  if (plateW < 60) plateW = 60;
  int x = hoverPixelX + TOOLTIP_CURSOR_GAP;
  int y = hoverPixelY + TOOLTIP_CURSOR_GAP;
  if (x + plateW > windowWidth - RULER) x = hoverPixelX - TOOLTIP_CURSOR_GAP - plateW;
  if (y + plateH > windowHeight - RULER) y = hoverPixelY - TOOLTIP_CURSOR_GAP - plateH;
  if (x < RULER) x = RULER;
  if (y < RULER) y = RULER;
  SDL_Rect plate = {x, y, plateW, plateH};
  Fill(plate, BACKGROUND);
  Outline(plate, ITEM_MARKER);
  for (size_t i = 0; i < shown; i++)
  {
    int col = (int)i % columns;
    int row = (int)i / columns;
    SDL_Rect icon = {x + TOOLTIP_PAD + col * slot, y + TOOLTIP_PAD + row * slot, slot, slot};
    if (cells[i].nested) DrawNestedItem(icon, cells[i].item, cells[i].predicted);
    else DrawTooltipIcon(icon, cells[i].item);
    // A requirement is outlined white with a keyhole where the count badge would
    // sit unreadably small: needs read as keyholes, haves as plain icons.
    if (cells[i].required && !cells[i].nested)
    {
      Outline(icon, WHITE);
      int badge = std::max(10, icon.w * 40 / 100);
      SDL_Rect corner = {icon.x + 2, icon.y + 2, badge, badge};
      DrawKeyholeMark(corner);
    }
    DrawCountBadge(icon, cells[i].count);
  }
  if (truncated && columns > 0)
  {
    int col = (int)shown % columns;
    int row = (int)shown / columns;
    SDL_Rect icon = {x + TOOLTIP_PAD + col * slot, y + TOOLTIP_PAD + row * slot, slot, slot};
    DrawMoreItemsMark(icon);
    UseColour(ITEM_MARKER);
    DrawNumberCentred(icon.x + icon.w / 2, icon.y + icon.h - 2 - 5 * DIGIT_SCALE, leftover);
  }
  for (size_t i = 0; i < footerLines.size(); i++)
  {
    int lineY = y + iconH + (cells.empty() ? TOOLTIP_PAD : 0)
        + (int)i * (footerTextH + footerLineGap);
    DrawLegendText(x + TOOLTIP_PAD, lineY, footerLines[i].c_str(), WHITE);
  }
}

// The rulers label the global coordinates, aligned to multiples of the label step so that
// the numbers stand still while the view pans
static void DrawColumnRulers(const AutoMapView &view, int mapWidth, int windowHeight)
{
  int first = 0, last = -1;
  AutoMapScene_VisibleRange(view.originX - RULER, view.cellSize, mapWidth, shownDrawList.width,
                            &first, &last);
  if (last < first) return;
  int widest = shownDrawList.width - 1 + shownDrawList.offsetX;
  int step = AutoMapScene_LabelStep(view.cellSize, NumberPixelWidth(widest) + 2 * DIGIT_SCALE);
  int textY = (RULER - 5 * DIGIT_SCALE) / 2;
  for (int x = first; x <= last + 1; x++)
  {
    int edge = view.originX + x * view.cellSize;
    int global = x + shownDrawList.offsetX;
    bool isHovered = hoverValid && !middleDragging && x == hoverX && x <= last;
    bool isLabelled = (x <= last && global % step == 0) || isHovered;
    if (isHovered)
    {
      // Hard to miss: a gold plate with dark digits top and bottom plus an edge
      // line running the full ruler depth, a few pixels wide.
      UseColour(LINK_BADGE);
      for (int dx = -1; dx <= 1; dx++)
      {
        SDL_RenderDrawLine(mapRenderer, edge + dx, 0, edge + dx, RULER);
        SDL_RenderDrawLine(mapRenderer, edge + dx, windowHeight - RULER, edge + dx,
                           windowHeight - 1);
      }
      int plateW = NumberPixelWidth(global) + 6;
      int plateH = 5 * DIGIT_SCALE + 4;
      int centreX = edge + view.cellSize / 2;
      SDL_Rect topPlate = {centreX - plateW / 2, textY - 2, plateW, plateH};
      SDL_Rect bottomPlate = {centreX - plateW / 2, windowHeight - RULER + textY - 2,
                              plateW, plateH};
      Fill(topPlate, LINK_BADGE);
      Fill(bottomPlate, LINK_BADGE);
      UseColour(BACKGROUND);
      DrawNumberCentred(centreX, textY, global);
      DrawNumberCentred(centreX, windowHeight - RULER + textY, global);
      continue;
    }
    UseColour(isLabelled ? RULER_TEXT : RULER_TICK);
    int tick = isLabelled ? 9 : 5;
    SDL_RenderDrawLine(mapRenderer, edge, 0, edge, tick);
    SDL_RenderDrawLine(mapRenderer, edge, windowHeight - 1 - tick, edge, windowHeight - 1);
    if (!isLabelled) continue;
    UseColour(RULER_TEXT);
    DrawNumberCentred(edge + view.cellSize / 2, textY, global);
    DrawNumberCentred(edge + view.cellSize / 2, windowHeight - RULER + textY, global);
  }
}

static void DrawRowRulers(const AutoMapView &view, int mapHeight, int windowWidth)
{
  int first = 0, last = -1;
  AutoMapScene_VisibleRange(view.originY - RULER, view.cellSize, mapHeight, shownDrawList.height,
                            &first, &last);
  if (last < first) return;
  int widest = shownDrawList.height - 1 + shownDrawList.offsetY;
  int step = AutoMapScene_LabelStep(view.cellSize, NumberPixelWidth(widest) + 2 * DIGIT_SCALE);
  for (int y = first; y <= last + 1; y++)
  {
    int edge = view.originY + y * view.cellSize;
    int global = y + shownDrawList.offsetY;
    bool isHovered = hoverValid && !middleDragging && y == hoverY && y <= last;
    bool isLabelled = (y <= last && global % step == 0) || isHovered;
    if (isHovered)
    {
      UseColour(LINK_BADGE);
      for (int dy = -1; dy <= 1; dy++)
      {
        SDL_RenderDrawLine(mapRenderer, 0, edge + dy, RULER, edge + dy);
        SDL_RenderDrawLine(mapRenderer, windowWidth - RULER, edge + dy, windowWidth - 1,
                           edge + dy);
      }
      int plateW = NumberPixelWidth(global) + 6;
      int plateH = 5 * DIGIT_SCALE + 4;
      int textY = edge + (view.cellSize - 5 * DIGIT_SCALE) / 2;
      SDL_Rect leftPlate = {RULER / 2 - plateW / 2, textY - 2, plateW, plateH};
      SDL_Rect rightPlate = {windowWidth - RULER / 2 - plateW / 2, textY - 2, plateW, plateH};
      Fill(leftPlate, LINK_BADGE);
      Fill(rightPlate, LINK_BADGE);
      UseColour(BACKGROUND);
      DrawNumberCentred(RULER / 2, textY, global);
      DrawNumberCentred(windowWidth - RULER / 2, textY, global);
      continue;
    }
    UseColour(isLabelled ? RULER_TEXT : RULER_TICK);
    int tick = isLabelled ? 9 : 5;
    SDL_RenderDrawLine(mapRenderer, 0, edge, tick, edge);
    SDL_RenderDrawLine(mapRenderer, windowWidth - 1 - tick, edge, windowWidth - 1, edge);
    if (!isLabelled) continue;
    UseColour(RULER_TEXT);
    int textY = edge + (view.cellSize - 5 * DIGIT_SCALE) / 2;
    DrawNumberCentred(RULER / 2, textY, global);
    DrawNumberCentred(windowWidth - RULER / 2, textY, global);
  }
}

static void DrawRulers(const AutoMapView &view, int windowWidth, int windowHeight)
{
  if (view.cellSize == 0) return;
  SDL_Rect top = {0, 0, windowWidth, RULER};
  SDL_Rect bottom = {0, windowHeight - RULER, windowWidth, RULER};
  SDL_Rect left = {0, RULER, RULER, windowHeight - 2 * RULER};
  SDL_Rect right = {windowWidth - RULER, RULER, RULER, windowHeight - 2 * RULER};
  Fill(top, RULER_BACKGROUND);
  Fill(bottom, RULER_BACKGROUND);
  Fill(left, RULER_BACKGROUND);
  Fill(right, RULER_BACKGROUND);
  DrawColumnRulers(view, windowWidth - 2 * RULER, windowHeight);
  DrawRowRulers(view, windowHeight - 2 * RULER, windowWidth);
}

// ---------------------------------------------------------------------------------------------
// Legend: what the map shows, in the same ruler-digit look. No text backend needed.
// Page 0 is off; click the ? button (or the open panel) to cycle ground -> things -> off.

static void DrawLegendText(int x, int y, const char *text, const SDL_Color &colour)
{
  if (text == NULL) return;
  UseColour(colour);
  for (int i = 0; text[i] != '\0'; i++)
  {
    for (int row = 0; row < 5; row++)
    {
      for (int col = 0; col < 3; col++)
      {
        if (!AutoMapScene_FontPixel(text[i], row, col)) continue;
        SDL_Rect pixel = {x + (i * 4 + col) * DIGIT_SCALE, y + row * DIGIT_SCALE,
                          DIGIT_SCALE, DIGIT_SCALE};
        SDL_RenderFillRect(mapRenderer, &pixel);
      }
    }
  }
}

enum LegendSwatch
{
  SW_WALL,
  SW_FLOOR,
  SW_PIT,
  SW_HIDDEN_PIT_OPEN,
  SW_HIDDEN_PIT_CLOSED,
  SW_STAIR_UP,
  SW_STAIR_DOWN,
  SW_DOOR,
  SW_SEALED_DOOR,
  SW_TELEPORTER,
  SW_FALSEWALL,
  SW_SPINNER,
  SW_DORMANT_SPINNER,
  SW_PARTY,
  SW_MONSTER,
  SW_ITEM,
  SW_MORE,
  SW_MISSILE,
  SW_CLOUD,
  SW_PAD,
  SW_GENERATOR,
  SW_LINK,
  SW_COUNT,
  SW_INSIDE,
  SW_MAYBE,
  // Legend-only glyphs: the swatches below reuse the real map draw routines
  // (Task 2 WYSIWYG). Operator/scope triangles reuse DrawOperatorTriangle etc.
  // enlarged to the swatch: on the map they sit in the bottom-left corner.
  SW_DESTROYED_DOOR,
  SW_OPEN_FALSEWALL,
  SW_FLOOR_PICTURE,
  SW_INVISIBLE_TELEPORTER,
  SW_DORMANT_TELEPORTER,
  SW_WALL_FEATURE,
  SW_WALL_WRITING,
  SW_WALL_FLAVOUR,
  SW_WALL_HIDDEN,
  SW_WALL_NOSPRITE,
  SW_WALL_LOCK,
  SW_WALL_STOCKED,
  SW_BREAK_AXE,
  SW_BREAK_FIRE,
  SW_BREAK_BOTH,
  SW_PAD_DORMANT,
  SW_PAD_ANYTHING,
  SW_PAD_PARTY,
  SW_PAD_FACE_N,
  SW_PAD_FACE_E,
  SW_PAD_FACE_S,
  SW_PAD_FACE_W,
  SW_PAD_PARTY_OR_MONSTER,
  SW_PAD_MONSTER,
  SW_PAD_OBJECT,
  SW_PAD_CARRIED,
  SW_GENERATOR_SPENT,
  SW_COG,
  SW_SCOPE_ANYTHING,
  SW_SCOPE_PARTY,
  SW_SCOPE_MONSTER,
  SW_SCOPE_OBJECT,
  SW_SPIN_ABSOLUTE,
  SW_SPIN_RIGHT,
  SW_SPIN_LEFT,
  SW_SPIN_AROUND,
  SW_SPIN_STRAIGHT,
  SW_LEVEL_PLATE,
  SW_LINK_SET,
  SW_LINK_ONCE,
  SW_LINK_CLEAR,
  SW_LINK_TOGGLE,
  SW_LINK_PRESSURE,
  SW_LINK_SELF,
  SW_LINK_GHOST,
  // Text-lines glyph for the hover-footer reminder rows on THINGS.
  SW_HOVER_TEXT
};

struct LegendRow
{
  LegendSwatch swatch;
  const char *label;
};

static const LegendRow LEGEND_GROUND[] = {
  {SW_WALL, "WALL"},
  {SW_FLOOR, "FLOOR"},
  {SW_PIT, "PIT"},
  {SW_HIDDEN_PIT_OPEN, "HIDDEN PIT OPEN"},
  {SW_HIDDEN_PIT_CLOSED, "HIDDEN PIT CLOSED"},
  {SW_STAIR_UP, "STAIRS UP"},
  {SW_STAIR_DOWN, "STAIRS DOWN"},
  {SW_DOOR, "DOOR"},
  {SW_SEALED_DOOR, "SEALED DOOR"},
  {SW_DESTROYED_DOOR, "DESTROYED DOOR"},
  {SW_FALSEWALL, "FALSE WALL"},
  {SW_OPEN_FALSEWALL, "OPEN FALSE WALL"},
  {SW_FLOOR_PICTURE, "FLOOR PICTURE"},
};

static const LegendRow LEGEND_WALLS[] = {
  {SW_WALL_FEATURE, "FEATURE"},
  {SW_WALL_NOSPRITE, "NO PICTURE"},
  {SW_WALL_WRITING, "WRITING"},
  {SW_WALL_FLAVOUR, "FLAVOUR TICK"},
  {SW_WALL_HIDDEN, "HIDDEN SWITCH"},
  {SW_WALL_LOCK, "NEEDS KEY"},
  {SW_WALL_STOCKED, "ALCOVE GOODS"},
  {SW_BREAK_AXE, "AXE"},
  {SW_BREAK_FIRE, "FIREBALL"},
  {SW_BREAK_BOTH, "AXE+FIRE"},
};

static const LegendRow LEGEND_PADS[] = {
  {SW_PAD, "PAD"},
  {SW_PAD_DORMANT, "DORMANT PAD"},
  {SW_PAD_ANYTHING, "FIRES ANYTHING"},
  {SW_PAD_PARTY, "PARTY"},
  {SW_PAD_FACE_N, "FACES N"},
  {SW_PAD_FACE_E, "FACES E"},
  {SW_PAD_FACE_S, "FACES S"},
  {SW_PAD_FACE_W, "FACES W"},
  {SW_PAD_PARTY_OR_MONSTER, "PARTY OR MONSTER"},
  {SW_PAD_MONSTER, "MONSTER"},
  {SW_PAD_OBJECT, "NEEDS ON PAD"},
  {SW_PAD_CARRIED, "NEEDS CARRIED"},
  {SW_GENERATOR, "GENERATOR"},
  {SW_GENERATOR_SPENT, "SPENT GENERATOR"},
  {SW_COG, "COUNTER/DSA"},
};

static const LegendRow LEGEND_TELEPORT[] = {
  {SW_TELEPORTER, "TELEPORTER"},
  {SW_INVISIBLE_TELEPORTER, "INVISIBLE"},
  {SW_DORMANT_TELEPORTER, "DORMANT"},
  {SW_SCOPE_ANYTHING, "TAKES ANYTHING"},
  {SW_SCOPE_PARTY, "TAKES PARTY"},
  {SW_SCOPE_MONSTER, "TAKES MONSTERS"},
  {SW_SCOPE_OBJECT, "TAKES OBJECTS ONLY"},
  {SW_SPINNER, "SPINNER"},
  {SW_DORMANT_SPINNER, "DORMANT SPINNER"},
  {SW_SPIN_ABSOLUTE, "SPIN FACE"},
  {SW_SPIN_RIGHT, "SPIN RIGHT"},
  {SW_SPIN_LEFT, "SPIN LEFT"},
  {SW_SPIN_AROUND, "SPIN AROUND"},
  {SW_SPIN_STRAIGHT, "SPIN STRAIGHT"},
  {SW_LEVEL_PLATE, "LEVEL PLATE"},
};

static const LegendRow LEGEND_LINKS[] = {
  {SW_LINK, "LINKED"},
  {SW_LINK_SET, "SET HOVER"},
  {SW_LINK_ONCE, "ONCE ONLY HOVER"},
  {SW_LINK_CLEAR, "CLEAR HOVER"},
  {SW_LINK_TOGGLE, "TOGGLE HOVER"},
  {SW_LINK_PRESSURE, "PRESSURE HOVER"},
  {SW_LINK_SELF, "SELF RING"},
  {SW_LINK_GHOST, "DOOR GHOST"},
};

static const LegendRow LEGEND_THINGS[] = {
  {SW_PARTY, "PARTY"},
  {SW_MONSTER, "MONSTER"},
  {SW_ITEM, "ITEM"},
  {SW_MORE, "MORE ITEMS"},
  {SW_MISSILE, "MISSILE"},
  {SW_CLOUD, "CLOUD"},
  {SW_COUNT, "COUNT 2"},
  {SW_INSIDE, "INSIDE"},
  {SW_MAYBE, "MAY DROP"},
  {SW_HOVER_TEXT, "HOVER: TRIGGER TAKES"},
  {SW_HOVER_TEXT, "HOVER: SPIN TARGET"},
};

// The lowest spoiler level that can draw the swatch, as the highest AutoMapSpoilerLevel
// value it is still shown at. AutoMapSpoilersAll is 0, so "lower" levels show more:
// 0 only at all, 1 from learned on, 2 at every level
static int LegendRowLastLevel(LegendSwatch swatch)
{
  switch (swatch)
  {
  case SW_COG:
  case SW_PAD_DORMANT:
  case SW_GENERATOR_SPENT:
  case SW_WALL_FLAVOUR:
  case SW_SEALED_DOOR:
  case SW_BREAK_AXE:
  case SW_BREAK_FIRE:
  case SW_BREAK_BOTH:
  case SW_DORMANT_TELEPORTER:
  case SW_DORMANT_SPINNER:
  case SW_PAD_PARTY:
  case SW_PAD_FACE_N:
  case SW_PAD_FACE_E:
  case SW_PAD_FACE_S:
  case SW_PAD_FACE_W:
  case SW_PAD_PARTY_OR_MONSTER:
  case SW_PAD_MONSTER:
  case SW_PAD_OBJECT:
  case SW_PAD_CARRIED:
  case SW_SCOPE_ANYTHING:
  case SW_SCOPE_PARTY:
  case SW_SCOPE_MONSTER:
  case SW_SCOPE_OBJECT:
  case SW_LINK_SET:
  case SW_LINK_ONCE:
  case SW_LINK_CLEAR:
  case SW_LINK_TOGGLE:
  case SW_LINK_PRESSURE:
  case SW_LINK_GHOST:
    return AutoMapSpoilersAll;
  case SW_PAD:
  case SW_GENERATOR:
  case SW_WALL_HIDDEN:
  case SW_FALSEWALL:
  case SW_OPEN_FALSEWALL:
  case SW_HIDDEN_PIT_OPEN:
  case SW_HIDDEN_PIT_CLOSED:
  case SW_INVISIBLE_TELEPORTER:
  case SW_SPINNER:
  case SW_LINK:
  case SW_LINK_SELF:
  case SW_PAD_ANYTHING:
  case SW_SPIN_ABSOLUTE:
  case SW_SPIN_RIGHT:
  case SW_SPIN_LEFT:
  case SW_SPIN_AROUND:
  case SW_SPIN_STRAIGHT:
  case SW_WALL_LOCK:
    return AutoMapSpoilersLearned;
  default:
    return AutoMapSpoilersSeen;
  }
}

// The rows of the current page that the shown draw list can draw
static const LegendRow *LegendRows(int *count)
{
  const LegendRow *table;
  int tableCount;
  switch (legendPage)
  {
  case 1:
    table = LEGEND_GROUND;
    tableCount = (int)(sizeof(LEGEND_GROUND) / sizeof(LEGEND_GROUND[0]));
    break;
  case 2:
    table = LEGEND_WALLS;
    tableCount = (int)(sizeof(LEGEND_WALLS) / sizeof(LEGEND_WALLS[0]));
    break;
  case 3:
    table = LEGEND_PADS;
    tableCount = (int)(sizeof(LEGEND_PADS) / sizeof(LEGEND_PADS[0]));
    break;
  case 4:
    table = LEGEND_TELEPORT;
    tableCount = (int)(sizeof(LEGEND_TELEPORT) / sizeof(LEGEND_TELEPORT[0]));
    break;
  case 5:
    table = LEGEND_LINKS;
    tableCount = (int)(sizeof(LEGEND_LINKS) / sizeof(LEGEND_LINKS[0]));
    break;
  default:
    table = LEGEND_THINGS;
    tableCount = (int)(sizeof(LEGEND_THINGS) / sizeof(LEGEND_THINGS[0]));
    break;
  }
  static std::vector<LegendRow> shownRows;
  shownRows.clear();
  for (int i = 0; i < tableCount; i++)
  {
    if ((int)shownDrawList.spoilers <= LegendRowLastLevel(table[i].swatch))
      shownRows.push_back(table[i]);
  }
  *count = (int)shownRows.size();
  return shownRows.empty() ? table : &shownRows[0];
}

static bool LegendPageHasRows(int page)
{
  int shownPage = legendPage;
  legendPage = page;
  int count = 0;
  LegendRows(&count);
  legendPage = shownPage;
  return count > 0;
}

static const char *LegendTitle(void)
{
  switch (legendPage)
  {
  case 1: return "GROUND 1/6";
  case 2: return "WALLS 2/6";
  case 3: return "PADS 3/6";
  case 4: return "TELEPORT 4/6";
  case 5: return "LINKS 5/6 HOVER";
  default: return "THINGS 6/6";
  }
}

static const int LEGEND_SWATCH_W = 24;
static const int LEGEND_SWATCH_H = 14;
static const int LEGEND_ROW_H = 20;
// Glyph rows (operators, scopes, spinners, links, diamonds) need a near-square
// swatch to read: taller row, taller box, same left edge so text stays aligned.
static const int LEGEND_TALL_ROW_H = 30;
static const int LEGEND_TALL_SWATCH_H = 22;

static bool LegendRowIsTall(LegendSwatch swatch)
{
  switch (swatch)
  {
  case SW_WALL_HIDDEN:
  case SW_PAD_PARTY:
  case SW_PAD_FACE_N:
  case SW_PAD_FACE_E:
  case SW_PAD_FACE_S:
  case SW_PAD_FACE_W:
  case SW_PAD_PARTY_OR_MONSTER:
  case SW_PAD_MONSTER:
  case SW_PAD_OBJECT:
  case SW_PAD_CARRIED:
  case SW_GENERATOR:
  case SW_GENERATOR_SPENT:
  case SW_COG:
  case SW_SCOPE_PARTY:
  case SW_SCOPE_MONSTER:
  case SW_SCOPE_OBJECT:
  case SW_SPINNER:
  case SW_DORMANT_SPINNER:
  case SW_SPIN_ABSOLUTE:
  case SW_SPIN_RIGHT:
  case SW_SPIN_LEFT:
  case SW_SPIN_AROUND:
  case SW_SPIN_STRAIGHT:
  case SW_LEVEL_PLATE:
  case SW_LINK_SET:
  case SW_LINK_ONCE:
  case SW_LINK_CLEAR:
  case SW_LINK_TOGGLE:
  case SW_LINK_PRESSURE:
  case SW_LINK_SELF:
  case SW_LINK_GHOST:
  case SW_HOVER_TEXT:
    return true;
  default:
    return false;
  }
}

static int LegendRowH(LegendSwatch swatch)
{
  return LegendRowIsTall(swatch) ? LEGEND_TALL_ROW_H : LEGEND_ROW_H;
}

static int LegendSwatchH(LegendSwatch swatch)
{
  return LegendRowIsTall(swatch) ? LEGEND_TALL_SWATCH_H : LEGEND_SWATCH_H;
}
static const int LEGEND_PAD = 8;
static const int LEGEND_TEXT_GAP = 8;

static SDL_Rect LegendPanelRect(void)
{
  int windowWidth = 0, windowHeight = 0;
  SDL_GetRendererOutputSize(mapRenderer, &windowWidth, &windowHeight);
  int count = 0;
  const LegendRow *rows = LegendRows(&count);
  int textW = AutoMapScene_TextWidth(LegendTitle());
  for (int i = 0; i < count; i++)
  {
    int rowW = AutoMapScene_TextWidth(rows[i].label);
    if (rowW > textW) textW = rowW;
  }
  int w = LEGEND_PAD * 2 + LEGEND_SWATCH_W + LEGEND_TEXT_GAP + textW * DIGIT_SCALE;
  int h = LEGEND_PAD * 2 + 5 * DIGIT_SCALE + 4;
  for (int i = 0; i < count; i++) h += LegendRowH(rows[i].swatch);
  int x = RULER + 6;
  int y = RULER + 6;
  if (x + w > windowWidth - RULER) x = windowWidth - RULER - w;
  if (y + h > windowHeight - RULER) y = windowHeight - RULER - h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  SDL_Rect plate = {x, y, w, h};
  return plate;
}

static SDL_Rect LegendButtonRect(void)
{
  SDL_Rect button = {3, 3, 24, 24};
  return button;
}

static bool PointInRect(int x, int y, const SDL_Rect &area)
{
  return x >= area.x && x < area.x + area.w && y >= area.y && y < area.y + area.h;
}

// A hover link shortened into the swatch: the same shaft/head/tick primitives
// as the map arrows, eastward with room for the head.
static void DrawLegendLinkShaft(const SDL_Rect &box, bool dashed, int headMode, bool withTick)
{
  float midY = box.y + box.h / 2.0f;
  FloatPoint from = {(float)(box.x + 2), midY};
  FloatPoint tip = {(float)(box.x + box.w - 2), midY};
  float headSize = 7.0f;
  FloatPoint shaftEnd = {tip.x - headSize * 0.9f, midY};
  if (dashed) DrawDashedShaft(from, shaftEnd);
  else DrawArrowShaft(from, shaftEnd);
  static const float EAST = 1.57079633f;
  if (headMode == 1) DrawHollowArrowHead(tip, EAST, headSize);
  else DrawArrowHead(tip, EAST, headSize);
  if (headMode == 2) DrawArrowHead(from, EAST + 3.14159265f, headSize * 0.7f);
  if (withTick) DrawPressureTick(from, shaftEnd);
}

static void DrawLegendSwatch(const SDL_Rect &box, LegendSwatch swatch)
{
  switch (swatch)
  {
  case SW_WALL:
    Fill(box, WALL);
    Outline(box, WALL_EDGE);
    break;
  case SW_FLOOR:
    Fill(box, FLOOR);
    Outline(box, FLOOR_EDGE);
    break;
  case SW_PIT:
    Fill(box, PIT_MARKER);
    Outline(box, PIT_MARKER_EDGE);
    break;
  case SW_HIDDEN_PIT_OPEN:
  {
    DrawFloor(box);
    Fill(Inset(box, 25), PIT_MARKER);
    DrawHiddenPitMark(box);
    int dot = std::max(2, box.w / 12);
    SDL_Rect centre = {box.x + (box.w - dot) / 2, box.y + (box.h - dot) / 2, dot, dot};
    Fill(centre, PIT_MARKER_EDGE);
    break;
  }
  case SW_HIDDEN_PIT_CLOSED:
    DrawFloor(box);
    DrawHiddenPitMark(box);
    break;
  case SW_STAIR_UP:
  case SW_STAIR_DOWN:
  {
    AutoMapCellRecord stairs;
    stairs.runsNorthSouth = true;
    stairs.stairsLeadUp = swatch == SW_STAIR_UP;
    DrawStairs(box, stairs);
    break;
  }
  case SW_DOOR:
  {
    DrawFloor(box);
    SDL_Rect firstLeaf, secondLeaf;
    DoorLeafRects(box, true, AutoMapDoorOpen, &firstLeaf, &secondLeaf);
    Fill(firstLeaf, DOOR);
    Fill(secondLeaf, DOOR);
    break;
  }
  case SW_SEALED_DOOR:
  {
    DrawFloor(box);
    SDL_Rect firstLeaf, secondLeaf;
    DoorLeafRects(box, true, AutoMapDoorOpen, &firstLeaf, &secondLeaf);
    Fill(firstLeaf, SEALED_DOOR);
    Fill(secondLeaf, SEALED_DOOR);
    break;
  }
  case SW_TELEPORTER: DrawTeleporter(box); break;
  case SW_FALSEWALL:
    Fill(box, FALSE_WALL);
    Outline(box, FALSE_WALL_EDGE);
    break;
  case SW_SPINNER: DrawSpinnerMark(box, AutoMapNorth, AutoMapNorth); break;
  case SW_DORMANT_SPINNER:
    DrawSpinnerMark(box, AutoMapNorth, AutoMapNorth, DORMANT_SPINNER);
    break;
  case SW_PARTY:
  {
    FloatPoint tip = {(float)(box.x + box.w), (float)(box.y + box.h / 2)};
    FloatPoint upper = {(float)box.x, (float)box.y};
    FloatPoint lower = {(float)box.x, (float)(box.y + box.h)};
    FillTriangle(tip, upper, lower, PARTY);
    break;
  }
  case SW_MONSTER: Fill(box, MONSTER_MARKER); break;
  case SW_ITEM: Fill(box, ITEM_MARKER); break;
  case SW_MORE: DrawMoreItemsMark(box); break;
  case SW_MISSILE: Fill(box, MISSILE_MARKER); break;
  case SW_CLOUD:
    Fill(box, CLOUD_HAZE);
    Outline(box, CLOUD_EDGE);
    break;
  // SW_PAD and SW_GENERATOR are drawn by the WYSIWYG cases below.
  case SW_LINK: Outline(Inset(box, 8), LINK_BADGE); break;
  case SW_COUNT:
    Fill(box, BACKGROUND);
    Outline(box, ITEM_MARKER);
    UseColour(ITEM_MARKER);
    DrawNumberCentred(box.x + box.w / 2, box.y + (box.h - 5 * DIGIT_SCALE) / 2, 2);
    break;
  case SW_INSIDE:
  {
    Fill(box, BACKGROUND);
    Outline(box, ITEM_MARKER);
    SDL_Rect inner = Inset(box, 22);
    Fill(inner, ITEM_MARKER);
    break;
  }
  case SW_MAYBE:
  {
    Fill(box, BACKGROUND);
    SDL_Rect inner = Inset(box, 22);
    Fill(inner, ITEM_MARKER);
    break;
  }
  // WYSIWYG swatches: the real map primitives on an enlarged box. Pad and
  // teleporter operators sit bottom-left on the map; here they fill the swatch
  // so the direction reads at legend size.
  case SW_DESTROYED_DOOR:
  {
    DrawFloor(box);
    SDL_Rect firstLeaf, secondLeaf;
    DoorLeafRects(box, true, AutoMapDoorOpen, &firstLeaf, &secondLeaf);
    Fill(firstLeaf, DESTROYED_DOOR);
    Fill(secondLeaf, DESTROYED_DOOR);
    break;
  }
  case SW_OPEN_FALSEWALL:
    DrawFloor(box);
    DrawDashedRect(box, FALSE_WALL_EDGE);
    break;
  case SW_FLOOR_PICTURE:
    DrawFloor(box);
    Fill(Inset(box, 20), FLOOR_FEATURE_MARKER);
    break;
  case SW_INVISIBLE_TELEPORTER:
    DrawFloor(box);
    DrawDashedRect(Inset(box, 10), INVISIBLE_TELEPORTER);
    break;
  case SW_DORMANT_TELEPORTER:
  {
    DrawFloor(box);
    DrawDashedRect(Inset(box, 10), DORMANT_TELEPORTER);
    int dot = std::max(2, box.w / 12);
    SDL_Rect centre = {box.x + (box.w - dot) / 2, box.y + (box.h - dot) / 2, dot, dot};
    Fill(centre, DORMANT_TELEPORTER);
    break;
  }
  case SW_WALL_FEATURE:
  case SW_WALL_NOSPRITE:
  case SW_WALL_WRITING:
  {
    // Drawn like the map draws them: below All on the north face beside its strip, at All
    // across the whole cell
    bool atAll = shownDrawList.spoilers == AutoMapSpoilersAll;
    SDL_Rect area = atAll ? box : FeatureSquare(box, AutoMapNorth);
    Fill(box, WALL);
    Fill(Inset(area, 6), WALL_FEATURE_BACKDROP);
    if (swatch == SW_WALL_FEATURE) Fill(Inset(area, 30), WALL_SIDE_LINE);
    if (swatch == SW_WALL_NOSPRITE) Fill(Inset(area, 30), WALL_FEATURE_MARKER);
    if (swatch == SW_WALL_WRITING) DrawWriting(area);
    if (!atAll)
    {
      Fill(StripAlong(box, AutoMapNorth), swatch == SW_WALL_WRITING ? FLAVOR_TICK : WALL_SIDE_LINE);
    }
    break;
  }
  case SW_WALL_FLAVOUR:
    Fill(box, WALL);
    Fill(StripAlong(box, AutoMapNorth), FLAVOR_TICK);
    break;
  case SW_WALL_HIDDEN:
    // Full-cell dash and diamond: on the map they keep to the side's half.
    Fill(box, WALL);
    DrawDashedRect(Inset(box, 18), WALL_SIDE_LINE);
    DrawHollowDiamond(box, HIDDEN_SWITCH);
    break;
  case SW_WALL_LOCK:
    Fill(box, WALL);
    DrawSideRing(box.x + box.w / 2, box.y + 4, 5, LINK_BADGE);
    break;
  case SW_WALL_STOCKED:
    Fill(box, WALL);
    DrawSideDot(box.x + box.w / 2, box.y + 4, 5, ITEM_MARKER);
    break;
  case SW_BREAK_AXE:
  {
    DrawFloor(box);
    int side = std::min(box.w, box.h);
    SDL_Rect mark = {box.x + (box.w - side) / 2, box.y + (box.h - side) / 2, side, side};
    DrawBreakAxeMark(mark);
    break;
  }
  case SW_BREAK_FIRE:
    DrawFloor(box);
    DrawHollowCircle(box.x + box.w / 2.0f, box.y + box.h / 2.0f,
                     std::min(box.w, box.h) / 2.0f - 1.0f);
    break;
  case SW_BREAK_BOTH:
  {
    DrawFloor(box);
    int side = std::min(box.w / 2, box.h);
    SDL_Rect axeBox = {box.x + 1, box.y + (box.h - side) / 2, side, side};
    DrawBreakAxeMark(axeBox);
    DrawHollowCircle(box.x + box.w * 3.0f / 4.0f, box.y + box.h / 2.0f,
                     std::min(box.w / 2, box.h) / 2.0f - 1.0f);
    break;
  }
  case SW_PAD: DrawPadMark(box, AutoMapFloorActuator(0, 0, AutoMapActuatorPad)); break;
  case SW_PAD_DORMANT:
    DrawPadMark(box, AutoMapFloorActuator(0, 0, AutoMapActuatorDormantPad));
    break;
  case SW_PAD_ANYTHING: DrawPadMark(box, AutoMapFloorActuator(0, 0, AutoMapActuatorPad)); break;
  case SW_PAD_PARTY: DrawOperatorTriangle(Inset(box, 15), AutoMapNorth); break;
  case SW_PAD_FACE_N: DrawOperatorTriangle(Inset(box, 15), AutoMapNorth); break;
  case SW_PAD_FACE_E: DrawOperatorTriangle(Inset(box, 15), AutoMapEast); break;
  case SW_PAD_FACE_S: DrawOperatorTriangle(Inset(box, 15), AutoMapSouth); break;
  case SW_PAD_FACE_W: DrawOperatorTriangle(Inset(box, 15), AutoMapWest); break;
  case SW_PAD_PARTY_OR_MONSTER:
  {
    SDL_Rect triBox = {box.x + 2, box.y + 2, box.w * 55 / 100, box.h - 4};
    DrawOperatorTriangle(triBox, AutoMapNorth);
    DrawOperatorDot(box.x + box.w - 5, box.y + box.h / 2, 4);
    break;
  }
  case SW_PAD_MONSTER:
    DrawOperatorDot(box.x + box.w * 30 / 100, box.y + box.h / 2, 4);
    DrawOperatorDot(box.x + box.w * 70 / 100, box.y + box.h / 2, 4);
    break;
  case SW_PAD_OBJECT: DrawKeyholeMark(Inset(box, 15)); break;
  case SW_PAD_CARRIED:
  {
    SDL_Rect upper = {box.x + 4, box.y + 1, box.w - 8, box.h - 8};
    DrawKeyholeMark(upper);
    DrawOperatorDot(box.x + box.w / 2, box.y + box.h - 3, 3);
    break;
  }
  case SW_GENERATOR: DrawGeneratorMark(box, false); break;
  case SW_GENERATOR_SPENT: DrawGeneratorMark(box, true); break;
  case SW_COG: DrawCogMark(box); break;
  case SW_SCOPE_ANYTHING: DrawTeleporter(box); break;
  case SW_SCOPE_PARTY:
    DrawTeleporter(box);
    DrawOperatorTriangle(Inset(box, 20), AutoMapNorth);
    break;
  case SW_SCOPE_MONSTER:
    DrawTeleporter(box);
    DrawOperatorDot(box.x + box.w * 30 / 100, box.y + box.h / 2, 4);
    DrawOperatorDot(box.x + box.w * 70 / 100, box.y + box.h / 2, 4);
    break;
  case SW_SCOPE_OBJECT:
    DrawTeleporter(box);
    DrawOperatorHollowSquare(box);
    break;
  case SW_SPIN_ABSOLUTE: DrawSpinnerMark(box, AutoMapNorth, AutoMapNorth); break;
  case SW_SPIN_RIGHT: DrawSpinnerMark(box, -1, 1); break;
  case SW_SPIN_LEFT: DrawSpinnerMark(box, -1, 3); break;
  case SW_SPIN_AROUND: DrawSpinnerMark(box, -1, 2); break;
  case SW_SPIN_STRAIGHT: DrawSpinnerMark(box, -1, 0); break;
  case SW_LEVEL_PLATE: DrawLevelPlate(box, 2); break;
  case SW_LINK_SET: DrawLegendLinkShaft(box, false, 0, false); break;
  case SW_LINK_ONCE: DrawLegendLinkShaft(box, true, 0, false); break;
  case SW_LINK_CLEAR: DrawLegendLinkShaft(box, false, 1, false); break;
  case SW_LINK_TOGGLE: DrawLegendLinkShaft(box, false, 2, false); break;
  case SW_LINK_PRESSURE: DrawLegendLinkShaft(box, false, 0, true); break;
  case SW_LINK_SELF: Outline(Inset(box, 25), LINK_BADGE); break;
  case SW_LINK_GHOST:
  {
    DrawFloor(box);
    SDL_Rect firstLeaf, secondLeaf;
    DoorLeafRects(box, true, AutoMapDoorOpen, &firstLeaf, &secondLeaf);
    Fill(firstLeaf, LINK_GHOST);
    Fill(secondLeaf, LINK_GHOST);
    Outline(firstLeaf, LINK_BADGE);
    Outline(secondLeaf, LINK_BADGE);
    break;
  }
  case SW_HOVER_TEXT:
    // The hover footer's text lines, same 3-line glyph as wall writing.
    Fill(box, BACKGROUND);
    DrawWriting(box);
    break;
  }
}

static void DrawLegendButton(void)
{
  SDL_Rect button = LegendButtonRect();
  Fill(button, RULER_BACKGROUND);
  UseColour(legendPage == 0 ? WHITE : LINK_BADGE);
  SDL_RenderDrawRect(mapRenderer, &button);
  const char *mark = "?";
  int w = AutoMapScene_TextWidth(mark) * DIGIT_SCALE;
  DrawLegendText(button.x + (button.w - w) / 2, button.y + (button.h - 5 * DIGIT_SCALE) / 2,
                 mark, legendPage == 0 ? WHITE : LINK_BADGE);
}

static const int SEARCH_QUERY_H = 20;
static const int SEARCH_ROW_H = 16;
static const int SEARCH_FOOTER_H = 16;

// How many result rows fit the window: the panel stays clear of the rulers
// and always reserves its footer line, so every hit is reachable by scrolling
// instead of hiding past the eighth.
static int SearchVisibleRows(void)
{
  if (mapRenderer == NULL) return 8;
  int windowWidth = 0, windowHeight = 0;
  SDL_GetRendererOutputSize(mapRenderer, &windowWidth, &windowHeight);
  (void)windowWidth;
  int room = windowHeight - 60 - SEARCH_QUERY_H - 8 - SEARCH_FOOTER_H;
  int rows = room / SEARCH_ROW_H;
  if (rows < 4) rows = 4;
  if (rows > 20) rows = 20;
  return rows;
}

static SDL_Rect SearchPanelRect(void)
{
  SDL_Rect empty = {0, 0, 0, 0};
  if (!searchOpen || mapRenderer == NULL) return empty;
  int windowWidth = 0, windowHeight = 0;
  SDL_GetRendererOutputSize(mapRenderer, &windowWidth, &windowHeight);
  (void)windowHeight;
  int x = PanelLeft();
  int w = std::min(460, windowWidth - x - 5);
  if (w < 200) return empty;
  int total = (int)searchHits.size();
  int rows = std::min(SearchVisibleRows(), total);
  bool needFooter = total > SearchVisibleRows() || (!searchQuery.empty() && total == 0);
  if (!searchQuery.empty() && total == 0) rows = 1;
  int h = SEARCH_QUERY_H + rows * SEARCH_ROW_H + 8 + (needFooter ? SEARCH_FOOTER_H : 0);
  SDL_Rect plate = {x, 3, w, h};
  return plate;
}

static bool SearchHitAtPixel(int pixelX, int pixelY, int *hitIndex)
{
  if (!searchOpen) return false;
  SDL_Rect plate = SearchPanelRect();
  if (plate.w <= 0 || !PointInRect(pixelX, pixelY, plate)) return false;
  int row = (pixelY - plate.y - SEARCH_QUERY_H - 4) / SEARCH_ROW_H;
  int total = (int)searchHits.size();
  int rows = std::min(SearchVisibleRows(), total);
  if (row < 0 || row >= rows) return false;
  int index = searchTop + row;
  if (index < 0 || index >= total) return false;
  if (hitIndex != NULL) *hitIndex = index;
  return true;
}

static void DrawSearchButton(void)
{
  SDL_Rect button = SearchButtonRect();
  Fill(button, RULER_BACKGROUND);
  UseColour(searchOpen ? LINK_BADGE : WHITE);
  SDL_RenderDrawRect(mapRenderer, &button);
  const char *mark = "/";
  int w = AutoMapScene_TextWidth(mark) * DIGIT_SCALE;
  DrawLegendText(button.x + (button.w - w) / 2, button.y + (button.h - 5 * DIGIT_SCALE) / 2,
                 mark, searchOpen ? LINK_BADGE : WHITE);
}

static void DrawSearchPanel(void)
{
  if (!searchOpen) return;
  SDL_Rect plate = SearchPanelRect();
  if (plate.w <= 0) return;
  Fill(plate, BACKGROUND);
  Outline(plate, WHITE);
  char query[256];
  std::snprintf(query, sizeof(query), "FIND %s_ (%d)", searchQuery.c_str(),
                (int)searchHits.size());
  DrawLegendText(plate.x + 6, plate.y + 4, query, WHITE);
  int total = (int)searchHits.size();
  int visible = SearchVisibleRows();
  int rows = std::min(visible, total);
  int y = plate.y + SEARCH_QUERY_H + 4;
  for (int row = 0; row < rows; row++)
  {
    int i = searchTop + row;
    if (i < 0 || i >= total) break;
    const AutoMapSearchHit &hit = searchHits[(size_t)i];
    // Global ruler coordinates of the hit's own level, not the shown one
    char coords[64];
    std::snprintf(coords, sizeof(coords), "L%d (%d,%d)", hit.level, hit.globalX, hit.globalY);
    std::string line = std::string(coords) + " " + hit.name;
    if (hit.wallFeature != AutoMapWallFeatureNone)
    {
      char side[5] = {' ', '(', SideInitial(hit.slot.side), ')', '\0'};
      if (side[2] != '\0') line += side;
    }
    else if (hit.slot.required) line += " NEEDS";
    else if (hit.monsterCarried) line += " CARRIED";
    else if (hit.slot.isChest) line += " CHEST";
    else if (hit.slot.side >= 0) line += " ALCOVE";
    if (i == searchSelected)
    {
      SDL_Rect rowRect = {plate.x + 1, y - 2, plate.w - 2, SEARCH_ROW_H};
      Fill(rowRect, RULER_BACKGROUND);
    }
    DrawLegendText(plate.x + 6, y, line.c_str(),
                   i == searchSelected ? LINK_BADGE : RULER_TEXT);
    y += SEARCH_ROW_H;
  }
  if (!searchQuery.empty() && total == 0)
  {
    DrawLegendText(plate.x + 6, y, "NOTHING SEEN MATCHES", RULER_TEXT);
  }
  else if (total > visible)
  {
    char range[64];
    int last = searchTop + rows;
    if (last > total) last = total;
    std::snprintf(range, sizeof(range), "SHOWING %d-%d OF %d, WHEEL SCROLLS", searchTop + 1,
                  last, total);
    DrawLegendText(plate.x + 6, y, range, RULER_TEXT);
  }
}

static void DrawSearchMarkers(const AutoMapView &view)
{
  for (size_t i = 0; i < searchHits.size(); i++)
  {
    // Only the shown level is on screen; other levels wait for their switch
    if (searchHits[i].level != shownDrawList.level) continue;
    SDL_Rect cell = CellArea(view, searchHits[i].x, searchHits[i].y);
    if ((int)i == searchSelected)
    {
      Outline(cell, WHITE);
      Outline(Inset(cell, 8), ITEM_MARKER);
    }
    else
    {
      Outline(Inset(cell, 12), ITEM_MARKER);
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Debug menu

static const int DEBUG_MENU_ROW_H = 16;
static const int DEBUG_MENU_PAD = 6;

static void DebugMenuClose(void)
{
  if (!debugMenuOpen) return;
  debugMenuOpen = false;
  debugMenuRows.clear();
  debugMenuHovered = -1;
  isDirty = true;
}

static void DebugMenuAddDebugRows(int x, int y);
static void LinkModeStart(int x, int y, int side);

// A wall cell takes its links on a face; anything else is a floor tile the player may mark.
// The kind is what the map shows, so a guess is never checked against what the cell really is.
static bool IsWallKind(AutoMapCellKind kind)
{
  return kind == AutoMapCellWall || kind == AutoMapCellFalseWall;
}

// The player's rows for a shown cell: the pit marks on a tile drawn as plain floor, REMOVE MARK
// on any marked tile, then LINK FROM HERE, and REMOVE LINKS when a shown player link starts or
// ends on the cell. A marked tile the map has since learned to be a pit
// (the party fell through it) is drawn as that pit, so it offers only REMOVE MARK: the guess
// can go now that the map knows. side is the face nearest the pointer, used for a wall.
static void DebugMenuAddNoteRows(const AutoMapShownCell &shown, int side)
{
  if (shown.cell.kind == AutoMapCellFloor)
  {
    DebugMenuRow open = {MenuRowPitMark, 1, "HIDDEN PIT (OPEN)", true};
    DebugMenuRow closed = {MenuRowPitMark, 0, "HIDDEN PIT (CLOSED)", true};
    debugMenuRows.push_back(open);
    debugMenuRows.push_back(closed);
  }
  if (shown.playerPit >= 0)
  {
    DebugMenuRow remove = {MenuRowPitMark, -1, "REMOVE MARK", true};
    debugMenuRows.push_back(remove);
  }
  DebugMenuRow link = {MenuRowLinkFromHere, IsWallKind(shown.cell.kind) ? side : -1,
                       "LINK FROM HERE", true};
  debugMenuRows.push_back(link);
  for (size_t i = 0; i < shownDrawList.playerLinks.size(); i++)
  {
    const AutoMapLink &each = shownDrawList.playerLinks[i];
    if ((each.srcX == shown.x && each.srcY == shown.y) ||
        (each.dstX == shown.x && each.dstY == shown.y))
    {
      DebugMenuRow remove = {MenuRowRemoveLinks, 0, "REMOVE LINKS", true};
      debugMenuRows.push_back(remove);
      break;
    }
  }
}

// The face of the cell under a pixel nearest to it: the pointer's place inside the cell
static int NearestSideAtPixel(int pixelX, int pixelY)
{
  AutoMapView view = ViewOfShownLevel();
  if (view.cellSize <= 0) return AutoMapNorth;
  int localX = pixelX - view.originX;
  int localY = pixelY - view.originY;
  if (localX < 0 || localY < 0) return AutoMapNorth;
  return AutoMapScene_NearestSide(localX % view.cellSize, localY % view.cellSize, view.cellSize);
}

static void DebugMenuOpenAt(int pixelX, int pixelY)
{
  DebugMenuClose();
  int x = -1, y = -1;
  if (shownDrawList.level == AUTOMAP_NO_LEVEL || !CellAtPixel(pixelX, pixelY, &x, &y)) return;
  // The player cannot note what the map does not show; the debug rows still work anywhere
  const AutoMapShownCell *shown = ShownCellAt(shownDrawList, x, y);
  if (shown == NULL && !debugMenuOn) return;
  debugMenuLevel = shownDrawList.level;
  debugMenuX = x;
  debugMenuY = y;
  debugMenuPixelX = pixelX;
  debugMenuPixelY = pixelY;
  char title[96];
  std::snprintf(title, sizeof(title), "L%d (%d,%d)", debugMenuLevel,
                x + shownDrawList.offsetX, y + shownDrawList.offsetY);
  debugMenuTitle = title;
  // The testing rows come first: they exist only for whoever set map_debug, who reaches for
  // them all the time, and without map_debug the menu holds the player's rows alone
  if (debugMenuOn) DebugMenuAddDebugRows(x, y);
  // The rim rock outside the level is shown but takes no note: the .map file keeps none there
  if (shown != NULL && AutoMapScene_CellTakesNotes(shownDrawList, x, y))
  {
    if (!debugMenuRows.empty())
    {
      DebugMenuRow separator = {MenuRowSeparator, 0, "", false};
      debugMenuRows.push_back(separator);
    }
    DebugMenuAddNoteRows(*shown, NearestSideAtPixel(pixelX, pixelY));
  }
  if (debugMenuRows.empty()) return;
  debugMenuOpen = true;
  isDirty = true;
}

// The testing rows of map_debug; the title names what the cell takes
static void DebugMenuAddDebugRows(int x, int y)
{
  AutoMapPlacement placement = AutoMap_PlacementAt(debugMenuLevel, x, y);
  bool isFree = placement == AutoMapPlacementFree;
  debugMenuTitle += std::string(" ") + AutoMapScene_DescribePlacement(placement);
  DebugMenuRow teleport = {DebugRowTeleport, 0, "TELEPORT HERE", isFree};
  debugMenuRows.push_back(teleport);
  std::vector<AutoMapMonsterChoice> choices = AutoMapLevelReader_MonsterTypesOfLevel(debugMenuLevel);
  for (size_t i = 0; i < choices.size(); i++)
  {
    DebugMenuRow spawn = {DebugRowMonster, (int)i, "SPAWN " + choices[i].name, isFree};
    debugMenuRows.push_back(spawn);
  }
}

static const char *const FACING_NAMES[4] = {"NORTH", "EAST", "SOUTH", "WEST"};

static void DebugMenuAskForFacing(const AutoMapSpawnSpot &spot)
{
  debugMenuSpot = spot;
  char count[16];
  std::snprintf(count, sizeof(count), "%d ", debugMenuCount);
  debugMenuTitle = (debugMenuCount > 1 ? std::string(count) : std::string()) + debugMenuMonster.name
                   + (debugMenuCount > 1 ? std::string() : " " + spot.name);
  debugMenuRows.clear();
  for (size_t i = 0; i < spot.facings.size(); i++)
  {
    DebugMenuRow facing = {DebugRowFacing, spot.facings[i],
                           std::string("FACING ") + FACING_NAMES[spot.facings[i] & 3], true};
    debugMenuRows.push_back(facing);
  }
  DebugMenuRow any = {DebugRowFacing, -1, "FACING ANY", true};
  debugMenuRows.push_back(any);
  debugMenuHovered = -1;
  debugMenuOpen = true;
  isDirty = true;
}

static void DebugMenuAskForSpot(const AutoMapMonsterChoice &monster)
{
  std::vector<AutoMapSpawnSpot> spots = AutoMapScene_SpawnSpots(monster.horizontalSize);
  // A group stands where CreateMonster puts it: only the centre's facings are asked for
  if (spots.size() == 1 || debugMenuCount > 1)
  {
    DebugMenuAskForFacing(spots[0]);
    return;
  }
  debugMenuTitle = monster.name;
  debugMenuRows.clear();
  for (size_t i = 0; i < spots.size(); i++)
  {
    DebugMenuRow spot = {DebugRowSpot, (int)i, spots[i].name, true};
    debugMenuRows.push_back(spot);
  }
  debugMenuHovered = -1;
  debugMenuOpen = true;
  isDirty = true;
}

// Beside the clicked pixel, pushed back inside the window
static SDL_Rect DebugMenuRect(void)
{
  int widest = AutoMapScene_TextWidth(debugMenuTitle.c_str());
  for (size_t i = 0; i < debugMenuRows.size(); i++)
  {
    widest = std::max(widest, AutoMapScene_TextWidth(debugMenuRows[i].label.c_str()));
  }
  SDL_Rect plate = {debugMenuPixelX + 4, debugMenuPixelY + 4,
                    widest * DIGIT_SCALE + 2 * DEBUG_MENU_PAD,
                    (int)(debugMenuRows.size() + 1) * DEBUG_MENU_ROW_H + DEBUG_MENU_PAD};
  int windowWidth = 0, windowHeight = 0;
  if (mapRenderer != NULL) SDL_GetRendererOutputSize(mapRenderer, &windowWidth, &windowHeight);
  if (plate.x + plate.w > windowWidth) plate.x = std::max(0, windowWidth - plate.w);
  if (plate.y + plate.h > windowHeight) plate.y = std::max(0, windowHeight - plate.h);
  return plate;
}

// The row under a pixel, -1 on the title or off the menu
static int DebugMenuRowAtPixel(int pixelX, int pixelY)
{
  SDL_Rect plate = DebugMenuRect();
  if (!PointInRect(pixelX, pixelY, plate)) return -1;
  int row = (pixelY - plate.y - DEBUG_MENU_PAD / 2) / DEBUG_MENU_ROW_H - 1;
  if (row < 0 || row >= (int)debugMenuRows.size()) return -1;
  return row;
}

static void DebugMenuAskForCount(const AutoMapMonsterChoice &monster)
{
  debugMenuMonster = monster;
  debugMenuCount = 1;
  int largest = AutoMapScene_LargestGroup(monster.horizontalSize);
  if (largest == 1)
  {
    DebugMenuAskForSpot(monster);
    return;
  }
  debugMenuTitle = monster.name;
  debugMenuRows.clear();
  for (int count = 1; count <= largest; count++)
  {
    char label[24];
    std::snprintf(label, sizeof(label), count == largest ? "%d FILL THE CELL" : "%d", count);
    DebugMenuRow row = {DebugRowCount, count, label, true};
    debugMenuRows.push_back(row);
  }
  debugMenuHovered = -1;
  debugMenuOpen = true;
  isDirty = true;
}

static void DebugMenuCarryOut(const DebugMenuRow &row)
{
  if (row.kind == MenuRowPitMark)
  {
    AutoMap_SetPlayerPit(debugMenuLevel, debugMenuX - shownDrawList.boundaryMarginX,
                         debugMenuY - shownDrawList.boundaryMarginY, row.choice);
    return;
  }
  if (row.kind == MenuRowLinkFromHere)
  {
    LinkModeStart(debugMenuX, debugMenuY, row.choice);
    return;
  }
  if (row.kind == MenuRowRemoveLinks)
  {
    AutoMap_RemovePlayerLinksOf(debugMenuLevel, debugMenuX - shownDrawList.boundaryMarginX,
                                debugMenuY - shownDrawList.boundaryMarginY);
    return;
  }
  if (row.kind == MenuRowSeparator) return;
  if (row.kind == DebugRowMonster)
  {
    std::vector<AutoMapMonsterChoice> choices = AutoMapLevelReader_MonsterTypesOfLevel(debugMenuLevel);
    if (row.choice < (int)choices.size()) DebugMenuAskForCount(choices[(size_t)row.choice]);
    return;
  }
  if (row.kind == DebugRowCount)
  {
    debugMenuCount = row.choice;
    DebugMenuAskForSpot(debugMenuMonster);
    return;
  }
  if (row.kind == DebugRowSpot)
  {
    std::vector<AutoMapSpawnSpot> spots = AutoMapScene_SpawnSpots(debugMenuMonster.horizontalSize);
    if (row.choice < (int)spots.size()) DebugMenuAskForFacing(spots[(size_t)row.choice]);
    return;
  }
  if (row.kind == DebugRowFacing)
  {
    int facing = row.choice;
    // Any facing of four is the generator's random one; a half only takes two
    if (facing < 0 && debugMenuSpot.facings.size() < 4)
    {
      facing = debugMenuSpot.facings[(size_t)(std::rand() % (int)debugMenuSpot.facings.size())];
    }
    AutoMap_RequestSpawn(debugMenuLevel, debugMenuX, debugMenuY, debugMenuMonster.type,
                         debugMenuCount, debugMenuSpot.positions, facing);
    return;
  }
  AutoMap_RequestTeleport(debugMenuLevel, debugMenuX, debugMenuY);
  // The map goes along with the party, wherever it lands
  AutoMap_FollowParty();
}

static bool DebugMenuHandlePointerEvent(const SDL_Event &event)
{
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT)
  {
    DebugMenuOpenAt(event.button.x, event.button.y);
    return true;
  }
  if (!debugMenuOpen) return false;
  if (event.type == SDL_MOUSEMOTION)
  {
    int hovered = DebugMenuRowAtPixel(event.motion.x, event.motion.y);
    if (hovered != debugMenuHovered)
    {
      debugMenuHovered = hovered;
      isDirty = true;
    }
    return PointInRect(event.motion.x, event.motion.y, DebugMenuRect());
  }
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
  {
    int row = DebugMenuRowAtPixel(event.button.x, event.button.y);
    bool onMenu = PointInRect(event.button.x, event.button.y, DebugMenuRect());
    if (row >= 0 && debugMenuRows[(size_t)row].isEnabled)
    {
      DebugMenuRow chosen = debugMenuRows[(size_t)row];
      DebugMenuClose();
      DebugMenuCarryOut(chosen);
      return true;
    }
    if (onMenu) return true;
    DebugMenuClose();
    return true;
  }
  // Anything else that moves the view leaves the menu pointing at the wrong cell
  if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEWHEEL) DebugMenuClose();
  return false;
}

static void DrawDebugMenu(void)
{
  if (!debugMenuOpen) return;
  SDL_Rect plate = DebugMenuRect();
  Fill(plate, BACKGROUND);
  Outline(plate, WHITE);
  int y = plate.y + DEBUG_MENU_PAD;
  DrawLegendText(plate.x + DEBUG_MENU_PAD, y, debugMenuTitle.c_str(), WHITE);
  y += DEBUG_MENU_ROW_H;
  for (size_t i = 0; i < debugMenuRows.size(); i++)
  {
    const DebugMenuRow &row = debugMenuRows[i];
    if (row.kind == MenuRowSeparator)
    {
      int lineY = y - 4 + DEBUG_MENU_ROW_H / 2;
      UseColour(RULER_TICK);
      SDL_RenderDrawLine(mapRenderer, plate.x + DEBUG_MENU_PAD, lineY,
                         plate.x + plate.w - DEBUG_MENU_PAD, lineY);
      y += DEBUG_MENU_ROW_H;
      continue;
    }
    bool isHovered = (int)i == debugMenuHovered && row.isEnabled;
    if (isHovered)
    {
      SDL_Rect rowRect = {plate.x + 1, y - 4, plate.w - 2, DEBUG_MENU_ROW_H};
      Fill(rowRect, RULER_BACKGROUND);
    }
    const SDL_Color &colour = !row.isEnabled ? RULER_TICK : isHovered ? LINK_BADGE : RULER_TEXT;
    DrawLegendText(plate.x + DEBUG_MENU_PAD, y, row.label.c_str(), colour);
    y += DEBUG_MENU_ROW_H;
  }
}

// ---------------------------------------------------------------------------------------------
// Link mode

static const char *const LINK_MODE_HINT = "LINK MODE: CLICK TO ADD OR REMOVE, ESC TO END";

// x and y are a draw-list cell; the source is kept as a level cell, like the notes
static void LinkModeStart(int x, int y, int side)
{
  linkModeOn = true;
  linkModeLevel = shownDrawList.level;
  linkModeX = x - shownDrawList.boundaryMarginX;
  linkModeY = y - shownDrawList.boundaryMarginY;
  linkModeSide = side;
  isDirty = true;
}

static void LinkModeEnd(void)
{
  if (!linkModeOn) return;
  linkModeOn = false;
  isDirty = true;
}

// A left-click in link mode: the shown cell under it becomes a target of the source, or stops
// being one. A cell the map does not show cannot be picked, as for the right-click menu, nor can
// the rim rock outside the level or the source cell itself.
static void LinkModeClickAt(int pixelX, int pixelY)
{
  int x = -1, y = -1;
  if (!CellAtPixel(pixelX, pixelY, &x, &y)) return;
  if (!AutoMapScene_CellTakesNotes(shownDrawList, x, y)) return;
  int targetX = x - shownDrawList.boundaryMarginX;
  int targetY = y - shownDrawList.boundaryMarginY;
  if (targetX == linkModeX && targetY == linkModeY) return;
  AutoMap_TogglePlayerLink(linkModeLevel, linkModeX, linkModeY, linkModeSide, shownDrawList.level,
                           targetX, targetY);
}

// The source and its targets now, in the player colour. The targets come from the draw list, so
// one under the fog is not drawn, as it cannot be picked either.
static void DrawLinkModeHighlights(const AutoMapView &view)
{
  if (!linkModeOn) return;
  int srcX = linkModeX + shownDrawList.boundaryMarginX;
  int srcY = linkModeY + shownDrawList.boundaryMarginY;
  HighlightLinkedCell(view, srcX, srcY, PLAYER_NOTE);
  for (size_t i = 0; i < shownDrawList.playerLinks.size(); i++)
  {
    const AutoMapLink &link = shownDrawList.playerLinks[i];
    int side = link.srcIsWall ? link.srcSide : -1;
    if (link.srcX != srcX || link.srcY != srcY || side != linkModeSide) continue;
    HighlightLinkedCell(view, link.dstX, link.dstY, PLAYER_NOTE);
  }
}

static void DrawLinkModeHint(void)
{
  if (!linkModeOn) return;
  int windowWidth = 0, windowHeight = 0;
  SDL_GetRendererOutputSize(mapRenderer, &windowWidth, &windowHeight);
  int width = AutoMapScene_TextWidth(LINK_MODE_HINT) * DIGIT_SCALE;
  SDL_Rect plate = {std::max(RULER, (windowWidth - width) / 2 - 4), RULER + 4, width + 8,
                    5 * DIGIT_SCALE + 8};
  Fill(plate, BACKGROUND);
  Outline(plate, PLAYER_NOTE);
  DrawLegendText(plate.x + 4, plate.y + 4, LINK_MODE_HINT, PLAYER_NOTE);
}

// ---------------------------------------------------------------------------------------------
// Level panel

static const char *const LEVEL_PANEL_SKILLS[4] = {"FIG", "NIN", "PRI", "WIZ"};
static const int LEVEL_PANEL_GAP = 12;

static void LevelPanelClose(void)
{
  if (!levelPanelOpen) return;
  levelPanelOpen = false;
  levelPanelHoveredRow = -1;
  levelPanelHoveredColumn = -1;
  isDirty = true;
}

static void LevelPanelOpen(void)
{
  legendPage = 0;
  if (searchOpen) SearchCloseBox();
  levelPanelOpen = true;
  isDirty = true;
}

// The panel's rows: one per champion, then the light, monsters, forget and measure rows. Hover and
// clicks name a row and a column; champion rows have the four skills as columns.
static const char *const LIGHT_ROW_NAME = "LIGHT";
static const char *const FORGET_ROW_NAME = "FORGET";
static const int LIGHT_PALETTE_COUNT = 6;
// LEVEL, DUNGEON and LEARNED forget what the map saw and learned; only MARKS forgets the player's
// own notes, which are not the map's
static const char *const FORGET_CELLS[4] = {"LEVEL", "DUNGEON", "LEARNED", "MARKS"};
static const char *const MONSTERS_ROW_NAME = "MONSTERS";
static const char *const MONSTERS_CELLS[2] = {"MOVE", "FROZEN"};
static const char *const MEASURE_ROW_NAME = "MEASURE";
static const char *const MEASURE_CELLS[1] = {"WALLS"};

static int LightRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return (int)champions.size();
}

static int MonstersRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return (int)champions.size() + 1;
}

static int ForgetRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return (int)champions.size() + 2;
}

static int MeasureRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return (int)champions.size() + 3;
}

static int PanelRowCount(const std::vector<AutoMapChampionLevels> &champions)
{
  return (int)champions.size() + 4;
}

// The light row's first cell is the party's own light, then the six palettes
static std::vector<std::string> LightCells(void)
{
  std::vector<std::string> cells;
  char real[16];
  std::snprintf(real, sizeof(real), "REAL %d", AutoMap_RealPalette());
  cells.push_back(real);
  for (int palette = 0; palette < LIGHT_PALETTE_COUNT; palette++)
  {
    char label[4];
    std::snprintf(label, sizeof(label), "%d", palette);
    cells.push_back(label);
  }
  return cells;
}

static std::vector<std::string> ForgetCells(void)
{
  return std::vector<std::string>(FORGET_CELLS, FORGET_CELLS + 4);
}

static std::vector<std::string> MonstersCells(void)
{
  return std::vector<std::string>(MONSTERS_CELLS, MONSTERS_CELLS + 2);
}

static std::vector<std::string> MeasureCells(void)
{
  return std::vector<std::string>(MEASURE_CELLS, MEASURE_CELLS + 1);
}

static int LevelPanelNameWidth(const std::vector<AutoMapChampionLevels> &champions)
{
  int widest = std::max(AutoMapScene_TextWidth(LIGHT_ROW_NAME),
                        std::max(AutoMapScene_TextWidth(FORGET_ROW_NAME),
                                 std::max(AutoMapScene_TextWidth(MONSTERS_ROW_NAME),
                                          AutoMapScene_TextWidth(MEASURE_ROW_NAME))));
  for (size_t i = 0; i < champions.size(); i++)
  {
    widest = std::max(widest, AutoMapScene_TextWidth(champions[i].name.c_str()));
  }
  return widest * DIGIT_SCALE;
}

static int LevelPanelCellWidth(void)
{
  return AutoMapScene_TextWidth("WIZ 21") * DIGIT_SCALE + LEVEL_PANEL_GAP;
}

static int ToolCellWidth(const std::string &label)
{
  return AutoMapScene_TextWidth(label.c_str()) * DIGIT_SCALE + LEVEL_PANEL_GAP;
}

static int ToolRowWidth(const std::vector<std::string> &cells)
{
  int w = 0;
  for (size_t i = 0; i < cells.size(); i++) w += ToolCellWidth(cells[i]);
  return w;
}

static const char *LevelPanelTitle(const std::vector<AutoMapChampionLevels> &champions)
{
  return champions.empty() ? "NO CHAMPION TO GAIN A LEVEL" : "GAIN A LEVEL";
}

static SDL_Rect LevelPanelRect(const std::vector<AutoMapChampionLevels> &champions)
{
  int cells = std::max(4 * LevelPanelCellWidth(),
                       std::max(ToolRowWidth(LightCells()), ToolRowWidth(ForgetCells())));
  int w = LevelPanelNameWidth(champions) + LEVEL_PANEL_GAP + cells;
  w = std::max(w, AutoMapScene_TextWidth(LevelPanelTitle(champions)) * DIGIT_SCALE);
  SDL_Rect plate = {PanelLeft(), 3, w + 2 * DEBUG_MENU_PAD,
                    (PanelRowCount(champions) + 1) * DEBUG_MENU_ROW_H + DEBUG_MENU_PAD};
  return plate;
}

static int LevelPanelCellsX(const std::vector<AutoMapChampionLevels> &champions)
{
  return LevelPanelRect(champions).x + DEBUG_MENU_PAD + LevelPanelNameWidth(champions)
       + LEVEL_PANEL_GAP / 2;
}

// The column of a tool row under pixelX, -1 when none
static int ToolColumnAt(const std::vector<std::string> &cells, int cellsX, int pixelX)
{
  int x = cellsX;
  for (size_t i = 0; i < cells.size(); i++)
  {
    int w = ToolCellWidth(cells[i]);
    if (pixelX >= x && pixelX < x + w) return (int)i;
    x += w;
  }
  return -1;
}

// The row and column of the cell under a pixel; false on the title, a name or off the panel
static bool LevelPanelCellAtPixel(const std::vector<AutoMapChampionLevels> &champions, int pixelX,
                                  int pixelY, int *row, int *column)
{
  SDL_Rect plate = LevelPanelRect(champions);
  if (!PointInRect(pixelX, pixelY, plate)) return false;
  int panelRow = (pixelY - plate.y - DEBUG_MENU_PAD / 2) / DEBUG_MENU_ROW_H - 1;
  if (panelRow < 0 || panelRow >= PanelRowCount(champions)) return false;
  int cellsX = LevelPanelCellsX(champions);
  if (pixelX < cellsX) return false;
  int cell = -1;
  if (panelRow == LightRow(champions)) cell = ToolColumnAt(LightCells(), cellsX, pixelX);
  else if (panelRow == MonstersRow(champions))
  {
    cell = ToolColumnAt(MonstersCells(), cellsX, pixelX);
  }
  else if (panelRow == ForgetRow(champions)) cell = ToolColumnAt(ForgetCells(), cellsX, pixelX);
  else if (panelRow == MeasureRow(champions)) cell = ToolColumnAt(MeasureCells(), cellsX, pixelX);
  else
  {
    cell = (pixelX - cellsX) / LevelPanelCellWidth();
    if (cell > 3) cell = -1;
  }
  if (cell < 0) return false;
  *row = panelRow;
  *column = cell;
  return true;
}

// The light cell that is in use: REAL (column 0) or the overriding palette
static int ActiveLightColumn(void)
{
  int palette = AutoMap_LightOverride();
  return palette < 0 ? 0 : palette + 1;
}

static void CarryOutPanelClick(const std::vector<AutoMapChampionLevels> &champions, int row,
                               int column)
{
  if (row == LightRow(champions))
  {
    AutoMap_SetLightOverride(column - 1);
    return;
  }
  if (row == MonstersRow(champions))
  {
    AutoMap_SetMonstersFrozen(column == 1);
    return;
  }
  if (row == ForgetRow(champions))
  {
    // The first three leave the player's notes alone: they are the player's, not the map's
    if (column == 0) AutoMap_ForgetExploredLevel(shownDrawList.level);
    else if (column == 1) AutoMap_ForgetExploredDungeon();
    else if (column == 2) AutoMap_ForgetLearned();
    else AutoMap_ForgetPlayerNotes();
    return;
  }
  if (row == MeasureRow(champions))
  {
    AutoMap_RequestDecorationMeasure();
    return;
  }
  if (champions[(size_t)row].canGainLevel[column]) AutoMap_RequestLevelUp(row, column);
}

static bool LevelPanelHandlePointerEvent(const SDL_Event &event)
{
  if (!debugMenuOn) return false;
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
      && PointInRect(event.button.x, event.button.y, LevelButtonRect()))
  {
    if (levelPanelOpen) LevelPanelClose();
    else LevelPanelOpen();
    return true;
  }
  if (!levelPanelOpen) return false;
  std::vector<AutoMapChampionLevels> champions = AutoMap_ChampionLevels();
  if (event.type == SDL_MOUSEMOTION)
  {
    int row = -1, column = -1;
    LevelPanelCellAtPixel(champions, event.motion.x, event.motion.y, &row, &column);
    if (row != levelPanelHoveredRow || column != levelPanelHoveredColumn)
    {
      levelPanelHoveredRow = row;
      levelPanelHoveredColumn = column;
      isDirty = true;
    }
    return PointInRect(event.motion.x, event.motion.y, LevelPanelRect(champions));
  }
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
  {
    if (!PointInRect(event.button.x, event.button.y, LevelPanelRect(champions))) return false;
    int row = -1, column = -1;
    if (LevelPanelCellAtPixel(champions, event.button.x, event.button.y, &row, &column))
    {
      CarryOutPanelClick(champions, row, column);
      isDirty = true;
    }
    return true;
  }
  return false;
}

static void DrawLevelButton(void)
{
  if (!debugMenuOn) return;
  SDL_Rect button = LevelButtonRect();
  Fill(button, RULER_BACKGROUND);
  UseColour(levelPanelOpen ? LINK_BADGE : WHITE);
  SDL_RenderDrawRect(mapRenderer, &button);
  const char *mark = "+";
  int w = AutoMapScene_TextWidth(mark) * DIGIT_SCALE;
  DrawLegendText(button.x + (button.w - w) / 2, button.y + (button.h - 5 * DIGIT_SCALE) / 2,
                 mark, levelPanelOpen ? LINK_BADGE : WHITE);
}

// One cell of the panel: a hovered cell gets the ruler backdrop and the badge colour
static void DrawPanelCell(int cellX, int y, int cellW, const char *label, bool isHovered,
                          const SDL_Color &colour)
{
  if (isHovered)
  {
    SDL_Rect cellRect = {cellX, y - 4, cellW, DEBUG_MENU_ROW_H};
    Fill(cellRect, RULER_BACKGROUND);
  }
  DrawLegendText(cellX + LEVEL_PANEL_GAP / 2, y, label, isHovered ? LINK_BADGE : colour);
}

// A tool row: the cell in use (activeColumn, -1 for none) stands out in white
static void DrawToolRow(int row, const char *name, const std::vector<std::string> &cells,
                        int activeColumn, int nameX, int cellsX, int y)
{
  DrawLegendText(nameX, y, name, RULER_TEXT);
  int cellX = cellsX;
  for (size_t column = 0; column < cells.size(); column++)
  {
    int w = ToolCellWidth(cells[column]);
    bool isHovered = row == levelPanelHoveredRow && (int)column == levelPanelHoveredColumn;
    const SDL_Color &colour = (int)column == activeColumn ? WHITE : RULER_TEXT;
    DrawPanelCell(cellX, y, w, cells[column].c_str(), isHovered, colour);
    cellX += w;
  }
}

static void DrawLevelPanel(void)
{
  if (!levelPanelOpen) return;
  std::vector<AutoMapChampionLevels> champions = AutoMap_ChampionLevels();
  SDL_Rect plate = LevelPanelRect(champions);
  Fill(plate, BACKGROUND);
  Outline(plate, WHITE);
  int nameX = plate.x + DEBUG_MENU_PAD;
  int y = plate.y + DEBUG_MENU_PAD;
  DrawLegendText(nameX, y, LevelPanelTitle(champions), WHITE);
  y += DEBUG_MENU_ROW_H;
  int cellsX = LevelPanelCellsX(champions);
  for (size_t champion = 0; champion < champions.size(); champion++)
  {
    const AutoMapChampionLevels &levels = champions[champion];
    DrawLegendText(nameX, y, levels.name.c_str(), RULER_TEXT);
    for (int skill = 0; skill < 4; skill++)
    {
      bool isHovered = (int)champion == levelPanelHoveredRow
                       && skill == levelPanelHoveredColumn && levels.canGainLevel[skill];
      char label[16];
      std::snprintf(label, sizeof(label), "%s %d", LEVEL_PANEL_SKILLS[skill], levels.level[skill]);
      DrawPanelCell(cellsX + skill * LevelPanelCellWidth(), y, LevelPanelCellWidth(), label,
                    isHovered, levels.canGainLevel[skill] ? RULER_TEXT : RULER_TICK);
    }
    y += DEBUG_MENU_ROW_H;
  }
  DrawToolRow(LightRow(champions), LIGHT_ROW_NAME, LightCells(), ActiveLightColumn(), nameX,
              cellsX, y);
  y += DEBUG_MENU_ROW_H;
  DrawToolRow(MonstersRow(champions), MONSTERS_ROW_NAME, MonstersCells(),
              AutoMap_MonstersAreFrozen() ? 1 : 0, nameX, cellsX, y);
  y += DEBUG_MENU_ROW_H;
  DrawToolRow(ForgetRow(champions), FORGET_ROW_NAME, ForgetCells(), -1, nameX, cellsX, y);
  y += DEBUG_MENU_ROW_H;
  DrawToolRow(MeasureRow(champions), MEASURE_ROW_NAME, MeasureCells(), -1, nameX, cellsX, y);
}

static void DrawLegend(void)
{
  if (legendPage == 0) return;
  SDL_Rect plate = LegendPanelRect();
  Fill(plate, BACKGROUND);
  Outline(plate, WHITE);
  int count = 0;
  const LegendRow *rows = LegendRows(&count);
  int textX = plate.x + LEGEND_PAD + LEGEND_SWATCH_W + LEGEND_TEXT_GAP;
  DrawLegendText(textX, plate.y + LEGEND_PAD, LegendTitle(), WHITE);
  int y = plate.y + LEGEND_PAD + 5 * DIGIT_SCALE + 4;
  for (int i = 0; i < count; i++)
  {
    int rowH = LegendRowH(rows[i].swatch);
    int swatchH = LegendSwatchH(rows[i].swatch);
    SDL_Rect box = {plate.x + LEGEND_PAD, y + (rowH - swatchH) / 2,
                    LEGEND_SWATCH_W, swatchH};
    DrawLegendSwatch(box, rows[i].swatch);
    DrawLegendText(textX, y + (rowH - 5 * DIGIT_SCALE) / 2, rows[i].label, RULER_TEXT);
    y += rowH;
  }
}

// Renders the unmoving layer into the cache texture when the draw list or the zoom
// changed. False when the renderer cannot hold a target: the caller draws directly.
static bool RenderStaticCache(const AutoMapView &view)
{
  int levelPixelsX = shownDrawList.width * view.cellSize;
  int levelPixelsY = shownDrawList.height * view.cellSize;
  if (levelPixelsX <= 0 || levelPixelsY <= 0) return false;
  if (staticCache == NULL || staticSerial != drawListSerial || staticCellSize != view.cellSize)
  {
    if (staticCache != NULL) SDL_DestroyTexture(staticCache);
    staticCache = SDL_CreateTexture(mapRenderer, SDL_PIXELFORMAT_RGBA8888,
                                     SDL_TEXTUREACCESS_TARGET, levelPixelsX, levelPixelsY);
    staticSerial = drawListSerial;
    staticCellSize = view.cellSize;
    if (staticCache == NULL) return false;
    if (SDL_SetRenderTarget(mapRenderer, staticCache) < 0)
    {
      SDL_DestroyTexture(staticCache);
      staticCache = NULL;
      return false;
    }
    AutoMapView cacheView = {view.cellSize, 0, 0};
    for (size_t i = 0; i < shownDrawList.cells.size(); i++)
    {
      DrawCellBase(cacheView, shownDrawList.cells[i], shownDrawList);
    }
    DrawGlimpses(cacheView, shownDrawList);
    DrawPlayerNotes(cacheView, shownDrawList);
    SDL_SetRenderTarget(mapRenderer, NULL);
    staticRenders++;
  }
  else
  {
    staticBlits++;
  }
  return true;
}

static void BlitStaticCache(const AutoMapView &view, int mapWidth, int mapHeight)
{
  int levelPixelsX = shownDrawList.width * view.cellSize;
  int levelPixelsY = shownDrawList.height * view.cellSize;
  int relativeX = view.originX - RULER;
  int relativeY = view.originY - RULER;
  SDL_Rect source = {std::max(0, -relativeX), std::max(0, -relativeY), 0, 0};
  SDL_Rect target = {RULER + std::max(0, relativeX), RULER + std::max(0, relativeY), 0, 0};
  source.w = target.w = std::min(levelPixelsX - source.x, mapWidth - (target.x - RULER));
  source.h = target.h = std::min(levelPixelsY - source.y, mapHeight - (target.y - RULER));
  if (source.w <= 0 || source.h <= 0) return;
  SDL_RenderCopy(mapRenderer, staticCache, &source, &target);
}

// What moves between rebuilds: the party, the monsters, the missiles, hover arrows
// and the tile-contents tooltip
static void DrawDynamicLayer(const AutoMapView &view)
{
  for (size_t i = 0; i < shownDrawList.cells.size(); i++)
  {
    const AutoMapShownCell &shown = shownDrawList.cells[i];
    DrawMissiles(CellArea(view, shown.x, shown.y), shown, shownDrawList);
  }
  if (shownDrawList.partyIsOnShownLevel) DrawParty(view, shownDrawList.party);
  if (Shows(AutoMapShowMonsters)) DrawMonsters(view, shownDrawList);
  DrawSearchMarkers(view);
  DrawLinkArrows(view);
  DrawLinkModeHighlights(view);
  DrawTileTooltip(view);
}

static void DrawLevel(void)
{
  AutoMapView view = ViewOfShownLevel();
  if (view.cellSize == 0) return;
  int windowWidth = 0, windowHeight = 0;
  SDL_GetRendererOutputSize(mapRenderer, &windowWidth, &windowHeight);
  int mapWidth = windowWidth - 2 * RULER;
  int mapHeight = windowHeight - 2 * RULER;
  // The zoomed level reaches past the map area: keep it off the rulers
  SDL_Rect mapArea = {RULER, RULER, mapWidth, mapHeight};
  bool cached = RenderStaticCache(view);
  SDL_RenderSetClipRect(mapRenderer, &mapArea);
  if (cached)
  {
    BlitStaticCache(view, mapWidth, mapHeight);
  }
  else
  {
    for (size_t i = 0; i < shownDrawList.cells.size(); i++)
    {
      DrawCellBase(view, shownDrawList.cells[i], shownDrawList);
    }
    DrawGlimpses(view, shownDrawList);
    DrawPlayerNotes(view, shownDrawList);
  }
  DrawDynamicLayer(view);
  SDL_RenderSetClipRect(mapRenderer, NULL);
  DrawRulers(view, windowWidth, windowHeight);
  DrawLegendButton();
  DrawSearchButton();
  DrawLevelButton();
  DrawLinkModeHint();
  DrawLegend();
  DrawSearchPanel();
  DrawLevelPanel();
  DrawDebugMenu();
}

void AutoMapWindow_PaintIfDirty(void)
{
  // A drag sends dozens of motion events per second; repaint at frame rate instead of
  // once per event. The flag stays set, so the next loop wake (timer events arrive
  // constantly) flushes the pending paint.
  static const Uint32 SMALLEST_REPAINT_GAP = 16;
  static Uint32 lastPaint = 0;
  // Shift unfolds the hovered tooltip and decides whether the wheel scrolls it, but a Shift
  // press sends the map no event when the game window has the keyboard, and the map ignores
  // it when it has. Without a repaint the tooltip kept its folded lines and no scroll range,
  // so the first notch zoomed instead. SDL keeps the modifier state for every window.
  static bool shiftWasHeld = false;
  bool shiftIsHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
  if (shiftIsHeld != shiftWasHeld)
  {
    shiftWasHeld = shiftIsHeld;
    if (hoverValid) isDirty = true;
  }
  if (!isShown || !isDirty) return;
  if (SDL_GetTicks() - lastPaint < SMALLEST_REPAINT_GAP) return;
  Uint64 paintStart = SDL_GetPerformanceCounter();
  UseColour(BACKGROUND);
  SDL_RenderClear(mapRenderer);
  if (shownDrawList.level != AUTOMAP_NO_LEVEL) DrawLevel();
  SDL_RenderPresent(mapRenderer);
  isDirty = false;
  drawListWasPainted = true;
  lastPaint = SDL_GetTicks();
  if (MapTimingOn())
  {
    paintMicroseconds += (Uint64)((SDL_GetPerformanceCounter() - paintStart) * MicrosecondsPerTick());
    paintCount++;
    if (paintCount >= 60)
    {
      printf("Automap timing: %d paints, %.1f ms each, static renders %d, blits %d\n", paintCount,
             paintMicroseconds / 1000.0 / paintCount, staticRenders, staticBlits);
      paintMicroseconds = 0;
      paintCount = 0;
      staticRenders = 0;
      staticBlits = 0;
    }
  }
}
