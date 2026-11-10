// HeroStatsDraw.cpp : the pages of the Statistics window, drawn with Dear ImGui. Every page is
// made from HeroStatsCatalogue through HeroStatsPages; nothing here counts anything.
#include "stdafx.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include "imgui.h"
#include "imgui_internal.h"
#include "HeroStatsLayout.h"
#include "HeroStatsDraw.h"
#include "HeroStatsHooks.h"
#include "HeroStatsPages.h"

// ---------------------------------------------------------------------------------------------
// Colours

static const ImU32 BACKGROUND_RGB = 0x1e1f24;
static const ImU32 PANEL_RGB = 0x272930;
static const ImU32 PANEL_HOVERED_RGB = 0x2e3139;
static const ImU32 TITLE_RGB = 0x294a7a;
static const ImU32 CRUMB_RGB = 0xb9c9e4;
static const ImU32 WHITE_RGB = 0xffffff;
static const ImU32 TEXT_RGB = 0xd8d8d8;
static const ImU32 PART_RGB = 0xb4b6bf;       // a row that is a part of the row above
static const ImU32 DIM_RGB = 0x8a8d98;        // labels, summaries, table headers
static const ImU32 FAINT_RGB = 0x5a5d68;      // the dot of a zero
static const ImU32 BUTTON_RGB = 0x2c3e5c;
static const ImU32 BUTTON_HOVERED_RGB = 0x36507a;
static const ImU32 BUTTON_ON_RGB = 0x4272b8;  // what is picked
static const ImU32 BORDER_RGB = 0x3a3d46;
static const ImU32 LINE_RGB = 0x31343d;
static const ImU32 ROW_LINE_RGB = 0x292b32;
static const ImU32 TRACK_RGB = 0x2f323a;      // the empty part of a bar
static const ImU32 BAD_RGB = 0xe5735f;
static const ImU32 YELLOW_RGB = 0xe8d44a;     // food and water below 0
static const ImU32 FOOD_RGB = 0x9b6a3c;       // the food bar, brown as in the game
static const ImU32 WATER_RGB = 0x4a8fe0;      // the water bar
static const ImU32 GOLD_RGB = 0xe0b54a;       // a champion's record that is the party's
// The game's own champion colours, by party slot, softened for the dark background
static const ImU32 CHAMPION_RGB[4] = {0x5fbf5f, 0xd8c447, 0xd9604f, 0x5b8fd9};

static const float GUTTER = 8;
static const char *const MIDDLE_DOT = "\xc2\xb7";

static ImVec4 Colour(ImU32 rgb, float alpha = 1.0f)
{
  return ImVec4(((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f, (rgb & 0xff) / 255.0f, alpha);
}

static ImU32 Packed(ImU32 rgb, float alpha = 1.0f)
{
  return ImGui::GetColorU32(Colour(rgb, alpha));
}

static ImU32 ChampionRgb(size_t column)
{
  return CHAMPION_RGB[column % 4];
}

// The record star and the arrow. Neither ImGui's own font nor Vera Mono has glyphs for them,
// so text holding one is drawn in pieces with the symbol painted between them.
static const char *const STAR = "\xe2\x98\x85";
static const char *const ARROW = "\xe2\x86\x92";
static const size_t SYMBOL_BYTES = 3; // both are three bytes in UTF-8

// The next painted symbol in the text from a position, or npos
static size_t NextSymbol(const std::string &text, size_t from)
{
  return std::min(text.find(STAR, from), text.find(ARROW, from));
}

static bool IsStar(const std::string &text, size_t at)
{
  return text.compare(at, SYMBOL_BYTES, STAR) == 0;
}

static float SymbolWidth(float fontSize, bool star)
{
  return fontSize * (star ? 0.75f : 0.9f);
}

static float TextPieceWidth(float fontSize, const std::string &text, size_t from, size_t end)
{
  return ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str() + from, text.c_str() + end).x;
}

static float TextWidthWithSymbols(float fontSize, const std::string &text)
{
  float width = 0;
  size_t from = 0;
  for (;;)
  {
    size_t symbol = NextSymbol(text, from);
    width += TextPieceWidth(fontSize, text, from, symbol == std::string::npos ? text.size() : symbol);
    if (symbol == std::string::npos) return width;
    width += SymbolWidth(fontSize, IsStar(text, symbol));
    from = symbol + SYMBOL_BYTES;
  }
}

static void DrawStar(ImDrawList *drawList, ImVec2 centre, float radius, ImU32 colour)
{
  ImVec2 points[10];
  for (int i = 0; i < 10; i++)
  {
    float r = (i % 2) == 0 ? radius : radius * 0.42f;
    float angle = -1.5707963f + i * 0.62831853f; // from the top, a tenth of a turn each
    points[i] = ImVec2(centre.x + r * cosf(angle), centre.y + r * sinf(angle));
  }
  // Concave: a fan from the centre over each spike keeps it filled
  for (int i = 0; i < 10; i++) drawList->AddTriangleFilled(centre, points[i], points[(i + 1) % 10], colour);
}

// A right arrow across the box, a little in from both ends
static void DrawArrow(ImDrawList *drawList, ImVec2 min, float width, float fontSize, ImU32 colour)
{
  float y = min.y + fontSize * 0.5f;
  float left = min.x + width * 0.15f, right = min.x + width * 0.85f;
  float head = fontSize * 0.25f;
  float thickness = std::max(1.0f, fontSize / 12.0f);
  drawList->AddLine(ImVec2(left, y), ImVec2(right - head * 0.5f, y), colour, thickness);
  drawList->AddTriangleFilled(ImVec2(right - head, y - head), ImVec2(right, y), ImVec2(right - head, y + head), colour);
}

static void TextWithSymbols(ImDrawList *drawList, float fontSize, ImVec2 at, ImU32 colour, const std::string &text)
{
  size_t from = 0;
  for (;;)
  {
    size_t symbol = NextSymbol(text, from);
    size_t end = symbol == std::string::npos ? text.size() : symbol;
    drawList->AddText(ImGui::GetFont(), fontSize, at, colour, text.c_str() + from, text.c_str() + end);
    at.x += TextPieceWidth(fontSize, text, from, end);
    if (symbol == std::string::npos) return;
    bool star = IsStar(text, symbol);
    float width = SymbolWidth(fontSize, star);
    if (star) DrawStar(drawList, ImVec2(at.x + width / 2, at.y + fontSize * 0.5f), width * 0.5f, colour);
    else DrawArrow(drawList, at, width, fontSize, colour);
    at.x += width;
    from = symbol + SYMBOL_BYTES;
  }
}

// TextColored for text that may hold a symbol: an item of its own, so hovering it still works
static void TextPackedWithSymbols(ImU32 colour, const std::string &text)
{
  float fontSize = ImGui::GetFontSize();
  ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(TextWidthWithSymbols(fontSize, text), ImGui::GetTextLineHeight()));
  TextWithSymbols(ImGui::GetWindowDrawList(), fontSize, at, colour, text);
}

static void TextColoredWithSymbols(ImU32 rgb, const std::string &text)
{
  TextPackedWithSymbols(Packed(rgb), text);
}

// SetTooltip for text that may hold a symbol
static void TooltipWithSymbols(const std::string &text)
{
  ImGui::BeginTooltip();
  TextPackedWithSymbols(ImGui::GetColorU32(ImGuiCol_Text), text);
  ImGui::EndTooltip();
}

void HeroStatsDraw_ApplyStyle(void)
{
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(0, 0);
  style.WindowBorderSize = 0;
  style.WindowRounding = 0;
  style.FramePadding = ImVec2(8, 3);
  style.FrameRounding = 3;
  style.ItemSpacing = ImVec2(4, 4);
  style.CellPadding = ImVec2(8, 3);
  style.Colors[ImGuiCol_WindowBg] = Colour(BACKGROUND_RGB);
  style.Colors[ImGuiCol_ChildBg] = Colour(BACKGROUND_RGB);
  style.Colors[ImGuiCol_PopupBg] = Colour(PANEL_RGB);
  style.Colors[ImGuiCol_Text] = Colour(TEXT_RGB);
  style.Colors[ImGuiCol_TextDisabled] = Colour(DIM_RGB);
  style.Colors[ImGuiCol_Border] = Colour(BORDER_RGB);
  style.Colors[ImGuiCol_Separator] = Colour(LINE_RGB);
  style.Colors[ImGuiCol_Button] = Colour(BUTTON_RGB);
  style.Colors[ImGuiCol_ButtonHovered] = Colour(BUTTON_HOVERED_RGB);
  style.Colors[ImGuiCol_ButtonActive] = Colour(BUTTON_ON_RGB);
  style.Colors[ImGuiCol_FrameBg] = Colour(PANEL_RGB);
  style.Colors[ImGuiCol_FrameBgHovered] = Colour(BUTTON_HOVERED_RGB);
  style.Colors[ImGuiCol_FrameBgActive] = Colour(BUTTON_ON_RGB);
  style.Colors[ImGuiCol_Header] = Colour(PANEL_RGB);
  style.Colors[ImGuiCol_HeaderHovered] = Colour(PANEL_HOVERED_RGB);
  style.Colors[ImGuiCol_HeaderActive] = Colour(PANEL_HOVERED_RGB);
  style.Colors[ImGuiCol_TableHeaderBg] = Colour(BACKGROUND_RGB);
  style.Colors[ImGuiCol_TableBorderLight] = Colour(ROW_LINE_RGB);
  style.Colors[ImGuiCol_TableBorderStrong] = Colour(LINE_RGB);
}

// ---------------------------------------------------------------------------------------------
// State

HeroStatsDrawState HeroStatsDraw_InitialState(void)
{
  HeroStatsDrawState state;
  state.who = HERO_STATS_PARTY;
  state.topic = HeroTopicOverview;
  state.level = HERO_STATS_WHOLE_RUN;
  state.perLevel = false;
  state.sideBySide = false;
  for (int t = 0; t < HERO_TOPIC_COUNT; t++)
  {
    std::vector<HeroStatsNumber> numbers = HeroStatsCatalogue_GridNumbers(t);
    state.gridNumber[t] = numbers.empty() ? HeroNumDamage : numbers[0];
  }
  state.closedSections = 0;
  state.inventoryList = false;
  return state;
}

// The bits csb.cfg's stats_closed keeps: the first eight detail tables, then Numbers (256) and Key
// numbers (512), then the tables that came later (Skills 1024 ... Records 8192)
int HeroStatsDraw_SectionBit(int detail)
{
  if (detail == HERO_STATS_ATTRIBUTES_SECTION) return 1 << 14;
  if (detail == HERO_STATS_RANKS_SECTION) return 1 << 15;
  if (detail == HERO_STATS_INVENTORY_SECTION) return 1 << 16;
  if (detail == HERO_STATS_NUMBERS_SECTION) return 1 << HeroDetailSkills;
  if (detail == HERO_STATS_KEY_NUMBERS_SECTION) return 1 << (HeroDetailSkills + 1);
  if (detail >= HeroDetailSkills) return 1 << (detail + 2);
  return 1 << detail;
}

static std::string LevelLabel(int level)
{
  return level == HERO_STATS_WHOLE_RUN ? "Whole run" : "Lv " + std::to_string(level);
}

static std::string WhoLabel(const std::vector<HeroStatsColumn> &columns, HeroStatsChampion who)
{
  for (size_t i = 0; i < columns.size(); i++)
    if (columns[i].id == who) return columns[i].name;
  return "Party";
}

// ---------------------------------------------------------------------------------------------
// Small parts

static float ButtonWidth(const char *label)
{
  return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
}

// A button of a group where one is picked (the level picker, Show, Number)
static bool ChoiceButton(const char *label, bool isOn)
{
  if (isOn)
  {
    ImGui::PushStyleColor(ImGuiCol_Button, Colour(BUTTON_ON_RGB));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colour(BUTTON_ON_RGB));
    ImGui::PushStyleColor(ImGuiCol_Text, Colour(WHITE_RGB));
  }
  else
  {
    ImGui::PushStyleColor(ImGuiCol_Button, Colour(PANEL_RGB));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colour(PANEL_HOVERED_RGB));
    ImGui::PushStyleColor(ImGuiCol_Text, Colour(DIM_RGB));
  }
  bool clicked = ImGui::Button(label);
  ImGui::PopStyleColor(3);
  return clicked;
}

static void ChampionDot(ImVec2 centre, size_t column, bool dimmed)
{
  ImGui::GetWindowDrawList()->AddCircleFilled(centre, 4.0f, Packed(ChampionRgb(column), dimmed ? 0.45f : 1.0f));
}

// Bad news: its text on a faint red pill, so that it reads apart from the red champion's colour
static void PillBehind(ImDrawList *drawList, ImVec2 at, ImVec2 size)
{
  drawList->AddRectFilled(ImVec2(at.x - 4, at.y - 1), ImVec2(at.x + size.x + 4, at.y + size.y + 1), Packed(BAD_RGB, 0.16f), 3.0f);
}

// ---------------------------------------------------------------------------------------------
// The title strip, who, the level picker and the topic tabs

static void DrawTitle(const HeroStatsDrawState &state, const std::vector<HeroStatsColumn> &columns,
                      void (*exportStatistics)(void))
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float width = ImGui::GetContentRegionAvail().x;
  float height = ImGui::GetTextLineHeight() + 8;
  drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), Packed(TITLE_RGB));
  drawList->AddText(ImVec2(origin.x + GUTTER, origin.y + 4), Packed(WHITE_RGB), "Statistics");
  // The strip gives way from its middle: the crumb shortens, then the button
  const char *exportLabel = "Export as HTML...";
  float titleEnd = origin.x + GUTTER + ImGui::CalcTextSize("Statistics").x + 16;
  if (titleEnd + ButtonWidth(exportLabel) + 4 > origin.x + width) exportLabel = "Export...";
  float exportX = origin.x + width - ButtonWidth(exportLabel) - 4;
  std::string crumb = WhoLabel(columns, state.who) + " \xc2\xb7 ";
  if (state.topic == HERO_STATS_CHAMPIONS_TAB) crumb += "Champions";  // the party now: no level
  else
    crumb += std::string(HeroStatsCatalogue_Topic(state.topic).label) + " \xc2\xb7 " +
             (state.perLevel ? std::string("Per level") : LevelLabel(state.level));
  // The Who buttons and the tabs say the same, so the crumb alone may be cut short
  float crumbRoom = exportX - 12 - titleEnd;
  std::string shown = crumb;
  while (!shown.empty() && ImGui::CalcTextSize((shown + (shown.size() < crumb.size() ? "..." : "")).c_str()).x > crumbRoom)
  {
    shown.erase(shown.size() - 1);
    while (!shown.empty() && ((unsigned char)shown[shown.size() - 1] & 0xc0) == 0x80) shown.erase(shown.size() - 1);  // a whole UTF-8 character
    if (!shown.empty() && ((unsigned char)shown[shown.size() - 1] & 0x80) != 0) shown.erase(shown.size() - 1);
  }
  if (shown.size() < crumb.size() && !shown.empty()) shown += "...";
  if (shown.size() > 3)
    drawList->AddText(ImVec2(exportX - 12 - ImGui::CalcTextSize(shown.c_str()).x, origin.y + 4), Packed(CRUMB_RGB), shown.c_str());
  ImGui::SetCursorScreenPos(ImVec2(exportX, origin.y + 2));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 1));
  if (ImGui::Button(exportLabel)) exportStatistics();
  ImGui::PopStyleVar();
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, height));
}

// A button of the who row: picked, it stands out on a panel with a lighter border
static bool WhoButton(const char *label, bool on, bool dimmed)
{
  ImGui::PushStyleColor(ImGuiCol_Button, Colour(on ? PANEL_RGB : BACKGROUND_RGB));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colour(PANEL_HOVERED_RGB));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colour(PANEL_HOVERED_RGB));
  ImGui::PushStyleColor(ImGuiCol_Border, Colour(on ? 0x6b7080 : BORDER_RGB));
  ImGui::PushStyleColor(ImGuiCol_Text, Colour(on ? WHITE_RGB : dimmed ? DIM_RGB : TEXT_RGB));
  bool clicked = ImGui::Button(label);
  ImGui::PopStyleColor(5);
  return clicked;
}

// Party, then a button per champion with their colour's dot; the width it took
static float DrawWhoRow(HeroStatsDrawState *state, const std::vector<HeroStatsColumn> &columns)
{
  float start = ImGui::GetCursorPosX();
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  if (WhoButton("Party", state->who == HERO_STATS_PARTY, false)) state->who = HERO_STATS_PARTY;
  for (size_t i = 0; i < columns.size(); i++)
  {
    std::string label = "   " + columns[i].name;  // room for the dot
    // A button that would cross the window's edge starts the next line
    ImGui::SameLine();
    if (ImGui::GetCursorPosX() + ButtonWidth(label.c_str()) > ImGui::GetWindowWidth() - GUTTER)
    {
      ImGui::NewLine();
      ImGui::SetCursorPosX(start);
    }
    ImGui::PushID((int)i);
    if (WhoButton(label.c_str(), state->who == columns[i].id, columns[i].hasLeft)) state->who = columns[i].id;
    ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ChampionDot(ImVec2(min.x + ImGui::GetStyle().FramePadding.x + 4, (min.y + max.y) / 2), i, columns[i].hasLeft);
    ImGui::PopID();
  }
  ImGui::PopStyleVar();
  ImGui::SameLine(0, 0);
  return ImGui::GetCursorPosX() - start;
}

static float PickerWidth(const HeroStatsPicker &picker)
{
  float spacing = 1;
  float width = ButtonWidth("Whole run") + spacing + ButtonWidth("Per level");
  if (picker.levels.empty()) return width;
  width += 8 + ImGui::CalcTextSize("Lv").x + 6;
  if (picker.stepper) return width + spacing * 3 + ImGui::GetFrameHeight() * 2 + 110;
  for (size_t i = 0; i < picker.levels.size(); i++) width += spacing + ButtonWidth(std::to_string(picker.levels[i]).c_str());
  return width;
}

