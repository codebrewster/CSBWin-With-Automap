#ifndef HEROSTATS_H
#define HEROSTATS_H

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

typedef uint16_t HeroStatsChampion;              // CHARDESC::fingerPrint
const HeroStatsChampion HERO_STATS_NOBODY = 0xffff;
const HeroStatsChampion HERO_STATS_PARTY = 0xfffe; // "all champions" in queries
const int HERO_STATS_WHOLE_RUN = -1;               // "all levels" in queries
const int HERO_STATS_UNARMED = -1;                 // weapon of an empty hand
const int HERO_STATS_NO_WEAPON = -2;               // spells entered as runes
const int HERO_STATS_THROW_OPTION = -1;            // option of an item thrown by hand
const int HERO_STATS_NO_OPTION = -2;
const uint32_t HERO_STATS_NO_SPELL = 0;

enum HeroDamageKind { HeroDamageMelee, HeroDamageRanged, HeroDamageMagic, HERO_DAMAGE_KIND_COUNT };
enum HeroBlowBy { HeroBlowByChampion, HeroBlowByMonster, HeroBlowByTrap, HeroBlowByDoor,
                  HeroBlowByPit, HeroBlowByCloud, HeroBlowByOther };

// Who or what dealt a blow, and with what. Missiles and clouds carry one (HeroStatsOrigins).
struct HeroStatsBlow
{
  HeroBlowBy by;
  HeroStatsChampion champion;  // HeroBlowByChampion, else HERO_STATS_NOBODY
  HeroDamageKind kind;
  int weapon;                  // basic object type, HERO_STATS_UNARMED or HERO_STATS_NO_WEAPON
  bool thrown;                 // the weapon itself was thrown by hand
  int option;                  // attack option index (d.AttackNames), or the two constants
  uint32_t spell;              // rune key, or HERO_STATS_NO_SPELL
  int monsterType;             // HeroBlowByMonster, else -1
};
HeroStatsBlow HeroStatsBlow_Nobody(HeroBlowBy by);      // all fields "none"

enum HeroHarmSource { HeroHarmMonster, HeroHarmMonsterMissile, HeroHarmTrap, HeroHarmFriendlyFire,
                      HeroHarmPoison, HeroHarmPoisonCloud, HeroHarmPit, HeroHarmWall, HeroHarmDoor,
                      HeroHarmStarvation, HeroHarmExhaustion, HeroHarmPotion, HeroHarmOther,
                      HERO_HARM_SOURCE_COUNT };
enum HeroHealKind { HeroHealRegeneration, HeroHealPotion, HeroHealAction, HERO_HEAL_KIND_COUNT };
enum HeroManaGain { HeroManaRegeneration, HeroManaPotion, HERO_MANA_GAIN_COUNT };

// Spell rows that are not rune spells
const uint32_t HERO_SPELL_RUNES_CLEARED = 0x01000000;
const uint32_t HERO_SPELL_SHIELD_ACTION = 0x02000000;
const uint32_t HERO_SPELL_HEAL_ACTION   = 0x03000000;
const uint32_t HERO_SPELL_ITEM_SPELLS   = 0x04000000;  // spells cast from staffs and wands
const uint32_t HERO_SPELL_NO_SUCH_SPELL = 0x80000000;  // ORed onto runes that form no spell
// A monster's missile: ORed onto the low byte of the missile's RN (0xff80 fireball ... 0xff87
// poison cloud), kept in HeroStatsBlow::spell of HeroBlowByMonster
const uint32_t HERO_SPELL_MONSTER_MISSILE = 0x05000000;
// The rune key of an incantation (CHARDESC::incantation: four bytes, no terminator when all four
// are entered): incantation[1] | incantation[2] << 8 | incantation[3] << 16, the power rune
// left out and nothing after the first 0 taken as a rune
uint32_t HeroStats_RuneSpellKey(const void *incantation);
// The display name of a weapon key: the basic object type's name from the game's item names
// (d.ObjectNames, nameCount entries), "UNARMED" for an empty hand, "" for spells entered as runes,
// "#N" for a type the table does not name
std::string HeroStats_WeaponName(int weapon, const char *const *itemNames, int nameCount);

