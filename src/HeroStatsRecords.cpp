// HeroStatsRecords.cpp : the best moments of the run
#include "HeroStats.h"
#include "HeroStatsRecords.h"
#include "HeroStatsBytes.h"
#include <algorithm>

bool HeroRecord_LowerIsBetter(HeroRecordKind kind)
{
  return kind == HeroRecClosestCall;
}

bool HeroRecord_PartyOnly(HeroRecordKind kind)
{
  return kind == HeroRecLongestFall || kind == HeroRecLongestTeleportChain || kind == HeroRecLongestSleep;
}

bool HeroRecord_PartyHasItsOwn(HeroRecordKind kind)
{
  return HeroRecord_PartyOnly(kind) || kind == HeroRecKillStreak;
}

bool HeroRecord_IsTotal(HeroRecordKind kind)
{
  return kind == HeroRecTotalPoisonings || kind == HeroRecTotalWounds;
}

HeroRecordEntry HeroRecordEntry_Empty(void)
{
  HeroRecordEntry entry;
  entry.value = 0;
  entry.who = HERO_STATS_PARTY;
  entry.level = 0;
  entry.time = 0;
  entry.how = HeroXpSource_Other();
  entry.monsterType = -1;
  entry.variant = 0;
  entry.count = 0;
  entry.extra = 0;
  entry.firstLevel = 0;
  entry.firstTime = 0;
  entry.kinds[0][0] = entry.kinds[1][0] = -1;
  entry.kinds[0][1] = entry.kinds[1][1] = 0;
  return entry;
}

void HeroStatsRecords::Forget(void)
{
  m_best.clear();
  m_streaks.clear();
  m_fall = m_teleports = m_sleep = Running();
  m_poisoned.clear();
}

// a beats b: bigger (smaller for the closest call), or as good and earlier
static bool Beats(HeroRecordKind kind, const HeroRecordEntry &a, const HeroRecordEntry &b)
{
  if (a.value != b.value) return HeroRecord_LowerIsBetter(kind) ? a.value < b.value : a.value > b.value;
  return a.time < b.time;
}

void HeroStatsRecords::Offer(HeroRecordKind kind, const HeroRecordEntry &entry)
{
  if (kind < 0 || kind >= HERO_RECORD_COUNT) return;
  if (kind == HeroRecClosestCall ? entry.value < 1 : entry.value <= 0) return;
  Key key((int)kind, entry.who, entry.level);
  std::map<Key, HeroRecordEntry>::iterator kept = m_best.find(key);
  if (kept == m_best.end()) m_best[key] = entry;
  else if (entry.value != kept->second.value && Beats(kind, entry, kept->second)) kept->second = entry;
}

static HeroRecordEntry EntryOf(HeroStatsChampion who, int count, int firstLevel, int level, uint32_t firstTime,
                               uint32_t time, const std::map<int, int> &kinds)
{
  HeroRecordEntry entry = HeroRecordEntry_Empty();
  entry.value = count;
  entry.who = who;
  entry.level = level;
  entry.time = time;
  entry.firstLevel = firstLevel;
  entry.firstTime = firstTime;
  std::vector<std::pair<int, int> > most;  // (-count, type): most killed first, then by type
  for (std::map<int, int>::const_iterator it = kinds.begin(); it != kinds.end(); ++it) most.push_back(std::make_pair(-it->second, it->first));
  std::sort(most.begin(), most.end());
  for (size_t i = 0; i < 2 && i < most.size(); i++)
  {
    entry.kinds[i][0] = most[i].second;
    entry.kinds[i][1] = -most[i].first;
  }
  return entry;
}