static void DrawPicker(HeroStatsDrawState *state, const HeroStatsPicker &picker)
{
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, ImGui::GetStyle().ItemSpacing.y));
  bool single = !state->perLevel;
  if (ChoiceButton("Whole run", single && state->level == HERO_STATS_WHOLE_RUN))
  {
    state->perLevel = false;
    state->level = HERO_STATS_WHOLE_RUN;
  }
  ImGui::SameLine();
  if (ChoiceButton("Per level", state->perLevel)) state->perLevel = true;
  if (picker.levels.empty())
  {
    // No level visited yet (the party not in the dungeon): nothing to pick, and the line ends here
    ImGui::PopStyleVar();
    return;
  }
  ImGui::SameLine(0, 8);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(Colour(FAINT_RGB), "Lv");
  ImGui::SameLine(0, 6);
  if (!picker.stepper)
  {
    for (size_t i = 0; i < picker.levels.size(); i++)
    {
      if (i > 0) ImGui::SameLine();
      ImGui::PushID((int)i);
      if (ChoiceButton(std::to_string(picker.levels[i]).c_str(), single && state->level == picker.levels[i]))
      {
        state->perLevel = false;
        state->level = picker.levels[i];
      }
      ImGui::PopID();
    }
    ImGui::PopStyleVar();
    return;
  }
  ImGui::BeginDisabled(picker.previous == -2);
  if (ImGui::ArrowButton("##previous", ImGuiDir_Left))
  {
    state->perLevel = false;
    state->level = picker.previous;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  std::string current = single && state->level != HERO_STATS_WHOLE_RUN ? std::to_string(state->level) : "-";
  ImGui::SetNextItemWidth(110);
  if (ImGui::BeginCombo("##level", current.c_str()))
  {
    for (size_t i = 0; i < picker.levels.size(); i++)
      if (ImGui::Selectable(LevelLabel(picker.levels[i]).c_str(), single && state->level == picker.levels[i]))
      {
        state->perLevel = false;
        state->level = picker.levels[i];
      }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(picker.next == -2);
  if (ImGui::ArrowButton("##next", ImGuiDir_Right))
  {
    state->perLevel = false;
    state->level = picker.next;
  }
  ImGui::EndDisabled();
  ImGui::PopStyleVar();
}

// The topics as tabs: the picked one in white over a line in the picked colour
static void DrawTabs(HeroStatsDrawState *state)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImGui::SetCursorPosX(GUTTER);
  float lineY = 0;
  for (int t = 0; t <= HERO_STATS_CHAMPIONS_TAB; t++)
  {
    const char *label = t == HERO_STATS_CHAMPIONS_TAB ? "Champions" : HeroStatsCatalogue_Topic(t).label;
    ImVec2 size = ImGui::CalcTextSize(label);
    // A tab that would cross the window's edge starts the next line
    if (t > 0) ImGui::SameLine(0, 2);
    if (t > 0 && ImGui::GetCursorPosX() + size.x + 20 > ImGui::GetWindowWidth() - GUTTER)
    {
      ImGui::NewLine();
      ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y));
    }
    ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::PushID(t);
    ImVec2 box(size.x + 20, size.y + 12);
    if (ImGui::InvisibleButton("##tab", box)) state->topic = t;
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    bool on = state->topic == t;
    drawList->AddText(ImVec2(at.x + 10, at.y + 5), Packed(on || hovered ? WHITE_RGB : DIM_RGB), label);
    if (on) drawList->AddRectFilled(ImVec2(at.x, at.y + box.y - 2), ImVec2(at.x + box.x, at.y + box.y), Packed(BUTTON_ON_RGB));
    lineY = at.y + box.y;
  }
  ImVec2 windowPos = ImGui::GetWindowPos();
  drawList->AddLine(ImVec2(windowPos.x, lineY), ImVec2(windowPos.x + ImGui::GetWindowWidth(), lineY), Packed(LINE_RGB));
  ImGui::Dummy(ImVec2(0, 2));
}

// ---------------------------------------------------------------------------------------------
// The numbers of the page, kept for a while: every query scans the whole store, and the numbers
// move only as the game plays

static const Uint32 CACHE_LIFETIME = 500;
static bool pageCached = false;

static bool perLevelCached = false;

void HeroStatsDraw_Forget(void)
{
  pageCached = false;
  perLevelCached = false;
}

static std::string ColumnsKey(const std::vector<HeroStatsColumn> &columns)
{
  std::string key;
  for (size_t i = 0; i < columns.size(); i++)
    key += std::to_string(columns[i].id) + (columns[i].hasLeft ? "L," : ",");
  return key;
}

// The page of who, topic and level; Cards only for the Party's Overview
struct PageData
{
  HeroStatsTopicPage topic;
  std::vector<HeroStatsCard> cards;
  std::vector<std::string> topicSummaries;  // a champion's Overview: one per topic
};

static const PageData &CachedPage(const HeroStatsDrawState &state, const HeroStats &stats,
                                  const std::vector<HeroStatsColumn> &columns)
{
  static PageData data;
  static std::string cachedKey;
  static Uint32 cachedAt;
  std::string key = ColumnsKey(columns) + "|" + std::to_string(state.who) + "|" + std::to_string(state.topic) + "|" +
                    std::to_string(state.level);
  Uint32 now = SDL_GetTicks();
  if (!pageCached || key != cachedKey || now - cachedAt >= CACHE_LIFETIME)
  {
    static const HeroStatsNames names = HeroStats_NamesOfTheGame();
    data.topic = HeroStatsPages_Topic(stats, columns, state.who, state.level, state.topic, names);
    data.cards.clear();
    data.topicSummaries.clear();
    if (state.topic == HeroTopicOverview && state.who == HERO_STATS_PARTY)
      data.cards = HeroStatsPages_Cards(stats, columns, state.level);
    if (state.topic == HeroTopicOverview && state.who != HERO_STATS_PARTY)
      for (int t = 0; t < HERO_TOPIC_COUNT; t++)
        data.topicSummaries.push_back(HeroStatsCatalogue_Summary(t, data.topic.record, state.who, state.level));
    cachedKey = key;
    cachedAt = now;
    pageCached = true;
  }
  return data;
}

// ---------------------------------------------------------------------------------------------
// Cells, bars and section headers

// A number column and a bar before their table has been measured (its first frame)
static const float NUMBER_WIDTH = 70;
static const float BAR_WIDTH = 170;
static const ImU32 GUIDE_RGB = 0x2b3140;       // the row under the pointer
static const ImU32 GUIDE_EDGE_RGB = 0x5b8fd9;  // its edge, and the hovered column's line
static const ImU32 GUIDE_CROSS_RGB = 0x36405a; // where the row and the column meet

// How wide a table's columns are. No column has a width of its own: a number column is as wide
// as its widest text, which the cells note as they are drawn, so a table is laid out with the
// widths of the frame before (its first frame with the defaults above). What the table gives up
// when the page is too narrow is HeroStatsLayout_FitTable's.
enum ColumnKind { ColumnLabel, ColumnNumber, ColumnDerived, ColumnBar, ColumnText };
struct TableMeasure
{
  std::vector<int> kinds, lastKinds;  // of this frame as the columns are set up, and of the last
  std::vector<float> seen, width;     // the widest text noted this frame, and the last frame's
  std::vector<bool> disabled;         // columns the fit dropped this frame: they note nothing
  HeroStatsTableFit fit;
  bool known;                         // the last frame measured this table with these columns
};
static std::map<ImGuiID, TableMeasure> tableMeasures;
// The widest full width (HeroStatsLayout_FullWidth) of the tables begun since it was last set to
// 0: what a section needs to stand beside another
static float widestTableBegun = 0;
static TableMeasure *measuring = NULL;
static ImGuiID measuringId = 0;
static bool rowIsHot = false;

// The table being drawn is the one BeginPageTable began
static bool InMeasuredTable(void)
{
  ImGuiTable *table = ImGui::GetCurrentTable();
  return measuring != NULL && table != NULL && table->ID == measuringId;
}

// A cell's text is this wide: its column must be
static void NoteWidth(float width)
{
  if (!InMeasuredTable()) return;
  int column = ImGui::TableGetColumnIndex();
  if (column < 0 || column >= (int)measuring->seen.size()) return;
  measuring->seen[column] = std::max(measuring->seen[column], width);
}

// The text of the row under the pointer is white
static ImU32 Hot(ImU32 rgb)
{
  return rowIsHot && InMeasuredTable() && (rgb == TEXT_RGB || rgb == PART_RGB || rgb == DIM_RGB) ? WHITE_RGB : rgb;
}

static void RightText(const std::string &text, ImU32 rgb)
{
  float width = ImGui::CalcTextSize(text.c_str()).x;
  NoteWidth(width);
  float room = ImGui::GetContentRegionAvail().x - width;
  if (room > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room);
  ImGui::TextColored(Colour(Hot(rgb)), "%s", text.c_str());
}

// A number in a table cell: a dim dot for zero, bad news on a red pill
static void NumberCell(const std::string &text, int value, bool isBad, ImU32 rgb)
{
  if (value == 0)
  {
    RightText(MIDDLE_DOT, FAINT_RGB);
    return;
  }
  if (!isBad || value < 0)
  {
    RightText(text, rgb);
    return;
  }
  float width = ImGui::CalcTextSize(text.c_str()).x;
  NoteWidth(width);
  float room = ImGui::GetContentRegionAvail().x - width;
  if (room > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room);
  PillBehind(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ImVec2(width, ImGui::GetTextLineHeight()));
  ImGui::TextColored(Colour(BAD_RGB), "%s", text.c_str());
}

static void CatalogueCell(HeroStatsNumber number, int value, ImU32 rgb)
{
  const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(number);
  NumberCell(HeroStatsCatalogue_Format(number, value), info.isRatio && value < 0 ? 1 : value, info.isBad, rgb);
}

static void PlainCell(int value, ImU32 rgb = TEXT_RGB, bool isBad = false)
{
  NumberCell(HeroStatsView_Number(value), value, isBad, rgb);
}

// Segments side by side in the champions' colours, as wide as the cell
static void ShareBar(const std::vector<int> &values, float width = 0)
{
  ImVec2 at = ImGui::GetCursorScreenPos();
  if (width <= 0) width = std::max(20.0f, ImGui::GetContentRegionAvail().x - 2);
  float lineHeight = ImGui::GetTextLineHeight();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 top(at.x, at.y + lineHeight / 2 - 4), bottom(at.x + width, at.y + lineHeight / 2 + 5);
  drawList->AddRectFilled(top, bottom, Packed(TRACK_RGB), 2.0f);
  long long total = 0;
  for (size_t i = 0; i < values.size(); i++) total += std::max(0, values[i]);
  if (total > 0)
  {
    float x = top.x;
    for (size_t i = 0; i < values.size(); i++)
    {
      if (values[i] <= 0) continue;
      float w = width * (float)values[i] / (float)total;
      drawList->AddRectFilled(ImVec2(x, top.y), ImVec2(x + w, bottom.y), Packed(ChampionRgb(i)));
      x += w;
    }
  }
  ImGui::Dummy(ImVec2(width, lineHeight));
}

// The part of whole, in one champion's colour
static void PartBar(int part, int whole, size_t column)
{
  ImVec2 at = ImGui::GetCursorScreenPos();
  float width = std::max(20.0f, ImGui::GetContentRegionAvail().x - 2);
  float lineHeight = ImGui::GetTextLineHeight();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 top(at.x, at.y + lineHeight / 2 - 4), bottom(at.x + width, at.y + lineHeight / 2 + 5);
  drawList->AddRectFilled(top, bottom, Packed(TRACK_RGB), 2.0f);
  if (whole > 0 && part > 0)
    drawList->AddRectFilled(top, ImVec2(top.x + width * std::min(1.0f, (float)part / (float)whole), bottom.y),
                            Packed(ChampionRgb(column)));
  ImGui::Dummy(ImVec2(width, lineHeight));
}

// A header that folds its section: arrow and title on the left, the summary on the right. True
// when the section is open.
static bool SectionHeader(HeroStatsDrawState *state, int section, const char *title, const std::string &summary)
{
  int bit = HeroStatsDraw_SectionBit(section);
  bool open = (state->closedSections & bit) == 0;
  ImVec2 at = ImGui::GetCursorScreenPos();
  float width = ImGui::GetContentRegionAvail().x;
  float height = ImGui::GetTextLineHeight() + 8;
  ImGui::PushID(section + 100);
  if (ImGui::InvisibleButton("##section", ImVec2(width, height))) state->closedSections ^= bit;
  bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  std::string upper = title;
  for (size_t i = 0; i < upper.size(); i++) upper[i] = (char)toupper((unsigned char)upper[i]);
  float cx = at.x + 6, cy = at.y + height / 2 - 1;
  if (open) drawList->AddTriangleFilled(ImVec2(cx - 4, cy - 2), ImVec2(cx + 4, cy - 2), ImVec2(cx, cy + 3), Packed(DIM_RGB));
  else drawList->AddTriangleFilled(ImVec2(cx - 2, cy - 4), ImVec2(cx - 2, cy + 4), ImVec2(cx + 3, cy), Packed(DIM_RGB));
  drawList->AddText(ImVec2(at.x + 16, at.y + 3), Packed(hovered ? WHITE_RGB : TEXT_RGB), upper.c_str());
  float summaryWidth = TextWidthWithSymbols(ImGui::GetFontSize(), summary);
  float titleEnd = at.x + 16 + ImGui::CalcTextSize(upper.c_str()).x + 20;
  if (at.x + width - summaryWidth > titleEnd)
    TextWithSymbols(drawList, ImGui::GetFontSize(), ImVec2(at.x + width - summaryWidth, at.y + 3), Packed(DIM_RGB), summary);
  drawList->AddLine(ImVec2(at.x, at.y + height - 1), ImVec2(at.x + width, at.y + height - 1), Packed(LINE_RGB));
  return open;
}

static void SectionGap(void)
{
  ImGui::Dummy(ImVec2(0, 8));
}

// ---------------------------------------------------------------------------------------------
// Tables

static const ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH;

// A table as wide as the page. Its columns follow with LabelColumn, NumberColumn, BarColumn and
// TextColumn, one call per column in order. When the page is too narrow for it, the bar column
// and then the derived columns are left out, then the label wraps, and at last the table scrolls
// sideways on its own with its label staying in place.
static bool BeginPageTable(const char *id, int columnCount, bool sortable)
{
  ImGuiID tableId = ImGui::GetID(id);
  TableMeasure &measure = tableMeasures[tableId];
  // What the last frame saw is what this frame is laid out with
  if (!measure.kinds.empty())
  {
    if (measure.width.size() != measure.kinds.size()) measure.width.assign(measure.kinds.size(), 0.0f);
    for (size_t i = 0; i < measure.kinds.size() && i < measure.seen.size(); i++)
      if (!(i < measure.disabled.size() && measure.disabled[i])) measure.width[i] = measure.seen[i];
    measure.lastKinds = measure.kinds;
  }
  ImGuiTable *last = ImGui::TableFindByID(tableId);
  float available = ImGui::GetContentRegionAvail().x;
  float padding = ImGui::GetStyle().CellPadding.x * 2;
  measure.known = (int)measure.lastKinds.size() == columnCount && last != NULL && last->ColumnsCount == columnCount;
  HeroStatsTableFit everything = {true, true, false, false, 0, BAR_WIDTH + padding, available};
  measure.fit = everything;
  if (measure.known)
  {
    HeroStatsTableNeeds needs = {0, 0, 0, false, 0};
    for (int i = 0; i < columnCount; i++)
    {
      float width = measure.width[i];
      // A text column sizes itself to what is in it (ImGui's own measure of the last frame)
      if (measure.lastKinds[i] == ColumnText) width = last->Columns[i].WidthAuto;
      switch (measure.lastKinds[i])
      {
      case ColumnLabel: needs.label += width + padding; break;
      case ColumnDerived: needs.derived += width + padding; break;
      case ColumnBar:
        // A bar is never narrower than its header ("Share of party", "Who: Experience")
        needs.hasBar = true;
        needs.barHeader = width + padding;
        break;
      default: needs.numbers += width + padding; break;
      }
    }
    measure.fit = HeroStatsLayout_FitTable(needs, available);
    widestTableBegun = std::max(widestTableBegun, HeroStatsLayout_FullWidth(needs));
  }
  measure.kinds.clear();
  measure.seen.assign(columnCount, 0.0f);
  measure.disabled.assign(columnCount, false);
  measuring = &measure;
  measuringId = tableId;
  rowIsHot = false;
  ImGuiTableFlags flags = TABLE_FLAGS | ImGuiTableFlags_HighlightHoveredColumn | (sortable ? ImGuiTableFlags_Sortable : 0);
  ImVec2 size(available, 0);
  if (measure.fit.scroll)
  {
    // A scrolling table is a window of its own and needs its height: the last frame's rows
    bool scrolled = last->InnerWindow != last->OuterWindow;
    size.y = (scrolled ? last->InnerWindow->ContentSize.y : last->OuterRect.GetHeight()) + ImGui::GetStyle().ScrollbarSize;
    flags |= ImGuiTableFlags_ScrollX;
  }
  if (!ImGui::BeginTable(id, columnCount, flags, size, measure.fit.scroll ? measure.fit.tableWidth : 0.0f))
  {
    measuring = NULL;
    return false;
  }
  if (measure.fit.scroll) ImGui::TableSetupScrollFreeze(1, 0);
  return true;
}

// The column being set up: its place, and whether the fit leaves it out
static int AddColumn(ColumnKind kind, bool off)
{
  if (measuring == NULL) return -1;
  int index = (int)measuring->kinds.size();
  measuring->kinds.push_back(kind);
  if (index < (int)measuring->disabled.size()) measuring->disabled[index] = off;
  return index;
}

