// HeroStatsCatalogue.cpp : every number of the Statistics window, defined once
#include "HeroStatsCatalogue.h"
#include <ctype.h>
#include <algorithm>
#include "HeroStatsView.h"

static const HeroStatsNumberInfo NUMBERS[HERO_NUMBER_COUNT] = {
    // label, help, partyOnly, wholeRunOnly, isBad, isRatio, isTicks
    {"Damage dealt", "", false, false, false, false, false},
    {"Melee", "", false, false, false, false, false},
    {"Ranged", "", false, false, false, false, false},
    {"Magic", "", false, false, false, false, false},
    {"Kills", "", false, false, false, false, false},
    {"Attacks", "Blows and missiles aimed at a monster.", false, false, false, false, false},
    {"Hits", "", false, false, false, false, false},
    {"Hit rate", "", false, false, false, true, false},
    {"Damage per hit", "", false, false, false, true, false},
    {"Other actions", "Actions that cannot hit: War Cry, Calm, Brandish, Blow Horn, Confuse, shields and the like.", false, false, false, false, false},
    {"Friendly fire caused", "", false, false, true, false, false},
    {"Mana generated", "", false, false, false, false, false},
    {"Regeneration", "", false, false, false, false, false},
    {"Mana potions", "", false, false, false, false, false},
    {"Mana spent", "Spent as each rune is entered, on the level it is entered on, whatever becomes of the spell; staffs, wands and the shield and heal actions as they are used. The four lines below add up to it.", false, false, false, false, false},
    {"Spells", "Runes of the spells cast, successful or failed, staffs, wands and the shield and heal actions: the Mana of All spells in the Spells table. On one level the two can differ: here runes count where they were entered, the table counts a spell where it was cast.", false, false, false, false, false},
    {"Successful casts", "Runes of spells that worked, staffs and wands, the shield and heal actions.", false, false, false, false, false},
    {"Wasted on failures", "Runes of spells that failed.", false, false, true, false, false},
    {"Runes cleared", "Runes entered and never cast: cleared, replaced by a new incantation, or lost when the caster died.", false, false, true, false, false},
    {"Not cast yet", "Runes entered and waiting in an incantation, on the level they were entered on.", false, false, false, false, false},
    {"Spells cast", "", false, false, false, false, false},
    {"Successful", "", false, false, false, false, false},
    {"Failed", "", false, false, true, false, false},
    {"Damage taken", "", false, false, false, false, false},
    {"Healed", "", false, false, false, false, false},
    {"Regeneration", "", false, false, false, false, false},
    {"Potions", "", false, false, false, false, false},
    {"Heal action", "", false, false, false, false, false},
    {"Healing given", "HP healed by potions this champion brewed, whoever drank them.", false, false, false, false, false},
    {"Poisoned", "Times poisoned: each new dose of poison counts once, not each tick of it.", false, false, true, false, false},
    {"Wounds", "Parts of the body wounded that were whole before; a part wounded again does not count.", false, false, true, false, false},
    {"Deaths", "", false, false, true, false, false},
    {"Resurrected", "", false, false, false, false, false},
    {"Party wiped out", "", true, true, true, false, false},
    {"Reloads", "", true, true, false, false, false},
    {"Eaten", "", false, false, false, false, false},
    {"Drunk", "", false, false, false, false, false},
    {"Food gained", "", false, false, false, false, false},
    {"Food wasted (over full)", "", false, false, true, false, false},
    {"Water gained", "", false, false, false, false, false},
    {"Water wasted (over full)", "", false, false, true, false, false},
    {"Items picked up", "Items taken from the dungeon into the hand, each only the first time.", true, false, false, false, false},
    {"Used in walls", "Items a wall took: keys in locks, coins and gems in slots.", true, false, false, false, false},
    {"Steps", "Moves you made to another square, stairs included. Falls and teleports are no steps.", true, false, false, false, false},
    {"Tiles walked", "The different squares the party stood on; each counts once.", true, false, false, false, false},
    {"Tiles explored", "The squares the map shows as explored that are not solid wall.", true, false, false, false, false},
    {"Turns", "", true, false, false, false, false},
    {"Wall bumps", "Moves that ran into a wall, a closed door or a solid false wall.", true, false, false, false, false},
    {"Teleports", "", true, false, false, false, false},
    {"Pit falls", "", true, false, false, false, false},
    {"Pit climbs", "", true, false, false, false, false},
    {"Time (ticks)", "", true, false, false, false, true},
    {"Asleep (ticks)", "", true, false, false, false, true},
    {"Doors bashed open", "Doors this champion broke down with an attack, a missile or a spell.", false, false, false, false, false},
    {"Smashed", "", false, false, false, false, false},
    {"Shot open", "", false, false, false, false, false},
    {"By spell", "", false, false, false, false, false},
    {"Opened with Zo", "Closed doors a Zo spell opened; the door stays whole.", false, false, false, false, false},
    {"Opened with button", "", true, false, false, false, false},
    {"Closed with button", "", true, false, false, false, false},
    {"Chests opened", "", true, false, false, false, false},
    {"Scrolls read", "Scrolls read, each the first time it is opened.", true, false, false, false, false},
    {"Wall texts read", "The wall texts the map knows as read.", true, false, false, false, false},
    {"Champions seen", "Champion portraits in mirrors seen face to face; each mirror once.", true, false, false, false, false},
    {"Champions viewed", "Mirrors clicked to look at the champion; each mirror once.", true, false, false, false, false},
    {"Crushed by doors", "", true, false, false, false, false},
    {"Killed by falls", "", true, false, false, false, false},
    {"Other causes", "Clouds nobody is known to have made, traps, other monsters.", true, false, false, false, false},
    {"Experience gained", "Experience as the game added it: with the level's multiplier, the bonus in combat and the dungeon's own rules.", false, false, false, false, false},
    {"Fighter", "", false, false, false, false, false},
    {"Ninja", "", false, false, false, false, false},
    {"Priest", "", false, false, false, false, false},
    {"Wizard", "", false, false, false, false, false},
    {"Ranks gained", "", false, false, false, false, false},
    {"Biggest hit", "The most damage a blow, a thrown item or a shot did to one monster.", false, false, false, false, false, true},
    {"Biggest spell hit", "The most damage one monster took from one spell.", false, false, false, false, false, true},
    {"Biggest spell in all", "The most damage one spell did to a whole group.", false, false, false, false, false, true},
    {"Most kills at once", "Monsters killed by one blow, one explosion or one tick of a cloud.", false, false, false, false, false, true},
    {"Kills without a scratch", "The most kills in a row with no damage taken in between.", false, false, false, false, false, true},
    {"Biggest hit taken", "The most damage one monster attack did, after armour.", false, false, false, false, false, true},
    {"Biggest spell hit taken", "The most damage one monster spell or missile did.", false, false, false, false, false, true},
    {"Closest call", "The fewest hit points left after taking damage and living.", false, false, false, false, false, true},
    {"Biggest heal", "The most hit points gained at once.", false, false, false, false, false, true},
    {"Most mana on one spell", "The mana of the runes of one spell that worked.", false, false, false, false, false, true},
    {"Biggest experience gain", "The most experience gained at once.", false, false, false, false, false, true},
    {"Heaviest load", "The most weight carried, and the share of what the champion could carry.", false, false, false, false, false, true},
    {"Longest fall", "Pits fallen through one after the other.", true, false, false, false, false, true},
    {"Longest teleport chain", "Teleporters one after the other.", true, false, false, false, false, true},
    {"Longest sleep", "Ticks of one sleep.", true, false, false, false, true, true},
    {"Most poison at once", "The most poison working on a champion at one time, all doses added up; it is the number of poison ticks still to come.", false, false, true, false, false, true},
    {"Longest poisoned", "Ticks of the longest time a champion stayed poisoned without a break, with the poison damage taken in it. Death or a cure ends it.", false, false, true, false, true, true},
    {"Most poisonings in a row", "The most new doses of poison taken within one stretch of being poisoned.", false, false, true, false, false, true},
    {"Most wounds at once", "The most wounded parts of the body of one champion at the same time.", false, false, true, false, false, true},
    {"Most wounds from one blow", "The most parts of the body one blow, missile or explosion newly wounded.", false, false, true, false, false, true},
    {"Poison doses suffered", "All the doses of poison a champion took in the run (or on the level): each new dose counts once.", false, false, true, false, false, true},
    {"Wounds suffered", "All the parts of the body wounded that were whole before, added up over the run (or the level).", false, false, true, false, false, true},
    {"Most poison doses at once", "The most doses of poison working on a champion at the same time.", false, false, true, false, false, true},
    {"Most poison per 36 ticks", "The most HP a champion's poison could take at one poison tick (every 36 ticks), all doses added up.", false, false, true, false, false, true},
    {"Biggest melee hit", "The most damage a blow at close range, with a weapon or an empty hand, did to one monster.", false, false, false, false, false, true},
    {"Biggest ranged hit", "The most damage a thrown item or a shot did to one monster.", false, false, false, false, false, true},
};

