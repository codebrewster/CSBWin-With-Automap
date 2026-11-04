// HeroStatsPages.cpp : the numbers of the statistics pages, made from a HeroStats.
#include "HeroStatsPages.h"

#include <algorithm>
#include <map>
#include <utility>

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
  // Grouped by weapon; throwing it is one of its options (THROW, Throw (by hand)). Rune spells are
  // in Spells.
  std::vector<HeroStatsAttackRow> attacks = stats.Attacks(who, level);
  std::map<int, size_t> weaponAt;
  for (size_t i = 0; i < attacks.size(); i++)
  {
    const HeroStatsAttackRow &a = attacks[i];
    if (a.weapon == HERO_STATS_NO_WEAPON) continue;
    std::map<int, size_t>::iterator found = weaponAt.find(a.weapon);
    if (found == weaponAt.end())
    {
      HeroStatsWeaponLine w;
      w.name = names.weapon(a.weapon);
      w.tries = w.hits = w.damage = w.kills = w.uses = 0;
      w.id = a.weapon + 2;
      page->weapons.push_back(w);
      found = weaponAt.insert(std::make_pair(a.weapon, page->weapons.size() - 1)).first;
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
    // Cleared runes cast nothing: they are a part of Mana spent, not a spell
    if (row.spell == HERO_SPELL_RUNES_CLEARED) continue;
    // Mana: every cast's runes, the failed ones too; Wasted is the failed casts' part of it
    HeroStatsSpellLine line = {HeroStatsView_SpellName(row.spell), row.casts + row.failed, row.casts, row.failed,
                               row.mana + row.wasted, row.wasted, row.damage, row.kills};
    page->spells.push_back(line);
  }
}

