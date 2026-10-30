#ifndef HEROSTATS_H
#define HEROSTATS_H

#include <stdint.h>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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
  int weapon;                  // item name index, HERO_STATS_FISTS or HERO_STATS_NO_WEAPON
  bool thrown;                 // the weapon itself was thrown by hand
  int option;                  // attack option index (d.AttackNames), or the two constants
  uint32_t spell;              // rune key, or HERO_STATS_NO_SPELL
  int monsterType;             // HeroBlowByMonster, else -1
};
HeroStatsBlow HeroStatsBlow_Nobody(HeroBlowBy by);      // all fields "none"

struct HeroStatsChampionInfo { HeroStatsChampion id; std::string name; };
struct HeroStatsAttackRow { int weapon; bool thrown; int option; int tries, hits, damage, kills; };
struct HeroStatsMonsterRow { int monsterType, variant; HeroBlowBy by; HeroStatsChampion champion; int count; };

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

  int DamageDealt(HeroStatsChampion who, int level, HeroDamageKind kind) const;
  int Kills(HeroStatsChampion who, int level) const;
  std::vector<HeroStatsAttackRow> Attacks(HeroStatsChampion who, int level) const;  // weapon then option order
  std::vector<HeroStatsMonsterRow> MonsterKills(int level) const;

private:
  struct AttackCounts { int tries, hits, damage, kills; };
  // (champion, level, weapon, thrown, option)
  typedef std::tuple<HeroStatsChampion, int, int, bool, int> AttackKey;
  // (champion, level, kind)
  typedef std::tuple<HeroStatsChampion, int, int> DamageKey;
  // (level, monster type, variant, by, champion)
  typedef std::tuple<int, int, int, int, HeroStatsChampion> MonsterKey;

  static bool Matches(HeroStatsChampion want, HeroStatsChampion have);
  static bool LevelMatches(int want, int have);
  AttackCounts &AttackRow(int level, const HeroStatsBlow &blow);

  std::vector<HeroStatsChampionInfo> m_champions;
  std::vector<int> m_levels;
  std::map<AttackKey, AttackCounts> m_attacks;
  std::map<DamageKey, int> m_damage;
  std::map<std::pair<HeroStatsChampion, int>, int> m_kills;
  std::map<MonsterKey, int> m_monsterKills;
};

#endif
