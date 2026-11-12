// calc_dump: prints the tables of a graphics.dat, or writes them as the
// C++ source of the built-in copy (core/Builtin.cpp).
//   calc_dump graphics.dat            readable listing
//   calc_dump --emit function "label" graphics.dat     one C++ function on stdout
#include <cstdio>
#include <cstring>
#include "../io/GraphicsDat.h"

using namespace calc;

static void listing(const GameData &g)
{
  std::printf("monsters\n");
  for (auto &m : g.monsters)
    std::printf("%2d %-30s def %3d hp %3d att %3d poi %3d dex %3d atkT %3d mvT %3d pause %2d "
                "mis %d exp %2d fire %2d pois %2d wound %04x kind %d size %d%s%s%s\n",
                m.id, m.name.c_str(), m.defense, m.baseHealth, m.attack, m.poison, m.dexterity,
                m.attackTicks, m.moveTicks, m.attackPause, m.missileClass, m.expClass,
                m.fireResistance, m.poisonResistance, m.woundNibbles, m.attackKind, m.size,
                m.nonMaterial ? " nonmat" : "", m.invincible ? " invincible" : "",
                m.hitsAnyHero ? " any" : "");
  std::printf("weapons\n");
  for (auto &w : g.weapons) {
    std::printf("%2d %-30s wt %3d cls %3d str %3d dist %3d attr %04x set %2d:", w.id,
                w.name.c_str(), w.weight, w.cls, w.strength, w.distance, w.attribute, w.actionSet);
    for (auto &o : g.physicalActions(w))
      std::printf(" %s(%d)", g.actions[size_t(o.action)].name.c_str(), o.minMastery);
    std::printf("\n");
  }
  std::printf("armour\n");
  for (auto &c : g.armours)
    std::printf("%2d %-34s wt %3d def %3d pierce %02x slots %04x\n", c.id, c.name.c_str(),
                c.weight, c.defense, c.pierce, c.wearSlots);
  std::printf("actions\n");
  for (auto &a : g.actions)
    std::printf("%2d %-12s exp %3d skill %2d def %3d energy %3d hit %3d dmg %3d ticks %3d%s\n",
                a.id, a.name.c_str(), a.exp, a.skill, a.defenseBonus, a.energy, a.hitChance,
                a.damage, a.ticks, a.physical ? " physical" : "");
  std::printf("spells\n");
  for (auto &s : g.spells)
    std::printf("%2d %-24s %-14s difficulty %2d skill %2d kind %d missile %d\n", s.id,
                s.name.c_str(), s.incantation().c_str(), s.difficulty, s.skill, s.kind, s.missile);
  std::printf("food");
  for (auto &f : g.foods) std::printf(" %s=%d/%d", f.name.c_str(), f.food, f.water);
  std::printf("\nrune cost\n");
  for (int r = 0; r < 4; r++) {
    for (int k = 0; k < 6; k++) std::printf(" %3d", g.runeCost[r][k]);
    std::printf("\n");
  }
  std::printf("power factor");
  for (int p = 1; p <= 6; p++) std::printf(" %d", g.powerFactor[p]);
  std::printf("\nslot protection");
  for (int v : g.slotProtection) std::printf(" %d", v);
  std::printf("\nwound slots");
  for (int v : g.woundSlots) std::printf(" %02x", v);
  std::printf("\n");
}

