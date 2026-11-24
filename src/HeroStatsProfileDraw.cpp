// HeroStatsProfileDraw.cpp : the Statistics window's Profile tab, drawn with Dear ImGui from
// HeroStatsProfile. Nothing here counts anything.
#include "stdafx.h"

#include <cfloat>
#include <cmath>
#include <string>
#include <vector>
#include "imgui.h"
#include "HeroStatsDrawColours.h"
#include "HeroStatsProfile.h"
#include "HeroStatsProfileDraw.h"

namespace
{

const ImU32 PARTY_SHAPE_RGB = 0x7fa6e6;  // the party's shape, as in the mockup
const ImU32 LOW_RGB = 0xe0a35c;          // a share under half: the text
const ImU32 LOW_BAR_RGB = 0xc98a3c;      // and the bar
const ImU32 GRID_RGB = 0x3a3d46;
const ImU32 EVEN_RGB = 0x9aa0ad;
const float CARD_GAP = 14;
const float CARD_PADDING = 14;
const float NARROWEST_CARD = 420;
const float STAR_BOX = 300;   // the height a star takes in its card
const float PI = 3.14159265f;

ImU32 ShapeRgb(const std::vector<HeroStatsColumn> &columns, HeroStatsChampion who)
{
  if (who == HERO_STATS_PARTY) return PARTY_SHAPE_RGB;
  size_t c = 0;
  while (c < columns.size() && columns[c].id != who) c++;
  return ChampionRgb(c);
}

std::string NameOf(const std::vector<HeroStatsColumn> &columns, HeroStatsChampion who)
{
  if (who == HERO_STATS_PARTY) return "Party";
  for (size_t c = 0; c < columns.size(); c++)
    if (columns[c].id == who) return columns[c].name;
  return "?";
}

// How far out a value lies, 0 at the centre and 1 at the edge; an undefined value at the centre
float Reach(const HeroStatsStar &star, double value)
{
  if (value < 0 || star.outer <= star.centre) return 0;
  double reach = (value - star.centre) / (star.outer - star.centre);
  return (float)(reach < 0 ? 0 : reach > 1 ? 1 : reach);
}

ImVec2 PointAt(ImVec2 centre, float radius, int axis, int axisCount, float reach)
{
  float angle = -PI / 2 + 2 * PI * axis / axisCount;
  return ImVec2(centre.x + radius * reach * cosf(angle), centre.y + radius * reach * sinf(angle));
}

void Polygon(ImDrawList *drawList, ImVec2 centre, float radius, int axisCount, float reach, ImU32 colour, float thickness)
{
  std::vector<ImVec2> points;
  for (int a = 0; a < axisCount; a++) points.push_back(PointAt(centre, radius, a, axisCount, reach));
  drawList->AddPolyline(points.data(), (int)points.size(), colour, ImDrawFlags_Closed, thickness);
}

void DashedPolygon(ImDrawList *drawList, ImVec2 centre, float radius, int axisCount, float reach, ImU32 colour)
{
  for (int a = 0; a < axisCount; a++)
  {
    ImVec2 from = PointAt(centre, radius, a, axisCount, reach);
    ImVec2 to = PointAt(centre, radius, (a + 1) % axisCount, axisCount, reach);
    float length = sqrtf((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y));
    for (float at = 0; at < length; at += 8)
    {
      float end = at + 4 < length ? at + 4 : length;
      drawList->AddLine(ImVec2(from.x + (to.x - from.x) * at / length, from.y + (to.y - from.y) * at / length),
                        ImVec2(from.x + (to.x - from.x) * end / length, from.y + (to.y - from.y) * end / length), colour, 1.2f);
    }
  }
}

// A shape filled: a fan from the centre, so a star that is not convex fills right
void FillShape(ImDrawList *drawList, ImVec2 centre, const std::vector<ImVec2> &points, ImU32 colour)
{
  for (size_t i = 0; i < points.size(); i++)
    drawList->AddTriangleFilled(centre, points[i], points[(i + 1) % points.size()], colour);
}

float DistanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b)
{
  float dx = b.x - a.x, dy = b.y - a.y;
  float lengthSquared = dx * dx + dy * dy;
  float t = lengthSquared > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  float x = a.x + t * dx - p.x, y = a.y + t * dy - p.y;
  return sqrtf(x * x + y * y);
}

