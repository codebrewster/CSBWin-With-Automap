// HeroStatsCatalogue.cpp : every number of the Statistics window, defined once
#include "HeroStatsCatalogue.h"
#include <ctype.h>
#include "HeroStatsView.h"

static const HeroStatsNumberInfo NUMBERS[HERO_NUMBER_COUNT] = {
    // label, help, partyOnly, wholeRunOnly, isBad, isRatio, isTicks
    {"Damage dealt", "", false, false, false, false, false},
    {"Melee", "", false, false, false, false, false},
    {"Ranged", "", false, false, false, false, false},
    {"Magic", "", false, false, false, false, false},
    {"Kills", "", false, false, false, false, false},
    {"Attacks", "", false, false, false, false, false},
    {"Hits", "", false, false, false, false, false},
    {"Hit rate", "", false, false, false, true, false},
    {"Damage per hit", "", false, false, false, true, false},
    {"Friendly fire caused", "", false, false, true, false, false},
    {"Mana generated", "", false, false, false, false, false},
    {"Regeneration", "", false, false, false, false, false},
    {"Mana potions", "", false, false, false, false, false},
    {"Mana spent", "", false, false, false, false, false},
    {"Wasted on failures", "", false, false, true, false, false},
    {"Spells cast", "", false, false, false, false, false},
    {"Failed", "", false, false, true, false, false},
    {"Damage taken", "", false, false, false, false, false},
    {"Healed", "", false, false, false, false, false},
    {"Regeneration", "", false, false, false, false, false},
    {"Potions", "", false, false, false, false, false},
    {"Heal action", "", false, false, false, false, false},
    {"Healing given", "HP healed by potions this champion brewed, whoever drank them.", false, false, false, false, false},
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
};

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
                              {HeroNumFriendlyFire, 0}},
                             {HeroNumDamage, HeroNumKills, HeroNumHitRate, HeroNumFriendlyFire}};
  topics[HeroTopicMagic] = {"Magic",
                            {{HeroNumManaGenerated, 0}, {HeroNumManaRegeneration, 1}, {HeroNumManaPotions, 1},
                             {HeroNumManaSpent, 0}, {HeroNumManaWasted, 1}, {HeroNumCasts, 0}, {HeroNumCastsFailed, 1}},
                            {HeroNumManaGenerated, HeroNumManaSpent, HeroNumCasts, HeroNumManaWasted}};
  topics[HeroTopicSurvival] = {"Survival",
                               {{HeroNumTaken, 0}, {HeroNumHealed, 0}, {HeroNumHealRegeneration, 1}, {HeroNumHealPotions, 1},
                                {HeroNumHealAction, 1}, {HeroNumHealingGiven, 0}, {HeroNumDeaths, 0}, {HeroNumResurrected, 0},
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
  return topics;
}

const HeroStatsTopicInfo &HeroStatsCatalogue_Topic(int topic)
{
  static const std::vector<HeroStatsTopicInfo> topics = MakeTopics();
  if (topic < 0 || topic >= HERO_TOPIC_COUNT) topic = HeroTopicOverview;
  return topics[topic];
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
// cannot hit are attacks but not in the hit rate
static void FillAttacks(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  int tries = 0, attackDamage = 0;
  v[HeroNumAttacks] = v[HeroNumHits] = 0;
  std::vector<HeroStatsAttackRow> attacks = stats.Attacks(who, level);
  for (size_t i = 0; i < attacks.size(); i++)
  {
    if (attacks[i].weapon == HERO_STATS_NO_WEAPON) continue;
    tries += attacks[i].tries;
    v[HeroNumAttacks] += attacks[i].tries + attacks[i].uses;
    v[HeroNumHits] += attacks[i].hits;
    attackDamage += attacks[i].damage;
  }
  v[HeroNumHitRate] = RoundedRatio((long long)v[HeroNumHits] * 100, tries);
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
}

static void FillCasts(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumCasts] = v[HeroNumCastsFailed] = 0;
  std::vector<HeroStatsSpellRow> spells = stats.Spells(who, level);
  for (size_t i = 0; i < spells.size(); i++)
  {
    v[HeroNumCasts] += spells[i].casts + spells[i].failed;
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

static const FillGroup CHAMPION_GROUPS[] = {FillDamage, FillKills, FillAttacks, FillFriendlyFire, FillMana, FillCasts,
                                            FillTaken, FillHealing, FillDeaths, FillFood, FillDoors};
static const FillGroup PARTY_GROUPS[] = {FillHardship, FillItems, FillMovement, FillDungeon, FillKillCauses};

static FillGroup GroupOf(HeroStatsNumber number)
{
  switch (number)
  {
  case HeroNumDamage: case HeroNumMelee: case HeroNumRanged: case HeroNumMagic: return FillDamage;
  case HeroNumKills: return FillKills;
  case HeroNumAttacks: case HeroNumHits: case HeroNumHitRate: case HeroNumDamagePerHit: return FillAttacks;
  case HeroNumFriendlyFire: return FillFriendlyFire;
  case HeroNumManaGenerated: case HeroNumManaRegeneration: case HeroNumManaPotions: case HeroNumManaSpent:
  case HeroNumManaWasted: return FillMana;
  case HeroNumCasts: case HeroNumCastsFailed: return FillCasts;
  case HeroNumTaken: return FillTaken;
  case HeroNumHealed: case HeroNumHealRegeneration: case HeroNumHealPotions: case HeroNumHealAction:
  case HeroNumHealingGiven: return FillHealing;
  case HeroNumDeaths: case HeroNumResurrected: return FillDeaths;
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
  default: return FillKillCauses;
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
  if (NUMBERS[number].partyOnly && who != HERO_STATS_PARTY) return 0;
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
  return !NUMBERS[number].isRatio;
}

std::string HeroStatsCatalogue_Format(HeroStatsNumber number, int value)
{
  if (NUMBERS[number].isRatio && value < 0) return "-";
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
    if (!NUMBERS[number].partyOnly && !NUMBERS[number].isRatio) numbers.push_back(number);
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
    if (NUMBERS[number].isRatio && record.value[number] < 0) continue;  // "- hit rate" says nothing
    std::string label = NUMBERS[number].label;
    label[0] = (char)tolower((unsigned char)label[0]);
    if (shown > 0) summary += " \xc2\xb7 ";
    summary += HeroStatsCatalogue_Format(number, record.value[number]) + " " + label;
    shown++;
  }
  return summary;
}
