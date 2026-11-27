// The rules a game is played by, from the csb.cfg next to a savegame.
#pragma once
#include <string>
#include "../core/Model.h"

namespace calc {

// Gives false when there is no such file (rules then stay as they are).  A rule the file does
// not name gets the game's default, as when the game reads the file.
bool readRulesFromConfig(const std::string &path, Rules &rules);

}  // namespace calc
