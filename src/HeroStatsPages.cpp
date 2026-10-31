// HeroStatsPages.cpp : the numbers of the statistics pages, made from a HeroStats.
#include "HeroStatsPages.h"

#include <algorithm>
#include <map>
#include <utility>

const char *const HERO_STATS_METRIC_LABELS[HERO_METRIC_COUNT] = {"Damage dealt", "Kills", "Damage taken", "Healed",
                                                                 "Mana spent", "Movement"};

const char *const HERO_STATS_MOVEMENT_LABELS[HERO_MOVE_COLUMN_COUNT] = {
    "Steps", "Turns", "Wall bumps", "Teleports", "Pit falls", "Pit climbs", "Time (ticks)", "Asleep (ticks)"};

int HeroStatsPages_MovementValue(const HeroStatsMovement &m, int column)
{
  switch (column)
  {
  case HeroMoveSteps: return m.steps;
  case HeroMoveTurns: return m.turns;
  case HeroMoveBumps: return m.bumps;
  case HeroMoveTeleports: return m.teleports;
  case HeroMovePitFalls: return m.pitFalls;
  case HeroMovePitClimbs: return m.pitClimbs;
  case HeroMoveTicks: return m.ticks;
  default: return m.sleepTicks;
  }
}

// ---------------------------------------------------------------------------------------------
// Table and Bars

static HeroStatsCell CellOf(const HeroStats &stats, HeroStatsChampion who, int level)
{
  HeroStatsCell c;
  c.melee = stats.DamageDealt(who, level, HeroDamageMelee);
  c.ranged = stats.DamageDealt(who, level, HeroDamageRanged);
  c.magic = stats.DamageDealt(who, level, HeroDamageMagic);
  c.total = c.melee + c.ranged + c.magic;
  c.kills = stats.Kills(who, level);
  c.taken = stats.Harm(who, level);
  c.friendlyFire = stats.FriendlyFireCaused(who, level);
  c.healed = stats.Healed(who, level);
  c.given = stats.HealingGiven(who, level);
  c.generated = stats.ManaGained(who, level);
  c.spent = stats.ManaSpent(who, level);
  c.wasted = stats.ManaWasted(who, level);
  c.deaths = 0;
  std::vector<HeroStatsDeath> deaths = stats.Deaths(who);
  for (size_t i = 0; i < deaths.size(); i++)
    if (level == HERO_STATS_WHOLE_RUN || deaths[i].level == level) c.deaths++;
  return c;
}

HeroStatsOverview HeroStatsPages_Overview(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level)
{
  HeroStatsOverview data;
  data.columns = columns;
  for (size_t i = 0; i < data.columns.size(); i++) data.cells.push_back(CellOf(stats, data.columns[i].id, level));
  data.cells.push_back(CellOf(stats, HERO_STATS_PARTY, level));
  data.wipes = stats.PartyWipes();
  data.reloads = stats.Reloads();
  data.movement = stats.Movement(level);
  return data;
}

int HeroStatsPages_Metric(const HeroStats &stats, HeroStatsChampion who, int level, int metric)
{
  switch (metric)
  {
  case HeroMetricDealt:
    return stats.DamageDealt(who, level, HeroDamageMelee) + stats.DamageDealt(who, level, HeroDamageRanged) +
           stats.DamageDealt(who, level, HeroDamageMagic);
  case HeroMetricKills: return stats.Kills(who, level);
  case HeroMetricTaken: return stats.Harm(who, level);
  case HeroMetricHealed: return stats.Healed(who, level);
  case HeroMetricSpent: return stats.ManaSpent(who, level);
  default: return 0;
  }
}

// ---------------------------------------------------------------------------------------------
// Monsters

static bool MonsterLineBefore(const HeroStatsMonsterLine &a, const HeroStatsMonsterLine &b)
{
  if (a.total != b.total) return a.total > b.total;
  return a.name < b.name;
}

static HeroStatsMonsterLine EmptyMonsterLine(const std::string &name, size_t columnCount)
{
  HeroStatsMonsterLine line;
  line.name = name;
  line.byColumn.assign(columnCount, 0);
  line.door = line.pit = line.other = line.total = 0;
  return line;
}

std::vector<HeroStatsMonsterLine> HeroStatsPages_Monsters(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                                          int level, const HeroStatsNames &names)
{
  std::vector<HeroStatsMonsterRow> kills = stats.MonsterKills(level);
  std::map<std::string, size_t> lineOf;
  std::vector<HeroStatsMonsterLine> lines;
  for (size_t k = 0; k < kills.size(); k++)
  {
    const HeroStatsMonsterRow &kill = kills[k];
    if (kill.count <= 0) continue;
    std::string name = names.monster(kill.monsterType, kill.variant);
    std::map<std::string, size_t>::iterator found = lineOf.find(name);
    if (found == lineOf.end())
    {
      lines.push_back(EmptyMonsterLine(name, columns.size()));
      found = lineOf.insert(std::make_pair(name, lines.size() - 1)).first;
    }
    HeroStatsMonsterLine &line = lines[found->second];
    line.total += kill.count;
    if (kill.by == HeroBlowByDoor) line.door += kill.count;
    else if (kill.by == HeroBlowByPit) line.pit += kill.count;
    else
    {
      int column = -1;
      if (kill.by == HeroBlowByChampion)
        for (size_t i = 0; i < columns.size(); i++)
          if (columns[i].id == kill.champion) column = (int)i;
      if (column >= 0) line.byColumn[column] += kill.count;
      else line.other += kill.count;  // clouds, traps, monsters, unknown
    }
  }
  std::sort(lines.begin(), lines.end(), MonsterLineBefore);
  return lines;
}

