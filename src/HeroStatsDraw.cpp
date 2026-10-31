// HeroStatsDraw.cpp : the pages of the Statistics window, drawn with Dear ImGui. Every page is
// made from HeroStatsCatalogue through HeroStatsPages; nothing here counts anything.
#include "stdafx.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "imgui.h"
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
  return state;
}

int HeroStatsDraw_SectionBit(int detail)
{
  if (detail == HERO_STATS_NUMBERS_SECTION) return 1 << HERO_DETAIL_COUNT;
  if (detail == HERO_STATS_KEY_NUMBERS_SECTION) return 1 << (HERO_DETAIL_COUNT + 1);
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
  const char *exportLabel = "Export as HTML...";
  float exportX = origin.x + width - ButtonWidth(exportLabel) - 4;
  std::string crumb = WhoLabel(columns, state.who) + " \xc2\xb7 " + HeroStatsCatalogue_Topic(state.topic).label +
                      " \xc2\xb7 " + (state.perLevel ? std::string("Per level") : LevelLabel(state.level));
  float crumbX = exportX - 12 - ImGui::CalcTextSize(crumb.c_str()).x;
  if (crumbX > origin.x + 120) drawList->AddText(ImVec2(crumbX, origin.y + 4), Packed(CRUMB_RGB), crumb.c_str());
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
    ImGui::SameLine();
    ImGui::PushID((int)i);
    std::string label = "   " + columns[i].name;  // room for the dot
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
  float width = ButtonWidth("Whole run") + spacing + ButtonWidth("Per level") + 8 + ImGui::CalcTextSize("Lv").x + 6;
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
  for (int t = 0; t < HERO_TOPIC_COUNT; t++)
  {
    if (t > 0) ImGui::SameLine(0, 2);
    const char *label = HeroStatsCatalogue_Topic(t).label;
    ImVec2 size = ImGui::CalcTextSize(label);
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

static const float NUMBER_WIDTH = 70;
static const float BAR_WIDTH = 170;
static const float WHO_WIDTH = 140;
static const float LABEL_SMALLEST = 190;

static void RightText(const std::string &text, ImU32 rgb)
{
  float room = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text.c_str()).x;
  if (room > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room);
  ImGui::TextColored(Colour(rgb), "%s", text.c_str());
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
  float room = ImGui::GetContentRegionAvail().x - width;
  if (room > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room);
  ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(at.x - 4, at.y - 1), ImVec2(at.x + width + 4, at.y + ImGui::GetTextLineHeight() + 1),
                                            Packed(BAD_RGB, 0.16f), 3.0f);
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
  float summaryWidth = ImGui::CalcTextSize(summary.c_str()).x;
  float titleEnd = at.x + 16 + ImGui::CalcTextSize(upper.c_str()).x + 20;
  if (at.x + width - summaryWidth > titleEnd)
    drawList->AddText(ImVec2(at.x + width - summaryWidth, at.y + 3), Packed(DIM_RGB), summary.c_str());
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

// A table as wide as the page, or as wide as its columns need when the page is narrower
static bool BeginPageTable(const char *id, int columnCount, float fixedWidth, bool sortable)
{
  float width = std::max(ImGui::GetContentRegionAvail().x, LABEL_SMALLEST + fixedWidth);
  ImGuiTableFlags flags = TABLE_FLAGS | (sortable ? ImGuiTableFlags_Sortable : 0);
  return ImGui::BeginTable(id, columnCount, flags, ImVec2(width, 0));
}

// The label column takes the room the numbers leave
static void LabelColumn(const char *label, bool sortable = false)
{
  ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_WidthStretch;
  if (!sortable) flags |= ImGuiTableColumnFlags_NoSort;
  ImGui::TableSetupColumn(label, flags);
}

static void NumberColumn(const char *label, float width = NUMBER_WIDTH, bool sortsFirst = false)
{
  ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending;
  if (sortsFirst) flags |= ImGuiTableColumnFlags_DefaultSort;
  ImGui::TableSetupColumn(label, flags, width);
}

static void BarColumn(const char *label, float width)
{
  ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, width);
}