struct HeroStatsChampionInfo { HeroStatsChampion id; std::string name; };
// uses: actions that cannot hit (War Cry, Calm, Spellshield ...), counted apart from the tries
struct HeroStatsAttackRow { int weapon; bool thrown; int option; int tries, hits, damage, kills, uses; };
struct HeroStatsMonsterRow { int monsterType, variant; HeroBlowBy by; HeroStatsChampion champion; int count; };
// spell: the missile kind of HeroHarmMonsterMissile (HERO_SPELL_MONSTER_MISSILE | kind), else 0
struct HeroStatsHarmRow { HeroHarmSource source; HeroStatsChampion causer; int monsterType; int amount; uint32_t spell; };
struct HeroStatsHealRow { HeroHealKind kind; HeroStatsChampion brewer; int amount; };
// Cleared runes count their mana as wasted; the shield and heal actions count theirs as mana
struct HeroStatsSpellRow { uint32_t spell; int casts, mana, failed, wasted, damage, kills; };

// What the party did on its own feet: steps and turns the player made, teleporters passed,
// engine ticks spent on the level (and how many of them asleep), pits fallen into, pits climbed
// down with a rope, and bumps into walls and closed doors. tilesWalked: the different cells the
// party stood on; tilesExplored: the cells the map shows as explored that are no solid wall
// (from the automap, see SetExploredTiles).
struct HeroStatsMovement
{
  int steps, turns, teleports, ticks, sleepTicks, pitFalls, pitClimbs, bumps;
  int tilesWalked, tilesExplored;
};

// What the party did with items, by item (its basic object type, like a weapon key): items picked
// up the first time from the dungeon (the floor, alcoves, torch holders, chests), and items a
// wall took (keys in locks, coins and gems in slots)
enum HeroItemEvent { HeroItemPickedUp, HeroItemUsedInWall, HERO_ITEM_EVENT_COUNT };
struct HeroStatsItemRow { int item; int pickedUp, usedInWall; };
// What a champion ate or drank (potions, water, food), by item
enum HeroConsumeKind { HeroConsumeDrunk, HeroConsumeEaten, HERO_CONSUME_KIND_COUNT };
struct HeroStatsConsumedRow { int item; int drunk, eaten; };
// Food and water a champion took in: what the item or fountain added, and what was lost over the
// game's maximum (2048); drinks straight from a fountain
struct HeroStatsNourishment { int foodGained, foodWasted, waterGained, waterWasted, fountainDrinks; };
// How a door was broken down. HeroDoorUnknown: counted before the method was kept (version 17).
// The rest are no bashes, kept in the same table: a closed door a Zo spell opened, and doors the
// party opened or closed with their button (champion HERO_STATS_NOBODY).
enum HeroDoorMethod { HeroDoorSmashed, HeroDoorShot, HeroDoorBySpell, HeroDoorUnknown, HeroDoorOpenedByZo,
                      HeroDoorButtonOpened, HeroDoorButtonClosed, HERO_DOOR_METHOD_COUNT };
const int HERO_DOOR_ANY_METHOD = -1;

// A champion's death: where, from what, and the blow that did it (monster type, causing champion)
struct HeroStatsDeath { HeroStatsChampion who; int level; HeroHarmSource cause; HeroStatsBlow by; };

// What the champions did, kept per champion and dungeon level. Sums over champions or levels are
// made when asked; nothing is cached.
class HeroStats
{
public:
  void Forget(void);
  // Adds the champion, or renames one already met; order of first meeting is kept
  void MeetChampion(HeroStatsChampion id, const std::string &name);
  const std::vector<HeroStatsChampionInfo> &Champions(void) const;
  void MarkLevelVisited(int level);
  std::vector<int> VisitedLevels(void) const;         // ascending

