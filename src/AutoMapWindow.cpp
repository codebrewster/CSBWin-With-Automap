// AutoMapWindow.cpp : the second window, which draws the map.
#include "stdafx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

#include "AutoMapScene.h"
#include "AutoMapTooltip.h"
#include "AutoMapBar.h"
#include "AutoMapExportDialog.h"
#include "AutoMapLayout.h"
#include "AutoMapPaint.h"
#include "AutoMapSprites.h"
#include "AutoMapWindow.h"
#include "AutoMapWindowTooltip.h"
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
// The cells the party stood on: warm, apart from the green of the explored tint, drawn instead of it
static const SDL_Color WALKED_HIGHLIGHT = {0xff, 0x9a, 0x2e, 72};
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
static const SDL_Color STAIRS_UP_PLATE = {0xff, 0xf6, 0xdc, 0xff};
static const SDL_Color STAIRS_DOWN_PLATE = {0xc9, 0xc0, 0xad, 0xff};
static const SDL_Color TELEPORTER_HAZE = {80, 200, 255, 115};
static const SDL_Color TELEPORTER_EDGE = {150, 225, 255, 204};
static const SDL_Color PIT_FILL = {0x2e, 0x2b, 0x27, 0xff};
static const SDL_Color PIT_MARKER = {0x00, 0x00, 0x00, 0xff};
static const SDL_Color PIT_MARKER_EDGE = {0x8a, 0x4a, 0x4a, 0xff};
static const SDL_Color PARTY = {0xff, 0x4d, 0x4d, 0xff};
static const SDL_Color WHITE = {0xff, 0xff, 0xff, 0xff};
static const SDL_Color FACING_MARK_EDGE = {0x00, 0x00, 0x00, 0xff};
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
// The line along a decorated face: bright for a switch the party knows about, dark brown for
// everything else, so it stands out from the stone either way
static const SDL_Color KNOWN_SWITCH_LINE = {0xff, 0xf6, 0xdc, 0xff};
static const SDL_Color DECORATION_LINE = {0x3b, 0x31, 0x28, 0xff};
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
static const SDL_Color DORMANT_TELEPORT_LINE = {0x5a, 0x8a, 0x94, 0xff};
static const SDL_Color SPINNER = {0x6a, 0xd8, 0xe8, 0xff};
static const SDL_Color DORMANT_SPINNER = {0x40, 0x6e, 0x76, 0xff};
static const SDL_Color PAD = {0xc8, 0x9a, 0x4a, 0xff};
static const SDL_Color PAD_DORMANT = {0x8a, 0x84, 0x7a, 0xff};
// The "who sets it off" badge of pads and teleporters
static const SDL_Color BADGE_FRAME = {0x6b, 0x62, 0x55, 0xff};
static const SDL_Color FACING_BADGE_FRAME = {0xe0, 0xb6, 0x4a, 0xff};
static const SDL_Color TELEPORTER_BADGE_FRAME = {0x3f, 0x7d, 0x88, 0xff};
static const SDL_Color BADGE_GLYPH = {0xd8, 0xcf, 0xbd, 0xff};
static const SDL_Color GEN_TEXT = {0xff, 0x80, 0x80, 0xff};
// What a dormant teleporter or spinner would do: muted, so it never reads as active
static const SDL_Color DORMANT_TEXT = {0xa8, 0xb8, 0xbc, 0xff};
// Tooltip line styles (AutoMapTipStyle)
static const SDL_Color TIP_HEADER_TEXT = FACING_BADGE_FRAME;
static const SDL_Color TIP_STATE_TEXT = DORMANT_TEXT;
static const SDL_Color TIP_CONDITION_TEXT = SPINNER;
static const SDL_Color TIP_RESULT_TEXT = {0xff, 0xcc, 0x33, 0xff};
static const SDL_Color TIP_HINT_TEXT = {0x8a, 0x8a, 0x9a, 0xff};
// What acts on the hovered cell: a paler LINK_INCOMING, so the line reads as its blue lines
static const SDL_Color TIP_FEEDER_TEXT = {0x7f, 0xb4, 0xff, 0xff};
// The game's pressure plate: what a pad without a floor picture of its own is drawn as, faint
static const int PRESSURE_PLATE_PICTURE = 258;
static const SDL_Color GENERATOR = {0xc0, 0x50, 0x50, 0xff};
static const SDL_Color GENERATOR_SPENT = {0x8a, 0x5a, 0x5a, 0xff};
static const SDL_Color COG = {0x9a, 0x94, 0x88, 0xff};
static const SDL_Color LINK_BADGE = {0xff, 0xc8, 0x3a, 0xff};
// The lines from a switch or pad to what it operates
static const SDL_Color LINK_GOLD = {0xff, 0xcc, 0x33, 0xff};
// Feeders of a hovered relay, and goals that still wait on something
static const SDL_Color LINK_INCOMING = {0x4a, 0x9e, 0xff, 0xff};
static const SDL_Color LINK_WARNING = {0xff, 0x6a, 0x2b, 0xff};
// The player's own notes: pit marks, player links and link mode. A violet apart from the link
// yellow, the feeder blue and the warning orange, and brighter than the false-wall purple.
static const SDL_Color PLAYER_NOTE = {0xc7, 0x7d, 0xff, 0xff};
// A replay's breakpoint: where the replay will pause
static const SDL_Color BREAKPOINT = {0xff, 0x3b, 0x3b, 0xff};

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
// The tooltip magnified (Ctrl+wheel, map_tip_scale) is drawn into this first, at its scale
static SDL_Texture *tooltipTexture = NULL;
static int tooltipTextureW = 0;
static int tooltipTextureH = 0;
static int tooltipScale = 1;
static const int TOOLTIP_SCALE_MOST = 4;

static void DropTooltipTexture(void)
{
  if (tooltipTexture != NULL) SDL_DestroyTexture(tooltipTexture);
  tooltipTexture = NULL;
  tooltipTextureW = 0;
  tooltipTextureH = 0;
}
static Uint32 staticSerial = 0;
static int staticCellSize = 0;
static WindowPlacement placement = {SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DEFAULT_SIZE, DEFAULT_SIZE};
static bool wasShownWhenSettingsWereRead = false;
static bool isShown = false;
static bool isDirty = false;
static bool drawListWasPainted = true;
static bool pointerIsOverMap = false;
// The last pointer pixel over the map window, for the wheel to zoom towards: wheel events of
// older SDL versions carry no position of their own
static bool pointerPixelKnown = false;
static int pointerPixelX = 0;
static int pointerPixelY = 0;
// The pointer in window pixels, for the bar: wheel events carry no position
static int barPointerX = 0;
static int barPointerY = 0;
static int pointerStateOfTheGame = SDL_ENABLE;
static int zoomSteps = 0;
// Set by a zoom towards the pointer: a level that still fits an axis is then placed by the
// view centre, not centred, so that the cell under the pointer does not jump
static bool zoomPlacesFreely = false;
// One bit per AutoMapShown; everything by default
static unsigned shownThings = (1u << AUTOMAP_SHOWN_COUNT) - 1;
// How a pit is drawn: by the game's picture, by stripes, or by the cell size (map_pit_style)
static AutoMapPitStyle pitStyle = AutoMapPitStyleAuto;
// Wall decorations in a square by their face (true) or as big tiles across the wall (false)

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
// A left-drag pans the zoomed view: the exact centre (fractions included) freezes where
// the drag started and the view stops following the party until a middle-click recentres it.
// Fractions keep re-pans continuous and zooming centred on the same spot.
static bool viewFollowsParty = true;
// Following asked for while another level is on screen: the view keeps still until a draw list
// with the party arrives, or the old level would be centred for a few frames (a ghost image)
static bool followWhenPartyShown = false;
// Set only while AutoMapWindow_RenderToFile draws: a fixed cell size centred on the hovered cell,
// and Shift as if held
static int renderCellSize = 0;
static bool renderShift = false;
static float viewCenterX = 0;
// The centre for a level asked for but not on screen yet (Page Down, a search hit elsewhere)
static AutoMapPendingCentre pendingCentre = AutoMapScene_NoPendingCentre();
// The centre waiting for another level is a search hit's: it lands below the search panel too
static bool pendingCentreIsSearchHit = false;
// The stair a Ctrl+click or ASCEND / DESCEND came out on, in dungeon coordinates. It is framed
// (the rest of the level dims) once its level is on screen and the map shows the cell as stairs;
// a click, the wheel, another way of changing the level or leaving the level ends it.
static bool stairMarkOn = false;
static bool stairMarkArrived = false;
static int stairMarkLevel = AUTOMAP_NO_LEVEL;
static int stairMarkX = 0;
static int stairMarkY = 0;
static void ShiftCentreBelowSearchPanel(void);
static float viewCenterY = 0;
static const int DRAG_START_DISTANCE = 4;
static void EndLeftDrag(void);
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
// The icon grid's rows that did not fit the last time the tooltip was drawn: the scroll takes
// them first, then the footer lines (the footer is laid out before the grid knows its rows)
static int tooltipGridHiddenRows = 0;
// Window pixel of the pointer when the hover was last updated: the icon tooltip
// parks next to the cursor, not on top of the hovered cell.
static int hoverPixelX = 0;
static int hoverPixelY = 0;
static void ClearHover(void);
// Legend overlay state and geometry, drawn with the legend below.
static bool PointInRect(int x, int y, const SDL_Rect &area);
// Item search state: the query the player typed, the hits across all levels
// (already fog-filtered, so unexplored never matches), the selected hit and
// the first shown row of the scrollable results panel.
static bool searchOpen = false;
static std::string searchQuery;
static std::vector<AutoMapSearchHit> searchHits;
static int searchSelected = 0;
// searchSelected indexes searchHits; searchTop is the first row of the panel (headings count)
static int searchTop = 0;
static std::set<int> searchFolded;
// SDL text input is one switch for the whole program, and the game's ImGui
// dialogs (the Save As name) need it on, so the search box hands it back the
// way it found it rather than switching it off.
static bool searchStartedTextInput = false;
static std::vector<AutoMapSearchRow> SearchRowsNow(void)
{
  return AutoMapScene_SearchRows(searchHits, shownDrawList.level, searchFolded);
}

static int RowOfSelectedHit(const std::vector<AutoMapSearchRow> &rows)
{
  for (size_t r = 0; r < rows.size(); r++)
  {
    if (!rows[r].heading && rows[r].hit == searchSelected) return (int)r;
  }
  return -1;
}

// Up and Down move through the hits in the order the panel lists them, past the headings
static void SearchStep(int delta)
{
  std::vector<AutoMapSearchRow> rows = SearchRowsNow();
  int at = RowOfSelectedHit(rows);
  for (int r = at + delta; r >= 0 && r < (int)rows.size(); r += delta)
  {
    if (rows[(size_t)r].heading) continue;
    searchSelected = rows[(size_t)r].hit;
    return;
  }
}

static void SearchGiveBackTextInput(void);
static void SearchFullRefresh(void);
static std::vector<AutoMapSearchRow> SearchRowsNow(void);
static void SearchRefreshCurrentLevel(void);
static void SearchCenterOnSelected(void);
static void SearchEnsureSelectedVisible(void);
static int SearchVisibleRows(void);
static bool BelongsToMapWindow(Uint32 windowId);
// A right-click on a cell the map shows opens one menu. Its first rows are the player's notes:
// a hidden-pit mark for a floor tile, LINK FROM HERE (link mode) for a floor tile or a wall
// face, and REMOVE LINKS for a cell a player link starts or ends on. While a replay runs, PAUSE
// REPLAY HERE (or REMOVE REPLAY PAUSE) comes first on any cell but a wall. Set map_debug = 1 in the settings and the testing rows follow behind a separator, on any
// cell: teleport the party there or spawn a monster there. A spawn asks for the monster, then how
// many (skipped when only one fits a cell), for one alone its spot on the cell (skipped when it
// fills the cell), and the facing. The menu rows hold the cell they act on; it closes with any
// click outside an enabled row, the wheel, Escape or a change of the shown level.
enum DebugMenuRowKind
{
  DebugRowTeleport, DebugRowMonster, DebugRowCount, DebugRowSpot, DebugRowFacing,
  // The player's notes; choice is the mark (-1 none, 0 closed, 1 open) or the link's side
  MenuRowPitMark, MenuRowLinkFromHere, MenuRowRemoveLinks,
  // A replay's breakpoint on the cell, set or removed
  MenuRowBreakpoint,
  // Ascend or descend the stairs on the cell
  MenuRowFollowStairs,
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
  char stone[] = "stone";
  AutoMapBar_SetStyle(csb_get_config_string(SETTINGS_SECTION, "map_bar_style", stone));
  tooltipScale = std::max(1, std::min(TOOLTIP_SCALE_MOST, csb_get_config_int(SETTINGS_SECTION, "map_tip_scale", 1)));
  int style = csb_get_config_int(SETTINGS_SECTION, "map_pit_style", AutoMapPitStyleAuto);
  pitStyle = style >= 0 && style < AUTOMAP_PIT_STYLE_COUNT ? (AutoMapPitStyle)style : AutoMapPitStyleAuto;
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
  // Written back so the key is there to edit
  std::string style = AutoMapBar_StyleName();
  csb_set_config_string(SETTINGS_SECTION, "map_bar_style", &style[0]);
  csb_set_config_int(SETTINGS_SECTION, "map_tip_scale", tooltipScale);
  csb_set_config_int(SETTINGS_SECTION, "map_pit_style", (int)pitStyle);
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

AutoMapPitStyle AutoMapWindow_PitStyle(void)
{
  return pitStyle;
}

void AutoMapWindow_SetPitStyle(AutoMapPitStyle style)
{
  pitStyle = style;
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
  DropTooltipTexture();
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
  AutoMapPaint_Use(mapRenderer);
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
  pointerPixelKnown = false;
  if (!pointerIsOverMap) return;
  pointerIsOverMap = false;
  SDL_ShowCursor(pointerStateOfTheGame);
}

void AutoMapWindow_Hide(void)
{
  if (!isShown) return;
  RememberPlacement();
  GiveThePointerBackToTheGame();
  EndLeftDrag();
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
    zoomPlacesFreely = false;
    sprites.ForgetAll();
    ShowLevelInTitle(drawList.level);
    ClearHover();
    DebugMenuClose();
    LinkModeEnd();
  }
  sprites.UseColourSchemes(drawList.paletteScheme9, drawList.paletteScheme10);
  shownDrawList = drawList;
  // The stair mark ends when the level it belongs to has been shown and something else follows
  if (stairMarkOn)
  {
    if (drawList.level == stairMarkLevel) stairMarkArrived = true;
    else if (stairMarkArrived) stairMarkOn = false;
  }
  // The level asked for is here: only now does the view move to the spot asked for on it
  float x = 0, y = 0;
  if (followWhenPartyShown && (drawList.partyIsOnShownLevel || AutoMap_PartyLevel() == drawList.level))
  {
    followWhenPartyShown = false;
    viewFollowsParty = true;
  }
  if (AutoMapScene_TakeCentre(&pendingCentre, drawList.level, &x, &y))
  {
    followWhenPartyShown = false;
    viewFollowsParty = false;
    viewCenterX = x;
    viewCenterY = y;
    if (pendingCentreIsSearchHit && searchOpen) ShiftCentreBelowSearchPanel();
    pendingCentreIsSearchHit = false;
  }
  drawListSerial++;
  isDirty = true;
  drawListWasPainted = false;
  if (searchOpen) SearchRefreshCurrentLevel();
}

// ---------------------------------------------------------------------------------------------
// Item search



// The search and level panels open at the top left of the map, under the bar
static int PanelLeft(void)
{
  return RULER + 4;
}

