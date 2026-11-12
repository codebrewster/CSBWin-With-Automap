// calc_save: prints what the calculator reads out of a savegame.
//   calc_save SAVEGAME [graphics.dat]
#include <cstdio>
#include "../io/GraphicsDat.h"
#include "../io/SaveGame.h"

using namespace calc;

int main(int argc, char **argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: calc_save SAVEGAME [graphics.dat]\n");
    return 2;
  }
  try {
    GameData g = argc > 2 ? loadGraphicsDat(argv[2]) : builtinDungeonMaster();
    SaveGame s = loadSaveGame(argv[1], g);
    std::printf("%s: %s\ntime %u, level %d at (%d,%d) facing %d, shields party %d fire %d spell %d\n",
                s.label.c_str(), s.format.c_str(), s.time, s.level, s.x, s.y, s.facing,
                s.party.partyShield, s.party.fireShield, s.party.spellShield);
    for (auto &w : s.warnings) std::printf("warning: %s\n", w.c_str());
    for (auto &sh : s.heroes) {
      const Hero &h = sh.hero;
      std::printf("%s %s: hp %d/%d sta %d/%d mana %d/%d load %d wounds %d food %d water %d shield %d\n  attr",
                  h.name.c_str(), sh.title.c_str(), h.hp, h.maxHp, h.stamina, h.maxStamina, h.mana,
                  h.maxMana, h.load, h.wounds, h.food, h.water, h.shield);
      for (int k = 0; k < AttributeCount; k++) std::printf(" %d", h.attr[k]);
      std::printf("\n  mastery");
      for (int k = 0; k < SkillCount; k++) std::printf(" %d", h.mastery[k]);
      std::printf("\n  weapon %s, boots %d, cross %d\n  items", weaponName(h.weapon), h.elvenBoots,
                  h.ekkhardCross);
      for (int k = 0; k < SlotCount; k++)
        if (sh.items[k].kind != ItemNone) std::printf(" [%d %s]", k, sh.items[k].name.c_str());
      std::printf("\n");
    }
    for (size_t i = 0; i < s.levels.size(); i++) {
      const SavedLevel &l = s.levels[i];
      std::printf("level %zu: %dx%d multiplier %d, %zu groups:", i, l.width, l.height,
                  l.expMultiplier, l.groups.size());
      for (auto &grp : l.groups) {
        std::printf(" %s x%d (", monsterName(grp.type), grp.count);
        for (int k = 0; k < grp.count; k++) std::printf("%s%d", k ? "," : "", grp.hp[k]);
        std::printf(")");
      }
      std::printf("\n");
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
  return 0;
}
