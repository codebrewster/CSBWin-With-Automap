#include "HeroStatsProfile.h"
#include <cmath>
#include <cstdio>
#include "HeroStatsCatalogue.h"
#include "HeroStatsExperience.h"
#include "HeroStatsView.h"

namespace
{

// part / whole, -1 when there is no whole
double Ratio(long long part, long long whole)
{
  if (whole <= 0) return -1;
  double value = (double)part / (double)whole;
  return value < 0 ? 0 : value > 1 ? 1 : value;
}

std::string Percent(double value)
{
  if (value < 0) return "-";
  char text[16];
  std::snprintf(text, sizeof text, "%d%%", (int)std::floor(value * 100 + 0.5));
  return text;
}

std::string Count(long long value)
{
  return HeroStatsCatalogue_Format(HeroNumDamage, (int)value);
}

std::string OfWhole(long long part, long long whole)
{
  return Count(part) + " of " + Count(whole);
}

// Up to the next tenth, at least least
double RoundUpToTenth(double value, double least)
{
  double rounded = std::ceil(value * 10 - 1e-9) / 10;
  return rounded < least ? least : rounded;
}

HeroStatsStar NewStar(const char *title, const char *caption)
{
  HeroStatsStar star;
  star.title = title;
  star.caption = caption;
  star.outer = 1;
  star.evenRing = -1;
  star.centre = 0;
  return star;
}

void PercentRings(HeroStatsStar *star)
{
  star->rings.push_back(std::make_pair(0.25, std::string("")));
  star->rings.push_back(std::make_pair(0.5, std::string("50%")));
  star->rings.push_back(std::make_pair(0.75, std::string("")));
  star->rings.push_back(std::make_pair(1.0, std::string("100%")));
}

// Rings at thirds of the edge, each labelled with its percentage
void ThirdRings(HeroStatsStar *star)
{
  for (int i = 1; i <= 3; i++)
  {
    double value = star->outer * i / 3;
    star->rings.push_back(std::make_pair(value, Percent(value)));
  }
}

// The faint shapes go first: they are drawn behind the bright one
void FaintFirst(std::vector<HeroStatsStarShape> *shapes)
{
  std::vector<HeroStatsStarShape> ordered;
  for (int pass = 0; pass < 2; pass++)
    for (size_t i = 0; i < shapes->size(); i++)
      if ((*shapes)[i].faint == (pass == 0)) ordered.push_back((*shapes)[i]);
  *shapes = ordered;
}

// ---- Completion ----

HeroStatsStar Completion(const HeroStats &stats, int level)
{
  HeroStatsStar star = NewStar("How much of the dungeon they found", "Each axis: a share of what the dungeon holds.");
  static const char *const AXES[HERO_COMPLETION_AXIS_COUNT] = {"Explored", "Items", "Monsters", "Doors", "Switches"};
  static const char *const HELPS[HERO_COMPLETION_AXIS_COUNT] = {
    "Floor cells explored, of the floor cells the party can reach from where it has been, through stairs and teleporters. "
    "An area no way leads into does not count.",
    "Items picked up for the first time, of those and the items still lying on floors, in alcoves and in chests. "
    "An item a monster drops adds to the total; a champion's gear in a mirror does not count.",
    "Monsters killed, by any cause, of those and the monsters still alive. A monster a generator spawns adds to the total.",
    "Doors the party has seen open or broken, or that are open now, of all the doors.",
    "Wall switches, buttons, locks and the like that do something, of those the party has worked at least once."};
  const char *where = level == HERO_STATS_WHOLE_RUN ? " The whole dungeon, levels not visited yet included." : " This level only.";
  for (int axis = 0; axis < HERO_COMPLETION_AXIS_COUNT; axis++) star.helps.push_back(std::string(HELPS[axis]) + where);
  HeroStatsStarShape shape;
  shape.who = HERO_STATS_PARTY;
  shape.faint = false;
  for (int axis = 0; axis < HERO_COMPLETION_AXIS_COUNT; axis++)
  {
    star.axes.push_back(AXES[axis]);
    HeroStatsCompletionAxis value = stats.Completion(level, (HeroCompletionAxis)axis);
    double ratio = Ratio(value.found, value.total);
    shape.values.push_back(ratio);
    shape.texts.push_back(Percent(ratio));
    shape.details.push_back(OfWhole(value.found, value.total));
  }
  star.shapes.push_back(shape);
  PercentRings(&star);
  return star;
}

// ---- How the party played ----

HeroStatsStarShape PlayedShape(const HeroStats &stats, HeroStatsChampion who, int level, bool faint)
{
  HeroStatsRecord record = HeroStatsCatalogue_Record(stats, who, level);
  const int *v = record.value;
  long long used[8][2] = {
    {v[HeroNumHits], v[HeroNumAttacks]},
    {v[HeroNumMelee], v[HeroNumDamage]},
    {v[HeroNumCastsSuccessful], v[HeroNumCasts]},
    {(long long)v[HeroNumAttacksReceived] - v[HeroNumHitsTaken], v[HeroNumAttacksReceived]},
    {v[HeroNumReduced], (long long)v[HeroNumTaken] + v[HeroNumReduced]},
    {v[HeroNumHealPotions], v[HeroNumHealed]},
    {v[HeroNumManaRegeneration], (long long)v[HeroNumManaRegeneration] + v[HeroNumManaRegenLost]},
    {(long long)v[HeroNumFoodGained] + v[HeroNumWaterGained],
     (long long)v[HeroNumFoodGained] + v[HeroNumWaterGained] + v[HeroNumFoodWasted] + v[HeroNumWaterWasted]}};
  HeroStatsStarShape shape;
  shape.who = who;
  shape.faint = faint;
  for (int axis = 0; axis < 8; axis++)
  {
    double ratio = Ratio(used[axis][0], used[axis][1]);
    shape.values.push_back(ratio);
    shape.texts.push_back(Percent(ratio));
    shape.details.push_back(OfWhole(used[axis][0], used[axis][1]));
  }
  return shape;
}

HeroStatsStar Played(const HeroStats &stats, HeroStatsChampion who, int level)
{
  HeroStatsStar star = NewStar("How the party played",
                               "Eight rates, 0-100%. Bigger is better, apart from the two style axes: Melee share and Potion healing.");
  static const char *const AXES[8] = {"Hit rate", "Melee share", "Casts worked", "Blows avoided",
                                      "Stopped by defences", "Potion healing", "Mana used", "Food & water used"};
  star.axes.assign(AXES, AXES + 8);
  static const char *const HELPS[8] = {
    "Attacks that hit a monster, of all attacks made: blows, throws and shots.",
    "Damage dealt at close range (weapons and bare hands), of all damage dealt. A style, not a score: the rest was "
    "dealt by throws, shots and spells.",
    "Spells that worked, of all spells tried. A spell fails when the champion's skill falls short.",
    "Monster attacks that did no harm (dodged, lucky, too weak, or stopped by the defences), of all monster attacks "
    "on the champions.",
    "Damage the defences took off (armour, shields, resistance, protection spells), of that and the damage taken.",
    "Hit points healed by potions, of all healing: regeneration, potions and the heal action. A style, not a score.",
    "Mana regeneration that went into the champions, of all the regeneration. The rest was lost because their mana "
    "was already full: casting more often keeps it flowing.",
    "Food and water that went into the champions, of all they ate and drank. The rest was lost over the maximum, "
    "eating or drinking while already full."};
  star.helps.assign(HELPS, HELPS + 8);
  if (who != HERO_STATS_PARTY)
  {
    star.title = "How they played";
    star.shapes.push_back(PlayedShape(stats, HERO_STATS_PARTY, level, true));
  }
  star.shapes.push_back(PlayedShape(stats, who, level, false));
  PercentRings(&star);
  return star;
}

// ---- What they trained ----

int RankOf(const HeroStatsExperience &xp, HeroStatsChampion who, int cls)
{
  std::vector<HeroRankUp> ups = xp.RankUps(who);  // newest first
  for (size_t i = 0; i < ups.size(); i++)
    if (ups[i].cls == cls) return ups[i].rank;
  int start = xp.StartRank(who, cls);
  return start < 1 ? 1 : start;
}

HeroStatsStar Trained(const HeroStats &stats, HeroStatsChampion who, int level)
{
  const HeroStatsExperience &xp = stats.Experience();
  bool wholeRun = level == HERO_STATS_WHOLE_RUN;
  HeroStatsStar star = NewStar("Ranks per class", "The edge is the best rank anyone reached in this run.");
  for (int cls = 0; cls < HERO_XP_CLASS_COUNT; cls++)
  {
    star.axes.push_back(HeroXp_ClassName(cls));
    std::string name = HeroXp_ClassName(cls);
    star.helps.push_back(wholeRun ? "Each champion's rank as a " + name + " now: the rank they started with, raised by every rank "
                                    "gained. The edge is the best rank any champion reached in any class."
                                  : "The experience each champion gained as a " + name + " on this level. The edge is the "
                                    "biggest gain of any champion in any class here.");
  }
  const std::vector<HeroStatsChampionInfo> &champions = stats.Champions();
  double best = 0;
  for (size_t c = 0; c < champions.size(); c++)
  {
    HeroStatsStarShape shape;
    shape.who = champions[c].id;
    shape.faint = who != HERO_STATS_PARTY && who != shape.who;
    bool any = false;
    for (int cls = 0; cls < HERO_XP_CLASS_COUNT; cls++)
    {
      int gained = xp.Gained(shape.who, level, cls);
      double value = wholeRun ? RankOf(xp, shape.who, cls) : gained;
      any = any || (wholeRun ? value > 1 || gained > 0 : gained > 0);
      if (value > best) best = value;
      shape.values.push_back(value);
      shape.texts.push_back(wholeRun ? HeroXp_RankName((int)value) : Count(gained) + " xp");
      shape.details.push_back(Count(gained) + " experience gained");
    }
    if (any) star.shapes.push_back(shape);
  }
  FaintFirst(&star.shapes);
  if (wholeRun)
  {
    star.centre = 1;
    star.outer = best < 2 ? 2 : best;
    static const int RINGS[3] = {5, 9, 11};
    for (int i = 0; i < 3; i++)
      if (RINGS[i] < star.outer) star.rings.push_back(std::make_pair((double)RINGS[i], HeroXp_RankName(RINGS[i])));
    star.edgeLabel = HeroXp_RankName((int)star.outer);
  }
  else
  {
    star.title = "What they trained here";
    star.caption = "The experience gained on this level, by class. The edge is the biggest gain.";
    star.outer = best < 1 ? 1 : best;
    for (int i = 1; i <= 3; i++)
      star.rings.push_back(std::make_pair(star.outer * i / 3, Count((long long)(star.outer * i / 3)) + " xp"));
  }
  return star;
}

// ---- Where the mana went ----

enum ManaPurpose { ManaAttack, ManaPotions, ManaLight, ManaProtection, ManaOther, ManaWasted, MANA_PURPOSE_COUNT };

ManaPurpose PurposeOf(uint32_t spell)
{
  if (spell == HERO_SPELL_SHIELD_ACTION) return ManaProtection;
  if ((spell & 0xff000000) != 0) return ManaOther;  // item spells, the heal action, no such spell
  static const struct { const char *runes; ManaPurpose purpose; } TABLE[] = {
    {"Ful Ir", ManaAttack}, {"Oh Kath Ra", ManaAttack}, {"Des Ven", ManaAttack}, {"Oh Ven", ManaAttack},
    {"Des Ew", ManaAttack}, {"Zo Kath Ra", ManaAttack},
    {"Vi", ManaPotions}, {"Ya", ManaPotions}, {"Zo Bro Ra", ManaPotions}, {"Ful Bro Ku", ManaPotions},
    {"Oh Bro Ros", ManaPotions}, {"Ya Bro Dain", ManaPotions}, {"Ya Bro Neta", ManaPotions}, {"Vi Bro", ManaPotions},
    {"Ya Bro", ManaPotions}, {"Zo Ven", ManaPotions},
    {"Ful", ManaLight}, {"Oh Ir Ra", ManaLight}, {"Des Ir Sar", ManaLight},
    {"Ya Ir", ManaProtection}, {"Ful Bro Neta", ManaProtection}};
  for (size_t i = 0; i < sizeof TABLE / sizeof TABLE[0]; i++)
    if (HeroStatsView_SpellKey(TABLE[i].runes) == spell) return TABLE[i].purpose;
  return ManaOther;
}

HeroStatsStar Mana(const HeroStats &stats, HeroStatsChampion who, int level)
{
  HeroStatsStar star = NewStar("Where the mana went",
                               "Each champion's mana spent, by what it bought. One shape adds up to 100%: a big shape is no more mana.");
  static const char *const AXES[MANA_PURPOSE_COUNT] = {"Attack", "Potions", "Light", "Protection", "Other", "Wasted"};
  star.axes.assign(AXES, AXES + MANA_PURPOSE_COUNT);
  static const char *const HELPS[MANA_PURPOSE_COUNT] = {
    "Mana spent on attack spells: Fireball, Lightning, Poison bolt, Poison cloud, Weaken nonmaterial and Zokathra.",
    "Mana spent brewing potions into empty flasks: health, stamina, mana, strength, dexterity, wisdom, vitality, "
    "cure poison, shield and poison potions.",
    "Mana spent on light and dark: Torch, Light and Darkness.",
    "Mana spent on protection: Shield party, Fire shield and the shield action of staffs and wands.",
    "Mana spent on everything else: Open door, Footprints, See-through-walls, Invisibility, the spells of staffs and wands, "
    "the heal action, and spells this list does not know.",
    "Mana lost: failed casts, runes that formed no spell, and runes cleared before casting."};
  star.helps.assign(HELPS, HELPS + MANA_PURPOSE_COUNT);
  const std::vector<HeroStatsChampionInfo> &champions = stats.Champions();
  double best = 0;
  for (size_t c = 0; c < champions.size(); c++)
  {
    HeroStatsChampion id = champions[c].id;
    long long mana[MANA_PURPOSE_COUNT] = {0, 0, 0, 0, 0, 0};
    std::vector<HeroStatsSpellRow> spells = stats.Spells(id, level);
    for (size_t i = 0; i < spells.size(); i++)
    {
      if (spells[i].spell == HERO_SPELL_RUNES_CLEARED) continue;  // counted as a rune outcome
      bool special = (spells[i].spell & 0xff000000) != 0 && (spells[i].spell & HERO_SPELL_NO_SUCH_SPELL) == 0;
      // A rune spell's failed casts are in the rune outcomes; a special row keeps its own
      mana[PurposeOf(spells[i].spell)] += spells[i].mana + (special ? spells[i].wasted : 0);
    }
    mana[ManaWasted] = (long long)stats.ManaWasted(id, level) + stats.ManaMeaningless(id, level) + stats.RunesCleared(id, level);
    long long total = 0;
    for (int p = 0; p < MANA_PURPOSE_COUNT; p++) total += mana[p];
    if (total <= 0) continue;
    HeroStatsStarShape shape;
    shape.who = id;
    shape.faint = who != HERO_STATS_PARTY && who != id;
    for (int p = 0; p < MANA_PURPOSE_COUNT; p++)
    {
      double share = Ratio(mana[p], total);
      if (share > best) best = share;
      shape.values.push_back(share);
      shape.texts.push_back(Percent(share));
      shape.details.push_back(Count(mana[p]) + " mana");
    }
    star.shapes.push_back(shape);
  }
  FaintFirst(&star.shapes);
  star.outer = RoundUpToTenth(best, 0.3);
  ThirdRings(&star);
  return star;
}

// ---- Roles in the party ----

HeroStatsStar Roles(const HeroStats &stats, HeroStatsChampion who, int level, bool *show)
{
  HeroStatsStar star = NewStar("Who carried what",
                               "Each champion's share of the party's total. The dashed ring is an even share.");
  static const HeroStatsNumber NUMBERS[6] = {HeroNumDamage, HeroNumKills, HeroNumManaSpent, HeroNumHitsTaken,
                                             HeroNumPotionsBrewed, HeroNumExperience};
  static const char *const AXES[6] = {"Damage", "Kills", "Mana spent", "Blows taken", "Potions brewed", "Experience"};
  star.axes.assign(AXES, AXES + 6);
  static const char *const HELPS[6] = {
    "The champion's share of all the damage the party dealt to monsters.",
    "The champion's share of the monsters the party killed.",
    "The champion's share of all the mana the party spent.",
    "The champion's share of the monster blows that hurt the party: who stood in front.",
    "The champion's share of the potions the party brewed.",
    "The champion's share of all the experience the party gained."};
  star.helps.assign(HELPS, HELPS + 6);
  const std::vector<HeroStatsChampionInfo> &champions = stats.Champions();
  std::vector<HeroStatsRecord> records;
  std::vector<HeroStatsChampion> shown;
  long long party[6] = {0, 0, 0, 0, 0, 0};
  for (size_t c = 0; c < champions.size(); c++)
  {
    HeroStatsRecord record = HeroStatsCatalogue_Record(stats, champions[c].id, level);
    bool any = false;
    for (int n = 0; n < 6; n++) any = any || record.value[NUMBERS[n]] > 0;
    if (!any) continue;
    for (int n = 0; n < 6; n++) party[n] += record.value[NUMBERS[n]];
    records.push_back(record);
    shown.push_back(champions[c].id);
  }
  *show = shown.size() >= 2;
  double best = 0;
  for (size_t c = 0; c < shown.size(); c++)
  {
    HeroStatsStarShape shape;
    shape.who = shown[c];
    shape.faint = who != HERO_STATS_PARTY && who != shown[c];
    for (int n = 0; n < 6; n++)
    {
      int value = records[c].value[NUMBERS[n]];
      double share = Ratio(value, party[n]);
      if (share > best) best = share;
      shape.values.push_back(share);
      shape.texts.push_back(Percent(share));
      shape.details.push_back(Percent(share) + "  " + Count(value));
    }
    star.shapes.push_back(shape);
  }
  FaintFirst(&star.shapes);
  star.evenRing = shown.empty() ? -1 : 1.0 / shown.size();
  star.outer = RoundUpToTenth(best, 0.5);
  star.rings.push_back(std::make_pair(star.outer / 2, std::string("")));
  star.rings.push_back(std::make_pair(star.outer, Percent(star.outer)));
  return star;
}

HeroStatsCompletionRow CompletionRow(const HeroStats &stats, int level)
{
  HeroStatsCompletionRow row;
  row.level = level;
  for (int axis = 0; axis < HERO_COMPLETION_AXIS_COUNT; axis++)
  {
    HeroStatsCompletionAxis value = stats.Completion(level, (HeroCompletionAxis)axis);
    row.values[axis] = Ratio(value.found, value.total);
  }
  return row;
}

}  // namespace

HeroStatsProfile HeroStatsProfile_Make(const HeroStats &stats, HeroStatsChampion who, int level)
{
  HeroStatsProfile profile;
  profile.completion = Completion(stats, level);
  profile.played = Played(stats, who, level);
  profile.trained = Trained(stats, who, level);
  profile.mana = Mana(stats, who, level);
  profile.roles = Roles(stats, who, level, &profile.showRoles);
  std::vector<int> levels = stats.VisitedLevels();
  for (size_t i = 0; i < levels.size(); i++)
  {
    HeroStatsCompletionAxis explored = stats.Completion(levels[i], HeroCompletionExplored);
    HeroStatsLevelBar bar = {levels[i], Ratio(explored.found, explored.total)};
    profile.levelBars.push_back(bar);
    profile.perLevel.push_back(CompletionRow(stats, levels[i]));
  }
  profile.perLevel.push_back(CompletionRow(stats, HERO_STATS_WHOLE_RUN));
  return profile;
}
