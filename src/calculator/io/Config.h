// The rules a game is played by, from the csb.cfg next to a savegame.
#pragma once
#include <string>
#include "../core/Model.h"

namespace calc {

// Gives false when there is no such file; rules it does not name stay as they are
bool readRulesFromConfig(const std::string &path, Rules &rules);

}  // namespace calc
