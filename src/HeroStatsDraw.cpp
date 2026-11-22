// HeroStatsDraw.cpp : the pages of the Statistics window, drawn with Dear ImGui. Every page is
// made from HeroStatsCatalogue through HeroStatsPages; nothing here counts anything.
#include "stdafx.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
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
static const ImU32 TOOLTIP_BORDER_RGB = 0x7a8294;  // a tooltip is told from the rows by its edge
static const ImU32 WHITE_RGB = 0xffffff;
static const ImU32 TEXT_RGB = 0xd8d8d8;
static const ImU32 PART_RGB = 0xb4b6bf;       // a row that is a part of the row above
static const ImU32 DIM_RGB = 0x8a8d98;        // labels, summaries, table headers
static const ImU32 FAINT_RGB = 0x5a5d68;      // the dot of a zero
static const ImU32 BUTTON_RGB = 0x2c3e5c;
static const ImU32 BUTTON_HOVERED_RGB = 0x36507a;
static const ImU32 BUTTON_ON_RGB = 0x4272b8;  // what is picked
static const ImU32 PICKED_GROUND_RGB = 0x2a3a57;  // the ground of a picked champion's button
static const ImU32 GUIDE_EDGE_RGB = 0x5b8fd9;  // the edge of what is picked or under the pointer, and the hovered column's line
static const ImU32 BORDER_RGB = 0x3a3d46;
static const ImU32 LINE_RGB = 0x31343d;
static const ImU32 ROW_LINE_RGB = 0x292b32;
static const ImU32 TRACK_RGB = 0x2f323a;      // the empty part of a bar
static const ImU32 BAD_RGB = 0xe5735f;
static const ImU32 YELLOW_RGB = 0xe8d44a;     // food and water below 0
static const ImU32 FOOD_RGB = 0x9b6a3c;       // the food bar, brown as in the game
static const ImU32 WATER_RGB = 0x4a8fe0;      // the water bar
static const ImU32 GOLD_RGB = 0xe0b54a;       // a champion's record that is the party's, always with its star
static const ImU32 RAISED_RGB = 0xb48ce6;     // an attribute above its max: violet, no champion's colour
static const ImU32 DAMAGE_RGB = 0xe39a7c;     // a weapon's damage and kinetic energy
static const ImU32 ARMOR_RGB = 0xa9b8cf;      // a piece of armor's strength
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

// Text bigger than the body text is drawn with the scalable font when the body font is a bitmap:
// scaled up, a bitmap font shows its pixels
static ImFont *scalableFont = NULL;

void HeroStatsDraw_SetScalableFont(ImFont *font)
{
  scalableFont = font;
}

static ImFont *FontFor(float fontSize)
{
  return scalableFont != NULL && fontSize > ImGui::GetFontSize() + 0.5f ? scalableFont : ImGui::GetFont();
}

static float TextPieceWidth(float fontSize, const std::string &text, size_t from, size_t end)
{
  return FontFor(fontSize)->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str() + from, text.c_str() + end).x;
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
    drawList->AddText(FontFor(fontSize), fontSize, at, colour, text.c_str() + from, text.c_str() + end);
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

// The explanations of rows, columns and tiles show when the pointer rests on them
// A tooltip window: room around the text, so that its first letters are not cut off, and an edge
// light enough to tell it from the rows under it
static bool BeginTip(void)
{
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 5));
  ImGui::PushStyleColor(ImGuiCol_Border, Colour(TOOLTIP_BORDER_RGB));
  bool open = ImGui::BeginTooltip();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  return open;
}

// A tooltip that wraps instead of running out of the window
static void WrappedTooltip(const char *text)
{
  if (!BeginTip()) return;
  ImGui::PushTextWrapPos(std::min(ImGui::GetFontSize() * 24.0f, ImGui::GetIO().DisplaySize.x - 24.0f));
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
  ImGui::EndTooltip();
}

static void HelpTooltip(const char *text)
{
  if (text[0] != 0 && ImGui::IsItemHovered()) WrappedTooltip(text);
}

static void TooltipWithSymbols(const std::string &text)
{
  if (!BeginTip()) return;
  TextPackedWithSymbols(ImGui::GetColorU32(ImGuiCol_Text), text);
  ImGui::EndTooltip();
}

static void (*tickSink)(int tick) = NULL;
void HeroStatsDraw_SetTickSink(void (*sink)(int tick)) { tickSink = sink; }

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
  style.PopupBorderSize = 1;
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
// numbers (512), then the tables that came later (Skills 1024 ... Records 8192), the character
// sheet's sections (16384 ... 131072) and Damage reduced by source (262144)
int HeroStatsDraw_SectionBit(int detail)
{
  if (detail == HeroDetailReduced) return 1 << 18;
  if (detail == HERO_STATS_ATTRIBUTES_SECTION) return 1 << 14;
  if (detail == HERO_STATS_RANKS_SECTION) return 1 << 15;
  if (detail == HERO_STATS_INVENTORY_SECTION) return 1 << 16;
  if (detail == HERO_STATS_PANELS_SECTION) return 1 << 17;
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

// A button of a group where one is picked (the level picker, Show, Number). faint: a number the
// game ignores (Resistance in old replays), greyed while it is not picked
static bool ChoiceButton(const char *label, bool isOn, bool faint = false)
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
    ImGui::PushStyleColor(ImGuiCol_Text, Colour(faint ? FAINT_RGB : DIM_RGB));
  }
  bool clicked = ImGui::Button(label);
  ImGui::PopStyleColor(3);
  return clicked;
}

// Greyed: Resistance in a replay of a version that ignored it (always 0)
static bool IgnoredNumber(HeroStatsNumber number)
{
  return number == HeroNumReducedResistance && HeroStats_ResistanceIgnored();
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

// Bad news as text where the cursor is: always on its pill, never red alone. wrapWidth as
// CalcTextSize takes it, for text under PushTextWrapPos.
static void PillText(const std::string &text, float wrapWidth = -1.0f)
{
  ImVec2 at = ImGui::GetCursorScreenPos();
  PillBehind(ImGui::GetWindowDrawList(), at, ImGui::CalcTextSize(text.c_str(), NULL, false, wrapWidth));
  ImGui::TextColored(Colour(BAD_RGB), "%s", text.c_str());
}

// ---------------------------------------------------------------------------------------------
// Who, the level picker, the export button and the topic tabs

// A button of the who row: picked, it is filled and has the picked colour's border
static bool WhoButton(const char *label, bool on, bool dimmed)
{
  ImGui::PushStyleColor(ImGuiCol_Button, Colour(on ? PICKED_GROUND_RGB : BACKGROUND_RGB));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colour(on ? PICKED_GROUND_RGB : PANEL_HOVERED_RGB));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colour(PANEL_HOVERED_RGB));
  ImGui::PushStyleColor(ImGuiCol_Border, Colour(on ? GUIDE_EDGE_RGB : BORDER_RGB));
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
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetFrameHeight() / 2);  // round ends
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
  ImGui::PopStyleVar(2);
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
static const ImU32 GUIDE_CROSS_RGB = 0x36405a; // where the row and the column meet

// How wide a table's columns are. No column has a width of its own: a number column is as wide
// as its widest text, which the cells note as they are drawn, so a table is laid out with the
// widths of the frame before (its first frame with the defaults above). What the table gives up
// when the page is too narrow is HeroStatsLayout_FitTable's.
// ColumnFirstToGo: a number column the table leaves out as soon as it does not fit with everything
// shown, before it gives up anything else (the inventory list's Kinetic)
enum ColumnKind { ColumnLabel, ColumnNumber, ColumnDerived, ColumnBar, ColumnText, ColumnFirstToGo };
struct TableMeasure
{
  std::vector<int> kinds, lastKinds;  // of this frame as the columns are set up, and of the last
  std::vector<float> seen, width;     // the widest text noted this frame, and the last frame's
  std::vector<bool> disabled;         // columns the fit dropped this frame: they note nothing
  // What a column needs in each form, whatever form it is drawn in. This frame's: its header with
  // a champion's name and with the dot alone, its widest cell with numbers in full and in short
  std::vector<float> headFull, headDot, cellFull, cellShort;
  // The last frame's column widths: names and full numbers, dots and full numbers, dots and short numbers
  std::vector<float> fullWidth, dotWidth, shortWidth;
  HeroStatsTableFit fit;
  bool keepFirst;                     // the columns that are the first to go show this frame
  bool known;                         // the last frame measured this table with these columns
};
static std::map<ImGuiID, TableMeasure> tableMeasures;
// The widest full width (HeroStatsLayout_FullWidth) of the tables begun since it was last set to
// 0: what a section needs to stand beside another
static float widestTableBegun = 0;
// The tables begun this frame, to tell afterwards whether any was laid out with stale widths
static std::vector<TableMeasure *> tablesThisFrame;
// Frames still to draw unseen after the page, the champion, the level or the window's width
// changed: children that size themselves and sections side by side take a few to settle
static int settlingPasses = 0;
static bool sectionsMoved = false;
static TableMeasure *measuring = NULL;
static ImGuiID measuringId = 0;
static bool rowIsHot = false;
static bool tightPadding = false;  // the table being drawn has half its cell padding pushed

// The table being drawn is the one BeginPageTable began
static bool InMeasuredTable(void)
{
  ImGuiTable *table = ImGui::GetCurrentTable();
  return measuring != NULL && table != NULL && table->ID == measuringId;
}

// A cell's text is this wide in full, and this wide with its number in short: its column must be
// as wide as the form the table is drawn in
static void NoteCell(float full, float inShort)
{
  if (!InMeasuredTable()) return;
  int column = ImGui::TableGetColumnIndex();
  if (column < 0 || column >= (int)measuring->seen.size()) return;
  measuring->cellFull[column] = std::max(measuring->cellFull[column], full);
  measuring->cellShort[column] = std::max(measuring->cellShort[column], inShort);
  measuring->seen[column] = std::max(measuring->seen[column], measuring->fit.shortNumbers ? inShort : full);
}

// A header is this wide with its champion's name, and this wide with the dot alone
static void NoteHead(float full, float dotOnly)
{
  if (!InMeasuredTable()) return;
  int column = ImGui::TableGetColumnIndex();
  if (column < 0 || column >= (int)measuring->seen.size()) return;
  measuring->headFull[column] = std::max(measuring->headFull[column], full);
  measuring->headDot[column] = std::max(measuring->headDot[column], dotOnly);
  measuring->seen[column] = std::max(measuring->seen[column], measuring->fit.names ? full : dotOnly);
}