// The names csb.cfg knows the numbers by, in the order of HeroStatsNumber
static const char *const KEYS[] = {
    "damage", "melee", "ranged", "magic", "kills", "attacks", "hits", "hit_rate", "damage_per_hit", "other_actions",
    "friendly_fire",
    "mana_generated", "mana_regeneration", "mana_potions", "mana_spent", "mana_on_spells", "mana_successful", "mana_wasted",
    "runes_cleared", "runes_not_cast",
    "casts", "casts_successful", "casts_failed",
    "taken", "healed", "heal_regeneration", "heal_potions", "heal_action", "healing_given", "poisoned", "wounds",
    "deaths", "resurrected", "wipes", "reloads",
    "eaten", "drunk", "food_gained", "food_wasted", "water_gained", "water_wasted",
    "picked_up", "used_in_walls",
    "steps", "tiles_walked", "tiles_explored", "turns", "wall_bumps", "teleports",
    "pit_falls", "pit_climbs", "ticks", "sleep_ticks",
    "doors_bashed", "doors_smashed", "doors_shot", "doors_by_spell", "doors_by_zo",
    "doors_button_opened", "doors_button_closed", "chests", "scrolls", "wall_texts",
    "champions_seen", "champions_viewed",
    "door_kills", "pit_kills", "other_kills",
    "experience", "fighter", "ninja", "priest", "wizard", "ranks_gained",
    "biggest_hit", "biggest_spell_hit", "biggest_spell_total", "most_kills_at_once", "kill_streak",
    "biggest_hit_taken", "biggest_spell_hit_taken", "closest_call", "biggest_heal", "most_mana_on_spell",
    "biggest_experience_gain", "heaviest_load", "longest_fall", "longest_teleport_chain", "longest_sleep",
    "most_poison", "longest_poisoned", "most_poisonings", "most_wounds", "most_wounds_one_blow",
    "poison_doses_suffered", "wounds_suffered", "most_poison_doses", "most_poison_per_36_ticks",
    "biggest_melee_hit", "biggest_ranged_hit"};