  // Only blows by a champion change these
  void CountAttackTry(int level, const HeroStatsBlow &blow);
  void CountActionUse(int level, const HeroStatsBlow &blow);   // an action that cannot hit
  void CountAttackHit(int level, const HeroStatsBlow &blow);
  void CreditMonsterDamage(int level, const HeroStatsBlow &blow, int damage);
  // Every death; the champion's kills, attack row and spell row count a champion's blow
  void CountMonsterKill(int level, int monsterType, int variant, const HeroStatsBlow &blow);

  // blow names the causer: a champion (friendly fire, poison cloud by caster) or a monster type
  void CreditHarm(HeroStatsChampion victim, int level, HeroHarmSource source, const HeroStatsBlow &blow, int amount);
  void CreditHealing(HeroStatsChampion who, int level, HeroHealKind kind, HeroStatsChampion brewer, int amount);
  void CreditManaGained(HeroStatsChampion who, int level, HeroManaGain gain, int amount);
  void CountCast(HeroStatsChampion who, int level, uint32_t spell, int mana);
  void CountFailedCast(HeroStatsChampion who, int level, uint32_t spell, int wastedMana);
  void CreditManaSpent(HeroStatsChampion who, int level, uint32_t specialSpell, int mana);  // the 0x0n000000 rows

  int DamageDealt(HeroStatsChampion who, int level, HeroDamageKind kind) const;
  int Kills(HeroStatsChampion who, int level) const;
  std::vector<HeroStatsAttackRow> Attacks(HeroStatsChampion who, int level) const;  // weapon then option order
  std::vector<HeroStatsMonsterRow> MonsterKills(int level) const;

  int Harm(HeroStatsChampion who, int level) const;                       // total taken
  int FriendlyFireCaused(HeroStatsChampion who, int level) const;          // harm whose causer is who (PARTY: any champion)
  std::vector<HeroStatsHarmRow> HarmRows(HeroStatsChampion who, int level) const;  // biggest first, zero rows left out
  int Healed(HeroStatsChampion who, int level) const;
  int HealingGiven(HeroStatsChampion who, int level) const;                // potion healing with brewer == who
  std::vector<HeroStatsHealRow> HealRows(HeroStatsChampion who, int level) const;  // biggest first
  int ManaGained(HeroStatsChampion who, int level) const;
  int ManaGainedBy(HeroStatsChampion who, int level, HeroManaGain gain) const;  // one source of ManaGained
  int ManaSpent(HeroStatsChampion who, int level) const;                   // casts + special rows, wasted included
  int ManaWasted(HeroStatsChampion who, int level) const;                  // failed casts + runes cleared
  std::vector<HeroStatsSpellRow> Spells(HeroStatsChampion who, int level) const;  // most mana first

  // The party's movement, per level (not per champion)
  void CountStep(int level);
  void CountTurn(int level);
  void CountTeleport(int level);
  void CountTick(int level, bool asleep);
  void CountPitFall(int level);
  void CountPitClimb(int level);
  void CountBump(int level);
  // The party stands on the cell (any number of times: it counts once)
  void MarkTileWalked(int level, int x, int y);
  // The automap's count of explored tiles per level. Not stored: the automap keeps its cells in
  // the .map file itself, and the owner refreshes this before the numbers are shown.
  void SetExploredTiles(const std::map<int, int> &perLevel);
  HeroStatsMovement Movement(int level) const;