// The label column takes the room the others leave
static void LabelColumn(const char *label, bool sortable = false, bool sortsFirst = false)
{
  AddColumn(ColumnLabel, false);
  ImGuiTableColumnFlags flags = sortable ? 0 : ImGuiTableColumnFlags_NoSort;
  if (sortsFirst) flags |= ImGuiTableColumnFlags_DefaultSort;
  float padding = ImGui::GetStyle().CellPadding.x * 2;
  if (measuring != NULL && measuring->fit.scroll)
    ImGui::TableSetupColumn(label, flags | ImGuiTableColumnFlags_WidthFixed, std::max(20.0f, measuring->fit.labelWidth - padding));
  else ImGui::TableSetupColumn(label, flags | ImGuiTableColumnFlags_WidthStretch);
}

// A column of numbers, as wide as its widest. derived: worked out from other columns (a rate, an
// average), so it is left out before a label has to wrap.
static void NumberColumn(const char *label, bool sortsFirst = false, bool derived = false)
{
  bool off = derived && measuring != NULL && measuring->known && !measuring->fit.derived;
  int index = AddColumn(derived ? ColumnDerived : ColumnNumber, off);
  float width = NUMBER_WIDTH;
  if (measuring != NULL && measuring->known && index >= 0 && measuring->width[index] > 0) width = measuring->width[index];
  ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending;
  if (sortsFirst) flags |= ImGuiTableColumnFlags_DefaultSort;
  if (off) flags |= ImGuiTableColumnFlags_Disabled;
  ImGui::TableSetupColumn(label, flags, width);
}

// A bar (Share, Who, To next rank): a share of the table, the first to go when it is narrow
static void BarColumn(const char *label)
{
  bool off = measuring != NULL && measuring->known && !measuring->fit.bar;
  AddColumn(ColumnBar, off);
  float width = BAR_WIDTH;
  if (measuring != NULL && measuring->known && measuring->fit.bar)
    width = std::max(20.0f, measuring->fit.barWidth - ImGui::GetStyle().CellPadding.x * 2);
  ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending;
  if (off) flags |= ImGuiTableColumnFlags_Disabled;
  ImGui::TableSetupColumn(label, flags, width);
}

// A column of words, left-aligned, as wide as its widest
static void TextColumn(const char *label)
{
  AddColumn(ColumnText, false);
  ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
}

// The next row of a table, with the guide: the row under the pointer is lit with an edge at its
// label, the column under it tinted (its header underlined by HeaderRow)
static void NextGuidedRow(void)
{
  ImGui::TableNextRow();
  rowIsHot = false;
  if (!InMeasuredTable()) return;
  ImGuiTable *table = ImGui::GetCurrentTable();
  int hoveredColumn = ImGui::TableGetHoveredColumn();
  rowIsHot = hoveredColumn >= 0 && ImGui::TableGetRowIndex() == ImGui::TableGetHoveredRow();
  if (rowIsHot) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, Packed(GUIDE_RGB));
  if (hoveredColumn > 0 && hoveredColumn < table->ColumnsCount)
    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, rowIsHot ? Packed(GUIDE_CROSS_RGB) : Packed(GUIDE_EDGE_RGB, 0.05f), hoveredColumn);
  if (rowIsHot && ImGui::TableSetColumnIndex(0))
  {
    float x = ImGui::GetCursorScreenPos().x - ImGui::GetStyle().CellPadding.x;
    float height = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x, table->RowPosY1), ImVec2(x + 2, table->RowPosY1 + height), Packed(GUIDE_EDGE_RGB));
  }
}

// The header row: the label left, numbers right-aligned; a champion's name after their dot
// (columnOfHeader[i]: the champion column of table column i, or -1), dimmed when they left.
// lastLabel, when given, names the last column instead of its setup name ("Who: Damage").
static void HeaderRow(const std::vector<int> &columnOfHeader, const std::vector<HeroStatsColumn> &columns,
                      const std::string &lastLabel = "")
{
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  int count = ImGui::TableGetColumnCount();
  for (int i = 0; i < count; i++)
  {
    if (!ImGui::TableSetColumnIndex(i)) continue;
    const char *name = i == count - 1 && !lastLabel.empty() ? lastLabel.c_str() : ImGui::TableGetColumnName(i);
    int champion = i < (int)columnOfHeader.size() ? columnOfHeader[i] : -1;
    bool dimmed = champion >= 0 && columns[champion].hasLeft;
    // Numbers right-aligned, room left for the sort arrow; a champion's dot in two spaces before the name
    std::string prefix;
    float spaceWidth = ImGui::CalcTextSize(" ").x;
    bool isText = measuring != NULL && i < (int)measuring->kinds.size() && measuring->kinds[i] == ColumnText;
    bool guided = i > 0 && InMeasuredTable() && ImGui::TableGetHoveredColumn() == i;
    if (!isText) NoteWidth(ImGui::CalcTextSize(name).x + (champion >= 0 ? spaceWidth * 2 : 0) + 14);
    bool isLabel = measuring != NULL && i < (int)measuring->kinds.size() && measuring->kinds[i] == ColumnLabel;
    if (i > 0 && !isText && !isLabel)
    {
      float textWidth = ImGui::CalcTextSize(name).x + (champion >= 0 ? spaceWidth * 2 : 0);
      int spaces = (int)((ImGui::GetContentRegionAvail().x - textWidth - 14) / spaceWidth);
      if (spaces > 0) prefix = std::string(spaces, ' ');
    }
    std::string label = prefix + (champion >= 0 ? "  " : "") + name;
    ImGui::PushStyleColor(ImGuiCol_Text, Colour(guided ? WHITE_RGB : dimmed ? FAINT_RGB : DIM_RGB));
    ImGui::PushID(i);
    ImGui::TableHeader(label.c_str());
    ImGui::PopID();
    ImGui::PopStyleColor();
    if (guided)
    {
      ImRect cell = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), i);
      ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(cell.Min.x, cell.Max.y - 2), cell.Max, Packed(GUIDE_EDGE_RGB));
    }
    if (champion >= 0)
    {
      ImVec2 max = ImGui::GetItemRectMax(), min = ImGui::GetItemRectMin();
      float x = min.x + ImGui::CalcTextSize(prefix.c_str()).x + spaceWidth * 0.7f;
      ChampionDot(ImVec2(x, (min.y + max.y) / 2), (size_t)champion, dimmed);
    }
  }
}

static void TotalRow(void)
{
  NextGuidedRow();
  ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, Packed(PANEL_RGB));
}

// The sort the player clicked: column -1 for none
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

// key(row, column): the row's value in that column; names compare as text
template <typename Row, typename Key>
static void SortRows(std::vector<Row> *rows, const TableSort &sort, Key key)
{
  if (sort.column < 0) return;
  std::stable_sort(rows->begin(), rows->end(), [&](const Row &a, const Row &b) {
    int c = key(a, b, sort.column);
    return sort.descending ? c > 0 : c < 0;
  });
}

// A row's name with the arrow that folds what is under it (a weapon's actions, a class's skills).
// In a table too narrow for its names the name goes on under itself, as LabelCell's does: the
// arrow alone is the tree node then, and the name wrapped text after it.
static bool FoldingLabel(const std::string &text, ImGuiTreeNodeFlags flags, ImU32 rgb)
{
  bool wraps = InMeasuredTable() && measuring->fit.wrap;
  ImGui::PushStyleColor(ImGuiCol_Text, Colour(Hot(rgb)));
  bool open;
  if (!wraps) open = ImGui::TreeNodeEx(text.c_str(), flags);
  else
  {
    // The same id as the unwrapped node, so the fold stays as it was
    open = ImGui::TreeNodeEx(text.c_str(), flags & ~ImGuiTreeNodeFlags_SpanLabelWidth, "%s", "");
    ImGui::SameLine(0, 0);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
  }
  ImGui::PopStyleColor();
  return open;
}

static void LabelCell(const std::string &text, int depth, ImU32 rgb, const char *help = "")
{
  ImGui::TableSetColumnIndex(0);
  NoteWidth(ImGui::CalcTextSize(text.c_str()).x + 14.0f * depth);
  if (depth > 0) ImGui::Indent(14.0f * depth);
  // A label too long for a narrow table goes on under itself
  bool wraps = InMeasuredTable() && measuring->fit.wrap;
  if (wraps) ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(Colour(Hot(rgb)), "%s", text.c_str());
  if (wraps) ImGui::PopTextWrapPos();
  if (help[0] != 0 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", help);
  if (depth > 0) ImGui::Unindent(14.0f * depth);
}

// ---------------------------------------------------------------------------------------------
// Tiles and the Numbers table

static size_t ColumnOf(const std::vector<HeroStatsColumn> &columns, HeroStatsChampion who)
{
  size_t c = 0;
  while (c < columns.size() && columns[c].id != who) c++;
  return c;
}

// What a tile shows: the big text, the label, and on the Experience and Records pages a line
// under them (with a champion's dot before it) or a bar of the way to the next rank
struct TileText
{
  std::string big, label, sub;
  bool bad, gold, hasBar;
  int percent;
  size_t dotColumn;  // the champion's column for the dot before sub, columns.size(): none
};

static TileText TileTextOf(const HeroStatsTopicPage &page, HeroStatsNumber number, bool isParty,
                           const std::vector<HeroStatsColumn> &columns)
{
  int value = page.record.value[number];
  const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(number);
  TileText tile = {HeroStatsCatalogue_Format(number, value), info.label, "", info.isBad && value > 0, false, false, 0, columns.size()};
  int cls = number - HeroNumXpFighter;
  if (cls >= 0 && cls < HERO_XP_CLASS_COUNT && cls < (int)page.classes.size())
  {
    const HeroStatsClassLine &line = page.classes[cls];
    std::string rank = HeroXp_RankName(line.rankNow);
    if (isParty)
    {
      tile.label = line.name + " experience";
      if (!rank.empty())
      {
        tile.sub = "best: " + rank + " (" + WhoLabel(columns, line.best) + ")";
        tile.dotColumn = ColumnOf(columns, line.best);
      }
    }
    else
    {
      tile.big = rank.empty() ? "-" : rank;
      tile.label = line.name + " " + MIDDLE_DOT + " " + HeroStatsView_Number(line.amount) + " XP gained";
      std::string next = HeroXp_RankName(line.rankNow + 1);
      if (line.rankNow >= 0 && next != rank)
      {
        tile.hasBar = true;
        tile.percent = line.percentToNext;
        tile.sub = std::to_string(line.percentToNext) + "% to " + (next.empty() ? HeroXp_RankName(2) : next);
      }
    }
  }
  if (HeroStatsCatalogue_IsRecord(number))
    for (size_t r = 0; r < page.records.size(); r++)
    {
      const HeroStatsRecordLine &record = page.records[r];
      if (record.kind != HeroStatsCatalogue_RecordOf(number)) continue;
      if (!record.has)
      {
        if (!isParty && !record.partyValue.empty())
          tile.sub = "party: " + record.partyValue + " " + WhoLabel(columns, record.partyHolder);
        continue;
      }
      tile.big = record.value;
      std::string detail = !record.whom.empty() ? record.whom : record.how;
      if (isParty)
      {
        tile.sub = record.who + (detail.empty() ? "" : std::string(" ") + MIDDLE_DOT + " " + detail);
        tile.dotColumn = ColumnOf(columns, HeroStatsCatalogue_Info(number).partyOnly ? HERO_STATS_NOBODY : record.partyHolder);
      }
      else
      {
        tile.gold = record.held;
        if (record.held) tile.big += std::string(" ") + STAR;
        else if (!record.partyValue.empty())
          tile.sub = "party: " + record.partyValue + " " + WhoLabel(columns, record.partyHolder);
      }
    }
  return tile;
}

static void DrawTiles(const HeroStatsTopicPage &page, const HeroStatsDrawState &state, const std::vector<HeroStatsColumn> &columns)
{
  if (page.tiles.empty()) return;
  int count = (int)page.tiles.size();
  bool isParty = state.who == HERO_STATS_PARTY;
  std::vector<TileText> tiles;
  bool anySub = false;
  for (int i = 0; i < count; i++)
  {
    tiles.push_back(TileTextOf(page, page.tiles[i], isParty, columns));
    anySub = anySub || !tiles.back().sub.empty() || tiles.back().hasBar;
  }
  float gap = 8;
  float width = ImGui::GetContentRegionAvail().x;
  int perRow = HeroStatsLayout_Across(count, width, HERO_LAYOUT_TILE_SMALLEST, gap);
  float tileWidth = (width - gap * (perRow - 1)) / perRow;
  float bigSize = ImGui::GetFontSize() * 1.6f;
  float lineHeight = ImGui::GetTextLineHeight();
  float height = bigSize + lineHeight + 18 + (anySub ? lineHeight + 8 : 0);
  ImVec2 at = ImGui::GetCursorScreenPos();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  for (int i = 0; i < count; i++)
  {
    const TileText &tile = tiles[i];
    ImVec2 min(at.x + (i % perRow) * (tileWidth + gap), at.y + (i / perRow) * (height + gap));
    drawList->AddRectFilled(min, ImVec2(min.x + tileWidth, min.y + height), Packed(PANEL_RGB), 4.0f);
    drawList->PushClipRect(min, ImVec2(min.x + tileWidth - 6, min.y + height), true);
    // The big number in red alone, as in the export: a pill behind it looks heavy
    ImU32 bigRgb = tile.bad ? BAD_RGB : tile.gold ? GOLD_RGB : WHITE_RGB;
    TextWithSymbols(drawList, bigSize, ImVec2(min.x + 10, min.y + 7), Packed(bigRgb), tile.big);
    drawList->AddText(ImVec2(min.x + 10, min.y + 9 + bigSize), Packed(DIM_RGB), tile.label.c_str());
    float y = min.y + 13 + bigSize + lineHeight;
    if (tile.hasBar)
    {
      ImU32 rgb = ChampionRgb(ColumnOf(columns, state.who));
      ImVec2 barMin(min.x + 10, y), barMax(min.x + tileWidth - 10, y + 5);
      drawList->AddRectFilled(barMin, barMax, Packed(TRACK_RGB), 2.0f);
      drawList->AddRectFilled(barMin, ImVec2(barMin.x + (barMax.x - barMin.x) * tile.percent / 100.0f, barMax.y), Packed(rgb), 2.0f);
      y += 7;
    }
    if (!tile.sub.empty())
    {
      float x = min.x + 10;
      if (tile.dotColumn < columns.size())
      {
        ChampionDot(ImVec2(x + 4, y + lineHeight / 2), tile.dotColumn, columns[tile.dotColumn].hasLeft);
        x += 13;
      }
      TextWithSymbols(drawList, ImGui::GetFontSize(), ImVec2(x, y), Packed(tile.hasBar ? FAINT_RGB : TEXT_RGB), tile.sub);
    }
    drawList->PopClipRect();
  }
  int rows = (count + perRow - 1) / perRow;
  ImGui::Dummy(ImVec2(width, rows * (height + gap) - gap + 4));
}

// The topic's numbers: the Party's with a column per champion and a share bar, a champion's
// beside the party's
static void DrawNumbers(const HeroStatsTopicPage &page, const HeroStatsDrawState &state,
                        const std::vector<HeroStatsColumn> &columns)
{
  bool isParty = state.who == HERO_STATS_PARTY;
  size_t champion = 0;
  while (!isParty && champion < columns.size() && columns[champion].id != state.who) champion++;
  int count = isParty ? (int)columns.size() + 3 : 4;
  ImGui::PushID("numbers");
  if (!BeginPageTable("##numbers", count, false))
  {
    ImGui::PopID();
    return;
  }
  LabelColumn("");
  std::vector<int> headerColumns(1, -1);
  if (isParty)
    for (size_t c = 0; c < columns.size(); c++)
    {
      NumberColumn(columns[c].name.c_str());
      headerColumns.push_back((int)c);
    }
  else
  {
    NumberColumn(columns[champion].name.c_str());
    headerColumns.push_back((int)champion);
  }
  NumberColumn("Party");
  BarColumn(isParty ? "Share" : "Share of party");
  HeaderRow(headerColumns, columns);
  for (size_t r = 0; r < page.numbers.size(); r++)
  {
    const HeroStatsNumbersRow &row = page.numbers[r];
    const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(row.number);
    ImU32 rgb = row.depth > 0 ? PART_RGB : TEXT_RGB;
    NextGuidedRow();
    LabelCell(info.label, row.depth, row.depth > 0 ? DIM_RGB : TEXT_RGB, info.help);
    for (size_t c = 0; c < row.byColumn.size(); c++)
    {
      ImGui::TableSetColumnIndex((int)c + 1);
      bool dimmed = isParty && columns[c].hasLeft;
      CatalogueCell(row.number, row.byColumn[c], dimmed ? DIM_RGB : rgb);
    }
    int partyColumn = isParty ? (int)columns.size() + 1 : 2;
    ImGui::TableSetColumnIndex(partyColumn);
    CatalogueCell(row.number, row.party, rgb);
    ImGui::TableSetColumnIndex(partyColumn + 1);
    if (HeroStatsCatalogue_Summable(row.number) && !row.partyOnly)
    {
      if (isParty) ShareBar(row.byColumn);
      else if (row.party > 0) PartBar(row.byColumn[0], row.party, champion);
    }
  }
  ImGui::EndTable();
  ImGui::PopID();
}

// ---------------------------------------------------------------------------------------------
// Detail tables

static std::string Count(int value, const char *what)
{
  return HeroStatsView_Number(value) + " " + what;
}

static std::string Joined(const std::string &a, const std::string &b)
{
  return a + " \xc2\xb7 " + b;
}

// The Who bar: each champion's part of the line in the column it splits; hovered, the numbers
static void WhoCell(const std::vector<int> &byColumn, const std::vector<HeroStatsColumn> &columns)
{
  ShareBar(byColumn);
  if (!ImGui::IsItemHovered()) return;
  std::string text;
  for (size_t c = 0; c < byColumn.size() && c < columns.size(); c++)
    if (byColumn[c] != 0) text += (text.empty() ? "" : "\n") + columns[c].name + "  " + HeroStatsView_Number(byColumn[c]);
  if (!text.empty()) ImGui::SetTooltip("%s", text.c_str());
}

// What the Who bar splits: the column the table is sorted by when it can be split among the
// champions, else the table's own (a name, a rate or the bar itself cannot)
static int WhoMeasure(const TableSort &sort, const std::vector<int> &splittable, int fallback)
{
  for (size_t i = 0; i < splittable.size(); i++)
    if (splittable[i] == sort.column) return sort.column;
  return fallback;
}

static std::string MeasureLabel(const char *const *columnLabels, int measure)
{
  return std::string("Who: ") + columnLabels[measure];
}

// One champion's numbers with a weapon in a column of the Weapons table
static int AttackValue(const HeroStatsOptionLine &line, int column)
{
  switch (column)
  {
  case 1: return line.tries + line.uses;
  case 2: return line.hits;
  case 6: return line.kills;
  default: return line.damage;
  }
}

template <typename Line>
static void AttackCells(const Line &line, ImU32 rgb)
{
  ImGui::TableSetColumnIndex(1);
  PlainCell(line.tries + line.uses, rgb);
  if (line.tries == 0 && line.uses > 0)
  {
    // Actions that cannot hit (War Cry ...): no hits, damage or kills, and no rate or average to work out
    for (int column = 2; column <= 6; column++)
    {
      ImGui::TableSetColumnIndex(column);
      if (column == 3 || column == 4) RightText("-", FAINT_RGB);
      else PlainCell(0, rgb);
    }
    return;
  }
  ImGui::TableSetColumnIndex(2);
  PlainCell(line.hits, rgb);
  ImGui::TableSetColumnIndex(3);
  RightText(HeroStatsView_Percent(line.hits, line.tries), rgb);
  ImGui::TableSetColumnIndex(4);
  RightText(line.hits > 0 ? HeroStatsView_Number((line.damage + line.hits / 2) / line.hits) : "-", line.hits > 0 ? rgb : FAINT_RGB);
  ImGui::TableSetColumnIndex(5);
  PlainCell(line.damage, rgb);
  ImGui::TableSetColumnIndex(6);
  PlainCell(line.kills, rgb);
}

static double Ratio(int part, int whole)
{
  return whole > 0 ? (double)part / whole : -1;
}

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
  case 6: return Compare(a.kills, b.kills);
  default: return Compare(a.damage, b.damage);  // Who: by the damage it shares out
  }
}

