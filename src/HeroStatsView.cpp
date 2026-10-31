#include "HeroStatsView.h"
#include <algorithm>

static std::string thousandsSeparator = ",";

void HeroStatsView_SetThousandsSeparator(const std::string &separator)
{
  thousandsSeparator = separator;
}

const std::string &HeroStatsView_ThousandsSeparator(void)
{
  return thousandsSeparator;
}

std::string HeroStatsView_SeparatorOfSetting(const std::string &setting)
{
  if (setting == "dot") return ".";
  if (setting == "space") return " ";
  if (setting == "apostrophe") return "'";
  if (setting == "none") return "";
  return ",";
}

std::string HeroStatsView_Number(int value)
{
  std::string digits = std::to_string(value < 0 ? -(long long)value : (long long)value);
  std::string out;
  for (size_t i = 0; i < digits.size(); i++)
  {
    if (i > 0 && (digits.size() - i) % 3 == 0) out += thousandsSeparator;
    out += digits[i];
  }
  return value < 0 ? "-" + out : out;
}

std::string HeroStatsView_Percent(int part, int whole)
{
  if (whole == 0) return "-";
  long long percent = ((long long)part * 100 + whole / 2) / whole;
  return std::to_string(percent) + "%";
}

std::vector<HeroStatsColumn> HeroStatsView_Columns(const HeroStats &stats,
                                                   const std::vector<HeroStatsChampion> &inParty)
{
  std::vector<HeroStatsColumn> columns;
  const std::vector<HeroStatsChampionInfo> &met = stats.Champions();
  for (size_t i = 0; i < inParty.size(); i++)
    for (size_t j = 0; j < met.size(); j++)
      if (met[j].id == inParty[i])
      {
        HeroStatsColumn column = {met[j].id, met[j].name, false};
        columns.push_back(column);
        break;
      }
  for (size_t j = 0; j < met.size(); j++)
  {
    if (std::find(inParty.begin(), inParty.end(), met[j].id) != inParty.end()) continue;
    HeroStatsColumn column = {met[j].id, met[j].name, true};
    columns.push_back(column);
  }
  return columns;
}

HeroStatsPicker HeroStatsView_Picker(const HeroStats &stats, int shownLevel)
{
  HeroStatsPicker picker;
  picker.levels = stats.VisitedLevels();
  picker.stepper = picker.levels.size() > 16;
  picker.previous = picker.next = -2;
  if (picker.levels.empty()) return picker;
  if (shownLevel == HERO_STATS_WHOLE_RUN)
  {
    picker.previous = picker.levels.back();
    picker.next = picker.levels.front();
    return picker;
  }
  std::vector<int>::const_iterator at =
      std::lower_bound(picker.levels.begin(), picker.levels.end(), shownLevel);
  size_t i = at - picker.levels.begin();
  if (i > 0) picker.previous = picker.levels[i - 1];
  size_t after = (at != picker.levels.end() && *at == shownLevel) ? i + 1 : i;
  if (after < picker.levels.size()) picker.next = picker.levels[after];
  return picker;
}

static std::string Plural(int count, const char *one, const char *many)
{
  return HeroStatsView_Number(count) + " " + (count == 1 ? one : many);
}

