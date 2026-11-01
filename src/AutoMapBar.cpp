// AutoMapBar.cpp : see AutoMapBar.h
#include "stdafx.h"

#include <algorithm>
#include <string>
#include <vector>

#include "AutoMapLayout.h"
#include "AutoMapPaint.h"
#include "AutoMapBar.h"
#include "AutoMapHooks.h"
#include "AutoMapWindow.h"

static const int PIXEL = 2;
static const SDL_Color BAR_FILL = {0x24, 0x22, 0x1f, 0xff};
static const SDL_Color BAR_LINE = {0x3a, 0x36, 0x2f, 0xff};
static const SDL_Color BUTTON_FILL = {0x1d, 0x1a, 0x16, 0xff};
static const SDL_Color BUTTON_FRAME = {0x4a, 0x44, 0x3a, 0xff};
static const SDL_Color BUTTON_TEXT = {0xd8, 0xcf, 0xbd, 0xff};
static const SDL_Color CAPTION_TEXT = {0x8f, 0x86, 0x76, 0xff};
static const SDL_Color ON_FILL = {0xff, 0xcc, 0x33, 0xff};
static const SDL_Color ON_TEXT = {0x1a, 0x16, 0x0f, 0xff};
static const SDL_Color PANEL_FILL = {0x11, 0x11, 0x11, 0xff};
static const SDL_Color PANEL_FRAME = {0xff, 0xc8, 0x3a, 0xff};
static const SDL_Color WHITE = {0xff, 0xff, 0xff, 0xff};
static const SDL_Color GOLD = {0xff, 0xcc, 0x33, 0xff};
static const SDL_Color GREY = {0x8f, 0x86, 0x76, 0xff};
static const SDL_Color DIVIDER = {0x3a, 0x36, 0x2f, 0xff};

enum BarAction
{
  ActionNone,
  ActionLevelDown,
  ActionLevelList,
  ActionLevelUp,
  ActionFollow,
  ActionVisionFog,
  ActionVisionLive,
  ActionVisionWhole,
  ActionTint,
  ActionSeen,
  ActionLearned,
  ActionAll,
  ActionShow,
  ActionSearch,
  ActionHelp,
  ActionDebug
};

struct BarItem
{
  std::string label;
  BarAction action;
  bool on;
};

static bool showOpen = false;
static bool helpOpen = false;
// The level list (drawn from Task 12 on)
static bool levelListOpen = false;
static int levelListTop = 0;
static std::string typedLevel;
static int levelButtonX = 0;
static int searchButtonX = 0;
static const int LEVEL_ROWS = 12;
static int barScroll = 0;
static std::vector<AutoMapRect> itemRects;
static std::vector<BarItem> items;
static int showButtonX = 0;

// Spoilers as the setting numbers them (map_spoilers): 0 seen, 1 learned, 2 all
static std::vector<BarItem> ItemsNow(void)
{
  std::vector<BarItem> list;
  int vision = AutoMap_Vision();
  int spoilers = AutoMap_SpoilerLevel();
  int level = AutoMap_ShownLevel();
  int hidden = 0;
  for (int what = 0; what < AUTOMAP_SHOWN_COUNT; what++)
  {
    if (!AutoMapWindow_Shows((AutoMapShown)what)) hidden++;
  }
  BarItem entries[] = {
      {"<", ActionLevelDown, false},
      {"L" + std::to_string(level < 0 ? 0 : level) + " ~", ActionLevelList, levelListOpen},
      {">", ActionLevelUp, false},
      {"|FOLLOW", ActionFollow, AutoMap_FollowsParty()},
      {"|#VISION", ActionNone, false},
      {"FOG", ActionVisionFog, vision == 0},
      {"EXPLORED", ActionVisionLive, vision == 1},
      {"ALL", ActionVisionWhole, vision == 2},
  };
  list.assign(entries, entries + sizeof(entries) / sizeof(entries[0]));
  if (vision == 2) list.push_back(BarItem{"TINT", ActionTint, AutoMap_ExploredIsHighlighted()});
  BarItem rest[] = {
      {"|#SPOILERS", ActionNone, false},
      {"SEEN", ActionSeen, spoilers == 0},
      {"LEARNED", ActionLearned, spoilers == 1},
      {"ALL", ActionAll, spoilers == 2},
      {hidden ? "|SHOW: " + std::to_string(hidden) + " HIDDEN" : "|SHOW", ActionShow, showOpen},
      {"|/", ActionSearch, AutoMapWindow_SearchIsOpen()},
      {"?", ActionHelp, helpOpen},
  };
  list.insert(list.end(), rest, rest + sizeof(rest) / sizeof(rest[0]));
  if (AutoMapWindow_DebugIsOn()) list.push_back(BarItem{"+", ActionDebug, AutoMapWindow_DebugPanelIsOpen()});
  return list;
}