static_assert(sizeof KEYS / sizeof KEYS[0] == HERO_NUMBER_COUNT, "a name for every number");

const HeroStatsNumberInfo &HeroStatsCatalogue_Info(HeroStatsNumber number)
{
  return NUMBERS[number];
}

static std::vector<HeroStatsTopicInfo> MakeTopics(void)
{
  std::vector<HeroStatsTopicInfo> topics(HERO_TOPIC_COUNT);
  topics[HeroTopicOverview] = {"Overview",
                               {{HeroNumDamage, 0}, {HeroNumKills, 0}, {HeroNumTaken, 0}, {HeroNumHealed, 0},
                                {HeroNumManaSpent, 0}, {HeroNumDeaths, 0}, {HeroNumEaten, 0}, {HeroNumDrunk, 0},
                                {HeroNumDoorsBashed, 0}, {HeroNumSteps, 0}, {HeroNumTilesExplored, 0}, {HeroNumTicks, 0}},
                               {HeroNumDamage, HeroNumKills, HeroNumDeaths, HeroNumTilesExplored}};
  topics[HeroTopicCombat] = {"Combat",
                             {{HeroNumDamage, 0}, {HeroNumMelee, 1}, {HeroNumRanged, 1}, {HeroNumMagic, 1}, {HeroNumKills, 0},
                              {HeroNumAttacks, 0}, {HeroNumHits, 1}, {HeroNumHitRate, 1}, {HeroNumDamagePerHit, 1},
                              {HeroNumOtherActions, 0}, {HeroNumFriendlyFire, 0}},
                             {HeroNumDamage, HeroNumKills, HeroNumHitRate, HeroNumFriendlyFire}};
  topics[HeroTopicMagic] = {"Magic",
                            {{HeroNumManaGenerated, 0}, {HeroNumManaRegeneration, 1}, {HeroNumManaPotions, 1},
                             {HeroNumManaSpent, 0}, {HeroNumManaOnSpells, 1}, {HeroNumManaSuccessful, 2}, {HeroNumManaWasted, 2},
                             {HeroNumRunesCleared, 1},
                             {HeroNumRunesNotCast, 1}, {HeroNumCasts, 0}, {HeroNumCastsSuccessful, 1}, {HeroNumCastsFailed, 1}},
                            {HeroNumManaGenerated, HeroNumManaSpent, HeroNumCasts, HeroNumManaWasted}};
  topics[HeroTopicSurvival] = {"Survival",
                               {{HeroNumTaken, 0}, {HeroNumHealed, 0}, {HeroNumHealRegeneration, 1}, {HeroNumHealPotions, 1},
                                {HeroNumHealAction, 1}, {HeroNumHealingGiven, 0}, {HeroNumPoisonings, 0}, {HeroNumWounds, 0}, {HeroNumDeaths, 0}, {HeroNumResurrected, 0},
                                {HeroNumWipes, 0}, {HeroNumReloads, 0}},
                               {HeroNumTaken, HeroNumHealed, HeroNumDeaths, HeroNumResurrected}};
  topics[HeroTopicSupplies] = {"Food & items",
                               {{HeroNumEaten, 0}, {HeroNumDrunk, 0}, {HeroNumFoodGained, 0}, {HeroNumFoodWasted, 1},
                                {HeroNumWaterGained, 0}, {HeroNumWaterWasted, 1}, {HeroNumPickedUp, 0}, {HeroNumUsedInWalls, 0}},
                               {HeroNumEaten, HeroNumDrunk, HeroNumFoodWasted, HeroNumPickedUp}};
  topics[HeroTopicExploration] = {"Exploration",
                                  {{HeroNumSteps, 0}, {HeroNumTilesWalked, 0}, {HeroNumTilesExplored, 0}, {HeroNumTurns, 0},
                                   {HeroNumBumps, 0}, {HeroNumTeleports, 0}, {HeroNumPitFalls, 0}, {HeroNumPitClimbs, 0},
                                   {HeroNumTicks, 0}, {HeroNumSleepTicks, 1}, {HeroNumDoorsBashed, 0}, {HeroNumDoorsSmashed, 1},
                                   {HeroNumDoorsShot, 1}, {HeroNumDoorsBySpell, 1}, {HeroNumDoorsByZo, 0},
                                   {HeroNumDoorsButtonOpened, 0}, {HeroNumDoorsButtonClosed, 0}, {HeroNumChests, 0},
                                   {HeroNumScrolls, 0}, {HeroNumWallTexts, 0}, {HeroNumChampionsSeen, 0},
                                   {HeroNumChampionsViewed, 0}},
                                  {HeroNumTilesExplored, HeroNumSteps, HeroNumTicks, HeroNumDoorsBashed}};
  topics[HeroTopicMonsters] = {"Monsters",
                               {{HeroNumKills, 0}, {HeroNumDoorKills, 0}, {HeroNumPitKills, 0}, {HeroNumOtherKills, 0}},
                               {HeroNumKills, HeroNumDoorKills, HeroNumPitKills, HeroNumOtherKills}};
  topics[HeroTopicExperience] = {"Experience",
                                 {{HeroNumExperience, 0}, {HeroNumXpFighter, 1}, {HeroNumXpNinja, 1}, {HeroNumXpPriest, 1},
                                  {HeroNumXpWizard, 1}, {HeroNumRanksGained, 0}},
                                 {HeroNumXpFighter, HeroNumXpNinja, HeroNumXpPriest, HeroNumXpWizard}};
  topics[HeroTopicRecords] = {"Records", {},
                              {HeroNumRecBiggestMeleeHit, HeroNumRecBiggestRangedHit, HeroNumRecBiggestSpellTotal, HeroNumRecBiggestHitTaken}};
  // The melee and ranged hits first, where the biggest hit was; that one is no longer shown
  topics[HeroTopicRecords].rows.push_back({HeroNumRecBiggestMeleeHit, 0});
  topics[HeroTopicRecords].rows.push_back({HeroNumRecBiggestRangedHit, 0});
  for (int k = 0; k < HERO_RECORD_COUNT; k++)
    if (k != HeroRecBiggestHit && k != HeroRecBiggestMeleeHit && k != HeroRecBiggestRangedHit)
      topics[HeroTopicRecords].rows.push_back({(HeroStatsNumber)(HeroNumRecFirst + k), 0});
  return topics;
}