static void SearchFullRefresh(void)
{
  searchHits.clear();
  searchSelected = 0;
  searchTop = 0;
  if (!searchOpen || searchQuery.empty()) return;
  searchHits = AutoMap_SearchDungeon(searchQuery);
  // The selection starts on the first hit the panel lists
  searchSelected = std::max(0, AutoMapScene_FirstListedHit(SearchRowsNow()));
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
  AutoMapRecordId selectedGroup = AUTOMAP_NO_RECORD;
  // The refresh runs with every new draw list: it must not undo the wheel, so it keeps the
  // selection in view only when the selection was in view
  int selectedRow = RowOfSelectedHit(SearchRowsNow());
  bool selectedWasShown = selectedRow >= searchTop && selectedRow < searchTop + SearchVisibleRows();
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
    selectedGroup = old.kind == AutoMapHitMonster ? old.group : AUTOMAP_NO_RECORD;
  }
  std::vector<AutoMapSearchHit> merged;
  for (size_t i = 0; i < searchHits.size(); i++)
  {
    if (searchHits[i].level != shownDrawList.level) merged.push_back(searchHits[i]);
  }
  merged.insert(merged.end(), fresh.begin(), fresh.end());
  AutoMapScene_SortSearchHits(&merged, searchQuery);
  searchHits = merged;
  searchSelected = std::max(0, AutoMapScene_FirstListedHit(SearchRowsNow()));
  for (size_t i = 0; i < searchHits.size(); i++)
  {
    // A monster group walks: found again by its record, wherever it stands now
    if (selectedGroup != AUTOMAP_NO_RECORD)
    {
      if (searchHits[i].kind == AutoMapHitMonster && searchHits[i].group == selectedGroup)
      {
        searchSelected = (int)i;
        break;
      }
      continue;
    }
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
    int maxTop = (int)SearchRowsNow().size() - SearchVisibleRows();
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
  LevelPanelClose();
  searchOpen = true;
  searchQuery.clear();
  searchHits.clear();
  searchFolded.clear();
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

static AutoMapView ViewOfShownLevel(void);
static SDL_Rect SearchPanelRect(void);
static SDL_Rect SearchPanelPlace(void);

// The panel hangs from the top of the map: the hit goes in the middle of the map below it.
// Near the top of a level (or with the whole level in view) the view cannot move that far;
// the panel then steps aside (SearchPanelRect).
static void ShiftCentreBelowSearchPanel(void)
{
  AutoMapView view = ViewOfShownLevel();
  if (view.cellSize > 0) viewCenterY -= SearchPanelPlace().h / 2.0f / view.cellSize;
}

static void SearchCenterOnSelected(void)
{
  if (!searchOpen || searchHits.empty()) return;
  if (searchSelected < 0 || searchSelected >= (int)searchHits.size()) searchSelected = 0;
  const AutoMapSearchHit &hit = searchHits[(size_t)searchSelected];
  // A hit on another level switches the map there first; the per-tick rebuild
  // brings the level within a moment and the frozen centre lands on the cell.
  if (hit.level != AUTOMAP_NO_LEVEL && hit.level != shownDrawList.level)
  {
    // The level on screen keeps its view until the hit's level arrives
    AutoMapScene_RequestCentre(&pendingCentre, hit.level, (float)hit.x, (float)hit.y);
    pendingCentreIsSearchHit = true;
    AutoMap_ShowLevel(hit.level);
    return;
  }
  pendingCentre = AutoMapScene_NoPendingCentre();
  pendingCentreIsSearchHit = false;
  followWhenPartyShown = false;

  viewFollowsParty = false;
  viewCenterX = (float)hit.x;
  viewCenterY = (float)hit.y;
  ShiftCentreBelowSearchPanel();
  isDirty = true;
}

static void SearchEnsureSelectedVisible(void)
{
  int rows = SearchVisibleRows();
  std::vector<AutoMapSearchRow> list = SearchRowsNow();
  int selected = RowOfSelectedHit(list);
  if (selected < 0) selected = 0;
  if (selected < searchTop) searchTop = selected;
  if (selected >= searchTop + rows) searchTop = selected - rows + 1;
  int maxTop = (int)list.size() - rows;
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
  // The export dialog takes every key and all typing of the map window while it is open
  if (AutoMapExportDialog_IsOpen() && (event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT)
      && KeyWindowOf(event) != 0 && BelongsToMapWindow(KeyWindowOf(event)))
  {
    if (event.type == SDL_TEXTINPUT) AutoMapExportDialog_HandleText(event.text.text);
    else AutoMapExportDialog_HandleKey(event.key.keysym.sym, event.key.keysym.mod);
    isDirty = true;
    return true;
  }
  if (event.type == SDL_KEYDOWN && KeyWindowOf(event) != 0 && BelongsToMapWindow(KeyWindowOf(event))
      && AutoMapBar_HandleKey(event.key.keysym.sym))
  {
    isDirty = true;
    return true;
  }
  if (event.type != SDL_KEYDOWN && event.type != SDL_TEXTINPUT) return false;
  if (!isShown || !BelongsToMapWindow(KeyWindowOf(event))) return false;
  if (event.type == SDL_TEXTINPUT)
  {
    // The level list types digits of its own; they are not for the search
    if (AutoMapBar_LevelListIsOpen()) return true;
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
      SearchStep(-1);
      SearchEnsureSelectedVisible();
      SearchCenterOnSelected();
    }
    return true;
  }
  if (sym == SDLK_DOWN)
  {
    if (!searchHits.empty())
    {
      SearchStep(1);
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
    break;
  case SDL_WINDOWEVENT_LEAVE:
    GiveThePointerBackToTheGame();
    ClearHover();
    break;
  case SDL_WINDOWEVENT_FOCUS_LOST: EndLeftDrag(); ClearHover(); break;
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
  zoomPlacesFreely = false;
  pendingCentre = AutoMapScene_NoPendingCentre();
  pendingCentreIsSearchHit = false;
  if (shownDrawList.partyIsOnShownLevel || AutoMap_PartyLevel() == shownDrawList.level)
  {
    followWhenPartyShown = false;
    viewFollowsParty = true;
  }
  else
  {
    followWhenPartyShown = true;
  }
  isDirty = true;
}

void AutoMapWindow_RecenterView(void)
{
  RecenterViewOnParty();
}

// The map draws below the bar, in a viewport whose top left is (0, 0): its size, and switching to it
static void OutputSizeBelowBar(int *width, int *height)
{
  SDL_GetRendererOutputSize(mapRenderer, width, height);
  *height -= AutoMapBar_Height();
}

static void UseMapViewport(void)
{
  int width = 0, height = 0;
  SDL_GetRendererOutputSize(mapRenderer, &width, &height);
  SDL_Rect below = {0, AutoMapBar_Height(), width, height - AutoMapBar_Height()};
  SDL_RenderSetViewport(mapRenderer, &below);
}

static void UseWholeWindow(void)
{
  SDL_RenderSetViewport(mapRenderer, NULL);
}


static void MapAreaSize(int *mapWidth, int *mapHeight)
{
  int windowWidth = 0, windowHeight = 0;
  if (mapRenderer != NULL) OutputSizeBelowBar(&windowWidth, &windowHeight);
  *mapWidth = windowWidth - 2 * RULER;
  *mapHeight = windowHeight - 2 * RULER;
}

static AutoMapView ViewOfShownLevel(void);
static AutoMapViewRequest RequestOfShownLevel(void);

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

static void UpdateHoverFromPixel(int pixelX, int pixelY);
static bool CellAtPixel(int pixelX, int pixelY, int *cellX, int *cellY);

static void StepLevelCenteredOn(int delta, int cellX, int cellY);

// Left-drag pans like a web map; the cursor stays visible, no relative mode
static bool leftDragging = false;
static bool leftDragMoved = false;
static int leftDragDistance = 0;

static void StartLeftDrag(void)
{
  leftDragging = true;
  leftDragMoved = false;
  leftDragDistance = 0;
  SDL_CaptureMouse(SDL_TRUE);
}

static void DragLeft(int deltaX, int deltaY)
{
  if (!leftDragging) return;
  AutoMapView view = ViewOfShownLevel();
  if (view.cellSize <= 0) return;
  leftDragDistance += (deltaX < 0 ? -deltaX : deltaX) + (deltaY < 0 ? -deltaY : deltaY);
  if (!leftDragMoved && leftDragDistance < DRAG_START_DISTANCE) return;
  if (!leftDragMoved)
  {
    leftDragMoved = true;
    ClearHover();
    FreezeExactViewCenter(view);
    followWhenPartyShown = false;

    viewFollowsParty = false;
    if (shownDrawList.level != AUTOMAP_NO_LEVEL) AutoMap_ShowLevel(shownDrawList.level);
  }
  viewCenterX -= (float)deltaX / view.cellSize;
  viewCenterY -= (float)deltaY / view.cellSize;
  isDirty = true;
}

static void EndLeftDrag(void)
{
  if (!leftDragging) return;
  leftDragging = false;
  SDL_CaptureMouse(SDL_FALSE);
}

// The wheel zooms towards the pointer: the spot under it stays where it is, so a zoom that
// moves the view stops following the party, like a pan. While the whole level fits, the
// centre changes nothing on screen and the view keeps following. Zoomed back out to the
// whole level, the view follows the party again, as after a middle-click; a level the user
// browsed to stays on screen, though, when the party is elsewhere.
static void ZoomTowardsPointer(int notches)
{
  int newSteps = AutoMapScene_ClampZoomSteps(zoomSteps + (notches > 0 ? 1 : -1), RequestOfShownLevel());
  if (newSteps == zoomSteps) return;
  AutoMapView before = ViewOfShownLevel();
  zoomSteps = newSteps;
  isDirty = true;
  if (AutoMapScene_ShowsWholeLevel(RequestOfShownLevel()))
  {
    zoomPlacesFreely = false;
    if (notches < 0 && !viewFollowsParty)
    {
      RecenterViewOnParty();
      if (shownDrawList.partyIsOnShownLevel) AutoMap_FollowParty();
    }
    return;
  }
  int mapWidth = 0, mapHeight = 0;
  MapAreaSize(&mapWidth, &mapHeight);
  bool pointerOnMap = pointerPixelKnown && pointerPixelX >= RULER &&
                      pointerPixelY >= RULER && pointerPixelX < RULER + mapWidth &&
                      pointerPixelY < RULER + mapHeight;
  // Off the map (rulers, unknown position) the zoom stays on the centre, as before
  if (!pointerOnMap) return;
  AutoMapView after = ViewOfShownLevel();
  if (before.cellSize <= 0 || after.cellSize <= 0) return;
  // From the centre actually on screen: a frozen centre past the level's edge would jump
  FreezeExactViewCenter(before);
  float pointX = (float)(pointerPixelX - before.originX) / before.cellSize;
  float pointY = (float)(pointerPixelY - before.originY) / before.cellSize;
  zoomPlacesFreely = true;
  viewCenterX = AutoMapScene_CentreKeepingPointStill(viewCenterX, pointX, before.cellSize,
                                                     after.cellSize);
  viewCenterY = AutoMapScene_CentreKeepingPointStill(viewCenterY, pointY, before.cellSize,
                                                     after.cellSize);
  if (viewFollowsParty)
  {
    followWhenPartyShown = false;

    viewFollowsParty = false;
    // As for a pan: the level on screen stays while the view roams
    if (shownDrawList.level != AUTOMAP_NO_LEVEL) AutoMap_ShowLevel(shownDrawList.level);
  }
}

static void GoToLevelCenteredOn(int target, int cellX, int cellY)
{
  if (shownDrawList.level == AUTOMAP_NO_LEVEL) return;
  int levelCount = AutoMap_LevelCount();
  if (levelCount <= 0) return;
  int current = shownDrawList.level;
  if (target < 0) target = 0;
  if (target >= levelCount) target = levelCount - 1;
  stairMarkOn = false;
  int globalX = 0, globalY = 0;
  AutoMapScene_GlobalPosition(shownDrawList, cellX, cellY, &globalX, &globalY);
  // The target's draw list counts its cells from the rock the reader adds around it, so its
  // offset is the snapshot's, not the level's own (AutoMapScene_GlobalPosition)
  AutoMapLevelSnapshot targetSnapshot = AutoMapLevelReader_ReadLevel(target);
  float centerX = (float)(globalX - targetSnapshot.offsetX);
  float centerY = (float)(globalY - targetSnapshot.offsetY);
  if (target != current)
  {
    // The centre is in the target's cells: the level still on screen keeps its own view until
    // the target's draw list arrives (AutoMapWindow_ShowDrawList)
    AutoMapScene_RequestCentre(&pendingCentre, target, centerX, centerY);
    pendingCentreIsSearchHit = false;
    AutoMap_ShowLevel(target);
    return;
  }
  pendingCentre = AutoMapScene_NoPendingCentre();
  pendingCentreIsSearchHit = false;
  followWhenPartyShown = false;

  viewFollowsParty = false;
  viewCenterX = centerX;
  viewCenterY = centerY;
  isDirty = true;
  if (MapTimingOn())
  {
    printf("Automap gesture: level %d -> %d centred on (%d,%d)\n", current, target, cellX,
           cellY);
  }
}

static void StepLevelCenteredOn(int delta, int cellX, int cellY)
{
  if (shownDrawList.level == AUTOMAP_NO_LEVEL) return;
  GoToLevelCenteredOn(shownDrawList.level + delta, cellX, cellY);
}

// The shown cell at a pixel's cell when the map draws it as stairs whose target level is known
static const AutoMapShownCell *StairsShownAt(int cellX, int cellY);

// Comes out of the stairs on a cell: the level they lead to, centred on the same dungeon
// position (where the engine puts the party), with that position framed. Returns false when the
// cell is no stairs or the map does not know where they lead.
static bool FollowStairsAt(int cellX, int cellY)
{
  const AutoMapShownCell *stairs = StairsShownAt(cellX, cellY);
  if (stairs == NULL) return false;
  int target = stairs->cell.stairsTargetLevel;
  int globalX = 0, globalY = 0;
  AutoMapScene_GlobalPosition(shownDrawList, cellX, cellY, &globalX, &globalY);
  GoToLevelCenteredOn(target, cellX, cellY);
  stairMarkOn = true;
  stairMarkArrived = false;
  stairMarkLevel = target;
  stairMarkX = globalX;
  stairMarkY = globalY;
  return true;
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

static bool HandleMapPointerEvent(const SDL_Event &event)
{
  if (!IsPointerEventOfMapWindow(event)) return false;
  // Any press or turn of the wheel ends the frame on a stair; the gestures below may set it again
  if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEWHEEL) && stairMarkOn)
  {
    stairMarkOn = false;
    isDirty = true;
  }
  if (event.type == SDL_MOUSEMOTION)
  {
    pointerPixelKnown = true;
    pointerPixelX = event.motion.x;
    pointerPixelY = event.motion.y;
  }
  else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
  {
    pointerPixelKnown = true;
    pointerPixelX = event.button.x;
    pointerPixelY = event.button.y;
  }
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
    if (searchOpen)
    {
      int rowIndex = -1;
      if (SearchHitAtPixel(event.button.x, event.button.y, &rowIndex))
      {
        std::vector<AutoMapSearchRow> rows = SearchRowsNow();
        const AutoMapSearchRow &row = rows[(size_t)rowIndex];
        if (row.heading)
        {
          // A heading folds its level away, or unfolds it
          if (searchFolded.count(row.level)) searchFolded.erase(row.level);
          else searchFolded.insert(row.level);
          SearchEnsureSelectedVisible();
          isDirty = true;
          return true;
        }
        searchSelected = row.hit;
        SearchCenterOnSelected();
        return true;
      }
      // A click into the open panel but off any hit stays in the panel
      if (PointInRect(event.button.x, event.button.y, SearchPanelRect())) return true;
    }
    // In link mode a click on the map picks a target
    if (linkModeOn)
    {
      LinkModeClickAt(event.button.x, event.button.y);
      return true;
    }
    // Ctrl+click on stairs comes out of them on the other level
    if ((SDL_GetModState() & KMOD_CTRL) != 0)
    {
      int cellX = -1, cellY = -1;
      if (CellAtPixel(event.button.x, event.button.y, &cellX, &cellY) && FollowStairsAt(cellX, cellY))
      {
        return true;
      }
    }
    // On the map itself the left button pans, as on a web map; a press that hardly moves is a click
    StartLeftDrag();
    return true;
  }
  if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
  {
    EndLeftDrag();
    return true;
  }
  if (event.type == SDL_MOUSEMOTION && leftDragging)
  {
    // A release the map never saw (captured elsewhere) ends the drag at the next motion
    if ((event.motion.state & SDL_BUTTON_LMASK) == 0) EndLeftDrag();
    else DragLeft(event.motion.xrel, event.motion.yrel);
    return true;
  }
  if (event.type == SDL_MOUSEBUTTONDOWN
      && (event.button.button == SDL_BUTTON_X1 || event.button.button == SDL_BUTTON_X2))
  {
    // Side buttons step one level, centred on the hovered tile: back (X1) goes
    // down a level, forward (X2) goes up. At the top or bottom level the view
    // still recentres on the tile.
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
    bool ctrlHeld = (SDL_GetModState() & KMOD_CTRL) != 0;
    if (ctrlHeld && event.wheel.y != 0)
    {
      // Ctrl sizes the tooltip, never the map
      tooltipScale = std::max(1, std::min(TOOLTIP_SCALE_MOST, tooltipScale + (event.wheel.y > 0 ? 1 : -1)));
      isDirty = true;
    }
    else if (shiftHeld && hoverValid && tooltipScrollMax > 0)
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
      int total = (int)SearchRowsNow().size();
      int maxTop = total > rows ? total - rows : 0;
      searchTop -= event.wheel.y;
      if (searchTop < 0) searchTop = 0;
      if (searchTop > maxTop) searchTop = maxTop;
      isDirty = true;
    }
    else
    {
      ZoomTowardsPointer(event.wheel.y);
    }
  }
  else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_MIDDLE)
  {
    // A middle-click centres on the party again and follows it
    RecenterViewOnParty();
    AutoMap_FollowParty();
  }
  else if (event.type == SDL_MOUSEMOTION)
  {
    UpdateHoverFromPixel(event.motion.x, event.motion.y);
  }
  return true;
}

// The bar takes presses and the wheel above the map, and its open panels take the next press;
// everything else reaches the map shifted into the map's own coordinates, below the bar
bool AutoMapWindow_HandlePointerEvent(const SDL_Event &event)
{
  // A left release ends a pan wherever it happens
  if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && leftDragging)
  {
    EndLeftDrag();
    if (!IsPointerEventOfMapWindow(event)) return false;
  }
  int bar = AutoMapBar_Height();
  bool onMapWindow = IsPointerEventOfMapWindow(event) && mapRenderer != NULL;
  if (onMapWindow && !leftDragging)
  {
    int width = 0, height = 0;
    SDL_GetRendererOutputSize(mapRenderer, &width, &height);
    if (event.type == SDL_MOUSEMOTION)
    {
      barPointerX = event.motion.x;
      barPointerY = event.motion.y;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
        && AutoMapBar_HandlePress(event.button.x, event.button.y, width))
    {
      isDirty = true;
      return true;
    }
    // Under the export dialog the map takes no pointer: no hover, pan, zoom or menu
    if (AutoMapExportDialog_IsOpen() && event.type != SDL_MOUSEBUTTONUP)
    {
      if (hoverValid) isDirty = true;
      ClearHover();
      return true;
    }
    if (event.type == SDL_MOUSEWHEEL && event.wheel.y != 0
        && AutoMapBar_HandleWheel(barPointerX, barPointerY, event.wheel.y))
    {
      isDirty = true;
      return true;
    }
    if (event.type == SDL_MOUSEMOTION && event.motion.y < bar)
    {
      if (hoverValid) isDirty = true;
      ClearHover();
      return true;
    }
  }
  SDL_Event shifted = event;
  if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) shifted.button.y -= bar;
  if (event.type == SDL_MOUSEMOTION) shifted.motion.y -= bar;
  return HandleMapPointerEvent(shifted);
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
  // A plate with an arrow and the level the stairs lead to; without a known target, the arrow alone
  int pixel = AutoMapLayout_PlatePixel(cell.w);
  std::string label = stairs.stairsTargetLevel == AUTOMAP_NO_LEVEL ? "" : std::to_string(stairs.stairsTargetLevel);
  int arrowW = 6 * pixel;
  int width = arrowW + AutoMapPaint_TextWidth(label, pixel) + 4 * pixel - (label.empty() ? pixel : 0);
  int height = 9 * pixel;
  // Too little room: the arrow alone, then nothing
  if (!AutoMapLayout_Fits(width, height, cell.w))
  {
    label.clear();
    width = arrowW + 3 * pixel;
  }
  if (!AutoMapLayout_Fits(width, height, cell.w)) return;
  AutoMapRect plate = {cell.x + (cell.w - width) / 2, cell.y + (cell.h - height) / 2, width, height};
  const SDL_Color &colour = stairs.stairsLeadUp ? STAIRS_UP_PLATE : STAIRS_DOWN_PLATE;
  AutoMapPaint_Plate(plate, colour);
  static const unsigned char UP[5] = {0x04, 0x0e, 0x15, 0x04, 0x04};
  static const unsigned char DOWN[5] = {0x04, 0x04, 0x15, 0x0e, 0x04};
  const unsigned char *arrow = stairs.stairsLeadUp ? UP : DOWN;
  for (int row = 0; row < 5; row++)
  {
    for (int col = 0; col < 5; col++)
    {
      if (!(arrow[row] & (0x10 >> col))) continue;
      AutoMapRect dot = {plate.x + 2 * pixel + col * pixel, plate.y + 2 * pixel + row * pixel, pixel, pixel};
      AutoMapPaint_Fill(dot, colour);
    }
  }
  AutoMapPaint_Text(plate.x + 2 * pixel + arrowW, plate.y + 2 * pixel, label, colour, pixel);
}

