#ifndef HEROSTATSDRAWCOLOURS_H
#define HEROSTATSDRAWCOLOURS_H

// The Statistics window's colours and the helpers that turn them into ImGui's, shared by the
// pages in HeroStatsDraw.cpp and the Profile in HeroStatsProfileDraw.cpp

#include <stddef.h>
#include "imgui.h"

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
static const ImU32 RATE_BAR_RGB = 0x7fa6e6;   // a rate's bar on Per level (the party's shape on the Profile)
static const ImU32 LOW_BAR_RGB = 0xc98a3c;    // and under half
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

static inline ImVec4 Colour(ImU32 rgb, float alpha = 1.0f)
{
  return ImVec4(((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f, (rgb & 0xff) / 255.0f, alpha);
}

static inline ImU32 Packed(ImU32 rgb, float alpha = 1.0f)
{
  return ImGui::GetColorU32(Colour(rgb, alpha));
}

static inline ImU32 ChampionRgb(size_t column)
{
  return CHAMPION_RGB[column % 4];
}

#endif