static const std::vector<HeroStatsNumber> CHAMPION_TILES = {HeroNumDamage, HeroNumKills, HeroNumTaken, HeroNumDeaths};

// As MakeTopics made them, with the tiles and Key numbers csb.cfg chose
static std::vector<HeroStatsTopicInfo> &Topics(void)
{
  static std::vector<HeroStatsTopicInfo> topics = MakeTopics();
  return topics;
}

static std::vector<HeroStatsNumber> &ChampionTiles(void)
{
  static std::vector<HeroStatsNumber> tiles = CHAMPION_TILES;
  return tiles;
}

const HeroStatsTopicInfo &HeroStatsCatalogue_Topic(int topic)
{
  if (topic < 0 || topic >= HERO_TOPIC_COUNT) topic = HeroTopicOverview;
  return Topics()[topic];
}

const std::vector<HeroStatsNumber> &HeroStatsCatalogue_ChampionTiles(void)
{
  return ChampionTiles();
}

std::vector<HeroStatsNumber> HeroStatsCatalogue_DefaultTiles(int topic)
{
  if (topic < 0 || topic >= HERO_TOPIC_COUNT) topic = HeroTopicOverview;
  return MakeTopics()[topic].tiles;
}

static std::vector<HeroStatsNumber> DefaultKeyNumbers(void)
{
  std::vector<HeroStatsNumber> numbers;
  std::vector<HeroStatsTopicRow> rows = MakeTopics()[HeroTopicOverview].rows;
  for (size_t r = 0; r < rows.size(); r++) numbers.push_back(rows[r].number);
  return numbers;
}

std::string HeroStatsCatalogue_Key(HeroStatsNumber number)
{
  return KEYS[number];
}

std::vector<HeroStatsNumber> HeroStatsCatalogue_ParseNumbers(const std::string &text, size_t most)
{
  std::vector<HeroStatsNumber> numbers;
  size_t at = 0;
  while (at < text.size() && numbers.size() < most)
  {
    size_t start = text.find_first_not_of(" ,\t", at);
    if (start == std::string::npos) break;
    size_t end = text.find_first_of(" ,\t", start);
    if (end == std::string::npos) end = text.size();
    std::string word = text.substr(start, end - start);
    for (size_t i = 0; i < word.size(); i++) word[i] = (char)tolower((unsigned char)word[i]);
    // The biggest hit is no longer shown: a csb.cfg that names it gets the biggest melee hit
    if (word == "biggest_hit") word = "biggest_melee_hit";
    for (int n = 0; n < HERO_NUMBER_COUNT; n++)
      if (word == KEYS[n] && std::find(numbers.begin(), numbers.end(), (HeroStatsNumber)n) == numbers.end())
        numbers.push_back((HeroStatsNumber)n);
    at = end;
  }
  return numbers;
}