static void emit(const GameData &g, const char *function, const char *label)
{
  std::printf("const GameData &%s()\n{\n"
              "  static const GameData data = [] {\n    GameData g;\n"
              "    g.source = \"%s\";\n", function, label);
  std::printf("    static const int mon[][19] = {\n");
  for (auto &m : g.monsters)
    std::printf("      {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d},\n",
                m.moveTicks, m.attackTicks, m.defense, m.baseHealth, m.attack, m.poison,
                m.dexterity, m.missileClass, m.expClass, m.fireResistance, m.poisonResistance,
                m.attackPause, m.woundNibbles, m.attackKind, m.size, m.nonMaterial, m.invincible,
                m.hitsAnyHero, 0);
  std::printf("    };\n    for (int i = 0; i < %zu; i++) {\n      MonsterType m;\n      const int *v = mon[i];\n"
              "      m.id = i; m.name = monsterName(i);\n"
              "      m.moveTicks = v[0]; m.attackTicks = v[1]; m.defense = v[2]; m.baseHealth = v[3];\n"
              "      m.attack = v[4]; m.poison = v[5]; m.dexterity = v[6]; m.missileClass = v[7];\n"
              "      m.expClass = v[8]; m.fireResistance = v[9]; m.poisonResistance = v[10];\n"
              "      m.attackPause = v[11]; m.woundNibbles = v[12]; m.attackKind = v[13]; m.size = v[14];\n"
              "      m.nonMaterial = v[15]; m.invincible = v[16]; m.hitsAnyHero = v[17];\n"
              "      g.monsters.push_back(m);\n    }\n", g.monsters.size());
  std::printf("    static const int wpn[][6] = {\n");
  for (auto &w : g.weapons)
    std::printf("      {%d, %d, %d, %d, %d, %d},\n", w.weight, w.cls, w.strength, w.distance,
                w.attribute, w.actionSet);
  std::printf("    };\n    for (int i = 0; i < %zu; i++) {\n      WeaponType w;\n      const int *v = wpn[i];\n"
              "      w.id = i; w.name = weaponName(i);\n"
              "      w.weight = v[0]; w.cls = v[1]; w.strength = v[2]; w.distance = v[3];\n"
              "      w.attribute = v[4]; w.actionSet = v[5];\n      g.weapons.push_back(w);\n    }\n",
              g.weapons.size());
  std::printf("    static const int arm[][4] = {\n");
  for (auto &c : g.armours)
    std::printf("      {%d, %d, %d, %d},\n", c.weight, c.defense, c.pierce, c.wearSlots);
  std::printf("    };\n    for (int i = 0; i < %zu; i++) {\n      ArmourType c;\n      const int *v = arm[i];\n"
              "      c.id = i; c.name = armourName(i);\n"
              "      c.weight = v[0]; c.defense = v[1]; c.pierce = v[2]; c.wearSlots = v[3];\n"
              "      g.armours.push_back(c);\n    }\n", g.armours.size());
  std::printf("    static const int act[][14] = {\n");
  for (size_t i = 0; i < g.actions.size(); i++) {
    auto &a = g.actions[i];
    auto &s = g.actionSets[i];
    std::printf("      {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d},\n", a.exp, a.skill,
                a.defenseBonus, a.energy, a.hitChance, a.damage, a.ticks, a.physical, s.action[0],
                s.action[1], s.action[2], s.minMastery[1], s.minMastery[2],
                (s.needsCharge[1] ? 1 : 0) | (s.needsCharge[2] ? 2 : 0));
  }
  std::printf("    };\n    for (int i = 0; i < %zu; i++) {\n      ActionType a;\n      const int *v = act[i];\n"
              "      a.id = i; a.name = actionName(i);\n"
              "      a.exp = v[0]; a.skill = v[1]; a.defenseBonus = v[2]; a.energy = v[3];\n"
              "      a.hitChance = v[4]; a.damage = v[5]; a.ticks = v[6]; a.physical = v[7];\n"
              "      g.actions.push_back(a);\n      ActionSet s;\n"
              "      s.action[0] = v[8]; s.action[1] = v[9]; s.action[2] = v[10];\n"
              "      s.minMastery[1] = v[11]; s.minMastery[2] = v[12];\n"
              "      s.needsCharge[1] = v[13] & 1; s.needsCharge[2] = v[13] & 2;\n"
              "      g.actionSets.push_back(s);\n    }\n", g.actions.size());
  std::printf("    static const struct { const char *name; int r[3], difficulty, skill, kind, missile; } spl[] = {\n");
  for (auto &s : g.spells)
    std::printf("      {\"%s\", {%d, %d, %d}, %d, %d, %d, %d},\n", s.name.c_str(), s.runes[0],
                s.runes[1], s.runes[2], s.difficulty, s.skill, s.kind, s.missile);
  std::printf("    };\n    for (int i = 0; i < %zu; i++) {\n      SpellType s;\n"
              "      s.id = i; s.name = spl[i].name;\n"
              "      for (int k = 0; k < 3; k++) s.runes[k] = spl[i].r[k];\n"
              "      s.difficulty = spl[i].difficulty; s.skill = spl[i].skill;\n"
              "      s.kind = spl[i].kind; s.missile = spl[i].missile;\n"
              "      g.spells.push_back(s);\n    }\n", g.spells.size());
  for (auto &f : g.foods)
    std::printf("    g.foods.push_back({\"%s\", %d, %d});\n", f.name.c_str(), f.food, f.water);
  std::printf("    static const int runes[4][6] = {\n");
  for (int r = 0; r < 4; r++)
    std::printf("      {%d, %d, %d, %d, %d, %d},\n", g.runeCost[r][0], g.runeCost[r][1],
                g.runeCost[r][2], g.runeCost[r][3], g.runeCost[r][4], g.runeCost[r][5]);
  std::printf("    };\n    for (int r = 0; r < 4; r++)\n      for (int k = 0; k < 6; k++) g.runeCost[r][k] = runes[r][k];\n");
  std::printf("    static const int power[7] = {0, %d, %d, %d, %d, %d, %d};\n", g.powerFactor[1],
              g.powerFactor[2], g.powerFactor[3], g.powerFactor[4], g.powerFactor[5], g.powerFactor[6]);
  std::printf("    for (int p = 0; p < 7; p++) g.powerFactor[p] = power[p];\n");
  std::printf("    static const int slots[6] = {%d, %d, %d, %d, %d, %d};\n", g.slotProtection[0],
              g.slotProtection[1], g.slotProtection[2], g.slotProtection[3], g.slotProtection[4],
              g.slotProtection[5]);
  std::printf("    for (int i = 0; i < 6; i++) g.slotProtection[i] = slots[i];\n");
  std::printf("    static const int wounds[4] = {%d, %d, %d, %d};\n", g.woundSlots[0], g.woundSlots[1],
              g.woundSlots[2], g.woundSlots[3]);
  std::printf("    for (int i = 0; i < 4; i++) g.woundSlots[i] = wounds[i];\n");
  std::printf("    return g;\n  }();\n  return data;\n}\n\n");
}

int main(int argc, char **argv)
{
  bool asSource = argc > 1 && !std::strcmp(argv[1], "--emit");
  if (argc != (asSource ? 5 : 2)) {
    std::fprintf(stderr, "usage: calc_dump graphics.dat\n"
                         "       calc_dump --emit function \"label\" graphics.dat\n");
    return 2;
  }
  try {
    GameData g = loadGraphicsDat(argv[argc - 1]);
    if (asSource) emit(g, argv[2], argv[3]);
    else listing(g);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
  return 0;
}
