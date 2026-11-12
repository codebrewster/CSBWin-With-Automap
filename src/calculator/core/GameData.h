// The game's tables the formulas read: monsters, weapons, armour, actions,
// spells, food.  They come from a graphics.dat (io/GraphicsDat) or, when
// none is at hand, from the built-in copy of Dungeon Master's tables.
#pragma once
#include <array>
#include <string>
#include <vector>

namespace calc {

// CHARDESC::Attributes order
enum Attribute { Luck, Strength, Dexterity, Wisdom, Vitality, AntiMagic, AntiFire, AttributeCount };

// The four basic skills and their four sub-skills each (CHARDESC::skills92)
enum Skill {
  SkillFighter = 0, SkillNinja = 1, SkillPriest = 2, SkillWizard = 3,
  SkillSwing = 4, SkillThrust = 5, SkillClub = 6, SkillParry = 7,
  SkillSteal = 8, SkillFight = 9, SkillThrow = 10, SkillShoot = 11,
  SkillIdentify = 12, SkillHeal = 13, SkillInfluence = 14, SkillDefend = 15,
  SkillFire = 16, SkillAir = 17, SkillEarth = 18, SkillWater = 19,
  SkillCount = 20
};
const char *skillName(int skill);
const char *attributeName(int attribute);

// Possession slots of a hero (CHARDESC::possessions)
enum Slot {
  SlotReadyHand = 0, SlotActionHand = 1, SlotHead = 2, SlotTorso = 3,
  SlotLegs = 4, SlotFeet = 5, SlotNeck = 10, SlotCount = 30
};
const char *slotName(int slot);

// Wound bits (CHARDESC::ouches): bit n = slot n is wounded
enum Wound {
  WoundReadyHand = 1, WoundActionHand = 2, WoundHead = 4,
  WoundTorso = 8, WoundLegs = 16, WoundFeet = 32
};

struct MonsterType {            // MONSTERDESC (CSB.h)
  int id = 0;
  std::string name;
  int moveTicks = 1;            // movementTicks06
  int attackTicks = 1;          // attackTicks07
  int defense = 0;              // defense08
  int baseHealth = 0;           // baseHealth09
  int attack = 0;               // attack10
  int poison = 0;               // poisonAttack11
  int dexterity = 0;            // dexterity12
  int missileClass = 0;         // word14 bits 12-15; above 1 it launches missiles
  int expClass = 0;             // word16 bits 8-11
  int fireResistance = 0;       // word18 bits 4-7, 15 = immune
  int poisonResistance = 0;     // word18 bits 8-11, 15 = immune
  int attackPause = 0;          // word20 bits 0-3: ticks added after an attack
  int woundNibbles = 0;         // word22: where its blows land
  int attackKind = 0;           // uByte22[2]: the P4 of DamageCharacter
  int size = 0;                 // 0 = up to four in a square, 1 = two, 2 = one
  bool nonMaterial = false;
  bool invincible = false;
  bool hitsAnyHero = false;     // word2 bit 4: picks a random hero
};

struct WeaponType {             // WEAPONDESC
  int id = -1;                  // -1 = bare hands
  std::string name;
  int weight = 0;               // 0.1 kg
  int cls = 0;                  // uByte1: which skills add strength
  int strength = 0;             // damage
  int distance = 0;
  int attribute = 0;
  int actionSet = 2;            // row of actionSets (OBJDESC::attackClass); 2 = bare hands
};
enum { WeaponDiamondEdge = 15, WeaponVorpalBlade = 16, WeaponExecutioner = 19,
       WeaponFirestaffA = 7, WeaponFirestaffB = 45, WeaponSceptreOfLyf = 42 };

struct ArmourType {             // CLOTHINGDESC
  int id = 0;
  std::string name;
  int weight = 0;
  int defense = 0;
  int pierce = 0;               // bits 0-2 sharp resistance, bit 7 = a shield
  int wearSlots = 0;               // OBJDESC::word4: where it can be worn
  bool isShield() const { return (pierce & 0x80) != 0; }
};
enum { ArmourElvenBoots = 15 };

struct ActionType {             // the action tables of graphic 0x230
  int id = 0;
  std::string name;
  int exp = 0;                  // experienceForAttacking
  int skill = 0;                // SkillNumber
  int defenseBonus = 0;         // Byte20090: protection while the action lasts
  int energy = 0;               // Byte20046: stamina cost
  int hitChance = 0;            // hit_chance (P7)
  int damage = 0;               // critical_chance (P8)
  int ticks = 0;                // Byte19914: how long the hero is busy
  bool physical = false;        // goes through DeterminePhysicalAttackDamage
};
enum { ActionDisrupt = 24 };

struct ActionSet {              // LEGALATTACKS: the actions of one weapon class
  int action[3] = {255, 255, 255};
  int minMastery[3] = {0, 0, 0};
  bool needsCharge[3] = {false, false, false};
};

enum SpellKind { SpellPotion = 1, SpellMissile = 2, SpellOther = 3 };
enum Missile {                  // RN::GetSpellType
  MissileFireball = 0, MissilePoison = 1, MissileLightning = 2, MissileDispell = 3,
  MissileZo = 4, MissilePoisonBolt = 6, MissilePoisonCloud = 7
};

struct SpellType {              // SPELL
  int id = 0;
  std::string name;
  int runes[3] = {0, 0, 0};     // 1..6 within its row, 0 = none (after the power rune)
  int difficulty = 0;           // skillRequired
  int skill = 0;                // byte5
  int kind = 0;                 // word6 bits 0-3
  int missile = 0;              // word6 bits 4-9
  std::string incantation() const;
};

struct FoodType {
  std::string name;
  int food = 0;
  int water = 0;
};

struct GameData {
  std::string source;                 // where the tables came from, for display
  std::vector<MonsterType> monsters;  // 27
  std::vector<WeaponType> weapons;    // 46
  std::vector<ArmourType> armours;    // 58
  std::vector<ActionType> actions;    // 44
  std::vector<ActionSet> actionSets;  // 44
  std::vector<SpellType> spells;      // 25
  std::vector<FoodType> foods;        // what a hero can eat or drink
  int runeCost[4][6] = {};            // Byte19010: mana of a rune, by row
  int powerFactor[7] = {};            // Byte19016: by power 1..6, in eighths
  int slotProtection[6] = {5, 5, 4, 6, 3, 1};
  int woundSlots[4] = {};             // uByte590: wound mask by nibble count

  const WeaponType &weapon(int id) const;   // id -1 = bare hands
  WeaponType bareHands() const;
  // The physical actions a weapon offers, with the mastery each one needs
  struct Offer { int action; int minMastery; };
  std::vector<Offer> physicalActions(const WeaponType &w) const;
};

const char *runeName(int row, int index);   // row 0 = power, index 1..6
const char *powerName(int power);           // 1..6: LO .. MON

// The names the engine has no table for
const char *monsterName(int id);
const char *weaponName(int id);
const char *armourName(int id);
const char *miscName(int id);
const char *potionName(int id);
const char *actionName(int id);

// The tables of the two original games, compiled in (core/Builtin.cpp)
const GameData &builtinDungeonMaster();
const GameData &builtinChaosStrikesBack();

}  // namespace calc
