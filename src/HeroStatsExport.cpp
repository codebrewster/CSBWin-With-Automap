// HeroStatsExport.cpp : the statistics as one HTML file to share.
#include "HeroStatsExport.h"

#include <algorithm>
#include <cstdio>

// The JSON the page script reads:
// { "dungeon", "date", "separator" (between thousands), "heroes": [{"name", "left"}], "levels": [level, ...],
//   "numbers": the catalogue, one per HeroStatsNumber: [{"label", "help", "party", "run", "bad", "ratio", "pct", "ticks"}],
//   "topics": one per HeroStatsTopic: [{"label", "rows": [[number, depth], ...] (all its rows),
//             "groups": [{"title", "panel" (drawn as a panel of the party's numbers), "rows": [[number, depth,
//             the label in this place or null], ...]}, ...], "tiles": [number, ...],
//             "grid": [number, ...] (Champions side by side), "details": [HeroStatsDetail, ...] (the party's),
//             "heroDetails": [...] (a champion's)}],
//   "heroTiles": [number, ...] (a champion's Overview), "xpFirst", "recFirst": the numbers of Fighter and of
//   the first record,
//   "views": { "-1" for the whole run and one per visited level: {
//     "records": per hero then the party, a value per number, null where it does not apply,
//     "summaries": per hero then the party, one line per topic,
//     "pages": per hero then the party {
//       "weapons": [[name, tries, hits, damage, kills, uses, [[option, tries, hits, damage, kills, uses], ...], [by hero]], ...]
//                  (tries: blows and missiles; uses: actions that cannot hit),
//       "spells": [[name, casts, failed, mana, wasted, damage, kills, [by hero]], ...],
//       "taken": [[text, amount, isBad, [by hero]], ...], "healing": [[text, amount, [by hero]], ...],
//       "deaths": [text, ...], "eaten": [[text, amount, [by hero]], ...], "drunk": [...], "eatenTotal", "drunkTotal",
//       "xp": {"classes": [[name, rank now, rank at the start, % to next, next rank, experience, ranks gained,
//                           best hero (the party's), [by hero], [[skill, experience, [by hero]], ...]], ...],
//              "sources": [[name, note, depth, experience, [by hero; a hero's page: the party's], has children, help], ...],
//              "rankups": [[hero, class, rank, with, level], ...]},
//       "records": [[kind, group, label, value, who, how, whom, when, level, has, held, party value, party's hero], ...] },
//     "monsters": [[name, [kills per hero], door, pit, telefrag, landed, other, total], ...] most killed first,
//     "items": [[name, picked up, used in walls, from chests], ...] } } },
//   "live": the party when the page was written (the Champions tab): {"cursor": the item on the cursor
//     (an item as below) or null, "list": the inventory starts as a list, "icons": [PNG data URI, ...],
//     "champions": [{"hero" (its index in heroes, -1 none), "name", "title", "portrait" (a PNG data URI or
//     null), "hp", "stamina" (in tenths), "mana": [now, max], "food", "water", "load": [tenths, max tenths],
//     "attrs": [[name, current, max], ...], "ranks": [[class, rank, % to next], ...], "poison", "wounds"
//     (as words), "woundBits" (bit n: slot n is wounded), "items": [[slot (the game's number, -1 the
//     cursor), name, in a chest, icon (a place in "icons" or null), weight (tenths of a kilogram),
//     charges (-1 holds none), charges are gulps, food it gives, water it gives (a drink, a waterskin's gulp),
//     damage, kinetic energy, shoot damage (above 0: it shoots), armor, sharp resistance (each -1: the kind has none)], ...]}, ...]}
// [by hero] is empty except on the party's page.

// ---------------------------------------------------------------------------------------------
// JSON

static void AppendEscaped(std::string *out, unsigned int codePoint)
{
  char escaped[8];
  snprintf(escaped, sizeof escaped, "\\u%04x", codePoint);
  *out += escaped;
}

// The length of the UTF-8 sequence at text[i], 0 when the bytes there are not one
static size_t Utf8Length(const std::string &text, size_t i)
{
  unsigned char lead = (unsigned char)text[i];
  size_t length = lead >= 0xf0 && lead <= 0xf4 ? 4 : lead >= 0xe0 ? 3 : lead >= 0xc2 && lead < 0xe0 ? 2 : 0;
  if (length == 0 || i + length > text.size()) return 0;
  for (size_t k = 1; k < length; k++)
    if (((unsigned char)text[i + k] & 0xc0) != 0x80) return 0;
  return length;
}

std::string HeroStatsExport_JsonString(const std::string &text)
{
  std::string out = "\"";
  for (size_t i = 0; i < text.size(); i++)
  {
    unsigned char c = (unsigned char)text[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c < 0x20 || c == '<' || c == '>' || c == '&') AppendEscaped(&out, c);
    else if (c < 0x80) out += (char)c;
    else
    {
      size_t length = Utf8Length(text, i);
      if (length == 0) AppendEscaped(&out, c);  // Latin-1
      else
      {
        out.append(text, i, length);
        i += length - 1;
      }
    }
  }
  return out + "\"";
}

static std::string Number(int value)
{
  return std::to_string(value);
}

static std::string Bool(bool value)
{
  return value ? "true" : "false";
}

// [a,b,c] of numbers
static std::string Numbers(const int *values, size_t count)
{
  std::string out = "[";
  for (size_t i = 0; i < count; i++) out += (i > 0 ? "," : "") + Number(values[i]);
  return out + "]";
}

static std::string Ints(const std::vector<int> &values)
{
  return Numbers(values.data(), values.size());
}

std::string HeroStatsExport_RecordJson(const HeroStatsRecord &record, HeroStatsChampion who, int level)
{
  std::string out = "[";
  for (int n = 0; n < HERO_NUMBER_COUNT; n++)
  {
    if (n > 0) out += ",";
    out += HeroStatsCatalogue_Applies((HeroStatsNumber)n, who, level) ? Number(record.value[n]) : "null";
  }
  return out + "]";
}

static std::string MonsterJson(const HeroStatsMonsterLine &line)
{
  std::string out = "[" + HeroStatsExport_JsonString(line.name) + "," + Ints(line.byColumn);
  int rest[6] = {line.door, line.pit, line.telefrag, line.landed, line.other, line.total};
  for (int i = 0; i < 6; i++) out += "," + Number(rest[i]);
  return out + "]";
}

static std::string AmountLinesJson(const std::vector<HeroStatsAmountLine> &lines, bool withBad)
{
  std::string out = "[";
  for (size_t i = 0; i < lines.size(); i++)
  {
    out += (i > 0 ? ",[" : "[") + HeroStatsExport_JsonString(lines[i].text) + "," + Number(lines[i].amount);
    if (withBad) out += "," + Bool(lines[i].isBad);
    out += "," + Ints(lines[i].byColumn) + "]";
  }
  return out + "]";
}

static std::string PageJson(const HeroStatsHeroPage &page)
{
  std::string out = "{\"weapons\":[";
  for (size_t w = 0; w < page.weapons.size(); w++)
  {
    const HeroStatsWeaponLine &weapon = page.weapons[w];
    int counts[5] = {weapon.tries, weapon.hits, weapon.damage, weapon.kills, weapon.uses};
    out += (w > 0 ? ",[" : "[") + HeroStatsExport_JsonString(weapon.name);
    for (int i = 0; i < 5; i++) out += "," + Number(counts[i]);
    out += ",[";
    for (size_t o = 0; o < weapon.options.size(); o++)
    {
      const HeroStatsOptionLine &option = weapon.options[o];
      int optionCounts[5] = {option.tries, option.hits, option.damage, option.kills, option.uses};
      out += (o > 0 ? ",[" : "[") + HeroStatsExport_JsonString(option.name);
      for (int i = 0; i < 5; i++) out += "," + Number(optionCounts[i]);
      out += "]";
    }
    // Each column's own [tries, hits, damage, kills, uses] with it
    out += "],[";
    for (size_t c = 0; c < weapon.byColumn.size(); c++)
    {
      const HeroStatsOptionLine &mine = weapon.byColumn[c];
      out += std::string(c > 0 ? "," : "") + "[" + Number(mine.tries) + "," + Number(mine.hits) + "," + Number(mine.damage) +
             "," + Number(mine.kills) + "," + Number(mine.uses) + "]";
    }
    out += "]]";
  }
  out += "],\"spells\":[";
  for (size_t i = 0; i < page.spells.size(); i++)
  {
    const HeroStatsSpellLine &spell = page.spells[i];
    int counts[6] = {spell.casts, spell.failed, spell.mana, spell.wasted, spell.damage, spell.kills};
    out += (i > 0 ? ",[" : "[") + HeroStatsExport_JsonString(spell.name);
    for (int k = 0; k < 6; k++) out += "," + Number(counts[k]);
    // Each column's own [casts, failed, mana, wasted, damage, kills] with it
    out += ",[";
    for (size_t c = 0; c < spell.byColumn.size(); c++)
    {
      const HeroStatsSpellCounts &mine = spell.byColumn[c];
      out += std::string(c > 0 ? "," : "") + "[" + Number(mine.casts) + "," + Number(mine.failed) + "," + Number(mine.mana) +
             "," + Number(mine.wasted) + "," + Number(mine.damage) + "," + Number(mine.kills) + "]";
    }
    out += "]]";
  }
  out += "],\"taken\":" + AmountLinesJson(page.taken, true);
  out += ",\"healing\":" + AmountLinesJson(page.healing, false);
  out += ",\"deaths\":[";
  for (size_t i = 0; i < page.deathLines.size(); i++)
    out += (i > 0 ? "," : "") + HeroStatsExport_JsonString(page.deathLines[i]);
  out += "],\"eaten\":" + AmountLinesJson(page.eaten, false) + ",\"drunk\":" + AmountLinesJson(page.drunk, false);
  out += ",\"eatenTotal\":" + Number(page.eatenTotal) + ",\"drunkTotal\":" + Number(page.drunkTotal) + "}";
  return out;
}

// The hero (column) index of a champion, the party's index (the column count) for anyone else
static int HeroIndex(const std::vector<HeroStatsColumn> &columns, HeroStatsChampion who)
{
  size_t i = 0;
  while (i < columns.size() && columns[i].id != who) i++;
  return (int)i;
}

static std::string XpJson(const HeroStatsTopicPage &xp, const std::vector<HeroStatsColumn> &columns)
{
  std::string out = "{\"classes\":[";
  for (size_t c = 0; c < xp.classes.size(); c++)
  {
    const HeroStatsClassLine &line = xp.classes[c];
    out += std::string(c > 0 ? "," : "") + "[" + HeroStatsExport_JsonString(line.name) + "," +
           HeroStatsExport_JsonString(HeroXp_RankName(line.rankNow)) + "," + HeroStatsExport_JsonString(HeroXp_RankName(line.startRank)) +
           "," + Number(line.percentToNext) + "," + HeroStatsExport_JsonString(HeroXp_RankName(line.rankNow + 1)) + "," +
           Number(line.amount) + "," + Number(line.ranksGained) + "," + Number(HeroIndex(columns, line.best)) + "," +
           Ints(line.byColumn) + ",[";
    for (size_t k = 0; k < line.skills.size(); k++)
      out += std::string(k > 0 ? "," : "") + "[" + HeroStatsExport_JsonString(line.skills[k].name) + "," +
             Number(line.skills[k].amount) + "," + Ints(line.skills[k].byColumn) + "]";
    out += "]]";
  }
  out += "],\"sources\":[";
  for (size_t i = 0; i < xp.sources.size(); i++)
  {
    const HeroStatsXpLine &line = xp.sources[i];
    out += std::string(i > 0 ? "," : "") + "[" + HeroStatsExport_JsonString(line.name) + "," + HeroStatsExport_JsonString(line.note) +
           "," + Number(line.depth) + "," + Number(line.amount) + "," + Ints(line.byColumn) + "," + Bool(line.hasChildren) + "," +
           HeroStatsExport_JsonString(line.help) + "]";
  }
  out += "],\"rankups\":[";
  for (size_t i = 0; i < xp.rankUps.size(); i++)
  {
    const HeroStatsRankUpLine &line = xp.rankUps[i];
    out += std::string(i > 0 ? "," : "") + "[" + Number(HeroIndex(columns, line.who)) + "," + HeroStatsExport_JsonString(line.className) +
           "," + HeroStatsExport_JsonString(line.rank) + "," + HeroStatsExport_JsonString(line.with) + "," + Number(line.level) + "]";
  }
  return out + "]}";
}

static std::string RecordLinesJson(const std::vector<HeroStatsRecordLine> &lines, const std::vector<HeroStatsColumn> &columns)
{
  std::string out = "[";
  for (size_t i = 0; i < lines.size(); i++)
  {
    const HeroStatsRecordLine &l = lines[i];
    out += std::string(i > 0 ? "," : "") + "[" + Number(l.kind) + "," + HeroStatsExport_JsonString(l.group) + "," +
           HeroStatsExport_JsonString(l.label) + "," + HeroStatsExport_JsonString(l.value) + "," + HeroStatsExport_JsonString(l.who) +
           "," + HeroStatsExport_JsonString(l.how) + "," + HeroStatsExport_JsonString(l.whom) + "," + HeroStatsExport_JsonString(l.when) +
           "," + Number(l.level) + "," + Bool(l.has) + "," + Bool(l.held) + "," + HeroStatsExport_JsonString(l.partyValue) + "," +
           Number(l.partyHolder == HERO_STATS_NOBODY ? -1 : HeroIndex(columns, l.partyHolder)) + "]";
  }
  return out + "]";
}

