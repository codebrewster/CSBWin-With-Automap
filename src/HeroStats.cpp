// HeroStats.cpp : what the champions did, per champion and dungeon level.
#include "HeroStats.h"
#include "HeroStatsBytes.h"

#include <algorithm>
#include <stdio.h>

HeroStatsBlow HeroStatsBlow_Nobody(HeroBlowBy by)
{
  HeroStatsBlow blow;
  blow.by = by;
  blow.champion = HERO_STATS_NOBODY;
  blow.kind = HeroDamageMelee;
  blow.weapon = HERO_STATS_NO_WEAPON;
  blow.thrown = false;
  blow.option = HERO_STATS_NO_OPTION;
  blow.spell = HERO_STATS_NO_SPELL;
  blow.monsterType = -1;
  return blow;
}

uint32_t HeroStats_RuneSpellKey(const void *incantation)
{
  const uint8_t *runes = (const uint8_t *)incantation;
  uint32_t key = 0;
  for (int i = 1; i < 4 && runes[i] != 0; i++) key |= (uint32_t)runes[i] << (8 * (i - 1));
  return key;
}

std::string HeroStats_WeaponName(int weapon, const char *const *itemNames, int nameCount)
{
  if (weapon == HERO_STATS_FISTS) return "FISTS";
  if (weapon == HERO_STATS_NO_WEAPON) return "";
  if (weapon >= 0 && weapon < nameCount && itemNames[weapon] != NULL) return itemNames[weapon];
  char number[16];
  snprintf(number, sizeof number, "#%d", weapon);
  return number;
}

bool HeroStats::Matches(HeroStatsChampion want, HeroStatsChampion have)
{
  return want == HERO_STATS_PARTY || want == have;
}

bool HeroStats::LevelMatches(int want, int have)
{
  return want == HERO_STATS_WHOLE_RUN || want == have;
}

void HeroStats::Forget(void)
{
  m_champions.clear();
  m_levels.clear();
  m_attacks.clear();
  m_damage.clear();
  m_kills.clear();
  m_monsterKills.clear();
  m_harm.clear();
  m_healing.clear();
  m_manaGained.clear();
  m_spells.clear();
  m_deaths.clear();
  m_resurrections.clear();
  m_partyWipes = 0;
  m_reloads = 0;
}

void HeroStats::MeetChampion(HeroStatsChampion id, const std::string &name)
{
  for (size_t i = 0; i < m_champions.size(); i++)
  {
    if (m_champions[i].id == id)
    {
      m_champions[i].name = name;
      return;
    }
  }
  HeroStatsChampionInfo info;
  info.id = id;
  info.name = name;
  m_champions.push_back(info);
}

const std::vector<HeroStatsChampionInfo> &HeroStats::Champions(void) const
{
  return m_champions;
}

void HeroStats::MarkLevelVisited(int level)
{
  std::vector<int>::iterator at = std::lower_bound(m_levels.begin(), m_levels.end(), level);
  if (at == m_levels.end() || *at != level) m_levels.insert(at, level);
}

std::vector<int> HeroStats::VisitedLevels(void) const
{
  return m_levels;
}

HeroStats::AttackCounts &HeroStats::AttackRow(int level, const HeroStatsBlow &blow)
{
  AttackKey key(blow.champion, level, blow.weapon, blow.thrown, blow.option);
  std::map<AttackKey, AttackCounts>::iterator at = m_attacks.find(key);
  if (at == m_attacks.end())
  {
    AttackCounts zero = {0, 0, 0, 0};
    at = m_attacks.insert(std::make_pair(key, zero)).first;
  }
  return at->second;
}

HeroStats::SpellCounts &HeroStats::SpellRow(HeroStatsChampion who, int level, uint32_t spell)
{
  SpellKey key(who, level, spell);
  std::map<SpellKey, SpellCounts>::iterator at = m_spells.find(key);
  if (at == m_spells.end())
  {
    SpellCounts zero = {0, 0, 0, 0, 0, 0};
    at = m_spells.insert(std::make_pair(key, zero)).first;
  }
  return at->second;
}