// A cell's text is this wide: its column must be
static void NoteWidth(float width)
{
  NoteCell(width, width);
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

static void WrappedTooltip(const char *text);
static void DottedUnderline(float left, float right, float y);

// A number in a table cell: a dim dot for zero, bad news on a red pill. In a table that fits no
// other way a plain number from 10,000 on is short (86k, 1.2M), with the number as its tooltip;
// a rate, a weight or a record with words is never short.
static void NumberCell(const std::string &text, int value, bool isBad, ImU32 rgb)
{
  if (value == 0)
  {
    RightText(MIDDLE_DOT, FAINT_RGB);
    return;
  }
  bool hasShort = value >= 10000 && text == HeroStatsView_Number(value);
  std::string inShort = hasShort ? HeroStatsView_ShortNumber(value) : text;
  bool isShort = hasShort && InMeasuredTable() && measuring->fit.shortNumbers;
  const std::string &shown = isShort ? inShort : text;
  float width = ImGui::CalcTextSize(shown.c_str()).x;
  NoteCell(ImGui::CalcTextSize(text.c_str()).x, ImGui::CalcTextSize(inShort.c_str()).x);
  float room = ImGui::GetContentRegionAvail().x - width;
  if (room > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room);
  bool pill = isBad && value > 0;
  if (pill) PillBehind(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ImVec2(width, ImGui::GetTextLineHeight()));
  ImGui::TextColored(Colour(pill ? BAD_RGB : Hot(rgb)), "%s", shown.c_str());
  if (isShort && ImGui::IsItemHovered()) WrappedTooltip(text.c_str());
}

static void CatalogueCell(HeroStatsNumber number, int value, ImU32 rgb)
{
  const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(number);
  // A rate's or an average's 0 is worked out from something (0%: none of the casts worked): written, not a dot
  NumberCell(HeroStatsCatalogue_Format(number, value), info.isRatio && value <= 0 ? 1 : value, info.isBad, rgb);
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
    if (measure.fullWidth.size() != measure.kinds.size())
    {
      measure.fullWidth.assign(measure.kinds.size(), 0.0f);
      measure.dotWidth.assign(measure.kinds.size(), 0.0f);
      measure.shortWidth.assign(measure.kinds.size(), 0.0f);
    }
    for (size_t i = 0; i < measure.kinds.size() && i < measure.headFull.size(); i++)
    {
      if (i < measure.disabled.size() && measure.disabled[i]) continue;
      measure.fullWidth[i] = std::max(measure.headFull[i], measure.cellFull[i]);
      measure.dotWidth[i] = std::max(measure.headDot[i], measure.cellFull[i]);
      measure.shortWidth[i] = std::max(measure.headDot[i], measure.cellShort[i]);
    }
    measure.lastKinds = measure.kinds;
  }
  ImGuiTable *last = ImGui::TableFindByID(tableId);
  float available = ImGui::GetContentRegionAvail().x;
  float padding = ImGui::GetStyle().CellPadding.x * 2;
  measure.known = (int)measure.lastKinds.size() == columnCount && last != NULL && last->ColumnsCount == columnCount;
  HeroStatsTableFit everything = {true, true, false, false, 0, BAR_WIDTH + padding, available, true, true, false};
  measure.fit = everything;
  measure.keepFirst = true;
  if (measure.known)
  {
    HeroStatsTableNeeds needs = {0, 0, 0, false, 0, 0, 0, 0};
    float firstToGo = 0;
    for (int i = 0; i < columnCount; i++)
    {
      float width = measure.width[i];
      // A text column sizes itself to what is in it (ImGui's own measure of the last frame)
      if (measure.lastKinds[i] == ColumnText) width = last->Columns[i].WidthAuto;
      switch (measure.lastKinds[i])
      {
      case ColumnLabel: needs.label += width + padding; break;
      case ColumnDerived: needs.derived += width + padding; break;
      case ColumnFirstToGo: firstToGo += width + padding; break;
      case ColumnBar:
        // A bar is never narrower than its header ("Share of party", "Who: Experience")
        needs.hasBar = true;
        needs.barHeader = width + padding;
        break;
      case ColumnNumber:
      {
        // What it needs with everything shown, and what it can give up
        bool formsKnown = i < (int)measure.fullWidth.size() && measure.fullWidth[i] > 0;
        float full = formsKnown ? measure.fullWidth[i] : width;
        needs.numbers += full + padding;
        needs.padding += padding / 2;
        if (formsKnown)
        {
          needs.names += full - measure.dotWidth[i];
          needs.shortNumbers += measure.dotWidth[i] - measure.shortWidth[i];
        }
        break;
      }
      default: needs.numbers += width + padding; break;
      }
    }
    // What is the first to go stays only while the whole table fits; then it is a column like the others
    float everythingShown = HeroStatsLayout_FullWidth(needs) + firstToGo;
    measure.keepFirst = firstToGo == 0 || HeroStatsLayout_ListValues(everythingShown, firstToGo, needs.derived, available) == HeroListAllValues;
    if (measure.keepFirst) needs.numbers += firstToGo;
    measure.fit = HeroStatsLayout_FitTable(needs, available);
    // A number column is as wide as the form it is drawn in
    for (int i = 0; i < columnCount; i++)
      if (measure.lastKinds[i] == ColumnNumber && i < (int)measure.fullWidth.size() && measure.fullWidth[i] > 0)
        measure.width[i] = measure.fit.shortNumbers ? measure.shortWidth[i] : measure.fit.names ? measure.fullWidth[i] : measure.dotWidth[i];
    widestTableBegun = std::max(widestTableBegun, everythingShown);
  }
  measure.kinds.clear();
  measure.seen.assign(columnCount, 0.0f);
  measure.disabled.assign(columnCount, false);
  measure.headFull.assign(columnCount, 0.0f);
  measure.headDot.assign(columnCount, 0.0f);
  measure.cellFull.assign(columnCount, 0.0f);
  measure.cellShort.assign(columnCount, 0.0f);
  measuring = &measure;
  measuringId = tableId;
  tablesThisFrame.push_back(&measure);
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
  // A table that gave up half its padding keeps it so until EndPageTable
  tightPadding = measure.known && !measure.fit.fullPadding;
  if (tightPadding) ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(padding / 4, ImGui::GetStyle().CellPadding.y));
  if (!ImGui::BeginTable(id, columnCount, flags, size, measure.fit.scroll ? measure.fit.tableWidth : 0.0f))
  {
    if (tightPadding) ImGui::PopStyleVar();
    tightPadding = false;
    measuring = NULL;
    return false;
  }
  if (measure.fit.scroll) ImGui::TableSetupScrollFreeze(1, 0);
  return true;
}

