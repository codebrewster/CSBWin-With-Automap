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
  if (weapon == HERO_STATS_UNARMED) return "UNARMED";
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
  m_movement.clear();
  m_tilesWalked.clear();
  m_tilesExplored.clear();
  m_items.clear();
  m_consumed.clear();
  m_doorsBashed.clear();
  m_nourishment.clear();
  m_chestsOpened.clear();
  m_scrollsRead.clear();
  m_wallTextsRead.clear();
  m_championsSeen.clear();
  m_championsViewed.clear();
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
    AttackCounts zero = {0, 0, 0, 0, 0};
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

void HeroStats::CountActionUse(int level, const HeroStatsBlow &blow)
{
  if (blow.by != HeroBlowByChampion) return;
  AttackRow(level, blow).uses++;
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
  uint32_t missile = source == HeroHarmMonsterMissile && (blow.spell & 0xff000000) == HERO_SPELL_MONSTER_MISSILE
                         ? blow.spell : 0;
  m_harm[HarmKey(victim, level, source, causer, monsterType, missile)] += amount;
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
    AttackCounts zero = {0, 0, 0, 0, 0};
    AttackCounts &sum = sums.insert(std::make_pair(
        RowKey(std::get<2>(key), std::get<3>(key), std::get<4>(key)), zero)).first->second;
    sum.tries += at->second.tries;
    sum.hits += at->second.hits;
    sum.damage += at->second.damage;
    sum.kills += at->second.kills;
    sum.uses += at->second.uses;
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
    row.uses = at->second.uses;
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
  typedef std::tuple<int, HeroStatsChampion, int, uint32_t> RowKey;  // source, causer, monster type, missile
  std::map<RowKey, int> sums;
  for (std::map<HarmKey, int>::const_iterator at = m_harm.begin(); at != m_harm.end(); ++at)
  {
    const HarmKey &key = at->first;
    if (!Matches(who, std::get<0>(key)) || !LevelMatches(level, std::get<1>(key))) continue;
    sums[RowKey(std::get<2>(key), std::get<3>(key), std::get<4>(key), std::get<5>(key))] += at->second;
  }
  std::vector<HeroStatsHarmRow> rows;
  for (std::map<RowKey, int>::const_iterator at = sums.begin(); at != sums.end(); ++at)
  {
    HeroStatsHarmRow row;
    row.source = (HeroHarmSource)std::get<0>(at->first);
    row.causer = std::get<1>(at->first);
    row.monsterType = std::get<2>(at->first);
    row.spell = std::get<3>(at->first);
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

int HeroStats::ManaGainedBy(HeroStatsChampion who, int level, HeroManaGain gain) const
{
  int sum = 0;
  for (std::map<GainKey, int>::const_iterator at = m_manaGained.begin(); at != m_manaGained.end(); ++at)
  {
    if (Matches(who, std::get<0>(at->first)) && LevelMatches(level, std::get<1>(at->first)) &&
        std::get<2>(at->first) == gain)
      sum += at->second;
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

static HeroStatsMovement NoMovement(void)
{
  HeroStatsMovement none = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  return none;
}

void HeroStats::CountStep(int level)
{
  m_movement.insert(std::make_pair(level, NoMovement())).first->second.steps++;
}

void HeroStats::CountTurn(int level)
{
  m_movement.insert(std::make_pair(level, NoMovement())).first->second.turns++;
}

void HeroStats::CountTeleport(int level)
{
  m_movement.insert(std::make_pair(level, NoMovement())).first->second.teleports++;
}

void HeroStats::CountTick(int level, bool asleep)
{
  HeroStatsMovement &movement = m_movement.insert(std::make_pair(level, NoMovement())).first->second;
  movement.ticks++;
  if (asleep) movement.sleepTicks++;
}

void HeroStats::CountPitFall(int level)
{
  m_movement.insert(std::make_pair(level, NoMovement())).first->second.pitFalls++;
}

void HeroStats::CountPitClimb(int level)
{
  m_movement.insert(std::make_pair(level, NoMovement())).first->second.pitClimbs++;
}

void HeroStats::CountBump(int level)
{
  m_movement.insert(std::make_pair(level, NoMovement())).first->second.bumps++;
}

void HeroStats::MarkTileWalked(int level, int x, int y)
{
  m_tilesWalked.insert(std::make_tuple(level, x, y));
}

void HeroStats::SetExploredTiles(const std::map<int, int> &perLevel)
{
  m_tilesExplored = perLevel;
}

HeroStatsMovement HeroStats::Movement(int level) const
{
  HeroStatsMovement sum = NoMovement();
  for (std::set<std::tuple<int, int, int> >::const_iterator at = m_tilesWalked.begin(); at != m_tilesWalked.end(); ++at)
    if (LevelMatches(level, std::get<0>(*at))) sum.tilesWalked++;
  for (std::map<int, int>::const_iterator at = m_tilesExplored.begin(); at != m_tilesExplored.end(); ++at)
    if (LevelMatches(level, at->first)) sum.tilesExplored += at->second;
  for (std::map<int, HeroStatsMovement>::const_iterator at = m_movement.begin(); at != m_movement.end(); ++at)
  {
    if (!LevelMatches(level, at->first)) continue;
    sum.steps += at->second.steps;
    sum.turns += at->second.turns;
    sum.teleports += at->second.teleports;
    sum.ticks += at->second.ticks;
    sum.sleepTicks += at->second.sleepTicks;
    sum.pitFalls += at->second.pitFalls;
    sum.pitClimbs += at->second.pitClimbs;
    sum.bumps += at->second.bumps;
  }
  return sum;
}

void HeroStats::CountItem(int level, int item, HeroItemEvent event)
{
  ItemCounts zero = {0, 0};
  ItemCounts &counts = m_items.insert(std::make_pair(std::make_pair(level, item), zero)).first->second;
  if (event == HeroItemPickedUp) counts.pickedUp++;
  else if (event == HeroItemUsedInWall) counts.usedInWall++;
}

std::vector<HeroStatsItemRow> HeroStats::Items(int level) const
{
  std::map<int, ItemCounts> sums;
  for (std::map<std::pair<int, int>, ItemCounts>::const_iterator at = m_items.begin(); at != m_items.end(); ++at)
  {
    if (!LevelMatches(level, at->first.first)) continue;
    ItemCounts zero = {0, 0};
    ItemCounts &sum = sums.insert(std::make_pair(at->first.second, zero)).first->second;
    sum.pickedUp += at->second.pickedUp;
    sum.usedInWall += at->second.usedInWall;
  }
  std::vector<HeroStatsItemRow> rows;
  for (std::map<int, ItemCounts>::const_iterator at = sums.begin(); at != sums.end(); ++at)
  {
    if (at->second.pickedUp == 0 && at->second.usedInWall == 0) continue;
    HeroStatsItemRow row = {at->first, at->second.pickedUp, at->second.usedInWall};
    rows.push_back(row);
  }
  return rows;
}

void HeroStats::CountConsumed(HeroStatsChampion who, int level, int item, HeroConsumeKind kind)
{
  m_consumed[ConsumeKey(who, level, item, (int)kind)]++;
}

std::vector<HeroStatsConsumedRow> HeroStats::Consumed(HeroStatsChampion who, int level) const
{
  std::map<int, HeroStatsConsumedRow> sums;
  for (std::map<ConsumeKey, int>::const_iterator at = m_consumed.begin(); at != m_consumed.end(); ++at)
  {
    const ConsumeKey &key = at->first;
    if (!Matches(who, std::get<0>(key)) || !LevelMatches(level, std::get<1>(key))) continue;
    HeroStatsConsumedRow zero = {std::get<2>(key), 0, 0};
    HeroStatsConsumedRow &row = sums.insert(std::make_pair(std::get<2>(key), zero)).first->second;
    if (std::get<3>(key) == HeroConsumeDrunk) row.drunk += at->second;
    else row.eaten += at->second;
  }
  std::vector<HeroStatsConsumedRow> rows;
  for (std::map<int, HeroStatsConsumedRow>::const_iterator at = sums.begin(); at != sums.end(); ++at)
    rows.push_back(at->second);
  return rows;
}

int HeroStats::ConsumedOfKind(HeroStatsChampion who, int level, int kind) const
{
  int sum = 0;
  for (std::map<ConsumeKey, int>::const_iterator at = m_consumed.begin(); at != m_consumed.end(); ++at)
  {
    const ConsumeKey &key = at->first;
    if (Matches(who, std::get<0>(key)) && LevelMatches(level, std::get<1>(key)) && std::get<3>(key) == kind)
      sum += at->second;
  }
  return sum;
}

int HeroStats::Drunk(HeroStatsChampion who, int level) const
{
  return ConsumedOfKind(who, level, HeroConsumeDrunk);
}

int HeroStats::Eaten(HeroStatsChampion who, int level) const
{
  return ConsumedOfKind(who, level, HeroConsumeEaten);
}

HeroStatsNourishment &HeroStats::NourishmentRow(HeroStatsChampion who, int level)
{
  HeroStatsNourishment zero = {0, 0, 0, 0, 0};
  return m_nourishment.insert(std::make_pair(std::make_pair(who, level), zero)).first->second;
}

void HeroStats::CreditFood(HeroStatsChampion who, int level, int value, int gained)
{
  if (value <= 0) return;
  gained = std::max(0, std::min(gained, value));
  HeroStatsNourishment &row = NourishmentRow(who, level);
  row.foodGained += gained;
  row.foodWasted += value - gained;
}

void HeroStats::CreditWater(HeroStatsChampion who, int level, int value, int gained)
{
  if (value <= 0) return;
  gained = std::max(0, std::min(gained, value));
  HeroStatsNourishment &row = NourishmentRow(who, level);
  row.waterGained += gained;
  row.waterWasted += value - gained;
}

void HeroStats::CountFountainDrink(HeroStatsChampion who, int level)
{
  NourishmentRow(who, level).fountainDrinks++;
}

HeroStatsNourishment HeroStats::Nourishment(HeroStatsChampion who, int level) const
{
  HeroStatsNourishment sum = {0, 0, 0, 0, 0};
  for (std::map<std::pair<HeroStatsChampion, int>, HeroStatsNourishment>::const_iterator at = m_nourishment.begin();
       at != m_nourishment.end(); ++at)
  {
    if (!Matches(who, at->first.first) || !LevelMatches(level, at->first.second)) continue;
    sum.foodGained += at->second.foodGained;
    sum.foodWasted += at->second.foodWasted;
    sum.waterGained += at->second.waterGained;
    sum.waterWasted += at->second.waterWasted;
    sum.fountainDrinks += at->second.fountainDrinks;
  }
  return sum;
}

void HeroStats::CountDoorBashed(HeroStatsChampion who, int level, HeroDoorMethod method)
{
  m_doorsBashed[DoorKey(who, level, (int)method)]++;
}

int HeroStats::DoorsBashed(HeroStatsChampion who, int level, int method) const
{
  int sum = 0;
  for (std::map<DoorKey, int>::const_iterator at = m_doorsBashed.begin(); at != m_doorsBashed.end(); ++at)
  {
    const DoorKey &key = at->first;
    bool isBash = std::get<2>(key) <= HeroDoorUnknown;
    bool methodMatches = method == HERO_DOOR_ANY_METHOD ? isBash : method == std::get<2>(key);
    if (Matches(who, std::get<0>(key)) && LevelMatches(level, std::get<1>(key)) && methodMatches) sum += at->second;
  }
  return sum;
}

void HeroStats::CountDoorOpenedByZo(HeroStatsChampion who, int level)
{
  m_doorsBashed[DoorKey(who, level, (int)HeroDoorOpenedByZo)]++;
}

int HeroStats::DoorsOpenedByZo(HeroStatsChampion who, int level) const
{
  return DoorsBashed(who, level, HeroDoorOpenedByZo);
}

void HeroStats::CountDoorButton(int level, bool opened)
{
  m_doorsBashed[DoorKey(HERO_STATS_NOBODY, level, (int)(opened ? HeroDoorButtonOpened : HeroDoorButtonClosed))]++;
}

int HeroStats::DoorsOpenedByButton(int level) const
{
  return DoorsBashed(HERO_STATS_PARTY, level, HeroDoorButtonOpened);
}

int HeroStats::DoorsClosedByButton(int level) const
{
  return DoorsBashed(HERO_STATS_PARTY, level, HeroDoorButtonClosed);
}

void HeroStats::CountChestOpened(int level)
{
  m_chestsOpened[level]++;
}

int HeroStats::ChestsOpened(int level) const
{
  int sum = 0;
  for (std::map<int, int>::const_iterator at = m_chestsOpened.begin(); at != m_chestsOpened.end(); ++at)
    if (LevelMatches(level, at->first)) sum += at->second;
  return sum;
}

void HeroStats::CountScrollRead(int level)
{
  m_scrollsRead[level]++;
}

int HeroStats::ScrollsRead(int level) const
{
  int sum = 0;
  for (std::map<int, int>::const_iterator at = m_scrollsRead.begin(); at != m_scrollsRead.end(); ++at)
    if (LevelMatches(level, at->first)) sum += at->second;
  return sum;
}

void HeroStats::SetWallTextsRead(const std::map<int, int> &perLevel)
{
  m_wallTextsRead = perLevel;
}

int HeroStats::SumOverLevels(const std::map<int, int> &perLevel, int level)
{
  int sum = 0;
  for (std::map<int, int>::const_iterator at = perLevel.begin(); at != perLevel.end(); ++at)
    if (LevelMatches(level, at->first)) sum += at->second;
  return sum;
}

void HeroStats::SetChampionsSeen(const std::map<int, int> &seen, const std::map<int, int> &viewed)
{
  m_championsSeen = seen;
  m_championsViewed = viewed;
}

int HeroStats::ChampionsSeen(int level) const
{
  return SumOverLevels(m_championsSeen, level);
}

int HeroStats::ChampionsViewed(int level) const
{
  return SumOverLevels(m_championsViewed, level);
}

int HeroStats::WallTextsRead(int level) const
{
  int sum = 0;
  for (std::map<int, int>::const_iterator at = m_wallTextsRead.begin(); at != m_wallTextsRead.end(); ++at)
    if (LevelMatches(level, at->first)) sum += at->second;
  return sum;
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
    HeroBytes_PutInt(out, it->second.uses);
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
    HeroBytes_Put(out, std::get<5>(it->first), 4);
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
  HeroBytes_Put(out, (uint32_t)m_movement.size(), 4);
  for (std::map<int, HeroStatsMovement>::const_iterator it = m_movement.begin(); it != m_movement.end(); ++it)
  {
    HeroBytes_PutInt(out, it->first);
    HeroBytes_PutInt(out, it->second.steps);
    HeroBytes_PutInt(out, it->second.turns);
    HeroBytes_PutInt(out, it->second.teleports);
    HeroBytes_PutInt(out, it->second.ticks);
    HeroBytes_PutInt(out, it->second.sleepTicks);
    HeroBytes_PutInt(out, it->second.pitFalls);
    HeroBytes_PutInt(out, it->second.pitClimbs);
    HeroBytes_PutInt(out, it->second.bumps);
  }
  HeroBytes_Put(out, (uint32_t)m_items.size(), 4);
  for (std::map<std::pair<int, int>, ItemCounts>::const_iterator it = m_items.begin(); it != m_items.end(); ++it)
  {
    HeroBytes_PutInt(out, it->first.first);
    HeroBytes_PutInt(out, it->first.second);
    HeroBytes_PutInt(out, it->second.pickedUp);
    HeroBytes_PutInt(out, it->second.usedInWall);
  }
  HeroBytes_Put(out, (uint32_t)m_consumed.size(), 4);
  for (std::map<ConsumeKey, int>::const_iterator it = m_consumed.begin(); it != m_consumed.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_PutInt(out, std::get<2>(it->first));
    HeroBytes_PutInt(out, std::get<3>(it->first));
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_doorsBashed.size(), 4);
  for (std::map<DoorKey, int>::const_iterator it = m_doorsBashed.begin(); it != m_doorsBashed.end(); ++it)
  {
    HeroBytes_Put(out, std::get<0>(it->first), 2);
    HeroBytes_PutInt(out, std::get<1>(it->first));
    HeroBytes_PutInt(out, it->second);
    HeroBytes_PutInt(out, std::get<2>(it->first));
  }
  HeroBytes_Put(out, (uint32_t)m_chestsOpened.size(), 4);
  for (std::map<int, int>::const_iterator it = m_chestsOpened.begin(); it != m_chestsOpened.end(); ++it)
  {
    HeroBytes_PutInt(out, it->first);
    HeroBytes_PutInt(out, it->second);
  }
  HeroBytes_Put(out, (uint32_t)m_nourishment.size(), 4);
  for (std::map<std::pair<HeroStatsChampion, int>, HeroStatsNourishment>::const_iterator it = m_nourishment.begin();
       it != m_nourishment.end(); ++it)
  {
    HeroBytes_Put(out, it->first.first, 2);
    HeroBytes_PutInt(out, it->first.second);
    HeroBytes_PutInt(out, it->second.foodGained);
    HeroBytes_PutInt(out, it->second.foodWasted);
    HeroBytes_PutInt(out, it->second.waterGained);
    HeroBytes_PutInt(out, it->second.waterWasted);
    HeroBytes_PutInt(out, it->second.fountainDrinks);
  }
  HeroBytes_Put(out, (uint32_t)m_tilesWalked.size(), 4);
  for (std::set<std::tuple<int, int, int> >::const_iterator it = m_tilesWalked.begin(); it != m_tilesWalked.end(); ++it)
  {
    HeroBytes_PutInt(out, std::get<0>(*it));
    HeroBytes_Put(out, (uint32_t)std::get<1>(*it), 1);
    HeroBytes_Put(out, (uint32_t)std::get<2>(*it), 1);
  }
  HeroBytes_Put(out, (uint32_t)m_scrollsRead.size(), 4);
  for (std::map<int, int>::const_iterator it = m_scrollsRead.begin(); it != m_scrollsRead.end(); ++it)
  {
    HeroBytes_PutInt(out, it->first);
    HeroBytes_PutInt(out, it->second);
  }
}

bool HeroStats::Deserialize(const uint8_t *data, size_t size, int layout)
{
  bool hasMovement = layout >= FIRST_LAYOUT_WITH_MOVEMENT;
  bool hasPitsAndSleep = layout >= FIRST_LAYOUT_WITH_PITS_AND_SLEEP;
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

  count = in.GetCount(2 + 4 + 4 + 1 + 4 + (hasPitsAndSleep ? 20 : 16));
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
    counts.uses = hasPitsAndSleep ? in.GetInt() : 0;
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
  count = in.GetCount(2 + 4 + 4 + 2 + 4 + 4 + (hasMovement ? 4 : 0));
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion victim = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int source = in.GetInt();
    HeroStatsChampion causer = (HeroStatsChampion)in.Get(2);
    int type = in.GetInt();
    int amount = in.GetInt();
    uint32_t missile = hasMovement ? in.Get(4) : 0;
    if (source < 0 || source >= HERO_HARM_SOURCE_COUNT) return false;
    read.m_harm[HarmKey(victim, level, source, causer, type, missile)] = amount;
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
  count = hasMovement ? in.GetCount((hasPitsAndSleep ? 9 : 5) * 4) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    int level = in.GetInt();
    HeroStatsMovement movement = NoMovement();
    movement.steps = in.GetInt();
    movement.turns = in.GetInt();
    movement.teleports = in.GetInt();
    movement.ticks = in.GetInt();
    if (hasPitsAndSleep)
    {
      movement.sleepTicks = in.GetInt();
      movement.pitFalls = in.GetInt();
      movement.pitClimbs = in.GetInt();
      movement.bumps = in.GetInt();
    }
    read.m_movement[level] = movement;
  }
  count = layout >= FIRST_LAYOUT_WITH_ITEMS ? in.GetCount(4 * 4) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    int level = in.GetInt();
    int item = in.GetInt();
    ItemCounts counts;
    counts.pickedUp = in.GetInt();
    counts.usedInWall = in.GetInt();
    read.m_items[std::make_pair(level, item)] = counts;
  }
  bool hasConsumption = layout >= FIRST_LAYOUT_WITH_CONSUMPTION;
  count = hasConsumption ? in.GetCount(2 + 4 * 4) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int item = in.GetInt();
    int kind = in.GetInt();
    int number = in.GetInt();
    if (kind < 0 || kind >= HERO_CONSUME_KIND_COUNT) return false;
    read.m_consumed[ConsumeKey(who, level, item, kind)] = number;
  }
  bool hasNourishment = layout >= FIRST_LAYOUT_WITH_NOURISHMENT;
  count = hasConsumption ? in.GetCount(2 + 4 + 4 + (hasNourishment ? 4 : 0)) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    int number = in.GetInt();
    int method = hasNourishment ? in.GetInt() : (int)HeroDoorUnknown;
    if (method < 0 || method >= HERO_DOOR_METHOD_COUNT) return false;
    read.m_doorsBashed[DoorKey(who, level, method)] = number;
  }
  count = hasConsumption ? in.GetCount(4 + 4) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    int level = in.GetInt();
    read.m_chestsOpened[level] = in.GetInt();
  }
  count = hasNourishment ? in.GetCount(2 + 6 * 4) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    HeroStatsChampion who = (HeroStatsChampion)in.Get(2);
    int level = in.GetInt();
    HeroStatsNourishment row;
    row.foodGained = in.GetInt();
    row.foodWasted = in.GetInt();
    row.waterGained = in.GetInt();
    row.waterWasted = in.GetInt();
    row.fountainDrinks = in.GetInt();
    read.m_nourishment[std::make_pair(who, level)] = row;
  }
  count = layout >= FIRST_LAYOUT_WITH_TILES ? in.GetCount(4 + 1 + 1) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    int level = in.GetInt();
    int x = (int)in.Get(1);
    int y = (int)in.Get(1);
    read.m_tilesWalked.insert(std::make_tuple(level, x, y));
  }
  count = layout >= FIRST_LAYOUT_WITH_SCROLLS ? in.GetCount(4 + 4) : 0;
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    int level = in.GetInt();
    read.m_scrollsRead[level] = in.GetInt();
  }
  if (!in.ok || in.left != 0) return false;
  *this = read;
  return true;
}