void HeroStats::CountAttackTry(int level, const HeroStatsBlow &blow)
{
  if (blow.by != HeroBlowByChampion) return;
  AttackRow(level, blow).tries++;
}

void HeroStats::CountAttackHit(int level, const HeroStatsBlow &blow)
{
  if (blow.by != HeroBlowByChampion) return;
  AttackRow(level, blow).hits++;
}

void HeroStats::CreditMonsterDamage(int level, const HeroStatsBlow &blow, int damage)
{
  if (blow.by != HeroBlowByChampion || damage <= 0) return;
  AttackRow(level, blow).damage += damage;
  m_damage[DamageKey(blow.champion, level, blow.kind)] += damage;
  if (blow.spell != HERO_STATS_NO_SPELL) SpellRow(blow.champion, level, blow.spell).damage += damage;
}

void HeroStats::CountMonsterKill(int level, int monsterType, int variant, const HeroStatsBlow &blow)
{
  // Only a champion's blow names a champion; the other causes have no one to credit
  HeroStatsChampion champion = blow.by == HeroBlowByChampion ? blow.champion : HERO_STATS_NOBODY;
  m_monsterKills[MonsterKey(level, monsterType, variant, blow.by, champion)]++;
  if (blow.by != HeroBlowByChampion) return;
  m_kills[std::make_pair(blow.champion, level)]++;
  AttackRow(level, blow).kills++;
  if (blow.spell != HERO_STATS_NO_SPELL) SpellRow(blow.champion, level, blow.spell).kills++;
}

void HeroStats::CreditHarm(HeroStatsChampion victim, int level, HeroHarmSource source,
                           const HeroStatsBlow &blow, int amount)
{
  if (amount <= 0) return;
  HeroStatsChampion causer = blow.by == HeroBlowByChampion ? blow.champion : HERO_STATS_NOBODY;
  int monsterType = blow.by == HeroBlowByMonster ? blow.monsterType : -1;
  m_harm[HarmKey(victim, level, source, causer, monsterType)] += amount;
}

void HeroStats::CreditHealing(HeroStatsChampion who, int level, HeroHealKind kind,
                              HeroStatsChampion brewer, int amount)
{
  if (amount <= 0) return;
  m_healing[HealKey(who, level, kind, brewer)] += amount;
}

void HeroStats::CreditManaGained(HeroStatsChampion who, int level, HeroManaGain gain, int amount)
{
  if (amount <= 0) return;
  m_manaGained[GainKey(who, level, gain)] += amount;
}

void HeroStats::CountCast(HeroStatsChampion who, int level, uint32_t spell, int mana)
{
  SpellCounts &row = SpellRow(who, level, spell);
  row.casts++;
  row.mana += mana;
}

void HeroStats::CountFailedCast(HeroStatsChampion who, int level, uint32_t spell, int wastedMana)
{
  SpellCounts &row = SpellRow(who, level, spell);
  row.failed++;
  row.wasted += wastedMana;
}

void HeroStats::CreditManaSpent(HeroStatsChampion who, int level, uint32_t specialSpell, int mana)
{
  if (mana <= 0) return;
  SpellCounts &row = SpellRow(who, level, specialSpell);
  // Cleared runes bought nothing, so their mana is wasted; shield and heal actions are spent on purpose
  if (specialSpell == HERO_SPELL_RUNES_CLEARED) row.wasted += mana;
  else row.mana += mana;
}

int HeroStats::DamageDealt(HeroStatsChampion who, int level, HeroDamageKind kind) const
{
  int sum = 0;
  for (std::map<DamageKey, int>::const_iterator at = m_damage.begin(); at != m_damage.end(); ++at)
  {
    if (std::get<2>(at->first) != kind) continue;
    if (!Matches(who, std::get<0>(at->first))) continue;
    if (!LevelMatches(level, std::get<1>(at->first))) continue;
    sum += at->second;
  }
  return sum;
}

int HeroStats::Kills(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<std::pair<HeroStatsChampion, int>, int>::const_iterator at = m_kills.begin();
       at != m_kills.end(); ++at)
  {
    if (Matches(who, at->first.first) && LevelMatches(level, at->first.second)) sum += at->second;
  }
  return sum;
}