void AxisTooltip(const HeroStatsStar &star, int axis, const std::vector<HeroStatsColumn> &columns)
{
  if (!ImGui::BeginTooltip()) return;
  ImGui::TextColored(Colour(WHITE_RGB), "%s", star.axes[axis].c_str());
  // The bright shapes first: they are what the star is about
  for (int pass = 0; pass < 2; pass++)
    for (size_t s = star.shapes.size(); s-- > 0;)
    {
      const HeroStatsStarShape &shape = star.shapes[s];
      if (shape.faint != (pass == 1) || axis >= (int)shape.texts.size()) continue;
      ImVec2 at = ImGui::GetCursorScreenPos();
      float size = ImGui::GetFontSize();
      ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(at.x + size * 0.4f, at.y + size * 0.55f), size * 0.3f,
                                                  Packed(ShapeRgb(columns, shape.who)));
      ImGui::Dummy(ImVec2(size, size));
      ImGui::SameLine();
      ImGui::TextColored(Colour(shape.faint ? DIM_RGB : TEXT_RGB), "%-8s %5s   %s", NameOf(columns, shape.who).c_str(),
                         shape.texts[axis].c_str(), shape.details[axis].c_str());
    }
  ImGui::EndTooltip();
}

// The star in a box of width x STAR_BOX at origin
void DrawStar(const HeroStatsStar &star, ImVec2 origin, float width, const std::vector<HeroStatsColumn> &columns)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  int axisCount = (int)star.axes.size();
  if (axisCount < 3) return;
  float fontSize = ImGui::GetFontSize();
  float radius = std::fmin(width * 0.28f, STAR_BOX * 0.5f - fontSize * 3);
  ImVec2 centre(origin.x + width / 2, origin.y + STAR_BOX / 2);
  for (size_t r = 0; r < star.rings.size(); r++)
    Polygon(drawList, centre, radius, axisCount, Reach(star, star.rings[r].first), Packed(GRID_RGB), 1);
  Polygon(drawList, centre, radius, axisCount, 1, Packed(GRID_RGB), 1);
  if (star.evenRing > 0) DashedPolygon(drawList, centre, radius, axisCount, Reach(star, star.evenRing), Packed(EVEN_RGB));
  for (int a = 0; a < axisCount; a++)
    drawList->AddLine(centre, PointAt(centre, radius, a, axisCount, 1), Packed(GRID_RGB));
  // The rings' labels up the first spoke, the edge's at its end
  for (size_t r = 0; r < star.rings.size(); r++)
  {
    if (star.rings[r].second.empty()) continue;
    ImVec2 at = PointAt(centre, radius, 0, axisCount, Reach(star, star.rings[r].first));
    drawList->AddText(ImVec2(at.x + 4, at.y), Packed(DIM_RGB), star.rings[r].second.c_str());
  }
  if (!star.edgeLabel.empty())
  {
    ImVec2 at = PointAt(centre, radius, 0, axisCount, 1);
    drawList->AddText(ImVec2(at.x + 4, at.y), Packed(DIM_RGB), star.edgeLabel.c_str());
  }
  int bright = 0;
  for (size_t s = 0; s < star.shapes.size(); s++) bright += star.shapes[s].faint ? 0 : 1;
  for (size_t s = 0; s < star.shapes.size(); s++)
  {
    const HeroStatsStarShape &shape = star.shapes[s];
    ImU32 rgb = ShapeRgb(columns, shape.who);
    std::vector<ImVec2> points;
    for (int a = 0; a < axisCount && a < (int)shape.values.size(); a++)
      points.push_back(PointAt(centre, radius, a, axisCount, Reach(star, shape.values[a])));
    if ((int)points.size() != axisCount) continue;
    FillShape(drawList, centre, points, Packed(rgb, shape.faint ? 0.06f : bright == 1 ? 0.22f : 0.10f));
    drawList->AddPolyline(points.data(), axisCount, Packed(rgb, shape.faint ? 0.45f : 1.0f), ImDrawFlags_Closed, shape.faint ? 1.0f : 2.0f);
    if (shape.faint) continue;
    for (int a = 0; a < axisCount; a++) drawList->AddCircleFilled(points[a], 3, Packed(rgb));
  }
  // The axes' names, and each one's value when one shape is bright
  const HeroStatsStarShape *single = NULL;
  for (size_t s = 0; s < star.shapes.size(); s++)
    if (!star.shapes[s].faint && bright == 1) single = &star.shapes[s];
  ImVec2 mouse = ImGui::GetMousePos();
  int hoveredAxis = -1;
  for (int a = 0; a < axisCount; a++)
  {
    ImVec2 at = PointAt(centre, radius, a, axisCount, 1.0f + (fontSize * 1.2f) / radius);
    ImVec2 size = ImGui::CalcTextSize(star.axes[a].c_str());
    float dx = at.x - centre.x;
    float x = fabsf(dx) < 8 ? at.x - size.x / 2 : dx > 0 ? at.x : at.x - size.x;
    float y = at.y - size.y / 2;
    bool twoLines = single != NULL && a < (int)single->texts.size();
    if (fabsf(dx) < 8 && at.y < centre.y && twoLines) y -= size.y;
    drawList->AddText(ImVec2(x, y), Packed(TEXT_RGB), star.axes[a].c_str());
    float bottom = y + size.y;
    float right = x + size.x;
    if (twoLines)
    {
      const std::string &text = single->texts[a];
      ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
      float tx = fabsf(dx) < 8 ? at.x - textSize.x / 2 : dx > 0 ? at.x : at.x - textSize.x;
      drawList->AddText(ImVec2(tx, bottom), Packed(WHITE_RGB), text.c_str());
      bottom += textSize.y;
    }
    bool overLabel = mouse.x >= x - 2 && mouse.x <= right + 2 && mouse.y >= y - 2 && mouse.y <= bottom + 2;
    bool overSpoke = DistanceToSegment(mouse, centre, PointAt(centre, radius, a, axisCount, 1)) < 6;
    if (overLabel || overSpoke) hoveredAxis = a;
  }
  if (hoveredAxis >= 0 && ImGui::IsWindowHovered() && !star.shapes.empty()) AxisTooltip(star, hoveredAxis, columns);
}