std::string HeroStatsCatalogue_JoinNumbers(const std::vector<HeroStatsNumber> &numbers)
{
  std::string text;
  for (size_t i = 0; i < numbers.size(); i++) text += (i > 0 ? ", " : "") + std::string(KEYS[numbers[i]]);
  return text;
}

static const int MOST_TILES = 4, MOST_KEY_NUMBERS = 24;

struct SettingInfo { const char *entry; int topic; };  // topic -1: the champion's Overview tiles
static const SettingInfo SETTINGS[HERO_SETTING_COUNT] = {
    {"stats_tiles_overview", HeroTopicOverview}, {"stats_tiles_champion", -1}, {"stats_tiles_combat", HeroTopicCombat},
    {"stats_tiles_magic", HeroTopicMagic}, {"stats_tiles_survival", HeroTopicSurvival}, {"stats_tiles_food", HeroTopicSupplies},
    {"stats_tiles_exploration", HeroTopicExploration}, {"stats_tiles_monsters", HeroTopicMonsters},
    {"stats_tiles_experience", HeroTopicExperience}, {"stats_tiles_records", HeroTopicRecords},
    {"stats_key_numbers", HeroTopicOverview}};

const char *HeroStatsCatalogue_SettingEntry(HeroStatsSetting setting)
{
  return SETTINGS[setting].entry;
}

static std::vector<HeroStatsNumber> SettingDefaults(HeroStatsSetting setting)
{
  if (setting == HeroSettingKeyNumbers) return DefaultKeyNumbers();
  if (setting == HeroSettingChampionTiles) return CHAMPION_TILES;
  return HeroStatsCatalogue_DefaultTiles(SETTINGS[setting].topic);
}

std::string HeroStatsCatalogue_SettingDefault(HeroStatsSetting setting)
{
  return HeroStatsCatalogue_JoinNumbers(SettingDefaults(setting));
}

void HeroStatsCatalogue_ApplySetting(HeroStatsSetting setting, const std::string &text)
{
  bool keyNumbers = setting == HeroSettingKeyNumbers;
  std::vector<HeroStatsNumber> numbers = HeroStatsCatalogue_ParseNumbers(text, keyNumbers ? MOST_KEY_NUMBERS : MOST_TILES);
  if (numbers.empty()) numbers = SettingDefaults(setting);
  if (keyNumbers)
  {
    std::vector<HeroStatsTopicRow> &rows = Topics()[HeroTopicOverview].rows;
    rows.clear();
    for (size_t i = 0; i < numbers.size(); i++) rows.push_back({numbers[i], 0});
  }
  else if (setting == HeroSettingChampionTiles) ChampionTiles() = numbers;
  else Topics()[SETTINGS[setting].topic].tiles = numbers;
}

// part / whole as a rounded whole number, -1 when whole is 0
static int RoundedRatio(long long part, long long whole)
{
  if (whole <= 0) return -1;
  return (int)((part * 2 + whole) / (whole * 2));
}

// The numbers come in groups that one look at the store fills together. Record fills them all;
// Value only the group of its number, which is what Per level needs for one number per champion.
typedef void (*FillGroup)(const HeroStats &stats, HeroStatsChampion who, int level, int *v);

static void FillDamage(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumMelee] = stats.DamageDealt(who, level, HeroDamageMelee);
  v[HeroNumRanged] = stats.DamageDealt(who, level, HeroDamageRanged);
  v[HeroNumMagic] = stats.DamageDealt(who, level, HeroDamageMagic);
  v[HeroNumDamage] = v[HeroNumMelee] + v[HeroNumRanged] + v[HeroNumMagic];
}

static void FillKills(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumKills] = stats.Kills(who, level);
}

// As the Weapons table counts them: spells entered as runes are no attacks; the actions that
// cannot hit are no attacks either, they are Other actions
static void FillAttacks(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  int attackDamage = 0;
  v[HeroNumAttacks] = v[HeroNumHits] = v[HeroNumOtherActions] = 0;
  std::vector<HeroStatsAttackRow> attacks = stats.Attacks(who, level);
  for (size_t i = 0; i < attacks.size(); i++)
  {
    if (attacks[i].weapon == HERO_STATS_NO_WEAPON) continue;
    v[HeroNumAttacks] += attacks[i].tries;
    v[HeroNumOtherActions] += attacks[i].uses;
    v[HeroNumHits] += attacks[i].hits;
    attackDamage += attacks[i].damage;
  }
  v[HeroNumHitRate] = RoundedRatio((long long)v[HeroNumHits] * 100, v[HeroNumAttacks]);
  v[HeroNumDamagePerHit] = RoundedRatio(attackDamage, v[HeroNumHits]);
}