bool HeroStatsRecords::Best(HeroRecordKind kind, HeroStatsChampion who, int level, HeroRecordEntry *out) const
{
  bool found = false;
  HeroRecordEntry best = HeroRecordEntry_Empty();
  bool partyOwn = HeroRecord_PartyHasItsOwn(kind);
  auto consider = [&](const HeroRecordEntry &entry) {
    bool whoMatches = who == HERO_STATS_PARTY ? (partyOwn ? entry.who == HERO_STATS_PARTY : entry.who != HERO_STATS_PARTY)
                                              : entry.who == who;
    if (!whoMatches || !HeroStats::LevelMatches(level, entry.level)) return;
    if (kind == HeroRecClosestCall ? entry.value < 1 : entry.value <= 0) return;
    if (!found || Beats(kind, entry, best)) best = entry;
    found = true;
  };
  if (HeroRecord_IsTotal(kind))
  {
    // Each level keeps its champion's running total: add up the levels asked for
    std::map<HeroStatsChampion, HeroRecordEntry> sums;
    for (std::map<Key, HeroRecordEntry>::const_iterator it = m_best.begin(); it != m_best.end(); ++it)
    {
      const HeroRecordEntry &entry = it->second;
      if (std::get<0>(it->first) != (int)kind || !HeroStats::LevelMatches(level, entry.level)) continue;
      std::map<HeroStatsChampion, HeroRecordEntry>::iterator sum = sums.find(entry.who);
      if (sum == sums.end()) sums[entry.who] = entry;
      else
      {
        sum->second.value += entry.value;
        if (entry.time >= sum->second.time) { sum->second.time = entry.time; sum->second.level = entry.level; }
      }
    }
    for (std::map<HeroStatsChampion, HeroRecordEntry>::const_iterator it = sums.begin(); it != sums.end(); ++it) consider(it->second);
  }
  else
  for (std::map<Key, HeroRecordEntry>::const_iterator it = m_best.begin(); it != m_best.end(); ++it)
    if (std::get<0>(it->first) == (int)kind) consider(it->second);
  // What is still running is a record too
  if (kind == HeroRecKillStreak)
    for (std::map<HeroStatsChampion, Running>::const_iterator it = m_streaks.begin(); it != m_streaks.end(); ++it)
      consider(EntryOf(it->first, it->second.count, it->second.firstLevel, it->second.level, it->second.firstTime,
                       it->second.time, it->second.kinds));
  if (kind == HeroRecLongestPoisoned || kind == HeroRecMostPoisonings)
    for (std::map<HeroStatsChampion, Stretch>::const_iterator it = m_poisoned.begin(); it != m_poisoned.end(); ++it)
      consider(StretchEntry(it->first, it->second, kind == HeroRecLongestPoisoned));
  const Running *chain = kind == HeroRecLongestFall ? &m_fall : kind == HeroRecLongestTeleportChain ? &m_teleports
                         : kind == HeroRecLongestSleep ? &m_sleep : NULL;
  if (chain != NULL)
    consider(EntryOf(HERO_STATS_PARTY, chain->count, chain->firstLevel, chain->level, chain->firstTime, chain->time, chain->kinds));
  if (found && out != NULL) *out = best;
  return found;
}

// value: the ticks of the stretch (byLength) or the doses that began in it; extra: its poison
// damage, count: its doses
HeroRecordEntry HeroStatsRecords::StretchEntry(HeroStatsChampion who, const Stretch &stretch, bool byLength)
{
  HeroRecordEntry entry = HeroRecordEntry_Empty();
  entry.value = byLength ? (int)(stretch.time - stretch.firstTime) : stretch.doses;
  entry.who = who;
  entry.level = stretch.level;
  entry.time = stretch.time;
  entry.firstLevel = stretch.firstLevel;
  entry.firstTime = stretch.firstTime;
  entry.extra = stretch.damage;
  entry.count = stretch.doses;
  return entry;
}

void HeroStatsRecords::Extend(Running *running, int level, uint32_t time)
{
  if (running->count == 0)
  {
    running->firstLevel = level;
    running->firstTime = time;
  }
  running->count++;
  running->level = level;
  running->time = time;
}