// Ends a table BeginPageTable began
static void EndPageTable(void)
{
  ImGui::EndTable();
  if (tightPadding) ImGui::PopStyleVar();
  tightPadding = false;
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
// firstToGo: left out before the table gives up anything else.
static void NumberColumn(const char *label, bool sortsFirst = false, bool derived = false, bool firstToGo = false)
{
  bool off = measuring != NULL && measuring->known && (firstToGo ? !measuring->keepFirst : derived && !measuring->fit.derived);
  int index = AddColumn(firstToGo ? ColumnFirstToGo : derived ? ColumnDerived : ColumnNumber, off);
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
static void TextColumn(const char *label, bool sortable = false, bool sortsFirst = false)
{
  AddColumn(ColumnText, false);
  ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_WidthFixed;
  if (!sortable) flags |= ImGuiTableColumnFlags_NoSort;
  if (sortsFirst) flags |= ImGuiTableColumnFlags_DefaultSort;
  ImGui::TableSetupColumn(label, flags);
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
// helps: a tooltip per table column (empty or missing: none), for what a header does not say.
// lastRoom: the widest the last header can be ("Who: Successful casts"), so that the table is as
// wide whatever it is sorted by and does not move below another section for one column.
typedef std::vector<const char *> HeaderHelps;
static void HeaderRow(const std::vector<int> &columnOfHeader, const std::vector<HeroStatsColumn> &columns,
                      const std::string &lastLabel = "", const HeaderHelps &helps = HeaderHelps(),
                      const std::string &lastRoom = "")
{
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  int count = ImGui::TableGetColumnCount();
  for (int i = 0; i < count; i++)
  {
    // A column scrolled out of sight still has its header's width noted
    bool inSight = ImGui::TableSetColumnIndex(i);
    const char *name = i == count - 1 && !lastLabel.empty() ? lastLabel.c_str() : ImGui::TableGetColumnName(i);
    int champion = i < (int)columnOfHeader.size() ? columnOfHeader[i] : -1;
    bool dimmed = champion >= 0 && columns[champion].hasLeft;
    // Numbers right-aligned, room left for the sort arrow; a champion's dot in two spaces before the name
    std::string prefix;
    float spaceWidth = ImGui::CalcTextSize(" ").x;
    bool isText = measuring != NULL && i < (int)measuring->kinds.size() && measuring->kinds[i] == ColumnText;
    bool guided = i > 0 && InMeasuredTable() && ImGui::TableGetHoveredColumn() == i;
    // Room for the sort arrow beside the name, so a sorted column's header is not cut short
    // (ImGui keeps the arrow's width at the right edge of a sortable header)
    float arrow = ImTrunc(ImGui::GetFontSize() * 0.65f + ImGui::GetStyle().FramePadding.x);
    // A table that gave up the champions' names shows their dots alone
    bool namesOff = champion >= 0 && InMeasuredTable() && !measuring->fit.names;
    float dotHead = spaceWidth * 2 + arrow + 12;
    float nameWidth = ImGui::CalcTextSize(name).x;
    if (i == count - 1 && !lastRoom.empty()) nameWidth = std::max(nameWidth, ImGui::CalcTextSize(lastRoom.c_str()).x);
    if (!isText) NoteHead(nameWidth + (champion >= 0 ? spaceWidth * 2 : 0) + arrow + 12,
                          champion >= 0 ? dotHead : nameWidth + arrow + 12);
    const char *fullName = name;
    if (namesOff) name = "";
    if (!inSight) continue;
    bool isLabel = measuring != NULL && i < (int)measuring->kinds.size() && measuring->kinds[i] == ColumnLabel;
    if (i > 0 && !isText && !isLabel)
    {
      float textWidth = ImGui::CalcTextSize(name).x + (champion >= 0 ? spaceWidth * 2 : 0);
      int spaces = (int)((ImGui::GetContentRegionAvail().x - textWidth - arrow - 4) / spaceWidth);
      if (spaces > 0) prefix = std::string(spaces, ' ');
    }
    std::string label = prefix + (champion >= 0 ? "  " : "") + name;
    ImGui::PushStyleColor(ImGuiCol_Text, Colour(guided ? WHITE_RGB : dimmed ? FAINT_RGB : DIM_RGB));
    ImGui::PushID(i);
    // Where TableHeader draws the text: inside the cell's padding, while its item rect is the whole cell
    ImVec2 textPos = ImGui::GetCursorScreenPos();
    ImGui::TableHeader(label.c_str());
    ImGui::PopID();
    ImGui::PopStyleColor();
    if (i < (int)helps.size() && helps[i] != NULL && helps[i][0] != 0)
    {
      HelpTooltip(helps[i]);
      // Dotted under the name, as any label with a hovertext
      float x = textPos.x + ImGui::CalcTextSize((prefix + (champion >= 0 ? "  " : "")).c_str()).x;
      DottedUnderline(x, x + ImGui::CalcTextSize(name).x, textPos.y + ImGui::GetTextLineHeight());
    }
    if (namesOff && ImGui::IsItemHovered()) WrappedTooltip(fullName);
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

// A row of headings over groups of columns (CASTS over Casts, Successful and Wasted), before
// HeaderRow: each heading centred over its columns, with a line under it as wide as they are
struct ColumnGroup
{
  const char *label;
  int first, last;
};
static void GroupRow(const std::vector<ColumnGroup> &groups)
{
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
  ImGuiTable *table = ImGui::GetCurrentTable();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  // A table scrolled sideways keeps its label column in place: nothing is drawn under it
  float left = table->InnerClipRect.Min.x, right = table->InnerClipRect.Max.x;
  if (table->FreezeColumnsCount > 0) left = std::max(left, table->Columns[0].ClipRect.Max.x);
  float pad = ImGui::GetStyle().CellPadding.x;
  for (size_t g = 0; g < groups.size(); g++)
  {
    int first = groups[g].first, last = groups[g].last;
    while (first <= last && !table->Columns[first].IsEnabled) first++;
    while (last >= first && !table->Columns[last].IsEnabled) last--;
    if (first > last) continue;
    ImRect from = ImGui::TableGetCellBgRect(table, first), to = ImGui::TableGetCellBgRect(table, last);
    ImVec2 min(from.Min.x + pad, from.Min.y), max(to.Max.x - pad, from.Max.y);
    drawList->PushClipRect(ImVec2(std::max(min.x, left), min.y), ImVec2(std::min(max.x, right), max.y), false);
    ImVec2 size = ImGui::CalcTextSize(groups[g].label);
    float y = min.y + ImGui::GetStyle().CellPadding.y;
    drawList->AddText(ImVec2((min.x + max.x - size.x) / 2, y), Packed(DIM_RGB), groups[g].label);
    drawList->AddLine(ImVec2(min.x, max.y - 1), ImVec2(max.x, max.y - 1), Packed(FAINT_RGB));
    drawList->PopClipRect();
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

// A label that has a hovertext says so with a line of dots under it
static void DottedUnderline(float left, float right, float y)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  for (float x = left; x < right; x += 3) drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + 1, y + 1), Packed(FAINT_RGB));
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
  HelpTooltip(help);
  ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
  if (help[0] != 0) DottedUnderline(min.x, max.x, max.y);
  // A part stands behind a line under the number it is a part of
  if (depth > 0)
  {
    // From the row's top to its bottom, so the lines of rows one under the other join; a part
    // of a part stands behind both lines
    ImGuiTable *table = ImGui::GetCurrentTable();
    float top = table != NULL ? table->RowPosY1 : min.y - 1;
    float bottom = max.y + (table != NULL ? ImGui::GetStyle().CellPadding.y : 1);
    for (int d = 1; d <= depth; d++)
    {
      float x = min.x - 9 - 14.0f * (depth - d);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(x, top), ImVec2(x, bottom), Packed(FAINT_RGB));
    }
    ImGui::Unindent(14.0f * depth);
  }
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
  bool bad, gold, hasBar, zero;  // zero: a dim dot, as in the tables
  int percent;
  size_t dotColumn;  // the champion's column for the dot before sub, columns.size(): none
  std::string help;  // the tooltip, empty for none
};

static TileText TileTextOf(const HeroStatsTopicPage &page, HeroStatsNumber number, bool isParty,
                           const std::vector<HeroStatsColumn> &columns)
{
  int value = page.record.value[number];
  const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(number);
  TileText tile = {HeroStatsCatalogue_Format(number, value), info.label, "", info.isBad && value > 0, false, false, false, 0, columns.size(), info.help};
  int cls = number - HeroNumXpFighter;
  if (cls >= 0 && cls < HERO_XP_CLASS_COUNT && cls < (int)page.classes.size())
  {
    const HeroStatsClassLine &line = page.classes[cls];
    std::string rank = HeroXp_RankName(line.rankNow);
    if (isParty)
    {
      tile.label = line.name + " experience";
      tile.help = HERO_STATS_BEST_RANK_HELP;
      if (!rank.empty())
      {
        // Champions who share the best rank are all named; the dot only for one
        std::string names;
        for (size_t k = 0; k < line.tied.size(); k++) names += (k > 0 ? ", " : "") + WhoLabel(columns, line.tied[k]);
        if (line.tied.empty()) names = WhoLabel(columns, line.best);
        tile.sub = "best: " + rank + " (" + names + ")";
        if (line.tied.size() <= 1) tile.dotColumn = ColumnOf(columns, line.best);
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
  // Zero is written as the tables write it; a rank or a record in its place is no zero, nor is a rate's 0%
  if (value == 0 && !HeroStatsCatalogue_IsRecord(number) && HeroStatsCatalogue_Summable(number) && tile.big == HeroStatsCatalogue_Format(number, 0))
  {
    tile.big = MIDDLE_DOT;
    tile.zero = true;
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
    // Beside Monsters killed, the champions' kills are a part of it
    if (state.topic == HeroTopicMonsters && isParty && page.tiles[i] == HeroNumKills)
    {
      tiles.back().label = "By champions";
      tiles.back().help = HERO_STATS_BY_CHAMPIONS_HELP;
    }
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
    // No box: a strip of numbers, each behind a thin line
    drawList->AddLine(ImVec2(min.x, min.y + 4), ImVec2(min.x, min.y + height - 4), Packed(LINE_RGB));
    drawList->PushClipRect(min, ImVec2(min.x + tileWidth - 6, min.y + height), true);
    // Bad news on its pill, as everywhere else: red alone reads as the red champion's colour
    ImU32 bigRgb = tile.bad ? BAD_RGB : tile.gold ? GOLD_RGB : tile.zero ? FAINT_RGB : WHITE_RGB;
    if (tile.bad) PillBehind(drawList, ImVec2(min.x + 14, min.y + 7), ImVec2(TextWidthWithSymbols(bigSize, tile.big), bigSize));
    TextWithSymbols(drawList, bigSize, ImVec2(min.x + 14, min.y + 7), Packed(bigRgb), tile.big);
    drawList->AddText(ImVec2(min.x + 14, min.y + 9 + bigSize), Packed(DIM_RGB), tile.label.c_str());
    if (!tile.help.empty())
      DottedUnderline(min.x + 14, min.x + 14 + ImGui::CalcTextSize(tile.label.c_str()).x, min.y + 9 + bigSize + lineHeight);
    float y = min.y + 13 + bigSize + lineHeight;
    if (tile.hasBar)
    {
      ImU32 rgb = ChampionRgb(ColumnOf(columns, state.who));
      ImVec2 barMin(min.x + 14, y), barMax(min.x + tileWidth - 10, y + 5);
      drawList->AddRectFilled(barMin, barMax, Packed(TRACK_RGB), 2.0f);
      drawList->AddRectFilled(barMin, ImVec2(barMin.x + (barMax.x - barMin.x) * tile.percent / 100.0f, barMax.y), Packed(rgb), 2.0f);
      y += 7;
    }
    if (!tile.sub.empty())
    {
      float x = min.x + 14;
      if (tile.dotColumn < columns.size())
      {
        ChampionDot(ImVec2(x + 4, y + lineHeight / 2), tile.dotColumn, columns[tile.dotColumn].hasLeft);
        x += 13;
      }
      TextWithSymbols(drawList, ImGui::GetFontSize(), ImVec2(x, y), Packed(tile.hasBar ? FAINT_RGB : TEXT_RGB), tile.sub);
    }
    drawList->PopClipRect();
    // What the number counts, and what it does or does not contain
    if (!tile.help.empty() && ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(min, ImVec2(min.x + tileWidth, min.y + height)))
      WrappedTooltip(tile.help.c_str());
  }
  int rows = (count + perRow - 1) / perRow;
  ImGui::Dummy(ImVec2(width, rows * (height + gap) - gap + 4));
}

// The groups of the page's numbers that are table rows, not panels
static int TableGroupCount(const HeroStatsTopicPage &page)
{
  int count = 0;
  for (size_t g = 0; g < page.groups.size(); g++)
    if (!page.groups[g].panel) count++;
  return count;
}

// The small heading over a group of rows of the Numbers table, with room above it
static void GroupHeadingRow(const std::string &title, bool isFirst)
{
  ImGui::TableNextRow();
  rowIsHot = false;
  ImGui::TableSetColumnIndex(0);
  if (!isFirst) ImGui::Dummy(ImVec2(0, 7));
  std::string upper = title;
  for (size_t i = 0; i < upper.size(); i++) upper[i] = (char)toupper((unsigned char)upper[i]);
  NoteWidth(ImGui::CalcTextSize(upper.c_str()).x);
  ImGui::TextColored(Colour(DIM_RGB), "%s", upper.c_str());
}

// The numbers only the party has (steps, teleports, chests ...), a panel per group: its title,
// then label and value on a line. As many panels in a row as fit, all of a row equally high.
static void DrawPanels(HeroStatsDrawState *state, const HeroStatsTopicPage &page)
{
  std::vector<int> panels;
  for (size_t g = 0; g < page.groups.size(); g++)
    if (page.groups[g].panel) panels.push_back((int)g);
  if (panels.empty()) return;
  if (!SectionHeader(state, HERO_STATS_PANELS_SECTION, "The party", "")) return;
  ImGui::Dummy(ImVec2(0, 4));
  const float gap = 10, inset = 12;
  float width = ImGui::GetContentRegionAvail().x;
  int perRow = HeroStatsLayout_Across((int)panels.size(), width, HERO_LAYOUT_PANEL_SMALLEST, gap);
  float panelWidth = (width - gap * (perRow - 1)) / perRow;
  float line = ImGui::GetTextLineHeight() + 5;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float y = origin.y;
  for (size_t first = 0; first < panels.size(); first += perRow)
  {
    size_t end = std::min(panels.size(), first + perRow);
    int mostRows = 0;
    for (size_t p = first; p < end; p++)
    {
      int rows = 0;
      for (size_t r = 0; r < page.numbers.size(); r++)
        if (page.numbers[r].group == panels[p]) rows++;
      mostRows = std::max(mostRows, rows);
    }
    float height = 9 + line * (1 + mostRows) + 7;
    for (size_t p = first; p < end; p++)
    {
      ImVec2 min(origin.x + (p - first) * (panelWidth + gap), y);
      drawList->AddRectFilled(min, ImVec2(min.x + panelWidth, min.y + height), Packed(PANEL_RGB), 6.0f);
      std::string upper = page.groups[panels[p]].title;
      for (size_t i = 0; i < upper.size(); i++) upper[i] = (char)toupper((unsigned char)upper[i]);
      drawList->AddText(ImVec2(min.x + inset, min.y + 9), Packed(DIM_RGB), upper.c_str());
      float rowY = min.y + 9 + line;
      for (size_t r = 0; r < page.numbers.size(); r++)
      {
        const HeroStatsNumbersRow &row = page.numbers[r];
        if (row.group != panels[p]) continue;
        const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(row.number);
        float x = min.x + inset + 14.0f * row.depth;
        float labelWidth = ImGui::CalcTextSize(row.label.c_str()).x;
        if (row.depth > 0) drawList->AddLine(ImVec2(x - 9, rowY - 1), ImVec2(x - 9, rowY + ImGui::GetTextLineHeight() + 1), Packed(FAINT_RGB));
        drawList->AddText(ImVec2(x, rowY), Packed(row.depth > 0 ? PART_RGB : TEXT_RGB), row.label.c_str());
        bool dot = row.party == 0 && !info.isRatio;  // a rate's 0% is written
        std::string value = dot ? MIDDLE_DOT : HeroStatsCatalogue_Format(row.number, row.party);
        if (!row.runShare.empty()) value += " (" + row.runShare + ")";  // the time's share of the run
        float valueWidth = ImGui::CalcTextSize(value.c_str()).x;
        drawList->AddText(ImVec2(min.x + panelWidth - inset - valueWidth, rowY), Packed(dot ? FAINT_RGB : TEXT_RGB), value.c_str());
        if (info.help[0] != 0)
        {
          DottedUnderline(x, x + labelWidth, rowY + ImGui::GetTextLineHeight());
          if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(ImVec2(x, rowY), ImVec2(x + labelWidth, rowY + line)))
            WrappedTooltip(info.help);
        }
        rowY += line;
      }
    }
    y += height + gap;
  }
  ImGui::Dummy(ImVec2(width, y - origin.y - gap));
  SectionGap();
}

// The topic's numbers: the Party's with a column per champion and a share bar, a champion's
// beside the party's. With one champion, who is all of the party, the champion's column alone:
// on the Party's page it holds the party's own numbers too.
static void DrawNumbers(const HeroStatsTopicPage &page, const HeroStatsDrawState &state,
                        const std::vector<HeroStatsColumn> &columns)
{
  bool isParty = state.who == HERO_STATS_PARTY;
  size_t champion = 0;
  while (!isParty && champion < columns.size() && columns[champion].id != state.who) champion++;
  bool shared = columns.size() != 1;
  int count = !shared ? 2 : isParty ? (int)columns.size() + 3 : 4;
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
  if (shared)
  {
    NumberColumn("Total");
    BarColumn(isParty ? "Share" : "Share of party");
  }
  HeaderHelps helps(headerColumns.size(), "");
  // Only a table that holds a record says what Total means for one
  bool holdsRecord = false;
  for (size_t r = 0; r < page.numbers.size(); r++)
  {
    const HeroStatsNumbersRow &row = page.numbers[r];
    bool panel = row.group >= 0 && row.group < (int)page.groups.size() && page.groups[row.group].panel;
    if (!panel && HeroStatsCatalogue_IsRecord(row.number)) holdsRecord = true;
  }
  if (shared) helps.push_back(holdsRecord ? "All the champions together; for a record, the best of them." : "All the champions together.");
  HeaderRow(headerColumns, columns, "", helps);
  // Each group with a title has a small heading, a lone one too: the section is always Numbers
  int groupShown = -1;
  for (size_t r = 0; r < page.numbers.size(); r++)
  {
    const HeroStatsNumbersRow &row = page.numbers[r];
    if (row.group >= 0 && row.group < (int)page.groups.size() && page.groups[row.group].panel) continue;  // DrawPanels' rows
    if (row.group != groupShown && row.group >= 0 && row.group < (int)page.groups.size() && !page.groups[row.group].title.empty())
      GroupHeadingRow(page.groups[row.group].title, groupShown < 0);
    groupShown = row.group;
    const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(row.number);
    bool off = IgnoredNumber(row.number);
    ImU32 rgb = off ? FAINT_RGB : row.depth > 0 ? PART_RGB : TEXT_RGB;
    NextGuidedRow();
    // Beside Monsters killed, the champions' kills are a part of it
    bool byChampions = row.number == HeroNumKills && row.label == "By champions";
    LabelCell(row.label, row.depth, rgb, byChampions ? HERO_STATS_BY_CHAMPIONS_HELP : info.help);
    if (!shared)
    {
      ImGui::TableSetColumnIndex(1);
      CatalogueCell(row.number, row.byColumn.empty() ? row.party : row.byColumn[0], isParty && columns[0].hasLeft ? DIM_RGB : rgb);
      continue;
    }
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
  EndPageTable();
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

// The Who bar: each champion's part of the line in the column it splits; hovered, the numbers.
// notes: a champion's own value of what the table is sorted by when the bar cannot split it (a
// rate: "HALK  30 · 88%"), one per column, or none
static void WhoCell(const std::vector<int> &byColumn, const std::vector<HeroStatsColumn> &columns,
                    const std::vector<std::string> &notes = std::vector<std::string>())
{
  ShareBar(byColumn);
  if (!ImGui::IsItemHovered()) return;
  std::string text;
  for (size_t c = 0; c < byColumn.size() && c < columns.size(); c++)
    if (byColumn[c] != 0)
      text += (text.empty() ? "" : "\n") + columns[c].name + "  " + HeroStatsView_Number(byColumn[c]) +
              (c < notes.size() && !notes[c].empty() ? " " + std::string(MIDDLE_DOT) + " " + notes[c] : "");
  if (!text.empty()) WrappedTooltip(text.c_str());
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

// The widest Who header of those the sorts can give: HeaderRow's lastRoom
static std::string WidestMeasureLabel(const char *const *columnLabels, const std::vector<int> &splittable)
{
  std::string widest;
  for (size_t i = 0; i < splittable.size(); i++)
  {
    std::string label = MeasureLabel(columnLabels, splittable[i]);
    if (ImGui::CalcTextSize(label.c_str()).x > ImGui::CalcTextSize(widest.c_str()).x) widest = label;
  }
  return widest;
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
  // The Who column only with champions to tell apart: one champion is all of the party
  bool withWho = state->who == HERO_STATS_PARTY && columns.size() != 1;
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
  int count = withWho ? 8 : 7;
  if (BeginPageTable("##weapons", count, true))
  {
    LabelColumn("Weapon", true);
    NumberColumn("Tries");
    NumberColumn("Hits");
    NumberColumn("Rate", false, true);
    NumberColumn("Avg", false, true);
    NumberColumn("Damage", true);
    NumberColumn("Kills");
    if (withWho) BarColumn("Who");
    TableSort sort = CurrentSort();
    static const char *const LABELS[7] = {"Weapon", "Tries", "Hits", "Rate", "Avg", "Damage", "Kills"};
    static const std::vector<int> SPLITTABLE = {1, 2, 5, 6};
    // Sorted by Rate, which cannot be shared out, the bar splits the Tries it divides, each
    // champion's rate in its hovertext
    int measure = sort.column == 3 ? 1 : WhoMeasure(sort, SPLITTABLE, 5);
    static const HeaderHelps HELPS = {
        "", "Blows and missiles, and the actions that cannot hit (War Cry, Calm...). The Combat page's Attacks leaves those out; its Other actions counts them.",
        "", "Hits out of the tries that can hit; actions that cannot hit are left out. A rate cannot be shared out: sorted by it, the Who bar splits Tries, and its hovertext has each champion's rate.", HERO_STATS_AVG_HELP,
        HERO_STATS_WEAPON_DAMAGE_HELP};
    HeaderRow(std::vector<int>(), columns, withWho ? MeasureLabel(LABELS, measure) : "", HELPS,
              withWho ? WidestMeasureLabel(LABELS, SPLITTABLE) : "");
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
      if (withWho)
      {
        ImGui::TableSetColumnIndex(7);
        std::vector<int> split;
        std::vector<std::string> notes;
        for (size_t c = 0; c < weapon.byColumn.size(); c++)
        {
          split.push_back(AttackValue(weapon.byColumn[c], measure));
          if (sort.column == 3) notes.push_back(HeroStatsView_Percent(weapon.byColumn[c].hits, weapon.byColumn[c].tries));
        }
        WhoCell(split, columns, notes);
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
    EndPageTable();
  }
  SectionGap();
}

template <typename Line>
static int SpellValue(const Line &line, int column)
{
  switch (column)
  {
  case 1: return line.casts;
  case 2: return line.successful;
  case 4: return line.failed;
  case 5: return line.mana;
  case 6: return line.mana - line.wasted;
  case 8: return line.wasted;
  case 9: return line.damage;
  case 10: return line.kills;
  default: return line.mana;
  }
}

// The Spells table's Rate columns: 3 the casts', 7 the mana's; -1 with nothing to divide (and
// for the mana of Runes cleared, which is no cast)
static double SpellRate(const HeroStatsSpellLine &line, int column)
{
  if (column == 3) return Ratio(line.successful, line.casts);
  return line.noManaRate ? -1 : Ratio(line.mana - line.wasted, line.mana);
}

static void DrawSpells(HeroStatsDrawState *state, const HeroStatsHeroPage &page, const std::vector<HeroStatsColumn> &columns)
{
  // The Who column only with champions to tell apart: one champion is all of the party
  bool withWho = state->who == HERO_STATS_PARTY && columns.size() != 1;
  HeroStatsSpellLine all = {"All spells", 0, 0, 0, 0, 0, 0, 0, std::vector<HeroStatsSpellCounts>(), false};
  for (size_t i = 0; i < page.spells.size(); i++)
  {
    all.casts += page.spells[i].casts;
    all.successful += page.spells[i].successful;
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
  int count = withWho ? 12 : 11;
  if (BeginPageTable("##spells", count, true))
  {
    // CASTS and MANA: All, Successful, Rate (worked out, left out first when narrow), Wasted
    LabelColumn("Spell", true);
    NumberColumn("All");
    NumberColumn("Successful");
    NumberColumn("Rate", false, true);
    NumberColumn("Wasted");
    NumberColumn("All", true);
    NumberColumn("Successful");
    NumberColumn("Rate", false, true);
    NumberColumn("Wasted");
    NumberColumn("Damage");
    NumberColumn("Kills");
    if (withWho) BarColumn("Who");
    TableSort sort = CurrentSort();
    // Under the group headings each column says only its part ("All"); the Who bar's heading
    // names the group too ("Who: Wasted mana"). A rate cannot be shared out: sorted by one, the
    // bar splits its group's All.
    static const char *const LABELS[11] = {"Spell", "Casts", "Successful casts", "Cast rate", "Wasted casts", "Mana",
                                           "Successful mana", "Mana rate", "Wasted mana", "Damage", "Kills"};
    static const std::vector<int> SPLITTABLE = {1, 2, 4, 5, 6, 8, 9, 10};
    int measure = sort.column == 3 ? 1 : sort.column == 7 ? 5 : WhoMeasure(sort, SPLITTABLE, 5);
    static const HeaderHelps HELPS(HERO_STATS_SPELL_HELPS, HERO_STATS_SPELL_HELPS + HERO_STATS_SPELL_COLUMNS);
    GroupRow({{"CASTS", 1, 4}, {"MANA", 5, 8}});
    HeaderRow(std::vector<int>(), columns, withWho ? MeasureLabel(LABELS, measure) : "", HELPS,
              withWho ? WidestMeasureLabel(LABELS, SPLITTABLE) : "");
    std::vector<HeroStatsSpellLine> spells = page.spells;
    SortRows(&spells, sort, [](const HeroStatsSpellLine &a, const HeroStatsSpellLine &b, int column) {
      if (column == 0) return a.name.compare(b.name);
      if (column == 3 || column == 7) return Compare(SpellRate(a, column), SpellRate(b, column));
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
      for (int column = 1; column <= 10; column++)
      {
        ImGui::TableSetColumnIndex(column);
        if (column == 3 || column == 7)
        {
          double rate = SpellRate(line, column);
          RightText(rate < 0 ? "-" : HeroStatsView_Percent(column == 3 ? line.successful : line.mana - line.wasted,
                                                           column == 3 ? line.casts : line.mana),
                    rate < 0 ? FAINT_RGB : rgb);
        }
        else PlainCell(SpellValue(line, column), rgb, column == 4 || column == 8);
      }
      if (withWho && !isAll)
      {
        ImGui::TableSetColumnIndex(11);
        std::vector<int> split;
        std::vector<std::string> notes;
        for (size_t c = 0; c < line.byColumn.size(); c++)
        {
          const HeroStatsSpellCounts &mine = line.byColumn[c];
          split.push_back(SpellValue(mine, measure));
          if (sort.column == 3) notes.push_back(HeroStatsView_Percent(mine.successful, mine.casts));
          if (sort.column == 7) notes.push_back(line.noManaRate ? "-" : HeroStatsView_Percent(mine.mana - mine.wasted, mine.mana));
        }
        WhoCell(split, columns, notes);
      }
    }
    EndPageTable();
  }
  SectionGap();
}

// A table of amount lines (damage taken by source, healing by source): text, amount, Who
static void DrawAmounts(HeroStatsDrawState *state, int section, const char *title, const char *column,
                        const char *allLabel, const char *what, const std::vector<HeroStatsAmountLine> &source,
                        const std::vector<HeroStatsColumn> &columns)
{
  // The Who column only with champions to tell apart: one champion is all of the party
  bool withWho = state->who == HERO_STATS_PARTY && columns.size() != 1;
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
  if (BeginPageTable("##amounts", withWho ? 3 : 2, true))
  {
    LabelColumn("Source", true);
    NumberColumn(column, true);
    if (withWho) BarColumn("Who");
    HeaderRow(std::vector<int>(), columns, withWho ? std::string("Who: ") + column : "");
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
      if (withWho)
      {
        ImGui::TableSetColumnIndex(2);
        WhoCell(lines[i].byColumn, columns);
      }
    }
    TotalRow();
    LabelCell(allLabel, 0, WHITE_RGB);
    ImGui::TableSetColumnIndex(1);
    PlainCell(total, WHITE_RGB);
    EndPageTable();
  }
  ImGui::PopID();
  SectionGap();
}

// A table as Ranks gained: who (the Party's page), what killed them, the level and the tick,
// newest first (a game that kept no ticks: in the order they happened). In a replay the tick
// goes there.
static void DrawDeaths(HeroStatsDrawState *state, const HeroStatsHeroPage &page, const std::vector<HeroStatsColumn> &columns)
{
  int deaths = (int)page.deaths.size();
  std::string summary = deaths == 0 ? "never died" : Count(deaths, deaths == 1 ? "death" : "deaths");
  if (!SectionHeader(state, HeroDetailDeaths, "Deaths", summary)) return;
  if (deaths == 0)
  {
    ImGui::TextColored(Colour(DIM_RGB), "Nobody died.");
    SectionGap();
    return;
  }
  bool isParty = state->who == HERO_STATS_PARTY;
  if (BeginPageTable("##deaths", isParty ? 4 : 3, true))
  {
    if (isParty) LabelColumn("Who", true);
    if (isParty) TextColumn("Killed by", true);
    else LabelColumn("Killed by", true);
    NumberColumn("Lv");
    NumberColumn("Tick", true);
    static const char *const TICK_HELP = "When the champion died; empty in a game saved before the statistics kept it.";
    static const HeaderHelps PARTY_HELPS = {"", "", "", TICK_HELP};
    static const HeaderHelps CHAMPION_HELPS = {"", "", TICK_HELP};
    HeaderRow(std::vector<int>(), columns, "", isParty ? PARTY_HELPS : CHAMPION_HELPS);
    std::vector<HeroStatsDeathLine> lines = page.deaths;
    int shift = isParty ? 0 : 1;  // a column of the champion's page is one to the left of the party's
    SortRows(&lines, CurrentSort(), [&](const HeroStatsDeathLine &a, const HeroStatsDeathLine &b, int c) {
        switch (c + shift)
        {
        case 0: return WhoLabel(columns, a.who).compare(WhoLabel(columns, b.who));
        case 1: return a.killedBy.compare(b.killedBy);
        case 2: return Compare(a.level, b.level);
        default: return Compare(a.tick, b.tick);
        }
      });
    for (size_t i = 0; i < lines.size(); i++)
    {
      const HeroStatsDeathLine &line = lines[i];
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
      TextColoredWithSymbols(TEXT_RGB, line.killedBy);
      ImGui::TableSetColumnIndex(column++);
      RightText(std::to_string(line.level), TEXT_RGB);
      ImGui::TableSetColumnIndex(column++);
      if (line.tick < 0) RightText("-", FAINT_RGB);
      else
      {
        RightText(HeroStatsView_Number(line.tick), TEXT_RGB);
        if (tickSink && ImGui::IsItemHovered())
        {
          ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
          TooltipWithSymbols("Click: go to tick " + HeroStatsView_Number(line.tick) + " in the replay and pause there");
          if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) tickSink(line.tick);
        }
      }
    }
    EndPageTable();
  }
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
  // The Who column only with champions to tell apart: one champion is all of the party
  bool withWho = state->who == HERO_STATS_PARTY && columns.size() != 1;
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
  if (BeginPageTable("##consumed", withWho ? 4 : 3, true))
  {
    LabelColumn("Item", true);
    NumberColumn("Eaten", true);
    NumberColumn("Drunk");
    if (withWho) BarColumn("Who");
    TableSort sort = CurrentSort();
    static const char *const LABELS[3] = {"Item", "Eaten", "Drunk"};
    static const std::vector<int> SPLITTABLE = {1, 2};
    int measure = WhoMeasure(sort, SPLITTABLE, 1);
    static const HeaderHelps HELPS = {"", "", "Times the item was drunk; the fountain counts as a drink."};
    HeaderRow(std::vector<int>(), columns, withWho ? MeasureLabel(LABELS, measure) : "", HELPS,
              withWho ? WidestMeasureLabel(LABELS, SPLITTABLE) : "");
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
      if (withWho && !isAll)
      {
        ImGui::TableSetColumnIndex(3);
        // A row with nothing in the measured column splits its other one: a drink sorted by Eaten
        bool drunk = measure == 1 ? lines[i].eaten == 0 : lines[i].drunk > 0 || lines[i].eaten == 0;
        WhoCell(drunk ? lines[i].drunkBy : lines[i].eatenBy, columns);
      }
    }
    EndPageTable();
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
    static const HeaderHelps HELPS = {"", "", "Included in Picked up: the ones taken out of a chest."};
    HeaderRow(std::vector<int>(), columns, "", HELPS);
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
    EndPageTable();
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
  // The Party: a column per champion, the doors, pits, telefrags, landings and other causes when there are any, all of them, Who
  bool door = isParty && all.door > 0, pit = isParty && all.pit > 0, other = isParty && all.other > 0;
  bool telefrag = isParty && all.telefrag > 0, landed = isParty && all.landed > 0;
  bool withWho = isParty && champions != 1;  // one champion is all of the party
  int count = 1 + (isParty ? (int)champions + door + pit + telefrag + landed + other + 1 + withWho : 1);
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
      // Named as the rows of the Numbers table above
      if (door) NumberColumn(HeroStatsCatalogue_Info(HeroNumDoorKills).label);
      if (pit) NumberColumn(HeroStatsCatalogue_Info(HeroNumPitKills).label);
      if (telefrag) NumberColumn(HeroStatsCatalogue_Info(HeroNumTelefragged).label);
      if (landed) NumberColumn(HeroStatsCatalogue_Info(HeroNumLandedOn).label);
      if (other) NumberColumn(HeroStatsCatalogue_Info(HeroNumOtherKills).label);
      NumberColumn("All", true);
      if (withWho) BarColumn("Who");
    }
    else NumberColumn("Kills", true);
    HeaderHelps helps(1, "");
    if (isParty)
    {
      for (size_t c = 0; c < champions; c++) helps.push_back("");
      if (door) helps.push_back(HeroStatsCatalogue_Info(HeroNumDoorKills).help);
      if (pit) helps.push_back(HeroStatsCatalogue_Info(HeroNumPitKills).help);
      if (telefrag) helps.push_back(HeroStatsCatalogue_Info(HeroNumTelefragged).help);
      if (landed) helps.push_back(HeroStatsCatalogue_Info(HeroNumLandedOn).help);
      if (other) helps.push_back(HeroStatsCatalogue_Info(HeroNumOtherKills).help);
      helps.push_back("Every kill, whatever the cause.");
    }
    HeaderRow(headerColumns, columns, withWho ? "Who: Kills" : "", helps);
    // The values of a line in table order after the name
    auto values = [&](const HeroStatsMonsterLine &line) {
      std::vector<int> v;
      if (!isParty) return std::vector<int>(1, line.total);
      for (size_t c = 0; c < champions; c++) v.push_back(line.byColumn[c]);
      if (door) v.push_back(line.door);
      if (pit) v.push_back(line.pit);
      if (telefrag) v.push_back(line.telefrag);
      if (landed) v.push_back(line.landed);
      if (other) v.push_back(line.other);
      v.push_back(line.total);
      if (withWho) v.push_back(line.total);  // Who sorts by all of them
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
      int numbers = withWho ? (int)v.size() - 1 : (int)v.size();
      for (int c = 0; c < numbers; c++)
      {
        ImGui::TableSetColumnIndex(c + 1);
        bool dimmed = isParty && c < (int)champions && columns[c].hasLeft;
        PlainCell(v[c], dimmed ? DIM_RGB : rgb);
      }
      if (withWho && !isAll)
      {
        ImGui::TableSetColumnIndex(numbers + 1);
        std::vector<int> split(line.byColumn.begin(), line.byColumn.begin() + champions);
        WhoCell(split, columns);
      }
    }
    EndPageTable();
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
  bool withWho = isParty && columns.size() != 1;  // one champion is all of the party
  int count = isParty && !withWho ? 3 : 4;
  if (BeginPageTable("##skills", count, false))
  {
    LabelColumn("Skill");
    if (isParty)
    {
      NumberColumn("Experience");
      NumberColumn("Ranks gained");
      if (withWho) BarColumn("Who: Experience");
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
        if (withWho)
        {
          ImGui::TableSetColumnIndex(3);
          WhoCell(line.byColumn, columns);
        }
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
          if (withWho)
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
    EndPageTable();
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
  // One champion is all of the party: no Who, no Total and no share of it
  bool shared = columns.size() != 1;
  int count = !shared ? 2 : isParty ? 3 : 4;
  size_t champion = ColumnOf(columns, state->who);
  if (BeginPageTable("##xpsources", count, false))
  {
    LabelColumn("Source");
    if (isParty) NumberColumn("Experience");
    else
    {
      NumberColumn(champion < columns.size() ? columns[champion].name.c_str() : "");
      if (shared) NumberColumn("Total");
    }
    if (shared) BarColumn(isParty ? "Who: Experience" : "Share of party");
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
      HelpTooltip(line.help.c_str());
      if (!line.note.empty())
      {
        ImGui::SameLine();
        ImGui::TextColored(Colour(DIM_RGB), "%s", line.note.c_str());
      }
      if (line.depth > 0) ImGui::Unindent(14.0f * line.depth);
      ImGui::TableSetColumnIndex(1);
      PlainCell(line.amount, rgb);
      if (!shared) continue;
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
    EndPageTable();
  }
  SectionGap();
}

static void DrawRankUps(HeroStatsDrawState *state, std::vector<HeroStatsRankUpLine> lines,
                        const std::vector<HeroStatsColumn> &columns)
{
  int ups = (int)lines.size();
  if (!SectionHeader(state, HeroDetailRankUps, "Ranks gained", ups == 0 ? "none yet" : Count(ups, ups == 1 ? "rank" : "ranks")))
    return;
  if (ups == 0)
  {
    ImGui::TextColored(Colour(DIM_RGB), "No new rank yet.");
    SectionGap();
    return;
  }
  bool isParty = state->who == HERO_STATS_PARTY;
  int count = isParty ? 6 : 5;
  if (BeginPageTable("##rankups", count, true))
  {
    if (isParty) LabelColumn("Who", true);
    if (isParty) TextColumn("Class", true);
    else LabelColumn("Class", true);
    TextColumn("Rank", true);
    TextColumn("With", true);
    NumberColumn("Lv");
    NumberColumn("Tick", true);
    static const HeaderHelps PARTY_HELPS = {"", "", "Sorts by the rank reached.", "What gave the experience that made the rank: an action, a spell, or a monster attacking the champion (Parry).",
                                            "", "When the rank came: newest first."};
    static const HeaderHelps CHAMPION_HELPS = {"", "Sorts by the rank reached.", "What gave the experience that made the rank: an action, a spell, or a monster attacking the champion (Parry).",
                                               "", "When the rank came: newest first."};
    HeaderRow(std::vector<int>(), columns, "", isParty ? PARTY_HELPS : CHAMPION_HELPS);
    // The rows come newest first; a column of the champion's page is one to the left of the party's
    int shift = isParty ? 0 : 1;
    SortRows(&lines, CurrentSort(), [&](const HeroStatsRankUpLine &a, const HeroStatsRankUpLine &b, int c) {
      switch (c + shift)
      {
      case 0: return WhoLabel(columns, a.who).compare(WhoLabel(columns, b.who));
      case 1: return a.className.compare(b.className);
      case 2: return Compare(a.rankNumber, b.rankNumber);
      case 3: return a.with.compare(b.with);
      case 4: return Compare(a.level, b.level);
      default: return Compare(a.tick, b.tick);
      }
    });
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
      ImGui::TextColored(Colour(PART_RGB), "%s", line.with.c_str());
      ImGui::TableSetColumnIndex(column++);
      RightText(LevelLabel(line.level).substr(3), TEXT_RGB);
      ImGui::TableSetColumnIndex(column++);
      RightText(HeroStatsView_Number(line.tick), TEXT_RGB);
    }
    EndPageTable();
  }
  SectionGap();
}

// Records: one table in its four groups. A champion's page: their best, gold with a star when it
// is the party's, and the party's best beside it.
// The records sorted by a column within their groups, which keep their order; by Record they are
// in the order they are listed in. Values compare as numbers, a missing one below the rest.
static std::vector<HeroStatsRecordLine> SortedRecords(const std::vector<HeroStatsRecordLine> &lines, const TableSort &sort,
                                                      bool isParty)
{
  std::vector<size_t> order(lines.size());
  std::map<std::string, size_t> groupAt;
  for (size_t i = 0; i < lines.size(); i++)
  {
    order[i] = i;
    groupAt.insert(std::make_pair(lines[i].group, i));
  }
  // The party's page has Who after Value; a champion's has Party best at the end
  int column = sort.column < 0 ? 0 : !isParty && sort.column >= 2 ? sort.column + 1 : sort.column;
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    const HeroStatsRecordLine &x = lines[a], &y = lines[b];
    size_t gx = groupAt[x.group], gy = groupAt[y.group];
    if (gx != gy) return gx < gy;
    int c;
    switch (column)
    {
    case 1: c = Compare(x.amount, y.amount); break;
    case 2: c = x.who.compare(y.who); break;
    case 3: c = x.how.compare(y.how); break;
    case 4: c = x.whom.compare(y.whom); break;
    case 5: c = Compare(x.has ? x.level : -1, y.has ? y.level : -1); break;
    case 6: c = Compare(x.partyAmount, y.partyAmount); break;
    default: c = Compare((double)a, (double)b); break;
    }
    return sort.descending ? c > 0 : c < 0;
  });
  std::vector<HeroStatsRecordLine> sorted;
  for (size_t i = 0; i < order.size(); i++) sorted.push_back(lines[order[i]]);
  return sorted;
}

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
  if (BeginPageTable("##records", count, true))
  {
    LabelColumn("Record", true, true);
    TextColumn(isParty ? "Value" : (champion < columns.size() ? columns[champion].name.c_str() : "Value"), true);
    if (isParty) TextColumn("Who", true);
    TextColumn("How", true);
    TextColumn("On / from", true);
    NumberColumn("Lv");
    if (!isParty) TextColumn("Party best", true);
    static const HeaderHelps HELPS = {HERO_STATS_RECORDS_SORT_HELP};
    HeaderRow(std::vector<int>(), columns, "", HELPS);
    std::vector<HeroStatsRecordLine> sorted = SortedRecords(lines, CurrentSort(), isParty);
    std::string group;
    for (size_t i = 0; i < sorted.size(); i++)
    {
      const HeroStatsRecordLine &line = sorted[i];
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
        bool clickable = tickSink && line.tick >= 0;
        if (ImGui::IsItemHovered())
        {
          if (clickable) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
          if (!line.when.empty()) TooltipWithSymbols(clickable ? line.when + "\nClick: go to tick " + HeroStatsView_Number(line.tick) + " in the replay and pause there" : line.when);
          if (clickable && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) tickSink(line.tick);
        }
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
      TextColoredWithSymbols(PART_RGB, line.how);
      ImGui::TableSetColumnIndex(column++);
      ImGui::TextColored(Colour(PART_RGB), "%s", line.whom.c_str());
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
            if (ImGui::IsItemHovered()) WrappedTooltip(columns[c].name.c_str());
          }
        }
      }
    }
    EndPageTable();
  }
  SectionGap();
}


// ---------------------------------------------------------------------------------------------
// Overview

// The Party's Overview: a card per champion with small bars against the best of them. True with
// the champion whose card was clicked.

static bool DrawCards(const std::vector<HeroStatsCard> &cards, HeroStatsChampion *clicked)
{
  bool anyClicked = false;
  if (cards.empty()) return false;
  static const HeroStatsNumber ROWS[6] = {HeroNumDamage, HeroNumKills, HeroNumTaken, HeroNumExperience, HeroNumManaSpent, HeroNumDeaths};
  // Exp: "Experience" would take the room of the bars
  static const char *const LABELS[6] = {"Damage", "Kills", "Taken", "Exp", "Mana", "Deaths"};
  int largest[6] = {0};
  for (size_t c = 0; c < cards.size(); c++)
    for (int r = 0; r < 6; r++) largest[r] = std::max(largest[r], cards[c].record.value[ROWS[r]]);
  // The label and number columns are as wide as the widest label and number of any card
  float labelWidth = 0, numberWidth = ImGui::CalcTextSize("00,000").x;
  for (int r = 0; r < 6; r++)
  {
    labelWidth = std::max(labelWidth, ImGui::CalcTextSize(LABELS[r]).x);
    for (size_t c = 0; c < cards.size(); c++)
      numberWidth = std::max(numberWidth, ImGui::CalcTextSize(HeroStatsView_Number(cards[c].record.value[ROWS[r]]).c_str()).x);
  }
  float gap = 8;
  float width = ImGui::GetContentRegionAvail().x;
  int perRow = HeroStatsLayout_Across((int)cards.size(), width, std::max(HERO_LAYOUT_OVERVIEW_CARD_SMALLEST, 24 + labelWidth + 10 + 60 + 8 + numberWidth), gap);
  // A lone champion's card does not stretch across the window: its bars would be very long
  float cardWidth = std::min(HERO_LAYOUT_CARD_WIDEST, (width - gap * (perRow - 1)) / perRow);
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
    drawList->AddRectFilled(min, max, Packed(hovered ? PANEL_HOVERED_RGB : PANEL_RGB), 6.0f);
    drawList->AddRectFilled(ImVec2(min.x + 3, min.y), ImVec2(max.x - 3, min.y + 2), Packed(ChampionRgb(c), card.column.hasLeft ? 0.45f : 1.0f));
    drawList->AddText(ImVec2(min.x + 10, min.y + 9), Packed(card.column.hasLeft ? DIM_RGB : WHITE_RGB), card.column.name.c_str());
    // One champion did all of the party's damage: no share to tell
    std::string share = cards.size() == 1 ? "" : std::to_string(card.damagePercent) + "% of damage";
    float shareWidth = ImGui::CalcTextSize(share.c_str()).x;
    drawList->AddText(ImVec2(max.x - 10 - shareWidth, min.y + 9), Packed(DIM_RGB), share.c_str());
    for (int r = 0; r < 6; r++)
    {
      float y = min.y + 9 + line * (r + 1);
      int value = card.record.value[ROWS[r]];
      bool bad = HeroStatsCatalogue_Info(ROWS[r]).isBad && value > 0;
      drawList->AddText(ImVec2(min.x + 10, y), Packed(DIM_RGB), LABELS[r]);
      // The short label's full name, as the tables have it
      ImVec2 mouse = ImGui::GetIO().MousePos;
      if (hovered && std::strcmp(LABELS[r], HeroStatsCatalogue_Info(ROWS[r]).label) != 0 && mouse.y >= y && mouse.y < y + line && mouse.x >= min.x + 10 && mouse.x < min.x + 10 + labelWidth)
        ImGui::SetTooltip("%s", HeroStatsCatalogue_Info(ROWS[r]).label);
      std::string text = value == 0 ? MIDDLE_DOT : HeroStatsView_Number(value);
      ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
      if (bad) PillBehind(drawList, ImVec2(max.x - 10 - textSize.x, y), textSize);
      drawList->AddText(ImVec2(max.x - 10 - textSize.x, y), Packed(bad ? BAD_RGB : value == 0 ? FAINT_RGB : TEXT_RGB), text.c_str());
      float barLeft = min.x + 10 + labelWidth + 10;
      float barRight = max.x - 10 - numberWidth - 8;
      float mid = y + ImGui::GetTextLineHeight() / 2;
      if (barRight > barLeft)
      {
        drawList->AddRectFilled(ImVec2(barLeft, mid - 3), ImVec2(barRight, mid + 3), Packed(TRACK_RGB), 2.0f);
        if (largest[r] > 0 && value > 0)
          drawList->AddRectFilled(ImVec2(barLeft, mid - 3),
                                  ImVec2(barLeft + (barRight - barLeft) * value / largest[r], mid + 3),
                                  Packed(ChampionRgb(c)), 2.0f);
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
  // A summary too long for one line wraps onto a second; the tiles all have the height that takes
  float wrapWidth = tileWidth - 20;
  int lines = 1;
  for (int t = HeroTopicOverview + 1; t < HERO_TOPIC_COUNT && t < (int)summaries.size(); t++)
    lines = std::max(lines, (int)(ImGui::CalcTextSize(summaries[t].c_str(), NULL, false, wrapWidth).y / ImGui::GetTextLineHeight() + 0.5f));
  float height = ImGui::GetTextLineHeight() * (1 + lines) + 18;
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
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(min.x + 10, min.y + 9 + ImGui::GetTextLineHeight()),
                      Packed(DIM_RGB), summaries[t].c_str(), NULL, wrapWidth);
    drawList->PopClipRect();
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
  {
    if (TableGroupCount(page) == 0) break;
    if (SectionHeader(state, HERO_STATS_NUMBERS_SECTION, "Numbers", whose))
    {
      DrawNumbers(page, *state, columns);
      SectionGap();
    }
    break;
  }
  case HeroDetailWeapons: DrawWeapons(state, page.hero, columns); break;
  case HeroDetailSpells: DrawSpells(state, page.hero, columns); break;
  case HeroDetailHarm:
    DrawAmounts(state, HeroDetailHarm, "Damage taken by source", "Damage", "All sources", "taken", page.hero.taken, columns);
    break;
  case HeroDetailReduced:
    DrawAmounts(state, HeroDetailReduced, "Damage reduced by source", "Reduced", "All sources", "reduced", page.hero.reduced,
                columns);
    break;
  case HeroDetailDeaths: DrawDeaths(state, page.hero, columns); break;
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
  DrawPanels(state, page);
  std::string whose;  // the header row says whose the columns are
  // The page's sections: Numbers (the Records table is that page's numbers), then the detail tables
  std::vector<int> sections;
  if (state->topic != HeroTopicRecords) sections.push_back(HERO_STATS_NUMBERS_SECTION);
  for (size_t d = 0; d < page.details.size(); d++) sections.push_back(page.details[d]);
  // In a window wide enough for two sections side by side at their full width they stand in two
  // columns, in their order: the first ones on the left (HeroStatsLayout_Flow). Widths and heights
  // are the last frame's.
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
      float height = ImGui::GetCursorPosY() - top;
      if (fabsf(height - sizeOf[s]->height) > 1 || (widestTableBegun > 0 && fabsf(widestTableBegun - sizeOf[s]->fullWidth) > 1))
        sectionsMoved = true;
      sizeOf[s]->height = height;
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
static bool DrawPerLevelChoices(HeroStatsDrawState *state, size_t champions)
{
  std::vector<HeroStatsNumber> numbers = HeroStatsCatalogue_GridNumbers(state->topic);
  if (state->who != HERO_STATS_PARTY || numbers.empty()) return false;
  // Records are one at a time: fifteen side by side, each a best, would say little
  bool recordsOnly = state->topic == HeroTopicRecords;
  // One champion side by side with nobody would be the party's totals again
  if (champions == 1 && !recordsOnly) return false;
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
      bool ignored = IgnoredNumber(numbers[n]);
      if (ChoiceButton(label, state->gridNumber[state->topic] == numbers[n], ignored)) state->gridNumber[state->topic] = numbers[n];
      if (ignored)  // why it is grey: a hovertext, dotted under the label as any label with one
      {
        HelpTooltip(HeroStatsCatalogue_Info(numbers[n]).help);
        ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax(), pad = ImGui::GetStyle().FramePadding;
        DottedUnderline(min.x + pad.x, max.x - pad.x, max.y - pad.y);
      }
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
  if (ImGui::IsItemHovered()) WrappedTooltip(("Show " + LevelLabel(level) + " on its own").c_str());
  ImGui::PopID();
}

static void DrawPerLevel(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns)
{
  bool grid = DrawPerLevelChoices(state, columns.size());
  const HeroStatsPerLevel &table = CachedPerLevel(*state, grid, stats, columns);
  if (table.numbers.empty() || table.rows.empty()) return;
  // One champion is all of the party: the grid has no Total beside the champion's column
  bool withTotal = columns.size() != 1;
  int count;
  std::vector<int> headerColumns(1, -1);
  if (grid) count = 1 + (int)columns.size() + (withTotal ? 1 : 0) + (table.hasShare ? 1 : 0);
  else count = 1 + (int)table.numbers.size() + (table.hasShare ? 1 : 0);
  std::vector<std::string> labels;
  if (grid)
  {
    for (size_t c = 0; c < columns.size(); c++) labels.push_back(columns[c].name);
    if (withTotal) labels.push_back("Total");
    if (table.hasShare) labels.push_back("Share");
  }
  else
  {
    for (size_t n = 0; n < table.numbers.size(); n++)
    {
      // Beside Monsters killed, the champions' kills are a part of it, as on the page
      bool byChampions = state->topic == HeroTopicMonsters && state->who == HERO_STATS_PARTY && table.numbers[n] == HeroNumKills;
      labels.push_back(byChampions ? "By champions" : HeroStatsCatalogue_Info(table.numbers[n]).label);
    }
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
    size_t shown = grid && !withTotal ? columns.size() : row.values.size();
    for (size_t v = 0; v < shown && v < row.values.size(); v++)
    {
      ImGui::TableSetColumnIndex((int)v + 1);
      HeroStatsNumber number = grid ? table.numbers[0] : table.numbers[v];
      bool dimmed = grid && v < columns.size() && columns[v].hasLeft;
      if (v < row.runShare.size() && !row.runShare[v].empty())  // the time's share of the run
      {
        NumberCell(HeroStatsCatalogue_Format(number, row.values[v]) + " (" + row.runShare[v] + ")", row.values[v],
                   HeroStatsCatalogue_Info(number).isBad, rgb);
        continue;
      }
      CatalogueCell(number, row.values[v], dimmed ? DIM_RGB : rgb);
    }
    if (table.hasShare)
    {
      ImGui::TableSetColumnIndex((int)shown + 1);
      ShareBar(row.share);
    }
  }
  EndPageTable();
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
  std::string charges = HeroStatsLive_ChargesText(item.charges, item.gulps, item.torch);
  if (!charges.empty()) text += std::string(" ") + MIDDLE_DOT + " " + charges;
  return text;
}

// What a slot says under the pointer: the item large, its name, where it is, its weight and
// charges, and for a chest what it holds
static void SlotTooltip(const std::vector<HeroStatsLiveItem> &items, int index, const std::string &slotName, bool wounded)
{
  // Room around the picture: the window's own tooltips are text and need none
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
  ImGui::PushStyleColor(ImGuiCol_Border, Colour(TOOLTIP_BORDER_RGB));
  bool open = ImGui::BeginTooltip();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  if (!open) return;
  if (index < 0)
  {
    ImGui::TextColored(Colour(DIM_RGB), "%s %s empty", slotName.c_str(), MIDDLE_DOT);
    if (wounded)
    {
      ImGui::SameLine(0, 0);
      ImGui::TextColored(Colour(DIM_RGB), " %s", MIDDLE_DOT);
      ImGui::SameLine(0, 8);
      PillText("wounded");
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
    ImGui::TextColored(Colour(DIM_RGB), " %s", MIDDLE_DOT);
    ImGui::SameLine(0, 8);
    PillText("wounded");
  }
  ImGui::TextColored(Colour(PART_RGB), "%s", WeightAndCharges(item, held).c_str());
  // What the item's kind is worth as a weapon or as armor
  std::string weapon = HeroStatsLive_WeaponText(item), armor = HeroStatsLive_ArmorText(item);
  if (!weapon.empty()) ImGui::TextColored(Colour(DAMAGE_RGB), "%s", weapon.c_str());
  if (!armor.empty()) ImGui::TextColored(Colour(ARMOR_RGB), "%s", armor.c_str());
  if (item.food > 0) ImGui::TextColored(Colour(FOOD_RGB + 0x303030), "Food +%d", item.food);
  if (item.water > 0)
    ImGui::TextColored(Colour(WATER_RGB), item.gulps ? "Water +%d per gulp" : "Water +%d", item.water);
  ImGui::EndGroup();
  if (held > 0)
  {
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
    ImGui::TextColored(Colour(DIM_RGB), "Holds %d %s", held, held == 1 ? "item" : "items");
    if (ImGui::BeginTable("##held", 3, ImGuiTableFlags_SizingFixedFit))
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
        std::string charges = HeroStatsLive_ChargesText(inside.charges, inside.gulps, inside.torch);
        if (!charges.empty())
        {
          ImGui::SameLine(0, 0);
          ImGui::TextColored(Colour(DIM_RGB), " %s %s", MIDDLE_DOT, charges.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextColored(Colour(PART_RGB), "%s", HeroStatsLive_WeightText(inside.weight, false).c_str());
        // What eating or drinking it adds, as for an item in a slot
        ImGui::TableNextColumn();
        if (inside.food > 0) ImGui::TextColored(Colour(FOOD_RGB + 0x303030), "Food +%d", inside.food);
        if (inside.food > 0 && inside.water > 0) ImGui::SameLine();
        if (inside.water > 0)
          ImGui::TextColored(Colour(WATER_RGB), inside.gulps ? "Water +%d/gulp" : "Water +%d", inside.water);
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
static void LiveVitals(const HeroStatsLiveChampion &live, ImU32 rgb, float width, bool showDoses)
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
    if (line.textRgb == BAD_RGB) PillText(line.text);
    else ImGui::TextColored(Colour(line.textRgb), "%s", line.text.c_str());
  }
  float wrapAt = left + width;
  std::string wounds = HeroStatsLive_Wounds(live.wounds);
  ImGui::TextColored(Colour(DIM_RGB), "Wounds");
  ImGui::SameLine(left + LIVE_LABEL_WIDTH);
  ImGui::PushTextWrapPos(wrapAt);
  if (wounds.empty()) ImGui::TextColored(Colour(DIM_RGB), "none");
  else PillText(wounds, wrapAt - ImGui::GetCursorPosX());
  ImGui::PopTextWrapPos();
  ImGui::TextColored(Colour(DIM_RGB), "Poison");
  ImGui::SameLine(left + LIVE_LABEL_WIDTH);
  if (live.poisonCount > 0 || live.poison > 0)
  {
    // The game hurts max(1, v / 64) at each of a dose's v ticks; the doses work side by side
    int perTick = 0;
    for (size_t i = 0; i < live.poisonDoses.size(); i++) perTick += std::max(1, live.poisonDoses[i] / 64);
    ImGui::PushTextWrapPos(wrapAt);
    PillText(std::to_string(live.poison) + " (" + std::to_string(live.poisonCount) + (live.poisonCount == 1 ? " dose, " : " doses, ") +
                 std::to_string(perTick) + " HP per 36 ticks)", wrapAt - ImGui::GetCursorPosX());
    ImGui::PopTextWrapPos();
    if (ImGui::IsItemHovered())
      WrappedTooltip("The poison still to tick, all doses added up. A dose of strength v hurts max(1, v / 64) HP "
                     "every 36 ticks and gets weaker by 1 each time; a Cure Poison removes every dose.");
    // The doses fold away (30 and more can work on one champion); the fold is kept per champion.
    // The Party's cards leave it out, so a poisoned champion's card is no taller than the others
    if (showDoses && !live.poisonDoses.empty())
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

// An attribute's bar against the best of the party: the track is the largest value any champion
// has. Solid in the champion's colour up to what they have now; a drained part hatched in red up
// to the max; a raised part in violet past the max, after a small gap so that it reads beside a
// blue champion. Hovered: the numbers and the percent of the best.
static void AttributeBar(const HeroStatsLiveParty &party, const HeroStatsLiveChampion &live, int attribute, ImU32 rgb, float width)
{
  float lineHeight = ImGui::GetTextLineHeight();
  ImVec2 at = ImGui::GetCursorScreenPos();
  ImVec2 top(at.x, at.y + lineHeight / 2 - 4), bottom(at.x + width, at.y + lineHeight / 2 + 5);
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(top, bottom, Packed(TRACK_RGB), 2.0f);
  HeroStatsAttributeBar bar = HeroStatsLive_AttributeBar(live.attributeNow[attribute], live.attributeMax[attribute],
                                                         HeroStatsLive_AttributeTop(party, attribute));
  float solidEnd = top.x + width * bar.solid;
  if (bar.solid > 0) drawList->AddRectFilled(top, ImVec2(solidEnd, bottom.y), Packed(rgb), 2.0f);
  if (bar.lost > 0)
  {
    float lostEnd = solidEnd + width * bar.lost, height = bottom.y - top.y;
    drawList->PushClipRect(ImVec2(solidEnd, top.y), ImVec2(lostEnd, bottom.y), true);
    for (float x = solidEnd - height; x < lostEnd; x += 4)
      drawList->AddLine(ImVec2(x, bottom.y), ImVec2(x + height, top.y), Packed(BAD_RGB), 1.5f);
    drawList->PopClipRect();
  }
  if (bar.raised > 0)
  {
    float raisedEnd = solidEnd + width * bar.raised;
    float from = raisedEnd - solidEnd > 4 ? solidEnd + 2 : solidEnd;
    drawList->AddRectFilled(ImVec2(from, top.y), ImVec2(raisedEnd, bottom.y), Packed(RAISED_RGB));
  }
  ImGui::Dummy(ImVec2(width, lineHeight));
  if (ImGui::IsItemHovered()) WrappedTooltip(HeroStatsLive_AttributeHelp(party, live, attribute).c_str());
}

// The attributes, current and max: drained below the max in red, raised above it in violet; the
// bar against the best of the party is the first the table gives up when it is narrow
static void LiveAttributes(const HeroStatsLiveParty &party, const HeroStatsLiveChampion &live, ImU32 rgb)
{
  if (!BeginPageTable("##attributes", 4, false)) return;
  LabelColumn("Attribute");
  NumberColumn("Current");
  NumberColumn("Max");
  BarColumn("Against the best");
  HeaderRow(std::vector<int>(), std::vector<HeroStatsColumn>());
  for (int a = 0; a < HERO_LIVE_ATTRIBUTE_COUNT; a++)
  {
    NextGuidedRow();
    LabelCell(HeroStatsLive_AttributeName(a), 0, TEXT_RGB);
    int now = live.attributeNow[a], most = live.attributeMax[a];
    ImGui::TableSetColumnIndex(1);
    if (now < most)
    {
      std::string nowText = std::to_string(now);
      float room = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(nowText.c_str()).x;
      NoteWidth(ImGui::CalcTextSize(nowText.c_str()).x + 8);
      if (room > 4) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room - 4);
      PillText(nowText);
    }
    else RightText(std::to_string(now), now > most ? RAISED_RGB : TEXT_RGB);
    ImGui::TableSetColumnIndex(2);
    RightText(std::to_string(most), TEXT_RGB);
    if (ImGui::TableSetColumnIndex(3)) AttributeBar(party, live, a, rgb, ImGui::GetContentRegionAvail().x);
  }
  EndPageTable();
}

// A thin line between the parts of a Party card
static void CardRule(float width)
{
  ImGui::Dummy(ImVec2(0, 3));
  ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddLine(at, ImVec2(at.x + width, at.y), Packed(LINE_RGB));
  ImGui::Dummy(ImVec2(0, 5));
}

// The attributes on a Party card, as its vitals stand: the name, the bar against the best of the
// party taking the room left, then current / max with the current red when drained, violet when raised
static void CardAttributes(const HeroStatsLiveParty &party, const HeroStatsLiveChampion &live, ImU32 rgb, float width)
{
  float labels = 0, numbers = 0;
  for (int a = 0; a < HERO_LIVE_ATTRIBUTE_COUNT; a++)
  {
    labels = std::max(labels, ImGui::CalcTextSize(HeroStatsLive_AttributeName(a)).x);
    numbers = std::max(numbers, ImGui::CalcTextSize(OutOf(live.attributeNow[a], live.attributeMax[a]).c_str()).x);
  }
  labels += 10;
  float barWidth = std::max(30.0f, width - labels - 10 - numbers);
  float left = ImGui::GetCursorPosX();
  for (int a = 0; a < HERO_LIVE_ATTRIBUTE_COUNT; a++)
  {
    int now = live.attributeNow[a], most = live.attributeMax[a];
    ImGui::TextColored(Colour(DIM_RGB), "%s", HeroStatsLive_AttributeName(a));
    ImGui::SameLine(left + labels);
    AttributeBar(party, live, a, rgb, barWidth);
    std::string nowText = std::to_string(now), rest = " / " + std::to_string(most);
    float textWidth = ImGui::CalcTextSize((nowText + rest).c_str()).x;
    ImGui::SameLine(left + labels + barWidth + 10 + numbers - textWidth);
    if (now < most) PillText(nowText);
    else ImGui::TextColored(Colour(now > most ? RAISED_RGB : TEXT_RGB), "%s", nowText.c_str());
    ImGui::SameLine(0, 0);
    ImGui::TextColored(Colour(TEXT_RGB), "%s", rest.c_str());
  }
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
    if (ImGui::IsItemHovered()) WrappedTooltip((std::to_string(live.percentToNext[cls]) + "% of the way to the next rank").c_str());
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

// size: of a slot, the page's SLOT_SIZE or the smaller one of a Party card
static void LivePicture(const HeroStatsLiveChampion &live, bool packBeside, float size = SLOT_SIZE)
{
  static const char *const CAPTION[13] = {"hand", "hand", "head", "torso", "legs", "feet", "", "", "", "", "neck", "", ""};
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float step = size + SLOT_GAP;
  float dollWidth = HERO_LIVE_DOLL_COLUMNS * step - SLOT_GAP, packWidth = HERO_LIVE_BACKPACK_ACROSS * step - SLOT_GAP;
  for (int slot = 0; slot < HERO_LIVE_FIRST_BACKPACK_SLOT; slot++)
  {
    int column = 0, row = 0;
    if (!HeroStatsLive_DollCell(slot, &column, &row)) continue;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (column - 1) * step, origin.y + (row - 1) * step));
    DrawSlot(live.items, ItemIn(live.items, slot), HeroStatsLive_SlotName(slot), size, IsWounded(live, slot), CAPTION[slot]);
  }
  float captionY = origin.y + 3 * step - ImGui::GetFontSize() * 0.78f - 5;
  SmallCaption(ImVec2(origin.x, captionY), "Pouch");
  SmallCaption(ImVec2(origin.x + 3 * step, captionY), "Quiver");
  float dollHeight = HERO_LIVE_DOLL_ROWS * step - SLOT_GAP;
  ImVec2 pack = packBeside ? ImVec2(origin.x + dollWidth + PICTURE_GAP, origin.y) : ImVec2(origin.x, origin.y + dollHeight + 14);
  SmallCaption(pack, "Backpack");
  float packTop = pack.y + ImGui::GetFontSize() * 0.78f + 6;
  int count = HERO_LIVE_SLOT_COUNT - HERO_LIVE_FIRST_BACKPACK_SLOT;
  for (int i = 0; i < count; i++)
  {
    int slot = HERO_LIVE_FIRST_BACKPACK_SLOT + i;
    ImGui::SetCursorScreenPos(ImVec2(pack.x + (i % HERO_LIVE_BACKPACK_ACROSS) * step, packTop + (i / HERO_LIVE_BACKPACK_ACROSS) * step));
    DrawSlot(live.items, ItemIn(live.items, slot), HeroStatsLive_SlotName(slot), size, false, NULL);
  }
  int packRows = (count + HERO_LIVE_BACKPACK_ACROSS - 1) / HERO_LIVE_BACKPACK_ACROSS;
  float packBottom = packTop + packRows * step - SLOT_GAP;
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(packBeside ? dollWidth + PICTURE_GAP + packWidth : std::max(dollWidth, packWidth),
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
  // Weight stands last, at the edge: it matters most. A list too narrow gives up Kinetic first,
  // then Damage and Armor (they stay in the slot's hovertext); Food, Water and Weight always show.
  if (!BeginPageTable("##items", 8, true)) return;
  TextColumn("Slot", true, true);
  LabelColumn("Item", true);
  NumberColumn("Damage", false, true);
  NumberColumn("Kinetic", false, false, true);
  NumberColumn("Armor", false, true);
  NumberColumn("Food");
  NumberColumn("Water");
  NumberColumn("Weight");
  static const HeaderHelps HELPS = {"", "", "The weapon's own strength in a blow. Blank for what shoots: see its hovertext.",
                                    "Kinetic energy: how far and how hard it flies when thrown, or what a bow or sling adds to a shot.",
                                    "The armor's strength. Its sharp resistance is in its hovertext."};
  HeaderRow(std::vector<int>(), std::vector<HeroStatsColumn>(), "", HELPS);
  // By slot as the game numbers them, by name or by a column's number; a chest keeps what it holds
  // under it. A column the list left out sorts nothing: by slot then.
  static const HeroStatsLiveOrder ORDERS[8] = {HeroLiveBySlot, HeroLiveByName, HeroLiveByDamage, HeroLiveByKinetic,
                                               HeroLiveByArmor, HeroLiveByFood, HeroLiveByWater, HeroLiveByWeight};
  // ImGui moves the sort to Slot for good when its column is left out. The order the player
  // chose is kept here: by slot while its column is away, and theirs again when it is back.
  static int chosenColumn = -1;
  static bool chosenDescending = false, chosenAway = false;
  TableSort sort = CurrentSort();
  auto away = [](int column) {
    return InMeasuredTable() && column >= 0 && column < (int)measuring->disabled.size() && measuring->disabled[column];
  };
  if (chosenColumn >= 0 && away(chosenColumn) && sort.column > 0 && sort.column != chosenColumn && !away(sort.column))
  {
    // Another column picked while the chosen one is away: that is the choice now
    chosenAway = false;
    chosenColumn = sort.column;
    chosenDescending = sort.descending;
  }
  else if (chosenColumn >= 0 && away(chosenColumn)) chosenAway = true;
  else if (chosenAway && chosenColumn >= 0)
  {
    chosenAway = false;
    ImGui::TableSetColumnSortDirection(chosenColumn, chosenDescending ? ImGuiSortDirection_Descending : ImGuiSortDirection_Ascending, false);
    sort.column = chosenColumn;
    sort.descending = chosenDescending;
  }
  else
  {
    chosenColumn = sort.column;
    chosenDescending = sort.descending;
  }
  bool sortShows = sort.column >= 0 && sort.column < 8 && !chosenAway;
  HeroStatsLiveOrder by = sortShows ? ORDERS[sort.column] : HeroLiveBySlot;
  std::vector<size_t> order = HeroStatsLive_Order(live.items, by, sort.descending);
  for (size_t k = 0; k < order.size(); k++)
  {
    const HeroStatsLiveItem &item = live.items[order[k]];
    NextGuidedRow();
    ImGui::TableSetColumnIndex(0);
    if (!item.inChest) ImGui::TextColored(Colour(Hot(DIM_RGB)), "%s", HeroStatsLive_SlotName(item.slot).c_str());
    ImGui::TableSetColumnIndex(1);
    float indent = item.inChest ? 14.0f : 0.0f;
    std::string charges = HeroStatsLive_ChargesText(item.charges, item.gulps, item.torch);
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
    // What it is worth as a weapon and as armor; blank for what is neither, and for what the list left out
    int damage = HeroStatsLive_ListDamage(item), kinetic = HeroStatsLive_ListKinetic(item);
    if (ImGui::TableSetColumnIndex(2) && damage >= 0) RightText(std::to_string(damage), DAMAGE_RGB);
    if (ImGui::TableSetColumnIndex(3) && kinetic >= 0) RightText(std::to_string(kinetic), DAMAGE_RGB);
    if (ImGui::TableSetColumnIndex(4) && item.armor >= 0) RightText(std::to_string(item.armor), ARMOR_RGB);
    // What eating or drinking it adds (a waterskin: per gulp); blank for what is neither
    ImGui::TableSetColumnIndex(5);
    if (item.food > 0) RightText(std::to_string(item.food), item.inChest ? PART_RGB : TEXT_RGB);
    ImGui::TableSetColumnIndex(6);
    if (item.water > 0) RightText(std::to_string(item.water), item.inChest ? PART_RGB : TEXT_RGB);
    ImGui::TableSetColumnIndex(7);
    RightText(HeroStatsLive_WeightText(item.weight, false), item.inChest ? PART_RGB : TEXT_RGB);
  }
  EndPageTable();
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

// A card of the Party's page: who, how they are, what they hold and wear, and their ranks. With
// room for it (step), the attributes and the whole inventory stand in place of the strip of what
// is held and worn. True when the card itself was clicked (not a slot or a fold in it).
// *frame: the card's height without that middle part, the same at every step; *middle: the
// middle part's height as drawn.
static bool LiveCard(const HeroStatsLiveParty &party, const HeroStatsLiveChampion &live, ImU32 rgb, float width,
                     HeroStatsPartyStep step, float *frame, float *middle)
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
    LiveVitals(live, rgb, inner, false);
    ImGui::Dummy(ImVec2(0, 4));
    float middleTop = ImGui::GetCursorPosY();
    // What is held and worn: both hands, head, neck, torso, legs, feet
    static const int STRIP[7] = {0, 1, 2, 10, 3, 4, 5};
    const float size = 36;
    float left = ImGui::GetCursorPosX();
    if (step != HeroPartyPlain)
    {
      // The whole inventory holds the strip's items: the body, pouch and quiver, the backpack under them
      if (step == HeroPartyAttributesAndInventory)
      {
        CardRule(inner);
        CardAttributes(party, live, rgb, inner);
      }
      CardRule(inner);
      LivePicture(live, false, size);
    }
    for (int i = 0; i < 7 && step == HeroPartyPlain; i++)
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
    if (carried > 0 && step == HeroPartyPlain)
    {
      std::string more = "+" + std::to_string(carried) + " carried";
      ImGui::SameLine(0, 10);
      if (ImGui::GetCursorPosX() + ImGui::CalcTextSize(more.c_str()).x > left + inner) ImGui::NewLine();
      else ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (size - ImGui::GetTextLineHeight()) / 2);
      ImGui::TextColored(Colour(DIM_RGB), "%s", more.c_str());
    }
    if (step != HeroPartyPlain) CardRule(inner);
    else ImGui::Dummy(ImVec2(0, 4));
    *middle = ImGui::GetCursorPosY() - middleTop;
    LiveRanks(live, rgb, inner);
    // From the content, not from the card's size: that follows its content a frame late
    *frame = ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y - *middle;
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
static void DrawLiveChampion(HeroStatsDrawState *state, const HeroStatsLiveParty &party, const HeroStatsLiveChampion &live, ImU32 rgb)
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
    LiveVitals(live, rgb, leftWidth, true);
    ImGui::Dummy(ImVec2(0, 8));
    if (SectionHeader(state, HERO_STATS_ATTRIBUTES_SECTION, "Attributes", ""))
    {
      LiveAttributes(party, live, rgb);
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
    if (isParty && !live.champions.empty())
    {
      // The item on the cursor, as a slot of its own. The line is there with nothing on the
      // cursor too, so the cards do not move up and down as items are taken and put away.
      bool holds = !live.cursor.name.empty();
      std::vector<HeroStatsLiveItem> cursor(1, live.cursor);
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(Colour(DIM_RGB), "On the cursor:");
      ImGui::SameLine(0, 10);
      float y = ImGui::GetCursorPosY();
      DrawSlot(cursor, holds ? 0 : -1, "On the cursor", 36, false, NULL);
      ImGui::SameLine(0, 10);
      ImGui::SetCursorPosY(y + (36 - ImGui::GetTextLineHeight()) / 2);
      ImGui::TextColored(Colour(holds ? TEXT_RGB : FAINT_RGB), "%s", holds ? live.cursor.name.c_str() : "nothing");
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
      // With all the cards in one row, each shows the most that fits in the page's height without
      // scrolling: attributes and inventory, the inventory alone, or the card as it is. The
      // heights are the last frame's: a card's frame (the same at every step, the tallest card's
      // counts) and the middle part of each step as it was last drawn.
      static float frameSeen[8] = {0}, middleSeen[3] = {0, 0, 0};
      float line = ImGui::GetTextLineHeightWithSpacing(), tallest = 0;
      bool framesKnown = true;
      for (int i = 0; i < count && i < 8; i++)
      {
        framesKnown = framesKnown && frameSeen[i] > 0;
        tallest = std::max(tallest, frameSeen[i]);
      }
      // What a step not yet drawn needs, from the sizes LiveCard, CardRule and LivePicture draw
      // with, and never less than they do: a step is first shown only when it surely fits, so
      // the page never scrolls for having shown more
      const float cardSlot = 36, cardStep = cardSlot + SLOT_GAP, spacing = ImGui::GetStyle().ItemSpacing.y;
      float rule = 3 + 5 + 2 * spacing;
      float picture = HERO_LIVE_DOLL_ROWS * cardStep - SLOT_GAP + 14 + ImGui::GetFontSize() * 0.78f + 6 + 3 * cardStep - SLOT_GAP + spacing;
      float pictureNeeds = rule + picture + rule + 2, attributesNeed = rule + HERO_LIVE_ATTRIBUTE_COUNT * line;
      float withInventory = tallest + (middleSeen[HeroPartyInventory] > 0 ? middleSeen[HeroPartyInventory] : pictureNeeds);
      float withBoth = tallest + (middleSeen[HeroPartyAttributesAndInventory] > 0 ? middleSeen[HeroPartyAttributesAndInventory]
                                                                                 : pictureNeeds + attributesNeed);
      float cardInner = width - 2 * 12, pictureWidth = HERO_LIVE_BACKPACK_ACROSS * cardStep - SLOT_GAP;
      HeroStatsPartyStep step = framesKnown && count <= 8
          ? HeroStatsLayout_PartyStep(count, across, withBoth, withInventory, ImGui::GetContentRegionAvail().y, cardInner, pictureWidth)
          : HeroPartyPlain;
      HeroStatsChampion picked = HERO_STATS_PARTY;
      bool anyPicked = false;
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
      for (int i = 0; i < count; i++)
      {
        const HeroStatsLiveChampion &champion = live.champions[i];
        if (i % across != 0) ImGui::SameLine(0, gap);
        ImGui::PopStyleVar();
        float frame = 0, middle = 0;
        bool clicked = LiveCard(live, champion, ChampionRgb(ColumnOf(columns, champion.who)), width, step, &frame, &middle);
        if (i < 8 && frame > 0) frameSeen[i] = frame;
        if (middle > 0) middleSeen[step] = middle;
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
      if (champion != NULL) DrawLiveChampion(state, live, *champion, ChampionRgb(ColumnOf(columns, champion->who)));
    }
  }
  ImGui::EndChild();
}

bool HeroStatsDraw_WantsAnotherPass(void)
{
  if (settlingPasses > 0)
  {
    settlingPasses--;
    return true;
  }
  if (sectionsMoved) return true;
  for (size_t t = 0; t < tablesThisFrame.size(); t++)
  {
    const TableMeasure &measure = *tablesThisFrame[t];
    if (!measure.known) return true;
    for (size_t i = 0; i < measure.seen.size() && i < measure.width.size() && i < measure.kinds.size(); i++)
    {
      bool measured = measure.kinds[i] != ColumnText && !(i < measure.disabled.size() && measure.disabled[i]);
      if (measured && fabsf(measure.seen[i] - measure.width[i]) > 0.5f) return true;
    }
  }
  return false;
}

void HeroStatsDraw_Frame(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                         const HeroStatsPicker &picker, const HeroStatsLiveParty &live, const HeroStatsDrawPictures &pictures,
                         void (*exportStatistics)(void))
{
  livePictures = &pictures;
  tablesThisFrame.clear();
  sectionsMoved = false;
  // What decides the layout besides the numbers: when it changes, the next frames settle unseen
  static std::string shownBefore;
  char shown[160];
  snprintf(shown, sizeof shown, "%u|%d|%d|%d|%d|%d|%d|%d|%d", (unsigned)state->who, state->topic, state->level, (int)state->perLevel,
           (int)state->sideBySide, state->closedSections, (int)state->inventoryList, (int)ImGui::GetWindowWidth(), (int)columns.size());
  if (shownBefore != shown)
  {
    shownBefore = shown;
    settlingPasses = 3;
  }
  // One row: who on the left; the levels and the export on the right, or under it when narrow
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 8));
  float whoWidth = DrawWhoRow(state, columns);
  // The levels and Per level are the statistics': the Champions tab shows the party now
  bool hasPicker = state->topic != HERO_STATS_CHAMPIONS_TAB;
  // The row of level numbers that does not fit even on a line of its own becomes the stepper
  HeroStatsPicker fitted = picker;
  if (!fitted.stepper && GUTTER * 2 + PickerWidth(fitted) > ImGui::GetWindowWidth()) fitted.stepper = true;
  const char *exportLabel = "Export as HTML...";
  const float pickerGap = 10;
  float rightWidth = (hasPicker ? PickerWidth(fitted) + pickerGap : 0) + ButtonWidth(exportLabel);
  float rightLeft = ImGui::GetWindowWidth() - GUTTER - rightWidth;
  if (rightLeft > GUTTER + whoWidth + 16) ImGui::SetCursorPosX(rightLeft);
  else
  {
    // Too narrow to sit beside the champions: a line of its own
    ImGui::NewLine();
    ImGui::SetCursorPosX(GUTTER);
  }
  if (hasPicker)
  {
    DrawPicker(state, fitted);
    ImGui::SameLine(0, pickerGap);
    // And the export a line of its own when it does not fit beside the levels
    if (ImGui::GetCursorPosX() + ButtonWidth(exportLabel) > ImGui::GetWindowWidth() - GUTTER)
    {
      ImGui::NewLine();
      ImGui::SetCursorPosX(GUTTER);
    }
  }
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  if (WhoButton(exportLabel, false, true)) exportStatistics();
  ImGui::PopStyleVar();
  ImGui::Dummy(ImVec2(0, 2));
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