std::vector<HeroStatsAttackRow> HeroStats::Attacks(HeroStatsChampion who, int level) const
{
  // Rows of the same weapon, thrown flag and option add up, whoever dealt them
  typedef std::tuple<int, bool, int> RowKey;
  std::map<RowKey, AttackCounts> sums;
  for (std::map<AttackKey, AttackCounts>::const_iterator at = m_attacks.begin();
       at != m_attacks.end(); ++at)
  {
    const AttackKey &key = at->first;
    if (!Matches(who, std::get<0>(key)) || !LevelMatches(level, std::get<1>(key))) continue;
    AttackCounts zero = {0, 0, 0, 0};
    AttackCounts &sum = sums.insert(std::make_pair(
        RowKey(std::get<2>(key), std::get<3>(key), std::get<4>(key)), zero)).first->second;
    sum.tries += at->second.tries;
    sum.hits += at->second.hits;
    sum.damage += at->second.damage;
    sum.kills += at->second.kills;
  }
  std::vector<HeroStatsAttackRow> rows;
  for (std::map<RowKey, AttackCounts>::const_iterator at = sums.begin(); at != sums.end(); ++at)
  {
    HeroStatsAttackRow row;
    row.weapon = std::get<0>(at->first);
    row.thrown = std::get<1>(at->first);
    row.option = std::get<2>(at->first);
    row.tries = at->second.tries;
    row.hits = at->second.hits;
    row.damage = at->second.damage;
    row.kills = at->second.kills;
    rows.push_back(row);
  }
  return rows;
}

std::vector<HeroStatsMonsterRow> HeroStats::MonsterKills(int level) const
{
  typedef std::tuple<int, int, int, HeroStatsChampion> RowKey;  // type, variant, by, champion
  std::map<RowKey, int> sums;
  for (std::map<MonsterKey, int>::const_iterator at = m_monsterKills.begin();
       at != m_monsterKills.end(); ++at)
  {
    const MonsterKey &key = at->first;
    if (!LevelMatches(level, std::get<0>(key))) continue;
    sums[RowKey(std::get<1>(key), std::get<2>(key), std::get<3>(key), std::get<4>(key))] += at->second;
  }
  std::vector<HeroStatsMonsterRow> rows;
  for (std::map<RowKey, int>::const_iterator at = sums.begin(); at != sums.end(); ++at)
  {
    HeroStatsMonsterRow row;
    row.monsterType = std::get<0>(at->first);
    row.variant = std::get<1>(at->first);
    row.by = (HeroBlowBy)std::get<2>(at->first);
    row.champion = std::get<3>(at->first);
    row.count = at->second;
    rows.push_back(row);
  }
  return rows;
}

int HeroStats::Harm(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<HarmKey, int>::const_iterator at = m_harm.begin(); at != m_harm.end(); ++at)
  {
    if (Matches(who, std::get<0>(at->first)) && LevelMatches(level, std::get<1>(at->first))) sum += at->second;
  }
  return sum;
}

int HeroStats::FriendlyFireCaused(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<HarmKey, int>::const_iterator at = m_harm.begin(); at != m_harm.end(); ++at)
  {
    HeroStatsChampion causer = std::get<3>(at->first);
    if (causer == HERO_STATS_NOBODY || !Matches(who, causer)) continue;
    if (LevelMatches(level, std::get<1>(at->first))) sum += at->second;
  }
  return sum;
}

static bool BiggestHarmFirst(const HeroStatsHarmRow &a, const HeroStatsHarmRow &b)
{
  return a.amount > b.amount;
}