HeroStatsMonsterLine HeroStatsPages_AllMonsters(const std::vector<HeroStatsMonsterLine> &lines, size_t columnCount)
{
  HeroStatsMonsterLine all = EmptyMonsterLine("All monsters", columnCount);
  for (size_t l = 0; l < lines.size(); l++)
  {
    for (size_t i = 0; i < columnCount && i < lines[l].byColumn.size(); i++) all.byColumn[i] += lines[l].byColumn[i];
    all.door += lines[l].door;
    all.pit += lines[l].pit;
    all.other += lines[l].other;
    all.total += lines[l].total;
  }
  return all;
}

// ---------------------------------------------------------------------------------------------
// Hero and Party pages

static const HeroStatsColumn *ColumnOf(const std::vector<HeroStatsColumn> &columns, HeroStatsChampion id)
{
  for (size_t i = 0; i < columns.size(); i++)
    if (columns[i].id == id) return &columns[i];
  return NULL;
}

static void AddLine(std::vector<HeroStatsAmountLine> *lines, const std::string &text, int amount, bool isBad)
{
  for (size_t i = 0; i < lines->size(); i++)
    if ((*lines)[i].text == text)
    {
      (*lines)[i].amount += amount;
      return;
    }
  HeroStatsAmountLine line = {text, amount, isBad};
  lines->push_back(line);
}

static bool LineBefore(const HeroStatsAmountLine &a, const HeroStatsAmountLine &b)
{
  return a.amount > b.amount;
}

static bool WeaponBefore(const HeroStatsWeaponLine &a, const HeroStatsWeaponLine &b)
{
  if (a.damage != b.damage) return a.damage > b.damage;
  return a.name < b.name;
}

static bool OptionBefore(const HeroStatsOptionLine &a, const HeroStatsOptionLine &b)
{
  if (a.damage != b.damage) return a.damage > b.damage;
  return a.name < b.name;
}

static void AddAttacks(HeroStatsHeroPage *page, const HeroStats &stats, HeroStatsChampion who, int level,
                       const HeroStatsNames &names)
{
  // Grouped by weapon (and by whether it was thrown). Rune spells are in Spells.
  std::vector<HeroStatsAttackRow> attacks = stats.Attacks(who, level);
  std::map<std::pair<int, bool>, size_t> weaponAt;
  for (size_t i = 0; i < attacks.size(); i++)
  {
    const HeroStatsAttackRow &a = attacks[i];
    if (a.weapon == HERO_STATS_NO_WEAPON) continue;
    std::pair<int, bool> key(a.weapon, a.thrown);
    std::map<std::pair<int, bool>, size_t>::iterator found = weaponAt.find(key);
    if (found == weaponAt.end())
    {
      HeroStatsWeaponLine w;
      w.name = names.weapon(a.weapon);
      if (a.thrown) w.name += ", thrown";
      w.tries = w.hits = w.damage = w.kills = w.uses = 0;
      w.id = (a.weapon + 2) * 2 + (a.thrown ? 1 : 0);
      page->weapons.push_back(w);
      found = weaponAt.insert(std::make_pair(key, page->weapons.size() - 1)).first;
    }
    HeroStatsWeaponLine &w = page->weapons[found->second];
    w.tries += a.tries;
    w.hits += a.hits;
    w.damage += a.damage;
    w.kills += a.kills;
    w.uses += a.uses;
    std::string optionName = names.option(a.option);
    if (optionName.empty()) optionName = "(no option)";
    size_t o = 0;
    while (o < w.options.size() && w.options[o].name != optionName) o++;
    if (o == w.options.size())
    {
      HeroStatsOptionLine none = {optionName, 0, 0, 0, 0, 0};
      w.options.push_back(none);
    }
    HeroStatsOptionLine &option = w.options[o];
    option.tries += a.tries;
    option.hits += a.hits;
    option.damage += a.damage;
    option.kills += a.kills;
    option.uses += a.uses;
  }
  std::sort(page->weapons.begin(), page->weapons.end(), WeaponBefore);
  for (size_t i = 0; i < page->weapons.size(); i++)
    std::sort(page->weapons[i].options.begin(), page->weapons[i].options.end(), OptionBefore);
}