void Legend(ImVec2 origin, float width, const HeroStatsStar &star, const std::vector<HeroStatsColumn> &columns)
{
  if (star.shapes.size() < 2) return;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  float fontSize = ImGui::GetFontSize();
  float total = 0;
  for (size_t s = 0; s < star.shapes.size(); s++)
    total += fontSize * 1.2f + ImGui::CalcTextSize(NameOf(columns, star.shapes[s].who).c_str()).x + fontSize;
  float x = origin.x + (width - total) / 2;
  // In party order, not drawing order
  for (size_t c = 0; c <= columns.size(); c++)
    for (size_t s = 0; s < star.shapes.size(); s++)
    {
      HeroStatsChampion who = star.shapes[s].who;
      bool match = c < columns.size() ? who == columns[c].id : who == HERO_STATS_PARTY;
      if (!match) continue;
      std::string name = NameOf(columns, who);
      drawList->AddCircleFilled(ImVec2(x + fontSize * 0.4f, origin.y + fontSize * 0.55f), fontSize * 0.3f,
                                Packed(ShapeRgb(columns, who), star.shapes[s].faint ? 0.45f : 1.0f));
      x += fontSize * 1.2f;
      drawList->AddText(ImVec2(x, origin.y), Packed(star.shapes[s].faint ? DIM_RGB : TEXT_RGB), name.c_str());
      x += ImGui::CalcTextSize(name.c_str()).x + fontSize;
    }
}

// A card's title and caption; returns where its body starts
float CardHeading(ImVec2 origin, float width, const std::string &title, const std::string &caption)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImFont *font = ImGui::GetFont();
  float fontSize = ImGui::GetFontSize();
  float y = origin.y + CARD_PADDING;
  drawList->AddText(ImVec2(origin.x + CARD_PADDING, y), Packed(WHITE_RGB), title.c_str());
  y += fontSize + 6;
  float wrap = width - 2 * CARD_PADDING;
  ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, wrap, caption.c_str());
  drawList->AddText(font, fontSize, ImVec2(origin.x + CARD_PADDING, y), Packed(DIM_RGB), caption.c_str(), NULL, wrap);
  return y + size.y + 6 - origin.y;
}

float StarCardHeight(void)
{
  float fontSize = ImGui::GetFontSize();
  return CARD_PADDING * 2 + fontSize * 4 + 12 + STAR_BOX + fontSize * 1.6f;
}

void StarCard(const HeroStatsStar &star, ImVec2 origin, float width, float height, const std::vector<HeroStatsColumn> &columns)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), Packed(PANEL_RGB), 6);
  float body = CardHeading(origin, width, star.title, star.caption);
  float top = std::fmax(body, ImGui::GetFontSize() * 4 + CARD_PADDING + 12);
  DrawStar(star, ImVec2(origin.x, origin.y + top), width, columns);
  Legend(ImVec2(origin.x, origin.y + top + STAR_BOX + 4), width, star, columns);
}