HeroStatsSummaries HeroStatsView_Summaries(const HeroStats &stats, HeroStatsChampion who, int level)
{
  HeroStatsSummaries s;

  // Rune spells (HERO_STATS_NO_WEAPON) are in the Spells section, not here
  // Tries are what the Attacks table shows: the blows and missiles, and the actions that cannot
  // hit (War Cry ...); the hit rate is the blows' and missiles' only
  int tries = 0, uses = 0, hits = 0, damage = 0, kills = 0;
  std::vector<HeroStatsAttackRow> attacks = stats.Attacks(who, level);
  for (size_t i = 0; i < attacks.size(); i++)
  {
    if (attacks[i].weapon == HERO_STATS_NO_WEAPON) continue;
    tries += attacks[i].tries;
    uses += attacks[i].uses;
    hits += attacks[i].hits;
    damage += attacks[i].damage;
    kills += attacks[i].kills;
  }
  s.attacks = HeroStatsView_Number(tries + uses) + " tries · " + HeroStatsView_Percent(hits, tries) +
              " hits · " + HeroStatsView_Number(damage) + " damage · " + HeroStatsView_Number(kills) +
              " kills";

  // Every cast, failed or not, as the Casts column of the Spells table counts them
  int casts = 0, spellDamage = 0, spellKills = 0;
  std::vector<HeroStatsSpellRow> spells = stats.Spells(who, level);
  for (size_t i = 0; i < spells.size(); i++)
  {
    casts += spells[i].casts + spells[i].failed;
    spellDamage += spells[i].damage;
    spellKills += spells[i].kills;
  }
  if (casts == 0)
    s.spells = "none";
  else
    s.spells = Plural(casts, "cast", "casts") + " · " + HeroStatsView_Number(stats.ManaSpent(who, level)) +
               " mana · " + HeroStatsView_Number(spellDamage) + " damage · " +
               HeroStatsView_Number(spellKills) + " kills · " +
               HeroStatsView_Number(stats.ManaWasted(who, level)) + " wasted";

  s.harm = HeroStatsView_Number(stats.Harm(who, level));
  int friendly = stats.FriendlyFireCaused(who, level);
  if (friendly != 0) s.harm += " · friendly fire " + HeroStatsView_Number(friendly);

  s.healing = HeroStatsView_Number(stats.Healed(who, level)) + " healed";
  int given = stats.HealingGiven(who, level);
  if (given != 0) s.healing += " · " + HeroStatsView_Number(given) + " given";

  // With a level picked only the deaths on that level count; wipes and reloads are not kept per
  // level and belong to the Party column of the whole run
  int deaths = 0;
  std::vector<HeroStatsDeath> died = stats.Deaths(who);
  for (size_t i = 0; i < died.size(); i++)
    if (level == HERO_STATS_WHOLE_RUN || died[i].level == level) deaths++;
  bool wholeParty = who == HERO_STATS_PARTY && level == HERO_STATS_WHOLE_RUN;
  int wipes = wholeParty ? stats.PartyWipes() : 0;
  int reloads = wholeParty ? stats.Reloads() : 0;
  std::string parts;
  if (deaths != 0) parts = Plural(deaths, "death", "deaths");
  if (wipes != 0) parts += (parts.empty() ? "" : " · ") + Plural(wipes, "wipe", "wipes");
  if (reloads != 0) parts += (parts.empty() ? "" : " · ") + Plural(reloads, "reload", "reloads");
  s.hardship = parts.empty() ? "never died" : parts;

  HeroStatsNourishment food = stats.Nourishment(who, level);
  int eaten = stats.Eaten(who, level);
  int drunk = stats.Drunk(who, level) + food.fountainDrinks;
  if (eaten == 0 && drunk == 0)
    s.food = "nothing eaten or drunk";
  else
  {
    s.food = HeroStatsView_Number(eaten) + " eaten · " + HeroStatsView_Number(drunk) + " drunk";
    if (food.foodWasted != 0 || food.waterWasted != 0)
      s.food += " · wasted " + HeroStatsView_Number(food.foodWasted) + " food, " +
                HeroStatsView_Number(food.waterWasted) + " water";
  }
  return s;
}

// ---- Spell names (docs/spell-checks.md, Priest and Wizard tables) ----

static const char *const RUNE_NAMES[] = {
    "Lo", "Um", "On", "Ee", "Pal", "Mon",                   // 96-101
    "Ya", "Vi", "Oh", "Ful", "Des", "Zo",                   // 102-107
    "Ven", "Ew", "Kath", "Ir", "Bro", "Gor",                // 108-113
    "Ku", "Ros", "Dain", "Neta", "Ra", "Sar"};              // 114-119
static const int FIRST_RUNE = 96;
static const int RUNE_COUNT = 24;

struct KnownSpell { const char *runes; const char *name; };
static const KnownSpell KNOWN_SPELLS[] = {
    // Priest
    {"Vi", "Health"}, {"Ya", "Stamina"}, {"Zo Bro Ra", "Mana potion"}, {"Ful Bro Ku", "Strength"},
    {"Oh Bro Ros", "Dexterity"}, {"Ya Bro Dain", "Wisdom"}, {"Ya Bro Neta", "Vitality"},
    {"Vi Bro", "Cure poison"}, {"Ya Bro", "Shield"}, {"Ful Bro Neta", "Fire shield"},
    {"Ya Ir", "Shield party"}, {"Des Ir Sar", "Darkness"},
    // Wizard
    {"Ful", "Torch"}, {"Oh Ir Ra", "Light"}, {"Zo", "Open door"}, {"Ya Bro Ros", "Footprints"},
    {"Oh Ew Ra", "See-through-walls"}, {"Oh Ew Sar", "Invisibility"}, {"Zo Ven", "Poison potion"},
    {"Des Ven", "Poison bolt"}, {"Oh Ven", "Poison cloud"}, {"Des Ew", "Weaken nonmaterial"},
    {"Ful Ir", "Fireball"}, {"Oh Kath Ra", "Lightning"}, {"Zo Kath Ra", "Zokathra"}};

static uint32_t KeyOfRunes(const char *runes)
{
  uint32_t key = 0;
  int at = 0;
  std::string word;
  for (const char *p = runes;; p++)
  {
    if (*p == ' ' || *p == 0)
    {
      for (int r = 0; r < RUNE_COUNT; r++)
        if (word == RUNE_NAMES[r]) key |= (uint32_t)(FIRST_RUNE + r) << (8 * at);
      at++;
      word.clear();
      if (*p == 0) break;
    }
    else
      word += *p;
  }
  return key;
}

static std::string RuneText(uint32_t key)
{
  std::string text;
  for (int at = 0; at < 3; at++)
  {
    int byte = (key >> (8 * at)) & 0xff;
    if (byte == 0) break;
    int index = byte - FIRST_RUNE;
    std::string rune = (index >= 0 && index < RUNE_COUNT) ? RUNE_NAMES[index] : "#" + std::to_string(byte);
    text += (text.empty() ? "" : " ") + rune;
  }
  return text;
}