// One line per source; monster attacks in one line, a monster's missiles by kind and caster;
// friendly fire and clouds by causer
static void AddHarm(HeroStatsHeroPage *page, const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                    HeroStatsChampion who, int level, const HeroStatsNames &names, bool isParty)
{
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
                       HeroStatsChampion who, int level, bool isParty)
{
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

// partyNames: name the lines as the Party's page does (no "own", no "from"), so that a
// champion's lines can be matched with the party's
static HeroStatsHeroPage BuildHero(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                   HeroStatsChampion who, int level, const HeroStatsNames &names, bool partyNames)
{
  HeroStatsHeroPage page;
  AddAttacks(&page, stats, who, level, names);
  AddSpells(&page, stats, who, level);
  AddHarm(&page, stats, columns, who, level, names, partyNames);
  AddHealing(&page, stats, columns, who, level, partyNames);
  AddFood(&page, stats, who, level, names);
  AddHardship(&page, stats, columns, who, level, names);
  return page;
}

// Each column's part of the party's amount lines, matched by text
static void SplitAmounts(std::vector<HeroStatsAmountLine> *party, const std::vector<HeroStatsAmountLine> &champion,
                         size_t column, size_t columnCount)
{
  for (size_t i = 0; i < party->size(); i++)
  {
    HeroStatsAmountLine &line = (*party)[i];
    line.byColumn.resize(columnCount, 0);
    for (size_t c = 0; c < champion.size(); c++)
      if (champion[c].text == line.text) line.byColumn[column] += champion[c].amount;
  }
}

HeroStatsHeroPage HeroStatsPages_Hero(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                      HeroStatsChampion who, int level, const HeroStatsNames &names)
{
  HeroStatsHeroPage page = BuildHero(stats, columns, who, level, names, who == HERO_STATS_PARTY);
  if (who != HERO_STATS_PARTY) return page;
  size_t n = columns.size();
  HeroStatsOptionLine noAttacks = {"", 0, 0, 0, 0, 0};
  HeroStatsSpellCounts noCasts = {0, 0, 0, 0, 0, 0, 0};
  for (size_t w = 0; w < page.weapons.size(); w++) page.weapons[w].byColumn.assign(n, noAttacks);
  for (size_t s = 0; s < page.spells.size(); s++) page.spells[s].byColumn.assign(n, noCasts);
  for (size_t c = 0; c < n; c++)
  {
    HeroStatsHeroPage one = BuildHero(stats, columns, columns[c].id, level, names, true);
    for (size_t w = 0; w < page.weapons.size(); w++)
      for (size_t o = 0; o < one.weapons.size(); o++)
        if (one.weapons[o].id == page.weapons[w].id)
        {
          HeroStatsOptionLine &mine = page.weapons[w].byColumn[c];
          mine.tries += one.weapons[o].tries;
          mine.hits += one.weapons[o].hits;
          mine.damage += one.weapons[o].damage;
          mine.kills += one.weapons[o].kills;
          mine.uses += one.weapons[o].uses;
        }
    for (size_t s = 0; s < page.spells.size(); s++)
      for (size_t o = 0; o < one.spells.size(); o++)
        if (one.spells[o].name == page.spells[s].name)
        {
          HeroStatsSpellCounts &mine = page.spells[s].byColumn[c];
          const HeroStatsSpellLine &theirs = one.spells[o];
          mine.casts += theirs.casts;
          mine.successful += theirs.successful;
          mine.failed += theirs.failed;
          mine.mana += theirs.mana;
          mine.wasted += theirs.wasted;
          mine.damage += theirs.damage;
          mine.kills += theirs.kills;
        }
    SplitAmounts(&page.taken, one.taken, c, n);
    SplitAmounts(&page.healing, one.healing, c, n);
    SplitAmounts(&page.eaten, one.eaten, c, n);
    SplitAmounts(&page.drunk, one.drunk, c, n);
  }
  return page;
}

// ---------------------------------------------------------------------------------------------
// Topic pages

std::vector<HeroStatsDetail> HeroStatsPages_Details(int topic, HeroStatsChampion who)
{
  std::vector<HeroStatsDetail> details;
  switch (topic)
  {
  case HeroTopicCombat: details.push_back(HeroDetailWeapons); break;
  case HeroTopicMagic: details.push_back(HeroDetailSpells); break;
  case HeroTopicSurvival:
    details.push_back(HeroDetailHarm);
    details.push_back(HeroDetailHealing);
    details.push_back(HeroDetailDeaths);
    break;
  case HeroTopicSupplies:
    details.push_back(HeroDetailConsumed);
    if (who == HERO_STATS_PARTY) details.push_back(HeroDetailItems);
    break;
  case HeroTopicMonsters: details.push_back(HeroDetailMonsters); break;
  case HeroTopicExperience:
    details.push_back(HeroDetailSkills);
    details.push_back(HeroDetailXpSources);
    details.push_back(HeroDetailRankUps);
    break;
  case HeroTopicRecords: details.push_back(HeroDetailRecords); break;
  default: break;
  }
  return details;
}


// ---------------------------------------------------------------------------------------------
// Experience and Records

static const char *const DOT = " \xc2\xb7 ";

static std::string SkillNote(int skill)
{
  int cls = HeroXp_ClassOf(skill);
  if (skill < HERO_XP_CLASS_COUNT) return HeroXp_ClassName(cls);
  return std::string(HeroXp_ClassName(cls)) + DOT + HeroXp_SkillName(skill);
}

static std::string OptionName(const HeroStatsNames &names, int option)
{
  std::string name = option == HERO_STATS_THROW_OPTION ? "Throw (by hand)" : names.option(option);
  return name.empty() ? "#" + std::to_string(option) : name;
}

// "SWING with DIAMOND EDGE", "Ful Ir Fireball", "Mummy", ""
static std::string SourceText(const HeroXpSource &source, const HeroStatsNames &names)
{
  switch (source.kind)
  {
  case HeroXpAttack: case HeroXpOtherAction:
  {
    std::string option = OptionName(names, source.option);
    if (source.weapon == HERO_STATS_NO_WEAPON) return option;
    return option + " with " + names.weapon(source.weapon);
  }
  case HeroXpSpell:
  {
    std::string missile = HeroStatsView_MissileName(source.spell);
    return missile.empty() ? HeroStatsView_SpellName(source.spell) : missile;
  }
  case HeroXpMonster: return source.monsterType >= 0 ? names.monster(source.monsterType, 0) : "";
  default: return "";
  }
}

// A tree of experience lines, built from the rows of who and of each column
struct XpNode
{
  std::string name, note;
  int amount;
  std::vector<int> byColumn;
  std::vector<std::string> order;
  std::map<std::string, XpNode> children;
};

static XpNode &Child(XpNode &parent, const std::string &key, const std::string &name, const std::string &note, size_t columns)
{
  std::map<std::string, XpNode>::iterator found = parent.children.find(key);
  if (found != parent.children.end()) return found->second;
  XpNode node;
  node.name = name;
  node.note = note;
  node.amount = 0;
  node.byColumn.assign(columns, 0);
  parent.order.push_back(key);
  return parent.children[key] = node;
}

static const char *const SOURCE_NAMES[5] = {"Attacks", "Monsters attacking you", "Other actions", "Spells", "Other"};

static void AddXpRow(XpNode &root, const HeroXpRow &row, int column, size_t columns, const HeroStatsNames &names)
{
  int top;
  switch (row.source.kind)
  {
  case HeroXpAttack: top = 0; break;
  case HeroXpMonster: top = 1; break;
  case HeroXpOtherAction: top = 2; break;
  case HeroXpSpell: top = 3; break;
  default: top = 4; break;
  }
  std::vector<XpNode *> path;
  XpNode &first = Child(root, std::to_string(top), SOURCE_NAMES[top], "", columns);
  path.push_back(&first);
  if (top == 0)
  {
    XpNode &weapon = Child(first, "w" + std::to_string(row.source.weapon), names.weapon(row.source.weapon), "", columns);
    path.push_back(&weapon);
    path.push_back(&Child(weapon, "o" + std::to_string(row.source.option) + "s" + std::to_string(row.skill),
                          OptionName(names, row.source.option), SkillNote(row.skill), columns));
  }
  else if (top == 3)
  {
    int cls = HeroXp_ClassOf(row.skill);
    XpNode &spellClass = Child(first, "c" + std::to_string(cls), HeroXp_ClassName(cls), "", columns);
    path.push_back(&spellClass);
    std::string note = row.skill < HERO_XP_CLASS_COUNT ? HeroXp_ClassName(cls) : HeroXp_SkillName(row.skill);
    path.push_back(&Child(spellClass, "s" + std::to_string(row.source.spell) + "k" + std::to_string(row.skill),
                          HeroStatsView_SpellName(row.source.spell), note, columns));
  }
  for (size_t i = 0; i < path.size(); i++)
  {
    if (column < 0) path[i]->amount += row.amount;
    else path[i]->byColumn[column] += row.amount;
  }
}

static void WriteXpLines(const XpNode &node, int depth, const std::string &path, std::vector<HeroStatsXpLine> *out)
{
  // Most first, like the other tables; the top lines keep their order
  std::vector<std::string> keys = node.order;
  if (depth > 0)
    std::stable_sort(keys.begin(), keys.end(), [&](const std::string &a, const std::string &b) {
      return node.children.at(a).amount > node.children.at(b).amount;
    });
  for (size_t i = 0; i < keys.size(); i++)
  {
    const XpNode &child = node.children.at(keys[i]);
    if (depth > 0 && child.amount <= 0) continue;
    std::string childPath = path + "/" + keys[i];
    HeroStatsXpLine line = {child.name, child.note, depth, (int)(std::hash<std::string>()(childPath) & 0x7fffffff),
                            child.amount, child.byColumn, !child.children.empty()};
    out->push_back(line);
    WriteXpLines(child, depth + 1, childPath, out);
  }
}

static std::vector<HeroStatsXpLine> XpSources(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                              HeroStatsChampion who, int level, const HeroStatsNames &names)
{
  bool isParty = who == HERO_STATS_PARTY;
  size_t columnCount = isParty ? columns.size() : 1;
  XpNode root;
  for (int top = 0; top < 5; top++) Child(root, std::to_string(top), SOURCE_NAMES[top], "", columnCount);
  std::vector<HeroXpRow> rows = stats.Experience().Rows(who, level);
  for (size_t r = 0; r < rows.size(); r++) AddXpRow(root, rows[r], -1, columnCount, names);
  // A champion's page: the party's amount of each line, for the share
  if (!isParty)
  {
    std::vector<HeroXpRow> party = stats.Experience().Rows(HERO_STATS_PARTY, level);
    for (size_t r = 0; r < party.size(); r++) AddXpRow(root, party[r], 0, columnCount, names);
    columnCount = 0;
  }
  for (size_t c = 0; c < columnCount; c++)
  {
    std::vector<HeroXpRow> own = stats.Experience().Rows(columns[c].id, level);
    for (size_t r = 0; r < own.size(); r++) AddXpRow(root, own[r], (int)c, columnCount, names);
  }
  std::vector<HeroStatsXpLine> lines;
  WriteXpLines(root, 0, "", &lines);
  return lines;
}

// The rank now; a champion the game no longer has: where the statistics last saw it
static int RankOf(const HeroStats &stats, HeroStatsChampion who, int cls, const HeroStatsNames &names, int *percent)
{
  *percent = 0;
  int rank = names.rankNow != NULL ? names.rankNow(who, cls, percent) : -1;
  if (rank >= 0) return rank;
  *percent = 0;
  int start = stats.Experience().StartRank(who, cls);
  if (start < 0) return -1;
  return start + stats.Experience().RanksGained(who, HERO_STATS_WHOLE_RUN, cls);
}

static std::vector<HeroStatsClassLine> XpClasses(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                                 HeroStatsChampion who, int level, const HeroStatsNames &names)
{
  bool isParty = who == HERO_STATS_PARTY;
  const HeroStatsExperience &xp = stats.Experience();
  std::vector<HeroStatsClassLine> classes;
  for (int cls = 0; cls < HERO_XP_CLASS_COUNT; cls++)
  {
    HeroStatsClassLine line;
    line.name = HeroXp_ClassName(cls);
    line.amount = xp.Gained(who, level, cls);
    line.ranksGained = xp.RanksGained(who, level, cls);
    line.best = who;
    line.startRank = isParty ? -1 : xp.StartRank(who, cls);
    line.rankNow = -1;
    line.percentToNext = 0;
    if (isParty)
    {
      for (size_t c = 0; c < columns.size(); c++)
      {
        int percent;
        int rank = RankOf(stats, columns[c].id, cls, names, &percent);
        if (rank > line.rankNow) { line.rankNow = rank; line.percentToNext = percent; line.best = columns[c].id; }
        line.byColumn.push_back(xp.Gained(columns[c].id, level, cls));
      }
    }
    else
      line.rankNow = RankOf(stats, who, cls, names, &line.percentToNext);
    for (int skill = HERO_XP_CLASS_COUNT + 4 * cls; skill < HERO_XP_CLASS_COUNT + 4 * cls + 4; skill++)
    {
      HeroStatsXpLine skillLine = {HeroXp_SkillName(skill), "", 1, skill, xp.SkillGained(who, level, skill), {}, false};
      for (size_t c = 0; isParty && c < columns.size(); c++) skillLine.byColumn.push_back(xp.SkillGained(columns[c].id, level, skill));
      line.skills.push_back(skillLine);
    }
    if (xp.SkillGained(who, level, cls) > 0)
    {
      HeroStatsXpLine itself = {line.name + " itself", "", 1, cls, xp.SkillGained(who, level, cls), {}, false};
      for (size_t c = 0; isParty && c < columns.size(); c++) itself.byColumn.push_back(xp.SkillGained(columns[c].id, level, cls));
      line.skills.push_back(itself);
    }
    classes.push_back(line);
  }
  return classes;
}

static std::vector<HeroStatsRankUpLine> XpRankUps(const HeroStats &stats, HeroStatsChampion who, int level, const HeroStatsNames &names)
{
  std::vector<HeroStatsRankUpLine> lines;
  std::vector<HeroRankUp> ups = stats.Experience().RankUps(who);
  for (size_t i = 0; i < ups.size(); i++)
  {
    if (!HeroStats::LevelMatches(level, ups[i].level)) continue;
    std::string before = HeroXp_RankName(ups[i].rank - 1);
    std::string rank = before.empty() ? HeroXp_RankName(ups[i].rank) : before + " \xe2\x86\x92 " + HeroXp_RankName(ups[i].rank);
    HeroStatsRankUpLine line = {ups[i].who, HeroXp_ClassName(ups[i].cls), rank, SourceText(ups[i].source, names), ups[i].level};
    lines.push_back(line);
  }
  return lines;
}

static const char *RecordGroup(HeroRecordKind kind)
{
  if (kind <= HeroRecKillStreak) return "Dealt";
  if (kind <= HeroRecClosestCall) return "Taken";
  if (kind <= HeroRecBiggestXpGain) return "Magic & healing";
  return "Carrying & moving";
}

static std::string ChampionName(const std::vector<HeroStatsColumn> &columns, HeroStatsChampion who)
{
  if (who == HERO_STATS_PARTY) return "party";
  for (size_t c = 0; c < columns.size(); c++)
    if (columns[c].id == who) return columns[c].name;
  return "";
}

static std::string RecordValue(HeroRecordKind kind, const HeroRecordEntry &entry)
{
  HeroStatsNumber number = (HeroStatsNumber)(HeroNumRecFirst + kind);
  std::string text = HeroStatsCatalogue_Format(number, entry.value);
  if (kind == HeroRecHeaviestLoad && entry.extra > 0) text += DOT + HeroStatsView_Percent(entry.value, entry.extra);
  return text;
}

static void DescribeRecord(HeroRecordKind kind, const HeroRecordEntry &entry, const HeroStatsNames &names, HeroStatsRecordLine *line)
{
  std::string monster = entry.monsterType >= 0 ? names.monster(entry.monsterType, entry.variant) : "";
  switch (kind)
  {
  case HeroRecBiggestHit: case HeroRecBiggestSpellHit: case HeroRecBiggestSpellTotal: case HeroRecMostKillsAtOnce:
    line->how = SourceText(entry.how, names);
    line->whom = entry.count > 1 && !monster.empty() ? std::to_string(entry.count) + " \xc3\x97 " + monster : monster;
    break;
  case HeroRecKillStreak:
  {
    for (int i = 0; i < 2; i++)
      if (entry.kinds[i][0] >= 0)
        line->whom += (i > 0 ? ", " : "") + std::to_string(entry.kinds[i][1]) + " " + names.monster(entry.kinds[i][0], 0);
    if (entry.firstLevel != entry.level) line->how = "Lv " + std::to_string(entry.firstLevel) + " \xe2\x86\x92 " + std::to_string(entry.level);
    break;
  }
  case HeroRecBiggestHitTaken:
    line->how = "Monster attack";
    line->whom = monster;
    break;
  case HeroRecBiggestSpellHitTaken:
    line->how = SourceText(entry.how, names);
    line->whom = monster;
    break;
  case HeroRecClosestCall:
  {
    HeroStatsHarmRow row = {(HeroHarmSource)entry.extra, HERO_STATS_NOBODY, entry.monsterType, 0, entry.how.spell};
    line->how = HeroStatsView_HarmName(row, NULL, (HeroHarmSource)entry.extra == HeroHarmMonsterMissile ? monster : "");
    if ((HeroHarmSource)entry.extra == HeroHarmMonster) line->how = "Monster attack";
    if ((HeroHarmSource)entry.extra == HeroHarmMonster || (HeroHarmSource)entry.extra == HeroHarmMonsterMissile) line->whom = monster;
    break;
  }
  case HeroRecBiggestHeal:
    line->how = entry.how.option == HeroHealPotion ? "Potion" : entry.how.option == HeroHealAction ? "Heal action" : "Regeneration";
    break;
  case HeroRecMostManaOnSpell: line->how = SourceText(entry.how, names); break;
  case HeroRecBiggestXpGain:
    line->how = SourceText(entry.how, names);
    line->whom = SkillNote(entry.extra);
    break;
  case HeroRecLongestFall: case HeroRecLongestTeleportChain:
    if (entry.firstLevel != entry.level) line->how = "Lv " + std::to_string(entry.firstLevel) + " \xe2\x86\x92 " + std::to_string(entry.level);
    break;
  default: break;
  }
}

static std::vector<HeroStatsRecordLine> RecordLines(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                                    HeroStatsChampion who, int level, const HeroStatsNames &names)
{
  bool isParty = who == HERO_STATS_PARTY;
  std::vector<HeroStatsRecordLine> lines;
  for (int k = 0; k < HERO_RECORD_COUNT; k++)
  {
    HeroRecordKind kind = (HeroRecordKind)k;
    if (!isParty && HeroRecord_PartyOnly(kind)) continue;
    HeroStatsRecordLine line;
    line.kind = kind;
    line.group = RecordGroup(kind);
    line.label = HeroStatsCatalogue_Info((HeroStatsNumber)(HeroNumRecFirst + k)).label;
    line.level = -1;
    line.held = false;
    line.partyHolder = HERO_STATS_NOBODY;
    HeroRecordEntry entry;
    line.has = stats.Records().Best(kind, who, level, &entry);
    line.value = line.has ? RecordValue(kind, entry) : "-";
    if (line.has)
    {
      line.who = ChampionName(columns, entry.who);
      line.level = entry.level;
      line.when = "tick " + HeroStatsView_Number((int)entry.time);
      DescribeRecord(kind, entry, names, &line);
    }
    HeroRecordEntry party;
    if (stats.Records().Best(kind, HERO_STATS_PARTY, level, &party))
    {
      line.partyValue = RecordValue(kind, party);
      line.partyHolder = party.who;
      // The champion's own streak can be longer than the party's; the party's own records are no champion's
      line.held = !isParty && line.has && !HeroRecord_PartyHasItsOwn(kind) && party.who == who;
    }
    lines.push_back(line);
  }
  return lines;
}

HeroStatsTopicPage HeroStatsPages_Topic(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                        HeroStatsChampion who, int level, int topic, const HeroStatsNames &names)
{
  HeroStatsTopicPage page;
  const HeroStatsTopicInfo &info = HeroStatsCatalogue_Topic(topic);
  bool isParty = who == HERO_STATS_PARTY;
  page.record = HeroStatsCatalogue_Record(stats, who, level);
  HeroStatsRecord party = isParty ? page.record : HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, level);
  std::vector<HeroStatsRecord> perColumn;
  if (isParty)
    for (size_t c = 0; c < columns.size(); c++) perColumn.push_back(HeroStatsCatalogue_Record(stats, columns[c].id, level));

  // A champion's Overview: their own damage, kills, harm and deaths (tiles explored is the party's)
  std::vector<HeroStatsNumber> tiles = info.tiles;
  if (topic == HeroTopicOverview && !isParty)
    tiles = HeroStatsCatalogue_ChampionTiles();
  for (size_t t = 0; t < tiles.size() && page.tiles.size() < 4; t++)
    if (HeroStatsCatalogue_Applies(tiles[t], who, level)) page.tiles.push_back(tiles[t]);

  for (size_t r = 0; r < info.rows.size(); r++)
  {
    HeroStatsNumber number = info.rows[r].number;
    if (!HeroStatsCatalogue_Applies(number, who, level)) continue;
    HeroStatsNumbersRow row;
    row.number = number;
    row.depth = info.rows[r].depth;
    row.partyOnly = HeroStatsCatalogue_Info(number).partyOnly;
    row.party = party.value[number];
    if (!isParty) row.byColumn.push_back(page.record.value[number]);
    else if (!row.partyOnly)
      for (size_t c = 0; c < perColumn.size(); c++) row.byColumn.push_back(perColumn[c].value[number]);
    page.numbers.push_back(row);
  }

  page.details = HeroStatsPages_Details(topic, who);
  for (size_t d = 0; d < page.details.size(); d++)
  {
    HeroStatsDetail detail = page.details[d];
    if (detail == HeroDetailMonsters)
    {
      page.monsters = HeroStatsPages_Monsters(stats, columns, level, names);
      // A champion's own kills only: the monsters they killed, in their column
      if (!isParty)
      {
        std::vector<HeroStatsMonsterLine> own;
        size_t column = 0;
        while (column < columns.size() && columns[column].id != who) column++;
        for (size_t l = 0; l < page.monsters.size(); l++)
        {
          int kills = column < columns.size() ? page.monsters[l].byColumn[column] : 0;
          if (kills <= 0) continue;
          HeroStatsMonsterLine line = page.monsters[l];
          line.byColumn.assign(1, kills);
          line.door = line.pit = line.other = 0;
          line.total = kills;
          own.push_back(line);
        }
        std::stable_sort(own.begin(), own.end(), [](const HeroStatsMonsterLine &a, const HeroStatsMonsterLine &b) { return a.total > b.total; });
        page.monsters = own;
      }
    }
    else if (detail == HeroDetailItems) page.items = HeroStatsPages_Items(stats, level, names);
    else if (detail == HeroDetailSkills) page.classes = XpClasses(stats, columns, who, level, names);
    else if (detail == HeroDetailXpSources) page.sources = XpSources(stats, columns, who, level, names);
    else if (detail == HeroDetailRankUps) page.rankUps = XpRankUps(stats, who, level, names);
    else if (detail == HeroDetailRecords) page.records = RecordLines(stats, columns, who, level, names);
  }
  if (topic == HeroTopicCombat || topic == HeroTopicMagic || topic == HeroTopicSurvival || topic == HeroTopicSupplies)
    page.hero = HeroStatsPages_Hero(stats, columns, who, level, names);
  page.summary = HeroStatsCatalogue_Summary(topic, page.record, who, level);
  return page;
}