static void FillFriendlyFire(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumFriendlyFire] = stats.FriendlyFireCaused(who, level);
}

static void FillMana(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumManaRegeneration] = stats.ManaGainedBy(who, level, HeroManaRegeneration);
  v[HeroNumManaPotions] = stats.ManaGainedBy(who, level, HeroManaPotion);
  v[HeroNumManaGenerated] = stats.ManaGained(who, level);
  v[HeroNumManaSpent] = stats.ManaSpent(who, level);
  v[HeroNumManaWasted] = stats.ManaWasted(who, level);
  v[HeroNumRunesCleared] = stats.RunesCleared(who, level);
  v[HeroNumManaSuccessful] = stats.ManaOnSuccessfulCasts(who, level);
  v[HeroNumRunesNotCast] = stats.RunesNotCastYet(who, level);
  v[HeroNumManaOnSpells] = v[HeroNumManaSuccessful] + v[HeroNumManaWasted];
}

static void FillCasts(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumCasts] = v[HeroNumCastsSuccessful] = v[HeroNumCastsFailed] = 0;
  std::vector<HeroStatsSpellRow> spells = stats.Spells(who, level);
  for (size_t i = 0; i < spells.size(); i++)
  {
    v[HeroNumCasts] += spells[i].casts + spells[i].failed;
    v[HeroNumCastsSuccessful] += spells[i].casts;
    v[HeroNumCastsFailed] += spells[i].failed;
  }
}

static void FillTaken(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumTaken] = stats.Harm(who, level);
}

static void FillHealing(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumHealed] = stats.Healed(who, level);
  v[HeroNumHealRegeneration] = v[HeroNumHealPotions] = v[HeroNumHealAction] = 0;
  std::vector<HeroStatsHealRow> heals = stats.HealRows(who, level);
  for (size_t i = 0; i < heals.size(); i++)
  {
    if (heals[i].kind == HeroHealRegeneration) v[HeroNumHealRegeneration] += heals[i].amount;
    else if (heals[i].kind == HeroHealPotion) v[HeroNumHealPotions] += heals[i].amount;
    else v[HeroNumHealAction] += heals[i].amount;
  }
  v[HeroNumHealingGiven] = stats.HealingGiven(who, level);
}

static void FillDeaths(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumDeaths] = 0;
  std::vector<HeroStatsDeath> deaths = stats.Deaths(who);
  for (size_t i = 0; i < deaths.size(); i++)
    if (level == HERO_STATS_WHOLE_RUN || deaths[i].level == level) v[HeroNumDeaths]++;
  v[HeroNumResurrected] = stats.Resurrections(who, level);
}

static void FillFood(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumEaten] = stats.Eaten(who, level);
  HeroStatsNourishment food = stats.Nourishment(who, level);
  v[HeroNumDrunk] = stats.Drunk(who, level) + food.fountainDrinks;  // the fountain is a way of drinking
  v[HeroNumFoodGained] = food.foodGained;
  v[HeroNumFoodWasted] = food.foodWasted;
  v[HeroNumWaterGained] = food.waterGained;
  v[HeroNumWaterWasted] = food.waterWasted;
}

static void FillDoors(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumDoorsBashed] = stats.DoorsBashed(who, level);
  v[HeroNumDoorsSmashed] = stats.DoorsBashed(who, level, HeroDoorSmashed);
  v[HeroNumDoorsShot] = stats.DoorsBashed(who, level, HeroDoorShot);
  v[HeroNumDoorsBySpell] = stats.DoorsBashed(who, level, HeroDoorBySpell);
  v[HeroNumDoorsByZo] = stats.DoorsOpenedByZo(who, level);
}

// The party's only: who is HERO_STATS_PARTY
static void FillHardship(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  bool wholeRun = level == HERO_STATS_WHOLE_RUN;
  v[HeroNumWipes] = wholeRun ? stats.PartyWipes() : 0;
  v[HeroNumReloads] = wholeRun ? stats.Reloads() : 0;
}

static void FillItems(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  v[HeroNumPickedUp] = v[HeroNumUsedInWalls] = 0;
  std::vector<HeroStatsItemRow> items = stats.Items(level);
  for (size_t i = 0; i < items.size(); i++)
  {
    v[HeroNumPickedUp] += items[i].pickedUp;
    v[HeroNumUsedInWalls] += items[i].usedInWall;
  }
}

static void FillMovement(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  HeroStatsMovement move = stats.Movement(level);
  v[HeroNumSteps] = move.steps;
  v[HeroNumTilesWalked] = move.tilesWalked;
  v[HeroNumTilesExplored] = move.tilesExplored;
  v[HeroNumTurns] = move.turns;
  v[HeroNumBumps] = move.bumps;
  v[HeroNumTeleports] = move.teleports;
  v[HeroNumPitFalls] = move.pitFalls;
  v[HeroNumPitClimbs] = move.pitClimbs;
  v[HeroNumTicks] = move.ticks;
  v[HeroNumSleepTicks] = move.sleepTicks;
}

