// HeroStats.cpp : what the champions did, per champion and dungeon level.
#include "HeroStats.h"

#include <algorithm>

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
