// HeroStatsExport.cpp : the statistics as one HTML file to share.
#include "HeroStatsExport.h"

#include <cstdio>

// The JSON the page script reads:
// { "dungeon", "date", "separator" (between thousands), "heroes": [{"name", "left"}], "levels": [level, ...],
//   "numbers": the catalogue, one per HeroStatsNumber: [{"label", "help", "party", "run", "bad", "ratio", "pct", "ticks"}],
//   "topics": one per HeroStatsTopic: [{"label", "rows": [[number, depth], ...], "tiles": [number, ...],
//             "grid": [number, ...] (Champions side by side), "details": [HeroStatsDetail, ...] (the party's),
//             "heroDetails": [...] (a champion's)}],
//   "heroTiles": [number, ...] (a champion's Overview),
//   "views": { "-1" for the whole run and one per visited level: {
//     "records": per hero then the party, a value per number, null where it does not apply,
//     "summaries": per hero then the party, one line per topic,
//     "pages": per hero then the party {
//       "weapons": [[name, tries, hits, damage, kills, uses, [[option, tries, hits, damage, kills, uses], ...], [by hero]], ...]
//                  (tries: blows and missiles; uses: actions that cannot hit),
//       "spells": [[name, casts, failed, mana, wasted, damage, kills, [by hero]], ...],
//       "taken": [[text, amount, isBad, [by hero]], ...], "healing": [[text, amount, [by hero]], ...],
//       "deaths": [text, ...], "eaten": [[text, amount, [by hero]], ...], "drunk": [...], "eatenTotal", "drunkTotal" },
//     "monsters": [[name, [kills per hero], door, pit, other, total], ...] most killed first,
//     "items": [[name, picked up, used in walls], ...] } } }
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
  int rest[4] = {line.door, line.pit, line.other, line.total};
  for (int i = 0; i < 4; i++) out += "," + Number(rest[i]);
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
    out += "]," + Ints(weapon.byColumn) + "]";
  }
  out += "],\"spells\":[";
  for (size_t i = 0; i < page.spells.size(); i++)
  {
    const HeroStatsSpellLine &spell = page.spells[i];
    int counts[6] = {spell.casts, spell.failed, spell.mana, spell.wasted, spell.damage, spell.kills};
    out += (i > 0 ? ",[" : "[") + HeroStatsExport_JsonString(spell.name);
    for (int k = 0; k < 6; k++) out += "," + Number(counts[k]);
    out += "," + Ints(spell.byColumn) + "]";
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
    pages += comma + PageJson(HeroStatsPages_Hero(stats, columns, who, level, names));
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
           Number(items[i].pickedUp) + "," + Number(items[i].usedInWall) + "]";
    first = false;
  }
  return out + "]}";
}

