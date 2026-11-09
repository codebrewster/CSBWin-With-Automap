// AutoMapBar.cpp : see AutoMapBar.h
#include "stdafx.h"

#include <algorithm>
#include <string>
#include <vector>

#include "AutoMapLayout.h"
#include "AutoMapPaint.h"
#include "AutoMapBar.h"
#include "AutoMapExportDialog.h"
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
  ActionWalked,
  ActionSeen,
  ActionLearned,
  ActionAll,
  ActionShow,
  ActionSearch,
  ActionHelp,
  ActionExport,
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
static bool scrollLeft = false;
static bool scrollRight = false;
static int barWidth = 0;
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
  if (vision == 2) list.push_back(BarItem{"^TINT", ActionTint, AutoMap_ExploredIsHighlighted()});
  // The cells the party stood on, at every vision
  list.push_back(BarItem{"^WALKED", ActionWalked, AutoMap_WalkedIsHighlighted()});
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
  // The testing aids sit with / and ?, EXPORT in a group of its own behind them
  if (AutoMapWindow_DebugIsOn()) list.push_back(BarItem{"+", ActionDebug, AutoMapWindow_DebugPanelIsOpen()});
  list.push_back(BarItem{"|EXPORT", ActionExport, AutoMapExportDialog_IsOpen()});
  return list;
}

int AutoMapBar_Height(void)
{
  return AutoMapLayout_BarHeight(PIXEL);
}

// ---------------- the looks (map_bar_style) ----------------
enum BarLook
{
  LookStone,     // grey stone, each group in a black well, bevelled brass buttons (the save dialog)
  LookSegmented, // the classic colours, each group one framed control
  LookCyan,      // black and cyan, like the champion's action panel
  LookBrass,     // a dark bar, segmented groups, flat brass buttons
  LookClassic,   // separate buttons, captions as bare text
  LOOK_COUNT
};
static const char *const LOOK_NAMES[LOOK_COUNT] = {"stone", "segmented", "cyan", "brass", "classic"};
static int look = LookStone;

void AutoMapBar_SetStyle(const char *name)
{
  look = LookStone;
  if (name == NULL) return;
  for (int i = 0; i < LOOK_COUNT; i++)
  {
    if (SDL_strcasecmp(name, LOOK_NAMES[i]) == 0) look = i;
  }
}

const char *AutoMapBar_StyleName(void)
{
  return LOOK_NAMES[look];
}