  // The party's items, per level (not per champion)
  void CountItem(int level, int item, HeroItemEvent event);
  std::vector<HeroStatsItemRow> Items(int level) const;   // item order, zero rows left out
  void CountConsumed(HeroStatsChampion who, int level, int item, HeroConsumeKind kind);
  std::vector<HeroStatsConsumedRow> Consumed(HeroStatsChampion who, int level) const;  // item order
  int Drunk(HeroStatsChampion who, int level) const;
  int Eaten(HeroStatsChampion who, int level) const;
  // value: what the item or fountain gives (0 for none, negative values count nothing), gained:
  // how much the champion's food or water actually rose
  void CreditFood(HeroStatsChampion who, int level, int value, int gained);
  void CreditWater(HeroStatsChampion who, int level, int value, int gained);
  void CountFountainDrink(HeroStatsChampion who, int level);
  HeroStatsNourishment Nourishment(HeroStatsChampion who, int level) const;
  // A champion's blow, missile or explosion broke a door down; chests opened the first time
  void CountDoorBashed(HeroStatsChampion who, int level, HeroDoorMethod method);
  // HERO_DOOR_ANY_METHOD: every door broken down, the ones opened by Zo left out
  int DoorsBashed(HeroStatsChampion who, int level, int method = HERO_DOOR_ANY_METHOD) const;
  void CountDoorOpenedByZo(HeroStatsChampion who, int level);
  int DoorsOpenedByZo(HeroStatsChampion who, int level) const;
  // The party clicked a door's button: opened sets the door opening, else closing
  void CountDoorButton(int level, bool opened);
  int DoorsOpenedByButton(int level) const;
  int DoorsClosedByButton(int level) const;
  void CountChestOpened(int level);
  int ChestsOpened(int level) const;
  // Scrolls read the first time; wall texts read, from the automap like the explored tiles (not
  // stored, see SetWallTextsRead)
  void CountScrollRead(int level);
  int ScrollsRead(int level) const;
  void SetWallTextsRead(const std::map<int, int> &perLevel);
  int WallTextsRead(int level) const;
  // Champion portraits in mirrors: seen on the wall straight ahead, and viewed (the mirror clicked
  // to look at the champion). From the automap like the explored tiles, not stored.
  void SetChampionsSeen(const std::map<int, int> &seen, const std::map<int, int> &viewed);
  int ChampionsSeen(int level) const;
  int ChampionsViewed(int level) const;

  // Hardship. Deaths are kept for the whole run, each with its level.
  void CountDeath(const HeroStatsDeath &death);
  void CountResurrection(HeroStatsChampion who, int level);
  void CountPartyWipe(void);
  void CountReload(void);
  std::vector<HeroStatsDeath> Deaths(HeroStatsChampion who) const;   // in the order they happened
  int Resurrections(HeroStatsChampion who, int level) const;
  int PartyWipes(void) const;
  int Reloads(void) const;
  // The hardship of other (the same game read from a file, or the one in memory):
  // deaths become whichever list is longer, wipes and reloads the larger count.
  void MergeHardship(const HeroStats &other);
  // Replaces everything but the hardship, which is merged
  void TakeFromFile(const HeroStats &fromFile);

  // For the .map file: appends every record to *out. Deserialize replaces everything (hardship
  // included) and returns false, leaving the store as it was, on any short, surplus or
  // out-of-range byte. layout is the .map format version the bytes were written in: 13 had no
  // missile kind in the harm rows and no movement, 14 only steps, turns, teleports and ticks and
  // no uses in the attack rows, 15 no items, 16 nothing eaten or drunk, no doors bashed and no
  // chests opened, 17 no door methods (the doors read as HeroDoorUnknown), no food and water,
  // 18 no tiles walked, 19 no scrolls read.
  static const int LAYOUT = 20;
  static const int FIRST_LAYOUT_WITH_MOVEMENT = 14;
  static const int FIRST_LAYOUT_WITH_PITS_AND_SLEEP = 15;
  static const int FIRST_LAYOUT_WITH_ITEMS = 16;
  static const int FIRST_LAYOUT_WITH_CONSUMPTION = 17;
  static const int FIRST_LAYOUT_WITH_NOURISHMENT = 18;
  static const int FIRST_LAYOUT_WITH_TILES = 19;
  static const int FIRST_LAYOUT_WITH_SCROLLS = 20;
  void Serialize(std::vector<uint8_t> *out) const;
  bool Deserialize(const uint8_t *data, size_t size, int layout = LAYOUT);

private:
  struct AttackCounts { int tries, hits, damage, kills, uses; };
  // (champion, level, weapon, thrown, option)
  typedef std::tuple<HeroStatsChampion, int, int, bool, int> AttackKey;
  // (champion, level, kind)
  typedef std::tuple<HeroStatsChampion, int, int> DamageKey;
  // (level, monster type, variant, by, champion)
  typedef std::tuple<int, int, int, int, HeroStatsChampion> MonsterKey;