int AutoMapBar_Height(void)
{
  return AutoMapLayout_BarHeight(PIXEL);
}

void AutoMapBar_Draw(int width)
{
  items = ItemsNow();
  std::vector<std::string> labels, shown;
  for (size_t i = 0; i < items.size(); i++) labels.push_back(items[i].label);
  itemRects = AutoMapLayout_PackBar(labels, width, PIXEL, &shown);
  int reach = itemRects.empty() ? 0 : itemRects.back().x + itemRects.back().w + 3 * PIXEL;
  barScroll = std::max(0, std::min(barScroll, reach - width));
  AutoMapRect bar = {0, 0, width, AutoMapBar_Height()};
  AutoMapPaint_Fill(bar, BAR_FILL);
  AutoMapRect line = {0, AutoMapBar_Height() - 1, width, 1};
  AutoMapPaint_Fill(line, BAR_LINE);
  for (size_t i = 0; i < itemRects.size(); i++)
  {
    itemRects[i].x -= barScroll;
    const AutoMapRect &r = itemRects[i];
    if (items[i].action == ActionShow) showButtonX = r.x;
    if (items[i].action == ActionLevelList) levelButtonX = r.x;
    if (items[i].action == ActionSearch) searchButtonX = r.x + r.w / 2;
    if (items[i].action == ActionNone)
    {
      AutoMapPaint_Text(r.x, r.y + 2 * PIXEL, shown[i], CAPTION_TEXT, PIXEL);
      continue;
    }
    AutoMapPaint_Fill(r, items[i].on ? ON_FILL : BUTTON_FILL);
    AutoMapPaint_Frame(r, items[i].on ? ON_FILL : BUTTON_FRAME, 1);
    AutoMapPaint_Text(r.x + 5 * PIXEL / 2, r.y + 2 * PIXEL, shown[i], items[i].on ? ON_TEXT : BUTTON_TEXT, PIXEL);
  }
}

// ---------------- SHOW ----------------
struct ShowRow
{
  const char *label;
  int what;
};

static const ShowRow SHOW_ROWS[] = {
    {"SHOW ALL", -1}, {"HIDE ALL", -2}, {NULL, 0},
    {"WEAPONS", AutoMapShowWeapons}, {"ARMOUR", AutoMapShowArmour}, {"POTIONS", AutoMapShowPotions},
    {"SCROLLS", AutoMapShowScrolls}, {"CHESTS", AutoMapShowChests}, {"FOOD", AutoMapShowFood},
    {"KEYS", AutoMapShowKeys}, {"OTHER ITEMS", AutoMapShowOtherItems}, {NULL, 0},
    {"MONSTERS", AutoMapShowMonsters}, {NULL, 0},
    {"WALL DECORATIONS", AutoMapShowWallDecorations}, {"WALL WRITING", AutoMapShowWallWriting},
    {"FLOOR DECORATIONS", AutoMapShowFloorDecorations}};
static const int SHOW_ROW_COUNT = sizeof(SHOW_ROWS) / sizeof(SHOW_ROWS[0]);
static const int ROW = 16;
static const int DIVIDER_ROW = 6;

static AutoMapRect ShowPanel(int width)
{
  int w = AutoMapPaint_TextWidth("X FLOOR DECORATIONS", PIXEL) + 24;
  int h = 10;
  for (int i = 0; i < SHOW_ROW_COUNT; i++) h += SHOW_ROWS[i].label ? ROW : DIVIDER_ROW;
  AutoMapRect panel = {std::max(4, std::min(showButtonX, width - w - 4)), AutoMapBar_Height() + 2, w, h};
  return panel;
}

