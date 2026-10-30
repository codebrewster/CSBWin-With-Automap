#ifndef HEROSTATS_H
#define HEROSTATS_H

#include <stddef.h>
#include <stdint.h>
#include <map>
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
const int HERO_STATS_FISTS = -1;                   // weapon of an empty hand
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
  int weapon;                  // basic object type, HERO_STATS_FISTS or HERO_STATS_NO_WEAPON
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
// The rune key of an incantation (CHARDESC::incantation: four bytes, no terminator when all four
// are entered): incantation[1] | incantation[2] << 8 | incantation[3] << 16, the power rune
// left out and nothing after the first 0 taken as a rune
uint32_t HeroStats_RuneSpellKey(const void *incantation);
// The display name of a weapon key: the basic object type's name from the game's item names
// (d.ObjectNames, nameCount entries), "FISTS" for an empty hand, "" for spells entered as runes,
// "#N" for a type the table does not name
std::string HeroStats_WeaponName(int weapon, const char *const *itemNames, int nameCount);

struct HeroStatsChampionInfo { HeroStatsChampion id; std::string name; };
struct HeroStatsAttackRow { int weapon; bool thrown; int option; int tries, hits, damage, kills; };
struct HeroStatsMonsterRow { int monsterType, variant; HeroBlowBy by; HeroStatsChampion champion; int count; };
struct HeroStatsHarmRow { HeroHarmSource source; HeroStatsChampion causer; int monsterType; int amount; };
struct HeroStatsHealRow { HeroHealKind kind; HeroStatsChampion brewer; int amount; };
// Cleared runes count their mana as wasted; the shield and heal actions count theirs as mana
struct HeroStatsSpellRow { uint32_t spell; int casts, mana, failed, wasted, damage, kills; };

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
  int ManaSpent(HeroStatsChampion who, int level) const;                   // casts + special rows, wasted included
  int ManaWasted(HeroStatsChampion who, int level) const;                  // failed casts + runes cleared
  std::vector<HeroStatsSpellRow> Spells(HeroStatsChampion who, int level) const;  // most mana first

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
  // out-of-range byte.
  void Serialize(std::vector<uint8_t> *out) const;
  bool Deserialize(const uint8_t *data, size_t size);

private:
  struct AttackCounts { int tries, hits, damage, kills; };
  // (champion, level, weapon, thrown, option)
  typedef std::tuple<HeroStatsChampion, int, int, bool, int> AttackKey;
  // (champion, level, kind)
  typedef std::tuple<HeroStatsChampion, int, int> DamageKey;
  // (level, monster type, variant, by, champion)
  typedef std::tuple<int, int, int, int, HeroStatsChampion> MonsterKey;

  struct SpellCounts { int casts, mana, failed, wasted, damage, kills; };
  // (victim, level, source, causer, monster type)
  typedef std::tuple<HeroStatsChampion, int, int, HeroStatsChampion, int> HarmKey;
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
  int m_partyWipes = 0;
  int m_reloads = 0;
};

#pragma pack(pop)

#endif
