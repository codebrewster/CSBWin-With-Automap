#include "GameData.h"

namespace calc {

namespace {

template <size_t N>
const char *pick(const char *const (&table)[N], int id)
{
  return id >= 0 && id < int(N) ? table[id] : "?";
}

// Dungeon Master's name first; Chaos Strikes Back's where it differs
const char *const kMonsters[] = {
  "Giant Scorpion", "Swamp Slime / Slime Devil", "Giggler", "Wizard Eye / Flying Eye",
  "Pain Rat / Hellhound", "Ruster", "Screamer", "Rockpile", "Ghost / Rive", "Stone Golem",
  "Mummy", "Black Flame", "Skeleton", "Couatl", "Vexirk", "Magenta Worm / Worm",
  "Trolin / Ant Man", "Giant Wasp / Muncher", "Animated Armour / Deth Knight",
  "Materializer / Zytaz", "Water Elemental", "Oitu", "Demon", "Lord Chaos", "Red Dragon",
  "Lord Order", "Grey Lord"};

const char *const kWeapons[] = {
  "Eye of Time", "Storm Ring", "Torch", "Flamitt", "Staff of Claws", "Storm / Bolt Blade",
  "Ra Blade / Fury", "The Firestaff", "Dagger", "Falchion", "Sword", "Rapier",
  "Biter / Sabre", "Samurai Sword", "Side Splitter / Delta", "Diamond Edge", "Vorpal Blade",
  "Dragon Fang / The Inquisitor", "Axe", "Executioner / Hardcleave", "Mace", "Mace of Order",
  "Morningstar", "Club", "Stone Club", "Claw Bow / Bow", "Crossbow", "Arrow", "Slayer",
  "Sling", "Rock", "Poison Dart", "Throwing Star", "Stick", "Staff", "Wand", "Teowand",
  "Yew Staff", "Staff of Irra / Staff of Manar", "Cross of Neta / Snake Staff",
  "Serpent Staff / The Conduit", "Dragon Spit", "Sceptre of Lyf", "Horn of Fear", "Speedbow",
  "The Firestaff (complete)"};

const char *const kArmours[] = {
  "Cape", "Cloak of Night", "Tattered Pants / Barbarian Hide", "Sandals", "Leather Boots",
  "Tattered Shirt / Robe (body)", "Robe (legs)", "Fine Robe (body)", "Fine Robe (legs)",
  "Kirtle", "Silk Shirt", "Tabard", "Gunna", "Elven Doublet", "Elven Huke", "Elven Boots",
  "Leather Jerkin", "Leather Pants", "Suede Boots", "Blue Pants", "Tunic", "Ghi",
  "Ghi Trousers", "Calista", "Crown of Nerra", "Bezerker Helm", "Helmet", "Basinet",
  "Buckler / Neta Shield", "Hide Shield / Crystal Shield", "Wooden Shield", "Small Shield",
  "Mail Aketon", "Leg Mail", "Mithral Aketon", "Mithral Mail", "Casque 'n Coif", "Hosen",
  "Armet", "Torso Plate", "Leg Plate", "Foot Plate", "Large Shield / Sar Shield",
  "Helm of Lyte / Helm of Ra", "Plate of Lyte / Plate of Ra", "Poleyn of Lyte / Poleyn of Ra",
  "Greave of Lyte / Greave of Ra", "Shield of Lyte / Shield of Ra", "Helm of Darc / Dragon Helm",
  "Plate of Darc / Dragon Plate", "Poleyn of Darc / Dragon Poleyn",
  "Greave of Darc / Dragon Greave", "Shield of Darc / Dragon Shield", "Dexhelm", "Flamebain",
  "Powertowers", "Boots of Speed", "Halter"};

const char *const kMisc[] = {
  "Compass", "Waterskin", "Jewel Symal", "Illumulet", "Ashes", "Bones", "Copper Coin / Sar Coin",
  "Silver Coin", "Gold Coin / Gor Coin", "Iron Key", "Key of B", "Solid Key", "Square Key",
  "Tourquoise Key", "Cross Key", "Onyx Key", "Skeleton Key", "Gold Key", "Winged Key",
  "Topaz Key", "Sapphire Key", "Emerald Key", "Ruby Key", "Ra Key", "Master Key", "Boulder",
  "Blue Gem", "Orange Gem", "Green Gem", "Apple", "Corn", "Bread", "Cheese", "Screamer Slice",
  "Worm Round", "Drumstick / Shank", "Dragon Steak", "Gem of Ages", "Ekkhard Cross", "Moonstone",
  "The Hellion", "Pendant Feral", "Magical Box (blue)", "Magical Box (green)", "Mirror of Dawn",
  "Rope", "Rabbit's Foot", "Corbamite / Corbum", "Choker", "Lock Picks", "Magnifier",
  "Zokathra Spell", "Bones"};

const char *const kPotions[] = {
  "Mon Potion", "Um Potion", "Des Potion", "Ven Potion", "Sar Potion", "Zo Potion", "Ros Potion",
  "Ku Potion", "Dane Potion", "Neta Potion", "Anti Venin", "Mon Potion", "Ya Potion", "Ee Potion",
  "Vi Potion", "Water Flask", "Kath Bomb", "Pew Bomb", "Ra Bomb", "Ful Bomb", "Empty Flask"};

// enum ATTACKTYPE (CSB.h)
const char *const kActions[] = {
  "Juggle", "Block", "Chop", "Speed", "Blow Horn", "Flip", "Punch", "Kick", "War Cry", "Stab",
  "Climb Down", "Freeze Life", "Hit", "Swing", "Stab", "Thrust", "Jab", "Parry", "Hack",
  "Berzerk", "Fireball", "Dispell", "Confuse", "Lightning", "Disrupt", "Melee", "Pray", "Invoke",
  "Slash", "Cleave", "Bash", "Stun", "Shoot", "Spellshield", "Fireshield", "Fluxcage", "Heal",
  "Calm", "Light", "Window", "Spit", "Brandish", "Throw", "Fuse"};

const char *const kSkills[] = {
  "Fighter", "Ninja", "Priest", "Wizard", "Swing", "Thrust", "Club", "Parry", "Steal", "Fight",
  "Throw", "Shoot", "Identify", "Heal", "Influence", "Defend", "Fire", "Air", "Earth", "Water"};

const char *const kAttributes[] = {"Luck", "Strength", "Dexterity", "Wisdom", "Vitality",
                                   "Anti-Magic", "Anti-Fire"};

const char *const kRunes[4][6] = {
  {"LO", "UM", "ON", "EE", "PAL", "MON"},
  {"YA", "VI", "OH", "FUL", "DES", "ZO"},
  {"VEN", "EW", "KATH", "IR", "BRO", "GOR"},
  {"KU", "ROS", "DAIN", "NETA", "RA", "SAR"}};

}  // namespace

const char *monsterName(int id) { return pick(kMonsters, id); }
const char *weaponName(int id) { return id < 0 ? "Bare hands" : pick(kWeapons, id); }
const char *armourName(int id) { return pick(kArmours, id); }
const char *miscName(int id) { return pick(kMisc, id); }
const char *potionName(int id) { return pick(kPotions, id); }
const char *actionName(int id) { return pick(kActions, id); }
const char *skillName(int skill) { return pick(kSkills, skill); }
const char *attributeName(int attribute) { return pick(kAttributes, attribute); }

const char *slotName(int slot)
{
  static const char *const names[] = {
    "Ready hand", "Action hand", "Head", "Torso", "Legs", "Feet", "Pouch", "Quiver",
    "Quiver", "Quiver", "Neck", "Pouch", "Quiver"};
  return slot >= 0 && slot < 13 ? names[slot] : "Backpack";
}

const char *runeName(int row, int index)
{
  if (row < 0 || row > 3 || index < 1 || index > 6) return "";
  return kRunes[row][index - 1];
}

const char *powerName(int power) { return runeName(0, power); }

std::string SpellType::incantation() const
{
  std::string s;
  for (int r = 0; r < 3; r++) {
    if (runes[r] == 0) break;
    if (!s.empty()) s += ' ';
    s += runeName(r + 1, runes[r]);
  }
  return s;
}

WeaponType GameData::bareHands() const
{
  WeaponType w;
  w.id = -1;
  w.name = weaponName(-1);
  w.actionSet = 2;   // PrepareAttack: no weapon wielded
  return w;
}

const WeaponType &GameData::weapon(int id) const
{
  static const WeaponType none = [] {
    WeaponType w;
    w.name = weaponName(-1);
    return w;
  }();
  return id >= 0 && id < int(weapons.size()) ? weapons[size_t(id)] : none;
}

std::vector<GameData::Offer> GameData::physicalActions(const WeaponType &w) const
{
  std::vector<Offer> out;
  if (w.actionSet <= 0 || w.actionSet >= int(actionSets.size())) return out;
  const ActionSet &set = actionSets[size_t(w.actionSet)];
  for (int i = 0; i < 3; i++) {
    int a = set.action[i];
    if (a < 0 || a >= int(actions.size()) || !actions[size_t(a)].physical) continue;
    out.push_back({a, set.minMastery[i]});
  }
  return out;
}

}  // namespace calc