static void DrawWeapons(HeroStatsDrawState *state, const HeroStatsHeroPage &page, const std::vector<HeroStatsColumn> &columns)
{
  bool isParty = state->who == HERO_STATS_PARTY;
  HeroStatsWeaponLine all;
  all.name = "All weapons";
  all.tries = all.hits = all.damage = all.kills = all.uses = 0;
  for (size_t w = 0; w < page.weapons.size(); w++)
  {
    all.tries += page.weapons[w].tries;
    all.hits += page.weapons[w].hits;
    all.damage += page.weapons[w].damage;
    all.kills += page.weapons[w].kills;
    all.uses += page.weapons[w].uses;
  }
  if (!SectionHeader(state, HeroDetailWeapons, "Weapons",
                     page.weapons.empty() ? "none" : Joined(Count(all.damage, "damage"), Count(all.kills, "kills"))))
    return;
  if (page.weapons.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "No attacks.");
    SectionGap();
    return;
  }
  int count = isParty ? 8 : 7;
  if (BeginPageTable("##weapons", count, true))
  {
    LabelColumn("Weapon", true);
    NumberColumn("Tries");
    NumberColumn("Hits");
    NumberColumn("Rate", false, true);
    NumberColumn("Avg", false, true);
    NumberColumn("Damage", true);
    NumberColumn("Kills");
    if (isParty) BarColumn("Who");
    TableSort sort = CurrentSort();
    static const char *const LABELS[7] = {"Weapon", "Tries", "Hits", "Rate", "Avg", "Damage", "Kills"};
    int measure = WhoMeasure(sort, {1, 2, 5, 6}, 5);
    HeaderRow(std::vector<int>(), columns, isParty ? MeasureLabel(LABELS, measure) : "");
    std::vector<HeroStatsWeaponLine> weapons = page.weapons;
    SortRows(&weapons, sort, CompareAttacks<HeroStatsWeaponLine>);
    for (size_t w = 0; w < weapons.size(); w++)
    {
      HeroStatsWeaponLine &weapon = weapons[w];
      SortRows(&weapon.options, sort, CompareAttacks<HeroStatsOptionLine>);
      NextGuidedRow();
      ImGui::TableSetColumnIndex(0);
      // Open or folded is ImGui's own, per weapon and per hero, for as long as the program runs
      ImGui::PushID((int)state->who);
      ImGui::PushID(weapon.id);
      NoteWidth(ImGui::CalcTextSize(weapon.name.c_str()).x + ImGui::GetTreeNodeToLabelSpacing());
      bool open = FoldingLabel(weapon.name, ImGuiTreeNodeFlags_SpanLabelWidth, TEXT_RGB);
      AttackCells(weapon, TEXT_RGB);
      if (isParty)
      {
        ImGui::TableSetColumnIndex(7);
        std::vector<int> split;
        for (size_t c = 0; c < weapon.byColumn.size(); c++) split.push_back(AttackValue(weapon.byColumn[c], measure));
        WhoCell(split, columns);
      }
      if (open)
      {
        for (size_t o = 0; o < weapon.options.size(); o++)
        {
          NextGuidedRow();
          LabelCell(weapon.options[o].name, 1, DIM_RGB);
          AttackCells(weapon.options[o], PART_RGB);
        }
        ImGui::TreePop();
      }
      ImGui::PopID();
      ImGui::PopID();
    }
    TotalRow();
    LabelCell(all.name, 0, WHITE_RGB);
    AttackCells(all, WHITE_RGB);
    ImGui::EndTable();
  }
  SectionGap();
}

template <typename Line>
static int SpellValue(const Line &line, int column)
{
  switch (column)
  {
  case 1: return line.casts;
  case 2: return line.failed;
  case 3: return line.mana;
  case 4: return line.wasted;
  case 5: return line.damage;
  case 6: return line.kills;
  default: return line.mana;
  }
}

static void DrawSpells(HeroStatsDrawState *state, const HeroStatsHeroPage &page, const std::vector<HeroStatsColumn> &columns)
{
  bool isParty = state->who == HERO_STATS_PARTY;
  HeroStatsSpellLine all = {"All spells", 0, 0, 0, 0, 0, 0, 0, std::vector<HeroStatsSpellCounts>()};
  for (size_t i = 0; i < page.spells.size(); i++)
  {
    all.casts += page.spells[i].casts;
    all.failed += page.spells[i].failed;
    all.mana += page.spells[i].mana;
    all.wasted += page.spells[i].wasted;
    all.damage += page.spells[i].damage;
    all.kills += page.spells[i].kills;
  }
  if (!SectionHeader(state, HeroDetailSpells, "Spells",
                     page.spells.empty() ? "none" : Joined(Count(all.casts, "casts"), Count(all.mana, "mana"))))
    return;
  if (page.spells.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "No spells cast.");
    SectionGap();
    return;
  }
  int count = isParty ? 8 : 7;
  if (BeginPageTable("##spells", count, true))
  {
    LabelColumn("Spell", true);
    NumberColumn("Casts");
    NumberColumn("Failed");
    NumberColumn("Mana", true);
    NumberColumn("Wasted");
    NumberColumn("Damage");
    NumberColumn("Kills");
    if (isParty) BarColumn("Who");
    TableSort sort = CurrentSort();
    static const char *const LABELS[7] = {"Spell", "Casts", "Failed", "Mana", "Wasted", "Damage", "Kills"};
    int measure = WhoMeasure(sort, {1, 2, 3, 4, 5, 6}, 3);
    HeaderRow(std::vector<int>(), columns, isParty ? MeasureLabel(LABELS, measure) : "");
    std::vector<HeroStatsSpellLine> spells = page.spells;
    SortRows(&spells, sort, [](const HeroStatsSpellLine &a, const HeroStatsSpellLine &b, int column) {
      if (column == 0) return a.name.compare(b.name);
      return Compare(SpellValue(a, column), SpellValue(b, column));
    });
    for (size_t i = 0; i <= spells.size(); i++)
    {
      bool isAll = i == spells.size();
      const HeroStatsSpellLine &line = isAll ? all : spells[i];
      ImU32 rgb = isAll ? WHITE_RGB : TEXT_RGB;
      if (isAll) TotalRow();
      else NextGuidedRow();
      LabelCell(line.name, 0, rgb);
      for (int column = 1; column <= 6; column++)
      {
        ImGui::TableSetColumnIndex(column);
        PlainCell(SpellValue(line, column), rgb, column == 2 || column == 4);
      }
      if (isParty && !isAll)
      {
        ImGui::TableSetColumnIndex(7);
        std::vector<int> split;
        for (size_t c = 0; c < line.byColumn.size(); c++) split.push_back(SpellValue(line.byColumn[c], measure));
        WhoCell(split, columns);
      }
    }
    ImGui::EndTable();
  }
  SectionGap();
}

// A table of amount lines (damage taken by source, healing by source): text, amount, Who
static void DrawAmounts(HeroStatsDrawState *state, int section, const char *title, const char *column,
                        const char *allLabel, const char *what, const std::vector<HeroStatsAmountLine> &source,
                        const std::vector<HeroStatsColumn> &columns)
{
  bool isParty = state->who == HERO_STATS_PARTY;
  int total = 0;
  for (size_t i = 0; i < source.size(); i++) total += source[i].amount;
  if (!SectionHeader(state, section, title, source.empty() ? "none" : Count(total, what))) return;
  if (source.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "None.");
    SectionGap();
    return;
  }
  ImGui::PushID(section);
  if (BeginPageTable("##amounts", isParty ? 3 : 2, true))
  {
    LabelColumn("Source", true);
    NumberColumn(column, true);
    if (isParty) BarColumn("Who");
    HeaderRow(std::vector<int>(), columns, isParty ? std::string("Who: ") + column : "");
    std::vector<HeroStatsAmountLine> lines = source;
    SortRows(&lines, CurrentSort(), [](const HeroStatsAmountLine &a, const HeroStatsAmountLine &b, int c) {
      return c == 0 ? a.text.compare(b.text) : Compare(a.amount, b.amount);
    });
    for (size_t i = 0; i < lines.size(); i++)
    {
      NextGuidedRow();
      LabelCell(lines[i].text, 0, TEXT_RGB);
      ImGui::TableSetColumnIndex(1);
      PlainCell(lines[i].amount, TEXT_RGB, lines[i].isBad);
      if (isParty)
      {
        ImGui::TableSetColumnIndex(2);
        WhoCell(lines[i].byColumn, columns);
      }
    }
    TotalRow();
    LabelCell(allLabel, 0, WHITE_RGB);
    ImGui::TableSetColumnIndex(1);
    PlainCell(total, WHITE_RGB);
    ImGui::EndTable();
  }
  ImGui::PopID();
  SectionGap();
}

static void DrawDeaths(HeroStatsDrawState *state, const HeroStatsHeroPage &page)
{
  int deaths = (int)page.deathLines.size();
  std::string summary = deaths == 0 ? "never died" : Count(deaths, deaths == 1 ? "death" : "deaths");
  if (!SectionHeader(state, HeroDetailDeaths, "Deaths", summary)) return;
  if (deaths == 0) ImGui::TextColored(Colour(DIM_RGB), "Nobody died.");
  for (size_t i = 0; i < page.deathLines.size(); i++) ImGui::TextColored(Colour(TEXT_RGB), "%s", page.deathLines[i].c_str());
  SectionGap();
}

// Eaten and drunk, one row per item: the two lists of the page joined by name
struct ConsumedLine
{
  std::string name;
  int eaten, drunk;
  std::vector<int> eatenBy, drunkBy;  // each column's part
};

static void DrawConsumed(HeroStatsDrawState *state, const HeroStatsHeroPage &page, const std::vector<HeroStatsColumn> &columns)
{
  bool isParty = state->who == HERO_STATS_PARTY;
  std::vector<ConsumedLine> lines;
  const std::vector<HeroStatsAmountLine> *lists[2] = {&page.eaten, &page.drunk};
  for (int list = 0; list < 2; list++)
    for (size_t i = 0; i < lists[list]->size(); i++)
    {
      const HeroStatsAmountLine &from = (*lists[list])[i];
      size_t at = 0;
      while (at < lines.size() && lines[at].name != from.text) at++;
      if (at == lines.size())
        lines.push_back({from.text, 0, 0, std::vector<int>(columns.size(), 0), std::vector<int>(columns.size(), 0)});
      (list == 0 ? lines[at].eaten : lines[at].drunk) += from.amount;
      std::vector<int> &by = list == 0 ? lines[at].eatenBy : lines[at].drunkBy;
      for (size_t c = 0; c < from.byColumn.size() && c < columns.size(); c++) by[c] += from.byColumn[c];
    }
  std::string summary = Joined(Count(page.eatenTotal, "eaten"), Count(page.drunkTotal, "drunk"));
  if (!SectionHeader(state, HeroDetailConsumed, "Eaten and drunk", lines.empty() ? "nothing" : summary)) return;
  if (lines.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "Nothing eaten or drunk.");
    SectionGap();
    return;
  }
  if (BeginPageTable("##consumed", isParty ? 4 : 3, true))
  {
    LabelColumn("Item", true);
    NumberColumn("Eaten", true);
    NumberColumn("Drunk");
    if (isParty) BarColumn("Who");
    TableSort sort = CurrentSort();
    static const char *const LABELS[3] = {"Item", "Eaten", "Drunk"};
    int measure = WhoMeasure(sort, {1, 2}, 1);
    HeaderRow(std::vector<int>(), columns, isParty ? MeasureLabel(LABELS, measure) : "");
    SortRows(&lines, sort, [](const ConsumedLine &a, const ConsumedLine &b, int c) {
      if (c == 0) return a.name.compare(b.name);
      if (c == 1) return Compare(a.eaten, b.eaten);
      if (c == 2) return Compare(a.drunk, b.drunk);
      return Compare(a.eaten + a.drunk, b.eaten + b.drunk);
    });
    for (size_t i = 0; i <= lines.size(); i++)
    {
      bool isAll = i == lines.size();
      if (isAll) TotalRow();
      else NextGuidedRow();
      ImU32 rgb = isAll ? WHITE_RGB : TEXT_RGB;
      LabelCell(isAll ? "Everything" : lines[i].name, 0, rgb);
      ImGui::TableSetColumnIndex(1);
      PlainCell(isAll ? page.eatenTotal : lines[i].eaten, rgb);
      ImGui::TableSetColumnIndex(2);
      PlainCell(isAll ? page.drunkTotal : lines[i].drunk, rgb);
      if (isParty && !isAll)
      {
        ImGui::TableSetColumnIndex(3);
        WhoCell(measure == 1 ? lines[i].eatenBy : lines[i].drunkBy, columns);
      }
    }
    ImGui::EndTable();
  }
  SectionGap();
}

static void DrawItems(HeroStatsDrawState *state, const std::vector<HeroStatsItemLine> &source, const std::vector<HeroStatsColumn> &columns)
{
  HeroStatsItemLine all = HeroStatsPages_AllItems(source);
  std::vector<HeroStatsItemLine> lines;
  for (size_t i = 0; i < source.size(); i++)
    if (source[i].pickedUp + source[i].usedInWall > 0) lines.push_back(source[i]);
  std::string summary = Joined(Joined(Count(all.pickedUp, "picked up"), Count(all.fromChest, "from chests")),
                               Count(all.usedInWall, "used in walls"));
  if (!SectionHeader(state, HeroDetailItems, "Items", lines.empty() ? "none" : summary)) return;
  if (lines.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "No items picked up or used in walls.");
    SectionGap();
    return;
  }
  if (BeginPageTable("##items", 4, true))
  {
    LabelColumn("Item", true);
    NumberColumn("Picked up", true);
    NumberColumn("From chest");
    NumberColumn("Used in walls");
    HeaderRow(std::vector<int>(), columns);
    SortRows(&lines, CurrentSort(), [](const HeroStatsItemLine &a, const HeroStatsItemLine &b, int c) {
      if (c == 0) return a.name.compare(b.name);
      if (c == 1) return Compare(a.pickedUp, b.pickedUp);
      return c == 2 ? Compare(a.fromChest, b.fromChest) : Compare(a.usedInWall, b.usedInWall);
    });
    for (size_t i = 0; i <= lines.size(); i++)
    {
      bool isAll = i == lines.size();
      const HeroStatsItemLine &line = isAll ? all : lines[i];
      if (isAll) TotalRow();
      else NextGuidedRow();
      ImU32 rgb = isAll ? WHITE_RGB : TEXT_RGB;
      LabelCell(line.name, 0, rgb);
      ImGui::TableSetColumnIndex(1);
      PlainCell(line.pickedUp, rgb);
      ImGui::TableSetColumnIndex(2);
      PlainCell(line.fromChest, rgb);
      ImGui::TableSetColumnIndex(3);
      PlainCell(line.usedInWall, rgb);
    }
    ImGui::EndTable();
  }
  SectionGap();
}