void HeroStatsRecords::OfferRunning(HeroRecordKind kind, HeroStatsChampion who, const Running &running)
{
  if (running.count > 0)
    Offer(kind, EntryOf(who, running.count, running.firstLevel, running.level, running.firstTime, running.time, running.kinds));
}

void HeroStatsRecords::CountKill(HeroStatsChampion who, int level, uint32_t time, int monsterType)
{
  HeroStatsChampion streaks[2] = {who, HERO_STATS_PARTY};
  for (int i = 0; i < 2; i++)
  {
    Running &running = m_streaks[streaks[i]];
    Extend(&running, level, time);
    running.kinds[monsterType]++;
  }
}

void HeroStatsRecords::CountHarm(HeroStatsChampion who)
{
  HeroStatsChampion streaks[2] = {who, HERO_STATS_PARTY};
  for (int i = 0; i < 2; i++)
  {
    std::map<HeroStatsChampion, Running>::iterator running = m_streaks.find(streaks[i]);
    if (running == m_streaks.end()) continue;
    OfferRunning(HeroRecKillStreak, streaks[i], running->second);
    m_streaks.erase(running);
  }
}

void HeroStatsRecords::CountPitFall(int level, uint32_t time)
{
  Extend(&m_fall, level, time);
}

void HeroStatsRecords::CountTeleport(int level, uint32_t time)
{
  Extend(&m_teleports, level, time);
}

void HeroStatsRecords::CountMove(void)
{
  OfferRunning(HeroRecLongestFall, HERO_STATS_PARTY, m_fall);
  OfferRunning(HeroRecLongestTeleportChain, HERO_STATS_PARTY, m_teleports);
  m_fall = m_teleports = Running();
}

void HeroStatsRecords::CountSleepTick(int level, uint32_t time, bool asleep)
{
  if (asleep)
  {
    Extend(&m_sleep, level, time);
    return;
  }
  OfferRunning(HeroRecLongestSleep, HERO_STATS_PARTY, m_sleep);
  m_sleep = Running();
}

void HeroStatsRecords::CountPoison(HeroStatsChampion who, int level, uint32_t time, int combined, bool newDose)
{
  std::map<HeroStatsChampion, Stretch>::iterator at = m_poisoned.find(who);
  if (at == m_poisoned.end())
  {
    Stretch stretch = {level, level, time, time, 0, 0};
    at = m_poisoned.insert(std::make_pair(who, stretch)).first;
  }
  at->second.level = level;
  at->second.time = time;
  if (newDose) at->second.doses++;
  HeroRecordEntry entry = HeroRecordEntry_Empty();
  entry.value = combined;
  entry.who = who;
  entry.level = level;
  entry.time = time;
  Offer(HeroRecMostPoison, entry);
  if (newDose) CountTotal(HeroRecTotalPoisonings, who, level, time, 1);
}

void HeroStatsRecords::CountPoisonDamage(HeroStatsChampion who, int damage)
{
  std::map<HeroStatsChampion, Stretch>::iterator at = m_poisoned.find(who);
  if (at != m_poisoned.end()) at->second.damage += damage;
}

void HeroStatsRecords::EndPoison(HeroStatsChampion who, int level, uint32_t time)
{
  std::map<HeroStatsChampion, Stretch>::iterator at = m_poisoned.find(who);
  if (at == m_poisoned.end()) return;
  at->second.level = level;
  at->second.time = time;
  Offer(HeroRecLongestPoisoned, StretchEntry(who, at->second, true));
  Offer(HeroRecMostPoisonings, StretchEntry(who, at->second, false));
  m_poisoned.erase(at);
}

void HeroStatsRecords::CountWounds(HeroStatsChampion who, int level, uint32_t time, int woundedNow, int newWounds)
{
  HeroRecordEntry entry = HeroRecordEntry_Empty();
  entry.who = who;
  entry.level = level;
  entry.time = time;
  entry.value = woundedNow;
  Offer(HeroRecMostWoundsAtOnce, entry);
  entry.value = newWounds;
  Offer(HeroRecMostWoundsInOneBlow, entry);
  if (newWounds > 0) CountTotal(HeroRecTotalWounds, who, level, time, newWounds);
}