std::vector<HeroStatsHarmRow> HeroStats::HarmRows(HeroStatsChampion who, int level) const
{
  typedef std::tuple<int, HeroStatsChampion, int> RowKey;  // source, causer, monster type
  std::map<RowKey, int> sums;
  for (std::map<HarmKey, int>::const_iterator at = m_harm.begin(); at != m_harm.end(); ++at)
  {
    const HarmKey &key = at->first;
    if (!Matches(who, std::get<0>(key)) || !LevelMatches(level, std::get<1>(key))) continue;
    sums[RowKey(std::get<2>(key), std::get<3>(key), std::get<4>(key))] += at->second;
  }
  std::vector<HeroStatsHarmRow> rows;
  for (std::map<RowKey, int>::const_iterator at = sums.begin(); at != sums.end(); ++at)
  {
    HeroStatsHarmRow row;
    row.source = (HeroHarmSource)std::get<0>(at->first);
    row.causer = std::get<1>(at->first);
    row.monsterType = std::get<2>(at->first);
    row.amount = at->second;
    rows.push_back(row);
  }
  std::stable_sort(rows.begin(), rows.end(), BiggestHarmFirst);
  return rows;
}

int HeroStats::Healed(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<HealKey, int>::const_iterator at = m_healing.begin(); at != m_healing.end(); ++at)
  {
    if (Matches(who, std::get<0>(at->first)) && LevelMatches(level, std::get<1>(at->first))) sum += at->second;
  }
  return sum;
}

int HeroStats::HealingGiven(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<HealKey, int>::const_iterator at = m_healing.begin(); at != m_healing.end(); ++at)
  {
    HeroStatsChampion brewer = std::get<3>(at->first);
    if (std::get<2>(at->first) != HeroHealPotion) continue;
    if (brewer == HERO_STATS_NOBODY || !Matches(who, brewer)) continue;
    if (LevelMatches(level, std::get<1>(at->first))) sum += at->second;
  }
  return sum;
}

static bool BiggestHealFirst(const HeroStatsHealRow &a, const HeroStatsHealRow &b)
{
  return a.amount > b.amount;
}

std::vector<HeroStatsHealRow> HeroStats::HealRows(HeroStatsChampion who, int level) const
{
  typedef std::pair<int, HeroStatsChampion> RowKey;  // kind, brewer
  std::map<RowKey, int> sums;
  for (std::map<HealKey, int>::const_iterator at = m_healing.begin(); at != m_healing.end(); ++at)
  {
    const HealKey &key = at->first;
    if (!Matches(who, std::get<0>(key)) || !LevelMatches(level, std::get<1>(key))) continue;
    sums[RowKey(std::get<2>(key), std::get<3>(key))] += at->second;
  }
  std::vector<HeroStatsHealRow> rows;
  for (std::map<RowKey, int>::const_iterator at = sums.begin(); at != sums.end(); ++at)
  {
    HeroStatsHealRow row;
    row.kind = (HeroHealKind)at->first.first;
    row.brewer = at->first.second;
    row.amount = at->second;
    rows.push_back(row);
  }
  std::stable_sort(rows.begin(), rows.end(), BiggestHealFirst);
  return rows;
}

int HeroStats::ManaGained(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<GainKey, int>::const_iterator at = m_manaGained.begin(); at != m_manaGained.end(); ++at)
  {
    if (Matches(who, std::get<0>(at->first)) && LevelMatches(level, std::get<1>(at->first))) sum += at->second;
  }
  return sum;
}

int HeroStats::ManaSpent(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<SpellKey, SpellCounts>::const_iterator at = m_spells.begin(); at != m_spells.end(); ++at)
  {
    if (Matches(who, std::get<0>(at->first)) && LevelMatches(level, std::get<1>(at->first)))
    {
      sum += at->second.mana + at->second.wasted;
    }
  }
  return sum;
}

int HeroStats::ManaWasted(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<SpellKey, SpellCounts>::const_iterator at = m_spells.begin(); at != m_spells.end(); ++at)
  {
    if (Matches(who, std::get<0>(at->first)) && LevelMatches(level, std::get<1>(at->first)))
    {
      sum += at->second.wasted;
    }
  }
  return sum;
}

static bool MostManaFirst(const HeroStatsSpellRow &a, const HeroStatsSpellRow &b)
{
  return a.mana + a.wasted > b.mana + b.wasted;
}