static SDL_Color Rgb(int rgb)
{
  SDL_Color colour = {(Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 0xff};
  return colour;
}

static AutoMapBarSpacing Spacing(void)
{
  if (look == LookClassic) return AutoMapLayout_ClassicBarSpacing(PIXEL);
  AutoMapBarSpacing spacing = {1, 5 * PIXEL, 2 * PIXEL, 4 * PIXEL, look == LookStone ? 2 : 1};
  return spacing;
}

static AutoMapRect Grown(const AutoMapRect &r, int by)
{
  AutoMapRect grown = {r.x - by, r.y - by, r.w + 2 * by, r.h + 2 * by};
  return grown;
}

static SDL_Color BarFill(void)
{
  switch (look)
  {
  case LookStone: return Rgb(0x6a6d6a);
  case LookCyan: return Rgb(0x000000);
  case LookBrass: return Rgb(0x1f1c18);
  default: return BAR_FILL;
  }
}

static SDL_Color ArrowColour(void)
{
  switch (look)
  {
  case LookStone:
  case LookBrass: return Rgb(0xffb500);
  case LookCyan: return Rgb(0x00dade);
  default: return GOLD;
  }
}

static void DrawBarBack(int width)
{
  int h = AutoMapBar_Height();
  AutoMapRect bar = {0, 0, width, h};
  AutoMapPaint_Fill(bar, BarFill());
  AutoMapRect bottom = {0, h - 1, width, 1};
  if (look == LookStone)
  {
    AutoMapRect top = {0, 0, width, 1};
    AutoMapRect shade = {0, h - 2, width, 1};
    AutoMapPaint_Fill(top, Rgb(0x949194));
    AutoMapPaint_Fill(shade, Rgb(0x4a484a));
    AutoMapPaint_Fill(bottom, Rgb(0x000000));
  }
  else AutoMapPaint_Fill(bottom, BAR_LINE);
}

// Behind a group's cells: the frame, which shows between the cells as their dividers
static void DrawGroupBack(const AutoMapRect &r)
{
  switch (look)
  {
  case LookStone:
  {
    // Sunk into the stone: shade at the top and left, light at the bottom and right
    AutoMapRect well = Grown(r, 2);
    AutoMapPaint_Fill(well, Rgb(0x949194));
    well.w -= 1;
    well.h -= 1;
    AutoMapPaint_Fill(well, Rgb(0x4a484a));
    AutoMapPaint_Fill(Grown(r, 1), Rgb(0x000000));
    break;
  }
  case LookSegmented: AutoMapPaint_Fill(Grown(r, 1), Rgb(0x5d5548)); break;
  case LookCyan: AutoMapPaint_Fill(Grown(r, 1), Rgb(0x00dade)); break;
  case LookBrass: AutoMapPaint_Fill(Grown(r, 1), Rgb(0x000000)); break;
  default: break;
  }
}

static void DrawCaption(const AutoMapRect &r, const std::string &label)
{
  int y = r.y + 2 * PIXEL;
  switch (look)
  {
  case LookStone: AutoMapPaint_Text(r.x + 2 * PIXEL, y, label, Rgb(0x949194), PIXEL); break;
  case LookSegmented:
    AutoMapPaint_Fill(r, Rgb(0x3a352d));
    AutoMapPaint_Text(r.x + 2 * PIXEL, y, label, Rgb(0xb3a78f), PIXEL);
    break;
  case LookCyan:
    AutoMapPaint_Fill(r, Rgb(0x00494a));
    AutoMapPaint_Text(r.x + 2 * PIXEL, y, label, Rgb(0x7fecee), PIXEL);
    break;
  case LookBrass:
    AutoMapPaint_Fill(r, Rgb(0x2b2824));
    AutoMapPaint_Text(r.x + 2 * PIXEL, y, label, Rgb(0xa8a39a), PIXEL);
    break;
  default: AutoMapPaint_Text(r.x, y, label, CAPTION_TEXT, PIXEL); break;
  }
}

static void Bevel(const AutoMapRect &r, SDL_Color light, SDL_Color dark)
{
  int t = std::max(1, PIXEL / 2);
  AutoMapRect top = {r.x, r.y, r.w, t};
  AutoMapRect left = {r.x, r.y, t, r.h};
  AutoMapRect bottom = {r.x, r.y + r.h - t, r.w, t};
  AutoMapRect right = {r.x + r.w - t, r.y, t, r.h};
  AutoMapPaint_Fill(top, light);
  AutoMapPaint_Fill(left, light);
  AutoMapPaint_Fill(bottom, dark);
  AutoMapPaint_Fill(right, dark);
}

void AutoMapBar_DrawWell(const AutoMapRect &r)
{
  DrawGroupBack(r);
}

int AutoMapBar_ButtonWidth(const std::string &label)
{
  return AutoMapPaint_TextWidth(label, PIXEL) + 5 * PIXEL;
}

int AutoMapBar_ButtonHeight(void)
{
  return 9 * PIXEL;
}

static void DrawButton(const AutoMapRect &r, const std::string &label, bool on);

void AutoMapBar_DrawButton(const AutoMapRect &r, const std::string &label, bool on)
{
  DrawButton(r, label, on);
}

static void DrawButton(const AutoMapRect &r, const std::string &label, bool on)
{
  SDL_Color text = BUTTON_TEXT;
  switch (look)
  {
  case LookStone:
    // A choice that is on is a gold key pressed in: its bevel turned around
    AutoMapPaint_Fill(r, on ? Rgb(0xffb500) : Rgb(0x944800));
    if (on) Bevel(r, Rgb(0xa86e00), Rgb(0xffe08a));
    else Bevel(r, Rgb(0xde916a), Rgb(0x6a2400));
    text = on ? Rgb(0x5a1e00) : Rgb(0xffb500);
    break;
  case LookSegmented:
    AutoMapPaint_Fill(r, on ? ON_FILL : BUTTON_FILL);
    text = on ? ON_TEXT : BUTTON_TEXT;
    break;
  case LookCyan:
    AutoMapPaint_Fill(r, on ? Rgb(0x00dade) : Rgb(0x000000));
    text = on ? Rgb(0x000000) : Rgb(0x00dade);
    break;
  case LookBrass:
  {
    AutoMapPaint_Fill(r, on ? Rgb(0xffb500) : Rgb(0x6e3600));
    AutoMapRect top = {r.x, r.y, r.w, 1};
    AutoMapPaint_Fill(top, on ? Rgb(0xffe08a) : Rgb(0xa8603a));
    text = on ? Rgb(0x4a1a00) : Rgb(0xffb500);
    break;
  }
  default:
    AutoMapPaint_Fill(r, on ? ON_FILL : BUTTON_FILL);
    AutoMapPaint_Frame(r, on ? ON_FILL : BUTTON_FRAME, 1);
    text = on ? ON_TEXT : BUTTON_TEXT;
    break;
  }
  AutoMapPaint_Text(r.x + 5 * PIXEL / 2, r.y + 2 * PIXEL, label, text, PIXEL);
}

void AutoMapBar_Draw(int width)
{
  items = ItemsNow();
  std::vector<std::string> labels, shown;
  for (size_t i = 0; i < items.size(); i++) labels.push_back(items[i].label);
  AutoMapBarSpacing spacing = Spacing();
  itemRects = AutoMapLayout_PackBarSpaced(labels, width, PIXEL, spacing, &shown);
  int reach = itemRects.empty() ? 0
                                : itemRects.back().x + itemRects.back().w + spacing.groupFrame + 3 * PIXEL;
  barScroll = std::max(0, std::min(barScroll, reach - width));
  DrawBarBack(width);
  for (size_t i = 0; i < itemRects.size(); i++)
  {
    itemRects[i].x -= barScroll;
    const AutoMapRect &r = itemRects[i];
    if (items[i].action == ActionShow) showButtonX = r.x;
    if (items[i].action == ActionLevelList) levelButtonX = r.x;
    if (items[i].action == ActionSearch) searchButtonX = r.x + r.w / 2;
  }
  // The groups' frames first: a group starts at a label marked '|' or '^'
  for (size_t first = 0; first < itemRects.size();)
  {
    size_t end = first + 1;
    while (end < itemRects.size() && labels[end][0] != '|' && labels[end][0] != '^') end++;
    AutoMapRect span = itemRects[first];
    span.w = itemRects[end - 1].x + itemRects[end - 1].w - span.x;
    DrawGroupBack(span);
    first = end;
  }
  for (size_t i = 0; i < itemRects.size(); i++)
  {
    if (items[i].action == ActionNone) DrawCaption(itemRects[i], shown[i]);
    else DrawButton(itemRects[i], shown[i], items[i].on);
  }
  // The bar reaches past the window: arrows at the clipped ends scroll it
  scrollLeft = barScroll > 0;
  scrollRight = reach - barScroll > width;
  barWidth = width;
  int arrowW = 9 * PIXEL;
  int arrowH = AutoMapBar_Height() - (look == LookStone ? 3 : 1);
  int arrowTop = look == LookStone ? 1 : 0;
  if (scrollLeft)
  {
    AutoMapRect arrow = {0, arrowTop, arrowW, arrowH};
    AutoMapPaint_Fill(arrow, BarFill());
    AutoMapPaint_Text(3 * PIXEL, 4 * PIXEL, "<", ArrowColour(), PIXEL);
  }
  if (scrollRight)
  {
    AutoMapRect arrow = {width - arrowW, arrowTop, arrowW, arrowH};
    AutoMapPaint_Fill(arrow, BarFill());
    AutoMapPaint_Text(width - arrowW + 3 * PIXEL, 4 * PIXEL, ">", ArrowColour(), PIXEL);
  }
}

// ---------------- SHOW ----------------
struct ShowRow
{
  const char *label;
  int what;
};

// The row that cycles how a pit is drawn; it has no box
static const int PITS_ROW = -3;

static const char *PitStyleName(void)
{
  switch (AutoMapWindow_PitStyle())
  {
  case AutoMapPitStylePicture: return "PICTURE";
  case AutoMapPitStyleStripes: return "STRIPES";
  default: return "AUTO";
  }
}

static const ShowRow SHOW_ROWS[] = {
    {"SHOW ALL", -1}, {"HIDE ALL", -2}, {NULL, 0},
    {"WEAPONS", AutoMapShowWeapons}, {"ARMOUR", AutoMapShowArmour}, {"POTIONS", AutoMapShowPotions},
    {"SCROLLS", AutoMapShowScrolls}, {"CHESTS", AutoMapShowChests}, {"FOOD", AutoMapShowFood},
    {"KEYS", AutoMapShowKeys}, {"OTHER ITEMS", AutoMapShowOtherItems}, {NULL, 0},
    {"MONSTERS", AutoMapShowMonsters}, {NULL, 0},
    {"WALL DECORATIONS", AutoMapShowWallDecorations}, {"WALL WRITING", AutoMapShowWallWriting},
    {"FLOOR DECORATIONS", AutoMapShowFloorDecorations}, {NULL, 0}, {"PITS", PITS_ROW}};
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
    std::string label = SHOW_ROWS[i].label;
    if (SHOW_ROWS[i].what == PITS_ROW) label = "PITS: " + std::string(PitStyleName());
    AutoMapPaint_Text(panel.x + (SHOW_ROWS[i].what == PITS_ROW ? 8 : 24), y + 3, label, box && !on ? GREY : WHITE, PIXEL);
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
      if (what == PITS_ROW)
        AutoMapWindow_SetPitStyle((AutoMapPitStyle)((AutoMapWindow_PitStyle() + 1) % AUTOMAP_PIT_STYLE_COUNT));
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
    {"CTRL+WHEEL", "SIZE OF THE TOOLTIP"},
    {"CTRL+CLICK", "ON A STAIR: COME OUT ON THE OTHER LEVEL"},
    {"RIGHT-CLICK", "MARKS, LINKS, ASCEND / DESCEND"},
    {"", ""},
    {"", "CLICK ANYWHERE TO CLOSE"}};