// Adds to the champion's total on the level: the entry of the kind, champion and level is the total there
void HeroStatsRecords::CountTotal(HeroRecordKind kind, HeroStatsChampion who, int level, uint32_t time, int add)
{
  std::map<Key, HeroRecordEntry>::iterator at = m_best.find(Key((int)kind, who, level));
  HeroRecordEntry entry = at == m_best.end() ? HeroRecordEntry_Empty() : at->second;
  entry.value += add;
  entry.who = who;
  entry.level = level;
  entry.time = time;
  m_best[Key((int)kind, who, level)] = entry;
}

// ---------------------------------------------------------------------------------------------
// The .map file

static const size_t SOURCE_BYTES = 1 + 4 + 4 + 4 + 4;
static const size_t ENTRY_BYTES = 4 + 2 + 4 + 4 + SOURCE_BYTES + 4 + 4 + 4 + 4 + 4 + 4 + 4 * 4;
static const size_t RUNNING_BYTES = 4 + 4 + 4 + 4 + 4 + 4;

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

static void PutEntry(std::vector<uint8_t> *out, const HeroRecordEntry &entry)
{
  HeroBytes_PutInt(out, entry.value);
  HeroBytes_Put(out, entry.who, 2);
  HeroBytes_PutInt(out, entry.level);
  HeroBytes_Put(out, entry.time, 4);
  PutSource(out, entry.how);
  HeroBytes_PutInt(out, entry.monsterType);
  HeroBytes_PutInt(out, entry.variant);
  HeroBytes_PutInt(out, entry.count);
  HeroBytes_PutInt(out, entry.extra);
  HeroBytes_PutInt(out, entry.firstLevel);
  HeroBytes_Put(out, entry.firstTime, 4);
  for (int i = 0; i < 2; i++)
  {
    HeroBytes_PutInt(out, entry.kinds[i][0]);
    HeroBytes_PutInt(out, entry.kinds[i][1]);
  }
}

static bool GetEntry(HeroBytesReader &in, HeroRecordEntry *entry)
{
  entry->value = in.GetInt();
  entry->who = (HeroStatsChampion)in.Get(2);
  entry->level = in.GetInt();
  entry->time = in.Get(4);
  if (!GetSource(in, &entry->how)) return false;
  entry->monsterType = in.GetInt();
  entry->variant = in.GetInt();
  entry->count = in.GetInt();
  entry->extra = in.GetInt();
  entry->firstLevel = in.GetInt();
  entry->firstTime = in.Get(4);
  for (int i = 0; i < 2; i++)
  {
    entry->kinds[i][0] = in.GetInt();
    entry->kinds[i][1] = in.GetInt();
  }
  return in.ok;
}

static void PutRunning(std::vector<uint8_t> *out, int count, int firstLevel, int level, uint32_t firstTime, uint32_t time,
                       const std::map<int, int> &kinds)
{
  HeroBytes_PutInt(out, count);
  HeroBytes_PutInt(out, firstLevel);
  HeroBytes_PutInt(out, level);
  HeroBytes_Put(out, firstTime, 4);
  HeroBytes_Put(out, time, 4);
  HeroBytes_Put(out, (uint32_t)kinds.size(), 4);
  for (std::map<int, int>::const_iterator it = kinds.begin(); it != kinds.end(); ++it)
  {
    HeroBytes_PutInt(out, it->first);
    HeroBytes_PutInt(out, it->second);
  }
}