// The header row: the label left, numbers right-aligned; a champion's name after their dot
// (columnOfHeader[i]: the champion column of table column i, or -1), dimmed when they left
static void HeaderRow(const std::vector<int> &columnOfHeader, const std::vector<HeroStatsColumn> &columns)
{
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  int count = ImGui::TableGetColumnCount();
  for (int i = 0; i < count; i++)
  {
    if (!ImGui::TableSetColumnIndex(i)) continue;
    const char *name = ImGui::TableGetColumnName(i);
    int champion = i < (int)columnOfHeader.size() ? columnOfHeader[i] : -1;
    bool dimmed = champion >= 0 && columns[champion].hasLeft;
    // Numbers right-aligned, room left for the sort arrow; a champion's dot in two spaces before the name
    std::string prefix;
    float spaceWidth = ImGui::CalcTextSize(" ").x;
    if (i > 0)
    {
      float textWidth = ImGui::CalcTextSize(name).x + (champion >= 0 ? spaceWidth * 2 : 0);
      int spaces = (int)((ImGui::GetContentRegionAvail().x - textWidth - 14) / spaceWidth);
      if (spaces > 0) prefix = std::string(spaces, ' ');
    }
    std::string label = prefix + (champion >= 0 ? "  " : "") + name;
    ImGui::PushStyleColor(ImGuiCol_Text, Colour(dimmed ? FAINT_RGB : DIM_RGB));
    ImGui::PushID(i);
    ImGui::TableHeader(label.c_str());
    ImGui::PopID();
    ImGui::PopStyleColor();
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
  ImGui::TableNextRow();
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

static void LabelCell(const std::string &text, int depth, ImU32 rgb, const char *help = "")
{
  ImGui::TableSetColumnIndex(0);
  if (depth > 0) ImGui::Indent(14);
  ImGui::TextColored(Colour(rgb), "%s", text.c_str());
  if (help[0] != 0 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", help);
  if (depth > 0) ImGui::Unindent(14);
}

// ---------------------------------------------------------------------------------------------
// Tiles and the Numbers table

static void DrawTiles(const HeroStatsTopicPage &page)
{
  if (page.tiles.empty()) return;
  int count = (int)page.tiles.size();
  float gap = 8;
  float width = ImGui::GetContentRegionAvail().x;
  float tileWidth = (width - gap * 3) / 4;
  float bigSize = ImGui::GetFontSize() * 1.6f;
  float height = bigSize + ImGui::GetTextLineHeight() + 18;
  ImVec2 at = ImGui::GetCursorScreenPos();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  for (int i = 0; i < count; i++)
  {
    HeroStatsNumber number = page.tiles[i];
    int value = page.record.value[number];
    const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(number);
    ImVec2 min(at.x + i * (tileWidth + gap), at.y);
    drawList->AddRectFilled(min, ImVec2(min.x + tileWidth, min.y + height), Packed(PANEL_RGB), 4.0f);
    drawList->PushClipRect(min, ImVec2(min.x + tileWidth - 6, min.y + height), true);
    std::string text = HeroStatsCatalogue_Format(number, value);
    drawList->AddText(ImGui::GetFont(), bigSize, ImVec2(min.x + 10, min.y + 7), Packed(info.isBad && value > 0 ? BAD_RGB : WHITE_RGB),
                      text.c_str());
    drawList->AddText(ImVec2(min.x + 10, min.y + 9 + bigSize), Packed(DIM_RGB), info.label);
    drawList->PopClipRect();
  }
  ImGui::Dummy(ImVec2(width, height + 4));
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
  float fixed = isParty ? NUMBER_WIDTH * (columns.size() + 1) + BAR_WIDTH : NUMBER_WIDTH * 2 + BAR_WIDTH;
  ImGui::PushID("numbers");
  if (!BeginPageTable("##numbers", count, fixed, false))
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
  BarColumn(isParty ? "Share" : "Share of party", BAR_WIDTH);
  HeaderRow(headerColumns, columns);
  for (size_t r = 0; r < page.numbers.size(); r++)
  {
    const HeroStatsNumbersRow &row = page.numbers[r];
    const HeroStatsNumberInfo &info = HeroStatsCatalogue_Info(row.number);
    ImU32 rgb = row.depth > 0 ? PART_RGB : TEXT_RGB;
    ImGui::TableNextRow();
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

static void WhoCell(const std::vector<int> &byColumn, bool isParty)
{
  if (isParty) ShareBar(byColumn);
}

template <typename Line>
static void AttackCells(const Line &line, ImU32 rgb)
{
  ImGui::TableSetColumnIndex(1);
  PlainCell(line.tries + line.uses, rgb);
  if (line.tries == 0 && line.uses > 0)
  {
    // Actions that cannot hit (War Cry ...): no hits, rate, average, damage or kills
    for (int column = 2; column <= 6; column++)
    {
      ImGui::TableSetColumnIndex(column);
      RightText("-", FAINT_RGB);
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
  if (BeginPageTable("##weapons", count, NUMBER_WIDTH * 6 + (isParty ? WHO_WIDTH : 0), true))
  {
    LabelColumn("Weapon", true);
    NumberColumn("Tries");
    NumberColumn("Hits");
    NumberColumn("Rate");
    NumberColumn("Avg");
    NumberColumn("Damage", NUMBER_WIDTH, true);
    NumberColumn("Kills");
    if (isParty) BarColumn("Who", WHO_WIDTH);
    HeaderRow(std::vector<int>(), columns);
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
      ImGui::PushID((int)state->who);
      ImGui::PushID(weapon.id);
      bool open = ImGui::TreeNodeEx(weapon.name.c_str(), ImGuiTreeNodeFlags_SpanLabelWidth);
      AttackCells(weapon, TEXT_RGB);
      if (isParty)
      {
        ImGui::TableSetColumnIndex(7);
        WhoCell(weapon.byColumn, true);
      }
      if (open)
      {
        for (size_t o = 0; o < weapon.options.size(); o++)
        {
          ImGui::TableNextRow();
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

static int SpellValue(const HeroStatsSpellLine &line, int column)
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
  HeroStatsSpellLine all = {"All spells", 0, 0, 0, 0, 0, 0, 0, std::vector<int>()};
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
  if (BeginPageTable("##spells", count, NUMBER_WIDTH * 6 + (isParty ? WHO_WIDTH : 0), true))
  {
    LabelColumn("Spell", true);
    NumberColumn("Casts");
    NumberColumn("Failed");
    NumberColumn("Mana", NUMBER_WIDTH, true);
    NumberColumn("Wasted");
    NumberColumn("Damage");
    NumberColumn("Kills");
    if (isParty) BarColumn("Who", WHO_WIDTH);
    HeaderRow(std::vector<int>(), columns);
    std::vector<HeroStatsSpellLine> spells = page.spells;
    SortRows(&spells, CurrentSort(), [](const HeroStatsSpellLine &a, const HeroStatsSpellLine &b, int column) {
      if (column == 0) return a.name.compare(b.name);
      return Compare(SpellValue(a, column), SpellValue(b, column));
    });
    for (size_t i = 0; i <= spells.size(); i++)
    {
      bool isAll = i == spells.size();
      const HeroStatsSpellLine &line = isAll ? all : spells[i];
      ImU32 rgb = isAll ? WHITE_RGB : TEXT_RGB;
      if (isAll) TotalRow();
      else ImGui::TableNextRow();
      LabelCell(line.name, 0, rgb);
      for (int column = 1; column <= 6; column++)
      {
        ImGui::TableSetColumnIndex(column);
        PlainCell(SpellValue(line, column), rgb, column == 2 || column == 4);
      }
      if (isParty && !isAll)
      {
        ImGui::TableSetColumnIndex(7);
        WhoCell(line.byColumn, true);
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
  if (BeginPageTable("##amounts", isParty ? 3 : 2, NUMBER_WIDTH + (isParty ? WHO_WIDTH : 0), true))
  {
    LabelColumn("Source", true);
    NumberColumn(column, NUMBER_WIDTH, true);
    if (isParty) BarColumn("Who", WHO_WIDTH);
    HeaderRow(std::vector<int>(), columns);
    std::vector<HeroStatsAmountLine> lines = source;
    SortRows(&lines, CurrentSort(), [](const HeroStatsAmountLine &a, const HeroStatsAmountLine &b, int c) {
      return c == 0 ? a.text.compare(b.text) : Compare(a.amount, b.amount);
    });
    for (size_t i = 0; i < lines.size(); i++)
    {
      ImGui::TableNextRow();
      LabelCell(lines[i].text, 0, TEXT_RGB);
      ImGui::TableSetColumnIndex(1);
      PlainCell(lines[i].amount, TEXT_RGB, lines[i].isBad);
      if (isParty)
      {
        ImGui::TableSetColumnIndex(2);
        WhoCell(lines[i].byColumn, true);
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
  std::vector<int> byColumn;
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
      if (at == lines.size()) lines.push_back({from.text, 0, 0, std::vector<int>(columns.size(), 0)});
      (list == 0 ? lines[at].eaten : lines[at].drunk) += from.amount;
      for (size_t c = 0; c < from.byColumn.size() && c < columns.size(); c++) lines[at].byColumn[c] += from.byColumn[c];
    }
  std::string summary = Joined(Count(page.eatenTotal, "eaten"), Count(page.drunkTotal, "drunk"));
  if (!SectionHeader(state, HeroDetailConsumed, "Eaten and drunk", lines.empty() ? "nothing" : summary)) return;
  if (lines.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "Nothing eaten or drunk.");
    SectionGap();
    return;
  }
  if (BeginPageTable("##consumed", isParty ? 4 : 3, NUMBER_WIDTH * 2 + (isParty ? WHO_WIDTH : 0), true))
  {
    LabelColumn("Item", true);
    NumberColumn("Eaten", NUMBER_WIDTH, true);
    NumberColumn("Drunk");
    if (isParty) BarColumn("Who", WHO_WIDTH);
    HeaderRow(std::vector<int>(), columns);
    SortRows(&lines, CurrentSort(), [](const ConsumedLine &a, const ConsumedLine &b, int c) {
      if (c == 0) return a.name.compare(b.name);
      if (c == 1) return Compare(a.eaten, b.eaten);
      if (c == 2) return Compare(a.drunk, b.drunk);
      return Compare(a.eaten + a.drunk, b.eaten + b.drunk);
    });
    for (size_t i = 0; i <= lines.size(); i++)
    {
      bool isAll = i == lines.size();
      if (isAll) TotalRow();
      else ImGui::TableNextRow();
      ImU32 rgb = isAll ? WHITE_RGB : TEXT_RGB;
      LabelCell(isAll ? "Everything" : lines[i].name, 0, rgb);
      ImGui::TableSetColumnIndex(1);
      PlainCell(isAll ? page.eatenTotal : lines[i].eaten, rgb);
      ImGui::TableSetColumnIndex(2);
      PlainCell(isAll ? page.drunkTotal : lines[i].drunk, rgb);
      if (isParty && !isAll)
      {
        ImGui::TableSetColumnIndex(3);
        WhoCell(lines[i].byColumn, true);
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
  std::string summary = Joined(Count(all.pickedUp, "picked up"), Count(all.usedInWall, "used in walls"));
  if (!SectionHeader(state, HeroDetailItems, "Items", lines.empty() ? "none" : summary)) return;
  if (lines.empty())
  {
    ImGui::TextColored(Colour(DIM_RGB), "No items picked up or used in walls.");
    SectionGap();
    return;
  }
  if (BeginPageTable("##items", 3, NUMBER_WIDTH * 2 + 30, true))
  {
    LabelColumn("Item", true);
    NumberColumn("Picked up", NUMBER_WIDTH, true);
    NumberColumn("Used in walls", NUMBER_WIDTH + 30);
    HeaderRow(std::vector<int>(), columns);
    SortRows(&lines, CurrentSort(), [](const HeroStatsItemLine &a, const HeroStatsItemLine &b, int c) {
      if (c == 0) return a.name.compare(b.name);
      return c == 1 ? Compare(a.pickedUp, b.pickedUp) : Compare(a.usedInWall, b.usedInWall);
    });
    for (size_t i = 0; i <= lines.size(); i++)
    {
      bool isAll = i == lines.size();
      const HeroStatsItemLine &line = isAll ? all : lines[i];
      if (isAll) TotalRow();
      else ImGui::TableNextRow();
      ImU32 rgb = isAll ? WHITE_RGB : TEXT_RGB;
      LabelCell(line.name, 0, rgb);
      ImGui::TableSetColumnIndex(1);
      PlainCell(line.pickedUp, rgb);
      ImGui::TableSetColumnIndex(2);
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
  float fixed = NUMBER_WIDTH * (count - 1 - (isParty ? 1 : 0)) + (isParty ? WHO_WIDTH : 0);
  if (BeginPageTable("##monsters", count, fixed, true))
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
      NumberColumn("All", NUMBER_WIDTH, true);
      BarColumn("Who", WHO_WIDTH);
    }
    else NumberColumn("Kills", NUMBER_WIDTH, true);
    HeaderRow(headerColumns, columns);
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
      else ImGui::TableNextRow();
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
        WhoCell(split, true);
      }
    }
    ImGui::EndTable();
  }
  SectionGap();
}

// ---------------------------------------------------------------------------------------------
// Overview

// The Party's Overview: a card per champion with small bars against the best of them
static void DrawCards(HeroStatsDrawState *state, const std::vector<HeroStatsCard> &cards)
{
  if (cards.empty()) return;
  static const HeroStatsNumber ROWS[6] = {HeroNumDamage, HeroNumKills, HeroNumTaken, HeroNumHealed, HeroNumManaSpent, HeroNumDeaths};
  static const char *const LABELS[6] = {"Damage", "Kills", "Taken", "Healed", "Mana", "Deaths"};
  int largest[6] = {0};
  for (size_t c = 0; c < cards.size(); c++)
    for (int r = 0; r < 6; r++) largest[r] = std::max(largest[r], cards[c].record.value[ROWS[r]]);
  float gap = 8;
  int perRow = std::min(4, (int)cards.size());
  float width = ImGui::GetContentRegionAvail().x;
  float cardWidth = (width - gap * 3) / 4;
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
    if (ImGui::InvisibleButton("##card", ImVec2(cardWidth, height))) state->who = card.column.id;
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
      float textWidth = ImGui::CalcTextSize(text.c_str()).x;
      drawList->AddText(ImVec2(max.x - 10 - textWidth, y), Packed(bad ? BAD_RGB : value == 0 ? FAINT_RGB : TEXT_RGB), text.c_str());
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
}

// A champion's Overview: a tile per topic with its summary; a click opens the topic
static void DrawTopicTiles(HeroStatsDrawState *state, const std::vector<std::string> &summaries)
{
  float gap = 8;
  float width = ImGui::GetContentRegionAvail().x;
  float tileWidth = (width - gap) / 2;
  float height = ImGui::GetTextLineHeight() * 2 + 18;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  int shown = 0;
  for (int t = HeroTopicOverview + 1; t < HERO_TOPIC_COUNT && t < (int)summaries.size(); t++, shown++)
  {
    ImVec2 min(origin.x + (shown % 2) * (tileWidth + gap), origin.y + (shown / 2) * (height + gap));
    ImGui::SetCursorScreenPos(min);
    ImGui::PushID(t);
    if (ImGui::InvisibleButton("##topic", ImVec2(tileWidth, height))) state->topic = t;
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    drawList->AddRectFilled(min, ImVec2(min.x + tileWidth, min.y + height), Packed(hovered ? PANEL_HOVERED_RGB : PANEL_RGB), 4.0f);
    drawList->PushClipRect(min, ImVec2(min.x + tileWidth - 8, min.y + height), true);
    drawList->AddText(ImVec2(min.x + 10, min.y + 7), Packed(WHITE_RGB), HeroStatsCatalogue_Topic(t).label);
    drawList->AddText(ImVec2(min.x + 10, min.y + 9 + ImGui::GetTextLineHeight()), Packed(DIM_RGB), summaries[t].c_str());
    drawList->PopClipRect();
    if (hovered) ImGui::SetTooltip("%s", summaries[t].c_str());
  }
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, ((shown + 1) / 2) * (height + gap) + 4));
}

// ---------------------------------------------------------------------------------------------
// The page

static void DrawTopicPage(HeroStatsDrawState *state, const PageData &data, const std::vector<HeroStatsColumn> &columns)
{
  const HeroStatsTopicPage &page = data.topic;
  bool isParty = state->who == HERO_STATS_PARTY;
  DrawTiles(page);
  if (state->topic == HeroTopicOverview)
  {
    if (isParty) DrawCards(state, data.cards);
    else DrawTopicTiles(state, data.topicSummaries);
    if (SectionHeader(state, HERO_STATS_KEY_NUMBERS_SECTION, "Key numbers",
                      isParty ? "one row from each topic" : "compared with the party"))
    {
      DrawNumbers(page, *state, columns);
      SectionGap();
    }
    return;
  }
  std::string whose = isParty ? "each champion and the party" : WhoLabel(columns, state->who) + " and the party";
  if (SectionHeader(state, HERO_STATS_NUMBERS_SECTION, "Numbers", whose))
  {
    DrawNumbers(page, *state, columns);
    SectionGap();
  }
  for (size_t d = 0; d < page.details.size(); d++)
  {
    switch (page.details[d])
    {
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
    default: break;
    }
  }
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
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 6));
  ChoiceLabel("Show  ");
  if (ChoiceButton("Party totals", !state->sideBySide)) state->sideBySide = false;
  ImGui::SameLine();
  if (ChoiceButton("Champions side by side", state->sideBySide)) state->sideBySide = true;
  if (state->sideBySide)
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
  return state->sideBySide;
}

static void LevelCell(HeroStatsDrawState *state, int level)
{
  ImGui::TableSetColumnIndex(0);
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
  // The columns' widths first: a table wider than the page scrolls sideways with it
  std::vector<std::string> labels;
  std::vector<float> widths;
  if (grid)
  {
    for (size_t c = 0; c < columns.size(); c++)
    {
      labels.push_back(columns[c].name);
      widths.push_back(std::max(NUMBER_WIDTH, ImGui::CalcTextSize(columns[c].name.c_str()).x + 40));
    }
    labels.push_back("Party");
    widths.push_back(NUMBER_WIDTH);
    if (table.hasShare)
    {
      labels.push_back("Share");
      widths.push_back(BAR_WIDTH);
    }
  }
  else
  {
    for (size_t n = 0; n < table.numbers.size(); n++)
    {
      labels.push_back(HeroStatsCatalogue_Info(table.numbers[n]).label);
      widths.push_back(std::max(NUMBER_WIDTH, ImGui::CalcTextSize(labels.back().c_str()).x + 26));
    }
    if (table.hasShare)
    {
      labels.push_back(std::string("Who: ") + HeroStatsCatalogue_Info(table.shareNumber).label);
      widths.push_back(std::max(WHO_WIDTH, ImGui::CalcTextSize(labels.back().c_str()).x + 26));
    }
  }
  const float LEVEL_WIDTH = 90;
  float tableWidth = LEVEL_WIDTH + ImGui::GetStyle().CellPadding.x * 2 * count;
  for (size_t i = 0; i < widths.size(); i++) tableWidth += widths[i];
  ImGui::PushID(grid ? "grid" : "totals");
  if (!ImGui::BeginTable("##perlevel", count, TABLE_FLAGS | ImGuiTableFlags_Sortable,
                         ImVec2(std::max(tableWidth, ImGui::GetContentRegionAvail().x), 0)))
  {
    ImGui::PopID();
    return;
  }
  ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, LEVEL_WIDTH);
  for (size_t i = 0; i < labels.size(); i++)
  {
    bool isBar = table.hasShare && i == labels.size() - 1;
    if (isBar) BarColumn(labels[i].c_str(), widths[i]);
    else NumberColumn(labels[i].c_str(), widths[i]);
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
    else ImGui::TableNextRow();
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
  bool visible = ImGui::BeginChild("##page", ImGui::GetContentRegionAvail(), ImGuiChildFlags_AlwaysUseWindowPadding,
                                   ImGuiWindowFlags_HorizontalScrollbar);
  ImGui::PopStyleVar();
  if (visible)
  {
    if (state->perLevel) DrawPerLevel(state, stats, columns);
    else DrawTopicPage(state, CachedPage(*state, stats, columns), columns);
  }
  ImGui::EndChild();
}

void HeroStatsDraw_Frame(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                         const HeroStatsPicker &picker, void (*exportStatistics)(void))
{
  DrawTitle(*state, columns, exportStatistics);
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 6));
  float whoWidth = DrawWhoRow(state, columns);
  float pickerLeft = ImGui::GetWindowWidth() - GUTTER - PickerWidth(picker);
  if (pickerLeft > GUTTER + whoWidth + 16) ImGui::SetCursorPosX(pickerLeft);
  else
  {
    // Too narrow to sit beside the champions: a line of its own
    ImGui::NewLine();
    ImGui::SetCursorPosX(GUTTER);
  }
  DrawPicker(state, picker);
  DrawTabs(state);
  // Steps and items count before a champion joins: only a game with no level yet has nothing
  if (stats.Champions().empty() && stats.VisitedLevels().empty())
  {
    ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 8));
    ImGui::TextColored(Colour(DIM_RGB), "No statistics yet.");
    return;
  }
  DrawPage(state, stats, columns);
}