static void FillDungeon(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  v[HeroNumDoorsButtonOpened] = stats.DoorsOpenedByButton(level);
  v[HeroNumDoorsButtonClosed] = stats.DoorsClosedByButton(level);
  v[HeroNumChests] = stats.ChestsOpened(level);
  v[HeroNumScrolls] = stats.ScrollsRead(level);
  v[HeroNumWallTexts] = stats.WallTextsRead(level);
  v[HeroNumChampionsSeen] = stats.ChampionsSeen(level);
  v[HeroNumChampionsViewed] = stats.ChampionsViewed(level);
}

// As the Monsters page counts them: a door, a fall, or any cause but a champion's blow
static void FillKillCauses(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  v[HeroNumDoorKills] = v[HeroNumPitKills] = v[HeroNumOtherKills] = 0;
  std::vector<HeroStatsMonsterRow> kills = stats.MonsterKills(level);
  for (size_t i = 0; i < kills.size(); i++)
  {
    if (kills[i].by == HeroBlowByDoor) v[HeroNumDoorKills] += kills[i].count;
    else if (kills[i].by == HeroBlowByPit) v[HeroNumPitKills] += kills[i].count;
    else if (kills[i].by != HeroBlowByChampion) v[HeroNumOtherKills] += kills[i].count;
  }
}

static void FillExperience(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  const HeroStatsExperience &xp = stats.Experience();
  v[HeroNumExperience] = xp.Gained(who, level, -1);
  for (int cls = 0; cls < HERO_XP_CLASS_COUNT; cls++) v[HeroNumXpFighter + cls] = xp.Gained(who, level, cls);
  v[HeroNumRanksGained] = xp.RanksGained(who, level, -1);
}

static void FillEvents(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumPoisonings] = stats.Events(who, level, HeroEventPoisoned);
  v[HeroNumWounds] = stats.Events(who, level, HeroEventWounded);
}

static void FillRecords(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  for (int k = 0; k < HERO_RECORD_COUNT; k++)
  {
    HeroRecordEntry entry;
    v[HeroNumRecFirst + k] = stats.Records().Best((HeroRecordKind)k, who, level, &entry) ? entry.value : -1;
  }
}

static const FillGroup CHAMPION_GROUPS[] = {FillDamage, FillKills, FillAttacks, FillFriendlyFire, FillMana, FillCasts,
                                            FillTaken, FillHealing, FillDeaths, FillFood, FillDoors, FillExperience,
                                            FillEvents, FillRecords};
static const FillGroup PARTY_GROUPS[] = {FillHardship, FillItems, FillMovement, FillDungeon, FillKillCauses};

static FillGroup GroupOf(HeroStatsNumber number)
{
  switch (number)
  {
  case HeroNumDamage: case HeroNumMelee: case HeroNumRanged: case HeroNumMagic: return FillDamage;
  case HeroNumKills: return FillKills;
  case HeroNumAttacks: case HeroNumHits: case HeroNumHitRate: case HeroNumDamagePerHit: case HeroNumOtherActions: return FillAttacks;
  case HeroNumFriendlyFire: return FillFriendlyFire;
  case HeroNumManaGenerated: case HeroNumManaRegeneration: case HeroNumManaPotions: case HeroNumManaSpent:
  case HeroNumManaWasted: case HeroNumRunesCleared: case HeroNumManaSuccessful: case HeroNumRunesNotCast:
  case HeroNumManaOnSpells:
    return FillMana;
  case HeroNumCasts: case HeroNumCastsSuccessful: case HeroNumCastsFailed: return FillCasts;
  case HeroNumTaken: return FillTaken;
  case HeroNumHealed: case HeroNumHealRegeneration: case HeroNumHealPotions: case HeroNumHealAction:
  case HeroNumHealingGiven: return FillHealing;
  case HeroNumDeaths: case HeroNumResurrected: return FillDeaths;
  case HeroNumPoisonings: case HeroNumWounds: return FillEvents;
  case HeroNumWipes: case HeroNumReloads: return FillHardship;
  case HeroNumEaten: case HeroNumDrunk: case HeroNumFoodGained: case HeroNumFoodWasted: case HeroNumWaterGained:
  case HeroNumWaterWasted: return FillFood;
  case HeroNumPickedUp: case HeroNumUsedInWalls: return FillItems;
  case HeroNumSteps: case HeroNumTilesWalked: case HeroNumTilesExplored: case HeroNumTurns: case HeroNumBumps:
  case HeroNumTeleports: case HeroNumPitFalls: case HeroNumPitClimbs: case HeroNumTicks: case HeroNumSleepTicks:
    return FillMovement;
  case HeroNumDoorsBashed: case HeroNumDoorsSmashed: case HeroNumDoorsShot: case HeroNumDoorsBySpell:
  case HeroNumDoorsByZo: return FillDoors;
  case HeroNumDoorsButtonOpened: case HeroNumDoorsButtonClosed: case HeroNumChests: case HeroNumScrolls:
  case HeroNumWallTexts: case HeroNumChampionsSeen: case HeroNumChampionsViewed: return FillDungeon;
  case HeroNumExperience: case HeroNumXpFighter: case HeroNumXpNinja: case HeroNumXpPriest: case HeroNumXpWizard:
  case HeroNumRanksGained: return FillExperience;
  case HeroNumDoorKills: case HeroNumPitKills: case HeroNumOtherKills: return FillKillCauses;
  default: return FillRecords;
  }
}