// A teleporter's field: a checkerboard of cyan pixels in a cyan frame, in the game's pixel style
static void DrawTeleporter(const SDL_Rect &cell)
{
  SDL_Rect haze = Inset(cell, 10);
  AutoMapRect field = {haze.x, haze.y, haze.w, haze.h};
  AutoMapPaint_Dither(field, SPINNER, 140, std::max(2, cell.w / 14));
  Outline(haze, SPINNER);
}

static int NumberPixelWidth(int value);
static void DrawNumberCentred(int centreX, int y, int value);

// The destination level of a cross-level teleporter as a small centred plate,
// the same digits as the title bar. The static layer shows it on every firing
// cross-level teleporter whose target survived the fog filter; hovering draws
// the same plate over the arrow's source cell.
static void DrawLevelPlate(const SDL_Rect &cell, int level)
{
  int pixel = AutoMapLayout_PlatePixel(cell.w);
  std::string label = "L" + std::to_string(level);
  int width = AutoMapPaint_TextWidth(label, pixel) + 4 * pixel;
  int height = 9 * pixel;
  if (!AutoMapLayout_Fits(width, height, cell.w)) return;
  AutoMapRect plate = {cell.x + (cell.w - width) / 2, cell.y + (cell.h - height) / 2, width, height};
  AutoMapPaint_Plate(plate, SPINNER);
  AutoMapPaint_Text(plate.x + 2 * pixel, plate.y + 2 * pixel, label, SPINNER, pixel);
}

// Stripes in place of the picture: a pit reads as a hazard where the picture is a speck. In
// AUTO the stripes take over below PIT_STRIPES_BELOW pixels per cell.
static const int PIT_STRIPES_BELOW = 24;
static const int PIT_STRIPES_SMALLEST = 6;

static bool PitIsStriped(int cellSize)
{
  if (pitStyle == AutoMapPitStyleStripes) return true;
  return pitStyle == AutoMapPitStyleAuto && cellSize < PIT_STRIPES_BELOW;
}

static void DrawPitStripes(const SDL_Rect &cell)
{
  Fill(cell, PIT_FILL);
  if (cell.w < PIT_STRIPES_SMALLEST) return;
  SDL_Rect oldClip;
  bool wasClipped = SDL_RenderIsClipEnabled(mapRenderer) == SDL_TRUE;
  SDL_RenderGetClipRect(mapRenderer, &oldClip);
  SDL_Rect clip = cell;
  if (wasClipped && !SDL_IntersectRect(&cell, &oldClip, &clip)) return;
  SDL_RenderSetClipRect(mapRenderer, &clip);
  UseColour(PIT_MARKER_EDGE);
  int step = std::max(4, cell.w / 3);
  int thickness = cell.w >= 16 ? 2 : 1;
  for (int k = -cell.h + step / 2; k < cell.w; k += step)
  {
    for (int t = 0; t < thickness; t++)
      SDL_RenderDrawLine(mapRenderer, cell.x + k + t, cell.y + cell.h, cell.x + k + cell.h + t, cell.y);
  }
  SDL_RenderSetClipRect(mapRenderer, wasClipped ? &oldClip : NULL);
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

// The faces that carry something to draw: they share the cell's room (AutoMapLayout_WallSlot)
static void DecoratedFaces(const AutoMapCellRecord &wall, bool decorated[AUTOMAP_SIDE_COUNT])
{
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    decorated[side] = HasFeature(wall.wallSides[side]) || wall.wallSides[side].hidden;
  }
}

struct FaceGoods;
static void GoodsOfFaces(const AutoMapCellRecord &record, FaceGoods goods[AUTOMAP_SIDE_COUNT]);

// Every face that takes a slot: decorations, hidden machinery, and faces whose lock or alcove shows
// an icon though they carry no picture; one set per cell, so all its slots are sized together
static void SlotFaces(const AutoMapCellRecord &wall, bool decorated[AUTOMAP_SIDE_COUNT]);

static SDL_Rect SlotOf(const SDL_Rect &cell, const bool decorated[AUTOMAP_SIDE_COUNT], int side)
{
  AutoMapRect slot = AutoMapLayout_WallSlot(decorated, side, cell.w);
  SDL_Rect area = {cell.x + slot.x, cell.y + slot.y, slot.w, slot.h};
  return area;
}

// The line along the face, as long as the slot: bright for a switch the party knows about
static void DrawFaceLine(const SDL_Rect &cell, const SDL_Rect &slot, int side, const SDL_Color &colour)
{
  int thickness = AutoMapLayout_FaceLine(cell.w);
  SDL_Rect line = {slot.x, cell.y, slot.w, thickness};
  if (side == AutoMapSouth) line.y = cell.y + cell.h - thickness;
  if (side == AutoMapEast || side == AutoMapWest)
  {
    SDL_Rect upright = {side == AutoMapEast ? cell.x + cell.w - thickness : cell.x, slot.y, thickness, slot.h};
    line = upright;
  }
  Fill(line, colour);
}

// Machinery the 3D view hides: a dashed frame and a hollow diamond in the slot, and an orange
// face line
static void DrawHiddenSwitchMark(const SDL_Rect &cell, const SDL_Rect &slot, int side)
{
  DrawDashedRect(slot, WALL_SIDE_LINE);
  DrawHollowDiamond(slot, HIDDEN_SWITCH);
  DrawFaceLine(cell, slot, side, HIDDEN_SWITCH);
}

static void DrawTooltipIcon(const SDL_Rect &slot, const AutoMapItem &item);

// What a face holds and wants, in the corners of its slot: the first item in the alcove (lower
// right) and the key its lock takes (lower left, gold frame; empty while the key is unknown).
// Only faces the party knows the machinery of, as the tooltip; doors keep their dots.
struct FaceGoods
{
  bool stocked;
  AutoMapItem stock;
  bool locked;
  AutoMapItem key;
};

static void GoodsOfFaces(const AutoMapCellRecord &record, FaceGoods goods[AUTOMAP_SIDE_COUNT])
{
  bool isDoor = record.kind == AutoMapCellDoor;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    goods[side].stocked = false;
    goods[side].locked = false;
  }
  for (size_t i = 0; i < record.wallLocks.size(); i++)
  {
    const AutoMapWallLock &lock = record.wallLocks[i];
    if (!lock.required || lock.side < 0 || lock.side >= AUTOMAP_SIDE_COUNT) continue;
    if (!isDoor && !record.wallSides[lock.side].functional) continue;
    if (goods[lock.side].locked) continue;
    goods[lock.side].locked = true;
    goods[lock.side].key = lock.object;
  }
  for (size_t i = 0; i < record.alcoveContents.size(); i++)
  {
    const AutoMapAlcoveEntry &entry = record.alcoveContents[i];
    if (entry.item.worldPicture == AUTOMAP_NO_PICTURE) continue;
    if (entry.side < 0 || entry.side >= AUTOMAP_SIDE_COUNT) continue;
    if (!Shows(ShownEntryOfItemKind(AutoMapScene_KindOfItem(entry.item)))) continue;
    // A torch holder's picture shows its torch already; a second one in the corner doubles it
    if (!isDoor && record.wallSides[entry.side].isTorchHolder) continue;
    // Below all, `functional` is cleared for unknown switches, but goods in an alcove show
    if (!isDoor && !record.wallSides[entry.side].functional
        && (shownDrawList.spoilers == AutoMapSpoilersAll || !record.wallSides[entry.side].isAlcove))
      continue;
    if (goods[entry.side].stocked) continue;
    goods[entry.side].stocked = true;
    goods[entry.side].stock = entry.item;
  }
}

static void DrawSlotCorner(const SDL_Rect &slot, bool right, const AutoMapItem *item, bool goldFrame)
{
  AutoMapRect slotRect = {slot.x, slot.y, slot.w, slot.h};
  AutoMapRect corner = AutoMapLayout_SlotCorner(slotRect, right);
  SDL_Rect area = {corner.x, corner.y, corner.w, corner.h};
  Fill(area, BACKGROUND);
  if (goldFrame) Outline(area, LINK_BADGE);
  if (item != NULL && item->worldPicture != AUTOMAP_NO_PICTURE) DrawTooltipIcon(area, *item);
}

static void DrawWallSide(const SDL_Rect &cell, const AutoMapCellRecord &wall, int side,
                         const bool decorated[AUTOMAP_SIDE_COUNT], const FaceGoods &goods)
{
  const AutoMapWallSide &wallSide = wall.wallSides[side];
  if (!decorated[side]) return;
  SDL_Rect slot = SlotOf(cell, decorated, side);
  if (slot.w <= 0) return;
  bool drawsPicture = Shows(AutoMapShowWallDecorations)
      && (wallSide.picture != AUTOMAP_NO_PICTURE || wallSide.portrait != AUTOMAP_NO_PICTURE);
  bool drawsWriting = Shows(AutoMapShowWallWriting) && wallSide.hasWriting;
  if (wallSide.hidden)
  {
    DrawHiddenSwitchMark(cell, slot, side);
    if (drawsPicture && wallSide.portrait != AUTOMAP_NO_PICTURE) DrawPortrait(slot, wallSide.portrait);
    if (drawsWriting) DrawWriting(slot);
    return;
  }
  // Below All the fog filter clears `functional` for switches the party has not found out about:
  // those read like any decoration
  const SDL_Color &faceColour = wallSide.functional ? KNOWN_SWITCH_LINE : DECORATION_LINE;
  if (!drawsPicture && !drawsWriting)
  {
    // A known switch keeps its line with its picture hidden
    if (wallSide.functional) DrawFaceLine(cell, slot, side, KNOWN_SWITCH_LINE);
    return;
  }
  if (drawsPicture)
  {
    const AutoMapSprite *sprite = SpriteOfTheGame(wallSide.picture);
    bool pictureIsMissing = sprite == NULL && wallSide.picture != AUTOMAP_NO_PICTURE;
    if (sprite != NULL) DrawSprite(*sprite, FitInto(Inset(slot, 4), *sprite));
    if (pictureIsMissing) Fill(Inset(slot, 30), WALL_FEATURE_MARKER);
    if (wallSide.portrait != AUTOMAP_NO_PICTURE) DrawPortrait(slot, wallSide.portrait);
  }
  if (drawsWriting) DrawWriting(slot);
  DrawFaceLine(cell, slot, side, faceColour);
  if (goods.stocked) DrawSlotCorner(slot, true, &goods.stock, false);
  if (goods.locked) DrawSlotCorner(slot, false, &goods.key, true);
}

static void SlotFaces(const AutoMapCellRecord &wall, bool decorated[AUTOMAP_SIDE_COUNT])
{
  DecoratedFaces(wall, decorated);
  FaceGoods goods[AUTOMAP_SIDE_COUNT];
  GoodsOfFaces(wall, goods);
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    bool showsGoods = goods[side].locked || goods[side].stocked;
    if (showsGoods && (wall.wallSides[side].functional || wall.wallSides[side].isAlcove)) decorated[side] = true;
  }
}

static void DrawWallFaces(const SDL_Rect &cell, const AutoMapCellRecord &wall)
{
  bool withPicture[AUTOMAP_SIDE_COUNT];
  DecoratedFaces(wall, withPicture);
  bool decorated[AUTOMAP_SIDE_COUNT];
  SlotFaces(wall, decorated);
  FaceGoods goods[AUTOMAP_SIDE_COUNT];
  GoodsOfFaces(wall, goods);
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    if (withPicture[side])
    {
      DrawWallSide(cell, wall, side, decorated, goods[side]);
      continue;
    }
    // A lock or goods on a face without a picture still show, in that face's slot
    if (!decorated[side]) continue;
    SDL_Rect slot = SlotOf(cell, decorated, side);
    if (slot.w <= 0) continue;
    if (goods[side].stocked) DrawSlotCorner(slot, true, &goods[side].stock, false);
    if (goods[side].locked) DrawSlotCorner(slot, false, &goods[side].key, true);
  }
}

// Doors carry no faces to hang icons on: a gold ring by a side whose lock is known, an item-yellow
// dot by one holding goods. A dark rim keeps both readable on the door's brown.
static void DrawSideDot(int centreX, int centreY, int size, const SDL_Color &colour)
{
  SDL_Rect rim = {centreX - size / 2 - 1, centreY - size / 2 - 1, size + 2, size + 2};
  Fill(rim, BACKGROUND);
  SDL_Rect dot = {centreX - size / 2, centreY - size / 2, size, size};
  Fill(dot, colour);
}

static void DrawSideRing(int centreX, int centreY, int size, const SDL_Color &colour)
{
  DrawSideDot(centreX, centreY, size, colour);
  int frame = std::max(1, size / 4);
  if (size - 2 * frame < 1) return;
  SDL_Rect hole = {centreX - size / 2 + frame, centreY - size / 2 + frame, size - 2 * frame,
                   size - 2 * frame};
  Fill(hole, BACKGROUND);
}

static void DrawDoorMarkers(const SDL_Rect &cell, const AutoMapCellRecord &door)
{
  FaceGoods goods[AUTOMAP_SIDE_COUNT];
  GoodsOfFaces(door, goods);
  int dotSize = std::max(6, cell.w / 7);
  int lockOffset = dotSize / 2 + 1;
  int stockedOffset = lockOffset + dotSize + 2;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    if (!goods[side].locked && !goods[side].stocked) continue;
    int edgeX = cell.x + cell.w / 2, edgeY = cell.y + cell.h / 2, inwardX = 0, inwardY = 0;
    if (side == AutoMapNorth) { edgeY = cell.y; inwardY = 1; }
    else if (side == AutoMapSouth) { edgeY = cell.y + cell.h; inwardY = -1; }
    else if (side == AutoMapWest) { edgeX = cell.x; inwardX = 1; }
    else { edgeX = cell.x + cell.w; inwardX = -1; }
    if (goods[side].locked) DrawSideRing(edgeX + inwardX * lockOffset, edgeY + inwardY * lockOffset, dotSize, LINK_BADGE);
    if (goods[side].stocked) DrawSideDot(edgeX + inwardX * stockedOffset, edgeY + inwardY * stockedOffset, dotSize, ITEM_MARKER);
  }
}