static const int HELP_ROWS = sizeof(HELP) / sizeof(HELP[0]);

static void DrawHelp(int width)
{
  // Measured from the rows, so a longer line never runs out of the box
  int keyW = 0, textW = 0;
  for (int i = 0; i < HELP_ROWS; i++)
  {
    keyW = std::max(keyW, AutoMapPaint_TextWidth(HELP[i][0], PIXEL));
    textW = std::max(textW, AutoMapPaint_TextWidth(HELP[i][1], PIXEL));
  }
  keyW += 20;
  int w = 10 + keyW + textW + 10;
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
  if (AutoMapExportDialog_IsOpen()) AutoMapExportDialog_Draw(width, height);
}

bool AutoMapBar_PanelIsOpen(void)
{
  return showOpen || helpOpen || levelListOpen || AutoMapExportDialog_IsOpen();
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
  case ActionWalked: AutoMap_SetWalkedHighlight(!AutoMap_WalkedIsHighlighted()); break;
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
  case ActionExport:
    AutoMapBar_ClosePanels();
    if (AutoMapExportDialog_IsOpen()) AutoMapExportDialog_Close();
    else AutoMapExportDialog_Open();
    break;
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
  // The export dialog takes every press below the bar, and on the bar only its own button
  if (AutoMapExportDialog_IsOpen() && y >= AutoMapBar_Height()) return AutoMapExportDialog_HandlePress(x, y);
  if (y >= AutoMapBar_Height()) return false;
  int arrowW = 9 * PIXEL;
  if (scrollLeft && x < arrowW)
  {
    barScroll = std::max(0, barScroll - width / 2);
    return true;
  }
  if (scrollRight && x >= barWidth - arrowW)
  {
    barScroll += width / 2;
    return true;
  }
  for (size_t i = 0; i < itemRects.size() && i < items.size(); i++)
  {
    const AutoMapRect &r = itemRects[i];
    if (items[i].action == ActionNone) continue;
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
    {
      if (AutoMapExportDialog_IsOpen() && items[i].action != ActionExport) return true;
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
  // The dialog's keys come with their modifiers and its typing (AutoMapWindow_HandleKeyEvent)
  if (AutoMapExportDialog_IsOpen()) return false;
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

bool AutoMapBar_LevelListIsOpen(void)
{
  return levelListOpen;
}
