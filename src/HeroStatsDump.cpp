// HeroStatsDump.cpp : the hero statistics as text, for map_debug and for looking things up.
#include "HeroStatsDump.h"
#include <stdio.h>

static std::string NameOf(const HeroStats &stats, HeroStatsChampion who)
{
  if (who == HERO_STATS_PARTY) return "PARTY";
  if (who == HERO_STATS_NOBODY) return "nobody";
  for (size_t i = 0; i < stats.Champions().size(); i++)
  {
    if (stats.Champions()[i].id == who) return stats.Champions()[i].name;
  }
  char number[16];
  snprintf(number, sizeof number, "#%d", (int)who);
  return number;
}

// A name goes into a format string: a % in it must not become a directive
static std::string InFormat(const std::string &name)
{
  std::string out;
  for (size_t i = 0; i < name.size(); i++)
  {
    if (name[i] == '%') out += '%';
    out += name[i];
  }
  return out;
}

// The format may use up to seven %d; the arguments it does not use are ignored
static void Line(std::string *out, const std::string &name, const std::string &format, int a = 0,
                 int b = 0, int c = 0, int d = 0, int e = 0, int f = 0, int g = 0)
{
  char text[256];
  snprintf(text, sizeof text, format.c_str(), a, b, c, d, e, f, g);
  *out += name + " " + text + "\n";
}

// A line for a number, unless it is zero
static void Number(std::string *out, const std::string &name, const char *format, int value)
{
  if (value != 0) Line(out, name, format, value);
}

static void DumpOne(const HeroStats &stats, HeroStatsChampion who, std::string *out)
{
  const int run = HERO_STATS_WHOLE_RUN;
  std::string name = NameOf(stats, who);
  *out += name + "\n";
  Number(out, name, "melee %d", stats.DamageDealt(who, run, HeroDamageMelee));
  Number(out, name, "ranged %d", stats.DamageDealt(who, run, HeroDamageRanged));
  Number(out, name, "magic %d", stats.DamageDealt(who, run, HeroDamageMagic));
  Number(out, name, "kills %d", stats.Kills(who, run));
  std::vector<HeroStatsAttackRow> attacks = stats.Attacks(who, run);
  for (size_t i = 0; i < attacks.size(); i++)
  {
    const HeroStatsAttackRow &a = attacks[i];
    Line(out, name, a.thrown ? "attack %d/%d thrown tries %d hits %d damage %d kills %d"
                             : "attack %d/%d tries %d hits %d damage %d kills %d",
         a.weapon, a.option, a.tries, a.hits, a.damage, a.kills);
  }
  std::vector<HeroStatsSpellRow> spells = stats.Spells(who, run);
  for (size_t i = 0; i < spells.size(); i++)
  {
    const HeroStatsSpellRow &s = spells[i];
    char key[32];
    snprintf(key, sizeof key, "spell 0x%08x", (unsigned)s.spell);
    Line(out, name, std::string(key) + " casts %d mana %d failed %d wasted %d damage %d kills %d",
         s.casts, s.mana, s.failed, s.wasted, s.damage, s.kills);
  }
  Number(out, name, "harm %d", stats.Harm(who, run));
  std::vector<HeroStatsHarmRow> harm = stats.HarmRows(who, run);
  for (size_t i = 0; i < harm.size(); i++)
  {
    const HeroStatsHarmRow &h = harm[i];
    Line(out, name, "harm source %d by " + InFormat(NameOf(stats, h.causer)) + " monster %d amount %d",
         (int)h.source, h.monsterType, h.amount);
  }
  Number(out, name, "friendly fire caused %d", stats.FriendlyFireCaused(who, run));
  Number(out, name, "healed %d", stats.Healed(who, run));
  std::vector<HeroStatsHealRow> heals = stats.HealRows(who, run);
  for (size_t i = 0; i < heals.size(); i++)
  {
    Line(out, name, "heal kind %d brewer " + InFormat(NameOf(stats, heals[i].brewer)) + " amount %d",
         (int)heals[i].kind, heals[i].amount);
  }
  Number(out, name, "healing given %d", stats.HealingGiven(who, run));
  Number(out, name, "mana gained %d", stats.ManaGained(who, run));
  Number(out, name, "mana spent %d", stats.ManaSpent(who, run));
  Number(out, name, "mana wasted %d", stats.ManaWasted(who, run));
  Number(out, name, "resurrections %d", stats.Resurrections(who, run));
  std::vector<HeroStatsDeath> deaths = stats.Deaths(who);
  for (size_t i = 0; i < deaths.size(); i++)
  {
    // The champion who caused it (friendly fire) is named; other deaths have nobody
    std::string killer;
    if (deaths[i].by.champion != HERO_STATS_NOBODY) killer = " by " + InFormat(NameOf(stats, deaths[i].by.champion));
    Line(out, name, "death level %d cause %d monster %d" + killer, deaths[i].level, (int)deaths[i].cause,
         deaths[i].by.monsterType);
  }
  if (who != HERO_STATS_PARTY) return;
  std::vector<HeroStatsMonsterRow> monsters = stats.MonsterKills(run);
  for (size_t i = 0; i < monsters.size(); i++)
  {
    const HeroStatsMonsterRow &m = monsters[i];
    Line(out, name, "monster type %d variant %d by %d champion " + InFormat(NameOf(stats, m.champion)) + " count %d",
         m.monsterType, m.variant, (int)m.by, m.count);
  }
  std::vector<int> visited = stats.VisitedLevels();
  if (!visited.empty())
  {
    std::string levels = "visited levels";
    for (size_t i = 0; i < visited.size(); i++)
    {
      char number[16];
      snprintf(number, sizeof number, " %d", visited[i]);
      levels += number;
    }
    *out += name + " " + levels + "\n";
  }
  Number(out, name, "wipes %d", stats.PartyWipes());
  Number(out, name, "reloads %d", stats.Reloads());
}

std::string HeroStatsDump_Text(const HeroStats &stats)
{
  std::string out;
  for (size_t i = 0; i < stats.Champions().size(); i++) DumpOne(stats, stats.Champions()[i].id, &out);
  DumpOne(stats, HERO_STATS_PARTY, &out);
  return out;
}