static void DrawShowPanel(int width)
{
  AutoMapRect panel = ShowPanel(width);
  AutoMapPaint_Fill(panel, PANEL_FILL);
  AutoMapPaint_Frame(panel, PANEL_FRAME, 1);
  int y = panel.y + 5;
  for (int i = 0; i < SHOW_ROW_COUNT; i++)
  {
    if (!SHOW_ROWS[i].label)
    {
      AutoMapRect line = {panel.x + 6, y + 2, panel.w - 12, 1};
      AutoMapPaint_Fill(line, DIVIDER);
      y += DIVIDER_ROW;
      continue;
    }
    bool box = SHOW_ROWS[i].what >= 0;
    bool on = box && AutoMapWindow_Shows((AutoMapShown)SHOW_ROWS[i].what);
    if (box)
    {
      AutoMapRect frame = {panel.x + 8, y + 2, 10, 10};
      AutoMapPaint_Frame(frame, GREY, 1);
      AutoMapRect tick = {panel.x + 10, y + 4, 6, 6};
      if (on) AutoMapPaint_Fill(tick, GOLD);
    }
    AutoMapPaint_Text(panel.x + 24, y + 3, SHOW_ROWS[i].label, box && !on ? GREY : WHITE, PIXEL);
    y += ROW;
  }
}

static bool ShowPanelPress(int x, int y, int width)
{
  AutoMapRect panel = ShowPanel(width);
  if (x < panel.x || x >= panel.x + panel.w || y < panel.y || y >= panel.y + panel.h)
  {
    showOpen = false;
    return true;
  }
  int rowY = panel.y + 5;
  for (int i = 0; i < SHOW_ROW_COUNT; i++)
  {
    int h = SHOW_ROWS[i].label ? ROW : DIVIDER_ROW;
    if (SHOW_ROWS[i].label && y >= rowY && y < rowY + h)
    {
      int what = SHOW_ROWS[i].what;
      for (int k = 0; k < AUTOMAP_SHOWN_COUNT; k++)
      {
        if (what == -1) AutoMapWindow_SetShows((AutoMapShown)k, true);
        if (what == -2) AutoMapWindow_SetShows((AutoMapShown)k, false);
      }
      if (what >= 0) AutoMapWindow_SetShows((AutoMapShown)what, !AutoMapWindow_Shows((AutoMapShown)what));
    }
    rowY += h;
  }
  return true;
}

// ---------------- controls help ----------------
static const char *const HELP[][2] = {
    {"LEFT-DRAG", "PAN THE MAP"},
    {"WHEEL", "ZOOM TOWARDS THE POINTER"},
    {"MIDDLE-CLICK", "CENTRE ON THE PARTY AND FOLLOW"},
    {"SIDE BUTTONS", "LEVEL DOWN / UP UNDER THE POINTER"},
    {"PAGE DOWN / UP", "LEVEL DOWN / UP (CTRL IN THE GAME)"},
    {"/ OR CTRL+F", "SEARCH ALL LEVELS"},
    {"HOVER", "WHAT IS THERE, AND ITS LINKS"},
    {"SHIFT", "THE WHOLE PRESS CHAIN (SPOILERS ALL)"},
    {"SHIFT+WHEEL", "SCROLL A LONG TOOLTIP"},
    {"RIGHT-CLICK", "YOUR OWN MARKS AND LINKS"},
    {"", ""},
    {"", "CLICK ANYWHERE TO CLOSE"}};
static const int HELP_ROWS = sizeof(HELP) / sizeof(HELP[0]);

static void DrawHelp(int width)
{
  int keyW = AutoMapPaint_TextWidth("PAGE DOWN / UP", PIXEL) + 20;
  int w = keyW + AutoMapPaint_TextWidth("LEVEL DOWN / UP UNDER THE POINTER", PIXEL) + 20;
  int h = HELP_ROWS * ROW + 30;
  AutoMapRect panel = {std::max(4, (width - w) / 2), AutoMapBar_Height() + 30, w, h};
  AutoMapPaint_Fill(panel, PANEL_FILL);
  AutoMapPaint_Frame(panel, PANEL_FRAME, 1);
  AutoMapPaint_Text(panel.x + 10, panel.y + 8, "CONTROLS", GOLD, PIXEL);
  for (int i = 0; i < HELP_ROWS; i++)
  {
    AutoMapPaint_Text(panel.x + 10, panel.y + 26 + i * ROW, HELP[i][0], GOLD, PIXEL);
    AutoMapPaint_Text(panel.x + 10 + keyW, panel.y + 26 + i * ROW, HELP[i][1], WHITE, PIXEL);
  }
}