std::vector<HeroStatsCard> HeroStatsPages_Cards(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level)
{
  std::vector<HeroStatsCard> cards;
  int partyDamage = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, level).value[HeroNumDamage];
  for (size_t c = 0; c < columns.size(); c++)
  {
    HeroStatsCard card;
    card.column = columns[c];
    card.record = HeroStatsCatalogue_Record(stats, columns[c].id, level);
    int damage = card.record.value[HeroNumDamage];
    card.damagePercent = partyDamage > 0 ? (int)(((long long)damage * 200 + partyDamage) / (partyDamage * 2)) : 0;
    cards.push_back(card);
  }
  return cards;
}

// ---------------------------------------------------------------------------------------------
// Per level

static std::vector<int> LevelsAndTheRun(const HeroStats &stats)
{
  std::vector<int> levels = stats.VisitedLevels();
  levels.push_back(HERO_STATS_WHOLE_RUN);
  return levels;
}

static std::vector<int> ShareOf(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns, int level,
                                HeroStatsNumber number)
{
  std::vector<int> share;
  for (size_t c = 0; c < columns.size(); c++)
    share.push_back(HeroStatsCatalogue_Value(stats, columns[c].id, level, number));
  return share;
}

HeroStatsPerLevel HeroStatsPages_PerLevelTotals(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                                HeroStatsChampion who, int topic)
{
  HeroStatsPerLevel table;
  table.hasShare = false;
  table.shareNumber = HeroNumDamage;
  const HeroStatsTopicInfo &info = HeroStatsCatalogue_Topic(topic);
  // Any level but the whole run: a number kept for the whole run only has no column here
  const int A_LEVEL = 0;
  for (size_t r = 0; r < info.rows.size(); r++)
  {
    HeroStatsNumber number = info.rows[r].number;
    if (info.rows[r].depth != 0 || !HeroStatsCatalogue_Applies(number, who, A_LEVEL)) continue;
    table.numbers.push_back(number);
    if (who == HERO_STATS_PARTY && !table.hasShare && !HeroStatsCatalogue_Info(number).partyOnly &&
        HeroStatsCatalogue_Summable(number))
    {
      table.hasShare = true;
      table.shareNumber = number;
    }
  }
  std::vector<int> levels = LevelsAndTheRun(stats);
  for (size_t l = 0; l < levels.size(); l++)
  {
    HeroStatsLevelRow row;
    row.level = levels[l];
    // Only the numbers shown: a whole record per level and champion would scan the store for all
    for (size_t n = 0; n < table.numbers.size(); n++)
      row.values.push_back(HeroStatsCatalogue_Value(stats, who, row.level, table.numbers[n]));
    if (table.hasShare) row.share = ShareOf(stats, columns, row.level, table.shareNumber);
    table.rows.push_back(row);
  }
  return table;
}

HeroStatsPerLevel HeroStatsPages_PerLevelGrid(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                              HeroStatsNumber number)
{
  HeroStatsPerLevel table;
  table.numbers.push_back(number);
  table.shareNumber = number;
  table.hasShare = HeroStatsCatalogue_Summable(number);
  std::vector<int> levels = LevelsAndTheRun(stats);
  for (size_t l = 0; l < levels.size(); l++)
  {
    HeroStatsLevelRow row;
    row.level = levels[l];
    row.values = ShareOf(stats, columns, row.level, number);
    if (table.hasShare) row.share = row.values;
    row.values.push_back(HeroStatsCatalogue_Value(stats, HERO_STATS_PARTY, row.level, number));
    table.rows.push_back(row);
  }
  return table;
}