static void DrawMonsters(HeroStatsDrawState *state, const std::vector<HeroStatsMonsterLine> &source,
                         const std::vector<HeroStatsColumn> &columns)
{
  bool isParty = state->who == HERO_STATS_PARTY;
  size_t champions = isParty ? columns.size() : 1;
  HeroStatsMonsterLine all = HeroStatsPages_AllMonsters(source, champions);
  std::string summary = Joined(Count(all.total, "kills"), Count((int)source.size(), "kinds"));
  if (!SectionHeader(state, HeroDetailMonsters, "Monsters killed", source.empty() ? "none" : summary)) return;
  if (source.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "No monsters killed.");
    SectionGap();
    return;
  }
  // The Party: a column per champion, the doors, pits and other causes when there are any, all of them, Who
  bool door = isParty && all.door > 0, pit = isParty && all.pit > 0, other = isParty && all.other > 0;
  int count = 1 + (isParty ? (int)champions + door + pit + other + 2 : 1);
  if (BeginPageTable("##monsters", count, true))
  {
    LabelColumn("Monster", true);
    std::vector<int> headerColumns(1, -1);
    if (isParty)
    {
      for (size_t c = 0; c < champions; c++)
      {
        NumberColumn(columns[c].name.c_str());
        headerColumns.push_back((int)c);
      }
      if (door) NumberColumn("Door");
      if (pit) NumberColumn("Pit");
      if (other) NumberColumn("Other");
      NumberColumn("All", true);
      BarColumn("Who");
    }
    else NumberColumn("Kills", true);
    HeaderRow(headerColumns, columns, isParty ? "Who: Kills" : "");
    // The values of a line in table order after the name
    auto values = [&](const HeroStatsMonsterLine &line) {
      std::vector<int> v;
      if (!isParty) return std::vector<int>(1, line.total);
      for (size_t c = 0; c < champions; c++) v.push_back(line.byColumn[c]);
      if (door) v.push_back(line.door);
      if (pit) v.push_back(line.pit);
      if (other) v.push_back(line.other);
      v.push_back(line.total);
      v.push_back(line.total);  // Who sorts by all of them
      return v;
    };
    std::vector<HeroStatsMonsterLine> lines = source;
    SortRows(&lines, CurrentSort(), [&](const HeroStatsMonsterLine &a, const HeroStatsMonsterLine &b, int c) {
      if (c == 0) return a.name.compare(b.name);
      return Compare(values(a)[c - 1], values(b)[c - 1]);
    });
    for (size_t i = 0; i <= lines.size(); i++)
    {
      bool isAll = i == lines.size();
      const HeroStatsMonsterLine &line = isAll ? all : lines[i];
      if (isAll) TotalRow();
      else NextGuidedRow();
      ImU32 rgb = isAll ? WHITE_RGB : TEXT_RGB;
      LabelCell(line.name, 0, rgb);
      std::vector<int> v = values(line);
      int numbers = isParty ? (int)v.size() - 1 : 1;
      for (int c = 0; c < numbers; c++)
      {
        ImGui::TableSetColumnIndex(c + 1);
        bool dimmed = isParty && c < (int)champions && columns[c].hasLeft;
        PlainCell(v[c], dimmed ? DIM_RGB : rgb);
      }
      if (isParty && !isAll)
      {
        ImGui::TableSetColumnIndex(numbers + 1);
        std::vector<int> split(line.byColumn.begin(), line.byColumn.begin() + champions);
        WhoCell(split, columns);
      }
    }
    ImGui::EndTable();
  }
  SectionGap();
}

// ---------------------------------------------------------------------------------------------
// Experience and Records

// Skills: a class per row, its hidden skills under it
static void DrawSkills(HeroStatsDrawState *state, const std::vector<HeroStatsClassLine> &classes,
                       const std::vector<HeroStatsColumn> &columns)
{
  if (!SectionHeader(state, HeroDetailSkills, "Skills", "the hidden skills under each class")) return;
  bool isParty = state->who == HERO_STATS_PARTY;
  int count = isParty ? 4 : 4;
  if (BeginPageTable("##skills", count, false))
  {
    LabelColumn("Skill");
    if (isParty)
    {
      NumberColumn("Experience");
      NumberColumn("Ranks gained");
      BarColumn("Who: Experience");
    }
    else
    {
      TextColumn("Rank now");
      NumberColumn("Experience");
      NumberColumn("Ranks gained");
    }
    HeaderRow(std::vector<int>(), columns);
    for (size_t c = 0; c < classes.size(); c++)
    {
      const HeroStatsClassLine &line = classes[c];
      NextGuidedRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::PushID((int)state->who);
      ImGui::PushID((int)c + 3000);
      std::string upper = line.name;
      for (size_t i = 0; i < upper.size(); i++) upper[i] = (char)toupper((unsigned char)upper[i]);
      NoteWidth(ImGui::CalcTextSize(upper.c_str()).x + ImGui::GetTreeNodeToLabelSpacing());
      bool open = FoldingLabel(upper, ImGuiTreeNodeFlags_SpanLabelWidth, TEXT_RGB);
      if (isParty)
      {
        ImGui::TableSetColumnIndex(1);
        PlainCell(line.amount);
        ImGui::TableSetColumnIndex(2);
        PlainCell(line.ranksGained);
        ImGui::TableSetColumnIndex(3);
        WhoCell(line.byColumn, columns);
      }
      else
      {
        ImGui::TableSetColumnIndex(1);
        std::string rank = HeroXp_RankName(line.rankNow);
        std::string start = HeroXp_RankName(line.startRank);
        ImGui::TextColored(Colour(TEXT_RGB), "%s", rank.empty() ? "-" : rank.c_str());
        if (!start.empty() && start != rank)
        {
          ImGui::SameLine();
          ImGui::TextColored(Colour(FAINT_RGB), "(%s at the start)", start.c_str());
        }
        ImGui::TableSetColumnIndex(2);
        PlainCell(line.amount);
        ImGui::TableSetColumnIndex(3);
        PlainCell(line.ranksGained);
      }
      if (open)
      {
        for (size_t k = 0; k < line.skills.size(); k++)
        {
          const HeroStatsXpLine &skill = line.skills[k];
          NextGuidedRow();
          LabelCell(skill.name, 1, DIM_RGB);
          ImGui::TableSetColumnIndex(isParty ? 1 : 2);
          PlainCell(skill.amount, PART_RGB);
          if (isParty)
          {
            ImGui::TableSetColumnIndex(3);
            WhoCell(skill.byColumn, columns);
          }
        }
        ImGui::TreePop();
      }
      ImGui::PopID();
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  SectionGap();
}

// Experience by source: the tree of the page, its lines folding their children. Open at first:
// Attacks with its first weapon, and Spells with its classes.
static void DrawXpSources(HeroStatsDrawState *state, const std::vector<HeroStatsXpLine> &lines,
                          const std::vector<HeroStatsColumn> &columns)
{
  if (!SectionHeader(state, HeroDetailXpSources, "Experience by source", "click a line to fold it")) return;
  bool isParty = state->who == HERO_STATS_PARTY;
  int count = isParty ? 3 : 4;
  size_t champion = ColumnOf(columns, state->who);
  if (BeginPageTable("##xpsources", count, false))
  {
    LabelColumn("Source");
    if (isParty) NumberColumn("Experience");
    else
    {
      NumberColumn(champion < columns.size() ? columns[champion].name.c_str() : "");
      NumberColumn("Party");
    }
    BarColumn(isParty ? "Who: Experience" : "Share of party");
    std::vector<int> headerColumns(1, -1);
    if (!isParty) headerColumns.push_back((int)champion);
    HeaderRow(headerColumns, columns);
    int skipDeeperThan = -1;  // a folded line hides the lines under it
    std::string topLine;      // the depth 0 line the lines below belong to
    for (size_t i = 0; i < lines.size(); i++)
    {
      const HeroStatsXpLine &line = lines[i];
      if (line.depth == 0) topLine = line.name;
      if (skipDeeperThan >= 0 && line.depth > skipDeeperThan) continue;
      skipDeeperThan = -1;
      NextGuidedRow();
      ImGui::TableSetColumnIndex(0);
      if (line.depth > 0) ImGui::Indent(14.0f * line.depth);
      ImU32 rgb = line.depth > 0 ? PART_RGB : TEXT_RGB;
      NoteWidth(ImGui::CalcTextSize((line.name + (line.note.empty() ? "" : "  " + line.note)).c_str()).x + ImGui::GetTreeNodeToLabelSpacing() + 14.0f * line.depth);
      if (line.hasChildren)
      {
        // Attacks with its first weapon, Spells with its classes
        bool openAtFirst = line.depth == 0 || (line.depth == 1 && (topLine == "Spells" || lines[i - 1].depth == 0));
        ImGui::PushID((int)state->who);
        ImGui::PushID(line.id);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanLabelWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (openAtFirst) flags |= ImGuiTreeNodeFlags_DefaultOpen;
        bool open = FoldingLabel(line.name, flags, line.depth > 0 ? DIM_RGB : TEXT_RGB);
        ImGui::PopID();
        ImGui::PopID();
        if (!open) skipDeeperThan = line.depth;
      }
      else
      {
        // In line with the text of the lines that fold
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing());
        ImGui::TextColored(Colour(line.depth > 0 ? DIM_RGB : TEXT_RGB), "%s", line.name.c_str());
      }
      if (!line.help.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", line.help.c_str());
      if (!line.note.empty())
      {
        ImGui::SameLine();
        ImGui::TextColored(Colour(FAINT_RGB), "%s", line.note.c_str());
      }
      if (line.depth > 0) ImGui::Unindent(14.0f * line.depth);
      ImGui::TableSetColumnIndex(1);
      PlainCell(line.amount, rgb);
      if (isParty)
      {
        ImGui::TableSetColumnIndex(2);
        WhoCell(line.byColumn, columns);
      }
      else
      {
        int party = line.byColumn.empty() ? 0 : line.byColumn[0];
        ImGui::TableSetColumnIndex(2);
        PlainCell(party, rgb);
        ImGui::TableSetColumnIndex(3);
        if (party > 0) PartBar(line.amount, party, champion);
      }
    }
    ImGui::EndTable();
  }
  SectionGap();
}

static void DrawRankUps(HeroStatsDrawState *state, const std::vector<HeroStatsRankUpLine> &lines,
                        const std::vector<HeroStatsColumn> &columns)
{
  int ups = (int)lines.size();
  if (!SectionHeader(state, HeroDetailRankUps, "Ranks gained", ups == 0 ? "none yet" : "newest first " + std::string(MIDDLE_DOT) + " " + Count(ups, ups == 1 ? "rank" : "ranks")))
    return;
  if (ups == 0)
  {
    ImGui::TextColored(Colour(DIM_RGB), "No new rank yet.");
    SectionGap();
    return;
  }
  bool isParty = state->who == HERO_STATS_PARTY;
  int count = isParty ? 5 : 4;
  if (BeginPageTable("##rankups", count, false))
  {
    if (isParty) LabelColumn("Who");
    if (isParty) TextColumn("Class");
    else LabelColumn("Class");
    TextColumn("Rank");
    TextColumn("With");
    NumberColumn("Lv");
    HeaderRow(std::vector<int>(), columns);
    for (size_t i = 0; i < lines.size(); i++)
    {
      const HeroStatsRankUpLine &line = lines[i];
      NextGuidedRow();
      int column = 0;
      if (isParty)
      {
        ImGui::TableSetColumnIndex(column++);
        size_t c = ColumnOf(columns, line.who);
        ImVec2 at = ImGui::GetCursorScreenPos();
        if (c < columns.size()) ChampionDot(ImVec2(at.x + 4, at.y + ImGui::GetTextLineHeight() / 2), c, columns[c].hasLeft);
        NoteWidth(14 + ImGui::CalcTextSize(WhoLabel(columns, line.who).c_str()).x);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14);
        ImGui::TextColored(Colour(TEXT_RGB), "%s", WhoLabel(columns, line.who).c_str());
      }
      ImGui::TableSetColumnIndex(column++);
      ImGui::TextColored(Colour(TEXT_RGB), "%s", line.className.c_str());
      ImGui::TableSetColumnIndex(column++);
      TextColoredWithSymbols(TEXT_RGB, line.rank);
      ImGui::TableSetColumnIndex(column++);
      ImGui::TextColored(Colour(DIM_RGB), "%s", line.with.c_str());
      ImGui::TableSetColumnIndex(column++);
      RightText(LevelLabel(line.level).substr(3), TEXT_RGB);
    }
    ImGui::EndTable();
  }
  SectionGap();
}

// Records: one table in its four groups. A champion's page: their best, gold with a star when it
// is the party's, and the party's best beside it.
static void DrawRecords(HeroStatsDrawState *state, const std::vector<HeroStatsRecordLine> &lines,
                        const std::vector<HeroStatsColumn> &columns)
{
  bool isParty = state->who == HERO_STATS_PARTY;
  int held = 0;
  for (size_t i = 0; i < lines.size(); i++) held += lines[i].held ? 1 : 0;
  std::string summary = isParty ? "the best of the run " + std::string(MIDDLE_DOT) + " hover a value for when"
                                : WhoLabel(columns, state->who) + "'s best " + MIDDLE_DOT + " " + STAR + " " +
                                      Count(held, held == 1 ? "party record" : "party records");
  if (!SectionHeader(state, HeroDetailRecords, "Records", summary)) return;
  size_t champion = ColumnOf(columns, state->who);
  int count = 6;
  if (BeginPageTable("##records", count, false))
  {
    LabelColumn("Record");
    TextColumn(isParty ? "Value" : (champion < columns.size() ? columns[champion].name.c_str() : "Value"));
    if (isParty) TextColumn("Who");
    TextColumn("How");
    TextColumn("On / from");
    NumberColumn("Lv");
    if (!isParty) TextColumn("Party best");
    HeaderRow(std::vector<int>(), columns);
    std::string group;
    for (size_t i = 0; i < lines.size(); i++)
    {
      const HeroStatsRecordLine &line = lines[i];
      if (line.group != group)
      {
        group = line.group;
        NextGuidedRow();
        std::string upper = group;
        for (size_t k = 0; k < upper.size(); k++) upper[k] = (char)toupper((unsigned char)upper[k]);
        LabelCell(upper, 0, DIM_RGB);
      }
      NextGuidedRow();
      LabelCell(line.label, 0, TEXT_RGB, HeroStatsCatalogue_Info((HeroStatsNumber)(HeroNumRecFirst + line.kind)).help);
      int column = 1;
      ImGui::TableSetColumnIndex(column++);
      if (!line.has) ImGui::TextColored(Colour(FAINT_RGB), "-");
      else
      {
        TextColoredWithSymbols(line.held ? GOLD_RGB : WHITE_RGB, line.value + (line.held ? std::string(" ") + STAR : ""));
        if (ImGui::IsItemHovered() && !line.when.empty()) TooltipWithSymbols(line.when);
      }
      if (isParty)
      {
        ImGui::TableSetColumnIndex(column++);
        size_t c = ColumnOf(columns, line.partyHolder);
        if (line.has && c < columns.size() && line.who != "party")
        {
          ImVec2 at = ImGui::GetCursorScreenPos();
          ChampionDot(ImVec2(at.x + 4, at.y + ImGui::GetTextLineHeight() / 2), c, columns[c].hasLeft);
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14);
        }
        ImGui::TextColored(Colour(line.who == "party" ? DIM_RGB : TEXT_RGB), "%s", line.who.c_str());
      }
      ImGui::TableSetColumnIndex(column++);
      TextColoredWithSymbols(DIM_RGB, line.how);
      ImGui::TableSetColumnIndex(column++);
      ImGui::TextColored(Colour(DIM_RGB), "%s", line.whom.c_str());
      ImGui::TableSetColumnIndex(column++);
      if (line.has) RightText(std::to_string(line.level), TEXT_RGB);
      if (!isParty)
      {
        ImGui::TableSetColumnIndex(column++);
        if (line.held) ImGui::TextColored(Colour(FAINT_RGB), "this one");
        else if (!line.partyValue.empty())
        {
          ImGui::TextColored(Colour(TEXT_RGB), "%s", line.partyValue.c_str());
          size_t c = ColumnOf(columns, line.partyHolder);
          if (c < columns.size())
          {
            ImGui::SameLine();
            ImVec2 at = ImGui::GetCursorScreenPos();
            ChampionDot(ImVec2(at.x + 4, at.y + ImGui::GetTextLineHeight() / 2), c, columns[c].hasLeft);
            ImGui::Dummy(ImVec2(8, ImGui::GetTextLineHeight()));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", columns[c].name.c_str());
          }
        }
      }
    }
    ImGui::EndTable();
  }
  SectionGap();
}


// ---------------------------------------------------------------------------------------------
// Overview

// The Party's Overview: a card per champion with small bars against the best of them. True with
// the champion whose card was clicked.
// A card of the Party's Overview needs this for its label, bar and number
static const float OVERVIEW_CARD_SMALLEST = 190;