static void AddSpells(HeroStatsHeroPage *page, const HeroStats &stats, HeroStatsChampion who, int level)
{
  std::vector<HeroStatsSpellRow> spells = stats.Spells(who, level);
  for (size_t i = 0; i < spells.size(); i++)
  {
    const HeroStatsSpellRow &row = spells[i];
    HeroStatsSpellLine line = {HeroStatsView_SpellName(row.spell), row.casts + row.failed, row.casts, row.failed,
                               row.mana, row.wasted, row.damage, row.kills};
    page->spells.push_back(line);
  }
}

// One line per source; monster attacks in one line, a monster's missiles by kind; friendly fire
// and clouds by causer
static void AddHarm(HeroStatsHeroPage *page, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                    HeroStatsChampion who, int level)
{
  bool isParty = who == HERO_STATS_PARTY;
  std::vector<HeroStatsHarmRow> harm = stats.HarmRows(who, level);
  for (size_t i = 0; i < harm.size(); i++)
  {
    const HeroStatsHarmRow &row = harm[i];
    if (row.amount <= 0) continue;
    bool byChampion = row.source == HeroHarmFriendlyFire || row.source == HeroHarmPoisonCloud;
    std::string text;
    if (byChampion && !isParty && row.causer == who)
      text = row.source == HeroHarmFriendlyFire ? "Friendly fire (own)" : "Poison clouds (own)";
    else
    {
      const HeroStatsColumn *causer = isParty ? NULL : ColumnOf(columns, row.causer);
      text = HeroStatsView_HarmName(row, causer, "");
    }
    AddLine(&page->taken, text, row.amount, byChampion);
  }
  std::stable_sort(page->taken.begin(), page->taken.end(), LineBefore);
}

static void AddHealing(HeroStatsHeroPage *page, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                       HeroStatsChampion who, int level)
{
  bool isParty = who == HERO_STATS_PARTY;
  page->healedTotal = stats.Healed(who, level);
  std::vector<HeroStatsHealRow> heals = stats.HealRows(who, level);
  for (size_t i = 0; i < heals.size(); i++)
  {
    const HeroStatsHealRow &row = heals[i];
    if (row.amount <= 0) continue;
    std::string text = "Regeneration";
    if (row.kind == HeroHealAction) text = "Heal action";
    else if (row.kind == HeroHealPotion)
    {
      text = "Potions";
      if (!isParty)
      {
        const HeroStatsColumn *brewer = ColumnOf(columns, row.brewer);
        if (row.brewer == who) text += " (own)";
        else if (brewer != NULL) text += " (from " + brewer->name + ")";
        else text += " (found)";
      }
    }
    AddLine(&page->healing, text, row.amount, false);
  }
  std::stable_sort(page->healing.begin(), page->healing.end(), LineBefore);
  page->given = stats.HealingGiven(who, level);
  page->resurrected = stats.Resurrections(who, level);
}

// Only the deaths on the picked level; wipes and reloads are the whole run's
static void AddHardship(HeroStatsHeroPage *page, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                        HeroStatsChampion who, int level, const HeroStatsNames &names)
{
  bool isParty = who == HERO_STATS_PARTY;
  std::vector<HeroStatsDeath> deaths = stats.Deaths(who);
  for (size_t i = 0; i < deaths.size(); i++)
  {
    const HeroStatsDeath &death = deaths[i];
    if (level != HERO_STATS_WHOLE_RUN && death.level != level) continue;
    std::string causerName;
    if (death.cause == HeroHarmMonster || death.cause == HeroHarmMonsterMissile)
    {
      // The store keeps no variant of the monster that killed: the first name of its type
      if (death.by.monsterType >= 0) causerName = names.monster(death.by.monsterType, 0);
    }
    else
    {
      const HeroStatsColumn *causer = ColumnOf(columns, death.by.champion);
      if (causer != NULL) causerName = causer->name;
    }
    std::string text = HeroStatsView_DeathName(death, causerName);
    if (isParty)
    {
      const HeroStatsColumn *victim = ColumnOf(columns, death.who);
      text = (victim != NULL ? victim->name : std::string("?")) + ": " + text;
    }
    page->deathLines.push_back(text);
  }
  bool wholeRun = level == HERO_STATS_WHOLE_RUN;
  page->wipes = isParty && wholeRun ? stats.PartyWipes() : 0;
  page->reloads = isParty && wholeRun ? stats.Reloads() : 0;
}

HeroStatsHeroPage HeroStatsPages_Hero(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                      HeroStatsChampion who, int level, const HeroStatsNames &names)
{
  HeroStatsHeroPage page;
  AddAttacks(&page, stats, who, level, names);
  AddSpells(&page, stats, who, level);
  AddHarm(&page, stats, columns, who, level);
  AddHealing(&page, stats, columns, who, level);
  AddHardship(&page, stats, columns, who, level, names);
  page.summaries = HeroStatsView_Summaries(stats, who, level);
  return page;
}
