#ifndef HEROSTATSEXPORT_H
#define HEROSTATSEXPORT_H

#include <string>
#include <vector>
#include "HeroStatsPages.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// The statistics as one HTML file to share: every page of the window for the whole run and for
// each visited level, with the window's look. It needs nothing beside it: the numbers are in the
// file as JSON and a script in the file draws the pages. No engine, no ImGui.

struct HeroStatsExportInfo
{
  std::string dungeon;  // the dungeon's file name, shown under the title
  std::string date;     // when it was written, as the player reads it
};

// A JSON string, quotes included. Bytes that are not UTF-8 are taken as Latin-1, and "<" is
// escaped so that the text cannot end the script it sits in.
std::string HeroStatsExport_JsonString(const std::string &text);
// One record as the JSON has it: a value per number, null where it does not apply to who on level
std::string HeroStatsExport_RecordJson(const HeroStatsRecord &record, HeroStatsChampion who, int level);
// The JSON object the page script reads (see HeroStatsExport.cpp for its fields)
std::string HeroStatsExport_Json(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                 const HeroStatsNames &names, const HeroStatsExportInfo &info);
std::string HeroStatsExport_Html(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                 const HeroStatsNames &names, const HeroStatsExportInfo &info);

#pragma pack(pop)

#endif