// ---------------- level list ----------------
static AutoMapRect LevelList(void)
{
  int levels = AutoMap_LevelCount();
  int rows = std::min(LEVEL_ROWS, std::max(1, levels));
  int w = AutoMapPaint_TextWidth("L000  NOT VISITED  000 HITS", PIXEL) + 34;
  int h = rows * ROW + 10 + (typedLevel.empty() ? 0 : 18);
  AutoMapRect panel = {std::max(4, levelButtonX - 4), AutoMapBar_Height() + 2, w, h};
  return panel;
}

static void PickLevel(int level)
{
  if (level < 0) return;
  if (level == AutoMap_PartyLevel()) AutoMapWindow_FollowParty();
  else AutoMap_ShowLevel(level);
  levelListOpen = false;
  typedLevel.clear();
}

static void DrawLevelList(void)
{
  int levels = AutoMap_LevelCount();
  AutoMapRect panel = LevelList();
  AutoMapPaint_Fill(panel, PANEL_FILL);
  AutoMapPaint_Frame(panel, PANEL_FRAME, 1);
  int y = panel.y + 5;
  if (!typedLevel.empty())
  {
    AutoMapPaint_Text(panel.x + 8, y + 2, "GO TO L" + typedLevel, GOLD, PIXEL);
    y += 18;
  }
  int shownLevel = AutoMap_ShownLevel();
  int nameW = AutoMapPaint_TextWidth("L000", PIXEL);
  for (int row = 0; row < LEVEL_ROWS; row++)
  {
    int level = levelListTop + row;
    if (level >= levels) break;
    bool selected = level == shownLevel;
    bool visited = AutoMap_LevelIsVisited(level);
    AutoMapRect line = {panel.x + 3, y, panel.w - 14, ROW};
    if (selected) AutoMapPaint_Fill(line, ON_FILL);
    SDL_Color text = selected ? ON_TEXT : (visited ? WHITE : GREY);
    AutoMapPaint_Text(panel.x + 8, y + 3, "L" + std::to_string(level), text, PIXEL);
    SDL_Color red = {0xff, 0x4d, 0x4d, 0xff};
    if (level == AutoMap_PartyLevel()) AutoMapPaint_PartyArrow(panel.x + 8 + nameW + 10, y + ROW / 2.0f, 5, 1, red, false);
    int x = panel.x + 8 + nameW + 22;
    if (!visited)
    {
      AutoMapPaint_Text(x, y + 3, "NOT VISITED", selected ? ON_TEXT : GREY, PIXEL);
      x += AutoMapPaint_TextWidth("NOT VISITED", PIXEL) + 10;
    }
    int hits = AutoMapWindow_SearchHitsOnLevel(level);
    if (hits > 0) AutoMapPaint_Text(x, y + 3, std::to_string(hits) + (hits > 1 ? " HITS" : " HIT"), selected ? ON_TEXT : GOLD, PIXEL);
    y += ROW;
  }
  if (levels > LEVEL_ROWS)
  {
    int track = panel.h - 10;
    int thumb = std::max(16, track * LEVEL_ROWS / levels);
    int thumbY = panel.y + 5 + (track - thumb) * levelListTop / std::max(1, levels - LEVEL_ROWS);
    AutoMapRect rail = {panel.x + panel.w - 8, panel.y + 5, 4, track};
    AutoMapRect knob = {rail.x, thumbY, 4, thumb};
    AutoMapPaint_Fill(rail, DIVIDER);
    AutoMapPaint_Fill(knob, GREY);
  }
}

static bool LevelListPress(int x, int y)
{
  AutoMapRect panel = LevelList();
  if (x < panel.x || x >= panel.x + panel.w || y < panel.y || y >= panel.y + panel.h)
  {
    levelListOpen = false;
    return true;
  }
  int rowsY = panel.y + 5 + (typedLevel.empty() ? 0 : 18);
  if (y < rowsY) return true;
  int level = levelListTop + (y - rowsY) / ROW;
  if (level < AutoMap_LevelCount()) PickLevel(level);
  return true;
}

void AutoMapBar_DrawPanels(int width, int height)
{
  (void)height;
  if (showOpen) DrawShowPanel(width);
  if (helpOpen) DrawHelp(width);
  if (levelListOpen) DrawLevelList();
}

bool AutoMapBar_PanelIsOpen(void)
{
  return showOpen || helpOpen || levelListOpen;
}

void AutoMapBar_ClosePanels(void)
{
  showOpen = false;
  helpOpen = false;
  levelListOpen = false;
}

