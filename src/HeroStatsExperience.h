#ifndef HEROSTATSEXPERIENCE_H
#define HEROSTATSEXPERIENCE_H

#include <map>
#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

typedef uint16_t HeroStatsChampion;  // as in HeroStats.h
struct HeroBytesReader;

// The experience the champions gained, as the game added it (after the level's multiplier, the
// combat bonus and the dungeon's filter), with where it came from, and the ranks it brought.
// No engine.
//
// Skills are the game's 20: 0-3 the classes Fighter, Ninja, Priest, Wizard; 4-19 the hidden
// skills, four per class in that order. A hidden skill's gain is kept once, under the skill; the
// class's experience is the sum of its hidden skills and of gains straight to the class.

enum HeroXpSourceKind { HeroXpAttack, HeroXpOtherAction, HeroXpSpell, HeroXpMonster, HeroXpOther, HERO_XP_SOURCE_COUNT };
// weapon, option: an attack's or action's weapon and attack option, as the Weapons table keeps
// them; spell: the rune key; monsterType: the monster that attacked. What a kind does not use is
// left as HeroXpSource_Other() has it.
struct HeroXpSource { HeroXpSourceKind kind; int weapon, option; uint32_t spell; int monsterType; };
HeroXpSource HeroXpSource_Other(void);
bool operator<(const HeroXpSource &a, const HeroXpSource &b);

const int HERO_XP_SKILL_COUNT = 20;
const int HERO_XP_CLASS_COUNT = 4;
int HeroXp_ClassOf(int skill);            // 0-3: itself; 4-19: (skill - 4) / 4
const char *HeroXp_ClassName(int cls);    // "Fighter" ...
const char *HeroXp_SkillName(int skill);  // "Swing" ... "Water"; 0-3: the class's name
// The game shows a class's mastery m as d.Pointer16770[m] (Character.cpp, Code1f9e6.cpp), m capped
// at 16: 1 shows no rank (""), 2 Neophyte ... 9 Expert, 10-15 the masters (the game writes their
// rune with a glyph: Lo, Um, On, Ee, Pal, Mon), 16 Archmaster
std::string HeroXp_RankName(int rank);

struct HeroXpRow { int skill; HeroXpSource source; int amount; };
// A class reached a new rank: the source is the gain that did it
struct HeroRankUp { HeroStatsChampion who; int level; int cls, rank; uint32_t time; HeroXpSource source; };

class HeroStatsExperience
{
public:
  void Forget(void);
  void Credit(HeroStatsChampion who, int level, int skill, const HeroXpSource &source, int amount);
  void CountRankUp(const HeroRankUp &up);
  // The rank a class had when the statistics first knew the champion; kept once
  void SetStartRank(HeroStatsChampion who, int cls, int rank);
  int StartRank(HeroStatsChampion who, int cls) const;  // -1: unknown
  // who: a champion or HERO_STATS_PARTY; level: one or HERO_STATS_WHOLE_RUN; cls -1: every class
  int Gained(HeroStatsChampion who, int level, int cls) const;
  int SkillGained(HeroStatsChampion who, int level, int skill) const;
  // Every skill and source with experience, merged over who and level, in skill then source order
  std::vector<HeroXpRow> Rows(HeroStatsChampion who, int level) const;
  std::vector<HeroRankUp> RankUps(HeroStatsChampion who) const;  // newest first
  int RanksGained(HeroStatsChampion who, int level, int cls) const;
  // For the .map file (HeroStats::FIRST_LAYOUT_WITH_EXPERIENCE): appended to HeroStats' records
  void Serialize(std::vector<uint8_t> *out) const;
  bool Deserialize(HeroBytesReader &in);
  size_t SerializedSize(void) const;

private:
  // (champion, level, skill, source)
  typedef std::tuple<HeroStatsChampion, int, int, HeroXpSource> Key;
  std::map<Key, int> m_gained;
  std::vector<HeroRankUp> m_rankUps;  // in the order they happened
  std::map<std::pair<HeroStatsChampion, int>, int> m_startRanks;
};

#pragma pack(pop)

#endif
