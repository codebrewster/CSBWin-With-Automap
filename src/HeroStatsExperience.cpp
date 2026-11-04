// HeroStatsExperience.cpp : the experience the champions gained, with its source, and their ranks
#include "HeroStats.h"
#include "HeroStatsExperience.h"
#include "HeroStatsBytes.h"

HeroXpSource HeroXpSource_Other(void)
{
  HeroXpSource source = {HeroXpOther, HERO_STATS_NO_WEAPON, HERO_STATS_NO_OPTION, HERO_STATS_NO_SPELL, -1};
  return source;
}

bool operator<(const HeroXpSource &a, const HeroXpSource &b)
{
  return std::make_tuple((int)a.kind, a.weapon, a.option, a.spell, a.monsterType) <
         std::make_tuple((int)b.kind, b.weapon, b.option, b.spell, b.monsterType);
}

int HeroXp_ClassOf(int skill)
{
  if (skill < HERO_XP_CLASS_COUNT) return skill;
  return (skill - HERO_XP_CLASS_COUNT) / 4;
}

const char *HeroXp_ClassName(int cls)
{
  static const char *const NAMES[HERO_XP_CLASS_COUNT] = {"Fighter", "Ninja", "Priest", "Wizard"};
  return cls >= 0 && cls < HERO_XP_CLASS_COUNT ? NAMES[cls] : "";
}

const char *HeroXp_SkillName(int skill)
{
  static const char *const HIDDEN[HERO_XP_SKILL_COUNT - HERO_XP_CLASS_COUNT] = {
      "Swing", "Thrust", "Club", "Parry", "Steal", "Fight", "Throw", "Shoot",
      "Identify", "Heal", "Influence", "Defend", "Fire", "Air", "Earth", "Water"};
  if (skill < 0 || skill >= HERO_XP_SKILL_COUNT) return "";
  if (skill < HERO_XP_CLASS_COUNT) return HeroXp_ClassName(skill);
  return HIDDEN[skill - HERO_XP_CLASS_COUNT];
}

std::string HeroXp_RankName(int rank)
{
  static const char *const NAMES[] = {"Neophyte", "Novice", "Apprentice", "Journeyman", "Craftsman", "Artisan",
                                      "Adept", "Expert", "Lo Master", "Um Master", "On Master", "Ee Master",
                                      "Pal Master", "Mon Master", "Archmaster"};
  if (rank < 2) return "";
  if (rank > 16) rank = 16;
  return NAMES[rank - 2];
}

void HeroStatsExperience::Forget(void)
{
  m_gained.clear();
  m_rankUps.clear();
  m_startRanks.clear();
}

void HeroStatsExperience::Credit(HeroStatsChampion who, int level, int skill, const HeroXpSource &source, int amount)
{
  if (amount <= 0 || skill < 0 || skill >= HERO_XP_SKILL_COUNT) return;
  m_gained[Key(who, level, skill, source)] += amount;
}

void HeroStatsExperience::CountRankUp(const HeroRankUp &up)
{
  m_rankUps.push_back(up);
}

void HeroStatsExperience::SetStartRank(HeroStatsChampion who, int cls, int rank)
{
  m_startRanks.insert(std::make_pair(std::make_pair(who, cls), rank));
}

int HeroStatsExperience::StartRank(HeroStatsChampion who, int cls) const
{
  std::map<std::pair<HeroStatsChampion, int>, int>::const_iterator found = m_startRanks.find(std::make_pair(who, cls));
  return found == m_startRanks.end() ? -1 : found->second;
}

int HeroStatsExperience::Gained(HeroStatsChampion who, int level, int cls) const
{
  int total = 0;
  for (std::map<Key, int>::const_iterator it = m_gained.begin(); it != m_gained.end(); ++it)
    if (HeroStats::Matches(who, std::get<0>(it->first)) && HeroStats::LevelMatches(level, std::get<1>(it->first)) &&
        (cls < 0 || HeroXp_ClassOf(std::get<2>(it->first)) == cls))
      total += it->second;
  return total;
}

int HeroStatsExperience::SkillGained(HeroStatsChampion who, int level, int skill) const
{
  int total = 0;
  for (std::map<Key, int>::const_iterator it = m_gained.begin(); it != m_gained.end(); ++it)
    if (HeroStats::Matches(who, std::get<0>(it->first)) && HeroStats::LevelMatches(level, std::get<1>(it->first)) &&
        std::get<2>(it->first) == skill)
      total += it->second;
  return total;
}

std::vector<HeroXpRow> HeroStatsExperience::Rows(HeroStatsChampion who, int level) const
{
  std::map<std::pair<int, HeroXpSource>, int> merged;
  for (std::map<Key, int>::const_iterator it = m_gained.begin(); it != m_gained.end(); ++it)
    if (HeroStats::Matches(who, std::get<0>(it->first)) && HeroStats::LevelMatches(level, std::get<1>(it->first)))
      merged[std::make_pair(std::get<2>(it->first), std::get<3>(it->first))] += it->second;
  std::vector<HeroXpRow> rows;
  for (std::map<std::pair<int, HeroXpSource>, int>::const_iterator it = merged.begin(); it != merged.end(); ++it)
  {
    HeroXpRow row = {it->first.first, it->first.second, it->second};
    rows.push_back(row);
  }
  return rows;
}