void HeroStatsRecords::Serialize(std::vector<uint8_t> *out) const
{
  HeroBytes_Put(out, (uint32_t)m_best.size(), 4);
  for (std::map<Key, HeroRecordEntry>::const_iterator it = m_best.begin(); it != m_best.end(); ++it)
  {
    HeroBytes_Put(out, (uint32_t)std::get<0>(it->first), 1);
    PutEntry(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_streaks.size(), 4);
  for (std::map<HeroStatsChampion, Running>::const_iterator it = m_streaks.begin(); it != m_streaks.end(); ++it)
  {
    HeroBytes_Put(out, it->first, 2);
    PutRunning(out, it->second.count, it->second.firstLevel, it->second.level, it->second.firstTime, it->second.time, it->second.kinds);
  }
  const Running *chains[3] = {&m_fall, &m_teleports, &m_sleep};
  for (int i = 0; i < 3; i++)
    PutRunning(out, chains[i]->count, chains[i]->firstLevel, chains[i]->level, chains[i]->firstTime, chains[i]->time, chains[i]->kinds);
  HeroBytes_Put(out, (uint32_t)m_poisoned.size(), 4);
  for (std::map<HeroStatsChampion, Stretch>::const_iterator it = m_poisoned.begin(); it != m_poisoned.end(); ++it)
  {
    HeroBytes_Put(out, it->first, 2);
    HeroBytes_PutInt(out, it->second.firstLevel);
    HeroBytes_PutInt(out, it->second.level);
    HeroBytes_Put(out, it->second.firstTime, 4);
    HeroBytes_Put(out, it->second.time, 4);
    HeroBytes_PutInt(out, it->second.damage);
    HeroBytes_PutInt(out, it->second.doses);
  }
}

static bool GetRunning(HeroBytesReader &in, int *count, int *firstLevel, int *level, uint32_t *firstTime, uint32_t *time,
                       std::map<int, int> *kinds)
{
  *count = in.GetInt();
  *firstLevel = in.GetInt();
  *level = in.GetInt();
  *firstTime = in.Get(4);
  *time = in.Get(4);
  uint32_t kindCount = in.GetCount(4 + 4);
  for (uint32_t i = 0; i < kindCount && in.ok; i++)
  {
    int type = in.GetInt();
    (*kinds)[type] = in.GetInt();
  }
  return in.ok && *count >= 0;
}

size_t HeroStatsRecords::SerializedSize(void) const
{
  std::vector<uint8_t> bytes;
  Serialize(&bytes);
  return bytes.size();
}

bool HeroStatsRecords::Deserialize(HeroBytesReader &in, bool withPoison)
{
  HeroStatsRecords read;
  uint32_t count = in.GetCount(1 + ENTRY_BYTES);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    uint32_t kind = in.Get(1);
    HeroRecordEntry entry;
    if (!GetEntry(in, &entry) || kind >= (uint32_t)HERO_RECORD_COUNT) return false;
    read.m_best[Key((int)kind, entry.who, entry.level)] = entry;
  }
  count = in.GetCount(2 + RUNNING_BYTES);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    Running running;
    if (!GetRunning(in, &running.count, &running.firstLevel, &running.level, &running.firstTime, &running.time, &running.kinds))
      return false;
    read.m_streaks[who] = running;
  }
  Running *chains[3] = {&read.m_fall, &read.m_teleports, &read.m_sleep};
  for (int i = 0; i < 3; i++)
    if (!GetRunning(in, &chains[i]->count, &chains[i]->firstLevel, &chains[i]->level, &chains[i]->firstTime, &chains[i]->time,
                    &chains[i]->kinds))
      return false;
  count = withPoison ? in.GetCount(2 + 4 + 4 + 4 + 4 + 4 + 4) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    Stretch stretch;
    stretch.firstLevel = in.GetInt();
    stretch.level = in.GetInt();
    stretch.firstTime = in.Get(4);
    stretch.time = in.Get(4);
    stretch.damage = in.GetInt();
    stretch.doses = in.GetInt();
    if (stretch.damage < 0 || stretch.doses < 0 || stretch.time < stretch.firstTime) return false;
    read.m_poisoned[who] = stretch;
  }
  if (!in.ok) return false;
  *this = read;
  return true;
}