float BarsCardHeight(const HeroStatsProfile &profile)
{
  float row = ImGui::GetFontSize() + 8;
  return std::fmax(StarCardHeight(), CARD_PADDING * 2 + ImGui::GetFontSize() * 4 + 12 + row * profile.levelBars.size());
}

void BarsCard(const HeroStatsProfile &profile, HeroStatsDrawState *state, ImVec2 origin, float width, float height)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), Packed(PANEL_RGB), 6);
  float top = CardHeading(origin, width, "Explored per level", "Floor cells explored, of those the party can reach. Under half in orange; click a level to pick it.");
  float fontSize = ImGui::GetFontSize();
  float row = fontSize + 8;
  float labelWidth = ImGui::CalcTextSize("Level 00").x + 10;
  float valueWidth = ImGui::CalcTextSize("100%").x + 6;
  float barLeft = origin.x + CARD_PADDING + labelWidth;
  float barWidth = width - 2 * CARD_PADDING - labelWidth - valueWidth - 10;
  for (size_t i = 0; i < profile.levelBars.size(); i++)
  {
    const HeroStatsLevelBar &bar = profile.levelBars[i];
    float y = origin.y + top + row * i;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + CARD_PADDING / 2, y - 3));
    ImGui::PushID((int)i);
    bool clicked = ImGui::InvisibleButton("##level", ImVec2(width - CARD_PADDING, row));
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    if (clicked)
    {
      state->perLevel = false;
      state->level = bar.level;
    }
    bool picked = !state->perLevel && state->level == bar.level;
    if (picked || hovered)
      drawList->AddRectFilled(ImVec2(origin.x + CARD_PADDING / 2, y - 3), ImVec2(origin.x + width - CARD_PADDING / 2, y + row - 3),
                              Packed(picked ? PICKED_GROUND_RGB : PANEL_HOVERED_RGB), 3);
    std::string label = "Level " + std::to_string(bar.level);
    drawList->AddText(ImVec2(origin.x + CARD_PADDING, y), Packed(TEXT_RGB), label.c_str());
    float barY = y + fontSize / 2 - 5;
    drawList->AddRectFilled(ImVec2(barLeft, barY), ImVec2(barLeft + barWidth, barY + 10), Packed(TRACK_RGB), 3);
    std::string text = "-";
    bool low = false;
    if (bar.explored >= 0)
    {
      low = bar.explored < 0.5;
      drawList->AddRectFilled(ImVec2(barLeft, barY), ImVec2(barLeft + barWidth * (float)bar.explored, barY + 10),
                              Packed(low ? LOW_BAR_RGB : PARTY_SHAPE_RGB), 3);
      text = std::to_string((int)floor(bar.explored * 100 + 0.5)) + "%";
    }
    ImVec2 size = ImGui::CalcTextSize(text.c_str());
    drawList->AddText(ImVec2(origin.x + width - CARD_PADDING - size.x, y), Packed(low ? LOW_RGB : WHITE_RGB), text.c_str());
  }
}