static std::string Details(const std::vector<HeroStatsDetail> &details)
{
  std::vector<int> ids(details.begin(), details.end());
  return Ints(ids);
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
           Bool(number.isRatio) + ",\"pct\":" + Bool(n == HeroNumHitRate) + ",\"ticks\":" + Bool(number.isTicks) + "}";
  }
  out += "],\"topics\":[";
  for (int t = 0; t < HERO_TOPIC_COUNT; t++)
  {
    const HeroStatsTopicInfo &topic = HeroStatsCatalogue_Topic(t);
    out += std::string(t > 0 ? "," : "") + "{\"label\":" + HeroStatsExport_JsonString(topic.label) + ",\"rows\":[";
    for (size_t r = 0; r < topic.rows.size(); r++)
      out += std::string(r > 0 ? ",[" : "[") + Number(topic.rows[r].number) + "," + Number(topic.rows[r].depth) + "]";
    std::vector<int> tiles(topic.tiles.begin(), topic.tiles.end());
    std::vector<HeroStatsNumber> gridNumbers = HeroStatsCatalogue_GridNumbers(t);
    std::vector<int> grid(gridNumbers.begin(), gridNumbers.end());
    out += "],\"tiles\":" + Ints(tiles) + ",\"grid\":" + Ints(grid);
    out += ",\"details\":" + Details(HeroStatsPages_Details(t, HERO_STATS_PARTY));
    out += ",\"heroDetails\":" + Details(HeroStatsPages_Details(t, HERO_STATS_NOBODY)) + "}";
  }
  int heroTiles[4] = {HeroNumDamage, HeroNumKills, HeroNumTaken, HeroNumDeaths};
  out += "],\"heroTiles\":" + Numbers(heroTiles, 4);
  out += ",\"views\":{\"-1\":" + ViewJson(stats, columns, names, HERO_STATS_WHOLE_RUN);
  for (size_t i = 0; i < levels.size(); i++)
    out += ",\"" + Number(levels[i]) + "\":" + ViewJson(stats, columns, names, levels[i]);
  return out + "}}";
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
  body { background:#121317; color:#ccc; font:14px system-ui,sans-serif; margin:24px }
  h1 { font-size:18px; color:#fff; margin:0 0 4px; font-weight:600 }
  .about { color:#999; margin-bottom:12px }
  .im { max-width:1000px; background:var(--bg); color:var(--text); font:12.5px/1.45 "DejaVu Sans Mono","Bitstream Vera Sans Mono",Menlo,Consolas,monospace;
    border:1px solid var(--line); border-radius:6px; overflow:hidden }
  .title { background:#294a7a; color:#fff; padding:4px 10px; display:flex; gap:12px }
  .title span:last-child { margin-left:auto; color:#b9c9e4 }
  .bar1 { display:flex; justify-content:space-between; align-items:center; gap:10px; flex-wrap:wrap; padding:8px 10px 0 }
  .whos, .seg { display:flex; gap:4px; flex-wrap:wrap; align-items:center }
  .seg { gap:1px }
  button { font:inherit; cursor:pointer }
  .who { background:transparent; color:var(--text); border:1px solid #3a3d46; border-radius:4px; padding:3px 9px; display:flex; gap:6px; align-items:center }
  .who:hover { background:var(--hover) } .who.on { background:var(--panel); border-color:#6b7080; color:#fff } .who.left { color:var(--dim) }
  .dot { width:8px; height:8px; border-radius:50%; display:inline-block; flex:none } .left .dot, th.left .dot { opacity:.45 }
  .ch { background:var(--panel); color:var(--dim); border:0; border-radius:3px; padding:3px 8px }
  .ch:hover { background:var(--hover) } .ch.on { background:var(--on); color:#fff } .ch:disabled { opacity:.35; cursor:default }
  .lab { color:var(--faint); margin:0 6px 0 8px }
  select { background:var(--panel); color:var(--text); border:1px solid #4a4d57; font:inherit; border-radius:3px }
  .tabs { display:flex; gap:2px; padding:6px 10px 0; border-bottom:1px solid var(--line); flex-wrap:wrap }
  .tab { background:none; border:0; color:var(--dim); padding:5px 10px 6px; border-bottom:2px solid transparent; margin-bottom:-1px }
  .tab:hover { color:#fff } .tab.on { color:#fff; border-bottom-color:var(--on) }
  #view { max-height:680px; overflow:auto; padding:10px 12px 14px }
  .tiles { display:grid; grid-template-columns:repeat(4,1fr); gap:8px; margin-bottom:12px }
  .tile { background:var(--panel); border-radius:4px; padding:7px 10px }
  .tile .v { font-size:20px; color:#fff } .tile.bad .v { color:var(--bad) } .tile .l { color:var(--dim) }
  .cards { display:grid; grid-template-columns:repeat(4,1fr); gap:8px; margin-bottom:12px }
  .card { background:var(--panel); border-radius:4px; padding:7px 10px 9px; border-top:3px solid; cursor:pointer; display:grid; gap:3px }
  .card:hover { background:var(--hover) } .card .nm { color:#fff; display:flex; justify-content:space-between } .card .nm span { color:var(--dim) }
  .cr { display:grid; grid-template-columns:58px 1fr 54px; align-items:center; gap:6px } .cr .l { color:var(--dim) } .cr .n { text-align:right }
  .mini { height:6px; background:var(--track); border-radius:2px; overflow:hidden } .mini i { display:block; height:100% }
  .topics { display:grid; grid-template-columns:1fr 1fr; gap:8px; margin-bottom:12px }
  .topic { background:var(--panel); border-radius:4px; padding:7px 10px; cursor:pointer; overflow:hidden; white-space:nowrap; text-overflow:ellipsis }
  .topic:hover { background:var(--hover) } .topic .t { color:#fff } .topic .s { color:var(--dim) }
  .sec { margin-bottom:10px } .sech { display:flex; align-items:baseline; gap:8px; padding:3px 0 5px; border-bottom:1px solid var(--line); cursor:pointer; user-select:none }
  .sech .t { text-transform:uppercase; color:var(--text) } .sech:hover .t { color:#fff }
  .sech .a { color:var(--dim); width:10px } .sech .s { color:var(--dim); margin-left:auto }
  .closed .secb { display:none }
  table { border-collapse:collapse; width:100% }
  td, th { padding:3px 8px; text-align:right; white-space:nowrap; border-bottom:1px solid var(--row) }
  th { color:var(--dim); font-weight:normal; border-bottom-color:var(--line) }
  th.sort { cursor:pointer; user-select:none } th.sort:hover { color:#fff } th .arrow { color:var(--dim) }
  td:first-child, th:first-child { text-align:left }
  tr.d1 td { color:var(--part) } tr.d1 td:first-child { padding-left:22px; color:var(--dim) }
  tr.all td { background:var(--panel); color:#fff }
  tr.lv { cursor:pointer } tr.lv:hover td { background:var(--hover) }
  tr.wrow { cursor:pointer } tr.wrow:hover td { background:var(--hover) }
  tr.opt td { color:var(--part) } tr.opt td:first-child { padding-left:26px; color:var(--dim) }
  .z { color:var(--faint) } .gone { color:var(--dim) }
  .pill { color:var(--bad); background:rgba(229,115,95,.16); border-radius:3px; padding:0 4px }
  td.bar { width:170px } td.who { width:140px }
  .share { display:flex; height:9px; background:var(--track); border-radius:2px; overflow:hidden; min-width:100px } .share i { display:block; height:100% }
  .choices { display:flex; gap:10px; align-items:center; flex-wrap:wrap; margin-bottom:8px }
  .note { color:var(--dim); padding:4px 0 }
  .wide { overflow-x:auto }
  @media (max-width:640px) { .tiles, .cards { grid-template-columns:1fr 1fr } .topics { grid-template-columns:1fr } }
</style></head>
<body>
)HTML";

static const char *const PAGE_SCRIPT = R"HTML(
<script>
const D = JSON.parse(document.getElementById('stats').textContent);
const H = D.heroes, N = H.length, NUM = D.numbers, TOP = D.topics;
const COLOUR = i => `var(--c${i % 4})`;
const DETAIL = {WEAPONS: 0, SPELLS: 1, HARM: 2, HEALING: 3, DEATHS: 4, CONSUMED: 5, ITEMS: 6, MONSTERS: 7};
const NUMBERS_SECTION = 'numbers', KEY_SECTION = 'key';
const st = {who: N, topic: 0, level: -1, per: false, side: false, grid: {}, closed: {}, sort: {}, open: {}};

const esc = s => String(s).replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'})[c]);
const fmt = n => String(n).replace(/\B(?=(\d{3})+(?!\d))/g, D.separator);
const lvName = l => l === -1 ? 'Whole run' : 'Lv ' + l;
const whoName = i => i === N ? 'Party' : H[i].name;
const isParty = () => st.who === N;
const view = l => D.views[String(l)];
const rec = (i, l) => view(l).records[i];
const page = () => view(st.level).pages[st.who];
const applies = (n, i, l) => !(NUM[n].party && i < N) && !(NUM[n].run && l !== -1);
const text = (n, v) => NUM[n].ratio && v < 0 ? '-' : NUM[n].pct ? v + '%' : fmt(v);
const sum = a => a.reduce((x, y) => x + Math.max(0, y || 0), 0);
const dotOf = i => `<span class="dot" style="background:${COLOUR(i)}"></span>`;

function set(changes) { Object.assign(st, changes); draw(); }

// A number in a cell: a dim dot for zero, bad news on a red pill, nothing where it does not apply
function cell(v, bad, ratio, label, cls) {
  const c = cls ? ` class="${cls}"` : '';
  if (v === null || v === undefined) return `<td${c}></td>`;
  if (v === 0) return `<td class="z">·</td>`;
  if (ratio && v < 0) return `<td class="z">-</td>`;
  return bad && v > 0 ? `<td${c}><span class="pill">${label}</span></td>` : `<td${c}>${label}</td>`;
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
  const head = cols.map((c, i) => `<th class="sort${c.cls ? ' ' + c.cls : ''}" onclick="sortBy('${id}',${i},${c.num ? 1 : 0})">` +
    (c.html || esc(c.label)) + (s.col === i ? ` <span class="arrow">${s.desc ? '&#9660;' : '&#9650;'}</span>` : '') + '</th>').join('');
  const line = r => `<tr${r.cls ? ` class="${r.cls}"` : ''}${r.act ? ` onclick="${r.act}"` : ''}>` + r.cells.map(c => c.html).join('') + '</tr>';
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
const nameCol = label => ({label});
const numCol = label => ({label, num: true});
const heroCol = i => ({label: H[i].name, html: dotOf(i) + ' ' + esc(H[i].name), num: true, cls: H[i].left ? 'left' : ''});
const td = (v, html) => ({v, html});
const nameTd = name => td(name, `<td>${esc(name)}</td>`);

// ---------------------------------------------------------------------------------------------

function picker() {
  const L = D.levels, one = !st.per;
  let out = `<div class="seg"><button class="ch${one && st.level === -1 ? ' on' : ''}" onclick="set({per:false,level:-1})">Whole run</button>` +
    `<button class="ch${st.per ? ' on' : ''}" onclick="set({per:true})">Per level</button><span class="lab">Lv</span>`;
  if (L.length <= 16)
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
  document.getElementById('top').innerHTML = `<div class="whos">${whos}</div>${picker()}`;
  document.getElementById('tabs').innerHTML = TOP.map((t, i) =>
    `<button class="tab${st.topic === i ? ' on' : ''}" onclick="set({topic:${i}})">${esc(t.label)}</button>`).join('');
  document.getElementById('where').textContent =
    [whoName(st.who), TOP[st.topic].label, st.per ? 'Per level' : lvName(st.level)].join(' · ');
}

function tiles(numbers) {
  const r = rec(st.who, st.level);
  const shown = numbers.filter(n => applies(n, st.who, st.level)).slice(0, 4);
  return '<div class="tiles">' + shown.map(n => `<div class="tile${NUM[n].bad && r[n] > 0 ? ' bad' : ''}"><div class="v">${text(n, r[n])}</div>` +
    `<div class="l">${esc(NUM[n].label)}</div></div>`).join('') + '</div>';
}

function numbersTable(t) {
  const party = rec(N, st.level), one = !isParty();
  const rows = TOP[t].rows.filter(([n]) => applies(n, st.who, st.level));
  let head = '<tr><th></th>';
  if (one) head += `<th class="${H[st.who].left ? 'left' : ''}">${dotOf(st.who)} ${esc(H[st.who].name)}</th><th>Party</th><th>Share of party</th>`;
  else head += H.map((h, i) => `<th class="${h.left ? 'left' : ''}">${dotOf(i)} ${esc(h.name)}</th>`).join('') + '<th>Party</th><th>Share</th>';
  let body = '';
  for (const [n, depth] of rows) {
    const info = NUM[n], summable = !info.ratio && !info.party;
    body += `<tr class="d${depth}"><td${info.help ? ` title="${esc(info.help)}"` : ''}>${esc(info.label)}</td>`;
    if (one) {
      const mine = rec(st.who, st.level)[n];
      body += numCell(n, mine) + numCell(n, party[n]) + `<td class="bar">${summable && party[n] > 0 ? partBar(mine, party[n], st.who) : ''}</td>`;
    } else {
      for (let i = 0; i < N; i++) body += numCell(n, info.party ? null : rec(i, st.level)[n], H[i].left ? 'gone' : '');
      body += numCell(n, party[n]) + `<td class="bar">${summable ? shareBar(H.map((h, i) => rec(i, st.level)[n])) : ''}</td>`;
    }
    body += '</tr>';
  }
  return `<div class="wide"><table>${head}</tr>${body}</table></div>`;
}

const attackCells = (l, cls) => {
  const [tries, hits, damage, kills, uses] = l;
  if (tries === 0 && uses > 0) return [td(uses, plain(uses)), td(-1, '<td class="z">-</td>'), td(-1, '<td class="z">-</td>'),
    td(-1, '<td class="z">-</td>'), td(-1, '<td class="z">-</td>'), td(-1, '<td class="z">-</td>')];
  const rate = tries ? hits / tries : -1, avg = hits ? damage / hits : -1;
  return [td(tries + uses, plain(tries + uses)), td(hits, plain(hits)), td(rate, rate < 0 ? '<td class="z">-</td>' : `<td>${Math.round(rate * 100)}%</td>`),
    td(avg, avg < 0 ? '<td class="z">-</td>' : `<td>${fmt(Math.round(avg))}</td>`), td(damage, plain(damage)), td(kills, plain(kills))];
};
const whoTd = by => td(sum(by), `<td class="who">${isParty() ? shareBar(by) : ''}</td>`);

function weapons(p) {
  const all = [0, 0, 0, 0, 0];
  for (const w of p.weapons) for (let k = 0; k < 5; k++) all[k] += w[k + 1];
  const summary = p.weapons.length ? `${fmt(all[2])} damage · ${fmt(all[3])} kills` : 'none';
  if (!p.weapons.length) return section(DETAIL.WEAPONS, 'Weapons', summary, '<div class="note">No attacks.</div>');
  const cols = [nameCol('Weapon'), numCol('Tries'), numCol('Hits'), numCol('Rate'), numCol('Avg'), numCol('Damage'), numCol('Kills')];
  if (isParty()) cols.push(numCol('Who'));
  const rows = p.weapons.map(w => {
    const key = st.who + ':' + w[0], open = !!st.open[key];
    const cells = [td(w[0], `<td>${open ? '&#9662;' : '&#9656;'} ${esc(w[0])}</td>`)].concat(attackCells(w.slice(1, 6)));
    if (isParty()) cells.push(whoTd(w[7]));
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
  const cols = ['Casts', 'Failed', 'Mana', 'Wasted', 'Damage', 'Kills'].map(numCol);
  cols.unshift(nameCol('Spell'));
  if (isParty()) cols.push(numCol('Who'));
  const cellsOf = v => v.map((x, k) => td(x, plain(x, k === 1 || k === 3)));
  const rows = p.spells.map(s => ({cells: [nameTd(s[0])].concat(cellsOf(s.slice(1, 7)), isParty() ? [whoTd(s[7])] : [])}));
  const foot = {cls: 'all', cells: [nameTd('All spells')].concat(cellsOf(all), isParty() ? [td(0, '<td></td>')] : [])};
  return section(DETAIL.SPELLS, 'Spells', summary, sortTable('spells', cols, rows, foot, 3));
}

function amounts(id, title, column, what, lines, withBad) {
  const total = lines.reduce((a, l) => a + l[1], 0);
  if (!lines.length) return section(id, title, 'none', '<div class="note">None.</div>');
  const cols = [nameCol('Source'), numCol(column)];
  if (isParty()) cols.push(numCol('Who'));
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
    if (!line) lines.push(line = [l[0], 0, 0, H.map(() => 0)]);
    line[k] += l[1];
    l[2].forEach((v, i) => line[3][i] += v);
  }
  const summary = `${fmt(p.eatenTotal)} eaten · ${fmt(p.drunkTotal)} drunk`;
  if (!lines.length) return section(DETAIL.CONSUMED, 'Eaten and drunk', 'nothing', '<div class="note">Nothing eaten or drunk.</div>');
  const cols = [nameCol('Item'), numCol('Eaten'), numCol('Drunk')];
  if (isParty()) cols.push(numCol('Who'));
  const rows = lines.map(l => ({cells: [nameTd(l[0]), td(l[1], plain(l[1])), td(l[2], plain(l[2]))].concat(isParty() ? [whoTd(l[3])] : [])}));
  const foot = {cls: 'all', cells: [nameTd('Everything'), td(0, plain(p.eatenTotal)), td(0, plain(p.drunkTotal))].concat(isParty() ? [td(0, '<td></td>')] : [])};
  return section(DETAIL.CONSUMED, 'Eaten and drunk', summary, sortTable('consumed', cols, rows, foot, 1));
}

function items() {
  const lines = view(st.level).items;
  const all = lines.reduce((a, l) => [a[0] + l[1], a[1] + l[2]], [0, 0]);
  if (!lines.length) return section(DETAIL.ITEMS, 'Items', 'none', '<div class="note">No items picked up or used in walls.</div>');
  const rows = lines.map(l => ({cells: [nameTd(l[0]), td(l[1], plain(l[1])), td(l[2], plain(l[2]))]}));
  const foot = {cls: 'all', cells: [nameTd('All items'), td(0, plain(all[0])), td(0, plain(all[1]))]};
  return section(DETAIL.ITEMS, 'Items', `${fmt(all[0])} picked up · ${fmt(all[1])} used in walls`,
    sortTable('items', [nameCol('Item'), numCol('Picked up'), numCol('Used in walls')], rows, foot, 1));
}

function monsters() {
  let lines = view(st.level).monsters;
  if (!isParty()) lines = lines.filter(m => m[1][st.who] > 0).map(m => [m[0], [m[1][st.who]], 0, 0, 0, m[1][st.who]]);
  if (!lines.length) return section(DETAIL.MONSTERS, 'Monsters killed', 'none', '<div class="note">No monsters killed.</div>');
  const all = lines.reduce((a, m) => { m[1].forEach((v, i) => a[1][i] += v); for (let k = 2; k < 6; k++) a[k] += m[k]; return a; },
    ['All monsters', lines[0][1].map(() => 0), 0, 0, 0, 0]);
  const summary = `${fmt(all[5])} kills · ${lines.length} kinds`;
  let cols, cellsOf;
  if (isParty()) {
    const extra = [[2, 'Door'], [3, 'Pit'], [4, 'Other']].filter(([k]) => all[k] > 0);
    cols = [nameCol('Monster')].concat(H.map((h, i) => heroCol(i)), extra.map(([, l]) => numCol(l)), [numCol('All'), numCol('Who')]);
    cellsOf = (m, foot) => [nameTd(m[0])].concat(m[1].map((v, i) => td(v, plain(v, false, H[i].left ? 'gone' : ''))),
      extra.map(([k]) => td(m[k], plain(m[k]))), [td(m[5], plain(m[5])), foot ? td(0, '<td></td>') : whoTd(m[1])]);
  } else {
    cols = [nameCol('Monster'), numCol('Kills')];
    cellsOf = m => [nameTd(m[0]), td(m[5], plain(m[5]))];
  }
  return section(DETAIL.MONSTERS, 'Monsters killed', summary,
    sortTable('monsters', cols, lines.map(m => ({cells: cellsOf(m)})), {cls: 'all', cells: cellsOf(all, true)}, cols.length - (isParty() ? 2 : 1)));
}

function cards() {
  const ROWS = [[0, 'Damage'], [4, 'Kills'], [-1, 'Taken'], [-2, 'Healed'], [-3, 'Mana'], [-4, 'Deaths']];
  const index = label => NUM.findIndex(n => n.label === label);
  const ids = ROWS.map(([, l], k) => [index(['Damage dealt', 'Kills', 'Damage taken', 'Healed', 'Mana spent', 'Deaths'][k]), l]);
  const party = rec(N, st.level)[ids[0][0]];
  const largest = ids.map(([n]) => Math.max(1, ...H.map((h, i) => rec(i, st.level)[n])));
  return '<div class="cards">' + H.map((h, i) => {
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

function topicTiles() {
  const sums = view(st.level).summaries[st.who];
  return '<div class="topics">' + TOP.map((t, i) => i === 0 ? '' :
    `<div class="topic" onclick="set({topic:${i}})" title="${esc(sums[i])}"><div class="t">${esc(t.label)}</div><div class="s">${esc(sums[i])}</div></div>`).join('') + '</div>';
}

function topicPage() {
  const t = st.topic, p = page();
  if (t === 0) {
    const body = tiles(isParty() ? TOP[0].tiles : D.heroTiles) + (isParty() ? cards() : topicTiles());
    return body + section(KEY_SECTION, 'Key numbers', isParty() ? 'one row from each topic' : 'compared with the party', numbersTable(0));
  }
  let html = tiles(TOP[t].tiles) +
    section(NUMBERS_SECTION, 'Numbers', isParty() ? 'each champion and the party' : esc(H[st.who].name) + ' and the party', numbersTable(t));
  for (const d of isParty() ? TOP[t].details : TOP[t].heroDetails) {
    if (d === DETAIL.WEAPONS) html += weapons(p);
    else if (d === DETAIL.SPELLS) html += spells(p);
    else if (d === DETAIL.HARM) html += amounts(DETAIL.HARM, 'Damage taken by source', 'Damage', 'taken', p.taken, true);
    else if (d === DETAIL.HEALING) html += amounts(DETAIL.HEALING, 'Healing by source', 'Healed', 'healed', p.healing, false);
    else if (d === DETAIL.DEATHS) html += deaths(p);
    else if (d === DETAIL.CONSUMED) html += consumed(p);
    else if (d === DETAIL.ITEMS) html += items();
    else if (d === DETAIL.MONSTERS) html += monsters();
  }
  return html;
}

// Per level: the Party's totals or its champions side by side, a champion's totals
function perLevel() {
  const t = st.topic, grid = TOP[t].grid, levels = D.levels.concat([-1]);
  let html = '';
  const side = isParty() && grid.length > 0 && st.side;
  if (isParty() && grid.length) {
    html += `<div class="choices"><span class="lab">Show</span><div class="seg"><button class="ch${!st.side ? ' on' : ''}" onclick="set({side:false})">Party totals</button>` +
      `<button class="ch${st.side ? ' on' : ''}" onclick="set({side:true})">Champions side by side</button></div></div>`;
  }
  let cols, rowOf;
  const levelTd = l => td(l, `<td>${lvName(l)}</td>`);
  if (side) {
    const n = grid.includes(st.grid[t]) ? st.grid[t] : grid[0];
    html += '<div class="choices"><span class="lab">Number</span><div class="seg">' + grid.map(g =>
      `<button class="ch${g === n ? ' on' : ''}" onclick="st.grid[${t}]=${g};draw()">${esc(NUM[g].label)}</button>`).join('') + '</div></div>';
    cols = [nameCol('Level')].concat(H.map((h, i) => heroCol(i)), [numCol('Party'), numCol('Share')]);
    rowOf = l => {
      const by = H.map((h, i) => rec(i, l)[n]);
      return [levelTd(l)].concat(by.map((v, i) => td(v, numCell(n, v, H[i].left ? 'gone' : ''))),
        [td(rec(N, l)[n], numCell(n, rec(N, l)[n])), td(rec(N, l)[n], `<td class="bar">${shareBar(by)}</td>`)]);
    };
  } else {
    const numbers = TOP[t].rows.filter(([n, depth]) => depth === 0 && applies(n, st.who, 0)).map(([n]) => n);
    const share = isParty() ? numbers.find(n => !NUM[n].party && !NUM[n].ratio) : undefined;
    cols = [nameCol('Level')].concat(numbers.map(n => numCol(NUM[n].label)));
    if (share !== undefined) cols.push(numCol('Who: ' + NUM[share].label));
    rowOf = l => [levelTd(l)].concat(numbers.map(n => td(rec(st.who, l)[n], numCell(n, rec(st.who, l)[n]))),
      share === undefined ? [] : [td(rec(N, l)[share], `<td class="who">${shareBar(H.map((h, i) => rec(i, l)[share]))}</td>`)]);
  }
  const rows = D.levels.map(l => ({cls: 'lv', act: `set({per:false,level:${l}})`, cells: rowOf(l)}));
  const foot = {cls: 'all lv', act: 'set({per:false,level:-1})', cells: rowOf(-1)};
  return html + sortTable('per' + t + (side ? 'g' : 't'), cols, rows, foot, 0);
}

function draw() {
  if (st.level !== -1 && !D.views[String(st.level)]) st.level = -1;
  frame();
  let html;
  if (N === 0 && D.levels.length === 0) html = '<div class="note">No statistics yet.</div>';
  else html = st.per ? perLevel() : topicPage();
  document.getElementById('view').innerHTML = html;
}
draw();
</script>
</body></html>
)HTML";

std::string HeroStatsExport_Html(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                                 const HeroStatsNames &names, const HeroStatsExportInfo &info)
{
  std::string html = PAGE_HEAD;
  html += "<h1>Statistics</h1>\n<div class=\"about\" id=\"about\"></div>\n";
  html += "<div class=\"im\">\n  <div class=\"title\"><span>Statistics</span><span id=\"where\"></span></div>\n";
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