static bool DrawCards(const std::vector<HeroStatsCard> &cards, HeroStatsChampion *clicked)
{
  bool anyClicked = false;
  if (cards.empty()) return false;
  static const HeroStatsNumber ROWS[6] = {HeroNumDamage, HeroNumKills, HeroNumTaken, HeroNumHealed, HeroNumManaSpent, HeroNumDeaths};
  static const char *const LABELS[6] = {"Damage", "Kills", "Taken", "Healed", "Mana", "Deaths"};
  int largest[6] = {0};
  for (size_t c = 0; c < cards.size(); c++)
    for (int r = 0; r < 6; r++) largest[r] = std::max(largest[r], cards[c].record.value[ROWS[r]]);
  float gap = 8;
  float width = ImGui::GetContentRegionAvail().x;
  int perRow = HeroStatsLayout_Across((int)cards.size(), width, OVERVIEW_CARD_SMALLEST, gap);
  float cardWidth = (width - gap * (perRow - 1)) / perRow;
  float line = ImGui::GetTextLineHeight() + 4;
  float height = 10 + line * 7 + 4;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  for (size_t c = 0; c < cards.size(); c++)
  {
    const HeroStatsCard &card = cards[c];
    ImVec2 min(origin.x + (c % perRow) * (cardWidth + gap), origin.y + (c / perRow) * (height + gap));
    ImVec2 max(min.x + cardWidth, min.y + height);
    ImGui::SetCursorScreenPos(min);
    ImGui::PushID((int)c);
    if (ImGui::InvisibleButton("##card", ImVec2(cardWidth, height)))
    {
      *clicked = card.column.id;
      anyClicked = true;
    }
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    drawList->AddRectFilled(min, max, Packed(hovered ? PANEL_HOVERED_RGB : PANEL_RGB), 4.0f);
    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 3), Packed(ChampionRgb(c), card.column.hasLeft ? 0.45f : 1.0f));
    drawList->AddText(ImVec2(min.x + 10, min.y + 9), Packed(card.column.hasLeft ? DIM_RGB : WHITE_RGB), card.column.name.c_str());
    std::string share = std::to_string(card.damagePercent) + "% of damage";
    float shareWidth = ImGui::CalcTextSize(share.c_str()).x;
    drawList->AddText(ImVec2(max.x - 10 - shareWidth, min.y + 9), Packed(DIM_RGB), share.c_str());
    for (int r = 0; r < 6; r++)
    {
      float y = min.y + 9 + line * (r + 1);
      int value = card.record.value[ROWS[r]];
      bool bad = HeroStatsCatalogue_Info(ROWS[r]).isBad && value > 0;
      drawList->AddText(ImVec2(min.x + 10, y), Packed(DIM_RGB), LABELS[r]);
      std::string text = value == 0 ? MIDDLE_DOT : HeroStatsView_Number(value);
      ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
      if (bad) PillBehind(drawList, ImVec2(max.x - 10 - textSize.x, y), textSize);
      drawList->AddText(ImVec2(max.x - 10 - textSize.x, y), Packed(bad ? BAD_RGB : value == 0 ? FAINT_RGB : TEXT_RGB), text.c_str());
      float barLeft = min.x + 10 + ImGui::CalcTextSize("Damage").x + 8;
      float barRight = max.x - 10 - ImGui::CalcTextSize("00,000").x - 6;
      float mid = y + ImGui::GetTextLineHeight() / 2;
      if (barRight > barLeft)
      {
        drawList->AddRectFilled(ImVec2(barLeft, mid - 3), ImVec2(barRight, mid + 3), Packed(TRACK_RGB), 2.0f);
        if (largest[r] > 0 && value > 0)
          drawList->AddRectFilled(ImVec2(barLeft, mid - 3),
                                  ImVec2(barLeft + (barRight - barLeft) * value / largest[r], mid + 3),
                                  Packed(bad ? BAD_RGB : ChampionRgb(c)), 2.0f);
      }
    }
  }
  int rows = ((int)cards.size() + perRow - 1) / perRow;
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, rows * (height + gap) + 4));
  return anyClicked;
}

// A champion's Overview: a tile per topic with its summary. The topic clicked, or -1.
static int DrawTopicTiles(const std::vector<std::string> &summaries)
{
  int clicked = -1;
  float gap = 8;
  float width = ImGui::GetContentRegionAvail().x;
  int perRow = width >= 2 * 260 + gap ? 2 : 1;
  float tileWidth = (width - gap * (perRow - 1)) / perRow;
  float height = ImGui::GetTextLineHeight() * 2 + 18;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  int shown = 0;
  for (int t = HeroTopicOverview + 1; t < HERO_TOPIC_COUNT && t < (int)summaries.size(); t++, shown++)
  {
    ImVec2 min(origin.x + (shown % perRow) * (tileWidth + gap), origin.y + (shown / perRow) * (height + gap));
    ImGui::SetCursorScreenPos(min);
    ImGui::PushID(t);
    if (ImGui::InvisibleButton("##topic", ImVec2(tileWidth, height))) clicked = t;
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    drawList->AddRectFilled(min, ImVec2(min.x + tileWidth, min.y + height), Packed(hovered ? PANEL_HOVERED_RGB : PANEL_RGB), 4.0f);
    drawList->PushClipRect(min, ImVec2(min.x + tileWidth - 8, min.y + height), true);
    drawList->AddText(ImVec2(min.x + 10, min.y + 7), Packed(WHITE_RGB), HeroStatsCatalogue_Topic(t).label);
    drawList->AddText(ImVec2(min.x + 10, min.y + 9 + ImGui::GetTextLineHeight()), Packed(DIM_RGB), summaries[t].c_str());
    drawList->PopClipRect();
    // Only a summary cut off at the tile's edge needs the whole of it on hover
    bool cutOff = 10 + ImGui::CalcTextSize(summaries[t].c_str()).x > tileWidth - 8;
    if (hovered && cutOff) ImGui::SetTooltip("%s", summaries[t].c_str());
  }
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, ((shown + perRow - 1) / perRow) * (height + gap) + 4));
  return clicked;
}

// ---------------------------------------------------------------------------------------------
// The page

// What a section of a topic page needed when it was last drawn
struct SectionSize
{
  float fullWidth, height;
  SectionSize() : fullWidth(0), height(0) {}
};
static const float SECTION_GAP = 22;

static void DrawSection(HeroStatsDrawState *state, const HeroStatsTopicPage &page, int section, const std::string &whose,
                        const std::vector<HeroStatsColumn> &columns)
{
  switch (section)
  {
  case HERO_STATS_NUMBERS_SECTION:
    if (SectionHeader(state, HERO_STATS_NUMBERS_SECTION, "Numbers", whose))
    {
      DrawNumbers(page, *state, columns);
      SectionGap();
    }
    break;
  case HeroDetailWeapons: DrawWeapons(state, page.hero, columns); break;
  case HeroDetailSpells: DrawSpells(state, page.hero, columns); break;
  case HeroDetailHarm:
    DrawAmounts(state, HeroDetailHarm, "Damage taken by source", "Damage", "All sources", "taken", page.hero.taken, columns);
    break;
  case HeroDetailHealing:
    DrawAmounts(state, HeroDetailHealing, "Healing by source", "Healed", "All sources", "healed", page.hero.healing, columns);
    break;
  case HeroDetailDeaths: DrawDeaths(state, page.hero); break;
  case HeroDetailConsumed: DrawConsumed(state, page.hero, columns); break;
  case HeroDetailItems: DrawItems(state, page.items, columns); break;
  case HeroDetailMonsters: DrawMonsters(state, page.monsters, columns); break;
  case HeroDetailSkills: DrawSkills(state, page.classes, columns); break;
  case HeroDetailXpSources: DrawXpSources(state, page.sources, columns); break;
  case HeroDetailRankUps: DrawRankUps(state, page.rankUps, columns); break;
  case HeroDetailRecords: DrawRecords(state, page.records, columns); break;
  default: break;
  }
}

static void DrawTopicPage(HeroStatsDrawState *state, const PageData &data, const std::vector<HeroStatsColumn> &columns)
{
  const HeroStatsTopicPage &page = data.topic;
  bool isParty = state->who == HERO_STATS_PARTY;
  DrawTiles(page, *state, columns);
  if (state->topic == HeroTopicOverview)
  {
    // A card or a tile opens another page from the next frame: the rest of this one is drawn
    // with the numbers it was made with
    HeroStatsChampion clickedWho = HERO_STATS_PARTY;
    bool whoClicked = isParty && DrawCards(data.cards, &clickedWho);
    int clickedTopic = isParty ? -1 : DrawTopicTiles(data.topicSummaries);
    if (SectionHeader(state, HERO_STATS_KEY_NUMBERS_SECTION, "Key numbers",
                      isParty ? "one row from each topic" : "compared with the party"))
    {
      DrawNumbers(page, *state, columns);
      SectionGap();
    }
    if (whoClicked) state->who = clickedWho;
    if (clickedTopic >= 0) state->topic = clickedTopic;
    return;
  }
  std::string whose = isParty ? "each champion and the party" : WhoLabel(columns, state->who) + " and the party";
  // The page's sections: Numbers (the Records table is that page's numbers), then the detail tables
  std::vector<int> sections;
  if (state->topic != HeroTopicRecords) sections.push_back(HERO_STATS_NUMBERS_SECTION);
  for (size_t d = 0; d < page.details.size(); d++) sections.push_back(page.details[d]);
  // In a window wide enough for two sections side by side at their full width they stand in two
  // columns, each in the column shorter so far. Widths and heights are the last frame's.
  static std::map<ImGuiID, SectionSize> sizes;
  std::vector<SectionSize *> sizeOf;
  std::vector<float> widths, heights;
  bool allKnown = true;
  for (size_t s = 0; s < sections.size(); s++)
  {
    ImGui::PushID(sections[s] + 500);
    sizeOf.push_back(&sizes[ImGui::GetID("##size")]);
    ImGui::PopID();
    allKnown = allKnown && sizeOf[s]->height > 0;
    widths.push_back(sizeOf[s]->fullWidth);
    heights.push_back(sizeOf[s]->height);
  }
  float available = ImGui::GetContentRegionAvail().x;
  bool two = allKnown && HeroStatsLayout_TwoColumns(widths, available, SECTION_GAP);
  std::vector<int> columnOf = two ? HeroStatsLayout_Flow(heights) : std::vector<int>(sections.size(), 0);
  bool inColumns = false;
  if (two)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(SECTION_GAP / 2, 0));
    inColumns = ImGui::BeginTable("##sections", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX);
    ImGui::PopStyleVar();
    if (inColumns) ImGui::TableNextRow();
  }
  for (int side = 0; side < (inColumns ? 2 : 1); side++)
  {
    if (inColumns) ImGui::TableSetColumnIndex(side);
    for (size_t s = 0; s < sections.size(); s++)
    {
      if (inColumns && columnOf[s] != side) continue;
      float top = ImGui::GetCursorPosY();
      widestTableBegun = 0;
      DrawSection(state, page, sections[s], whose, columns);
      sizeOf[s]->height = ImGui::GetCursorPosY() - top;
      // A folded section and one without a table keep what they needed when last open
      if (widestTableBegun > 0) sizeOf[s]->fullWidth = widestTableBegun;
    }
  }
  if (inColumns) ImGui::EndTable();
}

// ---------------------------------------------------------------------------------------------
// Per level

static const HeroStatsPerLevel &CachedPerLevel(const HeroStatsDrawState &state, bool grid, const HeroStats &stats,
                                               const std::vector<HeroStatsColumn> &columns)
{
  static HeroStatsPerLevel data;
  static std::string cachedKey;
  static Uint32 cachedAt;
  std::string key = ColumnsKey(columns) + "|" + std::to_string(state.who) + "|" + std::to_string(state.topic) + "|" +
                    (grid ? "grid" + std::to_string(state.gridNumber[state.topic]) : std::string("totals")) + "|" +
                    std::to_string(stats.VisitedLevels().size());
  Uint32 now = SDL_GetTicks();
  if (!perLevelCached || key != cachedKey || now - cachedAt >= CACHE_LIFETIME)
  {
    data = grid ? HeroStatsPages_PerLevelGrid(stats, columns, state.gridNumber[state.topic])
                : HeroStatsPages_PerLevelTotals(stats, columns, state.who, state.topic);
    cachedKey = key;
    cachedAt = now;
    perLevelCached = true;
  }
  return data;
}

// A label before a group of choices, on the choices' line
static void ChoiceLabel(const char *label)
{
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(Colour(FAINT_RGB), "%s", label);
  ImGui::SameLine(0, 10);
}

// The Party's Show switch, and in Champions side by side the Number to show; true for the grid
static bool DrawPerLevelChoices(HeroStatsDrawState *state)
{
  std::vector<HeroStatsNumber> numbers = HeroStatsCatalogue_GridNumbers(state->topic);
  if (state->who != HERO_STATS_PARTY || numbers.empty()) return false;
  // Records are one at a time: fifteen side by side, each a best, would say little
  bool recordsOnly = state->topic == HeroTopicRecords;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 6));
  if (!recordsOnly)
  {
    ChoiceLabel("Show  ");
    if (ChoiceButton("Party totals", !state->sideBySide)) state->sideBySide = false;
    ImGui::SameLine();
    if (ChoiceButton("Champions side by side", state->sideBySide)) state->sideBySide = true;
  }
  bool sideBySide = recordsOnly || state->sideBySide;
  if (sideBySide)
  {
    if (std::find(numbers.begin(), numbers.end(), state->gridNumber[state->topic]) == numbers.end())
      state->gridNumber[state->topic] = numbers[0];
    ChoiceLabel("Number");
    float left = ImGui::GetCursorPosX();
    for (size_t n = 0; n < numbers.size(); n++)
    {
      const char *label = HeroStatsCatalogue_Info(numbers[n]).label;
      if (n > 0)
      {
        ImGui::SameLine();
        // A button that does not fit starts a new line under the first
        if (ImGui::GetContentRegionAvail().x < ButtonWidth(label))
        {
          ImGui::NewLine();
          ImGui::SetCursorPosX(left);
        }
      }
      ImGui::PushID((int)n + 1000);
      if (ChoiceButton(label, state->gridNumber[state->topic] == numbers[n])) state->gridNumber[state->topic] = numbers[n];
      ImGui::PopID();
    }
  }
  ImGui::PopStyleVar();
  ImGui::Dummy(ImVec2(0, 4));
  return sideBySide;
}

