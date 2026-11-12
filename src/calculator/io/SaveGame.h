// Reads a Dungeon Master / Chaos Strikes Back savegame: classic saves of the
// Atari ST and Amiga (big-endian), saves of CSBwin and of this port (with
// their " Extended Features " prefix), and Amiga save disks (.adf).
// Layout after SaveGame.cpp, Unscramble() (CSBCode.cpp) and CSB.h.
#pragma once
#include <string>
#include <vector>
#include "../core/Model.h"
#include "Bytes.h"

namespace calc {

enum ItemKind { ItemNone, ItemWeapon, ItemArmour, ItemMisc, ItemPotion, ItemOther };

struct SavedItem {
  int kind = ItemNone;
  int type = -1;                // weapon / armour / misc / potion type
  std::string name;
};

struct SavedHero {
  Hero hero;                    // ready for the formulas
  std::string title;
  SavedItem items[SlotCount];
  long experience[SkillCount] = {};
};

struct SavedGroup {
  int type = 0;
  int count = 1;
  int hp[4] = {0, 0, 0, 0};
  int x = 0, y = 0;
};

struct SavedLevel {
  int expMultiplier = 0;
  int width = 0, height = 0;
  int offsetX = 0, offsetY = 0;   // the level's place in the dungeon's global coordinates
  std::vector<SavedGroup> groups;
};

struct SaveGame {
  std::string label;            // file name, for display
  std::string format;           // what kind of save it is
  unsigned time = 0;            // the game clock
  unsigned lastMoveTime = 0;
  int level = 0, x = 0, y = 0, facing = 0;
  Party party;
  std::vector<SavedHero> heroes;
  std::vector<SavedLevel> levels;
  std::vector<std::string> warnings;   // what could not be read
};

// Throws FormatError when the bytes are no savegame.  The tables give the
// items their weights and names.
SaveGame readSaveGame(const Bytes &data, const GameData &tables, const std::string &label);
// A savegame file or an .adf holding one
SaveGame loadSaveGame(const std::string &path, const GameData &tables);

// DetermineMastery (Code17818.cpp:15) from the experience of a skill and of
// its basic skill, without the bonus of items
int masteryFromExperience(int skill, const long experience[SkillCount]);

}  // namespace calc