HeroStatsRecord HeroStatsCatalogue_Record(const HeroStats &stats, HeroStatsChampion who, int level)
{
  HeroStatsRecord r = {};
  for (size_t g = 0; g < sizeof CHAMPION_GROUPS / sizeof CHAMPION_GROUPS[0]; g++) CHAMPION_GROUPS[g](stats, who, level, r.value);
  if (who == HERO_STATS_PARTY)
    for (size_t g = 0; g < sizeof PARTY_GROUPS / sizeof PARTY_GROUPS[0]; g++) PARTY_GROUPS[g](stats, who, level, r.value);
  return r;
}

int HeroStatsCatalogue_Value(const HeroStats &stats, HeroStatsChampion who, int level, HeroStatsNumber number)
{
  if (NUMBERS[number].partyOnly && who != HERO_STATS_PARTY) return NUMBERS[number].isRecord ? -1 : 0;
  HeroStatsRecord r = {};
  GroupOf(number)(stats, who, level, r.value);
  return r.value[number];
}

bool HeroStatsCatalogue_Applies(HeroStatsNumber number, HeroStatsChampion who, int level)
{
  const HeroStatsNumberInfo &info = NUMBERS[number];
  if (info.partyOnly && who != HERO_STATS_PARTY) return false;
  if (info.wholeRunOnly && level != HERO_STATS_WHOLE_RUN) return false;
  return true;
}

bool HeroStatsCatalogue_Summable(HeroStatsNumber number)
{
  return !NUMBERS[number].isRatio && !NUMBERS[number].isRecord;
}

bool HeroStatsCatalogue_IsRecord(HeroStatsNumber number)
{
  return NUMBERS[number].isRecord;
}

HeroRecordKind HeroStatsCatalogue_RecordOf(HeroStatsNumber number)
{
  return (HeroRecordKind)(number - HeroNumRecFirst);
}

std::string HeroStatsCatalogue_Format(HeroStatsNumber number, int value)
{
  if ((NUMBERS[number].isRatio || NUMBERS[number].isRecord) && value < 0) return "-";
  if (number == HeroNumRecHeaviestLoad) return HeroStatsView_Number(value / 10) + "." + std::to_string(value % 10) + " kg";
  if (number == HeroNumRecClosestCall) return HeroStatsView_Number(value) + " HP left";
  if (number == HeroNumHitRate) return std::to_string(value) + "%";
  return HeroStatsView_Number(value);
}

std::vector<HeroStatsNumber> HeroStatsCatalogue_GridNumbers(int topic)
{
  std::vector<HeroStatsNumber> numbers;
  const HeroStatsTopicInfo &info = HeroStatsCatalogue_Topic(topic);
  for (size_t i = 0; i < info.rows.size(); i++)
  {
    HeroStatsNumber number = info.rows[i].number;
    // Records: one at a time, a column per champion (their best) and the party's best
    if (NUMBERS[number].isRecord || (!NUMBERS[number].partyOnly && !NUMBERS[number].isRatio)) numbers.push_back(number);
  }
  return numbers;
}

int HeroStatsCatalogue_TopicOfOldPage(int oldPage)
{
  if (oldPage == 3) return HeroTopicMonsters;
  if (oldPage == 4) return HeroTopicSupplies;
  return HeroTopicOverview;
}

std::string HeroStatsCatalogue_Summary(int topic, const HeroStatsRecord &record, HeroStatsChampion who, int level)
{
  std::string summary;
  int shown = 0;
  const HeroStatsTopicInfo &info = HeroStatsCatalogue_Topic(topic);
  for (size_t i = 0; i < info.tiles.size() && shown < 3; i++)
  {
    HeroStatsNumber number = info.tiles[i];
    if (!HeroStatsCatalogue_Applies(number, who, level)) continue;
    if ((NUMBERS[number].isRatio || NUMBERS[number].isRecord) && record.value[number] < 0) continue;  // "- hit rate" says nothing
    std::string label = NUMBERS[number].label;
    label[0] = (char)tolower((unsigned char)label[0]);
    if (shown > 0) summary += " \xc2\xb7 ";
    summary += HeroStatsCatalogue_Format(number, record.value[number]) + " " + label;
    shown++;
  }
  return summary;
}