static std::string ViewJson(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                            const HeroStatsNames &names, int level)
{
  std::string records = "[", summaries = "[", pages = "[";
  for (size_t i = 0; i <= columns.size(); i++)
  {
    HeroStatsChampion who = i < columns.size() ? columns[i].id : HERO_STATS_PARTY;
    HeroStatsRecord record = HeroStatsCatalogue_Record(stats, who, level);
    std::string comma = i > 0 ? "," : "";
    records += comma + HeroStatsExport_RecordJson(record, who, level);
    summaries += comma + "[";
    for (int t = 0; t < HERO_TOPIC_COUNT; t++)
      summaries += (t > 0 ? "," : "") + HeroStatsExport_JsonString(HeroStatsCatalogue_Summary(t, record, who, level));
    summaries += "]";
    std::string hero = PageJson(HeroStatsPages_Hero(stats, columns, who, level, names));
    hero.pop_back();  // the page's closing brace: the Experience and Records tables join it
    HeroStatsTopicPage xp = HeroStatsPages_Topic(stats, columns, who, level, HeroTopicExperience, names);
    HeroStatsTopicPage records = HeroStatsPages_Topic(stats, columns, who, level, HeroTopicRecords, names);
    pages += comma + hero + ",\"xp\":" + XpJson(xp, columns) + ",\"records\":" + RecordLinesJson(records.records, columns) + "}";
  }
  std::string out = "{\"records\":" + records + "],\"summaries\":" + summaries + "],\"pages\":" + pages + "]";
  std::vector<HeroStatsMonsterLine> monsters = HeroStatsPages_Monsters(stats, columns, level, names);
  out += ",\"monsters\":[";
  for (size_t i = 0; i < monsters.size(); i++) out += (i > 0 ? "," : "") + MonsterJson(monsters[i]);
  out += "],\"items\":[";
  std::vector<HeroStatsItemLine> items = HeroStatsPages_Items(stats, level, names);
  bool first = true;
  for (size_t i = 0; i < items.size(); i++)
  {
    if (items[i].pickedUp + items[i].usedInWall == 0) continue;  // only eaten or drunk: in Eaten and drunk
    out += std::string(first ? "" : ",") + "[" + HeroStatsExport_JsonString(items[i].name) + "," +
           Number(items[i].pickedUp) + "," + Number(items[i].usedInWall) + "," + Number(items[i].fromChest) + "]";
    first = false;
  }
  return out + "]}";
}

static std::string Details(const std::vector<HeroStatsDetail> &details)
{
  std::vector<int> ids(details.begin(), details.end());
  return Ints(ids);
}

static std::string Pair(int a, int b)
{
  return "[" + Number(a) + "," + Number(b) + "]";
}

// One item of the Champions tab: [slot, name, in a chest, icon, weight, charges, gulps, food, water,
// damage, kinetic energy, shoot damage, armor, sharp resistance] (the last five -1 for none). icon: its
// place in the icons written so far, added on first use; null without a picture.
static std::string LiveItemJson(const HeroStatsLiveItem &item, const std::map<int, std::string> &pictures,
                                std::vector<int> *iconKeys)
{
  std::string icon = "null";
  int key = HeroStatsLive_IconKey(item);
  if (key >= 0 && pictures.find(key) != pictures.end())
  {
    size_t at = std::find(iconKeys->begin(), iconKeys->end(), key) - iconKeys->begin();
    if (at == iconKeys->size()) iconKeys->push_back(key);
    icon = Number((int)at);
  }
  return "[" + Number(item.slot) + "," + HeroStatsExport_JsonString(item.name) + "," + Bool(item.inChest) + "," + icon + "," +
         Number(item.weight) + "," + Number(item.charges) + "," + Bool(item.gulps) + "," + Number(item.food) + "," +
         Number(item.water) + "," + Number(item.damage) + "," + Number(item.kinetic) + "," + Number(item.shootDamage) + "," +
         Number(item.armor) + "," + Number(item.sharpResistance) + "]";
}

static std::string LiveJson(const HeroStatsExportInfo &info, const std::vector<HeroStatsColumn> &columns)
{
  const HeroStatsLiveParty &live = info.live;
  std::vector<int> iconKeys;
  std::string champions;
  for (size_t i = 0; i < live.champions.size(); i++)
  {
    const HeroStatsLiveChampion &c = live.champions[i];
    int hero = -1;
    for (size_t k = 0; k < columns.size(); k++)
      if (columns[k].id == c.who) hero = (int)k;
    std::map<HeroStatsChampion, std::string>::const_iterator portrait = info.portraits.find(c.who);
    std::string &out = champions;
    out += std::string(i > 0 ? "," : "") + "{\"hero\":" + Number(hero) + ",\"name\":" + HeroStatsExport_JsonString(c.name) +
           ",\"title\":" + HeroStatsExport_JsonString(c.title) +
           ",\"portrait\":" + (portrait == info.portraits.end() ? "null" : HeroStatsExport_JsonString(portrait->second)) +
           ",\"hp\":" + Pair(c.health, c.maxHealth) +
           ",\"stamina\":" + Pair(c.stamina, c.maxStamina) + ",\"mana\":" + Pair(c.mana, c.maxMana) +
           ",\"food\":" + Number(c.food) + ",\"water\":" + Number(c.water) + ",\"load\":" + Pair(c.load, c.maxLoad) +
           ",\"attrs\":[";
    for (int a = 0; a < HERO_LIVE_ATTRIBUTE_COUNT; a++)
      out += std::string(a > 0 ? "," : "") + "[" + HeroStatsExport_JsonString(HeroStatsLive_AttributeName(a)) + "," +
             Number(c.attributeNow[a]) + "," + Number(c.attributeMax[a]) + "]";
    out += "],\"ranks\":[";
    for (int cls = 0; cls < 4; cls++)
      out += std::string(cls > 0 ? "," : "") + "[" + HeroStatsExport_JsonString(HeroXp_ClassName(cls)) + "," +
             HeroStatsExport_JsonString(HeroXp_RankName(c.rank[cls])) + "," + Number(c.percentToNext[cls]) + "]";
    std::string doses;
    for (size_t k = 0; k < c.poisonDoses.size(); k++) doses += (k > 0 ? "," : "") + Number(c.poisonDoses[k]);
    out += "],\"poison\":" + Number(c.poisonCount) + ",\"poisonSum\":" + Number(c.poison) + ",\"poisonDoses\":[" + doses +
           "],\"wounds\":" + HeroStatsExport_JsonString(HeroStatsLive_Wounds(c.wounds)) + ",\"woundBits\":" + Number(c.wounds) +
           ",\"items\":[";
    for (size_t k = 0; k < c.items.size(); k++)
      out += std::string(k > 0 ? "," : "") + LiveItemJson(c.items[k], info.icons, &iconKeys);
    out += "]}";
  }
  std::string cursor = live.cursor.name.empty() ? "null" : LiveItemJson(live.cursor, info.icons, &iconKeys);
  std::string icons;
  for (size_t i = 0; i < iconKeys.size(); i++)
    icons += std::string(i > 0 ? "," : "") + HeroStatsExport_JsonString(info.icons.find(iconKeys[i])->second);
  return "{\"cursor\":" + cursor + ",\"list\":" + Bool(info.inventoryList) + ",\"icons\":[" + icons + "],\"champions\":[" +
         champions + "]}";
}

std::string HeroStatsExport_Json(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                 const HeroStatsNames &names, const HeroStatsExportInfo &info)
{
  std::string out = "{\"dungeon\":" + HeroStatsExport_JsonString(info.dungeon);
  out += ",\"date\":" + HeroStatsExport_JsonString(info.date);
  out += ",\"separator\":" + HeroStatsExport_JsonString(HeroStatsView_ThousandsSeparator());
  out += ",\"heroes\":[";
  for (size_t i = 0; i < columns.size(); i++)
    out += std::string(i > 0 ? "," : "") + "{\"name\":" + HeroStatsExport_JsonString(columns[i].name) +
           ",\"left\":" + Bool(columns[i].hasLeft) + "}";
  std::vector<int> levels = stats.VisitedLevels();
  out += "],\"levels\":" + Ints(levels);
  out += ",\"numbers\":[";
  for (int n = 0; n < HERO_NUMBER_COUNT; n++)
  {
    const HeroStatsNumberInfo &number = HeroStatsCatalogue_Info((HeroStatsNumber)n);
    out += std::string(n > 0 ? "," : "") + "{\"label\":" + HeroStatsExport_JsonString(number.label) +
           ",\"help\":" + HeroStatsExport_JsonString(number.help) + ",\"party\":" + Bool(number.partyOnly) +
           ",\"run\":" + Bool(number.wholeRunOnly) + ",\"bad\":" + Bool(number.isBad) + ",\"ratio\":" +
           Bool(number.isRatio) + ",\"pct\":" + Bool(n == HeroNumHitRate) + ",\"ticks\":" + Bool(number.isTicks) +
           ",\"rec\":" + Bool(number.isRecord) + "}";
  }
  out += "],\"topics\":[";
  for (int t = 0; t < HERO_TOPIC_COUNT; t++)
  {
    const HeroStatsTopicInfo &topic = HeroStatsCatalogue_Topic(t);
    out += std::string(t > 0 ? "," : "") + "{\"label\":" + HeroStatsExport_JsonString(topic.label) + ",\"rows\":[";
    for (size_t r = 0; r < topic.rows.size(); r++)
      out += std::string(r > 0 ? ",[" : "[") + Number(topic.rows[r].number) + "," + Number(topic.rows[r].depth) + "]";
    out += "],\"groups\":[";
    for (size_t g = 0; g < topic.groups.size(); g++)
    {
      const HeroStatsTopicGroup &group = topic.groups[g];
      out += std::string(g > 0 ? "," : "") + "{\"title\":" + HeroStatsExport_JsonString(group.title) + ",\"panel\":" + Bool(group.panel) +
             ",\"rows\":[";
      for (size_t r = 0; r < group.rows.size(); r++)
        out += std::string(r > 0 ? ",[" : "[") + Number(group.rows[r].number) + "," + Number(group.rows[r].depth) + "," +
               (group.rows[r].label != NULL ? HeroStatsExport_JsonString(group.rows[r].label) : std::string("null")) + "]";
      out += "]}";
    }
    std::vector<int> tiles(topic.tiles.begin(), topic.tiles.end());
    std::vector<HeroStatsNumber> gridNumbers = HeroStatsCatalogue_GridNumbers(t);
    std::vector<int> grid(gridNumbers.begin(), gridNumbers.end());
    out += "],\"tiles\":" + Ints(tiles) + ",\"grid\":" + Ints(grid);
    out += ",\"details\":" + Details(HeroStatsPages_Details(t, HERO_STATS_PARTY));
    out += ",\"heroDetails\":" + Details(HeroStatsPages_Details(t, HERO_STATS_NOBODY)) + "}";
  }
  std::vector<int> heroTiles(HeroStatsCatalogue_ChampionTiles().begin(), HeroStatsCatalogue_ChampionTiles().end());
  out += "],\"heroTiles\":" + Ints(heroTiles);
  out += ",\"xpFirst\":" + Number(HeroNumXpFighter) + ",\"recFirst\":" + Number(HeroNumRecFirst);
  out += ",\"views\":{\"-1\":" + ViewJson(stats, columns, names, HERO_STATS_WHOLE_RUN);
  for (size_t i = 0; i < levels.size(); i++)
    out += ",\"" + Number(levels[i]) + "\":" + ViewJson(stats, columns, names, levels[i]);
  return out + "},\"live\":" + LiveJson(info, columns) + "}";
}

// ---------------------------------------------------------------------------------------------
// The page