std::vector<HeroStatsSpellRow> HeroStats::Spells(HeroStatsChampion who, int level) const
{
  std::map<uint32_t, SpellCounts> sums;
  for (std::map<SpellKey, SpellCounts>::const_iterator at = m_spells.begin(); at != m_spells.end(); ++at)
  {
    const SpellKey &key = at->first;
    if (!Matches(who, std::get<0>(key)) || !LevelMatches(level, std::get<1>(key))) continue;
    SpellCounts zero = {0, 0, 0, 0, 0, 0};
    SpellCounts &sum = sums.insert(std::make_pair(std::get<2>(key), zero)).first->second;
    sum.casts += at->second.casts;
    sum.mana += at->second.mana;
    sum.failed += at->second.failed;
    sum.wasted += at->second.wasted;
    sum.damage += at->second.damage;
    sum.kills += at->second.kills;
  }
  std::vector<HeroStatsSpellRow> rows;
  for (std::map<uint32_t, SpellCounts>::const_iterator at = sums.begin(); at != sums.end(); ++at)
  {
    HeroStatsSpellRow row;
    row.spell = at->first;
    row.casts = at->second.casts;
    row.mana = at->second.mana;
    row.failed = at->second.failed;
    row.wasted = at->second.wasted;
    row.damage = at->second.damage;
    row.kills = at->second.kills;
    rows.push_back(row);
  }
  std::stable_sort(rows.begin(), rows.end(), MostManaFirst);
  return rows;
}

void HeroStats::CountDeath(const HeroStatsDeath &death)
{
  m_deaths.push_back(death);
}

void HeroStats::CountResurrection(HeroStatsChampion who, int level)
{
  m_resurrections[std::make_pair(who, level)]++;
}

void HeroStats::CountPartyWipe(void)
{
  m_partyWipes++;
}

void HeroStats::CountReload(void)
{
  m_reloads++;
}

std::vector<HeroStatsDeath> HeroStats::Deaths(HeroStatsChampion who) const
{
  std::vector<HeroStatsDeath> deaths;
  for (size_t i = 0; i < m_deaths.size(); i++)
  {
    if (Matches(who, m_deaths[i].who)) deaths.push_back(m_deaths[i]);
  }
  return deaths;
}

int HeroStats::Resurrections(HeroStatsChampion who, int level) const
{
  int sum = 0;
  for (std::map<std::pair<HeroStatsChampion, int>, int>::const_iterator at = m_resurrections.begin();
       at != m_resurrections.end(); ++at)
  {
    if (Matches(who, at->first.first) && LevelMatches(level, at->first.second)) sum += at->second;
  }
  return sum;
}

int HeroStats::PartyWipes(void) const
{
  return m_partyWipes;
}

int HeroStats::Reloads(void) const
{
  return m_reloads;
}

void HeroStats::MergeHardship(const HeroStats &other)
{
  // Both lists describe the same run, so the longer one has seen more of it
  if (other.m_deaths.size() > m_deaths.size()) m_deaths = other.m_deaths;
  m_partyWipes = std::max(m_partyWipes, other.m_partyWipes);
  m_reloads = std::max(m_reloads, other.m_reloads);
}

void HeroStats::TakeFromFile(const HeroStats &fromFile)
{
  // Copy first: fromFile may be this object, and the memory's hardship must outlive the replacement
  HeroStats inMemory(*this);
  *this = fromFile;
  MergeHardship(inMemory);
}