static void DrawWall(const SDL_Rect &cell, const AutoMapCellRecord &wall)
{
  Fill(cell, WALL);
  Outline(cell, WALL_EDGE);
  DrawWallFaces(cell, wall);
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
  DrawWallFaces(cell, wall);
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
  AutoMapRect field = {frame.x, frame.y, frame.w, frame.h};
  AutoMapPaint_Dither(field, INVISIBLE_TELEPORTER, 56, std::max(2, cell.w / 14));
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

// A teleporter to its own cell is a spinner (AutoMapScene_SpinnerGeometry). One that forces an exit
// facing is a compass dial: a ring, its centre, and a head sitting on the rim at that facing. One
// that turns the party draws only the arc of the turn with a tangential head: a quarter arc for
// right (clockwise) and left, a half circle for around. A straight spin keeps the facing: a gapped
// ring with a dot and no head. Unknown rotations keep a full ring with an east head. Lines and heads
// have a dark rim; heads are large enough that their tip reads at any zoom.
static void SpinnerArc(float centreX, float centreY, float radius, float startAngle, float sweepAngle,
                       const SDL_Color &colour, int width)
{
  static const float PI = 3.14159265f;
  int segments = std::max(2, (int)ceilf(24.0f * fabsf(sweepAngle) / (2.0f * PI)));
  for (int pass = 0; pass < 2; pass++)
  {
    for (int i = 0; i < segments; i++)
    {
      float a0 = startAngle + sweepAngle * i / segments, a1 = startAngle + sweepAngle * (i + 1) / segments;
      float x0 = centreX + radius * cos(a0), y0 = centreY - radius * sin(a0);
      float x1 = centreX + radius * cos(a1), y1 = centreY - radius * sin(a1);
      SDL_Color dark = {0x0b, 0x0a, 0x08, 0xff};
      if (pass == 0) AutoMapPaint_Line(x0, y0, x1, y1, dark, width + 2, false, false);
      else AutoMapPaint_Line(x0, y0, x1, y1, colour, width, false, false);
    }
  }
}

static void SpinnerHead(float tipX, float tipY, float alongX, float alongY, float length, float half,
                        const SDL_Color &colour)
{
  float acrossX = -alongY, acrossY = alongX;
  float baseX = tipX - alongX * length, baseY = tipY - alongY * length;
  SDL_Color dark = {0x0b, 0x0a, 0x08, 0xff};
  float grow = 1.18f;
  AutoMapPaint_Triangle(tipX + alongX * 2, tipY + alongY * 2, baseX - alongX + acrossX * half * grow,
                        baseY - alongY + acrossY * half * grow, baseX - alongX - acrossX * half * grow,
                        baseY - alongY - acrossY * half * grow, dark);
  AutoMapPaint_Triangle(tipX, tipY, baseX + acrossX * half, baseY + acrossY * half, baseX - acrossX * half,
                        baseY - acrossY * half, colour);
}

static void DrawSpinnerMark(const SDL_Rect &cell, int facing, int rotation,
                            const SDL_Color &colour = SPINNER)
{
  static const float PI = 3.14159265f;
  float centreX = cell.x + cell.w / 2.0f;
  float centreY = cell.y + cell.h / 2.0f;
  float radius = cell.w * 0.24f;
  int width = std::max(2, cell.w / 18);
  AutoMapSpinnerGeometry geometry = AutoMapScene_SpinnerGeometry(facing, rotation);
  float headX = centreX + radius * cos(geometry.headAngle);
  float headY = centreY - radius * sin(geometry.headAngle);
  switch (geometry.glyph)
  {
  case AutoMapSpinnerAbsolute:
  {
    // A compass dial: the ring, its centre, and a head sitting on the rim at the facing it sets
    SpinnerArc(centreX, centreY, radius, 0.0f, 2.0f * PI, colour, width);
    int size = std::max(2, cell.w / 12);
    SDL_Rect rim = {(int)(centreX - size / 2.0f) - 1, (int)(centreY - size / 2.0f) - 1, size + 2, size + 2};
    SDL_Color dark = {0x0b, 0x0a, 0x08, 0xff};
    Fill(rim, dark);
    SDL_Rect dot = {rim.x + 1, rim.y + 1, size, size};
    Fill(dot, colour);
    float outX = cos(geometry.headAngle), outY = -sin(geometry.headAngle);
    SpinnerHead(headX + outX * radius * 0.6f, headY + outY * radius * 0.6f, outX, outY, radius * 0.85f,
                radius * 0.5f, colour);
    break;
  }
  case AutoMapSpinnerRight:
  case AutoMapSpinnerLeft:
  case AutoMapSpinnerAround:
  {
    SpinnerArc(centreX, centreY, radius, geometry.startAngle, geometry.sweepAngle, colour, width);
    float sign = geometry.sweepAngle < 0.0f ? -1.0f : 1.0f;
    float alongX = sign * -sin(geometry.headAngle), alongY = sign * -cos(geometry.headAngle);
    SpinnerHead(headX + alongX * radius * 0.75f, headY + alongY * radius * 0.75f, alongX, alongY,
                radius * 0.95f, radius * 0.45f, colour);
    break;
  }
  case AutoMapSpinnerStraight:
  {
    // A gapped ring reads as "no turn" at any zoom: 60 degrees open at north
    static const float GAP = PI / 3.0f;
    SpinnerArc(centreX, centreY, radius, PI / 2.0f + GAP / 2.0f, 2.0f * PI - GAP, colour, width);
    int size = std::max(2, cell.w / 12);
    SDL_Rect dot = {(int)(centreX - size / 2.0f), (int)(centreY - size / 2.0f), size, size};
    Fill(dot, colour);
    break;
  }
  case AutoMapSpinnerUnknown:
  default:
    SpinnerArc(centreX, centreY, radius, 0.0f, 2.0f * PI, colour, width);
    SpinnerHead(centreX + radius, centreY - radius * 0.75f, 0.0f, -1.0f, radius * 0.95f, radius * 0.45f, colour);
    break;
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

static const AutoMapShownCell *ShownCellAt(const AutoMapDrawList &drawList, int x, int y);

static void DrawGlimpses(const AutoMapView &view, const AutoMapDrawList &drawList)
{
  for (size_t i = 0; i < drawList.glimpsedItems.size(); i++)
  {
    SDL_Rect cell = CellArea(view, drawList.glimpsedItems[i].x, drawList.glimpsedItems[i].y);
    DrawGlimpseMark(std::max(1, cell.w / 9), cell);
  }
  // On a wall, a smaller "?" sits in the face's slot
  for (size_t i = 0; i < drawList.glimpsedFaces.size(); i++)
  {
    const AutoMapGlimpsedFace &face = drawList.glimpsedFaces[i];
    SDL_Rect cell = CellArea(view, face.x, face.y);
    if (face.side < 0 || face.side >= AUTOMAP_SIDE_COUNT) continue;
    // The same slot the face's decoration would take, beside the cell's other slots
    bool faces[AUTOMAP_SIDE_COUNT] = {false, false, false, false};
    const AutoMapShownCell *wall = ShownCellAt(drawList, face.x, face.y);
    if (wall != NULL) SlotFaces(wall->cell, faces);
    faces[face.side] = true;
    SDL_Rect area = SlotOf(cell, faces, face.side);
    if (area.w <= 0) continue;
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
  // More than three: the whole pile's count on a plate in the upper right
  int total = kinds != AUTOMAP_ALL_ITEM_KINDS ? (int)AutoMapScene_ItemsOfKinds(shown.allItemPictures, kinds).size()
                                              : (int)shown.allItemPictures.size();
  if (hasMoreItems && total > AUTOMAP_MOST_ITEMS_SHOWN)
  {
    int pixel = std::max(1, cell.w / 24);
    std::string label = std::to_string(total);
    int width = AutoMapPaint_TextWidth(label, pixel) + 4 * pixel;
    if (!AutoMapLayout_Fits(width + 2, 9 * pixel + 2, cell.w)) return;
    AutoMapRect plate = {cell.x + cell.w - width - 2, cell.y + 2, width, 9 * pixel};
    AutoMapPaint_Plate(plate, ITEM_MARKER);
    AutoMapPaint_Text(plate.x + 2 * pixel, plate.y + 2 * pixel, label, ITEM_MARKER, pixel);
  }
  else if (hasMoreItems) DrawMoreItemsMark(cell);
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


// The "who sets it off" badge, lower left: a dark plate with one square per kind of trigger. The
// party is its own red arrow (turned the required way on a facing pad), monsters are claw marks,
// an item is its own inventory icon, a box stands for objects. Too small cells draw none.
static void DrawBadge(const SDL_Rect &cell, const AutoMapBadge &badge, const AutoMapItem *item,
                      const SDL_Color &frame)
{
  int size = AutoMapLayout_BadgeSize(cell.w);
  if (size <= 0 || badge.glyphs.empty()) return;
  int textPixel = std::max(1, size / 9);
  int margin = std::max(2, cell.w * 6 / 100);
  // Glyphs that do not fit inside the cell drop out from the end, then the badge itself
  AutoMapBadge fitting = badge;
  int width = 0;
  while (!fitting.glyphs.empty())
  {
    width = 2;
    for (size_t i = 0; i < fitting.glyphs.size(); i++)
    {
      width += fitting.glyphs[i] == AutoMapGlyphGen ? AutoMapPaint_TextWidth("GEN", textPixel) + 4 : size;
    }
    if (AutoMapLayout_Fits(width + margin, size + 2 + margin, cell.w)) break;
    fitting.glyphs.pop_back();
  }
  if (fitting.glyphs.empty()) return;
  AutoMapRect plate = {cell.x + margin, cell.y + cell.h - size - 2 - margin, width, size + 2};
  AutoMapPaint_Plate(plate, frame);
  int x = plate.x + 1;
  int y = plate.y + 1;
  for (size_t i = 0; i < fitting.glyphs.size(); i++)
  {
    AutoMapRect box = {x, y, size, size};
    switch (fitting.glyphs[i])
    {
    case AutoMapGlyphParty:
      AutoMapPaint_PartyArrow(x + size / 2.0f, y + size / 2.0f, size * 0.36f, badge.facing, PARTY, false);
      break;
    case AutoMapGlyphClaw: AutoMapPaint_Claw(box, BADGE_GLYPH); break;
    case AutoMapGlyphBox:
    {
      AutoMapRect square = {x + size * 22 / 100, y + size * 22 / 100, size * 56 / 100, size * 56 / 100};
      AutoMapPaint_Frame(square, BADGE_GLYPH, std::max(1, size / 10));
      break;
    }
    case AutoMapGlyphItem:
    {
      SDL_Rect area = {x, y, size, size};
      if (item == NULL || item->worldPicture == AUTOMAP_NO_PICTURE) DrawKeyholeMark(Inset(area, 10));
      else DrawTooltipIcon(area, *item);
      break;
    }
    case AutoMapGlyphGen:
    {
      int w = AutoMapPaint_TextWidth("GEN", textPixel);
      AutoMapPaint_Text(x + 2, y + (size - 5 * textPixel) / 2, "GEN", GEN_TEXT, textPixel);
      x += w + 4;
      continue;
    }
    }
    x += size;
  }
}

// Who a firing teleporter takes, in the pads' badge. Dormant teleporters, spinners and plain floors
// carry no scope and stay unmarked.
static void DrawTeleporterScopeMark(const SDL_Rect &cell, const AutoMapCellRecord &record)
{
  DrawBadge(cell, AutoMapLayout_TeleporterBadge(record.teleporterScope), NULL, TELEPORTER_BADGE_FRAME);
}

// The game's own floor picture is the pad. A pad that has none (a hidden one) gets the game's
// pressure plate drawn faint with a dotted rim; a dormant pad keeps its dashed plate.
static void DrawPadMark(const SDL_Rect &cell, const AutoMapFloorActuator &pad, bool hasFloorPicture = false)
{
  SDL_Rect plate = Inset(cell, 22);
  if (pad.kind == AutoMapActuatorDormantPad)
  {
    DrawDashedRect(plate, PAD_DORMANT);
    return;
  }
  if (!hasFloorPicture)
  {
    const AutoMapSprite *sprite = SpriteOfTheGame(PRESSURE_PLATE_PICTURE);
    if (sprite != NULL)
    {
      SDL_SetTextureAlphaMod(sprite->texture, 90);
      DrawSprite(*sprite, FitInto(Inset(cell, 8), *sprite));
      SDL_SetTextureAlphaMod(sprite->texture, 255);
    }
    DrawDashedRect(Inset(cell, 14), PAD);
  }
  AutoMapBadge badge = AutoMapLayout_PadBadge(pad.op, pad.requiredFacing);
  DrawBadge(cell, badge, &pad.object, badge.gold ? FACING_BADGE_FRAME : BADGE_FRAME);
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
  if (pad.hasGeneratedMonster)
  {
    AutoMapSpriteRequest request = {pad.generatedKind.frontPicture, pad.generatedKind.transparentColour};
    const AutoMapSprite *sprite = sprites.Find(request);
    if (sprite != NULL)
    {
      SDL_SetTextureAlphaMod(sprite->texture, 115);
      DrawSprite(*sprite, FitInto(Inset(cell, 8), *sprite));
      SDL_SetTextureAlphaMod(sprite->texture, 255);
    }
    else DrawGeneratorMark(cell, false);
  }
  else DrawGeneratorMark(cell, false);
  AutoMapBadge badge;
  badge.glyphs.push_back(AutoMapGlyphGen);
  badge.facing = 0;
  badge.gold = false;
  DrawBadge(cell, badge, NULL, GENERATOR);
}

// A sleeping generator: what it makes, faint, inside the spent generator's empty diamond
static void DrawSleepingGenerator(const SDL_Rect &cell, const AutoMapFloorActuator &pad)
{
  if (pad.hasGeneratedMonster)
  {
    AutoMapSpriteRequest request = {pad.generatedKind.frontPicture, pad.generatedKind.transparentColour};
    const AutoMapSprite *sprite = sprites.Find(request);
    if (sprite != NULL)
    {
      SDL_SetTextureAlphaMod(sprite->texture, 50);
      DrawSprite(*sprite, FitInto(Inset(cell, 8), *sprite));
      SDL_SetTextureAlphaMod(sprite->texture, 255);
    }
  }
  DrawGeneratorMark(cell, true);
}

static void DrawFloorActuators(const SDL_Rect &cell, const AutoMapShownCell &shown,
                               const AutoMapDrawList &drawList)
{
  // A hidden pad's plate is see-through: drawn once per cell, so that a cell of five pads does
  // not look brighter than one of a single pad
  bool plateDrawn = shown.cell.floorPicture != AUTOMAP_NO_PICTURE;
  for (size_t i = 0; i < drawList.actuators.size(); i++)
  {
    if (drawList.actuators[i].x != shown.x || drawList.actuators[i].y != shown.y) continue;
    switch (drawList.actuators[i].kind)
    {
    case AutoMapActuatorPad:
    case AutoMapActuatorDormantPad:
      DrawPadMark(cell, drawList.actuators[i], plateDrawn);
      if (drawList.actuators[i].kind == AutoMapActuatorPad) plateDrawn = true;
      break;
    case AutoMapActuatorGenerator: DrawGeneratedMonster(cell, drawList.actuators[i]); break;
    case AutoMapActuatorSpentGenerator: DrawGeneratorMark(cell, true); break;
    case AutoMapActuatorSleepingGenerator: DrawSleepingGenerator(cell, drawList.actuators[i]); break;
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
  // Upper right, clear of the wall slots and the who-badge in the lower left; a dark rim keeps it
  // apart from the stone
  int size = std::max(4, cell.w / 7);
  SDL_Rect dot = {cell.x + cell.w - size - 3, cell.y + 3, size, size};
  SDL_Rect rim = {dot.x - 1, dot.y - 1, size + 2, size + 2};
  Fill(rim, BACKGROUND);
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
  // The stripes are the background; the game's picture still stands on them
  // A concealed pit, open or closed, reads as floor in the 3D view: no stripes on it
  if (record.kind == AutoMapCellPit && !record.pitIsHidden && PitIsStriped(cell.w)) DrawPitStripes(cell);
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
  if (shown.isWalked) Fill(cell, WALKED_HIGHLIGHT);
  else if (shown.isHighlighted) Fill(cell, EXPLORED_HIGHLIGHT);
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
  if (record.kind == AutoMapCellDoor) DrawDoorMarkers(cell, record);
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
  // A replay's breakpoint: a red frame two pixels wide just inside the cell
  if (AutoMap_HasReplayBreakpoint(drawList.level, shown.x - drawList.boundaryMarginX, shown.y - drawList.boundaryMarginY))
  {
    SDL_Rect outer = {cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2};
    SDL_Rect inner = {cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4};
    Outline(outer, BREAKPOINT);
    Outline(inner, BREAKPOINT);
  }
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

// A facing mark is drawn in a square at least this wide, so a mark in a quarter of a small cell
// still reads as a triangle; it never grows past the cell
static const int FACING_MARK_SMALLEST_SIDE = 14;
// The dark rim lies outside the white triangle, this many pixels wide
static const float FACING_MARK_RIM = 1.0f;

// The triangle grown away from its middle so that each side moves out by the given distance
static void GrowTriangle(FloatPoint corners[3], float distance)
{
  FloatPoint middle = {(corners[0].x + corners[1].x + corners[2].x) / 3,
                       (corners[0].y + corners[1].y + corners[2].y) / 3};
  float twiceArea = std::fabs((corners[1].x - corners[0].x) * (corners[2].y - corners[0].y) -
                              (corners[2].x - corners[0].x) * (corners[1].y - corners[0].y));
  float perimeter = 0;
  for (int i = 0; i < 3; i++)
  {
    const FloatPoint &next = corners[(i + 1) % 3];
    perimeter += std::sqrt((next.x - corners[i].x) * (next.x - corners[i].x) +
                           (next.y - corners[i].y) * (next.y - corners[i].y));
  }
  if (twiceArea <= 0 || perimeter <= 0) return;
  // The inscribed circle's radius is twice the area over the perimeter; the sides move out
  // by the distance when the triangle is scaled about its middle by (radius + distance) / radius
  float radius = twiceArea / perimeter;
  float scale = (radius + distance) / radius;
  for (int i = 0; i < 3; i++)
  {
    corners[i].x = middle.x + (corners[i].x - middle.x) * scale;
    corners[i].y = middle.y + (corners[i].y - middle.y) * scale;
  }
}

// The square the mark is drawn in: flush with the side of the area the member faces and centred
// along it. A half tile is twice as long as it is wide, so the square takes the shorter side; in a
// small area it takes the smallest side instead, reaching into the rest of the cell but not out of it
static SDL_Rect FacingMarkSquare(const SDL_Rect &cell, const SDL_Rect &area, AutoMapDirection facing)
{
  int side = std::max(std::min(area.w, area.h), FACING_MARK_SMALLEST_SIDE);
  side = std::min(side, std::min(cell.w, cell.h));
  SDL_Rect square = {area.x + (area.w - side) / 2, area.y + (area.h - side) / 2, side, side};
  if (facing == AutoMapNorth) square.y = area.y;
  if (facing == AutoMapEast) square.x = area.x + area.w - side;
  if (facing == AutoMapSouth) square.y = area.y + area.h - side;
  if (facing == AutoMapWest) square.x = area.x;
  square.x = std::max(cell.x, std::min(square.x, cell.x + cell.w - side));
  square.y = std::max(cell.y, std::min(square.y, cell.y + cell.h - side));
  return square;
}

// The mark keeps the proportions it has in a whole tile. It lies on the figure's own body, so a
// dark rim around it, kept inside the cell, sets it apart from a pale figure (a mummy, a skeleton)
static void DrawFacingMark(const SDL_Rect &cell, const SDL_Rect &area, AutoMapDirection facing)
{
  static const FloatPoint TIP = {1.00f, 0.50f};
  static const FloatPoint UPPER_CORNER = {0.82f, 0.36f};
  static const FloatPoint LOWER_CORNER = {0.82f, 0.64f};
  SDL_Rect square = FacingMarkSquare(cell, area, facing);
  FloatPoint mark[3] = {TurnedPointInCell(square, TIP, facing), TurnedPointInCell(square, UPPER_CORNER, facing),
                        TurnedPointInCell(square, LOWER_CORNER, facing)};
  FloatPoint rim[3] = {mark[0], mark[1], mark[2]};
  GrowTriangle(rim, FACING_MARK_RIM);
  SDL_Rect oldClip;
  bool wasClipped = SDL_RenderIsClipEnabled(mapRenderer) == SDL_TRUE;
  SDL_RenderGetClipRect(mapRenderer, &oldClip);
  SDL_Rect clip = cell;
  if (wasClipped && !SDL_IntersectRect(&cell, &oldClip, &clip)) return;
  SDL_RenderSetClipRect(mapRenderer, &clip);
  FillTriangle(rim[0], rim[1], rim[2], FACING_MARK_EDGE);
  FillTriangle(mark[0], mark[1], mark[2], WHITE);
  SDL_RenderSetClipRect(mapRenderer, wasClipped ? &oldClip : NULL);
}

// The part of its tile in which a member stands, on screen
static SDL_Rect AreaOfMember(const SDL_Rect &cell, const AutoMapShownMonster &monster, int member)
{
  AutoMapRect area =
      AutoMapLayout_MemberArea(monster.positions, member, monster.size, (int)monster.memberFacings[0], cell.w);
  SDL_Rect onScreen = {cell.x + area.x, cell.y + area.y, area.w, area.h};
  return onScreen;
}

static void DrawFacingMarks(const AutoMapView &view, const AutoMapShownMonster &monster)
{
  SDL_Rect cell = CellArea(view, monster.x, monster.y);
  for (int member = 0; member < monster.memberCount; member++)
  {
    DrawFacingMark(cell, AreaOfMember(cell, monster, member), monster.memberFacings[member]);
  }
}

// Every figure is drawn at the same zoom, alone or in a group of four: the pictures already
// differ in size the way the 3D view shows them, so a golem stays bigger than a screamer
// without a lone unit being blown up to fill its cell
static const float FIGURE_SCALE = 0.5f;

struct FigurePlace
{
  int centreX;
  int feetY;
  float scale;
};

// Each member stands where the game stands it: in its corner of the tile (small), in its half
// (medium) or in the middle (large, or a group the game keeps centred). The figure keeps its zoom
// and may reach out of its area, into the tile above for a tall one
static FigurePlace PlaceOfFigure(const SDL_Rect &cell, const AutoMapShownMonster &monster, int member)
{
  AutoMapRect area =
      AutoMapLayout_MemberArea(monster.positions, member, monster.size, (int)monster.memberFacings[0], cell.w);
  AutoMapPointF feet = AutoMapLayout_FigureFeet(area, cell.w);
  FigurePlace place;
  place.centreX = cell.x + (int)feet.x;
  place.feetY = cell.y + (int)feet.y;
  place.scale = FIGURE_SCALE * cell.w / MONSTER_PIXELS_PER_CELL;
  return place;
}

static void DrawFigureMarker(const FigurePlace &place)
{
  int size = (int)(place.scale * MONSTER_PIXELS_PER_CELL / 2);
  SDL_Rect marker = {place.centreX - size / 2, place.feetY - size, size, size};
  Fill(marker, MONSTER_MARKER);
}

// A tall figure can reach into the cell above
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
  FigurePlace places[AUTOMAP_LARGEST_GROUP];
  int backToFront[AUTOMAP_LARGEST_GROUP];
  for (int member = 0; member < monster.memberCount; member++)
  {
    places[member] = PlaceOfFigure(cell, monster, member);
    backToFront[member] = member;
  }
  // The back row first, so the front row stands in front of it; a row keeps the group's order
  for (int i = 1; i < monster.memberCount; i++)
  {
    for (int j = i; j > 0 && places[backToFront[j]].feetY < places[backToFront[j - 1]].feetY; j--)
    {
      std::swap(backToFront[j], backToFront[j - 1]);
    }
  }
  for (int i = 0; i < monster.memberCount; i++)
  {
    int member = backToFront[i];
    const FigurePlace &place = places[member];
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
  for (size_t i = 0; i < fromNorthToSouth.size(); i++) DrawMonster(view, fromNorthToSouth[i]);
  // Over the figures, so a tall one does not hide the mark at the north edge of its cell
  for (size_t i = 0; i < fromNorthToSouth.size(); i++) DrawFacingMarks(view, fromNorthToSouth[i]);
}

static AutoMapViewRequest RequestOfShownLevel(void)
{
  AutoMapViewRequest request;
  request.levelSize.width = shownDrawList.width;
  request.levelSize.height = shownDrawList.height;
  MapAreaSize(&request.windowWidth, &request.windowHeight);
  request.zoomSteps = zoomSteps;
  request.placeFreely = zoomPlacesFreely && !viewFollowsParty;
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
  return request;
}

static AutoMapView ViewOfShownLevel(void)
{
  if (renderCellSize > 0)
  {
    int mapWidth = 0, mapHeight = 0;
    MapAreaSize(&mapWidth, &mapHeight);
    AutoMapView fixed = {renderCellSize, RULER + mapWidth / 2 - (hoverX * renderCellSize + renderCellSize / 2),
                         RULER + mapHeight / 2 - (hoverY * renderCellSize + renderCellSize / 2)};
    return fixed;
  }
  AutoMapView view = AutoMapScene_PlaceLevel(RequestOfShownLevel());
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
  // The open results panel covers the map: nothing under it is hovered, but the wheel still
  // needs to know the pointer is over the panel
  bool overPanel = searchOpen && PointInRect(pixelX, pixelY, SearchPanelRect());
  if (overPanel || !CellAtPixel(pixelX, pixelY, &x, &y))
  {
    ClearHover();
    hoverPixelX = pixelX;
    hoverPixelY = pixelY;
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
    tooltipGridHiddenRows = 0;
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








// While hovering, the map dims everywhere but the cells the lines run between (the spotlight).
// DrawDynamicLayer runs the hover drawing twice: first only to collect those cells and lines,
// then, after the dimming, to draw.
static bool collectingSpotlight = false;
static std::vector<AutoMapCell> spotlightCells;
static std::vector<AutoMapLink> spotlightLines;
// The goals among them, for the tooltip's place (dst is the goal's cell), and the height of
// their badge, a tab on the frame's top edge
static std::vector<AutoMapLink> spotlightGoals;
static const int GOAL_TAB_HEIGHT = 5 * DIGIT_SCALE + 6;

static void MarkSpotlightCell(int x, int y)
{
  if (!collectingSpotlight) return;
  AutoMapCell cell = {shownDrawList.level, x, y};
  spotlightCells.push_back(cell);
}

static SDL_Color Sdl(const SDL_Color &colour)
{
  return colour;
}

// One link as a pixel line with a dark rim from the switch's face (or the pad's centre) to where
// it enters its target, with a head that tells the signal at All: SET filled, CLEAR hollow,
// TOGGLE a rhomb, HOLD a crossbar, once-only dashed. A learned link and the player's own claim
// no signal: an open head. isPlayerLink: the player's own link. lane: pixels off the axis, to
// the right of the line's direction, when it shares a bundle (AutoMapLayout_BundleLanes).
static void DrawLinkEdge(const AutoMapView &view, const AutoMapLink &edge,
                         const SDL_Color &colour = LINK_GOLD, bool isPlayerLink = false,
                         float lane = 0.0f)
{
  if (collectingSpotlight)
  {
    MarkSpotlightCell(edge.srcX, edge.srcY);
    MarkSpotlightCell(edge.dstX, edge.dstY);
    spotlightLines.push_back(edge);
    return;
  }
  int width = AutoMapLayout_LineWidth(view.cellSize);
  float headSize = (float)AutoMapLayout_HeadSize(view.cellSize);
  FloatPoint from = LinkEndpoint(view, edge.srcX, edge.srcY, edge.srcIsWall, edge.srcSide);
  if (edge.srcX == edge.dstX && edge.srcY == edge.dstY && !edge.srcIsWall)
  {
    // Self-link: a square ring, dashed when one-shot
    SDL_Rect cell = CellArea(view, edge.dstX, edge.dstY);
    if (edge.onceOnly) DrawDashedRect(Inset(cell, 25), colour);
    else
    {
      AutoMapRect ring = {cell.x + cell.w / 4, cell.y + cell.h / 4, cell.w / 2, cell.h / 2};
      AutoMapPaint_Frame(ring, colour, 2);
    }
    return;
  }
  AutoMapPointF local = {from.x - view.originX, from.y - view.originY};
  AutoMapPointF end = AutoMapLayout_LineEnd(local, edge.dstX, edge.dstY, view.cellSize);
  float tipX = end.x + view.originX, tipY = end.y + view.originY;
  float angle = std::atan2(tipY - from.y, tipX - from.x);
  if (lane != 0.0f)
  {
    float acrossX = -std::sin(angle) * lane, acrossY = std::cos(angle) * lane;
    from.x += acrossX;
    from.y += acrossY;
    tipX += acrossX;
    tipY += acrossY;
  }
  float shaftX = tipX - std::cos(angle) * headSize * 0.8f, shaftY = tipY - std::sin(angle) * headSize * 0.8f;
  AutoMapPaint_Line(from.x, from.y, shaftX, shaftY, Sdl(colour), width, edge.onceOnly);
  if (isPlayerLink || shownDrawList.spoilers != AutoMapSpoilersAll)
  {
    AutoMapPaint_Head(tipX, tipY, angle, headSize, Sdl(colour), AutoMapHeadOpen);
    return;
  }
  AutoMapHead head = edge.signal == AutoMapSignalClear    ? AutoMapHeadHollow
                     : edge.signal == AutoMapSignalToggle ? AutoMapHeadRhomb
                                                          : AutoMapHeadFilled;
  AutoMapPaint_Head(tipX, tipY, angle, headSize, Sdl(colour), head);
  if (edge.signal == AutoMapSignalPressure)
  {
    float midX = (from.x + tipX) / 2, midY = (from.y + tipY) / 2;
    float across = std::max(4.0f, view.cellSize * 0.14f);
    AutoMapPaint_Line(midX - std::sin(angle) * across, midY + std::cos(angle) * across,
                      midX + std::sin(angle) * across, midY - std::cos(angle) * across, Sdl(colour), width, false);
  }
}

// Where the lines of a source start: a gold square with a dark rim
static void DrawSourceNode(const AutoMapView &view, const AutoMapLink &edge, const SDL_Color &colour)
{
  if (collectingSpotlight) return;
  int width = AutoMapLayout_LineWidth(view.cellSize);
  FloatPoint at = LinkEndpoint(view, edge.srcX, edge.srcY, edge.srcIsWall, edge.srcSide);
  AutoMapRect rim = {(int)at.x - width * 3 / 2 - 1, (int)at.y - width * 3 / 2 - 1, width * 3 + 2, width * 3 + 2};
  AutoMapRect node = {rim.x + 1, rim.y + 1, width * 3, width * 3};
  SDL_Color dark = {0x0b, 0x0a, 0x08, 0xff};
  AutoMapPaint_Fill(rim, dark);
  AutoMapPaint_Fill(node, colour);
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

static const AutoMapShownCell *StairsShownAt(int cellX, int cellY)
{
  if (shownDrawList.level == AUTOMAP_NO_LEVEL) return NULL;
  const AutoMapShownCell *shown = ShownCellAt(shownDrawList, cellX, cellY);
  if (shown == NULL || shown->cell.kind != AutoMapCellStairs) return NULL;
  if (shown->cell.stairsTargetLevel == AUTOMAP_NO_LEVEL) return NULL;
  return shown;
}

// The ghost of what hovering this source would do to a door target: its leaves redrawn
// where the signal would leave them. Nothing draws for non-doors (their response is not
// statically knowable), for destroyed doors (signals are no-ops), or when the signal
// changes nothing (the arrow alone says "linked but no change").
static const SDL_Color LINK_GHOST = {0xff, 0xc8, 0x3a, 140};

static void DrawDoorPreview(const AutoMapView &view, const AutoMapLink &edge)
{
  if (collectingSpotlight) return;
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

// Lines between the same two cells draw once: SET plus a delayed SET is one line, the tooltip
// lists both. Every source gets its node.
static bool SameCells(const AutoMapLink &a, const AutoMapLink &b)
{
  return a.srcX == b.srcX && a.srcY == b.srcY && a.dstX == b.dstX && a.dstY == b.dstY
      && a.srcIsWall == b.srcIsWall && a.srcSide == b.srcSide;
}

// edges with their lanes (pixels, DrawLinkEdge), parallel to edges or empty: every distinct line
// once, then every distinct source its square at the cell's centre
static void DrawEdgeGroups(const AutoMapView &view, const std::vector<AutoMapLink> &edges,
                           const SDL_Color &colour, bool isPlayerLink = false,
                           const std::vector<float> &lanes = std::vector<float>())
{
  for (size_t i = 0; i < edges.size(); i++)
  {
    bool drawn = false;
    for (size_t j = 0; j < i && !drawn; j++) drawn = SameCells(edges[i], edges[j]);
    if (!drawn) DrawLinkEdge(view, edges[i], colour, isPlayerLink, i < lanes.size() ? lanes[i] : 0.0f);
  }
  for (size_t i = 0; i < edges.size(); i++)
  {
    bool sameSource = false;
    for (size_t j = 0; j < i && !sameSource; j++)
    {
      sameSource = edges[i].srcX == edges[j].srcX && edges[i].srcY == edges[j].srcY
          && edges[i].srcIsWall == edges[j].srcIsWall && edges[i].srcSide == edges[j].srcSide;
    }
    if (!sameSource) DrawSourceNode(view, edges[i], colour);
  }
}

// What the hovered cell sends in gold and what reaches it in blue. A line already gold is not
// drawn again in blue (a relay that feeds itself is in both lists). Lines that share a stretch
// of one straight line spread across lanes (AutoMapLayout_BundleLanes), a line width apart.
static void DrawGoldAndBlue(const AutoMapView &view, const std::vector<AutoMapLink> &gold,
                            const std::vector<AutoMapLink> &blue)
{
  std::vector<AutoMapLink> feeders;
  for (size_t i = 0; i < blue.size(); i++)
  {
    bool shown = false;
    for (size_t g = 0; g < gold.size() && !shown; g++) shown = SameCells(blue[i], gold[g]);
    if (!shown) feeders.push_back(blue[i]);
  }
  // The bundles over the distinct lines; repeats take the lane of their first
  std::vector<AutoMapLink> all;
  std::vector<bool> isFeed;
  std::vector<size_t> slot(gold.size() + feeders.size());
  for (size_t i = 0; i < gold.size() + feeders.size(); i++)
  {
    bool feed = i >= gold.size();
    const AutoMapLink &line = feed ? feeders[i - gold.size()] : gold[i];
    size_t k = 0;
    while (k < all.size() && !(SameCells(all[k], line) && isFeed[k] == feed)) k++;
    if (k == all.size())
    {
      all.push_back(line);
      isFeed.push_back(feed);
    }
    slot[i] = k;
  }
  std::vector<float> units = AutoMapLayout_BundleLanes(all, isFeed);
  float width = (float)AutoMapLayout_LineWidth(view.cellSize);
  std::vector<float> goldLanes(gold.size()), blueLanes(feeders.size());
  for (size_t i = 0; i < gold.size(); i++) goldLanes[i] = units[slot[i]] * width;
  for (size_t i = 0; i < feeders.size(); i++) blueLanes[i] = units[slot[gold.size() + i]] * width;
  DrawEdgeGroups(view, feeders, LINK_INCOMING, false, blueLanes);
  DrawEdgeGroups(view, gold, LINK_GOLD, false, goldLanes);
}

// The player's links of the hovered cell: the ones it starts, else the ones that end on it
static std::vector<AutoMapLink> PlayerLinksAtHover(void)
{
  std::vector<AutoMapLink> outgoing;
  std::vector<AutoMapLink> incoming;
  for (size_t i = 0; i < shownDrawList.playerLinks.size(); i++)
  {
    const AutoMapLink &link = shownDrawList.playerLinks[i];
    if (link.srcX == hoverX && link.srcY == hoverY) outgoing.push_back(link);
    else if (link.dstX == hoverX && link.dstY == hoverY) incoming.push_back(link);
  }
  return !outgoing.empty() ? outgoing : incoming;
}

// A goal's frame and badge. Gold for what happens now or is done; the warning colour for what
// still waits on something, so a blocked door never reads like an open one. The badge is a tab
// on the frame's top edge, so the cell's centre stays free for a source square.
static void DrawGoalBadge(const AutoMapView &view, const AutoMapEffect &goal)
{
  std::string badge = AutoMapScene_GoalBadge(goal.distance);
  if (badge.empty()) return;
  if (collectingSpotlight)
  {
    MarkSpotlightCell(goal.x, goal.y);
    spotlightGoals.push_back(AutoMapLink(goal.x, goal.y, goal.x, goal.y));
    return;
  }
  AutoMapGoalStatus status = goal.distance.status;
  bool good = status == AutoMapGoalOnThisPress || status == AutoMapGoalMore
           || status == AutoMapGoalAlreadyDone;
  const SDL_Color &colour = good ? LINK_GOLD : LINK_WARNING;
  SDL_Rect cell = CellArea(view, goal.x, goal.y);
  AutoMapRect ring = {cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4};
  int frameLine = AutoMapLayout_FaceLine(cell.w);
  AutoMapPaint_Frame(ring, colour, frameLine);
  int width = AutoMapPaint_TextWidth(badge, DIGIT_SCALE);
  AutoMapRect plate = {cell.x + (cell.w - width) / 2 - 3, ring.y + frameLine / 2 - GOAL_TAB_HEIGHT / 2,
                       width + 6, GOAL_TAB_HEIGHT};
  AutoMapPaint_Plate(plate, colour);
  AutoMapPaint_Text(plate.x + 3, plate.y + 3, badge, colour, DIGIT_SCALE);
}

static void DrawTeleporterTarget(const AutoMapView &view);

// At All a press lights its whole chain (every link down to the goals, a badge on each goal), and
// a relay shows its feeders in blue beside what it feeds. False when the hovered cell has neither,
// so the plain links run as they always did.
static bool DrawChainArrows(const AutoMapView &view, const std::vector<AutoMapLink> &playerEdges)
{
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(shownDrawList, hoverX, hoverY);
  bool relayCell = !AutoMapScene_RelayBlock(shownDrawList, hoverX, hoverY, false).empty();
  if (sources.empty() && !relayCell) return false;
  std::vector<AutoMapLink> chain = AutoMapScene_PressChainLinks(sources);
  std::vector<AutoMapLink> direct = AutoMapScene_LinksFrom(shownDrawList, hoverX, hoverY);
  if (chain.empty()) chain = direct;
  // What reaches the cell, in blue beside what it sends: for a relay always, for a pad or
  // switch that is itself a target (a pad on a pit) as well
  std::vector<AutoMapLink> feeders;
  if (relayCell || !chain.empty()) feeders = AutoMapScene_LinksTo(shownDrawList, hoverX, hoverY);
  if (chain.empty() && feeders.empty()) return false;
  DrawGoldAndBlue(view, chain, feeders);
  DrawEdgeGroups(view, playerEdges, PLAYER_NOTE, true);
  // Door previews stay forward-only, and for the direct targets
  for (size_t i = 0; i < direct.size(); i++) DrawDoorPreview(view, direct[i]);
  std::vector<const AutoMapEffect *> goals = AutoMapScene_PressGoalsByCell(sources);
  for (size_t i = 0; i < goals.size(); i++) DrawGoalBadge(view, *goals[i]);
  DrawTeleporterTarget(view);
  return true;
}

static void DrawLinkArrows(const AutoMapView &view)
{
  if (!hoverValid) return;
  // The player's links draw beside the learned ones at every spoiler level, All included
  std::vector<AutoMapLink> playerEdges = PlayerLinksAtHover();
  if (shownDrawList.spoilers == AutoMapSpoilersAll && DrawChainArrows(view, playerEdges)) return;
  std::vector<AutoMapLink> outgoing = AutoMapScene_LinksFrom(shownDrawList, hoverX, hoverY);
  std::vector<AutoMapLink> incoming = AutoMapScene_LinksTo(shownDrawList, hoverX, hoverY);
  std::vector<AutoMapLink> edges = !outgoing.empty() ? outgoing : incoming;
  // A cell that sends and receives shows both: what reaches it in blue, beside its gold
  std::vector<AutoMapLink> feeders;
  if (!outgoing.empty()) feeders = incoming;
  DrawGoldAndBlue(view, edges, feeders);
  DrawEdgeGroups(view, playerEdges, PLAYER_NOTE, true);
  // Reverse hover shows every source but predicts nothing: with possibly conflicting signals the
  // effect is ambiguous by definition. A learned link has default fields: no preview below All.
  if (!outgoing.empty() && shownDrawList.spoilers == AutoMapSpoilersAll)
  {
    for (size_t i = 0; i < edges.size(); i++) DrawDoorPreview(view, edges[i]);
  }
  DrawTeleporterTarget(view);
}

// The spotlight: every shown cell not in the collected set dims
static void DrawSpotlight(const AutoMapView &view)
{
  if (spotlightCells.empty()) return;
  AutoMapCell hovered = {shownDrawList.level, hoverX, hoverY};
  spotlightCells.push_back(hovered);
  std::vector<SDL_Rect> dim;
  for (size_t i = 0; i < shownDrawList.cells.size(); i++)
  {
    const AutoMapShownCell &shown = shownDrawList.cells[i];
    bool lit = false;
    for (size_t k = 0; k < spotlightCells.size() && !lit; k++)
    {
      lit = spotlightCells[k].x == shown.x && spotlightCells[k].y == shown.y;
    }
    if (!lit) dim.push_back(CellArea(view, shown.x, shown.y));
  }
  SDL_SetRenderDrawColor(mapRenderer, 8, 7, 5, 158);
  if (!dim.empty()) SDL_RenderFillRects(mapRenderer, dim.data(), (int)dim.size());
}

// A firing teleporter points at its destination: a cyan arrow when the victim lands on
// the shown level, the destination level's number (as in the title bar) when it lands
// elsewhere. Spinners need none of this: their circular arrow is the self-target.
// A teleport as a line: cyan from a firing teleporter to where it lands; dashed and dim from a
// dormant one to where it would; the destination framed the same way
static void DrawTeleportLine(const AutoMapView &view, const AutoMapLink &edge, bool dormant)
{
  if (collectingSpotlight)
  {
    MarkSpotlightCell(edge.srcX, edge.srcY);
    MarkSpotlightCell(edge.dstX, edge.dstY);
    spotlightLines.push_back(edge);
    return;
  }
  const SDL_Color &colour = dormant ? DORMANT_TELEPORT_LINE : SPINNER;
  int width = AutoMapLayout_LineWidth(view.cellSize);
  float headSize = (float)AutoMapLayout_HeadSize(view.cellSize);
  SDL_Rect src = CellArea(view, edge.srcX, edge.srcY);
  SDL_Rect dst = CellArea(view, edge.dstX, edge.dstY);
  AutoMapPointF from = {src.x + src.w / 2.0f - view.originX, src.y + src.h / 2.0f - view.originY};
  AutoMapPointF end = AutoMapLayout_LineEnd(from, edge.dstX, edge.dstY, view.cellSize);
  float fromX = from.x + view.originX, fromY = from.y + view.originY;
  float tipX = end.x + view.originX, tipY = end.y + view.originY;
  float angle = std::atan2(tipY - fromY, tipX - fromX);
  AutoMapPaint_Line(fromX, fromY, tipX - std::cos(angle) * headSize * 0.8f, tipY - std::sin(angle) * headSize * 0.8f,
                    colour, width, dormant);
  AutoMapPaint_Head(tipX, tipY, angle, headSize, colour, dormant ? AutoMapHeadHollow : AutoMapHeadFilled);
  if (dormant) DrawDashedRect(Inset(dst, 4), colour);
  else
  {
    AutoMapRect frame = {dst.x + 2, dst.y + 2, dst.w - 4, dst.h - 4};
    AutoMapPaint_Frame(frame, colour, AutoMapLayout_FaceLine(dst.w));
  }
}

// Hovering a teleporter shows where it sends you on this level; hovering a cell teleporters land
// on shows where you arrive from; a dormant one shows where it would send you. Another level's
// destination is named by the plate on the cell and the tooltip.
static void DrawTeleporterTarget(const AutoMapView &view)
{
  const AutoMapShownCell *shown = ShownCellAt(shownDrawList, hoverX, hoverY);
  if (shown == NULL) return;
  const AutoMapCellRecord &cell = shown->cell;
  bool sameLevel = cell.teleporterDestLevel == shownDrawList.level;
  bool toItself = cell.teleporterDestX == hoverX && cell.teleporterDestY == hoverY;
  if (IsFiringTeleporterCell(shown))
  {
    if (!sameLevel) return;
    if (toItself)
    {
      if (!collectingSpotlight) Outline(Inset(CellArea(view, hoverX, hoverY), 25), SPINNER);
      return;
    }
    DrawTeleportLine(view, AutoMapLink(hoverX, hoverY, cell.teleporterDestX, cell.teleporterDestY), false);
    return;
  }
  if (cell.kind == AutoMapCellDormantTeleporter)
  {
    if (cell.hasTeleporterTarget && shown->teleporterTargetVisible && sameLevel && !toItself)
    {
      DrawTeleportLine(view, AutoMapLink(hoverX, hoverY, cell.teleporterDestX, cell.teleporterDestY), true);
    }
    return;
  }
  std::vector<AutoMapLink> into = AutoMapScene_TeleportsInto(shownDrawList, hoverX, hoverY);
  for (size_t i = 0; i < into.size(); i++) DrawTeleportLine(view, into[i], false);
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
// columns, and finally only a window of the rows shows, scrolled by Shift+wheel with a "+N"
// marker at each cut edge (AutoMapLayout_TooltipGridWindow).
// Items come grouped from the full (untruncated) pile; pad requirements arrive
// as NEEDS slots from the scene, monsters, missiles and clouds join from the
// same filtered draw list, so the tooltip never shows what the map itself
// hides. Counts use the ruler digit font: no text backend needed.
static const int TOOLTIP_MIN_SLOT = 28;
static const int TOOLTIP_TILE = 38;
static const SDL_Color TOOLTIP_TILE_FILL = {0x3a, 0x37, 0x33, 0xff};
static const SDL_Color TOOLTIP_TILE_DIM = {0x24, 0x22, 0x1f, 0xff};
static const SDL_Color TOOLTIP_TILE_FRAME = {0x5a, 0x55, 0x4c, 0xff};
static const int TOOLTIP_PAD = 6;

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
    // "COPPER COIN X5", "NEEDS ON PAD COIN X2": the count follows the name; "UM VEN POTION": a
    // potion's power rune goes first, as in the inventory
    std::string named = name;
    const char *rune = AutoMapScene_PotionPowerName(cells[i].item.potionPower);
    if (rune[0] != '\0') named = std::string(rune) + " " + named;
    std::string what = AutoMapScene_StackLine(named, cells[i].count);
    const char *need = "";
    if (cells[i].required && cells[i].side == -1) need = cells[i].carried ? "NEEDS CARRIED " : "NEEDS ON PAD ";
    else if (cells[i].required) need = "NEEDS ";
    std::snprintf(line, sizeof(line), "%s%s", need, what.c_str());
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

// The champions in the portraits of a wall, one line each with the side: they come right after
// the position, like a monster's name
static std::vector<std::string> BuildTooltipChampionLines(const std::vector<AutoMapWallText> &walls)
{
  std::vector<std::string> lines;
  for (size_t i = 0; i < walls.size(); i++)
  {
    if (!walls[i].isChampion) continue;
    std::string line = "CHAMPION";
    char sideInitial = SideInitial(walls[i].side);
    if (sideInitial != '\0')
    {
      line += " (";
      line += sideInitial;
      line += ")";
    }
    lines.push_back(line + ": " + walls[i].text);
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
    if (walls[i].isChampion) continue;
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

static SDL_Color TipLineColour(AutoMapTipStyle style)
{
  switch (style)
  {
  case AutoMapTipHeader: return TIP_HEADER_TEXT;
  case AutoMapTipDormantHeader:
  case AutoMapTipDormant: return DORMANT_TEXT;
  case AutoMapTipState: return TIP_STATE_TEXT;
  case AutoMapTipCondition: return TIP_CONDITION_TEXT;
  case AutoMapTipResult: return TIP_RESULT_TEXT;
  case AutoMapTipHint: return TIP_HINT_TEXT;
  case AutoMapTipMonsterName: return LINK_BADGE;
  case AutoMapTipFeeder: return TIP_FEEDER_TEXT;
  default: return WHITE;
  }
}

static void DrawBadgeGlyphsOfLine(int x, int y, const AutoMapTipLine &line)
{
  std::vector<AutoMapTipGlyph> glyphs = AutoMapScene_TipGlyphs(line.text);
  for (size_t g = 0; g < glyphs.size(); g++)
  {
    int size = 7 * DIGIT_SCALE;
    AutoMapRect box = {x + (int)glyphs[g].column * 4 * DIGIT_SCALE, y - DIGIT_SCALE, size, size};
    int kind = glyphs[g].kind;
    if (kind == 4)
    {
      int index = glyphs[g].argument;
      if (index < 0 || index >= (int)line.items.size()) continue;
      const AutoMapItem &item = line.items[index];
      SDL_Rect slot = {box.x, box.y, box.w, box.h};
      if (item.worldPicture == AUTOMAP_NO_PICTURE) DrawKeyholeMark(Inset(slot, 10));
      else DrawTooltipIcon(slot, item);
      continue;
    }
    AutoMapPaint_Plate(box, kind == 5 ? FACING_BADGE_FRAME : BADGE_FRAME);
    if (kind == 1) AutoMapPaint_PartyArrow(box.x + size / 2.0f, box.y + size / 2.0f, size * 0.36f, 0, PARTY, false);
    if (kind == 5)
    {
      int facing = glyphs[g].argument & 3;
      AutoMapPaint_PartyArrow(box.x + size / 2.0f, box.y + size / 2.0f, size * 0.36f, facing, PARTY, false);
    }
    if (kind == 2) AutoMapPaint_Claw(box, BADGE_GLYPH);
    if (kind == 3)
    {
      AutoMapRect square = {box.x + size / 4, box.y + size / 4, size / 2, size / 2};
      AutoMapPaint_Frame(square, BADGE_GLYPH, 1);
    }
  }
}

// Plain footer strings (health, names, texts, notes) become plain styled lines
static void AppendTipLines(std::vector<AutoMapTipLine> &out, const std::vector<std::string> &lines)
{
  for (size_t i = 0; i < lines.size(); i++) out.push_back(AutoMapScene_TipLine(lines[i], AutoMapTipPlain));
}

// The texture a magnified tooltip is drawn into, as big as the map area; false when the tooltip
// is not magnified or the renderer cannot draw into a texture (it is drawn plain then)
static bool TooltipTextureFor(int width, int height)
{
  if (tooltipScale <= 1 || width <= 0 || height <= 0) return false;
  if (tooltipTexture != NULL && tooltipTextureW >= width && tooltipTextureH >= height) return true;
  DropTooltipTexture();
  tooltipTexture = SDL_CreateTexture(mapRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
  if (tooltipTexture == NULL) return false;
  tooltipTextureW = width;
  tooltipTextureH = height;
  return true;
}

// The last line of every tooltip: how to size it, and its size now
static std::string TooltipSizeHint(void)
{
  char hint[48];
  if (tooltipScale > 1) std::snprintf(hint, sizeof(hint), "CTRL+WHEEL: SIZE (%dX)", tooltipScale);
  else std::snprintf(hint, sizeof(hint), "CTRL+WHEEL: SIZE");
  return hint;
}

// A tile at the cut edge of a scrolled icon grid: "+N" and a triangle pointing to the stacks
// out of view, up or down
static void DrawGridMarker(const SDL_Rect &icon, int count, bool up)
{
  Fill(icon, TOOLTIP_TILE_FILL);
  Outline(icon, TOOLTIP_TILE_FRAME);
  std::string more = "+" + std::to_string(count);
  int moreW = AutoMapPaint_TextWidth(more, DIGIT_SCALE);
  int textH = 5 * DIGIT_SCALE;
  int arrow = std::max(3, icon.w / 8);
  int blockH = textH + 2 + arrow;
  int top = icon.y + (icon.h - blockH) / 2;
  int textY = up ? top + arrow + 2 : top;
  float cx = icon.x + icon.w / 2.0f;
  float tipY = up ? (float)top : (float)(top + blockH);
  float baseY = up ? tipY + arrow : tipY - arrow;
  AutoMapPaint_Triangle(cx, tipY, cx - arrow, baseY, cx + arrow, baseY, ITEM_MARKER);
  AutoMapPaint_Text(icon.x + (icon.w - moreW) / 2, textY, more, ITEM_MARKER, DIGIT_SCALE);
}

static void DrawTileTooltip(const AutoMapView &view)
{
  // Set again below once the footer is known; early returns must not leave a stale max
  // that would swallow the zoom
  tooltipScrollMax = 0;
  if (!hoverValid) return;
  std::vector<TooltipCell> cells = FlattenTooltipSlots(BuildTooltipSlots(hoverX, hoverY));
  std::vector<std::string> hpLines = BuildTooltipHealthLines(hoverX, hoverY);
  std::vector<std::string> scrolls = AutoMapScene_ScrollTexts(shownDrawList, hoverX, hoverY);
  std::vector<AutoMapWallText> walls = AutoMapScene_WallTexts(shownDrawList, hoverX, hoverY);
  // Shift unfolds the compact press lines into the full chain; the tooltip redraws every
  // frame, so no event handler is needed.
  bool fullChain = renderShift || (SDL_GetModState() & KMOD_SHIFT) != 0;
  if (!fullChain) tooltipScrollLines = 0;
  std::vector<AutoMapTipLine> hint;
  std::vector<AutoMapTipLine> machinery = AutoMapTooltip_Machinery(
      shownDrawList, hoverX, hoverY, fullChain, AutoMapLevelReader_ItemName, &hint);
  std::vector<std::string> notes = AutoMapScene_PlayerNoteLines(shownDrawList, hoverX, hoverY);
  if (cells.empty() && hpLines.empty() && scrolls.empty() && walls.empty() && machinery.empty()
      && notes.empty())
  {
    return;
  }
  int windowWidth = 0, windowHeight = 0;
  OutputSizeBelowBar(&windowWidth, &windowHeight);
  // Magnified, the tooltip is laid out at its own size and drawn scale times as big
  int scale = TooltipTextureFor(windowWidth, windowHeight) ? tooltipScale : 1;
  int maxW = (windowWidth - 2 * RULER - 16) / scale;
  int maxH = (windowHeight - 2 * RULER - 16) / scale;
  if (maxW < 60) maxW = 60;
  if (maxH < 60) maxH = 60;
  // Footer: position first, then HP lines, then one name line per named stack,
  // left-aligned below the icon grid. The footer keeps a fixed text scale; the icon grid
  // shrinks into what is left.
  std::vector<AutoMapTipLine> footerLines;
  {
    int gx = 0, gy = 0;
    AutoMapScene_GlobalPosition(shownDrawList, hoverX, hoverY, &gx, &gy);
    char pos[64];
    std::snprintf(pos, sizeof(pos), "POS: L%d (%d, %d)", shownDrawList.level, gx, gy);
    footerLines.push_back(AutoMapScene_TipLine(pos, AutoMapTipPlain));
  }
  AppendTipLines(footerLines, BuildTooltipChampionLines(walls));
  std::string monsterName = AutoMapScene_MonsterNameLine(shownDrawList, hoverX, hoverY);
  if (!monsterName.empty() && Shows(AutoMapShowMonsters))
    footerLines.push_back(AutoMapScene_TipLine(monsterName, AutoMapTipMonsterName));
  AppendTipLines(footerLines, hpLines);
  std::vector<std::string> nameLines =
      BuildTooltipNameLines(cells, maxW - 2 * TOOLTIP_PAD);
  AppendTipLines(footerLines, nameLines);
  footerLines.insert(footerLines.end(), machinery.begin(), machinery.end());
  std::vector<std::string> textLines =
      BuildTooltipTextLines(scrolls, walls, maxW - 2 * TOOLTIP_PAD);
  AppendTipLines(footerLines, textLines);
  // The player's own notes come after what the map knows; the hints are the last lines
  AppendTipLines(footerLines, notes);
  footerLines.insert(footerLines.end(), hint.begin(), hint.end());
  // Items past the grid's room are scrolled to as long text is
  if (tooltipGridHiddenRows > 0 && !fullChain)
    footerLines.push_back(AutoMapScene_TipLine("SHIFT+WHEEL: MORE ITEMS", AutoMapTipHint));
  footerLines.push_back(AutoMapScene_TipLine(TooltipSizeHint(), AutoMapTipHint));
  int footerTextH = 5 * DIGIT_SCALE;
  int footerLineGap = 4;
  int footerTextW = 0;
  int footerScroll = std::max(0, tooltipScrollLines - tooltipGridHiddenRows);
  int footerScrollMax = 0;
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
        int w = AutoMapScene_TextWidth(footerLines[i].text.c_str()) * DIGIT_SCALE;
        if (w > footerTextW) footerTextW = w;
      }
      char widest[32];
      std::snprintf(widest, sizeof(widest), "+%d MORE BELOW", total);
      int noteW = AutoMapScene_TextWidth(widest) * DIGIT_SCALE;
      if (noteW > footerTextW) footerTextW = noteW;
      std::snprintf(widest, sizeof(widest), "+%d MORE ABOVE", total);
      noteW = AutoMapScene_TextWidth(widest) * DIGIT_SCALE;
      if (noteW > footerTextW) footerTextW = noteW;
      footerScrollMax = total - visible;
      if (footerScroll > footerScrollMax) footerScroll = footerScrollMax;
      std::vector<AutoMapTipLine> window(footerLines.begin() + footerScroll,
                                      footerLines.begin() + footerScroll + visible);
      char note[32];
      int below = total - footerScroll - visible;
      // The notes replace content lines, so only a window of 3 or more takes them; a
      // smaller one shows content only and every line stays reachable by scrolling.
      if (visible >= 3 && below > 0)
      {
        std::snprintf(note, sizeof(note), "+%d MORE BELOW", below + 1);
        window.back() = AutoMapScene_TipLine(note, AutoMapTipHint);
      }
      if (visible >= 3 && footerScroll > 0)
      {
        std::snprintf(note, sizeof(note), "+%d MORE ABOVE", footerScroll + 1);
        window.front() = AutoMapScene_TipLine(note, AutoMapTipHint);
      }
      footerLines = window;
    }
  }
  for (size_t i = 0; i < footerLines.size(); i++)
  {
    int w = AutoMapScene_TextWidth(footerLines[i].text.c_str()) * DIGIT_SCALE;
    if (w > footerTextW) footerTextW = w;
  }
  int footerH = footerLines.empty() ? 0 : (int)footerLines.size() * footerTextH
      + ((int)footerLines.size() - 1) * footerLineGap + (cells.empty() ? 2 * TOOLTIP_PAD : TOOLTIP_PAD);
  int iconMaxH = maxH - footerH;
  if (iconMaxH < 60 && !cells.empty()) iconMaxH = 60;
  // Start at full map-tile size so one stack reads like a single-item cell at any
  // zoom. Oversized multi-stack plates shrink back down to the map area below.
  // Inventory-slot tiles, six to a row, every stack; rows past the room scroll
  int slot = TOOLTIP_TILE;
  AutoMapTooltipGrid grid = AutoMapLayout_TooltipGrid((int)cells.size());
  size_t slots = cells.size();
  int columns = 0;
  int rows = 0;
  AutoMapTooltipGridWindow gridWindow = {0, 0, 0, 0, 0};
  if (!cells.empty())
  {
    columns = grid.columns;
    rows = grid.rows;
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
    // Still too tall: a window of the rows, scrolled first by Shift+wheel
    int maxRows = (iconMaxH - 2 * TOOLTIP_PAD) / slot;
    gridWindow = AutoMapLayout_TooltipGridWindow((int)cells.size(), columns, maxRows, tooltipScrollLines);
    rows = gridWindow.rows;
  }
  tooltipGridHiddenRows = gridWindow.hiddenRows;
  tooltipScrollMax = gridWindow.hiddenRows + footerScrollMax;
  if (tooltipScrollLines > tooltipScrollMax) tooltipScrollLines = tooltipScrollMax;
  int iconW = cells.empty() ? 0 : columns * slot + 2 * TOOLTIP_PAD;
  int iconH = cells.empty() ? 0 : rows * slot + 2 * TOOLTIP_PAD;
  int plateW = iconW;
  if (!footerLines.empty() && footerTextW + 2 * TOOLTIP_PAD > plateW)
  {
    plateW = footerTextW + 2 * TOOLTIP_PAD;
  }
  int plateH = iconH + footerH;
  if (plateW < 60) plateW = 60;
  // Where it hides the least of what the hover lights up (AutoMapLayout_TooltipPlace)
  AutoMapRect mapArea = {RULER, RULER, windowWidth - 2 * RULER, windowHeight - 2 * RULER};
  AutoMapRect place = AutoMapLayout_TooltipPlace(hoverX, hoverY, spotlightLines, spotlightGoals,
                                                 plateW * scale, plateH * scale, view.originX, view.originY,
                                                 view.cellSize, GOAL_TAB_HEIGHT, mapArea);
  int x = place.x;
  int y = place.y;
  SDL_Rect oldClip;
  bool wasClipped = SDL_RenderIsClipEnabled(mapRenderer) == SDL_TRUE;
  SDL_RenderGetClipRect(mapRenderer, &oldClip);
  if (scale > 1)
  {
    if (SDL_SetRenderTarget(mapRenderer, tooltipTexture) == 0)
    {
      SDL_RenderSetClipRect(mapRenderer, NULL);
      SDL_RenderSetScale(mapRenderer, (float)scale, (float)scale);
      x = 0;
      y = 0;
    }
    else scale = 1;
  }
  SDL_Rect plate = {x, y, plateW, plateH};
  Fill(plate, BACKGROUND);
  Outline(plate, ITEM_MARKER);
  size_t firstShown = (size_t)gridWindow.firstRow * (size_t)columns;
  size_t endShown = std::min(cells.size(), firstShown + (size_t)rows * (size_t)columns);
  for (size_t i = firstShown; i < endShown; i++)
  {
    int col = (int)(i - firstShown) % columns;
    int row = (int)(i - firstShown) / columns;
    SDL_Rect icon = {x + TOOLTIP_PAD + col * slot, y + TOOLTIP_PAD + row * slot, slot - 2, slot - 2};
    // The window's cut edges: how many stacks lie above and below, and which way
    bool aboveMark = i == firstShown && gridWindow.above > 0;
    bool belowMark = i + 1 == endShown && gridWindow.below > 0;
    if (aboveMark || belowMark)
    {
      DrawGridMarker(icon, aboveMark ? gridWindow.above : gridWindow.below, aboveMark);
      continue;
    }
    // A nested tile (chest contents, what a monster carries) is smaller; a predicted drop is dim
    SDL_Rect tile = cells[i].nested ? Inset(icon, 16) : icon;
    Fill(tile, cells[i].predicted ? TOOLTIP_TILE_DIM : TOOLTIP_TILE_FILL);
    Outline(tile, cells[i].required ? FACING_BADGE_FRAME
                                    : (cells[i].nested && !cells[i].predicted ? ITEM_MARKER : TOOLTIP_TILE_FRAME));
    DrawTooltipIcon(tile, cells[i].item);
    DrawCountBadge(icon, cells[i].count);
  }
  for (size_t i = 0; i < footerLines.size(); i++)
  {
    int lineY = y + iconH + (cells.empty() ? TOOLTIP_PAD : 0)
        + (int)i * (footerTextH + footerLineGap);
    DrawLegendText(x + TOOLTIP_PAD, lineY, AutoMapScene_TipTextWithoutGlyphs(footerLines[i].text).c_str(), TipLineColour(footerLines[i].style));
    DrawBadgeGlyphsOfLine(x + TOOLTIP_PAD, lineY, footerLines[i]);
  }
  if (scale > 1)
  {
    SDL_RenderSetScale(mapRenderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(mapRenderer, NULL);
    UseMapViewport();
    SDL_RenderSetClipRect(mapRenderer, wasClipped ? &oldClip : NULL);
    SDL_Rect from = {0, 0, plateW * scale, plateH * scale};
    SDL_Rect to = {place.x, place.y, plateW * scale, plateH * scale};
    SDL_RenderCopy(mapRenderer, tooltipTexture, &from, &to);
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
    bool isHovered = hoverValid && x == hoverX && x <= last;
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
    bool isHovered = hoverValid && y == hoverY && y <= last;
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
// Text in the map's 3x5 font

static void DrawLegendText(int x, int y, const char *text, const SDL_Color &colour)
{
  if (text == NULL) return;
  AutoMapPaint_Text(x, y, text, colour, DIGIT_SCALE);
}

static bool PointInRect(int x, int y, const SDL_Rect &area)
{
  return x >= area.x && x < area.x + area.w && y >= area.y && y < area.y + area.h;
}

static const int SEARCH_QUERY_H = 20;
static const int SEARCH_ROW_H = 20;
static const int SEARCH_MOST_ROWS = 16;
static const int SEARCH_PANEL_W = 440;
static const SDL_Color SEARCH_TAG = {0x8f, 0x86, 0x76, 0xff};

// How many result rows fit the window: the panel stays clear of the rulers
static int SearchVisibleRows(void)
{
  if (mapRenderer == NULL) return 8;
  int windowWidth = 0, windowHeight = 0;
  OutputSizeBelowBar(&windowWidth, &windowHeight);
  (void)windowWidth;
  int rows = (windowHeight - 40 - SEARCH_QUERY_H) / SEARCH_ROW_H;
  return std::max(4, std::min(SEARCH_MOST_ROWS, rows));
}

// Below the bar's / button, as wide as the results need, at most 440 pixels
static SDL_Rect SearchPanelPlace(void)
{
  SDL_Rect empty = {0, 0, 0, 0};
  if (!searchOpen || mapRenderer == NULL) return empty;
  int windowWidth = 0, windowHeight = 0;
  OutputSizeBelowBar(&windowWidth, &windowHeight);
  (void)windowHeight;
  int w = std::min(SEARCH_PANEL_W, windowWidth - 2 * RULER);
  if (w < 200) return empty;
  int x = std::max(RULER, std::min(AutoMapBar_SearchButtonX() - w / 2, windowWidth - RULER - w));
  int total = (int)SearchRowsNow().size();
  int rows = std::max(1, std::min(SearchVisibleRows(), total));
  int h = SEARCH_QUERY_H + rows * SEARCH_ROW_H + 8;
  SDL_Rect plate = {x, 2, w, h};
  return plate;
}

// The selected hit on the map, with a margin, when it is on the level shown
static bool SelectedHitArea(SDL_Rect *area)
{
  if (!searchOpen || searchSelected < 0 || searchSelected >= (int)searchHits.size()) return false;
  const AutoMapSearchHit &hit = searchHits[(size_t)searchSelected];
  if (hit.level != shownDrawList.level || hit.x < 0 || hit.y < 0) return false;
  AutoMapView view = ViewOfShownLevel();
  if (view.cellSize <= 0) return false;
  const int margin = 8;
  SDL_Rect cell = {view.originX + hit.x * view.cellSize - margin, view.originY + hit.y * view.cellSize - margin,
                   view.cellSize + 2 * margin, view.cellSize + 2 * margin};
  *area = cell;
  return true;
}

// Where the panel is drawn: under the / button, unless it would cover the selected hit; then at
// the window's edge away from the hit
static SDL_Rect SearchPanelRect(void)
{
  SDL_Rect plate = SearchPanelPlace();
  SDL_Rect hit;
  if (plate.w <= 0 || !SelectedHitArea(&hit) || !SDL_HasIntersection(&plate, &hit)) return plate;
  int windowWidth = 0, windowHeight = 0;
  OutputSizeBelowBar(&windowWidth, &windowHeight);
  SDL_Rect left = plate, right = plate;
  left.x = RULER;
  right.x = windowWidth - RULER - plate.w;
  bool hitIsRight = hit.x + hit.w / 2 > windowWidth / 2;
  SDL_Rect away = hitIsRight ? left : right;
  SDL_Rect near = hitIsRight ? right : left;
  if (!SDL_HasIntersection(&away, &hit)) return away;
  if (!SDL_HasIntersection(&near, &hit)) return near;
  return plate;
}

// The row of the panel under a pixel: an index into SearchRowsNow()
static bool SearchHitAtPixel(int pixelX, int pixelY, int *rowIndex)
{
  if (!searchOpen) return false;
  SDL_Rect plate = SearchPanelRect();
  if (plate.w <= 0 || !PointInRect(pixelX, pixelY, plate)) return false;
  int row = (pixelY - plate.y - SEARCH_QUERY_H - 4) / SEARCH_ROW_H;
  int total = (int)SearchRowsNow().size();
  int rows = std::min(SearchVisibleRows(), total);
  if (pixelY < plate.y + SEARCH_QUERY_H + 4 || row < 0 || row >= rows) return false;
  int index = searchTop + row;
  if (index < 0 || index >= total) return false;
  if (rowIndex != NULL) *rowIndex = index;
  return true;
}

static std::string WithoutGroupCount(const std::string &name)
{
  size_t at = name.rfind(" X");
  if (at == std::string::npos || at + 2 >= name.size()) return name;
  for (size_t i = at + 2; i < name.size(); i++)
  {
    if (name[i] < '0' || name[i] > '9') return name;
  }
  return name.substr(0, at);
}

// A stack of more than one, in front of where it lies: "X2 ALCOVE"
static std::string Counted(const AutoMapSearchHit &hit, const std::string &tag)
{
  return hit.count > 1 ? "X" + std::to_string(hit.count) + " " + tag : tag;
}

// The tag at the right of a hit: how many, and what kind of find it is
static std::string SearchTag(const AutoMapSearchHit &hit, SDL_Color *colour)
{
  *colour = SEARCH_TAG;
  switch (hit.kind)
  {
  case AutoMapHitNeeds:
    *colour = FACING_BADGE_FRAME;
    if (hit.slot.side < 0) return Counted(hit, hit.slot.carried ? "PAD, CARRIED" : "PAD NEEDS");
    return Counted(hit, "NEEDS");
  case AutoMapHitCarried: return Counted(hit, "CARRIED BY " + WithoutGroupCount(hit.carriedBy));
  case AutoMapHitInChest: return Counted(hit, "IN CHEST");
  case AutoMapHitAlcove: return Counted(hit, "ALCOVE");
  case AutoMapHitGenerator: *colour = GEN_TEXT; return "GENERATOR";
  case AutoMapHitFeature:
  {
    if (hit.stairsTarget != AUTOMAP_NO_LEVEL) return "TO L" + std::to_string(hit.stairsTarget);
    char side = SideInitial(hit.slot.side);
    return side ? std::string("(") + side + ")" : "";
  }
  default: return hit.count > 1 ? "X" + std::to_string(hit.count) : "";
  }
}

static void DrawSearchIcon(const SDL_Rect &box, const AutoMapSearchHit &hit)
{
  if (hit.kind == AutoMapHitNeeds)
  {
    SDL_Rect frame = Inset(box, -6);
    Fill(frame, BACKGROUND);
    Outline(frame, FACING_BADGE_FRAME);
  }
  if (hit.kind == AutoMapHitMonster || hit.kind == AutoMapHitGenerator)
  {
    AutoMapSpriteRequest request = {hit.monsterPicture, hit.monsterTransparentColour};
    // The hit may lie on another level, whose monsters have colours of their own
    request.scheme9PlusOne = hit.paletteScheme9 + 1;
    request.scheme10PlusOne = hit.paletteScheme10 + 1;
    const AutoMapSprite *sprite = hit.monsterPicture == AUTOMAP_NO_PICTURE ? NULL : sprites.Find(request);
    if (sprite != NULL) DrawSprite(*sprite, FitInto(box, *sprite));
    if (hit.kind == AutoMapHitGenerator)
    {
      AutoMapRect tag = {box.x + box.w - 9, box.y + box.h - 7, 10, 8};
      AutoMapPaint_Plate(tag, GENERATOR);
      AutoMapPaint_Text(tag.x + 3, tag.y + 1, "G", GEN_TEXT, 1);
    }
    return;
  }
  if (hit.wallFeature == AutoMapWallFeatureStairsUp || hit.wallFeature == AutoMapWallFeatureStairsDown)
  {
    // The stairs as the map draws them, too small for their plate: the stripes in their colours
    AutoMapCellRecord stairs;
    stairs.kind = AutoMapCellStairs;
    stairs.stairsLeadUp = hit.wallFeature == AutoMapWallFeatureStairsUp;
    stairs.runsNorthSouth = true;
    stairs.stairsTargetLevel = AUTOMAP_NO_LEVEL;
    int side = std::min(box.w, box.h);
    SDL_Rect square = {box.x + (box.w - side) / 2, box.y + (box.h - side) / 2, side, side};
    DrawStairs(square, stairs);
    return;
  }
  if (hit.kind == AutoMapHitFeature)
  {
    const AutoMapShownCell *shown = NULL;
    for (size_t i = 0; i < shownDrawList.cells.size() && hit.level == shownDrawList.level; i++)
    {
      if (shownDrawList.cells[i].x == hit.x && shownDrawList.cells[i].y == hit.y) shown = &shownDrawList.cells[i];
    }
    if (shown != NULL && hit.slot.side >= 0)
    {
      const AutoMapSprite *sprite = SpriteOfTheGame(shown->cell.wallSides[hit.slot.side].picture);
      if (sprite != NULL) DrawSprite(*sprite, FitInto(box, *sprite));
    }
    return;
  }
  DrawTooltipIcon(box, hit.slot.item);
}

// The results: a heading per level (the shown level first), then its hits in reading order with
// the item's icon, the cell as the rulers number it, the name and a tag
static void DrawSearchPanel(void)
{
  if (!searchOpen) return;
  SDL_Rect plate = SearchPanelRect();
  if (plate.w <= 0) return;
  Fill(plate, BACKGROUND);
  Outline(plate, LINK_BADGE);
  std::vector<AutoMapSearchRow> rows = SearchRowsNow();
  DrawLegendText(plate.x + 8, plate.y + 6, ("FIND " + searchQuery + "_").c_str(), WHITE);
  int levels = 0;
  for (size_t r = 0; r < rows.size(); r++) levels += rows[r].heading ? 1 : 0;
  if (!searchHits.empty())
  {
    std::string info = std::to_string(searchHits.size()) + " ON " + std::to_string(levels)
                       + (levels > 1 ? " LEVELS" : " LEVEL");
    DrawLegendText(plate.x + plate.w - 12 - AutoMapPaint_TextWidth(info, DIGIT_SCALE), plate.y + 6, info.c_str(),
                   RULER_TEXT);
  }
  SDL_Rect line = {plate.x + 4, plate.y + SEARCH_QUERY_H, plate.w - 8, 1};
  Fill(line, FLOOR_EDGE);
  int y = plate.y + SEARCH_QUERY_H + 4;
  if (!searchQuery.empty() && searchHits.empty()) DrawLegendText(plate.x + 8, y + 4, "NOTHING SEEN MATCHES", RULER_TEXT);
  if (searchQuery.empty()) DrawLegendText(plate.x + 8, y + 4, "TYPE A NAME: ITEM, MONSTER, GEN", FLAVOR_TICK);
  int visible = SearchVisibleRows();
  int coordsW = AutoMapPaint_TextWidth("(00,00)", DIGIT_SCALE);
  for (int r = searchTop; r < (int)rows.size() && r < searchTop + visible; r++)
  {
    const AutoMapSearchRow &row = rows[(size_t)r];
    SDL_Rect band = {plate.x + 4, y, plate.w - 16, SEARCH_ROW_H};
    if (row.heading)
    {
      SDL_Rect strip = {band.x, band.y + 1, band.w, band.h - 2};
      Fill(strip, RULER_BACKGROUND);
      std::string name = (searchFolded.count(row.level) ? "+ L" : "- L") + std::to_string(row.level);
      DrawLegendText(band.x + 6, y + 5, name.c_str(), LINK_BADGE);
      std::string count = std::to_string(row.hits) + (row.hits > 1 ? " HITS" : " HIT");
      DrawLegendText(band.x + 6 + AutoMapPaint_TextWidth("- L000", DIGIT_SCALE) + 10, y + 5, count.c_str(), RULER_TEXT);
      int right = band.x + band.w - 4;
      if (row.level == shownDrawList.level)
      {
        right -= AutoMapPaint_TextWidth("SHOWN", DIGIT_SCALE);
        DrawLegendText(right, y + 5, "SHOWN", RULER_TEXT);
      }
      if (row.level == AutoMap_PartyLevel()) AutoMapPaint_PartyArrow(right - 12, y + SEARCH_ROW_H / 2.0f, 5, 1, PARTY, false);
    }
    else
    {
      const AutoMapSearchHit &hit = searchHits[(size_t)row.hit];
      bool selected = row.hit == searchSelected;
      if (selected) Fill(band, LINK_BADGE);
      SDL_Rect icon = {band.x + 18, y + 2, 16, 16};
      DrawSearchIcon(icon, hit);
      SDL_Color dark = {0x1a, 0x16, 0x0f, 0xff};
      std::string at = "(" + std::to_string(hit.globalX) + "," + std::to_string(hit.globalY) + ")";
      DrawLegendText(band.x + 42, y + 5, at.c_str(), selected ? dark : RULER_TEXT);
      DrawLegendText(band.x + 42 + coordsW + 10, y + 5, hit.name.c_str(), selected ? dark : WHITE);
      SDL_Color tagColour;
      std::string tag = SearchTag(hit, &tagColour);
      if (!tag.empty())
      {
        DrawLegendText(band.x + band.w - 4 - AutoMapPaint_TextWidth(tag, DIGIT_SCALE), y + 5, tag.c_str(),
                       selected ? dark : tagColour);
      }
    }
    y += SEARCH_ROW_H;
  }
  if ((int)rows.size() > visible)
  {
    int track = plate.h - SEARCH_QUERY_H - 8;
    int thumb = std::max(14, track * visible / (int)rows.size());
    int thumbY = plate.y + SEARCH_QUERY_H + 4 + (track - thumb) * searchTop / std::max(1, (int)rows.size() - visible);
    SDL_Rect rail = {plate.x + plate.w - 8, plate.y + SEARCH_QUERY_H + 4, 4, track};
    SDL_Rect knob = {rail.x, thumbY, 4, thumb};
    Fill(rail, RULER_BACKGROUND);
    Fill(knob, RULER_TEXT);
  }
}

// The stair the last ASCEND / DESCEND came out on: the level dims around it and it gets a light
// frame. Nothing is drawn when the map does not show the cell as stairs, so it gives nothing away.
static void DrawStairMark(const AutoMapView &view)
{
  if (!stairMarkOn || collectingSpotlight || shownDrawList.level != stairMarkLevel) return;
  int x = stairMarkX - shownDrawList.offsetX;
  int y = stairMarkY - shownDrawList.offsetY;
  const AutoMapShownCell *shown = ShownCellAt(shownDrawList, x, y);
  if (shown == NULL || shown->cell.kind != AutoMapCellStairs) return;
  std::vector<SDL_Rect> dim;
  for (size_t i = 0; i < shownDrawList.cells.size(); i++)
  {
    const AutoMapShownCell &other = shownDrawList.cells[i];
    if (other.x != x || other.y != y) dim.push_back(CellArea(view, other.x, other.y));
  }
  SDL_SetRenderDrawColor(mapRenderer, 8, 7, 5, 140);
  if (!dim.empty()) SDL_RenderFillRects(mapRenderer, dim.data(), (int)dim.size());
  int thickness = AutoMapLayout_FaceLine(view.cellSize);
  SDL_Rect cell = CellArea(view, x, y);
  AutoMapRect rim = {cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2};
  AutoMapRect frame = {cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4};
  SDL_Color dark = {0x0b, 0x0a, 0x08, 0xff};
  AutoMapPaint_Frame(rim, dark, thickness + 2);
  AutoMapPaint_Frame(frame, KNOWN_SWITCH_LINE, thickness);
}

// Every hit on the shown level gets a pixel frame, the selected one a light one; while a hit is
// selected the map outside the shown level's hits dims, as for links
static void DrawSearchMarkers(const AutoMapView &view)
{
  if (!searchOpen || searchQuery.empty() || collectingSpotlight) return;
  // Once a hit was picked, every level with hits dims around them, also one stepped to later
  bool picked = searchSelected >= 0 && searchSelected < (int)searchHits.size();
  bool hitsHere = false;
  for (size_t i = 0; i < searchHits.size() && !hitsHere; i++) hitsHere = searchHits[i].level == shownDrawList.level;
  if (picked && hitsHere)
  {
    std::vector<SDL_Rect> dim;
    for (size_t i = 0; i < shownDrawList.cells.size(); i++)
    {
      const AutoMapShownCell &shown = shownDrawList.cells[i];
      bool hit = false;
      for (size_t k = 0; k < searchHits.size() && !hit; k++)
      {
        hit = searchHits[k].level == shownDrawList.level && searchHits[k].x == shown.x && searchHits[k].y == shown.y;
      }
      if (!hit) dim.push_back(CellArea(view, shown.x, shown.y));
    }
    SDL_SetRenderDrawColor(mapRenderer, 8, 7, 5, 140);
    if (!dim.empty()) SDL_RenderFillRects(mapRenderer, dim.data(), (int)dim.size());
  }
  int thickness = AutoMapLayout_FaceLine(view.cellSize);
  // Several hits can share a cell; the selected one is drawn last so its light frame stays on top
  for (int pass = 0; pass < 2; pass++)
  {
    for (size_t i = 0; i < searchHits.size(); i++)
    {
      bool selected = (int)i == searchSelected;
      if (searchHits[i].level != shownDrawList.level || selected != (pass == 1)) continue;
      SDL_Rect cell = CellArea(view, searchHits[i].x, searchHits[i].y);
      AutoMapRect rim = {cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2};
      AutoMapRect frame = {cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4};
      SDL_Color dark = {0x0b, 0x0a, 0x08, 0xff};
      AutoMapPaint_Frame(rim, dark, thickness + 2);
      AutoMapPaint_Frame(frame, selected ? KNOWN_SWITCH_LINE : LINK_GOLD, thickness);
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
  // The party never stands in a wall; a false wall it may walk through
  if (AutoMap_ReplayIsRunning() && shown.cell.kind != AutoMapCellWall)
  {
    bool isSet = AutoMap_HasReplayBreakpoint(shownDrawList.level, shown.x - shownDrawList.boundaryMarginX,
                                             shown.y - shownDrawList.boundaryMarginY);
    DebugMenuRow breakpoint = {MenuRowBreakpoint, 0, isSet ? "REMOVE REPLAY PAUSE" : "PAUSE REPLAY HERE", true};
    debugMenuRows.push_back(breakpoint);
  }
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
  // Stairs the map knows the target of: come out of them on the other level
  const AutoMapShownCell *stairs = StairsShownAt(x, y);
  if (stairs != NULL)
  {
    if (!debugMenuRows.empty())
    {
      DebugMenuRow separator = {MenuRowSeparator, 0, "", false};
      debugMenuRows.push_back(separator);
    }
    DebugMenuRow follow = {MenuRowFollowStairs, 0, stairs->cell.stairsLeadUp ? "ASCEND" : "DESCEND", true};
    debugMenuRows.push_back(follow);
  }
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
  if (mapRenderer != NULL) OutputSizeBelowBar(&windowWidth, &windowHeight);
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
  if (row.kind == MenuRowBreakpoint)
  {
    AutoMap_ToggleReplayBreakpoint(debugMenuLevel, debugMenuX - shownDrawList.boundaryMarginX,
                                   debugMenuY - shownDrawList.boundaryMarginY);
    return;
  }
  if (row.kind == MenuRowSeparator) return;
  if (row.kind == MenuRowFollowStairs)
  {
    FollowStairsAt(debugMenuX, debugMenuY);
    return;
  }
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
  OutputSizeBelowBar(&windowWidth, &windowHeight);
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
  if (searchOpen) SearchCloseBox();
  levelPanelOpen = true;
  isDirty = true;
}

// The panel's rows: one per champion and an ALL row for the whole party, then the light,
// monsters, forget and measure rows. Hover and clicks name a row and a column; champion rows
// have the four skills and ALL as columns.
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

static const char *const PARTY_ROW_NAME = "ALL";
static const int ALL_SKILLS_COLUMN = 4;

// The champions' rows and the ALL row under them (none without a champion)
static int ChampionRowCount(const std::vector<AutoMapChampionLevels> &champions)
{
  return champions.empty() ? 0 : (int)champions.size() + 1;
}

static int PartyRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return (int)champions.size();
}

static int LightRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return ChampionRowCount(champions);
}

static int MonstersRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return ChampionRowCount(champions) + 1;
}

static int ForgetRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return ChampionRowCount(champions) + 2;
}

static int MeasureRow(const std::vector<AutoMapChampionLevels> &champions)
{
  return ChampionRowCount(champions) + 3;
}

static int PanelRowCount(const std::vector<AutoMapChampionLevels> &champions)
{
  return ChampionRowCount(champions) + 4;
}

// Whether a click on a champion row's cell (or the ALL row's, champion -1) can raise anything
static bool LevelCellCanGain(const std::vector<AutoMapChampionLevels> &champions, int champion, int column)
{
  for (size_t c = 0; c < champions.size(); c++)
  {
    if (champion >= 0 && (int)c != champion) continue;
    for (int skill = 0; skill < 4; skill++)
    {
      if ((column == ALL_SKILLS_COLUMN || column == skill) && champions[c].canGainLevel[skill]) return true;
    }
  }
  return false;
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
                                 std::max(AutoMapScene_TextWidth(PARTY_ROW_NAME),
                                 std::max(AutoMapScene_TextWidth(MONSTERS_ROW_NAME),
                                          AutoMapScene_TextWidth(MEASURE_ROW_NAME)))));
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
  return champions.empty() ? "NO CHAMPION TO GAIN A LEVEL" : "GAIN A LEVEL   SHIFT: TO THE TOP";
}

static SDL_Rect LevelPanelRect(const std::vector<AutoMapChampionLevels> &champions)
{
  int cells = std::max(4 * LevelPanelCellWidth() + ToolCellWidth(PARTY_ROW_NAME),
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
    int along = pixelX - cellsX;
    if (along < 4 * LevelPanelCellWidth()) cell = along / LevelPanelCellWidth();
    else if (along < 4 * LevelPanelCellWidth() + ToolCellWidth(PARTY_ROW_NAME)) cell = ALL_SKILLS_COLUMN;
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
  // A champion's row or the ALL row; the ALL column is every skill. Shift goes on to the top.
  int champion = row == PartyRow(champions) ? -1 : row;
  if (!LevelCellCanGain(champions, champion, column)) return;
  bool toTheTop = (SDL_GetModState() & KMOD_SHIFT) != 0;
  AutoMap_RequestLevelUp(champion, column == ALL_SKILLS_COLUMN ? -1 : column, toTheTop);
}

static bool LevelPanelHandlePointerEvent(const SDL_Event &event)
{
  if (!debugMenuOn) return false;
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
  for (int row = 0; row < ChampionRowCount(champions); row++)
  {
    bool isParty = row == PartyRow(champions);
    int champion = isParty ? -1 : row;
    DrawLegendText(nameX, y, isParty ? PARTY_ROW_NAME : champions[(size_t)row].name.c_str(), RULER_TEXT);
    for (int column = 0; column <= ALL_SKILLS_COLUMN; column++)
    {
      bool canGain = LevelCellCanGain(champions, champion, column);
      bool isHovered = row == levelPanelHoveredRow && column == levelPanelHoveredColumn && canGain;
      char label[16];
      if (column == ALL_SKILLS_COLUMN) std::snprintf(label, sizeof(label), "%s", PARTY_ROW_NAME);
      else if (isParty) std::snprintf(label, sizeof(label), "%s", LEVEL_PANEL_SKILLS[column]);
      else std::snprintf(label, sizeof(label), "%s %d", LEVEL_PANEL_SKILLS[column], champions[(size_t)row].level[column]);
      int w = column == ALL_SKILLS_COLUMN ? ToolCellWidth(PARTY_ROW_NAME) : LevelPanelCellWidth();
      DrawPanelCell(cellsX + column * LevelPanelCellWidth(), y, w, label, isHovered,
                    canGain ? RULER_TEXT : RULER_TICK);
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
    // Blended, so the cells the list leaves out show the background behind the copy
    SDL_SetTextureBlendMode(staticCache, SDL_BLENDMODE_BLEND);
    if (SDL_SetRenderTarget(mapRenderer, staticCache) < 0)
    {
      SDL_DestroyTexture(staticCache);
      staticCache = NULL;
      return false;
    }
    // A new texture holds whatever its memory held: cells the list leaves out must stay see-through
    SDL_SetRenderDrawColor(mapRenderer, 0, 0, 0, 0);
    SDL_RenderClear(mapRenderer);
    AutoMapView cacheView = {view.cellSize, 0, 0};
    for (size_t i = 0; i < shownDrawList.cells.size(); i++)
    {
      DrawCellBase(cacheView, shownDrawList.cells[i], shownDrawList);
    }
    DrawGlimpses(cacheView, shownDrawList);
    DrawPlayerNotes(cacheView, shownDrawList);
    SDL_SetRenderTarget(mapRenderer, NULL);
    UseMapViewport();
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
  spotlightCells.clear();
  spotlightLines.clear();
  spotlightGoals.clear();
  collectingSpotlight = true;
  DrawLinkArrows(view);
  collectingSpotlight = false;
  // A hovered trigger or target owns the spotlight; the search marks wait until it is gone
  if (spotlightCells.empty()) DrawSearchMarkers(view);
  if (spotlightCells.empty()) DrawStairMark(view);
  DrawSpotlight(view);
  DrawLinkArrows(view);
  DrawLinkModeHighlights(view);
  DrawTileTooltip(view);
}

static void DrawLevel(void)
{
  AutoMapView view = ViewOfShownLevel();
  if (view.cellSize == 0) return;
  int windowWidth = 0, windowHeight = 0;
  OutputSizeBelowBar(&windowWidth, &windowHeight);
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
  DrawLinkModeHint();
  DrawSearchPanel();
  DrawLevelPanel();
  DrawDebugMenu();
}

// The bar on top, the map in its viewport below it, the bar's panels over everything
static void PaintWindow(void)
{
  int width = 0, height = 0;
  SDL_GetRendererOutputSize(mapRenderer, &width, &height);
  UseWholeWindow();
  UseColour(BACKGROUND);
  SDL_RenderClear(mapRenderer);
  AutoMapBar_Draw(width);
  UseMapViewport();
  if (shownDrawList.level != AUTOMAP_NO_LEVEL) DrawLevel();
  UseWholeWindow();
  AutoMapBar_DrawPanels(width, height);
}

void AutoMapWindow_PaintNow(void)
{
  if (!isShown || mapRenderer == NULL) return;
  PaintWindow();
  SDL_RenderPresent(mapRenderer);
  isDirty = false;
}

Uint32 AutoMapWindow_WindowId(void)
{
  return mapWindow != NULL ? SDL_GetWindowID(mapWindow) : 0;
}

void AutoMapWindow_Repaint(void)
{
  isDirty = true;
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
  PaintWindow();
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

// The list a TooltipScope holds in shownDrawList, and what it replaced
static const AutoMapDrawList *tooltipScopeList = NULL;
static AutoMapDrawList tooltipScopeSaved;

void AutoMapWindow_TooltipScope(const AutoMapDrawList *list)
{
  if (tooltipScopeList != NULL) std::swap(shownDrawList, tooltipScopeSaved);
  tooltipScopeList = list;
  if (list == NULL) return;
  tooltipScopeSaved = *list;
  std::swap(shownDrawList, tooltipScopeSaved);
}

// The tooltip of a cell as DrawTileTooltip builds it, for the export (AutoMapWindowTooltip.h)
void AutoMapWindow_TooltipFor(const AutoMapDrawList &list, int x, int y, bool fullChain,
                                std::vector<AutoMapTipLine> *lines, std::vector<AutoMapTooltipEntry> *out)
{
  // Inside a scope for this very list the copy is already in place; otherwise copy it for this call
  bool inScope = tooltipScopeList == &list;
  AutoMapDrawList saved;
  if (!inScope)
  {
    saved = shownDrawList;
    shownDrawList = list;
  }
  std::vector<TooltipCell> cells = FlattenTooltipSlots(BuildTooltipSlots(x, y));
  std::vector<std::string> hpLines = BuildTooltipHealthLines(x, y);
  std::vector<std::string> scrolls = AutoMapScene_ScrollTexts(shownDrawList, x, y);
  std::vector<AutoMapWallText> walls = AutoMapScene_WallTexts(shownDrawList, x, y);
  std::vector<AutoMapTipLine> hint;
  std::vector<AutoMapTipLine> machinery = AutoMapTooltip_Machinery(
      shownDrawList, x, y, fullChain, AutoMapLevelReader_ItemName, &hint);
  std::vector<std::string> notes = AutoMapScene_PlayerNoteLines(shownDrawList, x, y);
  bool any = !(cells.empty() && hpLines.empty() && scrolls.empty() && walls.empty()
               && machinery.empty() && notes.empty());
  if (any)
  {
    int gx = 0, gy = 0;
    AutoMapScene_GlobalPosition(shownDrawList, x, y, &gx, &gy);
    char pos[64];
    std::snprintf(pos, sizeof(pos), "POS: L%d (%d, %d)", shownDrawList.level, gx, gy);
    lines->push_back(AutoMapScene_TipLine(pos, AutoMapTipPlain));
    AppendTipLines(*lines, BuildTooltipChampionLines(walls));
    std::string monsterName = AutoMapScene_MonsterNameLine(shownDrawList, x, y);
    if (!monsterName.empty()) lines->push_back(AutoMapScene_TipLine(monsterName, AutoMapTipMonsterName));
    AppendTipLines(*lines, hpLines);
    AppendTipLines(*lines, BuildTooltipNameLines(cells, 100000));
    lines->insert(lines->end(), machinery.begin(), machinery.end());
    AppendTipLines(*lines, BuildTooltipTextLines(scrolls, walls, 100000));
    AppendTipLines(*lines, notes);
    lines->insert(lines->end(), hint.begin(), hint.end());
    for (size_t i = 0; i < cells.size(); i++)
    {
      AutoMapTooltipEntry c = {cells[i].item, cells[i].count, cells[i].nested, cells[i].predicted,
                        cells[i].required, cells[i].carried, cells[i].side};
      out->push_back(c);
    }
  }
  if (!inScope) shownDrawList = saved;
}

// The map window as it would look, drawn off screen into a picture: no window needed, so the
// look can be checked without playing (AutoMapExport.cpp, CSB_MAP_RENDER)
bool AutoMapWindow_RenderToFile(const AutoMapDrawList &list, int width, int height, int cellSize,
                                int cellX, int cellY, bool shift, const char *path)
{
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
  if (surface == NULL) return false;
  SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
  if (renderer == NULL)
  {
    SDL_FreeSurface(surface);
    return false;
  }
  SDL_Renderer *windowRenderer = mapRenderer;
  mapRenderer = renderer;
  SDL_SetRenderDrawBlendMode(mapRenderer, SDL_BLENDMODE_BLEND);
  sprites.UseRenderer(mapRenderer);
  AutoMapPaint_Use(mapRenderer);
  if (staticCache != NULL) SDL_DestroyTexture(staticCache);
  staticCache = NULL;
  // The window's tooltip texture belongs to the window's renderer: kept aside, not dropped
  SDL_Texture *windowTooltip = tooltipTexture;
  int windowTooltipW = tooltipTextureW, windowTooltipH = tooltipTextureH;
  tooltipTexture = NULL;
  tooltipTextureW = tooltipTextureH = 0;
  AutoMapWindow_ShowDrawList(list);
  hoverValid = true;
  hoverX = cellX;
  hoverY = cellY;
  renderCellSize = cellSize;
  renderShift = shift;
  // Pictures for the manual show the tooltip at its plain size
  int windowTooltipScale = tooltipScale;
  tooltipScale = 1;
  // The pointer rests on the middle of the hovered cell
  AutoMapView view = ViewOfShownLevel();
  hoverPixelX = view.originX + cellX * cellSize + cellSize / 2;
  hoverPixelY = view.originY + cellY * cellSize + cellSize / 2;
  PaintWindow();
  SDL_RenderPresent(mapRenderer);
  bool saved = SDL_SaveBMP(surface, path) == 0;
  renderCellSize = 0;
  renderShift = false;
  tooltipScale = windowTooltipScale;
  ClearHover();
  if (staticCache != NULL) SDL_DestroyTexture(staticCache);
  staticCache = NULL;
  DropTooltipTexture();
  tooltipTexture = windowTooltip;
  tooltipTextureW = windowTooltipW;
  tooltipTextureH = windowTooltipH;
  sprites.UseRenderer(windowRenderer);
  AutoMapPaint_Use(windowRenderer);
  mapRenderer = windowRenderer;
  SDL_DestroyRenderer(renderer);
  SDL_FreeSurface(surface);
  return saved;
}

// ---------------------------------------------------------------------------------------------
// What the bar above the map asks and does (AutoMapBar.cpp)

bool AutoMapWindow_SearchIsOpen(void)
{
  return searchOpen;
}

void AutoMapWindow_ToggleSearch(void)
{
  if (searchOpen) SearchCloseBox();
  else SearchOpenBox();
  isDirty = true;
}

bool AutoMapWindow_DebugPanelIsOpen(void)
{
  return levelPanelOpen;
}

void AutoMapWindow_ToggleDebugPanel(void)
{
  if (!debugMenuOn) return;
  if (levelPanelOpen) LevelPanelClose();
  else LevelPanelOpen();
  isDirty = true;
}

void AutoMapWindow_StepLevel(int delta)
{
  AutoMapWindow_HandleLevelKey(delta > 0 ? SDLK_PAGEDOWN : SDLK_PAGEUP);
}

void AutoMapWindow_FollowParty(void)
{
  RecenterViewOnParty();
  AutoMap_FollowParty();
}

int AutoMapWindow_SearchHitsOnLevel(int level)
{
  if (!searchOpen || searchQuery.empty()) return 0;
  int hits = 0;
  for (size_t i = 0; i < searchHits.size(); i++) hits += searchHits[i].level == level ? 1 : 0;
  return hits;
}