std::vector<HeroRankUp> HeroStatsExperience::RankUps(HeroStatsChampion who) const
{
  std::vector<HeroRankUp> ups;
  for (size_t i = m_rankUps.size(); i-- > 0;)
    if (HeroStats::Matches(who, m_rankUps[i].who)) ups.push_back(m_rankUps[i]);
  return ups;
}

int HeroStatsExperience::RanksGained(HeroStatsChampion who, int level, int cls) const
{
  int count = 0;
  for (size_t i = 0; i < m_rankUps.size(); i++)
    if (HeroStats::Matches(who, m_rankUps[i].who) && HeroStats::LevelMatches(level, m_rankUps[i].level) &&
        (cls < 0 || m_rankUps[i].cls == cls))
      count++;
  return count;
}

static const size_t SOURCE_BYTES = 1 + 4 + 4 + 4 + 4;

static void PutSource(std::vector<uint8_t> *out, const HeroXpSource &source)
{
  HeroBytes_Put(out, (uint32_t)source.kind, 1);
  HeroBytes_PutInt(out, source.weapon);
  HeroBytes_PutInt(out, source.option);
  HeroBytes_Put(out, source.spell, 4);
  HeroBytes_PutInt(out, source.monsterType);
}

static bool GetSource(HeroBytesReader &in, HeroXpSource *source)
{
  uint32_t kind = in.Get(1);
  source->weapon = in.GetInt();
  source->option = in.GetInt();
  source->spell = in.Get(4);
  source->monsterType = in.GetInt();
  if (kind >= (uint32_t)HERO_XP_SOURCE_COUNT) in.ok = false;
  source->kind = (HeroXpSourceKind)kind;
  return in.ok;
}

void HeroStatsExperience::Serialize(std::vector<uint8_t> *out) const
{
  HeroBytes_Put(out, (uint32_t)m_gained.size(), 4);
  for (std::map<Key, int>::const_iterator it = m_gained.begin(); it != m_gained.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_Put(out, (uint32_t)std::get<2>(it->first), 1);
    PutSource(out, std::get<3>(it->first));
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_rankUps.size(), 4);
  for (size_t i = 0; i < m_rankUps.size(); i++)
  {
    const HeroRankUp &up = m_rankUps[i];
    HeroBytes_Put(out, up.who, 2);
    HeroBytes_PutInt(out, up.level);
    HeroBytes_Put(out, (uint32_t)up.cls, 1);
    HeroBytes_Put(out, (uint32_t)up.rank, 1);
    HeroBytes_Put(out, up.time, 4);
    PutSource(out, up.source);
  }
  HeroBytes_Put(out, (uint32_t)m_startRanks.size(), 4);
  for (std::map<std::pair<HeroStatsChampion, int>, int>::const_iterator it = m_startRanks.begin(); it != m_startRanks.end(); ++it)
  {
    HeroBytes_Put(out, it->first.first, 2);
    HeroBytes_Put(out, (uint32_t)it->first.second, 1);
    HeroBytes_Put(out, (uint32_t)it->second, 1);
  }
}

size_t HeroStatsExperience::SerializedSize(void) const
{
  std::vector<uint8_t> bytes;
  Serialize(&bytes);
  return bytes.size();
}

bool HeroStatsExperience::Deserialize(HeroBytesReader &in)
{
  HeroStatsExperience read;
  uint32_t count = in.GetCount(2 + 4 + 1 + SOURCE_BYTES + 4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int skill = (int)in.Get(1);
    HeroXpSource source;
    if (!GetSource(in, &source)) return false;
    int amount = in.GetInt();
    if (skill >= HERO_XP_SKILL_COUNT || amount < 0) return false;
    read.m_gained[Key(who, level, skill, source)] = amount;
  }
  count = in.GetCount(2 + 4 + 1 + 1 + 4 + SOURCE_BYTES);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroRankUp up;
    up.who = (HeroStatsChampion)in.Get(2);
    up.level = in.GetInt();
    up.cls = (int)in.Get(1);
    up.rank = (int)in.Get(1);
    up.time = in.Get(4);
    if (!GetSource(in, &up.source) || up.cls >= HERO_XP_CLASS_COUNT) return false;
    read.m_rankUps.push_back(up);
  }
  count = in.GetCount(2 + 1 + 1);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int cls = (int)in.Get(1);
    int rank = (int)in.Get(1);
    if (cls >= HERO_XP_CLASS_COUNT) return false;
    read.m_startRanks[std::make_pair(who, cls)] = rank;
  }
  if (!in.ok) return false;
  *this = read;
  return true;
}
