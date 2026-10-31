// HeroStatsPages.cpp : the numbers of the statistics pages, made from a HeroStats.
#include "HeroStatsPages.h"

#include <algorithm>
#include <map>
#include <utility>

const char *const HERO_STATS_METRIC_LABELS[HERO_METRIC_COUNT] = {"Damage dealt", "Kills", "Damage taken", "Healed",
                                                                 "Mana spent", "Movement", "Items", "Food and water",
                                                                 "Dungeon", "Reading", "Champions"};

const char *const HERO_STATS_MOVEMENT_LABELS[HERO_MOVE_COLUMN_COUNT] = {
    "Steps", "Tiles walked", "Tiles explored", "Turns", "Wall bumps", "Teleports", "Pit falls", "Pit climbs", "Time (ticks)", "Asleep (ticks)"};

int HeroStatsPages_MovementValue(const HeroStatsMovement &m, int column)
{
  switch (column)
  {
  case HeroMoveSteps: return m.steps;
  case HeroMoveTilesWalked: return m.tilesWalked;
  case HeroMoveTilesExplored: return m.tilesExplored;
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
  HeroStatsNourishment food = stats.Nourishment(who, level);
  c.drunk = stats.Drunk(who, level) + food.fountainDrinks;   // the fountain is a way of drinking
  c.eaten = stats.Eaten(who, level);
  c.fountainDrinks = food.fountainDrinks;
  c.foodGained = food.foodGained;
  c.foodWasted = food.foodWasted;
  c.waterGained = food.waterGained;
  c.waterWasted = food.waterWasted;
  c.doorsBashed = stats.DoorsBashed(who, level);
  c.doorsSmashed = stats.DoorsBashed(who, level, HeroDoorSmashed);
  c.doorsShot = stats.DoorsBashed(who, level, HeroDoorShot);
  c.doorsBySpell = stats.DoorsBashed(who, level, HeroDoorBySpell);
  c.doorsOpenedByZo = stats.DoorsOpenedByZo(who, level);
  c.deaths = 0;
  std::vector<HeroStatsDeath> deaths = stats.Deaths(who);
  for (size_t i = 0; i < deaths.size(); i++)
    if (level == HERO_STATS_WHOLE_RUN || deaths[i].level == level) c.deaths++;
  return c;
}

static bool CountLineBefore(const HeroStatsOverview::CountLine &a, const HeroStatsOverview::CountLine &b)
{
  if (a.byCell.back() != b.byCell.back()) return a.byCell.back() > b.byCell.back();
  return a.name < b.name;
}

// One line per item name, a number per cell (the champions, then the party)
static void AddConsumedLines(HeroStatsOverview *data, const HeroStats &stats, int level, const HeroStatsNames &names)
{
  size_t cellCount = data->columns.size() + 1;
  std::map<std::string, std::vector<int> > eaten, drunk;
  for (size_t c = 0; c < cellCount; c++)
  {
    HeroStatsChampion who = c < data->columns.size() ? data->columns[c].id : HERO_STATS_PARTY;
    std::vector<HeroStatsConsumedRow> rows = stats.Consumed(who, level);
    for (size_t r = 0; r < rows.size(); r++)
    {
      std::string name = names.weapon(rows[r].item);
      if (rows[r].eaten > 0)
      {
        std::vector<int> &line = eaten.insert(std::make_pair(name, std::vector<int>(cellCount, 0))).first->second;
        line[c] += rows[r].eaten;
      }
      if (rows[r].drunk > 0)
      {
        std::vector<int> &line = drunk.insert(std::make_pair(name, std::vector<int>(cellCount, 0))).first->second;
        line[c] += rows[r].drunk;
      }
    }
  }
  std::map<std::string, std::vector<int> > *maps[2] = {&eaten, &drunk};
  std::vector<HeroStatsOverview::CountLine> *outs[2] = {&data->eatenLines, &data->drunkLines};
  for (int m = 0; m < 2; m++)
  {
    for (std::map<std::string, std::vector<int> >::const_iterator at = maps[m]->begin(); at != maps[m]->end(); ++at)
    {
      HeroStatsOverview::CountLine line = {at->first, at->second};
      outs[m]->push_back(line);
    }
    std::sort(outs[m]->begin(), outs[m]->end(), CountLineBefore);
  }
  HeroStatsOverview::CountLine fountain = {"Fountain", std::vector<int>()};
  for (size_t c = 0; c < cellCount; c++) fountain.byCell.push_back(data->cells[c].fountainDrinks);
  if (fountain.byCell.back() > 0) data->drunkLines.push_back(fountain);
}

HeroStatsOverview HeroStatsPages_Overview(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level,
                                          const HeroStatsNames *names)
{
  HeroStatsOverview data;
  data.columns = columns;
  for (size_t i = 0; i < data.columns.size(); i++) data.cells.push_back(CellOf(stats, data.columns[i].id, level));
  data.cells.push_back(CellOf(stats, HERO_STATS_PARTY, level));
  data.wipes = stats.PartyWipes();
  data.reloads = stats.Reloads();
  data.movement = stats.Movement(level);
  std::vector<HeroStatsItemRow> items = stats.Items(level);
  data.itemsPickedUp = data.itemsUsedInWalls = 0;
  for (size_t i = 0; i < items.size(); i++)
  {
    data.itemsPickedUp += items[i].pickedUp;
    data.itemsUsedInWalls += items[i].usedInWall;
  }
  data.chestsOpened = stats.ChestsOpened(level);
  data.doorsOpenedByButton = stats.DoorsOpenedByButton(level);
  data.doorsClosedByButton = stats.DoorsClosedByButton(level);
  data.scrollsRead = stats.ScrollsRead(level);
  data.wallTextsRead = stats.WallTextsRead(level);
  data.championsSeen = stats.ChampionsSeen(level);
  data.championsViewed = stats.ChampionsViewed(level);
  if (names != 0) AddConsumedLines(&data, stats, level, *names);
  return data;
}

const char *const HERO_STATS_TABLE_GROUP_TITLES[HERO_TABLE_GROUP_COUNT] = {
    "Damage dealt", "Damage taken", "Healing", "Hardship", "Mana", "Movement", "Items", "Food and water", "Dungeon",
    "Reading", "Champions"};

static std::string Count(int value, const char *one, const char *many)
{
  return HeroStatsView_Number(value) + " " + (value == 1 ? one : many);
}

std::vector<std::string> HeroStatsPages_TableSummaries(const HeroStatsOverview &data, bool wholeRun)
{
  const HeroStatsCell &p = data.cells.back();
  const char *dot = " \xc2\xb7 ";
  std::vector<std::string> s(HERO_TABLE_GROUP_COUNT);
  s[HeroGroupDealt] = HeroStatsView_Number(p.total) + " damage" + dot + Count(p.kills, "kill", "kills");
  s[HeroGroupTaken] = HeroStatsView_Number(p.taken) + " taken";
  if (p.friendlyFire != 0) s[HeroGroupTaken] += dot + std::string("friendly fire ") + HeroStatsView_Number(p.friendlyFire);
  s[HeroGroupHealing] = HeroStatsView_Number(p.healed) + " healed";
  if (p.given != 0) s[HeroGroupHealing] += dot + HeroStatsView_Number(p.given) + " given";
  std::string hardship;
  if (p.deaths != 0) hardship = Count(p.deaths, "death", "deaths");
  if (wholeRun && data.wipes != 0) hardship += (hardship.empty() ? "" : dot) + Count(data.wipes, "wipe", "wipes");
  if (wholeRun && data.reloads != 0) hardship += (hardship.empty() ? "" : dot) + Count(data.reloads, "reload", "reloads");
  s[HeroGroupHardship] = hardship.empty() ? "never died" : hardship;
  s[HeroGroupMana] = HeroStatsView_Number(p.generated) + " generated" + dot + HeroStatsView_Number(p.spent) + " spent";
  if (p.wasted != 0) s[HeroGroupMana] += dot + HeroStatsView_Number(p.wasted) + " wasted";
  s[HeroGroupMovement] = Count(data.movement.steps, "step", "steps") + dot +
                         Count(data.movement.tilesWalked, "tile walked", "tiles walked") + dot +
                         HeroStatsView_Number(data.movement.tilesExplored) + " explored";
  s[HeroGroupItems] = HeroStatsView_Number(data.itemsPickedUp) + " picked up" + dot +
                      HeroStatsView_Number(data.itemsUsedInWalls) + " used in walls";
  s[HeroGroupFood] = HeroStatsView_Number(p.eaten) + " eaten" + dot + HeroStatsView_Number(p.drunk) + " drunk";
  if (p.foodWasted != 0 || p.waterWasted != 0)
    s[HeroGroupFood] += dot + std::string("wasted ") + HeroStatsView_Number(p.foodWasted) + " food, " +
                        HeroStatsView_Number(p.waterWasted) + " water";
  s[HeroGroupDungeon] = Count(p.doorsBashed, "door bashed", "doors bashed") + dot +
                        Count(data.chestsOpened, "chest opened", "chests opened");
  s[HeroGroupReading] = Count(data.scrollsRead, "scroll", "scrolls") + dot +
                        Count(data.wallTextsRead, "wall text", "wall texts");
  s[HeroGroupChampions] = HeroStatsView_Number(data.championsSeen) + " seen" + dot +
                          HeroStatsView_Number(data.championsViewed) + " viewed";
  return s;
}

std::vector<std::string> HeroStatsPages_PartyTableLabels(int metric)
{
  static const char *const ITEMS[] = {"Picked up", "Used in walls"};
  static const char *const FOOD[] = {"Eaten", "Drunk", "Fountain drinks", "Food gained", "Food wasted", "Water gained",
                                     "Water wasted"};
  static const char *const DUNGEON[] = {"Doors bashed open", "Smashed", "Shot open", "By spell", "Opened with Zo",
                                        "Opened by button", "Closed by button", "Chests opened"};
  std::vector<std::string> labels;
  if (metric == HeroMetricMovement) labels.assign(HERO_STATS_MOVEMENT_LABELS, HERO_STATS_MOVEMENT_LABELS + HERO_MOVE_COLUMN_COUNT);
  else if (metric == HeroMetricItems) labels.assign(ITEMS, ITEMS + 2);
  else if (metric == HeroMetricFood) labels.assign(FOOD, FOOD + 7);
  else if (metric == HeroMetricDungeon) labels.assign(DUNGEON, DUNGEON + 8);
  else if (metric == HeroMetricReading)
  {
    labels.push_back("Scrolls read");
    labels.push_back("Wall texts read");
  }
  else if (metric == HeroMetricChampions)
  {
    labels.push_back("Champions seen");
    labels.push_back("Champions viewed");
  }
  return labels;
}

std::vector<int> HeroStatsPages_PartyTableRow(const HeroStats &stats, int level, int metric)
{
  std::vector<int> row;
  if (metric == HeroMetricMovement)
  {
    HeroStatsMovement movement = stats.Movement(level);
    for (int c = 0; c < HERO_MOVE_COLUMN_COUNT; c++) row.push_back(HeroStatsPages_MovementValue(movement, c));
    return row;
  }
  HeroStatsChampion party = HERO_STATS_PARTY;
  if (metric == HeroMetricItems)
  {
    std::vector<HeroStatsItemRow> items = stats.Items(level);
    int pickedUp = 0, usedInWall = 0;
    for (size_t i = 0; i < items.size(); i++)
    {
      pickedUp += items[i].pickedUp;
      usedInWall += items[i].usedInWall;
    }
    row.push_back(pickedUp);
    row.push_back(usedInWall);
  }
  else if (metric == HeroMetricFood)
  {
    HeroStatsNourishment food = stats.Nourishment(party, level);
    int values[7] = {stats.Eaten(party, level), stats.Drunk(party, level) + food.fountainDrinks, food.fountainDrinks,
                     food.foodGained, food.foodWasted, food.waterGained, food.waterWasted};
    row.assign(values, values + 7);
  }
  else if (metric == HeroMetricDungeon)
  {
    int values[8] = {stats.DoorsBashed(party, level), stats.DoorsBashed(party, level, HeroDoorSmashed),
                     stats.DoorsBashed(party, level, HeroDoorShot), stats.DoorsBashed(party, level, HeroDoorBySpell),
                     stats.DoorsOpenedByZo(party, level), stats.DoorsOpenedByButton(level),
                     stats.DoorsClosedByButton(level), stats.ChestsOpened(level)};
    row.assign(values, values + 8);
  }
  else if (metric == HeroMetricReading)
  {
    row.push_back(stats.ScrollsRead(level));
    row.push_back(stats.WallTextsRead(level));
  }
  else if (metric == HeroMetricChampions)
  {
    row.push_back(stats.ChampionsSeen(level));
    row.push_back(stats.ChampionsViewed(level));
  }
  return row;
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
// Items

static bool ItemLineBefore(const HeroStatsItemLine &a, const HeroStatsItemLine &b)
{
  if (a.pickedUp != b.pickedUp) return a.pickedUp > b.pickedUp;
  if (a.usedInWall != b.usedInWall) return a.usedInWall > b.usedInWall;
  if (a.drunk != b.drunk) return a.drunk > b.drunk;
  if (a.eaten != b.eaten) return a.eaten > b.eaten;
  return a.name < b.name;
}

std::vector<HeroStatsItemLine> HeroStatsPages_Items(const HeroStats &stats, int level, const HeroStatsNames &names)
{
  std::map<std::string, size_t> lineOf;
  std::vector<HeroStatsItemLine> lines;
  auto lineFor = [&](int item) -> HeroStatsItemLine & {
    std::string name = names.weapon(item);
    std::map<std::string, size_t>::iterator found = lineOf.find(name);
    if (found == lineOf.end())
    {
      HeroStatsItemLine empty = {name, 0, 0, 0, 0};
      lines.push_back(empty);
      found = lineOf.insert(std::make_pair(name, lines.size() - 1)).first;
    }
    return lines[found->second];
  };
  std::vector<HeroStatsItemRow> rows = stats.Items(level);
  for (size_t r = 0; r < rows.size(); r++)
  {
    HeroStatsItemLine &line = lineFor(rows[r].item);
    line.pickedUp += rows[r].pickedUp;
    line.usedInWall += rows[r].usedInWall;
  }
  std::vector<HeroStatsConsumedRow> consumed = stats.Consumed(HERO_STATS_PARTY, level);
  for (size_t r = 0; r < consumed.size(); r++)
  {
    HeroStatsItemLine &line = lineFor(consumed[r].item);
    line.drunk += consumed[r].drunk;
    line.eaten += consumed[r].eaten;
  }
  std::sort(lines.begin(), lines.end(), ItemLineBefore);
  return lines;
}

HeroStatsItemLine HeroStatsPages_AllItems(const std::vector<HeroStatsItemLine> &lines)
{
  HeroStatsItemLine all = {"All items", 0, 0, 0, 0};
  for (size_t l = 0; l < lines.size(); l++)
  {
    all.pickedUp += lines[l].pickedUp;
    all.usedInWall += lines[l].usedInWall;
    all.drunk += lines[l].drunk;
    all.eaten += lines[l].eaten;
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

// One line per source; monster attacks in one line, a monster's missiles by kind and caster;
// friendly fire and clouds by causer
static void AddHarm(HeroStatsHeroPage *page, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                    HeroStatsChampion who, int level, const HeroStatsNames &names)
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
      // The store keeps no variant of the caster: the first name of its type
      std::string caster;
      if (row.source == HeroHarmMonsterMissile && row.monsterType >= 0) caster = names.monster(row.monsterType, 0);
      text = HeroStatsView_HarmName(row, causer, caster);
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

// What the champion (or the party) ate and drank, by item; the fountain as the last drunk line
static void AddFood(HeroStatsHeroPage *page, const HeroStats &stats, HeroStatsChampion who, int level,
                    const HeroStatsNames &names)
{
  page->nourishment = stats.Nourishment(who, level);
  page->eatenTotal = page->drunkTotal = 0;
  std::vector<HeroStatsConsumedRow> rows = stats.Consumed(who, level);
  for (size_t r = 0; r < rows.size(); r++)
  {
    std::string name = names.weapon(rows[r].item);
    if (rows[r].eaten > 0) AddLine(&page->eaten, name, rows[r].eaten, false);
    if (rows[r].drunk > 0) AddLine(&page->drunk, name, rows[r].drunk, false);
    page->eatenTotal += rows[r].eaten;
    page->drunkTotal += rows[r].drunk;
  }
  std::stable_sort(page->eaten.begin(), page->eaten.end(), LineBefore);
  std::stable_sort(page->drunk.begin(), page->drunk.end(), LineBefore);
  if (page->nourishment.fountainDrinks > 0)
  {
    HeroStatsAmountLine fountain = {"Fountain", page->nourishment.fountainDrinks, false};
    page->drunk.push_back(fountain);
    page->drunkTotal += fountain.amount;
  }
}

HeroStatsHeroPage HeroStatsPages_Hero(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                      HeroStatsChampion who, int level, const HeroStatsNames &names)
{
  HeroStatsHeroPage page;
  AddAttacks(&page, stats, who, level, names);
  AddSpells(&page, stats, who, level);
  AddHarm(&page, stats, columns, who, level, names);
  AddHealing(&page, stats, columns, who, level);
  AddFood(&page, stats, who, level, names);
  AddHardship(&page, stats, columns, who, level, names);
  page.summaries = HeroStatsView_Summaries(stats, who, level);
  return page;
}