  struct SpellCounts { int casts, mana, failed, wasted, damage, kills; };
  // (victim, level, source, causer, monster type, missile kind)
  typedef std::tuple<HeroStatsChampion, int, int, HeroStatsChampion, int, uint32_t> HarmKey;
  // (champion, level, kind, brewer)
  typedef std::tuple<HeroStatsChampion, int, int, HeroStatsChampion> HealKey;
  // (champion, level, gain)
  typedef std::tuple<HeroStatsChampion, int, int> GainKey;
  // (champion, level, spell)
  typedef std::tuple<HeroStatsChampion, int, uint32_t> SpellKey;

  static bool Matches(HeroStatsChampion want, HeroStatsChampion have);
  static bool LevelMatches(int want, int have);
  AttackCounts &AttackRow(int level, const HeroStatsBlow &blow);
  SpellCounts &SpellRow(HeroStatsChampion who, int level, uint32_t spell);

  std::vector<HeroStatsChampionInfo> m_champions;
  std::vector<int> m_levels;
  std::map<AttackKey, AttackCounts> m_attacks;
  std::map<DamageKey, int> m_damage;
  std::map<std::pair<HeroStatsChampion, int>, int> m_kills;
  std::map<MonsterKey, int> m_monsterKills;
  std::map<HarmKey, int> m_harm;
  std::map<HealKey, int> m_healing;
  std::map<GainKey, int> m_manaGained;
  std::map<SpellKey, SpellCounts> m_spells;
  std::vector<HeroStatsDeath> m_deaths;
  std::map<std::pair<HeroStatsChampion, int>, int> m_resurrections;
  std::map<int, HeroStatsMovement> m_movement;
  std::set<std::tuple<int, int, int> > m_tilesWalked;   // (level, x, y)
  std::map<int, int> m_tilesExplored;                   // per level, not stored
  struct ItemCounts { int pickedUp, usedInWall; };
  std::map<std::pair<int, int>, ItemCounts> m_items;   // (level, item)
  // (champion, level, item, kind)
  typedef std::tuple<HeroStatsChampion, int, int, int> ConsumeKey;
  std::map<ConsumeKey, int> m_consumed;
  // (champion, level, method)
  typedef std::tuple<HeroStatsChampion, int, int> DoorKey;
  std::map<DoorKey, int> m_doorsBashed;
  std::map<std::pair<HeroStatsChampion, int>, HeroStatsNourishment> m_nourishment;
  HeroStatsNourishment &NourishmentRow(HeroStatsChampion who, int level);
  std::map<int, int> m_chestsOpened;                   // per level
  std::map<int, int> m_scrollsRead;                    // per level
  std::map<int, int> m_wallTextsRead;                  // per level, not stored
  std::map<int, int> m_championsSeen, m_championsViewed;  // per level, not stored
  static int SumOverLevels(const std::map<int, int> &perLevel, int level);
  int ConsumedOfKind(HeroStatsChampion who, int level, int kind) const;
  int m_partyWipes = 0;
  int m_reloads = 0;
};

#pragma pack(pop)

#endif