// The layout is documented in AutoMapFile.h. Every map is a count and then its records.
void HeroStats::Serialize(std::vector<uint8_t> *out) const
{
  HeroBytes_Put(out, (uint32_t)m_champions.size(), 4);
  for (size_t i = 0; i < m_champions.size(); i++)
  {
    std::string name = m_champions[i].name.substr(0, 255);
    HeroBytes_Put(out, m_champions[i].id, 2);
    HeroBytes_Put(out, (uint32_t)name.size(), 1);
    out->insert(out->end(), name.begin(), name.end());
  }
  HeroBytes_Put(out, (uint32_t)m_levels.size(), 4);
  for (size_t i = 0; i < m_levels.size(); i++) HeroBytes_PutInt(out, m_levels[i]);

  HeroBytes_Put(out, (uint32_t)m_attacks.size(), 4);
  for (std::map<AttackKey, AttackCounts>::const_iterator it = m_attacks.begin(); it != m_attacks.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_PutInt(out, std::get<2>(it->first));
    HeroBytes_Put(out, std::get<3>(it->first) ? 1 : 0, 1);
    HeroBytes_PutInt(out, std::get<4>(it->first));
    HeroBytes_PutInt(out, it->second.tries);
    HeroBytes_PutInt(out, it->second.hits);
    HeroBytes_PutInt(out, it->second.damage);
    HeroBytes_PutInt(out, it->second.kills);
  }
  HeroBytes_Put(out, (uint32_t)m_damage.size(), 4);
  for (std::map<DamageKey, int>::const_iterator it = m_damage.begin(); it != m_damage.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_PutInt(out, std::get<2>(it->first));
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_kills.size(), 4);
  for (std::map<std::pair<HeroStatsChampion, int>, int>::const_iterator it = m_kills.begin(); it != m_kills.end(); ++it)
  {
    HeroBytes_Put(out, it->first.first, 2);
    HeroBytes_PutInt(out, it->first.second);
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_monsterKills.size(), 4);
  for (std::map<MonsterKey, int>::const_iterator it = m_monsterKills.begin(); it != m_monsterKills.end(); ++it)
  {
    HeroBytes_PutInt(out, std::get<0>(it->first));
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_PutInt(out, std::get<2>(it->first));
    HeroBytes_PutInt(out, std::get<3>(it->first));
    HeroBytes_Put(out, std::get<4>(it->first), 2);
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_harm.size(), 4);
  for (std::map<HarmKey, int>::const_iterator it = m_harm.begin(); it != m_harm.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_PutInt(out, std::get<2>(it->first));
    HeroBytes_Put(out, std::get<3>(it->first), 2);
    HeroBytes_PutInt(out, std::get<4>(it->first));
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_healing.size(), 4);
  for (std::map<HealKey, int>::const_iterator it = m_healing.begin(); it != m_healing.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_PutInt(out, std::get<2>(it->first));
    HeroBytes_Put(out, std::get<3>(it->first), 2);
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_manaGained.size(), 4);
  for (std::map<GainKey, int>::const_iterator it = m_manaGained.begin(); it != m_manaGained.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_PutInt(out, std::get<2>(it->first));
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_spells.size(), 4);
  for (std::map<SpellKey, SpellCounts>::const_iterator it = m_spells.begin(); it != m_spells.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_Put(out, std::get<2>(it->first), 4);
    HeroBytes_PutInt(out, it->second.casts);
    HeroBytes_PutInt(out, it->second.mana);
    HeroBytes_PutInt(out, it->second.failed);
    HeroBytes_PutInt(out, it->second.wasted);
    HeroBytes_PutInt(out, it->second.damage);
    HeroBytes_PutInt(out, it->second.kills);
  }
  HeroBytes_Put(out, (uint32_t)m_deaths.size(), 4);
  for (size_t i = 0; i < m_deaths.size(); i++)
  {
    HeroBytes_Put(out, m_deaths[i].who, 2);
    HeroBytes_PutInt(out, m_deaths[i].level);
    HeroBytes_PutInt(out, (int)m_deaths[i].cause);
    HeroBytes_PutBlow(out, m_deaths[i].by);
  }
  HeroBytes_Put(out, (uint32_t)m_resurrections.size(), 4);
  for (std::map<std::pair<HeroStatsChampion, int>, int>::const_iterator it = m_resurrections.begin(); it != m_resurrections.end(); ++it)
  {
    HeroBytes_Put(out, it->first.first, 2);
    HeroBytes_PutInt(out, it->first.second);
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_PutInt(out, m_partyWipes);
  HeroBytes_PutInt(out, m_reloads);
}

bool HeroStats::Deserialize(const uint8_t *data, size_t size)
{
  HeroBytesReader in(data, size);
  HeroStats read;
  uint32_t count = in.GetCount(3);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampionInfo info;
    info.id = (HeroStatsChampion)in.Get(2);
    uint32_t length = in.Get(1);
    if (!in.ok || in.left < length) return false;
    info.name.assign((const char *)in.p, length);
    in.p += length;
    in.left -= length;
    read.m_champions.push_back(info);
  }
  count = in.GetCount(4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    int level = in.GetInt();
    if (level < 0) return false;
    read.MarkLevelVisited(level);   // keeps the list sorted and free of repeats
  }

  count = in.GetCount(2 + 4 + 4 + 1 + 4 + 16);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int weapon = in.GetInt();
    bool thrown = in.Get(1) != 0;
    int option = in.GetInt();
    AttackCounts counts;
    counts.tries = in.GetInt();
    counts.hits = in.GetInt();
    counts.damage = in.GetInt();
    counts.kills = in.GetInt();
    read.m_attacks[AttackKey(who, level, weapon, thrown, option)] = counts;
  }
  count = in.GetCount(2 + 4 + 4 + 4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int kind = in.GetInt();
    int damage = in.GetInt();
    if (kind < 0 || kind >= HERO_DAMAGE_KIND_COUNT) return false;
    read.m_damage[DamageKey(who, level, kind)] = damage;
  }
  count = in.GetCount(2 + 4 + 4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    read.m_kills[std::make_pair(who, level)] = in.GetInt();
  }
  count = in.GetCount(4 * 4 + 2 + 4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    int level = in.GetInt();
    int type = in.GetInt();
    int variant = in.GetInt();
    int by = in.GetInt();
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int number = in.GetInt();
    if (by < 0 || by > (int)HeroBlowByOther) return false;
    read.m_monsterKills[MonsterKey(level, type, variant, by, who)] = number;
  }
  count = in.GetCount(2 + 4 + 4 + 2 + 4 + 4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion victim = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int source = in.GetInt();
    HeroStatsChampion causer = (HeroStatsChampion)in.Get(2);
    int type = in.GetInt();
    int amount = in.GetInt();
    if (source < 0 || source >= HERO_HARM_SOURCE_COUNT) return false;
    read.m_harm[HarmKey(victim, level, source, causer, type)] = amount;
  }
  count = in.GetCount(2 + 4 + 4 + 2 + 4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int kind = in.GetInt();
    HeroStatsChampion brewer = (HeroStatsChampion)in.Get(2);
    int amount = in.GetInt();
    if (kind < 0 || kind >= HERO_HEAL_KIND_COUNT) return false;
    read.m_healing[HealKey(who, level, kind, brewer)] = amount;
  }
  count = in.GetCount(2 + 4 + 4 + 4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int gain = in.GetInt();
    int amount = in.GetInt();
    if (gain < 0 || gain >= HERO_MANA_GAIN_COUNT) return false;
    read.m_manaGained[GainKey(who, level, gain)] = amount;
  }
  count = in.GetCount(2 + 4 + 4 + 24);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    uint32_t spell = in.Get(4);
    SpellCounts counts;
    counts.casts = in.GetInt();
    counts.mana = in.GetInt();
    counts.failed = in.GetInt();
    counts.wasted = in.GetInt();
    counts.damage = in.GetInt();
    counts.kills = in.GetInt();
    read.m_spells[SpellKey(who, level, spell)] = counts;
  }
  count = in.GetCount(2 + 4 + 4 + HERO_BYTES_BLOW_SIZE);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsDeath death;
    death.who = (HeroStatsChampion)in.Get(2);
    death.level = in.GetInt();
    int cause = in.GetInt();
    if (cause < 0 || cause >= HERO_HARM_SOURCE_COUNT) return false;
    death.cause = (HeroHarmSource)cause;
    if (!in.GetBlow(&death.by)) return false;
    read.m_deaths.push_back(death);
  }
  count = in.GetCount(2 + 4 + 4);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    read.m_resurrections[std::make_pair(who, level)] = in.GetInt();
  }
  read.m_partyWipes = in.GetInt();
  read.m_reloads = in.GetInt();
  if (!in.ok || in.left != 0) return false;
  *this = read;
  return true;
}
