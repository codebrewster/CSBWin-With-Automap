// Reads the engine's tables out of a graphics.dat (items 0x22f, 0x230 and
// 0x232, the ones ReadTablesFromGraphicsFile loads), so a custom dungeon's
// monsters and weapons are calculated with its own numbers.
#pragma once
#include <string>
#include "../core/GameData.h"
#include "Bytes.h"

namespace calc {

// One item of a graphics.dat, expanded (LZWExpand, Graphics.cpp)
Bytes graphicsItem(const Bytes &file, int item);

// Throws FormatError when the file is no graphics.dat with these tables
GameData readGraphicsDat(const Bytes &file, const std::string &label);
GameData loadGraphicsDat(const std::string &path);

}  // namespace calc