static void LevelCell(HeroStatsDrawState *state, int level)
{
  ImGui::TableSetColumnIndex(0);
  NoteWidth(ImGui::CalcTextSize(LevelLabel(level).c_str()).x);
  ImGui::PushID(level);
  bool isRun = level == HERO_STATS_WHOLE_RUN;
  ImGui::PushStyleColor(ImGuiCol_Text, Colour(isRun ? WHITE_RGB : TEXT_RGB));
  if (ImGui::Selectable(LevelLabel(level).c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
  {
    state->perLevel = false;
    state->level = level;
  }
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show %s on its own", LevelLabel(level).c_str());
  ImGui::PopID();
}

static void DrawPerLevel(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns)
{
  bool grid = DrawPerLevelChoices(state);
  const HeroStatsPerLevel &table = CachedPerLevel(*state, grid, stats, columns);
  if (table.numbers.empty() || table.rows.empty()) return;
  int count;
  std::vector<int> headerColumns(1, -1);
  if (grid) count = 1 + (int)columns.size() + 1 + (table.hasShare ? 1 : 0);
  else count = 1 + (int)table.numbers.size() + (table.hasShare ? 1 : 0);
  std::vector<std::string> labels;
  if (grid)
  {
    for (size_t c = 0; c < columns.size(); c++) labels.push_back(columns[c].name);
    labels.push_back("Party");
    if (table.hasShare) labels.push_back("Share");
  }
  else
  {
    for (size_t n = 0; n < table.numbers.size(); n++) labels.push_back(HeroStatsCatalogue_Info(table.numbers[n]).label);
    if (table.hasShare) labels.push_back(std::string("Who: ") + HeroStatsCatalogue_Info(table.shareNumber).label);
  }
  ImGui::PushID(grid ? "grid" : "totals");
  if (!BeginPageTable("##perlevel", count, true))
  {
    ImGui::PopID();
    return;
  }
  LabelColumn("Level", true, true);
  for (size_t i = 0; i < labels.size(); i++)
  {
    bool isBar = table.hasShare && i == labels.size() - 1;
    if (isBar) BarColumn(labels[i].c_str());
    else NumberColumn(labels[i].c_str());
    if (grid && i < columns.size()) headerColumns.push_back((int)i);
  }
  HeaderRow(headerColumns, columns);
  // The levels sort; the whole run stays last
  std::vector<HeroStatsLevelRow> rows(table.rows.begin(), table.rows.end() - 1);
  size_t valueCount = table.rows.back().values.size();
  SortRows(&rows, CurrentSort(), [&](const HeroStatsLevelRow &a, const HeroStatsLevelRow &b, int column) {
    if (column == 0) return Compare(a.level, b.level);
    size_t value = std::min((size_t)column - 1, valueCount - 1);  // Share and Who sort by the last value
    if (!grid && table.hasShare && (size_t)column > valueCount)
    {
      long long sa = 0, sb = 0;
      for (size_t i = 0; i < a.share.size(); i++) sa += a.share[i];
      for (size_t i = 0; i < b.share.size(); i++) sb += b.share[i];
      return Compare((double)sa, (double)sb);
    }
    return Compare(a.values[value], b.values[value]);
  });
  rows.push_back(table.rows.back());
  for (size_t r = 0; r < rows.size(); r++)
  {
    const HeroStatsLevelRow &row = rows[r];
    bool isRun = row.level == HERO_STATS_WHOLE_RUN;
    if (isRun) TotalRow();
    else NextGuidedRow();
    LevelCell(state, row.level);
    ImU32 rgb = isRun ? WHITE_RGB : TEXT_RGB;
    for (size_t v = 0; v < row.values.size(); v++)
    {
      ImGui::TableSetColumnIndex((int)v + 1);
      HeroStatsNumber number = grid ? table.numbers[0] : table.numbers[v];
      bool dimmed = grid && v < columns.size() && columns[v].hasLeft;
      CatalogueCell(number, row.values[v], dimmed ? DIM_RGB : rgb);
    }
    if (table.hasShare)
    {
      ImGui::TableSetColumnIndex((int)row.values.size() + 1);
      ShareBar(row.share);
    }
  }
  ImGui::EndTable();
  ImGui::PopID();
}

static void DrawPage(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns)
{
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
  // The page never scrolls sideways: a table too wide for it scrolls on its own
  bool visible = ImGui::BeginChild("##page", ImGui::GetContentRegionAvail(), ImGuiChildFlags_AlwaysUseWindowPadding);
  ImGui::PopStyleVar();
  if (visible)
  {
    // Tables of their own for each layout: a table that gets other columns matches them to the
    // old ones by name and keeps their old places, which shuffled the champions' columns (a
    // champion's "BORIS" and "Party" landing between the Party page's champions). Our columns
    // never move, so a table never needs another layout's state.
    std::string layout = ColumnsKey(columns) + "|" + std::to_string(state->who) + "|" + std::to_string(state->topic) +
                         (state->perLevel ? (state->sideBySide ? "|side" : "|levels") : "");
    ImGui::PushID(layout.c_str());
    if (state->perLevel) DrawPerLevel(state, stats, columns);
    else DrawTopicPage(state, CachedPage(*state, stats, columns), columns);
    ImGui::PopID();
  }
  ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------
// The Champions tab: what the game's inventory screen shows of each champion, read only

static const float LIVE_LABEL_WIDTH = 66;
static const ImU32 SLOT_RGB = 0x17181c;
static const ImU32 SLOT_LINE_RGB = 0x2a2c33;
static const ImU32 ICON_GROUND_RGB = 0x484848;  // the grey the game's inventory icons stand on
static const float SLOT_SIZE = 40;
static const float SLOT_GAP = 4;
// The pictures of this frame (HeroStatsDraw_Frame sets them)
static const HeroStatsDrawPictures *livePictures = NULL;

static ImTextureID IconOf(const HeroStatsLiveItem &item)
{
  if (livePictures == NULL || livePictures->icon == NULL || item.iconGraphic < 0) return 0;
  return livePictures->icon(item.iconGraphic, item.iconTile);
}

// The item's icon at a size, or a "?" for an item the dungeon gives none
static void IconAt(ImDrawList *drawList, const HeroStatsLiveItem &item, ImVec2 min, float size)
{
  ImTextureID icon = IconOf(item);
  if (icon != 0)
  {
    drawList->AddImage(ImTextureRef(icon), min, ImVec2(min.x + size, min.y + size));
    return;
  }
  ImVec2 text = ImGui::CalcTextSize("?");
  drawList->AddRectFilled(min, ImVec2(min.x + size, min.y + size), Packed(ICON_GROUND_RGB));
  if (size >= text.y) drawList->AddText(ImVec2(min.x + (size - text.x) / 2, min.y + (size - text.y) / 2), Packed(DIM_RGB), "?");
}

// The index of the item in a slot (not one inside a chest), or -1
static int ItemIn(const std::vector<HeroStatsLiveItem> &items, int slot)
{
  for (size_t i = 0; i < items.size(); i++)
    if (items[i].slot == slot && !items[i].inChest) return (int)i;
  return -1;
}

// "0.9 kg", "7.3 kg with contents · 9 charges"
static std::string WeightAndCharges(const HeroStatsLiveItem &item, int held)
{
  std::string text = HeroStatsLive_WeightText(item.weight, held > 0);
  std::string charges = HeroStatsLive_ChargesText(item.charges, item.gulps);
  if (!charges.empty()) text += std::string(" ") + MIDDLE_DOT + " " + charges;
  return text;
}

// What a slot says under the pointer: the item large, its name, where it is, its weight and
// charges, and for a chest what it holds
static void SlotTooltip(const std::vector<HeroStatsLiveItem> &items, int index, const std::string &slotName, bool wounded)
{
  // Room around the picture: the window's own tooltips are text and need none
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
  bool open = ImGui::BeginTooltip();
  ImGui::PopStyleVar();
  if (!open) return;
  if (index < 0)
  {
    ImGui::TextColored(Colour(DIM_RGB), "%s %s empty", slotName.c_str(), MIDDLE_DOT);
    if (wounded)
    {
      ImGui::SameLine(0, 0);
      ImGui::TextColored(Colour(BAD_RGB), " %s wounded", MIDDLE_DOT);
    }
    ImGui::EndTooltip();
    return;
  }
  const HeroStatsLiveItem &item = items[index];
  int held = HeroStatsLive_HeldCount(items, (size_t)index);
  ImVec2 at = ImGui::GetCursorScreenPos();
  IconAt(ImGui::GetWindowDrawList(), item, at, 48);
  ImGui::Dummy(ImVec2(48, 48));
  ImGui::SameLine(0, 10);
  ImGui::BeginGroup();
  ImGui::TextColored(Colour(WHITE_RGB), "%s", item.name.c_str());
  ImGui::TextColored(Colour(DIM_RGB), "%s", slotName.c_str());
  if (wounded)
  {
    ImGui::SameLine(0, 0);
    ImGui::TextColored(Colour(BAD_RGB), " %s wounded", MIDDLE_DOT);
  }
  ImGui::TextColored(Colour(PART_RGB), "%s", WeightAndCharges(item, held).c_str());
  ImGui::EndGroup();
  if (held > 0)
  {
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
    ImGui::TextColored(Colour(DIM_RGB), "Holds %d %s", held, held == 1 ? "item" : "items");
    if (ImGui::BeginTable("##held", 2, ImGuiTableFlags_SizingFixedFit))
    {
      for (int i = 0; i < held; i++)
      {
        const HeroStatsLiveItem &inside = items[index + 1 + i];
        ImGui::TableNextColumn();
        ImVec2 iconAt = ImGui::GetCursorScreenPos();
        IconAt(ImGui::GetWindowDrawList(), inside, iconAt, 16);
        ImGui::Dummy(ImVec2(16, 16));
        ImGui::SameLine(0, 6);
        ImGui::TextColored(Colour(TEXT_RGB), "%s", inside.name.c_str());
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndTooltip();
}

// One slot of the inventory at the cursor: the item's icon on the game's grey, or a dark square
// with the part's name when empty; a red frame on a wounded part; a chest's count in its corner
static void DrawSlot(const std::vector<HeroStatsLiveItem> &items, int index, const std::string &slotName, float size,
                     bool wounded, const char *caption)
{
  ImVec2 min = ImGui::GetCursorScreenPos(), max(min.x + size, min.y + size);
  ImGui::PushID(slotName.c_str());
  ImGui::InvisibleButton("##slot", ImVec2(size, size));
  ImGui::PopID();
  bool hovered = ImGui::IsItemHovered();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  bool has = index >= 0;
  drawList->AddRectFilled(min, max, Packed(has ? ICON_GROUND_RGB : SLOT_RGB), 3.0f);
  if (has)
  {
    float icon = floorf((size - 4) / 16) * 16;  // a whole multiple of the icon's 16 pixels
    if (icon < 16) icon = 16;
    IconAt(drawList, items[index], ImVec2(min.x + (size - icon) / 2, min.y + (size - icon) / 2), icon);
    int held = HeroStatsLive_HeldCount(items, (size_t)index);
    if (held > 0)
    {
      std::string count = std::to_string(held);
      ImVec2 text = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * 0.85f, FLT_MAX, 0.0f, count.c_str());
      ImVec2 textAt(max.x - text.x - 3, max.y - text.y - 1);
      drawList->AddRectFilled(ImVec2(textAt.x - 2, textAt.y), ImVec2(max.x - 1, max.y - 1), Packed(0x000000, 0.72f), 2.0f);
      drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, textAt, Packed(WHITE_RGB), count.c_str());
    }
  }
  else if (caption != NULL && caption[0] != 0)
  {
    float small = ImGui::GetFontSize() * 0.72f;
    ImVec2 text = ImGui::GetFont()->CalcTextSizeA(small, FLT_MAX, 0.0f, caption);
    if (text.x <= size - 2)
      drawList->AddText(ImGui::GetFont(), small, ImVec2(min.x + (size - text.x) / 2, min.y + (size - text.y) / 2), Packed(BORDER_RGB), caption);
  }
  if (wounded) drawList->AddRect(min, max, Packed(BAD_RGB), 3.0f, 0, 2.0f);
  else drawList->AddRect(min, max, Packed(hovered && has ? WHITE_RGB : has ? BORDER_RGB : SLOT_LINE_RGB), 3.0f);
  if (hovered) SlotTooltip(items, index, slotName, wounded);
}

static bool IsWounded(const HeroStatsLiveChampion &live, int slot)
{
  return slot >= 0 && slot < HERO_LIVE_WOUND_COUNT && ((live.wounds >> slot) & 1) != 0;
}

static std::string OutOf(int now, int most)
{
  return std::to_string(now) + " / " + std::to_string(most);
}

// One line of the vitals: a label, a bar of a part (0 to 1) in a colour, and the number after it
struct VitalLine
{
  const char *label;
  float part;
  ImU32 rgb;
  std::string text;
  ImU32 textRgb;
};

// Health, stamina, mana, food, water and load as bars that take the room between the labels and
// the numbers, then wounds and poison as text. width: of the block.
static void LiveVitals(const HeroStatsLiveChampion &live, ImU32 rgb, float width)
{
  // Food and water: the game's bar, from HERO_LIVE_FOOD_EMPTY to HERO_LIVE_FOOD_FULL; the number
  // turns yellow below 0 and red below -512 as the game's bar does
  const float span = (float)(HERO_LIVE_FOOD_FULL - HERO_LIVE_FOOD_EMPTY);
  std::vector<VitalLine> lines;
  VitalLine health = {"Health", live.maxHealth > 0 ? (float)live.health / live.maxHealth : 0, rgb, OutOf(live.health, live.maxHealth), TEXT_RGB};
  VitalLine stamina = {"Stamina", live.maxStamina > 0 ? (float)live.stamina / live.maxStamina : 0, rgb,
                       OutOf(live.stamina / 10, live.maxStamina / 10), TEXT_RGB};
  VitalLine mana = {"Mana", live.maxMana > 0 ? (float)live.mana / live.maxMana : 0, rgb, OutOf(live.mana, live.maxMana), TEXT_RGB};
  VitalLine food = {"Food", (live.food - HERO_LIVE_FOOD_EMPTY) / span, FOOD_RGB,
                    std::to_string(live.food) + (live.food < 0 ? " (starving)" : ""),
                    live.food < -512 ? BAD_RGB : live.food < 0 ? YELLOW_RGB : TEXT_RGB};
  VitalLine water = {"Water", (live.water - HERO_LIVE_FOOD_EMPTY) / span, WATER_RGB,
                     std::to_string(live.water) + (live.water < 0 ? " (parched)" : ""),
                     live.water < -512 ? BAD_RGB : live.water < 0 ? YELLOW_RGB : TEXT_RGB};
  bool heavy = live.load > live.maxLoad;
  VitalLine load = {"Load", live.maxLoad > 0 ? (float)live.load / live.maxLoad : 0, heavy ? BAD_RGB : FAINT_RGB,
                    HeroStatsLive_Tenths(live.load) + " / " + HeroStatsLive_Tenths(live.maxLoad) + " kg", heavy ? BAD_RGB : TEXT_RGB};
  lines.push_back(health);
  lines.push_back(stamina);
  lines.push_back(mana);
  lines.push_back(food);
  lines.push_back(water);
  lines.push_back(load);
  float numbers = 0;
  for (size_t i = 0; i < lines.size(); i++) numbers = std::max(numbers, ImGui::CalcTextSize(lines[i].text.c_str()).x);
  float lineHeight = ImGui::GetTextLineHeight();
  float barWidth = std::max(30.0f, width - LIVE_LABEL_WIDTH - 10 - numbers);
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  float left = ImGui::GetCursorPosX();
  for (size_t i = 0; i < lines.size(); i++)
  {
    const VitalLine &line = lines[i];
    ImGui::TextColored(Colour(DIM_RGB), "%s", line.label);
    ImGui::SameLine(left + LIVE_LABEL_WIDTH);
    ImVec2 at = ImGui::GetCursorScreenPos();
    ImVec2 top(at.x, at.y + lineHeight / 2 - 4), bottom(at.x + barWidth, at.y + lineHeight / 2 + 5);
    drawList->AddRectFilled(top, bottom, Packed(TRACK_RGB), 2.0f);
    float part = std::max(0.0f, std::min(1.0f, line.part));
    if (part > 0) drawList->AddRectFilled(top, ImVec2(top.x + barWidth * part, bottom.y), Packed(line.rgb), 2.0f);
    ImGui::Dummy(ImVec2(barWidth, lineHeight));
    float textWidth = ImGui::CalcTextSize(line.text.c_str()).x;
    ImGui::SameLine(left + LIVE_LABEL_WIDTH + barWidth + 10 + numbers - textWidth);
    ImGui::TextColored(Colour(line.textRgb), "%s", line.text.c_str());
  }
  float wrapAt = left + width;
  std::string wounds = HeroStatsLive_Wounds(live.wounds);
  ImGui::TextColored(Colour(DIM_RGB), "Wounds");
  ImGui::SameLine(left + LIVE_LABEL_WIDTH);
  ImGui::PushTextWrapPos(wrapAt);
  ImGui::TextColored(Colour(wounds.empty() ? DIM_RGB : BAD_RGB), "%s", wounds.empty() ? "none" : wounds.c_str());
  ImGui::PopTextWrapPos();
  ImGui::TextColored(Colour(DIM_RGB), "Poison");
  ImGui::SameLine(left + LIVE_LABEL_WIDTH);
  if (live.poisonCount > 0 || live.poison > 0)
  {
    // The game hurts max(1, v / 64) at each of a dose's v ticks; the doses work side by side
    int perTick = 0;
    for (size_t i = 0; i < live.poisonDoses.size(); i++) perTick += std::max(1, live.poisonDoses[i] / 64);
    ImGui::PushTextWrapPos(wrapAt);
    ImGui::TextColored(Colour(BAD_RGB), "%d (%d %s, %d HP per 36 ticks)", live.poison, live.poisonCount,
                       live.poisonCount == 1 ? "dose" : "doses", perTick);
    ImGui::PopTextWrapPos();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("The poison still to tick, all doses added up. A dose of strength v hurts max(1, v / 64) HP\n"
                        "every 36 ticks and gets weaker by 1 each time; a Cure Poison removes every dose.");
    // The doses fold away (30 and more can work on one champion); the fold is kept per champion
    if (!live.poisonDoses.empty())
    {
      ImGui::PushID((int)live.who);
      ImGui::Indent(LIVE_LABEL_WIDTH);
      char title[64];
      snprintf(title, sizeof title, "Show the %d %s###doses", (int)live.poisonDoses.size(), live.poisonDoses.size() == 1 ? "dose" : "doses");
      bool open = ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_SpanLabelWidth);
      if (open)
      {
        ImGui::PushTextWrapPos(wrapAt);
        for (size_t i = 0; i < live.poisonDoses.size(); i++)
        {
          int dose = live.poisonDoses[i];
          ImGui::TextColored(Colour(PART_RGB), "%d  (%d HP per 36 ticks, %d ticks left)", dose, std::max(1, dose / 64), dose * 36);
        }
        ImGui::PopTextWrapPos();
        ImGui::TreePop();
      }
      ImGui::Unindent(LIVE_LABEL_WIDTH);
      ImGui::PopID();
    }
  }
  else ImGui::TextColored(Colour(DIM_RGB), "none");
}

// The portrait, the name in the champion's colour, the title, and what is wrong in a red pill
static void LiveHead(const HeroStatsLiveChampion &live, ImU32 rgb, float width)
{
  float left = ImGui::GetCursorPosX();
  int w = 0, h = 0;
  ImTextureID portrait = livePictures != NULL && livePictures->portrait != NULL ? livePictures->portrait(live.who, &w, &h) : 0;
  if (portrait != 0 && w > 0 && h > 0)
  {
    ImVec2 at = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(at, ImVec2(at.x + w * 2 + 2, at.y + h * 2 + 2), Packed(SLOT_RGB));
    drawList->AddImage(ImTextureRef(portrait), ImVec2(at.x + 1, at.y + 1), ImVec2(at.x + 1 + w * 2, at.y + 1 + h * 2));
    drawList->AddRect(at, ImVec2(at.x + w * 2 + 2, at.y + h * 2 + 2), Packed(BORDER_RGB));
    ImGui::Dummy(ImVec2(w * 2 + 2.0f, h * 2 + 2.0f));
    ImGui::SameLine(0, 10);
  }
  std::string status;
  if (live.health <= 0) status = "dead";
  else if (live.poisonCount > 0) status = "poisoned";
  ImGui::BeginGroup();
  ImGui::Dummy(ImVec2(0, portrait != 0 ? 8.0f : 0.0f));
  ImGui::TextColored(Colour(rgb), "%s", live.name.c_str());
  if (!live.title.empty()) ImGui::TextColored(Colour(DIM_RGB), "%s", live.title.c_str());
  ImGui::EndGroup();
  if (!status.empty())
  {
    ImVec2 size = ImGui::CalcTextSize(status.c_str());
    float x = left + width - size.x - 6;
    ImGui::SameLine(std::max(x, ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + 10));
    ImVec2 at = ImGui::GetCursorScreenPos();
    at.y += portrait != 0 ? 8.0f : 0.0f;
    PillBehind(ImGui::GetWindowDrawList(), at, size);
    ImGui::GetWindowDrawList()->AddText(at, Packed(BAD_RGB), status.c_str());
    ImGui::Dummy(size);
  }
  ImGui::Dummy(ImVec2(0, 4));
}

// The attributes, current and max: drained below the max in red, raised above it in gold
static void LiveAttributes(const HeroStatsLiveChampion &live)
{
  if (!BeginPageTable("##attributes", 3, false)) return;
  LabelColumn("Attribute");
  NumberColumn("Current");
  NumberColumn("Max");
  HeaderRow(std::vector<int>(), std::vector<HeroStatsColumn>());
  for (int a = 0; a < HERO_LIVE_ATTRIBUTE_COUNT; a++)
  {
    NextGuidedRow();
    LabelCell(HeroStatsLive_AttributeName(a), 0, TEXT_RGB);
    int now = live.attributeNow[a], most = live.attributeMax[a];
    ImGui::TableSetColumnIndex(1);
    RightText(std::to_string(now), now < most ? BAD_RGB : (now > most ? GOLD_RGB : TEXT_RGB));
    ImGui::TableSetColumnIndex(2);
    RightText(std::to_string(most), TEXT_RGB);
  }
  ImGui::EndTable();
}

// The four classes: the rank, and a bar of the way to the next that takes the room left
static void LiveRanks(const HeroStatsLiveChampion &live, ImU32 rgb, float width)
{
  float ranks = 0;
  for (int cls = 0; cls < 4; cls++)
  {
    std::string rank = HeroXp_RankName(live.rank[cls]);
    ranks = std::max(ranks, ImGui::CalcTextSize(rank.empty() ? "-" : rank.c_str()).x);
  }
  float left = ImGui::GetCursorPosX();
  float lineHeight = ImGui::GetTextLineHeight();
  float barLeft = LIVE_LABEL_WIDTH + ranks + 12;
  float barWidth = std::max(30.0f, width - barLeft);
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  for (int cls = 0; cls < 4; cls++)
  {
    std::string rank = HeroXp_RankName(live.rank[cls]);
    ImGui::TextColored(Colour(DIM_RGB), "%s", HeroXp_ClassName(cls));
    ImGui::SameLine(left + LIVE_LABEL_WIDTH);
    ImGui::TextColored(Colour(rank.empty() ? DIM_RGB : TEXT_RGB), "%s", rank.empty() ? "-" : rank.c_str());
    ImGui::SameLine(left + barLeft);
    ImVec2 at = ImGui::GetCursorScreenPos();
    ImVec2 top(at.x, at.y + lineHeight / 2 - 4), bottom(at.x + barWidth, at.y + lineHeight / 2 + 5);
    drawList->AddRectFilled(top, bottom, Packed(TRACK_RGB), 2.0f);
    if (live.percentToNext[cls] > 0)
      drawList->AddRectFilled(top, ImVec2(top.x + barWidth * live.percentToNext[cls] / 100.0f, bottom.y), Packed(rgb), 2.0f);
    ImGui::Dummy(ImVec2(barWidth, lineHeight));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%d%% of the way to the next rank", live.percentToNext[cls]);
  }
}

// The inventory as the game lays it out: the body on a grid of five by five, then the backpack
static const float DOLL_WIDTH = HERO_LIVE_DOLL_COLUMNS * (SLOT_SIZE + SLOT_GAP) - SLOT_GAP;
static const float PACK_WIDTH = HERO_LIVE_BACKPACK_ACROSS * (SLOT_SIZE + SLOT_GAP) - SLOT_GAP;
static const float PICTURE_GAP = 20;

static void SmallCaption(ImVec2 at, const char *text)
{
  float small = ImGui::GetFontSize() * 0.78f;
  std::string upper = text;
  for (size_t i = 0; i < upper.size(); i++) upper[i] = (char)toupper((unsigned char)upper[i]);
  ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), small, at, Packed(FAINT_RGB), upper.c_str());
}

static void LivePicture(const HeroStatsLiveChampion &live, bool packBeside)
{
  static const char *const CAPTION[13] = {"hand", "hand", "head", "torso", "legs", "feet", "", "", "", "", "neck", "", ""};
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float step = SLOT_SIZE + SLOT_GAP;
  for (int slot = 0; slot < HERO_LIVE_FIRST_BACKPACK_SLOT; slot++)
  {
    int column = 0, row = 0;
    if (!HeroStatsLive_DollCell(slot, &column, &row)) continue;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (column - 1) * step, origin.y + (row - 1) * step));
    DrawSlot(live.items, ItemIn(live.items, slot), HeroStatsLive_SlotName(slot), SLOT_SIZE, IsWounded(live, slot), CAPTION[slot]);
  }
  float captionY = origin.y + 3 * step - ImGui::GetFontSize() * 0.78f - 5;
  SmallCaption(ImVec2(origin.x, captionY), "Pouch");
  SmallCaption(ImVec2(origin.x + 3 * step, captionY), "Quiver");
  float dollHeight = HERO_LIVE_DOLL_ROWS * step - SLOT_GAP;
  ImVec2 pack = packBeside ? ImVec2(origin.x + DOLL_WIDTH + PICTURE_GAP, origin.y) : ImVec2(origin.x, origin.y + dollHeight + 14);
  SmallCaption(pack, "Backpack");
  float packTop = pack.y + ImGui::GetFontSize() * 0.78f + 6;
  int count = HERO_LIVE_SLOT_COUNT - HERO_LIVE_FIRST_BACKPACK_SLOT;
  for (int i = 0; i < count; i++)
  {
    int slot = HERO_LIVE_FIRST_BACKPACK_SLOT + i;
    ImGui::SetCursorScreenPos(ImVec2(pack.x + (i % HERO_LIVE_BACKPACK_ACROSS) * step, packTop + (i / HERO_LIVE_BACKPACK_ACROSS) * step));
    DrawSlot(live.items, ItemIn(live.items, slot), HeroStatsLive_SlotName(slot), SLOT_SIZE, false, NULL);
  }
  int packRows = (count + HERO_LIVE_BACKPACK_ACROSS - 1) / HERO_LIVE_BACKPACK_ACROSS;
  float packBottom = packTop + packRows * step - SLOT_GAP;
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(packBeside ? DOLL_WIDTH + PICTURE_GAP + PACK_WIDTH : std::max(DOLL_WIDTH, PACK_WIDTH),
                      std::max(origin.y + dollHeight, packBottom) - origin.y));
}