// The look of the Statistics window (the colours of HeroStatsDraw.cpp)
static const char *const PAGE_HEAD = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Statistics</title>
<style>
  :root { color-scheme: dark; --bg:#1e1f24; --panel:#272930; --hover:#2e3139; --line:#31343d; --row:#292b32;
    --text:#d8d8d8; --part:#b4b6bf; --dim:#8a8d98; --faint:#5a5d68; --on:#4272b8; --bad:#e5735f; --track:#2f323a;
    --c0:#5fbf5f; --c1:#d8c447; --c2:#d9604f; --c3:#5b8fd9 }
  /* The page fills the browser window: the panel takes the width and the height left under the
     heading, and only the view inside it scrolls */
  html, body { height:100% }
  body { background:#121317; color:#ccc; font:14px system-ui,sans-serif; margin:0; padding:24px; box-sizing:border-box;
    display:flex; flex-direction:column }
  @media (max-width:700px) { body { padding:10px } }
  h1 { font-size:18px; color:#fff; margin:0 0 4px; font-weight:600 }
  .about { color:#999; margin-bottom:12px }
  .im { flex:1 0 360px; min-height:360px; display:flex; flex-direction:column; background:var(--bg); color:var(--text); font:12.5px/1.45 "DejaVu Sans Mono","Bitstream Vera Sans Mono",Menlo,Consolas,monospace;
    border:1px solid var(--line); border-radius:6px; overflow:hidden }
  .bar1 { display:flex; justify-content:space-between; align-items:center; gap:10px; flex-wrap:wrap; padding:8px 10px 0 }
  .whos, .seg { display:flex; gap:4px; flex-wrap:wrap; align-items:center }
  .seg { gap:1px }
  button { font:inherit; cursor:pointer }
  .who { background:transparent; color:var(--text); border:1px solid #3a3d46; border-radius:999px; padding:2px 11px; display:flex; gap:6px; align-items:center }
  .who:hover { background:var(--hover) } .who.on { background:#2a3a57; border-color:#5b8fd9; color:#fff } .who.left { color:var(--dim) }
  .dot { width:8px; height:8px; border-radius:50%; display:inline-block; flex:none } .left .dot, th.left .dot { opacity:.45 }
  .ch { background:var(--panel); color:var(--dim); border:0; border-radius:3px; padding:3px 8px }
  .ch:hover { background:var(--hover) } .ch.on { background:var(--on); color:#fff } .ch:disabled { opacity:.35; cursor:default }
  .lab { color:var(--faint); margin:0 6px 0 8px }
  select { background:var(--panel); color:var(--text); border:1px solid #4a4d57; font:inherit; border-radius:3px }
  .tabs { display:flex; gap:2px; padding:6px 10px 0; border-bottom:1px solid var(--line); flex-wrap:wrap }
  .tab { background:none; border:0; color:var(--dim); padding:5px 10px 6px; border-bottom:2px solid transparent; margin-bottom:-1px }
  .tab:hover { color:#fff } .tab.on { color:#fff; border-bottom-color:var(--on) }
  #view { flex:1 1 0; min-height:0; overflow:auto; padding:10px 12px 14px }
  /* The headline numbers: a strip, each behind a thin line, no boxes */
  .tiles { display:grid; grid-template-columns:repeat(4,1fr); gap:14px 0; margin:4px 0 16px }
  .tile { border-left:1px solid var(--line); padding:0 14px; min-width:0 }
  .tile .v { font-size:22px; line-height:1.2; color:#fff; white-space:nowrap } .tile.bad .v { color:var(--bad) } .tile .l { color:var(--dim) }
  .cards { display:grid; grid-template-columns:repeat(4,1fr); gap:8px; margin-bottom:12px }
  .card { background:var(--panel); border-radius:6px; padding:8px 12px 10px; border-top:2px solid; cursor:pointer; display:grid; gap:3px }
  .card:hover { background:var(--hover) } .card .nm { color:#fff; display:flex; justify-content:space-between } .card .nm span { color:var(--dim) }
  /* Every bar starts at the same x and is as long on every card: the label column is as wide as
     the widest label, the number column as the widest number of any card */
  .cr { display:grid; grid-template-columns:6ch 1fr var(--nw,9ch); align-items:center; gap:10px } .cr .n { white-space:nowrap } .cr .l { color:var(--dim) } .cr .n { text-align:right }
  .mini { height:6px; background:var(--track); border-radius:2px; overflow:hidden } .mini i { display:block; height:100% }
  .topics { display:grid; grid-template-columns:1fr 1fr; gap:8px; margin-bottom:12px }
  .topic { background:var(--panel); border-radius:6px; padding:7px 10px; cursor:pointer }
  .topic:hover { background:var(--hover) } .topic .t { color:#fff } .topic .s { color:var(--dim) }
  .sec { margin-bottom:10px } .sech { display:flex; align-items:baseline; gap:8px; padding:3px 0 5px; border-bottom:1px solid var(--line); cursor:pointer; user-select:none }
  .sech .t { text-transform:uppercase; color:var(--text) } .sech:hover .t { color:#fff }
  .sech .a { color:var(--dim); width:10px } .sech .s { color:var(--dim); margin-left:auto }
  .closed .secb { display:none }
  table { border-collapse:collapse; width:100% }
  /* No column has a width of its own: a number column is as wide as its widest text, a bar takes
     a share of the table, the label the rest. A table too wide for the page gives up its bar,
     then the champions' names, half its padding, the columns worked out from others, then lets
     its labels wrap, shortens its big numbers, and at last scrolls on
     its own (fitTables in the script decides, as HeroStatsLayout_FitTable does for the window). */
  td, th { padding:3px 8px; text-align:right; white-space:nowrap; border-bottom:1px solid var(--row); width:1% }
  td:first-child, th:first-child { width:auto }
  th { color:var(--dim); font-weight:normal; border-bottom-color:var(--line); position:sticky; top:-10px; background:var(--bg); z-index:1 }
  .c-bar { width:26%; min-width:110px }
  .nobar .c-bar, .noder .c-der, .nokin .c-kin { display:none }
  /* then the champions' names in the header (their dots stay), half the padding, and at the end
     the long form of big numbers */
  .dots .hn { display:none } .tight td, .tight th { padding:3px 4px }
  .sh { display:none } .short .sh { display:inline } .short .fu { display:none }
  .wrap td:first-child { white-space:normal; min-width:120px }
  .scrolls { overflow-x:auto } .scrolls th { position:static }
  .scrolls td:first-child, .scrolls th:first-child { position:sticky; left:0; background:var(--bg) }
  .scrolls tr.all td:first-child { background:var(--panel) }
  .measure table, .measure td, .measure th { width:auto } .measure .c-bar { width:110px }
  /* The guide: the row under the pointer, and the column under it */
  tr:hover td { background:#2b3140; color:#fff } tr:hover td:first-child { box-shadow:inset 2px 0 #5b8fd9 }
  .scrolls tr:hover td:first-child { background:#2b3140 }
  th.hc { color:#fff; box-shadow:inset 0 -2px #5b8fd9 } td.hc { background:rgba(91,143,217,.05) } tr:hover td.hc { background:#36405a }
  .secs.two { display:grid; grid-template-columns:minmax(0,1fr) minmax(0,1fr); gap:0 22px; align-items:start }
  th.sort { cursor:pointer; user-select:none } th.sort:hover { color:#fff } th .arrow { color:var(--dim) }
  td:first-child, th:first-child { text-align:left }
  tr.d1 td, tr.d2 td { color:var(--part) } tr.d1 td:first-child { padding-left:22px; color:var(--dim) }
  tr.d2 td:first-child { padding-left:38px; color:var(--dim) }
  tr.all td { background:var(--panel); color:#fff }
  tr.lv { cursor:pointer } tr.lv:hover td { background:var(--hover) }
  tr.wrow { cursor:pointer } tr.wrow:hover td { background:var(--hover) }
  tr.opt td { color:var(--part) } tr.opt td:first-child { padding-left:26px; color:var(--dim) }
  tr.grp td { color:var(--dim); text-transform:uppercase; letter-spacing:.05em; font-size:11px; padding-top:9px }
  tr.grp:hover td { background:none; color:var(--dim) } tr.grp:hover td:first-child { box-shadow:none }
  /* A part of the number above it: behind a thin line */
  tr.g1 td, tr.g2 td { color:var(--part) } tr.g1 td:first-child { padding-left:13px } tr.g2 td:first-child { padding-left:29px }
  .ind { display:inline-block; border-left:1px solid var(--faint); padding-left:9px }
  /* A label with a hovertext says so */
  .tip { border-bottom:1px dotted var(--faint); cursor:help }
  /* The numbers only the party has: a panel per group */
  .facts { display:grid; gap:10px; margin:8px 0 4px }
  .fact { background:var(--panel); border-radius:6px; padding:8px 12px 9px }
  .fact h4 { margin:0 0 4px; font:inherit; font-size:11px; letter-spacing:.05em; text-transform:uppercase; color:var(--dim) }
  .fr { display:flex; justify-content:space-between; gap:12px; padding:2px 0 } .fr .v { white-space:nowrap }
  .fr.g1 .k, .fr.g2 .k { color:var(--part) }
  tr.xrow { cursor:pointer } tr.xrow:hover td { background:var(--hover) }
  .gold { color:#e0b54a } .tile .sub { color:var(--text); font-size:11.5px; margin-top:3px } .tile .sub.f { color:var(--faint) }
  .prog { height:5px; background:var(--track); border-radius:2px; overflow:hidden; margin-top:5px } .prog i { display:block; height:100% }
  .nt { color:var(--faint) } table td.l, table th.l { text-align:left }
  .z, table tr td.z { color:var(--faint) } .gone { color:var(--dim) }  /* a zero is faint in part and total rows too */
  .pill { color:var(--bad); background:rgba(229,115,95,.16); border-radius:3px; padding:0 4px }
  .share { display:flex; height:9px; background:var(--track); border-radius:2px; overflow:hidden; min-width:60px } .share i { display:block; height:100% }
  .choices { display:flex; gap:10px; align-items:center; flex-wrap:wrap; margin-bottom:8px }
  .note { color:var(--dim); padding:4px 0 }
  /* The Champions tab. A card and the champion's own column stop growing, so no bar gets long. */
  #view { container-type:inline-size }
  .vit { display:grid; grid-template-columns:max-content minmax(40px,1fr) max-content; gap:4px 10px; align-items:center }
  .vit .l, .lranks .l { color:var(--dim) } .vit .v { text-align:right; white-space:nowrap } .vit .w { grid-column:2 / 4 }
  .bad { color:var(--bad) } .none { color:var(--dim) } .raised { color:#e0b54a }
  .vbar { height:9px; background:var(--track); border-radius:2px; overflow:hidden } .vbar i { display:block; height:100% }
  .lranks { display:grid; grid-template-columns:max-content max-content minmax(30px,1fr); gap:3px 10px; align-items:center }
  .chead { display:flex; gap:10px; align-items:center; min-width:0 }
  .chead img { width:64px; height:58px; image-rendering:pixelated; flex:none; border:1px solid #3a3d46; background:#17181c }
  .chead .nm { font-size:14px } .chead .ti { color:var(--dim) } .chead .grow { flex:1; min-width:0 }
  .lcards { display:grid; gap:10px; justify-content:start }
  .lcard { background:var(--panel); border:1px solid transparent; border-radius:5px; padding:10px 12px 12px; display:grid; gap:10px; cursor:pointer; min-width:0; align-content:start }
  .lcard:hover { border-color:#3a3d46 }
  .strip { display:flex; flex-wrap:wrap; gap:4px; align-items:center } .strip .slot { width:36px; height:36px } .strip .more { color:var(--dim); margin-left:6px }
  .oncursor { display:flex; gap:10px; align-items:center; color:var(--dim); margin-bottom:10px } .oncursor b { color:var(--text); font-weight:normal }
  .slot { width:40px; height:40px; background:#17181c; border:1px solid #2a2c33; border-radius:3px; display:grid; place-items:center; position:relative; flex:none }
  .slot img { width:32px; height:32px; image-rendering:pixelated; display:block }
  .slot.has { background:#484848; border-color:#3a3d46 } .slot.has:hover, .slot.pin { border-color:#fff }
  .slot.wound { border-color:var(--bad); box-shadow:inset 0 0 0 1px var(--bad) }
  .slot .cnt { position:absolute; right:1px; bottom:0; font-size:10px; color:#fff; background:rgba(0,0,0,.72); border-radius:2px; padding:0 3px; line-height:13px }
  .slot .gh { color:#3a3d46; font-size:9.5px; text-transform:uppercase } .noicon { color:var(--dim) }
  .champ { display:grid; gap:14px 36px; grid-template-columns:minmax(0,1fr); grid-template-areas:"v" "a" "i"; align-items:start }
  .champ > * { min-width:0 } .a-v { grid-area:v; display:grid; gap:10px } .a-a { grid-area:a } .a-i { grid-area:i }
  .sech .seg { display:inline-flex; vertical-align:middle } .sech .ch { padding:0 8px }
  .inv { display:flex; flex-wrap:wrap; gap:14px 20px; padding-top:10px }
  .doll { display:grid; grid-template-columns:repeat(5,40px); grid-auto-rows:40px; gap:4px; flex:none }
  .cap { color:var(--faint); font-size:10px; text-transform:uppercase; letter-spacing:.05em; align-self:end; white-space:nowrap }
  .pack { display:grid; gap:6px; align-content:start } .pack .grid { display:grid; grid-template-columns:repeat(6,40px); gap:4px }
  .ilist td.ic { padding-right:0 } .ilist img { width:16px; height:16px; image-rendering:pixelated; display:block }
  .ilist td.dmg { color:#e39a7c } .ilist td.arm { color:#a9b8cf }
  .abar { position:relative; height:9px; background:var(--track); border-radius:2px; overflow:hidden; min-width:90px }
  .abar i { position:absolute; top:0; bottom:0; display:block }
  .abar .lost { background:repeating-linear-gradient(135deg, var(--bad) 0 2px, transparent 2px 4px) }
  .abar .over { background:#e0b54a; box-shadow:inset 2px 0 var(--panel) }
  td.c-bar .abar { min-width:110px }
  .cfull { display:grid; gap:10px } .psec { border-top:1px solid var(--line); padding-top:10px }
  .lcards:not(.full) .cfull, .lcards.full .strip { display:none }
  .cfull .inv { flex-direction:column; flex-wrap:nowrap; padding-top:0; gap:8px }
  .cfull .slot { width:36px; height:36px } .cfull .doll { grid-template-columns:repeat(5,36px); grid-auto-rows:36px }
  .cfull .pack .grid { grid-template-columns:repeat(6,36px) }
  .ilist .inl { display:inline-flex; gap:8px; align-items:center; padding-left:14px; color:var(--part) }
  @container (min-width:640px) { .champ { grid-template-columns:minmax(0,460px) auto; grid-template-rows:auto 1fr; grid-template-areas:"v i" "a i"; justify-content:start } .inv { flex-direction:column } }
  @container (min-width:1040px) { .inv { flex-direction:row; flex-wrap:nowrap } }
  #tip { position:fixed; z-index:20; pointer-events:none; background:var(--panel); border:1px solid #4a4e5a; border-radius:4px; padding:8px 10px;
    font:12.5px/1.45 "DejaVu Sans Mono","Bitstream Vera Sans Mono",Menlo,Consolas,monospace; color:var(--text); max-width:320px; box-shadow:0 6px 18px rgba(0,0,0,.5) }
  #tip .h { display:flex; gap:10px; align-items:center } #tip .h img { width:48px; height:48px; image-rendering:pixelated; flex:none }
  #tip .n { color:#fff } #tip .s { color:var(--dim) } #tip .x { color:var(--part) } #tip .bad { color:var(--bad) } #tip .dmg { color:#e39a7c } #tip .arm { color:#a9b8cf } #tip .food { color:#c99a6c } #tip .water { color:#4a8fe0 }
  #tip .in { border-top:1px solid var(--line); margin-top:8px; padding-top:7px; display:grid; grid-template-columns:1fr 1fr; gap:4px 14px }
  #tip .in div { display:flex; gap:6px; align-items:center; white-space:nowrap } #tip .in img { width:16px; height:16px; image-rendering:pixelated }
  #tip .in .cap { grid-column:1 / -1; color:var(--dim); font-size:inherit; text-transform:none; letter-spacing:0 }
</style></head>
<body>
)HTML";

static const char *const PAGE_SCRIPT = R"HTML(
<script>
const D = JSON.parse(document.getElementById('stats').textContent);
const H = D.heroes, N = H.length, NUM = D.numbers, TOP = D.topics;
const COLOUR = i => `var(--c${i % 4})`;
const DETAIL = {WEAPONS: 0, SPELLS: 1, HARM: 2, HEALING: 3, DEATHS: 4, CONSUMED: 5, ITEMS: 6, MONSTERS: 7,
                SKILLS: 8, XP_SOURCES: 9, RANK_UPS: 10, RECORDS: 11};
const TOPIC_MONSTERS = 6, TOPIC_RECORDS = 8;
// The tab after the topics: the champions when the page was written
const CHAMPIONS = TOP.length, LIVE = D.live || {cursor: null, list: false, icons: [], champions: []};
const NUMBERS_SECTION = 'numbers', KEY_SECTION = 'key';
const st = {who: N, topic: 0, level: -1, per: false, side: false, grid: {}, closed: {}, sort: {}, open: {}, compact: false, list: !!(D.live && D.live.list)};

const esc = s => String(s).replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'})[c]);
const fmt = n => String(n).replace(/\B(?=(\d{3})+(?!\d))/g, D.separator);
const lvName = l => l === -1 ? 'Whole run' : 'Lv ' + l;
const whoName = i => i === N ? 'Party' : H[i].name;
const isParty = () => st.who === N;
const view = l => D.views[String(l)];
const rec = (i, l) => view(l).records[i];
const page = () => view(st.level).pages[st.who];
const applies = (n, i, l) => !(NUM[n].party && i < N) && !(NUM[n].run && l !== -1);
const text = (n, v) => (NUM[n].ratio || NUM[n].rec) && v < 0 ? '-' : NUM[n].pct ? v + '%' :
  n === D.recFirst + 11 ? fmt(Math.floor(v / 10)) + '.' + (v % 10) + ' kg' : n === D.recFirst + 7 ? fmt(v) + ' HP left' : fmt(v);
const sum = a => a.reduce((x, y) => x + Math.max(0, y || 0), 0);
const dotOf = i => `<span class="dot" style="background:${COLOUR(i)}"></span>`;

function set(changes) { Object.assign(st, changes); draw(); }

// A number in a cell: a dim dot for zero, bad news on a red pill, nothing where it does not apply
function cell(v, bad, ratio, label, cls) {
  const c = cls ? ` class="${cls}"` : '';
  if (v === null || v === undefined) return `<td${c}></td>`;
  if (v === 0) return `<td class="z">·</td>`;
  if (ratio && v < 0) return `<td class="z">-</td>`;
  // A plain number from 10,000 on has a short form, for a table that fits no other way
  if (typeof v === 'number' && v >= 10000 && label === fmt(v)) label = `<span class="fu">${label}</span><span class="sh"${tipOf(label)}>${shortNum(v)}</span>`;
  return bad && v > 0 ? `<td${c}><span class="pill">${label}</span></td>` : `<td${c}>${label}</td>`;
}
// 86300 -> 86k, 1204880 -> 1.2M (HeroStatsView_ShortNumber)
function shortNum(v) {
  const k = Math.floor((v + 500) / 1000);
  if (k < 1000) return k + 'k';
  const tenths = Math.floor((v + 50000) / 100000);
  return Math.floor(tenths / 10) + '.' + (tenths % 10) + 'M';
}
const numCell = (n, v, cls) => cell(v, NUM[n].bad, NUM[n].ratio, v === null ? '' : text(n, v), cls);
const plain = (v, bad, cls) => cell(v, bad, false, fmt(v), cls);
function shareBar(values) {
  const t = sum(values);
  return '<div class="share">' + (t ? values.map((v, i) => v > 0 ? `<i style="width:${100 * v / t}%;background:${COLOUR(i)}"></i>` : '').join('') : '') + '</div>';
}
const partBar = (part, whole, i) =>
  `<div class="share"><i style="width:${whole > 0 ? Math.min(100, 100 * part / whole) : 0}%;background:${COLOUR(i)}"></i></div>`;

function section(id, title, summary, body) {
  return `<div class="sec${st.closed[id] ? ' closed' : ''}"><div class="sech" onclick="fold('${id}')"><span class="a">${st.closed[id] ? '&#9656;' : '&#9662;'}</span>` +
    `<span class="t">${esc(title)}</span><span class="s">${summary}</span></div><div class="secb">${body}</div></div>`;
}
function fold(id) { st.closed[id] = !st.closed[id]; draw(); }

// A table whose columns sort when their header is clicked; the foot row stays last.
// cols: [{label, num}], rows: [{cells: [{v, html}], cls, act, sub: [rows kept under it]}]
function sortTable(id, cols, rows, foot, defaultCol) {
  const s = st.sort[id] || {col: defaultCol, desc: defaultCol !== null && defaultCol > 0};
  if (s.col !== null && s.col !== undefined) rows = rows.slice().sort((a, b) => {
    const x = a.cells[s.col].v, y = b.cells[s.col].v;
    const c = typeof x === 'number' && typeof y === 'number' ? x - y : String(x).localeCompare(String(y));
    return s.desc ? -c : c;
  });
  // A Who column ({who: {split, fallback}}) splits the sorted column when it can be split among
  // the champions, else its table's own; its cells ({by: column => values}) draw that
  const whoCol = cols.find(c => c.who);
  const measure = whoCol ? (whoCol.who.split.includes(s.col) ? s.col : whoCol.who.fallback) : null;
  const label = c => c.who ? 'Who: ' + cols[measure].label : c.label;
  const head = cols.map((c, i) => `<th class="sort${c.cls ? ' ' + c.cls : ''}"${c.help ? tipOf(esc(c.help)) : ''}${c.hero !== undefined ? ` data-name="${esc(H[c.hero].name)}"` : ''} onclick="sortBy('${id}',${i},${c.num ? 1 : 0})">` +
    (c.html || esc(label(c))) + (s.col === i ? ` <span class="arrow">${s.desc ? '&#9660;' : '&#9650;'}</span>` : '') + '</th>').join('');
  const html = c => c.by ? whoBar(c.by(measure)) : c.html;
  const line = r => `<tr${r.cls ? ` class="${r.cls}"` : ''}${r.act ? ` onclick="${r.act}"` : ''}>` + r.cells.map(html).join('') + '</tr>';
  let body = '';
  for (const r of rows) { body += line(r); for (const sub of r.sub || []) body += line(sub); }
  if (foot) body += line(foot);
  return `<div class="wide"><table><tr>${head}</tr>${body}</table></div>`;
}
function sortBy(id, col, num) {
  const s = st.sort[id];
  st.sort[id] = s && s.col === col ? {col, desc: !s.desc} : {col, desc: !!num};
  draw();
}
const nameCol = (label, help) => ({label, help});
const whoColumn = (split, fallback) => ({label: 'Who', num: true, who: {split, fallback}});
// Each champion's part as a bar; hovered, their numbers
const whoBar = by => {
  const parts = by.map((v, i) => v ? esc(H[i].name) + ' ' + fmt(v) : '').filter(x => x);
  return `<td class="whobar"${parts.length ? tipOf(parts.join('<br>')) : ''}>${shareBar(by)}</td>`;
};
const numCol = (label, help) => ({label, num: true, help});
// hn: the name, which a table too wide leaves out (the dot stays)
const heroHead = i => `${dotOf(i)} <span class="hn">${esc(H[i].name)}</span>`;
const heroCol = i => ({label: H[i].name, html: heroHead(i), num: true, cls: H[i].left ? 'left' : '', hero: i});
const td = (v, html) => ({v, html});
const nameTd = name => td(name, `<td>${esc(name)}</td>`);

// ---------------------------------------------------------------------------------------------

function picker() {
  const L = D.levels, one = !st.per;
  let out = `<div class="seg"><button class="ch${one && st.level === -1 ? ' on' : ''}" onclick="set({per:false,level:-1})">Whole run</button>` +
    `<button class="ch${st.per ? ' on' : ''}" onclick="set({per:true})">Per level</button><span class="lab">Lv</span>`;
  if (L.length <= 16 && !st.compact)
    return out + L.map(l => `<button class="ch${one && st.level === l ? ' on' : ''}" onclick="set({per:false,level:${l}})">${l}</button>`).join('') + '</div>';
  const at = L.indexOf(st.level);
  const prev = !one || st.level === -1 ? L[L.length - 1] : at > 0 ? L[at - 1] : null;
  const next = !one || st.level === -1 ? L[0] : at < L.length - 1 ? L[at + 1] : null;
  out += `<button class="ch" ${prev === null ? 'disabled' : `onclick="set({per:false,level:${prev}})"`}>&#9664;</button>`;
  out += `<select onchange="set({per:false,level:+this.value})"><option value="-1">-</option>` +
    L.map(l => `<option value="${l}"${one && l === st.level ? ' selected' : ''}>Lv ${l}</option>`).join('') + '</select>';
  return out + `<button class="ch" ${next === null ? 'disabled' : `onclick="set({per:false,level:${next}})"`}>&#9654;</button></div>`;
}

function frame() {
  let whos = `<button class="who${isParty() ? ' on' : ''}" onclick="set({who:${N}})">Party</button>`;
  for (let i = 0; i < N; i++)
    whos += `<button class="who${st.who === i ? ' on' : ''}${H[i].left ? ' left' : ''}" onclick="set({who:${i}})">${dotOf(i)}${esc(H[i].name)}</button>`;
  const onChampions = st.topic === CHAMPIONS;
  document.getElementById('top').innerHTML = `<div class="whos">${whos}</div>${onChampions ? '' : picker()}`;
  document.getElementById('tabs').innerHTML = TOP.map(t => t.label).concat(['Champions']).map((label, i) =>
    `<button class="tab${st.topic === i ? ' on' : ''}" onclick="set({topic:${i}})">${esc(label)}</button>`).join('');
}

// A tile: the number and its label; on the Experience and Records pages the rank or the record's
// holder under it
// rows: the topic's rows; when none of its tiles applies (a champion's Exploration), its own top
// rows that do stand in (HeroStatsCatalogue_TilesFor in the game)
function tiles(numbers, rows) {
  const r = rec(st.who, st.level), p = page();
  let shown = numbers.filter(n => applies(n, st.who, st.level));
  if (!shown.length && numbers.length && rows) shown = rows.filter(([n, d]) => d === 0 && applies(n, st.who, st.level)).map(row => row[0]);
  shown = shown.slice(0, 4);
  return '<div class="tiles">' + shown.map(n => {
    let v = text(n, r[n]), l = esc(NUM[n].label), sub = '', cls = NUM[n].bad && r[n] > 0 ? ' bad' : '';
    // Beside Monsters killed, the champions' kills are a part of it
    if (st.topic === TOPIC_MONSTERS && isParty() && NUM[n].label === 'Kills') l = 'By champions';
    const k = n - D.xpFirst, line = k >= 0 && k < 4 ? p.xp.classes[k] : null;
    if (line) {
      // [name, rank now, at the start, % to next, next rank, experience, ranks gained, best hero, ...]
      if (isParty()) {
        l = esc(line[0]) + ' experience';
        if (line[1]) sub = `<div class="sub">${line[7] < N ? dotOf(line[7]) + ' ' : ''}best: ${esc(line[1])} (${esc(whoName(line[7]))})</div>`;
      } else {
        v = line[1] ? esc(line[1]) : '-';
        l = `${esc(line[0])} · ${fmt(line[5])} XP gained`;
        if (line[4] && line[4] !== line[1])
          sub = `<div class="prog"><i style="width:${line[3]}%;background:${COLOUR(st.who)}"></i></div><div class="sub f">${line[3]}% to ${esc(line[4])}</div>`;
      }
    }
    const record = NUM[n].rec ? p.records.find(x => x[0] === n - D.recFirst) : null;
    if (record && !record[9] && !isParty() && record[11])
      sub = `<div class="sub f">party: ${esc(record[11])} ${record[12] >= 0 ? esc(whoName(record[12])) : ''}</div>`;
    if (record && record[9]) {
      // [kind, group, label, value, who, how, whom, when, level, has, held, party value, party's hero]
      v = esc(record[3]);
      const detail = record[6] || record[5];
      if (isParty()) sub = `<div class="sub">${record[12] >= 0 && record[12] < N && record[4] !== 'party' ? dotOf(record[12]) + ' ' : ''}${esc(record[4])}${detail ? ' · ' + esc(detail) : ''}</div>`;
      else if (record[10]) { v += ' &#9733;'; cls += ' goldv'; }
      else if (record[11]) sub = `<div class="sub f">party: ${esc(record[11])} ${record[12] >= 0 ? esc(whoName(record[12])) : ''}</div>`;
    }
    const tipText = NUM[n].help;
    return `<div class="tile${cls}"><div class="v${cls.includes('goldv') ? ' gold' : ''}">${v}</div><div class="l">${tipText ? `<span class="tip"${tipOf(esc(tipText))}>${l}</span>` : l}</div>${sub}</div>`;
  }).join('') + '</div>';
}

const PARTY_HELP = 'All the champions together; for a record, the best of them.';
// The topic's groups with the rows that apply to who and the level: [{title, panel, rows: [{n, depth, label}]}].
// A part whose sum does not apply stands on its own under the number's own name (HeroStatsPages_Topic).
function groupsOf(t) {
  const out = [];
  for (const g of TOP[t].groups) {
    const applied = [false, false, false], rows = [];
    for (const [n, d, own] of g.rows) {
      const depth = Math.max(0, Math.min(2, d));
      applied[depth] = applies(n, st.who, st.level);
      if (!applied[depth]) continue;
      if (depth > 0 && !applied[depth - 1]) { applied[0] = true; rows.push({n, depth: 0, label: NUM[n].label}); }
      else rows.push({n, depth, label: own || NUM[n].label});
    }
    if (rows.length) out.push({title: g.title, panel: g.panel, rows});
  }
  return out;
}
const labelOf = row => {
  const help = NUM[row.n].help, label = help ? `<span class="tip"${tipOf(esc(help))}>${esc(row.label)}</span>` : esc(row.label);
  return row.depth ? `<span class="ind">${label}</span>` : label;
};
function numbersTable(t) {
  const party = rec(N, st.level), one = !isParty();
  const groups = groupsOf(t).filter(g => !g.panel);
  let head = '<tr><th></th>';
  if (one) head += `<th class="${H[st.who].left ? 'left' : ''}" data-name="${esc(H[st.who].name)}">${heroHead(st.who)}</th><th${tipOf(PARTY_HELP)}>Total</th><th>Share of party</th>`;
  else head += H.map((h, i) => `<th class="${h.left ? 'left' : ''}" data-name="${esc(h.name)}">${heroHead(i)}</th>`).join('') + `<th${tipOf(PARTY_HELP)}>Total</th><th>Share</th>`;
  const columns = (one ? 1 : N) + 3;
  let body = '';
  for (const g of groups) {
    // The groups have a small heading each, unless there is only the one (the section is named after it)
    if (groups.length > 1 && g.title) body += `<tr class="grp"><td colspan="${columns}">${esc(g.title)}</td></tr>`;
    for (const row of g.rows) {
      const n = row.n, info = NUM[n], summable = !info.ratio && !info.party && !info.rec;
      body += `<tr class="g${row.depth}"><td>${labelOf(row)}</td>`;
      if (one) {
        const mine = rec(st.who, st.level)[n];
        body += numCell(n, mine) + numCell(n, party[n]) + `<td class="bar">${summable && party[n] > 0 ? partBar(mine, party[n], st.who) : ''}</td>`;
      } else {
        for (let i = 0; i < N; i++) body += numCell(n, info.party ? null : rec(i, st.level)[n], H[i].left ? 'gone' : '');
        body += numCell(n, party[n]) + `<td class="bar">${summable ? shareBar(H.map((h, i) => rec(i, st.level)[n])) : ''}</td>`;
      }
      body += '</tr>';
    }
  }
  return `<div class="wide"><table>${head}</tr>${body}</table></div>`;
}
// The numbers only the party has (steps, teleports, chests ...): a panel per group
function panels(t) {
  const groups = groupsOf(t).filter(g => g.panel), party = rec(N, st.level);
  if (!groups.length) return '';
  return section('panels', 'The party', '', '<div class="facts">' + groups.map(g => `<div class="fact"><h4>${esc(g.title)}</h4>` +
    g.rows.map(row => `<div class="fr g${row.depth}"><span class="k">${labelOf(row)}</span>` +
      `<span class="v${party[row.n] === 0 ? ' z' : ''}">${party[row.n] === 0 ? '·' : text(row.n, party[row.n])}</span></div>`).join('') + '</div>').join('') + '</div>');
}

const attackCells = (l, cls) => {
  const [tries, hits, damage, kills, uses] = l;
  // Actions that cannot hit (War Cry ...): no hits, damage or kills, and no rate or average to work out
  if (tries === 0 && uses > 0) return [td(uses, plain(uses)), td(0, plain(0)), td(-1, '<td class="z">-</td>'),
    td(-1, '<td class="z">-</td>'), td(0, plain(0)), td(0, plain(0))];
  const rate = tries ? hits / tries : -1, avg = hits ? damage / hits : -1;
  return [td(tries + uses, plain(tries + uses)), td(hits, plain(hits)), td(rate, rate < 0 ? '<td class="z">-</td>' : `<td>${Math.round(rate * 100)}%</td>`),
    td(avg, avg < 0 ? '<td class="z">-</td>' : `<td>${fmt(Math.round(avg))}</td>`), td(damage, plain(damage)), td(kills, plain(kills))];
};
const whoTd = by => td(sum(by), whoBar(by));
// A Who cell that splits whichever column the table is sorted by: valueOf(column) is each champion's
const whoBy = (v, valueOf) => ({v, by: valueOf});

function weapons(p) {
  const all = [0, 0, 0, 0, 0];
  for (const w of p.weapons) for (let k = 0; k < 5; k++) all[k] += w[k + 1];
  const summary = p.weapons.length ? `${fmt(all[2])} damage · ${fmt(all[3])} kills` : 'none';
  if (!p.weapons.length) return section(DETAIL.WEAPONS, 'Weapons', summary, '<div class="note">No attacks.</div>');
  const cols = [nameCol('Weapon'), numCol('Tries', 'Blows and missiles, and the actions that cannot hit (War Cry, Calm...). The Combat page\'s Attacks leaves those out; its Other actions counts them.'), numCol('Hits'), numCol('Rate', 'Hits out of the tries that can hit; actions that cannot hit are left out.'), numCol('Avg'), numCol('Damage'), numCol('Kills')];
  if (isParty()) cols.push(whoColumn([1, 2, 5, 6], 5));
  // A champion's [tries, hits, damage, kills, uses] in a column of the table
  const attack = (c, k) => k === 1 ? c[0] + c[4] : k === 2 ? c[1] : k === 6 ? c[3] : c[2];
  const rows = p.weapons.map(w => {
    const key = st.who + ':' + w[0], open = !!st.open[key];
    const cells = [td(w[0], `<td>${open ? '&#9662;' : '&#9656;'} ${esc(w[0])}</td>`)].concat(attackCells(w.slice(1, 6)));
    if (isParty()) cells.push(whoBy(w[3], k => w[7].map(c => attack(c, k))));
    const sub = open ? w[6].map(o => ({cls: 'opt', cells: [nameTd(o[0])].concat(attackCells(o.slice(1, 6)), isParty() ? [td(0, '<td></td>')] : [])})) : [];
    return {cls: 'wrow', act: `toggleWeapon(${JSON.stringify(key).replace(/"/g, '&quot;')})`, cells, sub};
  });
  const foot = {cls: 'all', cells: [nameTd('All weapons')].concat(attackCells(all), isParty() ? [td(0, '<td></td>')] : [])};
  return section(DETAIL.WEAPONS, 'Weapons', summary, sortTable('weapons', cols, rows, foot, 5));
}
function toggleWeapon(key) { st.open[key] = !st.open[key]; draw(); }

function spells(p) {
  const all = [0, 0, 0, 0, 0, 0];
  for (const s of p.spells) for (let k = 0; k < 6; k++) all[k] += s[k + 1];
  const summary = p.spells.length ? `${fmt(all[0])} casts · ${fmt(all[2])} mana` : 'none';
  if (!p.spells.length) return section(DETAIL.SPELLS, 'Spells', summary, '<div class="note">No spells cast.</div>');
  const spellHelp = {Casts: 'Casts of the spell, the failed ones included.', Mana: 'The mana of the runes, the mana wasted on failed casts included.'};
  const cols = ['Casts', 'Failed', 'Mana', 'Wasted', 'Damage', 'Kills'].map(l => numCol(l, spellHelp[l]));
  cols.unshift(nameCol('Spell'));
  if (isParty()) cols.push(whoColumn([1, 2, 3, 4, 5, 6], 3));
  const cellsOf = v => v.map((x, k) => td(x, plain(x, k === 1 || k === 3)));
  // Each champion's [casts, failed, mana, wasted, damage, kills]: table column k is k - 1
  const rows = p.spells.map(s => ({cells: [nameTd(s[0])].concat(cellsOf(s.slice(1, 7)),
    isParty() ? [whoBy(s[3], k => s[7].map(c => c[k - 1]))] : [])}));
  const foot = {cls: 'all', cells: [nameTd('All spells')].concat(cellsOf(all), isParty() ? [td(0, '<td></td>')] : [])};
  return section(DETAIL.SPELLS, 'Spells', summary, sortTable('spells', cols, rows, foot, 3));
}

function amounts(id, title, column, what, lines, withBad) {
  const total = lines.reduce((a, l) => a + l[1], 0);
  if (!lines.length) return section(id, title, 'none', '<div class="note">None.</div>');
  const cols = [nameCol('Source'), numCol(column)];
  if (isParty()) cols.push(numCol('Who: ' + column));
  const rows = lines.map(l => ({cells: [nameTd(l[0]), td(l[1], plain(l[1], withBad && l[2]))].concat(isParty() ? [whoTd(l[withBad ? 3 : 2])] : [])}));
  const foot = {cls: 'all', cells: [nameTd('All sources'), td(total, plain(total))].concat(isParty() ? [td(0, '<td></td>')] : [])};
  return section(id, title, `${fmt(total)} ${what}`, sortTable('amounts' + id, cols, rows, foot, 1));
}

function deaths(p) {
  const n = p.deaths.length;
  return section(DETAIL.DEATHS, 'Deaths', n ? `${n} ${n === 1 ? 'death' : 'deaths'}` : 'never died',
    n ? p.deaths.map(d => `<div>${esc(d)}</div>`).join('') : '<div class="note">Nobody died.</div>');
}

function consumed(p) {
  const lines = [];
  for (const [list, k] of [[p.eaten, 1], [p.drunk, 2]]) for (const l of list) {
    let line = lines.find(x => x[0] === l[0]);
    // [name, eaten, drunk, each column's eaten, each column's drunk]
    if (!line) lines.push(line = [l[0], 0, 0, H.map(() => 0), H.map(() => 0)]);
    line[k] += l[1];
    l[2].forEach((v, i) => line[k + 2][i] += v);
  }
  const summary = `${fmt(p.eatenTotal)} eaten · ${fmt(p.drunkTotal)} drunk`;
  if (!lines.length) return section(DETAIL.CONSUMED, 'Eaten and drunk', 'nothing', '<div class="note">Nothing eaten or drunk.</div>');
  const cols = [nameCol('Item'), numCol('Eaten'), numCol('Drunk', 'Times the item was drunk; the fountain counts as a drink.')];
  if (isParty()) cols.push(whoColumn([1, 2], 1));
  const rows = lines.map(l => ({cells: [nameTd(l[0]), td(l[1], plain(l[1])), td(l[2], plain(l[2]))]
    .concat(isParty() ? [whoBy(l[1] + l[2], k => k === 2 ? l[4] : l[3])] : [])}));
  const foot = {cls: 'all', cells: [nameTd('Everything'), td(0, plain(p.eatenTotal)), td(0, plain(p.drunkTotal))].concat(isParty() ? [td(0, '<td></td>')] : [])};
  return section(DETAIL.CONSUMED, 'Eaten and drunk', summary, sortTable('consumed', cols, rows, foot, 1));
}

function items() {
  const lines = view(st.level).items;
  const all = lines.reduce((a, l) => [a[0] + l[1], a[1] + l[3], a[2] + l[2]], [0, 0, 0]);
  if (!lines.length) return section(DETAIL.ITEMS, 'Items', 'none', '<div class="note">No items picked up or used in walls.</div>');
  const rows = lines.map(l => ({cells: [nameTd(l[0]), td(l[1], plain(l[1])), td(l[3], plain(l[3])), td(l[2], plain(l[2]))]}));
  const foot = {cls: 'all', cells: [nameTd('All items'), td(0, plain(all[0])), td(0, plain(all[1])), td(0, plain(all[2]))]};
  return section(DETAIL.ITEMS, 'Items', `${fmt(all[0])} picked up · ${fmt(all[1])} from chests · ${fmt(all[2])} used in walls`,
    sortTable('items', [nameCol('Item'), numCol('Picked up'), numCol('From chest', 'Included in Picked up: the ones taken out of a chest.'), numCol('Used in walls')], rows, foot, 1));
}

function monsters() {
  let lines = view(st.level).monsters;
  if (!isParty()) lines = lines.filter(m => m[1][st.who] > 0).map(m => [m[0], [m[1][st.who]], 0, 0, 0, 0, 0, m[1][st.who]]);
  if (!lines.length) return section(DETAIL.MONSTERS, 'Monsters killed', 'none', '<div class="note">No monsters killed.</div>');
  const all = lines.reduce((a, m) => { m[1].forEach((v, i) => a[1][i] += v); for (let k = 2; k < 8; k++) a[k] += m[k]; return a; },
    ['All monsters', lines[0][1].map(() => 0), 0, 0, 0, 0, 0, 0]);
  const summary = `${fmt(all[7])} kills · ${lines.length} kinds`;
  let cols, cellsOf;
  if (isParty()) {
    const extra = [[2, 'Door'], [3, 'Pit'], [4, 'Telefrag'], [5, 'Landed'], [6, 'Other']].filter(([k]) => all[k] > 0);
    const extraHelp = {Telefrag: 'The party was teleported onto their square.', Landed: 'The party came down a pit onto their square.', Other: 'Clouds nobody is known to have made, traps, other monsters.'};
    cols = [nameCol('Monster')].concat(H.map((h, i) => heroCol(i)), extra.map(([, l]) => numCol(l, extraHelp[l] || '')), [numCol('All', 'Every kill, whatever the cause.'), numCol('Who: Kills')]);
    cellsOf = (m, foot) => [nameTd(m[0])].concat(m[1].map((v, i) => td(v, plain(v, false, H[i].left ? 'gone' : ''))),
      extra.map(([k]) => td(m[k], plain(m[k]))), [td(m[7], plain(m[7])), foot ? td(0, '<td></td>') : whoTd(m[1])]);
  } else {
    cols = [nameCol('Monster'), numCol('Kills')];
    cellsOf = m => [nameTd(m[0]), td(m[7], plain(m[7]))];
  }
  return section(DETAIL.MONSTERS, 'Monsters killed', summary,
    sortTable('monsters', cols, lines.map(m => ({cells: cellsOf(m)})), {cls: 'all', cells: cellsOf(all, true)}, cols.length - (isParty() ? 2 : 1)));
}

function cards() {
  // Exp: "Experience" would take the room of the bars
  const ROWS = [[0, 'Damage'], [4, 'Kills'], [-1, 'Taken'], [-2, 'Exp'], [-3, 'Mana'], [-4, 'Deaths']];
  const index = label => NUM.findIndex(n => n.label === label);
  const ids = ROWS.map(([, l], k) => [index(['Damage dealt', 'Kills', 'Damage taken', 'Experience gained', 'Mana spent', 'Deaths'][k]), l]);
  const party = rec(N, st.level)[ids[0][0]];
  const largest = ids.map(([n]) => Math.max(1, ...H.map((h, i) => rec(i, st.level)[n])));
  const widest = Math.max(1, ...ids.map(([n]) => Math.max(...H.map((h, i) => fmt(rec(i, st.level)[n]).length))));
  return `<div class="cards" style="--nw:${widest}ch">` + H.map((h, i) => {
    const r = rec(i, st.level);
    return `<div class="card${h.left ? ' left' : ''}" style="border-top-color:${COLOUR(i)}" onclick="set({who:${i}})">` +
      `<div class="nm">${esc(h.name)}<span>${party ? Math.round(100 * r[ids[0][0]] / party) : 0}% of damage</span></div>` +
      ids.map(([n, l], k) => {
        const v = r[n], bad = NUM[n].bad && v > 0;
        return `<div class="cr"><span class="l">${l}</span><div class="mini"><i style="width:${100 * v / largest[k]}%;background:${bad ? 'var(--bad)' : COLOUR(i)}"></i></div>` +
          `<span class="n${v === 0 ? ' z' : ''}">${v === 0 ? '·' : bad ? `<span class="pill">${fmt(v)}</span>` : fmt(v)}</span></div>`;
      }).join('') + '</div>';
  }).join('') + '</div>';
}

// ---- Experience and Records ----

function skills(p) {
  const cols = isParty() ? [nameCol('Skill'), numCol('Experience'), numCol('Ranks gained'), whoColumn([1], 1)]
                         : [nameCol('Skill'), nameCol('Rank now'), numCol('Experience'), numCol('Ranks gained')];
  const rows = p.xp.classes.map((c, i) => {
    const key = 'class:' + st.who + ':' + i, open = !!st.open[key];
    const name = td(c[0], `<td>${open ? '&#9662;' : '&#9656;'} ${esc(c[0].toUpperCase())}</td>`);
    const cells = isParty() ? [name, td(c[5], plain(c[5])), td(c[6], plain(c[6])), whoBy(c[5], () => c[8])]
      : [name, td(c[1], `<td class="l">${esc(c[1] || '-')}${c[2] && c[2] !== c[1] ? ` <span class="nt">(${esc(c[2])} at the start)</span>` : ''}</td>`),
         td(c[5], plain(c[5])), td(c[6], plain(c[6]))];
    const sub = open ? c[9].map(s => ({cls: 'opt', cells: isParty() ? [nameTd(s[0]), td(s[1], plain(s[1])), td(0, '<td></td>'), td(0, whoBar(s[2]))]
                                                             : [nameTd(s[0]), td(0, '<td></td>'), td(s[1], plain(s[1])), td(0, '<td></td>')]})) : [];
    return {cls: 'wrow', act: `toggleWeapon(${JSON.stringify(key).replace(/"/g, '&quot;')})`, cells, sub};
  });
  return section(DETAIL.SKILLS, 'Skills', 'the hidden skills under each class', sortTable('skills', cols, rows, null, null));
}

// Experience by source: Attacks opens to its weapons and their actions, Spells to its classes and
// their spells. Open at first: Attacks with its first weapon, Spells with its classes.
function xpSources(p) {
  const lines = p.xp.sources;
  let head = '<tr><th>Source</th>';
  head += isParty() ? '<th>Experience</th><th>Who: Experience</th>'
    : `<th>${dotOf(st.who)} ${esc(H[st.who].name)}</th><th>Total</th><th>Share of party</th>`;
  let body = '', hideBelow = -1, top = '';
  lines.forEach((l, i) => {
    // [name, note, depth, experience, [by hero; a hero's page: the party's], has children]
    const [name, note, depth, amount, by, kids, help] = l;
    if (depth === 0) top = name;
    if (hideBelow >= 0 && depth > hideBelow) return;
    hideBelow = -1;
    const key = 'xp:' + st.who + ':' + top + ':' + i;
    const atFirst = depth === 0 || (depth === 1 && (top === 'Spells' || lines[i - 1][2] === 0));
    const open = kids && (st.open[key] === undefined ? atFirst : st.open[key]);
    if (kids && !open) hideBelow = depth;
    const arrow = kids ? (open ? '&#9662; ' : '&#9656; ') : '';
    const act = kids ? ` onclick="st.open[${JSON.stringify(key).replace(/"/g, '&quot;')}]=${!open};draw()"` : '';
    body += `<tr class="d${Math.min(depth, 2)}${kids ? ' xrow' : ''}"${act}><td${help ? tipOf(esc(help)) : ''}>${arrow}${esc(name)}${note ? ` <span class="nt">${esc(note)}</span>` : ''}</td>`;
    if (isParty()) body += plain(amount) + `<td class="whobar">${shareBar(by)}</td>`;
    else body += plain(amount) + plain(by[0] || 0) + `<td class="bar">${by[0] > 0 ? partBar(amount, by[0], st.who) : ''}</td>`;
    body += '</tr>';
  });
  return section(DETAIL.XP_SOURCES, 'Experience by source', 'click a line to fold it', `<div class="wide"><table>${head}</tr>${body}</table></div>`);
}

function rankUps(p) {
  const ups = p.xp.rankups, n = ups.length;
  if (!n) return section(DETAIL.RANK_UPS, 'Ranks gained', 'none yet', '<div class="note">No new rank yet.</div>');
  const cols = (isParty() ? [nameCol('Who')] : []).concat([nameCol('Class'), nameCol('Rank'), nameCol('With', 'What gave the experience that made the rank: an action, a spell or a kill.'), numCol('Lv')]);
  const rows = ups.map((u, i) => ({cells: (isParty() ? [td(whoName(u[0]), `<td>${u[0] < N ? dotOf(u[0]) + ' ' : ''}${esc(whoName(u[0]))}</td>`)] : [])
    .concat([nameTd(u[1]), nameTd(u[2]), td(u[3], `<td class="nt">${esc(u[3])}</td>`), td(u[4], `<td>${u[4]}</td>`)])}));
  return section(DETAIL.RANK_UPS, 'Ranks gained', `newest first · ${n} ${n === 1 ? 'rank' : 'ranks'}`, sortTable('rankups', cols, rows, null, null));
}

function records(p) {
  const lines = p.records, held = lines.filter(l => l[10]).length;
  const summary = isParty() ? 'the best of the run · hover a value for when' : `${esc(H[st.who].name)}'s best · &#9733; ${held} party ${held === 1 ? 'record' : 'records'}`;
  let head = '<tr><th>Record</th>' + (isParty() ? '<th>Value</th><th class="l">Who</th>' : `<th>${dotOf(st.who)} ${esc(H[st.who].name)}</th>`) +
    '<th class="l">How</th><th class="l">On / from</th><th>Lv</th>' + (isParty() ? '' : '<th>Party best</th>');
  let body = '', group = '';
  const span = isParty() ? 6 : 6;
  for (const l of lines) {
    // [kind, group, label, value, who, how, whom, when, level, has, held, party value, party's hero]
    if (l[1] !== group) { group = l[1]; body += `<tr class="grp"><td colspan="${span}">${esc(group)}</td></tr>`; }
    const value = l[9] ? `<td${l[10] ? ' class="gold"' : ''}${l[7] ? tipOf(esc(l[7])) : ''}>${esc(l[3])}${l[10] ? ' &#9733;' : ''}</td>` : '<td class="z">-</td>';
    body += `<tr><td>${esc(l[2])}</td>${value}`;
    if (isParty()) body += `<td class="l">${l[9] && l[4] !== 'party' && l[12] >= 0 && l[12] < N ? dotOf(l[12]) + ' ' : ''}${esc(l[9] ? l[4] : '')}</td>`;
    body += `<td class="l nt">${esc(l[5])}</td><td class="l nt">${esc(l[6])}</td><td>${l[9] ? l[8] : ''}</td>`;
    if (!isParty()) body += `<td>${l[10] ? '<span class="nt">this one</span>' : l[11] ? esc(l[11]) + (l[12] >= 0 && l[12] < N ? ' ' + dotOf(l[12]) : '') : ''}</td>`;
    body += '</tr>';
  }
  return section(DETAIL.RECORDS, 'Records', summary, `<div class="wide"><table>${head}</tr>${body}</table></div>`);
}

function topicTiles() {
  const sums = view(st.level).summaries[st.who];
  return '<div class="topics">' + TOP.map((t, i) => i === 0 ? '' :
    `<div class="topic" onclick="set({topic:${i}})"><div class="t">${esc(t.label)}</div><div class="s">${esc(sums[i])}</div></div>`).join('') + '</div>';
}

function topicPage() {
  const t = st.topic, p = page();
  if (t === 0) {
    const body = tiles(isParty() ? TOP[0].tiles : D.heroTiles, TOP[0].rows) + (isParty() ? cards() : topicTiles());
    return body + section(KEY_SECTION, 'Key numbers', isParty() ? 'one row from each topic' : 'compared with the party', numbersTable(0));
  }
  let html = tiles(TOP[t].tiles, TOP[t].rows) + panels(t) + '<div class="secs">';
  // The Records table is the page's numbers. A page whose table rows are one named group
  // (Exploration's Doors) names the section after it.
  const tableGroups = groupsOf(t).filter(g => !g.panel);
  if (t !== TOPIC_RECORDS && tableGroups.length)
    html += section(NUMBERS_SECTION, tableGroups.length === 1 && tableGroups[0].title ? tableGroups[0].title : 'Numbers', '', numbersTable(t));
  for (const d of isParty() ? TOP[t].details : TOP[t].heroDetails) {
    if (d === DETAIL.WEAPONS) html += weapons(p);
    else if (d === DETAIL.SPELLS) html += spells(p);
    else if (d === DETAIL.HARM) html += amounts(DETAIL.HARM, 'Damage taken by source', 'Damage', 'taken', p.taken, true);
    else if (d === DETAIL.HEALING) html += amounts(DETAIL.HEALING, 'Healing by source', 'Healed', 'healed', p.healing, false);
    else if (d === DETAIL.DEATHS) html += deaths(p);
    else if (d === DETAIL.CONSUMED) html += consumed(p);
    else if (d === DETAIL.ITEMS) html += items();
    else if (d === DETAIL.MONSTERS) html += monsters();
    else if (d === DETAIL.SKILLS) html += skills(p);
    else if (d === DETAIL.XP_SOURCES) html += xpSources(p);
    else if (d === DETAIL.RANK_UPS) html += rankUps(p);
    else if (d === DETAIL.RECORDS) html += records(p);
  }
  return html + '</div>';
}

// Per level: the Party's totals or its champions side by side, a champion's totals
function perLevel() {
  const t = st.topic, grid = TOP[t].grid, levels = D.levels.concat([-1]);
  let html = '';
  // Records are one at a time: no Party totals
  const recordsOnly = t === TOPIC_RECORDS;
  const side = isParty() && grid.length > 0 && (st.side || recordsOnly);
  if (isParty() && grid.length && !recordsOnly) {
    html += `<div class="choices"><span class="lab">Show</span><div class="seg"><button class="ch${!st.side ? ' on' : ''}" onclick="set({side:false})">Party totals</button>` +
      `<button class="ch${st.side ? ' on' : ''}" onclick="set({side:true})">Champions side by side</button></div></div>`;
  }
  let cols, rowOf;
  const levelTd = l => td(l, `<td>${lvName(l)}</td>`);
  if (side) {
    const n = grid.includes(st.grid[t]) ? st.grid[t] : grid[0];
    html += '<div class="choices"><span class="lab">Number</span><div class="seg">' + grid.map(g =>
      `<button class="ch${g === n ? ' on' : ''}" onclick="st.grid[${t}]=${g};draw()">${esc(NUM[g].label)}</button>`).join('') + '</div></div>';
    const share = !NUM[n].rec;
    cols = [nameCol('Level')].concat(H.map((h, i) => heroCol(i)), [numCol('Total')], share ? [numCol('Share')] : []);
    rowOf = l => {
      const by = H.map((h, i) => NUM[n].party ? null : rec(i, l)[n]);
      return [levelTd(l)].concat(by.map((v, i) => td(v, numCell(n, v, H[i].left ? 'gone' : ''))),
        [td(rec(N, l)[n], numCell(n, rec(N, l)[n]))], share ? [td(rec(N, l)[n], `<td class="bar">${shareBar(by)}</td>`)] : []);
    };
  } else {
    // Monsters: the causes are parts of Monsters killed, and each is worth its column
    const numbers = TOP[t].rows.filter(([n, depth]) => depth <= (t === TOPIC_MONSTERS ? 1 : 0) && applies(n, st.who, 0)).map(([n]) => n);
    const share = isParty() ? numbers.find(n => !NUM[n].party && !NUM[n].ratio) : undefined;
    // Beside Monsters killed, the champions' kills are a part of it, as on the page
    const head = n => t === TOPIC_MONSTERS && isParty() && NUM[n].label === 'Kills' ? 'By champions' : NUM[n].label;
    cols = [nameCol('Level')].concat(numbers.map(n => numCol(head(n))));
    if (share !== undefined) cols.push(numCol('Who: ' + NUM[share].label));
    rowOf = l => [levelTd(l)].concat(numbers.map(n => td(rec(st.who, l)[n], numCell(n, rec(st.who, l)[n]))),
      share === undefined ? [] : [td(rec(N, l)[share], `<td class="whobar">${shareBar(H.map((h, i) => rec(i, l)[share]))}</td>`)]);
  }
  const rows = D.levels.map(l => ({cls: 'lv', act: `set({per:false,level:${l}})`, cells: rowOf(l)}));
  const foot = {cls: 'all lv', act: 'set({per:false,level:-1})', cells: rowOf(-1)};
  return html + sortTable('per' + t + (side ? 'g' : 't'), cols, rows, foot, 0);
}

// The Champions tab: what the game's inventory screen showed of each champion when the page was written
function vitalBar(label, now, most, colour, txt) {
  const w = most > 0 ? Math.max(0, Math.min(100, now * 100 / most)) : 0;
  return `<span class="l">${label}</span><div class="vbar"><i style="width:${w}%;background:${colour}"></i></div><span class="v">${txt}</span>`;
}
function nourished(label, v, colour, empty) {
  const low = -1024, full = 2048, tone = v < -512 ? 'var(--bad)' : v < 0 ? '#e8d44a' : 'var(--text)';
  const w = Math.max(0, Math.min(100, (v - low) * 100 / (full - low)));
  return `<span class="l">${label}</span><div class="vbar"><i style="width:${w}%;background:${colour}"></i></div>` +
    `<span class="v" style="color:${tone}">${v}${v < 0 ? ` (${empty})` : ''}</span>`;
}
const tenths = t => (t < 0 ? '-' : '') + Math.floor(Math.abs(t) / 10) + '.' + (Math.abs(t) % 10);
// The Champions tab: a card per champion for the Party; for one champion the inventory laid out
// as the game's own screen, a slot's tooltip with name, weight and charges, a chest's with what
// it holds. An item is [slot, name, in a chest, icon, weight, charges, gulps, food, water].
const SLOT_NAMES = ['Ready hand', 'Action hand', 'Head', 'Torso', 'Legs', 'Feet', 'Pouch 2', 'Quiver 2', 'Quiver 3', 'Quiver 4', 'Neck', 'Pouch 1', 'Quiver 1'];
const slotName = s => s < 0 ? 'On the cursor' : SLOT_NAMES[s] || 'Backpack ' + (s - 12);
// The place of a body slot on the picture's grid of five by five: [column, row]
const DOLL = {10: [2, 1], 2: [3, 1], 0: [1, 2], 3: [3, 2], 1: [5, 2], 4: [3, 3], 11: [1, 4], 5: [3, 4], 12: [4, 4], 7: [5, 4], 6: [1, 5], 8: [4, 5], 9: [5, 5]};
const PART = {0: 'hand', 1: 'hand', 2: 'head', 3: 'torso', 4: 'legs', 5: 'feet', 10: 'neck'};
const iconOf = it => it[3] === null || !LIVE.icons[it[3]] ? '<span class="noicon">?</span>' : `<img src="${LIVE.icons[it[3]]}" alt="">`;
const chargesOf = it => it[5] < 0 ? '' : it[6] ? (it[5] === 0 ? 'empty' : it[5] + (it[5] === 1 ? ' gulp left' : ' gulps left')) :
  it[5] === 0 ? 'no charges' : it[5] + (it[5] === 1 ? ' charge' : ' charges');
// What eating or drinking an item adds, as the window's tooltip says it
const gives = it => (it[7] > 0 ? `<span class="food">Food +${it[7]}</span>` : '') +
  (it[8] > 0 ? `<span class="water">${it[7] > 0 ? ' ' : ''}Water +${it[8]}${it[6] ? ' per gulp' : ''}</span>` : '');
const heldIn = (items, i) => { const held = []; if (i < 0 || items[i][2]) return held; for (let k = i + 1; k < items.length && items[k][2]; k++) held.push(items[k]); return held; };
// What an item is worth as a weapon and as armor: the same words and rules as the window's
// (HeroStatsLive_WeaponText, _ArmorText, _ListDamage, _ListKinetic); keep the two in step
const hasWeaponLine = it => it[9] >= 0 && (it[9] > 0 || it[10] > 0 || it[11] > 0);
const weaponText = it => !hasWeaponLine(it) ? '' : it[11] > 0 ? `Kinetic energy ${it[10]} · shoot damage ${it[11]}` : `Damage ${it[9]} · kinetic energy ${it[10]}`;
const armorText = it => it[12] >= 0 ? `Armor ${it[12]} · sharp resistance ${Math.max(0, it[13])}` : '';
const listDamage = it => hasWeaponLine(it) && !(it[11] > 0) ? it[9] : -1;
const listKinetic = it => hasWeaponLine(it) ? it[10] : -1;
const worth = it => (weaponText(it) ? `<div class="dmg">${weaponText(it)}</div>` : '') + (armorText(it) ? `<div class="arm">${armorText(it)}</div>` : '');
let TIPS = [];
function tipOf(html) { TIPS.push(html); return ` data-tip="${TIPS.length - 1}"`; }
function slotTip(items, i, slot, wounded) {
  const hurt = wounded ? ' <span class="bad">· wounded</span>' : '';
  if (i < 0) return `<div class="s">${slotName(slot)} · empty${hurt}</div>`;
  const it = items[i], held = heldIn(items, i), charges = chargesOf(it);
  let html = `<div class="h">${iconOf(it)}<div><div class="n">${esc(it[1])}</div><div class="s">${slotName(slot)}${hurt}</div>` +
    `<div class="x">${tenths(it[4])} kg${held.length ? ' with contents' : ''}${charges ? ' · ' + charges : ''}</div>${worth(it)}${gives(it) ? `<div class="x">${gives(it)}</div>` : ''}</div></div>`;
  if (held.length) html += `<div class="in"><div class="cap">Holds ${held.length} ${held.length === 1 ? 'item' : 'items'}</div>` +
    held.map(h => `<div>${iconOf(h)}${esc(h[1])}${gives(h) ? ' &nbsp;' + gives(h) : ''}</div>`).join('') + '</div>';
  return html;
}
function slotHtml(items, slot, wounded, part) {
  const i = items.findIndex(it => it[0] === slot && !it[2]), held = heldIn(items, i);
  return `<div class="slot ${i < 0 ? 'empty' : 'has'}${wounded ? ' wound' : ''}" tabindex="0"${tipOf(slotTip(items, i, slot, wounded))}${held.length ? ' data-pin="1"' : ''}>` +
    (i < 0 ? (part ? `<span class="gh">${part}</span>` : '') : iconOf(items[i]) + (held.length ? `<span class="cnt">${held.length}</span>` : '')) + '</div>';
}
const isWounded = (c, slot) => slot >= 0 && slot < 6 && ((c.woundBits >> slot) & 1) === 1;
function liveVitals(c, colour) {
  const heavy = c.load[0] > c.load[1];
  return '<div class="vit">' + vitalBar('Health', c.hp[0], c.hp[1], colour, `${c.hp[0]} / ${c.hp[1]}`) +
    vitalBar('Stamina', c.stamina[0], c.stamina[1], colour, `${Math.floor(c.stamina[0] / 10)} / ${Math.floor(c.stamina[1] / 10)}`) +
    vitalBar('Mana', c.mana[0], c.mana[1], colour, `${c.mana[0]} / ${c.mana[1]}`) +
    nourished('Food', c.food, '#9b6a3c', 'starving') + nourished('Water', c.water, '#4a8fe0', 'parched') +
    vitalBar('Load', c.load[0], c.load[1], heavy ? 'var(--bad)' : 'var(--faint)', `<span class="${heavy ? 'bad' : ''}">${tenths(c.load[0])} / ${tenths(c.load[1])} kg</span>`) +
    `<span class="l">Wounds</span><span class="w ${c.wounds ? 'bad' : 'none'}">${c.wounds ? esc(c.wounds) : 'none'}</span>` +
    `<span class="l">Poison</span><span class="w ${c.poison ? 'bad' : 'none'}">${c.poison ?
      `${c.poisonSum} (${c.poison} ${c.poison === 1 ? 'dose' : 'doses'}, ${c.poisonDoses.reduce((t, v) => t + Math.max(1, Math.floor(v / 64)), 0)} HP per 36 ticks)` : 'none'}</span>` +
    c.poisonDoses.map(v => `<span></span><span class="w none">${v} (${Math.max(1, Math.floor(v / 64))} HP per 36 ticks, ${v * 36} ticks left)</span>`).join('') + '</div>';
}
function liveHead(c, colour) {
  const status = c.hp[0] <= 0 ? 'dead' : c.poison ? 'poisoned' : '';
  return `<div class="chead">${c.portrait ? `<img src="${c.portrait}" alt="">` : ''}<div class="grow"><div class="nm" style="color:${colour}">${esc(c.name)}</div>` +
    `<div class="ti">${esc(c.title)}</div></div>${status ? `<span class="pill">${status}</span>` : ''}</div>`;
}
const liveRanks = (c, colour) => '<div class="lranks">' + c.ranks.map(r => `<span class="l">${esc(r[0])}</span><span class="${r[1] ? '' : 'none'}">${r[1] ? esc(r[1]) : '-'}</span>` +
  `<div class="vbar"${tipOf(r[2] + '% of the way to the next rank')}><i style="width:${r[2]}%;background:${colour}"></i></div>`).join('') + '</div>';
// An attribute's bar against the best of the party (HeroStatsLive_AttributeBar, _AttributeTop and
// _AttributeHelp in the game): solid up to now, a drained part hatched up to the max, a raised part in gold past it
function attributeBar(c, i, colour) {
  const all = LIVE.champions, now0 = c.attrs[i][1], most0 = c.attrs[i][2];
  const top = Math.max(1, ...all.map(o => Math.max(o.attrs[i][1], o.attrs[i][2])));
  const best = Math.max(most0, ...all.map(o => o.attrs[i][2])), holder = (all.find(o => o.attrs[i][2] === best) || c).name;
  const now = Math.min(top, Math.max(0, now0)), most = Math.min(top, Math.max(0, most0)), p = v => v * 100 / top;
  const percent = best > 0 ? Math.floor((most0 * 200 + best) / (2 * best)) : 0;
  const help = `${esc(c.name)}: ${now0 === most0 ? most0 : `${now0} now, ${most0} max`} · ${percent}% of the best (${esc(holder)} ${best})`;
  return `<div class="abar"${tipOf(help)}><i style="left:0;width:${p(Math.min(now, most))}%;background:${colour}"></i>` +
    (now < most ? `<i class="lost" style="left:${p(now)}%;width:${p(most - now)}%"></i>` : '') +
    (now > most ? `<i class="over" style="left:${p(most)}%;width:${p(now - most)}%"></i>` : '') + '</div>';
}
function liveCard(c) {
  const colour = c.hero >= 0 ? COLOUR(c.hero) : 'var(--text)';
  const carried = c.items.filter(it => !it[2] && !(it[0] >= 0 && it[0] <= 5) && it[0] !== 10).length;
  return `<div class="lcard"${c.hero >= 0 ? ` onclick="set({who:${c.hero}})"` : ''}>` + liveHead(c, colour) + liveVitals(c, colour) +
    '<div class="strip">' + [0, 1, 2, 10, 3, 4, 5].map(s => slotHtml(c.items, s, isWounded(c, s))).join('') +
    (carried ? `<span class="more">+${carried} carried</span>` : '') + '</div>' +
    // With all the cards in one row (fit sets "full" on them) the attributes and the whole inventory stand in the strip's place
    `<div class="cfull"><div class="vit psec">${c.attrs.map((a, i) => `<span class="l">${esc(a[0])}</span>${attributeBar(c, i, colour)}` +
      `<span class="v"><span class="${a[1] < a[2] ? 'bad' : a[1] > a[2] ? 'raised' : ''}">${a[1]}</span> / ${a[2]}</span>`).join('')}</div>` +
    `<div class="psec">${livePicture(c)}</div><div class="psec" style="padding-top:0"></div></div>` + liveRanks(c, colour) + '</div>';
}
// The places of the items in the order the list shows them (HeroStatsLive_Order in the game)
function listOrder(items, col, desc) {
  const key = col === 3 ? (it => it[4]) : col === 4 ? (it => it[7]) : col === 5 ? (it => it[8]) : col === 2 ? (it => it[1])
    : col === 6 ? listDamage : col === 7 ? listKinetic : col === 8 ? (it => it[12]) : null;
  const sorted = places => {
    if (!key) return desc ? places.reverse() : places;
    return places.sort((a, b) => { const x = key(items[a]), y = key(items[b]), c = x < y ? -1 : x > y ? 1 : 0; return (desc ? -c : c) || a - b; });
  };
  const out = [];
  for (const o of sorted(items.map((it, i) => i).filter(i => !items[i][2]))) {
    const held = [];
    for (let k = o + 1; k < items.length && items[k][2]; k++) held.push(k);
    out.push(o, ...(key ? sorted(held) : held));
  }
  return out;
}
function liveInventory(c) {
  if (!c.items.length) return '<div class="note">Carries nothing.</div>';
  // The list sorts by slot, name or a column's number; a chest keeps what it holds under it
  const s = st.sort.ilist || {col: 0, desc: false};
  const th = (col, label, num, cls, help) => `<th class="sort${cls}" onclick="sortBy('ilist',${col},${num})"${help ? tipOf(help) : ''}>${label}` +
    (s.col === col ? ` <span class="arrow">${s.desc ? '&#9660;' : '&#9650;'}</span>` : '') + '</th>';
  if (st.list) return '<div class="wide"><table class="ilist"><tr>' + th(0, 'Slot', 0, '') + '<th></th>' + th(2, 'Item', 0, ' l') + th(6, 'Damage', 1, ' c-der', 'The weapon\'s own strength in a blow. Blank for what shoots: see its hovertext.') +
    th(7, 'Kinetic', 1, ' c-kin', 'Kinetic energy: how far and how hard it flies when thrown, or what a bow or sling adds to a shot.') +
    th(8, 'Armor', 1, ' c-der', 'The armor\'s strength. Its sharp resistance is in its hovertext.') +
    th(4, 'Food', 1, '') + th(5, 'Water', 1, '') + th(3, 'Weight', 1, '') + '</tr>' +
    listOrder(c.items, s.col, s.desc).map(i => c.items[i]).map(it => {
    const charges = chargesOf(it);
    return `<tr class="${it[2] ? 'in' : ''}"><td class="nt">${it[2] ? '' : slotName(it[0])}</td><td class="ic">${it[2] ? '' : iconOf(it)}</td>` +
      `<td class="l nm">${it[2] ? `<span class="inl">${iconOf(it)}${esc(it[1])}</span>` : esc(it[1])}${charges ? ` <span class="none">· ${charges}</span>` : ''}</td><td class="dmg c-der">${listDamage(it) >= 0 ? listDamage(it) : ''}</td><td class="dmg c-kin">${listKinetic(it) >= 0 ? listKinetic(it) : ''}</td>` +
      `<td class="arm c-der">${it[12] >= 0 ? it[12] : ''}</td><td>${it[7] > 0 ? it[7] : ''}</td><td>${it[8] > 0 ? it[8] : ''}</td><td>${tenths(it[4])} kg</td></tr>`;
  }).join('') + '</table></div>';
  return livePicture(c);
}
// The inventory as the game lays it out: the body with pouch and quiver, then the backpack
function livePicture(c) {
  let doll = '';
  for (const s in DOLL) doll += `<div style="grid-column:${DOLL[s][0]};grid-row:${DOLL[s][1]}">${slotHtml(c.items, +s, isWounded(c, +s), PART[s])}</div>`;
  let pack = '';
  for (let s = 13; s < 30; s++) pack += slotHtml(c.items, s, false);
  return `<div class="inv"><div class="doll">${doll}<span class="cap" style="grid-column:1;grid-row:3">Pouch</span><span class="cap" style="grid-column:4/6;grid-row:3">Quiver</span></div>` +
    `<div class="pack"><span class="cap">Backpack</span><div class="grid">${pack}</div></div></div>`;
}
function liveChampion(c) {
  const colour = c.hero >= 0 ? COLOUR(c.hero) : 'var(--text)';
  const attrs = '<div class="wide"><table><tr><th>Attribute</th><th>Current</th><th>Max</th><th class="c-bar l">Against the best</th></tr>' + c.attrs.map((a, i) =>
    `<tr><td>${esc(a[0])}</td><td class="${a[1] < a[2] ? 'bad' : a[1] > a[2] ? 'raised' : ''}">${a[1]}</td><td>${a[2]}</td><td class="c-bar">${attributeBar(c, i, colour)}</td></tr>`).join('') + '</table></div>';
  const count = c.items.length;
  const choice = `<span class="seg" onclick="event.stopPropagation()"><button class="ch${st.list ? '' : ' on'}" onclick="set({list:false})">Picture</button>` +
    `<button class="ch${st.list ? ' on' : ''}" onclick="set({list:true})">List</button></span>`;
  return `<div class="champ"><div class="a-v">${liveHead(c, colour)}${liveVitals(c, colour)}</div>` +
    `<div class="a-a">${section('attrs', 'Attributes', '', attrs)}${section('ranks', 'Ranks', 'way to the next rank', liveRanks(c, colour))}</div>` +
    `<div class="a-i">${section('inventory', 'Inventory', `${count} ${count === 1 ? 'item' : 'items'} &nbsp;${choice}`, liveInventory(c))}</div></div>`;
}
function championsPage() {
  const all = LIVE.champions;
  if (!all.length) return '<div class="note">No champion in the party when the page was written.</div>';
  const shown = isParty() ? all : all.filter(c => c.hero === st.who);
  if (!shown.length) return `<div class="note">${esc(whoName(st.who))} was not in the party when the page was written.</div>`;
  let html = '<div class="note">The champions as they were when the page was written.</div>';
  if (!isParty()) return html + liveChampion(shown[0]);
  // The line is there with nothing on the cursor too, as in the window
  html += `<div class="oncursor">On the cursor: ${slotHtml(LIVE.cursor ? [LIVE.cursor] : [], -1, false)}` +
    (LIVE.cursor ? `<b>${esc(LIVE.cursor[1])}</b>` : '<span class="nt">nothing</span>') + '</div>';
  return html + '<div class="lcards">' + shown.map(liveCard).join('') + '</div>';
}

// A slot's tooltip follows the pointer and stays inside the page; a click on a chest keeps it
// open until the next click, for touch screens
const tip = document.createElement('div');
tip.id = 'tip';
tip.hidden = true;
document.body.appendChild(tip);
let pinned = null;
function placeTip(x, y) {
  const r = tip.getBoundingClientRect();
  let px = x + 16, py = y + 18;
  if (px + r.width > innerWidth - 8) px = Math.max(8, x - r.width - 12);
  if (py + r.height > innerHeight - 8) py = Math.max(8, y - r.height - 12);
  tip.style.left = px + 'px';
  tip.style.top = py + 'px';
}
function showTip(el, x, y) {
  tip.innerHTML = TIPS[+el.dataset.tip] || '';
  tip.hidden = false;
  if (x === undefined) { const r = el.getBoundingClientRect(); x = r.right - 12; y = r.bottom - 14; }
  placeTip(x, y);
}
function hideTip(unpin) {
  if (unpin && pinned) { pinned.classList.remove('pin'); pinned = null; }
  if (!pinned) tip.hidden = true;
}
document.addEventListener('mousemove', e => {
  if (pinned) return;
  const el = e.target.closest ? e.target.closest('[data-tip]') : null;
  if (el) showTip(el, e.clientX, e.clientY); else tip.hidden = true;
});
document.addEventListener('click', e => {
  const el = e.target.closest ? e.target.closest('.slot') : null;
  const was = pinned;
  hideTip(true);
  if (!el) return;
  e.stopPropagation();  // a slot in a card does not open the champion
  if (el.dataset.pin && el !== was) { pinned = el; el.classList.add('pin'); showTip(el, e.clientX, e.clientY); }
}, true);
document.addEventListener('focusin', e => { const el = e.target.closest ? e.target.closest('[data-tip]') : null; if (el && !pinned) showTip(el); });
document.addEventListener('focusout', () => hideTip(false));

function draw() {
  TIPS = [];
  hideTip(true);
  if (st.level !== -1 && !D.views[String(st.level)]) st.level = -1;
  frame();
  let html;
  if (N === 0 && D.levels.length === 0) html = '<div class="note">No statistics yet.</div>';
  else html = st.topic === CHAMPIONS ? championsPage() : st.per ? perLevel() : topicPage();
  document.getElementById('view').innerHTML = html;
  fit();
  writeLink();
}

// ---- following the page's width (the window does the same in HeroStatsLayout) ----
// How many of count equal boxes stand in a row: as many as fit, but four never three and one
function across(count, available, smallest, gap) {
  let fits = 1;
  while (fits < count && (fits + 1) * smallest + fits * gap <= available) fits++;
  return count === 4 && fits === 3 ? 2 : fits;
}
// The bar cells and the cells worked out from others (Rate, Avg) get a class by column, their
// headers too, so a narrow table can leave the whole column out
function tagColumns(table) {
  const rows = [...table.rows], head = rows[0];
  if (!head) return;
  const bars = new Set(), derived = new Set();
  rows.forEach(r => [...r.cells].forEach(c => { if (c.classList.contains('bar') || c.classList.contains('whobar')) bars.add(c.cellIndex); }));
  [...head.cells].forEach(c => { if (/^(Rate|Avg)\b/.test(c.textContent.trim())) derived.add(c.cellIndex); });
  rows.forEach(r => { if (r.cells.length !== head.cells.length) return;
    bars.forEach(i => r.cells[i].classList.add('c-bar')); derived.forEach(i => r.cells[i].classList.add('c-der')); });
}
function fitTables(view) {
  view.querySelectorAll('.wide').forEach(w => {
    const table = w.querySelector('table');
    if (!table) return;
    // What a table too wide gives up, in this order (HeroStatsLayout_FitTable): its bar, the
    // champions' names, half its padding, the worked-out columns, the label's single line, the
    // long form of big numbers; then it scrolls
    // (the inventory list's Kinetic goes before anything else)
    const steps = [...(table.querySelector('.c-kin') ? ['nokin'] : []), 'nobar', 'dots', 'tight', 'noder', 'wrap', 'short', 'scrolls'];
    w.classList.remove('nokin');
    w.classList.remove(...steps);
    for (const step of steps) {
      if (table.offsetWidth <= w.clientWidth + 1) break;
      w.classList.add(step);
    }
    // A header that shows the dot alone names its champion when hovered
    table.querySelectorAll('th[data-name]').forEach(th => { if (w.classList.contains('dots')) th.title = th.dataset.name; else th.removeAttribute('title'); });
  });
}
function fit() {
  const view = document.getElementById('view'), top = document.getElementById('top');
  // The row of level numbers that does not fit becomes the stepper
  const seg = top.querySelector('.seg');
  if (seg) {
    const tooWide = () => top.querySelector('.seg').offsetWidth > top.clientWidth - 20;
    if (st.compact) { st.compact = false; frame(); }
    if (tooWide()) { st.compact = true; frame(); }
  }
  const style = getComputedStyle(view);
  const width = view.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
  view.querySelectorAll('table').forEach(tagColumns);
  // Sections side by side when each fits in half the page at its full width, each in the column
  // shorter so far
  const secs = view.querySelector('.secs');
  if (secs) {
    const all = [...secs.children];
    view.classList.add('measure');
    const needs = all.map(s => Math.max(0, ...[...s.querySelectorAll('table')].map(x => x.offsetWidth)));
    view.classList.remove('measure');
    if (all.length >= 2 && needs.every(n => n <= (width - 22) / 2)) {
      const heights = all.map(s => s.offsetHeight);
      const cols = [document.createElement('div'), document.createElement('div')], tall = [0, 0];
      all.forEach((s, i) => { const c = tall[1] < tall[0] ? 1 : 0; cols[c].appendChild(s); tall[c] += heights[i]; });
      secs.classList.add('two');
      secs.append(cols[0], cols[1]);
    }
  }
  fitTables(view);
  view.querySelectorAll('.tiles').forEach(g => { g.style.gridTemplateColumns = `repeat(${across(g.children.length, width, 150, 8)},1fr)`; });
  view.querySelectorAll('.cards').forEach(g => { g.style.gridTemplateColumns = `repeat(${across(g.children.length, width, 230, 8)},minmax(0,480px))`; });
  view.querySelectorAll('.lcards').forEach(g => {
    const n = across(g.children.length, width, 290, 10);
    g.style.gridTemplateColumns = `repeat(${n},minmax(0,480px))`;
    g.classList.toggle('full', n === g.children.length);   // all in one row: the cards show attributes and inventory
  });
  view.querySelectorAll('.facts').forEach(g => { g.style.gridTemplateColumns = `repeat(${across(g.children.length, width, 230, 10)},minmax(0,1fr))`; });
  view.querySelectorAll('.topics').forEach(g => { g.style.gridTemplateColumns = width >= 528 ? '1fr 1fr' : '1fr'; });
}
let fitTimer = 0;
addEventListener('resize', () => { clearTimeout(fitTimer); fitTimer = setTimeout(draw, 80); });
// The column guide: the cells and the header of the column under the pointer
let hot = [];
document.addEventListener('mousemove', e => {
  hot.forEach(c => c.classList.remove('hc'));
  hot = [];
  const cell = e.target.closest ? e.target.closest('#view td') : null;
  if (!cell || cell.cellIndex === 0) return;
  const table = cell.closest('table'), n = cell.parentNode.cells.length;
  [...table.rows].forEach(r => { if (r.cells.length === n && r.cells[cell.cellIndex]) { r.cells[cell.cellIndex].classList.add('hc'); hot.push(r.cells[cell.cellIndex]); } });
});

// The view as a link: #who=Party&topic=Records&level=run (level: run, a number or per; side=1 for
// the champions side by side). Read when the page opens, from the hash or the query string, and
// written back after every change, so the address bar can be copied.
const same = (a, b) => String(a).trim().toLowerCase() === String(b).trim().toLowerCase();
function readLink() {
  const q = new URLSearchParams(location.search), h = new URLSearchParams(location.hash.replace(/^#/, ''));
  const get = k => h.has(k) ? h.get(k) : q.get(k);
  const who = get('who'), topic = get('topic'), level = get('level'), side = get('side');
  if (who !== null) {
    const i = same(who, 'party') ? N : H.findIndex(c => same(c.name, who));
    if (i >= 0) st.who = i; else if (/^\d+$/.test(who) && +who <= N) st.who = +who;
  }
  if (topic !== null) {
    const i = same(topic, 'champions') ? CHAMPIONS : TOP.findIndex(t => same(t.label, topic));
    if (i >= 0) st.topic = i;
  }
  if (level !== null) {
    if (same(level, 'per')) st.per = true;
    else if (same(level, 'run') || same(level, 'whole') || same(level, 'whole run')) { st.per = false; st.level = -1; }
    else if (/^\d+$/.test(level) && D.views[level]) { st.per = false; st.level = +level; }
  }
  if (side !== null) st.side = side === '1';
}
function writeLink() {
  const h = new URLSearchParams();
  h.set('who', whoName(st.who));
  h.set('topic', st.topic === CHAMPIONS ? 'Champions' : TOP[st.topic].label);
  if (st.topic !== CHAMPIONS) {
    h.set('level', st.per ? 'per' : st.level === -1 ? 'run' : st.level);
    if (st.per && st.side) h.set('side', '1');
  }
  try { history.replaceState(null, '', '#' + h.toString().replace(/\+/g, '%20')); } catch (e) {}
}
readLink();
draw();
</script>
</body></html>
)HTML";

std::string HeroStatsExport_Html(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                 const HeroStatsNames &names, const HeroStatsExportInfo &info)
{
  std::string html = PAGE_HEAD;
  html += "<h1>Statistics</h1>\n<div class=\"about\" id=\"about\"></div>\n";
  html += "<div class=\"im\">\n";
  html += "  <div class=\"bar1\" id=\"top\"></div>\n  <div class=\"tabs\" id=\"tabs\"></div>\n  <div id=\"view\"></div>\n</div>\n";
  html += "<script type=\"application/json\" id=\"stats\">" + HeroStatsExport_Json(stats, columns, names, info) +
          "</script>\n";
  // The line under the title, from the JSON so that names are escaped once
  html += "<script>{const d=JSON.parse(document.getElementById('stats').textContent);"
          "document.getElementById('about').textContent=[d.heroes.map(h=>h.name).join(', '),d.dungeon,d.date]"
          ".filter(s=>s).join(' \\u00b7 ');}</script>";
  html += PAGE_SCRIPT;
  return html;
}