static void Carry(BarAction action)
{
  int vision = AutoMap_Vision();
  switch (action)
  {
  case ActionLevelDown: AutoMapWindow_StepLevel(-1); break;
  case ActionLevelUp: AutoMapWindow_StepLevel(1); break;
  case ActionLevelList:
  {
    bool open = !levelListOpen;
    AutoMapBar_ClosePanels();
    levelListOpen = open;
    typedLevel.clear();
    levelListTop = AutoMapLayout_LevelListTop(AutoMap_LevelCount(), LEVEL_ROWS, AutoMap_ShownLevel());
    break;
  }
  case ActionFollow:
    if (AutoMap_FollowsParty()) AutoMap_ShowLevel(AutoMap_ShownLevel());
    else AutoMapWindow_FollowParty();
    break;
  case ActionVisionFog: AutoMap_SetVision(0); break;
  case ActionVisionLive: AutoMap_SetVision(1); break;
  case ActionVisionWhole: AutoMap_SetVision(2); break;
  case ActionTint: if (vision == 2) AutoMap_SetExploredHighlight(!AutoMap_ExploredIsHighlighted()); break;
  case ActionSeen: AutoMap_SetSpoilerLevel(0); break;
  case ActionLearned: AutoMap_SetSpoilerLevel(1); break;
  case ActionAll: AutoMap_SetSpoilerLevel(2); break;
  case ActionShow:
  {
    bool open = !showOpen;
    AutoMapBar_ClosePanels();
    showOpen = open;
    break;
  }
  case ActionSearch: AutoMapWindow_ToggleSearch(); break;
  case ActionHelp:
  {
    bool open = !helpOpen;
    AutoMapBar_ClosePanels();
    helpOpen = open;
    break;
  }
  case ActionDebug: AutoMapWindow_ToggleDebugPanel(); break;
  default: break;
  }
}

bool AutoMapBar_HandlePress(int x, int y, int width)
{
  if (helpOpen)
  {
    helpOpen = false;
    return true;
  }
  if (showOpen && y >= AutoMapBar_Height()) return ShowPanelPress(x, y, width);
  if (levelListOpen && y >= AutoMapBar_Height()) return LevelListPress(x, y);
  if (y >= AutoMapBar_Height()) return false;
  for (size_t i = 0; i < itemRects.size() && i < items.size(); i++)
  {
    const AutoMapRect &r = itemRects[i];
    if (items[i].action == ActionNone) continue;
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
    {
      Carry(items[i].action);
      return true;
    }
  }
  return true;
}

bool AutoMapBar_HandleWheel(int x, int y, int notches)
{
  if (levelListOpen)
  {
    AutoMapRect panel = LevelList();
    if (x >= panel.x && x < panel.x + panel.w && y >= panel.y && y < panel.y + panel.h)
    {
      int most = std::max(0, AutoMap_LevelCount() - LEVEL_ROWS);
      levelListTop = std::max(0, std::min(most, levelListTop - notches * 3));
      return true;
    }
  }
  if (y >= AutoMapBar_Height()) return false;
  barScroll = std::max(0, barScroll - notches * 24);
  return true;
}

bool AutoMapBar_HandleKey(int sym)
{
  if (!AutoMapBar_PanelIsOpen()) return false;
  if (sym == SDLK_ESCAPE)
  {
    AutoMapBar_ClosePanels();
    return true;
  }
  if (!levelListOpen) return false;
  int digit = -1;
  if (sym >= SDLK_0 && sym <= SDLK_9) digit = sym - SDLK_0;
  if (sym >= SDLK_KP_1 && sym <= SDLK_KP_9) digit = sym - SDLK_KP_1 + 1;
  if (sym == SDLK_KP_0) digit = 0;
  int levels = AutoMap_LevelCount();
  if (digit >= 0)
  {
    typedLevel += (char)('0' + digit);
    if (typedLevel.size() > 3) typedLevel.erase(0, 1);
    levelListTop = AutoMapLayout_LevelListTop(levels, LEVEL_ROWS, AutoMapLayout_TypedLevel(typedLevel, levels));
    return true;
  }
  if (sym == SDLK_BACKSPACE && !typedLevel.empty())
  {
    typedLevel.erase(typedLevel.size() - 1);
    return true;
  }
  if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER)
  {
    PickLevel(AutoMapLayout_TypedLevel(typedLevel, levels));
    return true;
  }
  return true;
}

int AutoMapBar_SearchButtonX(void)
{
  return searchButtonX;
}