// Every slot that holds an item, a chest's contents indented under it, each with its icon, its
// charges after the name and its weight
static void LiveList(const HeroStatsLiveChampion &live)
{
  if (live.items.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "Carries nothing.");
    return;
  }
  if (!BeginPageTable("##items", 3, false)) return;
  TextColumn("Slot");
  LabelColumn("Item");
  NumberColumn("Weight");
  HeaderRow(std::vector<int>(), std::vector<HeroStatsColumn>());
  for (size_t i = 0; i < live.items.size(); i++)
  {
    const HeroStatsLiveItem &item = live.items[i];
    NextGuidedRow();
    ImGui::TableSetColumnIndex(0);
    if (!item.inChest) ImGui::TextColored(Colour(Hot(DIM_RGB)), "%s", HeroStatsLive_SlotName(item.slot).c_str());
    ImGui::TableSetColumnIndex(1);
    float indent = item.inChest ? 14.0f : 0.0f;
    std::string charges = HeroStatsLive_ChargesText(item.charges, item.gulps);
    std::string after = charges.empty() ? "" : std::string(" ") + MIDDLE_DOT + " " + charges;
    NoteWidth(indent + 22 + ImGui::CalcTextSize((item.name + after).c_str()).x);
    if (indent > 0) ImGui::Indent(indent);
    ImVec2 at = ImGui::GetCursorScreenPos();
    float line = ImGui::GetTextLineHeight();
    IconAt(ImGui::GetWindowDrawList(), item, ImVec2(at.x, at.y + (line - 16) / 2), 16);
    ImGui::Dummy(ImVec2(16, line));
    ImGui::SameLine(0, 6);
    // A name too long for a narrow list goes on under itself, its charges on a line of their own
    bool wraps = InMeasuredTable() && measuring->fit.wrap;
    if (wraps) ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(Colour(Hot(item.inChest ? PART_RGB : TEXT_RGB)), "%s", item.name.c_str());
    if (!after.empty())
    {
      if (wraps) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 22);
      else ImGui::SameLine(0, 0);
      ImGui::TextColored(Colour(DIM_RGB), "%s", wraps ? charges.c_str() : after.c_str());
    }
    if (wraps) ImGui::PopTextWrapPos();
    if (indent > 0) ImGui::Unindent(indent);
    ImGui::TableSetColumnIndex(2);
    RightText(HeroStatsLive_WeightText(item.weight, false), item.inChest ? PART_RGB : TEXT_RGB);
  }
  ImGui::EndTable();
}

// The Inventory section: folds like the others; Picture or List
static void LiveInventory(HeroStatsDrawState *state, const HeroStatsLiveChampion &live, bool packBeside)
{
  int count = (int)live.items.size();
  if (!SectionHeader(state, HERO_STATS_INVENTORY_SECTION, "Inventory", Count(count, count == 1 ? "item" : "items"))) return;
  ImGui::Dummy(ImVec2(0, 2));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, ImGui::GetStyle().ItemSpacing.y));
  if (ChoiceButton("Picture", !state->inventoryList)) state->inventoryList = false;
  ImGui::SameLine();
  if (ChoiceButton("List", state->inventoryList)) state->inventoryList = true;
  ImGui::PopStyleVar();
  ImGui::Dummy(ImVec2(0, 4));
  if (state->inventoryList) LiveList(live);
  else LivePicture(live, packBeside);
  SectionGap();
}

// A card of the Party's page: who, how they are, what they hold and wear, and their ranks. True
// when the card itself was clicked (not a slot or a fold in it).
static bool LiveCard(const HeroStatsLiveChampion &live, ImU32 rgb, float width)
{
  bool clicked = false;
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Colour(PANEL_RGB));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
  ImGui::PushID((int)live.who);
  if (ImGui::BeginChild("##card", ImVec2(width, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
  {
    float inner = ImGui::GetContentRegionAvail().x;
    LiveHead(live, rgb, inner);
    LiveVitals(live, rgb, inner);
    ImGui::Dummy(ImVec2(0, 4));
    // What is held and worn: both hands, head, neck, torso, legs, feet
    static const int STRIP[7] = {0, 1, 2, 10, 3, 4, 5};
    const float size = 36;
    float left = ImGui::GetCursorPosX();
    for (int i = 0; i < 7; i++)
    {
      if (i > 0)
      {
        ImGui::SameLine(0, SLOT_GAP);
        if (ImGui::GetCursorPosX() + size > left + inner)
        {
          ImGui::NewLine();
          ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y + SLOT_GAP);
        }
      }
      DrawSlot(live.items, ItemIn(live.items, STRIP[i]), HeroStatsLive_SlotName(STRIP[i]), size, IsWounded(live, STRIP[i]), NULL);
    }
    int carried = HeroStatsLive_CarriedCount(live.items);
    if (carried > 0)
    {
      std::string more = "+" + std::to_string(carried) + " carried";
      ImGui::SameLine(0, 10);
      if (ImGui::GetCursorPosX() + ImGui::CalcTextSize(more.c_str()).x > left + inner) ImGui::NewLine();
      else ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (size - ImGui::GetTextLineHeight()) / 2);
      ImGui::TextColored(Colour(DIM_RGB), "%s", more.c_str());
    }
    ImGui::Dummy(ImVec2(0, 4));
    LiveRanks(live, rgb, inner);
    clicked = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered();
  }
  ImGui::EndChild();
  ImGui::PopID();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
  return clicked;
}

// One champion's page: the champion, attributes and ranks one under the other, the inventory in
// a column of its own beside them, or last when the window is narrow
static void DrawLiveChampion(HeroStatsDrawState *state, const HeroStatsLiveChampion &live, ImU32 rgb)
{
  const float COLUMN_GAP = 36, LEFT_SMALLEST = 300;
  float available = ImGui::GetContentRegionAvail().x;
  float pictureWide = DOLL_WIDTH + PICTURE_GAP + PACK_WIDTH, pictureTall = std::max(DOLL_WIDTH, PACK_WIDTH);
  bool closed = (state->closedSections & HeroStatsDraw_SectionBit(HERO_STATS_INVENTORY_SECTION)) != 0;
  // The backpack stands beside the body when the champion's column still gets its full width
  bool packBeside = available >= HERO_LAYOUT_CHAMPION_WIDEST + COLUMN_GAP + pictureWide;
  // The list is as wide as its table needed when last drawn (420 before that), the picture as its slots
  static float listNeeds = 0;
  float listWidth = std::min(available, std::max(420.0f, listNeeds));
  float inventoryWidth = state->inventoryList || closed ? listWidth : packBeside ? pictureWide : pictureTall;
  bool two = available >= LEFT_SMALLEST + COLUMN_GAP + inventoryWidth;
  float leftWidth = std::min(HERO_LAYOUT_CHAMPION_WIDEST, two ? available - COLUMN_GAP - inventoryWidth : available);
  if (!two) packBeside = available >= pictureWide;
  ImGui::PushID((int)live.who);
  ImGuiWindowFlags plain = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  if (ImGui::BeginChild("##self", ImVec2(leftWidth, 0), ImGuiChildFlags_AutoResizeY, plain))
  {
    LiveHead(live, rgb, leftWidth);
    LiveVitals(live, rgb, leftWidth);
    ImGui::Dummy(ImVec2(0, 8));
    if (SectionHeader(state, HERO_STATS_ATTRIBUTES_SECTION, "Attributes", ""))
    {
      LiveAttributes(live);
      SectionGap();
    }
    if (SectionHeader(state, HERO_STATS_RANKS_SECTION, "Ranks", "way to the next rank"))
    {
      ImGui::Dummy(ImVec2(0, 4));
      LiveRanks(live, rgb, leftWidth);
      SectionGap();
    }
  }
  ImGui::EndChild();
  if (two) ImGui::SameLine(0, COLUMN_GAP);
  if (ImGui::BeginChild("##inventory", ImVec2(two ? inventoryWidth : std::min(available, std::max(inventoryWidth, leftWidth)), 0),
                        ImGuiChildFlags_AutoResizeY, plain))
  {
    widestTableBegun = 0;
    LiveInventory(state, live, packBeside);
    if (state->inventoryList && widestTableBegun > 0) listNeeds = widestTableBegun;
  }
  ImGui::EndChild();
  ImGui::PopID();
}

static void DrawChampionsPage(HeroStatsDrawState *state, const HeroStatsLiveParty &live,
                              const std::vector<HeroStatsColumn> &columns)
{
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
  bool visible = ImGui::BeginChild("##champions", ImGui::GetContentRegionAvail(), ImGuiChildFlags_AlwaysUseWindowPadding);
  ImGui::PopStyleVar();
  if (visible)
  {
    bool isParty = state->who == HERO_STATS_PARTY;
    if (live.champions.empty()) ImGui::TextColored(Colour(DIM_RGB), "No champion in the party.");
    else if (!isParty && HeroStatsLive_Find(live, state->who) == NULL)
      ImGui::TextColored(Colour(DIM_RGB), "%s is not in the party now.", WhoLabel(columns, state->who).c_str());
    if (isParty && !live.cursor.name.empty())
    {
      // The item on the cursor, as a slot of its own
      std::vector<HeroStatsLiveItem> cursor(1, live.cursor);
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(Colour(DIM_RGB), "On the cursor:");
      ImGui::SameLine(0, 10);
      float y = ImGui::GetCursorPosY();
      DrawSlot(cursor, 0, "On the cursor", 36, false, NULL);
      ImGui::SameLine(0, 10);
      ImGui::SetCursorPosY(y + (36 - ImGui::GetTextLineHeight()) / 2);
      ImGui::TextColored(Colour(TEXT_RGB), "%s", live.cursor.name.c_str());
      ImGui::SetCursorPosY(y + 36 + 8);
    }
    if (isParty && !live.champions.empty())
    {
      // Four, two or one cards across, never three and one; a card is no wider than its widest
      const float gap = 10;
      float available = ImGui::GetContentRegionAvail().x;
      int count = (int)live.champions.size();
      int across = HeroStatsLayout_Across(count, available, HERO_LAYOUT_CARD_SMALLEST, gap);
      float width = std::min(HERO_LAYOUT_CARD_WIDEST, (available - gap * (across - 1)) / across);
      HeroStatsChampion picked = HERO_STATS_PARTY;
      bool anyPicked = false;
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
      for (int i = 0; i < count; i++)
      {
        const HeroStatsLiveChampion &champion = live.champions[i];
        if (i % across != 0) ImGui::SameLine(0, gap);
        ImGui::PopStyleVar();
        bool clicked = LiveCard(champion, ChampionRgb(ColumnOf(columns, champion.who)), width);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
        if (clicked)
        {
          picked = champion.who;
          anyPicked = true;
        }
      }
      ImGui::PopStyleVar();
      if (anyPicked) state->who = picked;
    }
    else if (!isParty)
    {
      const HeroStatsLiveChampion *champion = HeroStatsLive_Find(live, state->who);
      if (champion != NULL) DrawLiveChampion(state, *champion, ChampionRgb(ColumnOf(columns, champion->who)));
    }
  }
  ImGui::EndChild();
}

void HeroStatsDraw_Frame(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                         const HeroStatsPicker &picker, const HeroStatsLiveParty &live, const HeroStatsDrawPictures &pictures,
                         void (*exportStatistics)(void))
{
  livePictures = &pictures;
  DrawTitle(*state, columns, exportStatistics);
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 6));
  float whoWidth = DrawWhoRow(state, columns);
  // The row of level numbers that does not fit even on a line of its own becomes the stepper
  HeroStatsPicker fitted = picker;
  if (!fitted.stepper && GUTTER * 2 + PickerWidth(fitted) > ImGui::GetWindowWidth()) fitted.stepper = true;
  float pickerLeft = ImGui::GetWindowWidth() - GUTTER - PickerWidth(fitted);
  if (pickerLeft > GUTTER + whoWidth + 16) ImGui::SetCursorPosX(pickerLeft);
  else
  {
    // Too narrow to sit beside the champions: a line of its own
    ImGui::NewLine();
    ImGui::SetCursorPosX(GUTTER);
  }
  // The levels and Per level are the statistics': the Champions tab shows the party now
  if (state->topic != HERO_STATS_CHAMPIONS_TAB) DrawPicker(state, fitted);
  else ImGui::NewLine();
  DrawTabs(state);
  if (state->topic == HERO_STATS_CHAMPIONS_TAB)
  {
    DrawChampionsPage(state, live, columns);
    return;
  }
  // Steps and items count before a champion joins: only a game with no level yet has nothing
  if (stats.Champions().empty() && stats.VisitedLevels().empty())
  {
    ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 8));
    ImGui::TextColored(Colour(DIM_RGB), "No statistics yet.");
    return;
  }
  DrawPage(state, stats, columns);
}
