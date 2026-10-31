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
// The page

static void DrawPage(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns)
{
  (void)stats;
  (void)columns;
  ImGui::SetCursorPosX(GUTTER);
  ImGui::TextColored(Colour(DIM_RGB), "%s", HeroStatsCatalogue_Topic(state->topic).label);
}

void HeroStatsDraw_Forget(void)
{
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