// Per level: a row per level, a column per completion axis, each cell shaded by its value
void CompletionTable(const HeroStatsProfile &profile, HeroStatsDrawState *state)
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  float fontSize = ImGui::GetFontSize();
  static const char *const AXES[HERO_COMPLETION_AXIS_COUNT] = {"Explored", "Items", "Monsters", "Doors", "Switches"};
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 8));
  ImGui::TextColored(Colour(WHITE_RGB), "Completion, level by level");
  ImGui::SetCursorPosX(GUTTER);
  ImGui::TextColored(Colour(DIM_RGB), "Each cell: that level's share of what it holds. Darker and orange: under half. Click a level to pick it.");
  ImGui::Dummy(ImVec2(0, 4));
  ImVec2 origin = ImGui::GetCursorScreenPos();
  origin.x = ImGui::GetWindowPos().x + GUTTER;
  float first = ImGui::CalcTextSize("Whole run").x + 24;
  float cell = ImGui::CalcTextSize("Monsters").x + 28;
  float row = fontSize + 10;
  for (int axis = 0; axis < HERO_COMPLETION_AXIS_COUNT; axis++)
  {
    ImVec2 size = ImGui::CalcTextSize(AXES[axis]);
    drawList->AddText(ImVec2(origin.x + first + cell * (axis + 1) - size.x - 10, origin.y), Packed(DIM_RGB), AXES[axis]);
  }
  for (size_t r = 0; r < profile.perLevel.size(); r++)
  {
    const HeroStatsCompletionRow &line = profile.perLevel[r];
    bool wholeRun = line.level == HERO_STATS_WHOLE_RUN;
    float y = origin.y + row * (r + 1) + (wholeRun ? 4 : 0);
    if (wholeRun)
    {
      drawList->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + first + cell * HERO_COMPLETION_AXIS_COUNT, y + row), Packed(PANEL_RGB));
      drawList->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + first + cell * HERO_COMPLETION_AXIS_COUNT, y), Packed(LINE_RGB));
    }
    std::string label = wholeRun ? "Whole run" : "Level " + std::to_string(line.level);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
    ImGui::PushID((int)r);
    if (ImGui::InvisibleButton("##level", ImVec2(first, row)))
    {
      state->perLevel = false;
      state->level = line.level;
    }
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    drawList->AddText(ImVec2(origin.x + 6, y + 5), Packed(hovered || wholeRun ? WHITE_RGB : TEXT_RGB), label.c_str());
    for (int axis = 0; axis < HERO_COMPLETION_AXIS_COUNT; axis++)
    {
      double value = line.values[axis];
      float x = origin.x + first + cell * axis;
      std::string text = "-";
      bool low = false;
      if (value >= 0)
      {
        // From the track's colour to the party's blue
        float t = (float)value;
        ImVec4 from = Colour(TRACK_RGB), to = Colour(0x3f63a8);
        ImVec4 shade(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t, from.z + (to.z - from.z) * t, 1);
        if (!wholeRun) drawList->AddRectFilled(ImVec2(x + 2, y + 1), ImVec2(x + cell - 2, y + row - 1), ImGui::GetColorU32(shade), 2);
        low = value < 0.5;
        text = std::to_string((int)floor(value * 100 + 0.5)) + "%";
      }
      ImVec2 size = ImGui::CalcTextSize(text.c_str());
      drawList->AddText(ImVec2(x + cell - size.x - 10, y + 5), Packed(low ? LOW_RGB : WHITE_RGB), text.c_str());
    }
  }
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(first + cell * HERO_COMPLETION_AXIS_COUNT, row * (profile.perLevel.size() + 1) + 12));
}

}  // namespace

void HeroStatsProfileDraw_Page(HeroStatsDrawState *state, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns)
{
  // The profile scans the whole store several times: made again twice a second at most
  static HeroStatsProfile profile;
  static double madeAt = -1;
  static std::string madeFor;
  int level = state->perLevel ? HERO_STATS_WHOLE_RUN : state->level;
  std::string key = std::to_string(state->who) + "|" + std::to_string(level);
  double now = ImGui::GetTime();
  if (madeFor != key || madeAt < 0 || now - madeAt > 0.5)
  {
    profile = HeroStatsProfile_Make(stats, state->who, level);
    madeAt = now;
    madeFor = key;
  }
  if (state->perLevel)
  {
    CompletionTable(profile, state);
    return;
  }
  if (state->who != HERO_STATS_PARTY)
  {
    profile.completion.title = "How much of the dungeon the party found";
  }
  float available = ImGui::GetWindowWidth() - 2 * GUTTER;
  int across = (int)((available + CARD_GAP) / (NARROWEST_CARD + CARD_GAP));
  across = across < 1 ? 1 : across > 3 ? 3 : across;
  float width = (available - CARD_GAP * (across - 1)) / across;
  ImGui::SetCursorPos(ImVec2(GUTTER, ImGui::GetCursorPosY() + 10));
  ImVec2 origin = ImGui::GetCursorScreenPos();
  // The cards in their order; Roles only with two champions or more
  std::vector<const HeroStatsStar *> stars;
  stars.push_back(&profile.completion);
  stars.push_back(&profile.played);
  stars.push_back(&profile.trained);
  stars.push_back(&profile.mana);
  if (profile.showRoles) stars.push_back(&profile.roles);
  size_t cardCount = stars.size() + 1;  // and the bars
  float y = origin.y;
  for (size_t first = 0; first < cardCount; first += across)
  {
    float rowHeight = StarCardHeight();
    for (size_t c = first; c < first + across && c < cardCount; c++)
      if (c == stars.size()) rowHeight = std::fmax(rowHeight, BarsCardHeight(profile));
    for (size_t c = first; c < first + across && c < cardCount; c++)
    {
      ImVec2 at(origin.x + (width + CARD_GAP) * (c - first), y);
      if (c < stars.size()) StarCard(*stars[c], at, width, rowHeight, columns);
      else BarsCard(profile, state, at, width, rowHeight);
    }
    y += rowHeight + CARD_GAP;
  }
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(available, y - origin.y));
}