// The name of a known spell, "" when the runes form none we list
static std::string KnownName(uint32_t key)
{
  for (size_t i = 0; i < sizeof(KNOWN_SPELLS) / sizeof(KNOWN_SPELLS[0]); i++)
    if (KeyOfRunes(KNOWN_SPELLS[i].runes) == key) return KNOWN_SPELLS[i].name;
  return "";
}

std::string HeroStatsView_SpellName(uint32_t spell)
{
  if (spell == HERO_STATS_NO_SPELL) return "(no runes)";
  if (spell == HERO_SPELL_RUNES_CLEARED) return "Runes cleared";
  if (spell == HERO_SPELL_SHIELD_ACTION) return "Shield action";
  if (spell == HERO_SPELL_HEAL_ACTION) return "Heal action";
  if (spell == HERO_SPELL_ITEM_SPELLS) return "Spells from items";
  bool noSuch = (spell & HERO_SPELL_NO_SUCH_SPELL) != 0;
  uint32_t key = spell & 0x00ffffff;
  std::string runes = RuneText(key);
  if (noSuch) return runes + " (no spell)";
  std::string name = KnownName(key);
  return name.empty() ? runes : runes + " " + name;
}

// ---- Harm and death names ----

std::string HeroStatsView_MissileName(uint32_t spell)
{
  if ((spell & 0xff000000) != HERO_SPELL_MONSTER_MISSILE) return "";
  switch (spell & 0xff)
  {
  case 0x80: return "Fireball";
  case 0x81: return "Poison blob";
  case 0x82: return "Lightning";
  case 0x83: return "Weaken nonmaterial";
  case 0x84: return "Open door";
  case 0x86: return "Poison bolt";
  case 0x87: return "Poison cloud";
  default: return "Monster missiles";
  }
}

// What a champion's blow was: the spell's name (or its runes), else the weapon, which the view
// does not know
static std::string BlowWhat(const HeroStatsBlow &blow)
{
  if (blow.spell == HERO_STATS_NO_SPELL) return "blow";
  std::string name = KnownName(blow.spell & 0x00ffffff);
  return name.empty() ? RuneText(blow.spell & 0x00ffffff) : name;
}

std::string HeroStatsView_HarmName(const HeroStatsHarmRow &row, const HeroStatsColumn *causer,
                                   const std::string &monsterName)
{
  switch (row.source)
  {
  case HeroHarmMonster: return monsterName.empty() ? "Monster attacks" : monsterName;
  case HeroHarmMonsterMissile:
  {
    std::string missile = HeroStatsView_MissileName(row.spell);
    if (missile.empty()) missile = "Monster missiles";
    return monsterName.empty() ? missile : missile + " (" + monsterName + ")";
  }
  case HeroHarmTrap: return "Traps";
  case HeroHarmFriendlyFire:
  case HeroHarmPoisonCloud:
  {
    std::string base = row.source == HeroHarmFriendlyFire ? "Friendly fire" : "Poison clouds";
    if (causer == 0) return base;
    return base + " (from " + causer->name + ")";
  }
  case HeroHarmPoison: return "Poison";
  case HeroHarmPit: return "Pits";
  case HeroHarmWall: return "Walls";
  case HeroHarmDoor: return "Doors";
  case HeroHarmStarvation: return "Starvation";
  case HeroHarmExhaustion: return "Exhaustion";
  case HeroHarmPotion: return "Harmful potions or food";
  default: return "Unknown";
  }
}

std::string HeroStatsView_DeathName(const HeroStatsDeath &death, const std::string &causerName)
{
  std::string what;
  switch (death.cause)
  {
  case HeroHarmMonster: what = causerName.empty() ? "Monster" : causerName; break;
  case HeroHarmMonsterMissile:
  {
    std::string missile = HeroStatsView_MissileName(death.by.spell);
    if (missile.empty()) what = causerName.empty() ? "Monster" : causerName;
    else what = causerName.empty() ? missile : missile + " from " + causerName;
    break;
  }
  case HeroHarmFriendlyFire:
  {
    std::string blow = BlowWhat(death.by);
    if (death.by.champion == death.who)
      what = "own " + blow;
    else
      what = causerName.empty() ? blow : blow + " from " + causerName;
    break;
  }
  case HeroHarmPoisonCloud:
    if (death.by.champion == HERO_STATS_NOBODY || causerName.empty())
      what = "Poison cloud";
    else if (death.by.champion == death.who)
      what = "own Poison cloud";
    else
      what = "Poison cloud from " + causerName;
    break;
  case HeroHarmPit: what = "Fell into a pit"; break;
  case HeroHarmDoor: what = "Closing door"; break;
  case HeroHarmWall: what = "Ran into a wall"; break;
  case HeroHarmPoison: what = "Poison"; break;
  case HeroHarmStarvation: what = "Starvation"; break;
  case HeroHarmExhaustion: what = "Exhaustion"; break;
  case HeroHarmPotion: what = "Harmful potion or food"; break;
  case HeroHarmTrap: what = "Trap"; break;
  default: what = "Unknown"; break;
  }
  return what + " · Lv " + std::to_string(death.level);
}
