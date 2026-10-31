// HeroStatsExport.cpp : the statistics as one HTML file to share.
#include "HeroStatsExport.h"

#include <cstdio>

// The JSON the page script reads:
// { "dungeon", "date", "separator" (between thousands), "heroes": [{"name", "left"}], "levels": [level, ...], "wipes", "reloads",
//   "views": { "-1" for the whole run and one per visited level: {
//     "cells": per hero then the party [melee, ranged, magic, total, kills, taken, friendly fire
//              caused, healed, healing given, mana generated, spent, wasted, deaths],
//     "move": [steps, turns, wall bumps, teleports, pit falls, pit climbs, ticks, ticks asleep],
//     "monsters": [[name, [kills per hero], door, pit, other, total], ...] most killed first,
//     "pages": per hero then the party {
//       "sum": [attacks, spells, damage taken, healing, hardship] (the section summaries),
//       "weapons": [[name, tries, hits, damage, kills, uses, [[option, tries, hits, damage, kills, uses], ...]], ...]
//                  (tries: blows and missiles; uses: actions that cannot hit),
//       "spells": [[name, casts, successful, failed, mana, wasted, damage, kills], ...],
//       "taken": [[text, amount, isBad], ...], "healed", "healing": [[text, amount], ...],
//       "given", "resurrected", "deaths": [text, ...], "wipes", "reloads" } } } }

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

static std::string CellJson(const HeroStatsCell &c)
{
  int values[13] = {c.melee, c.ranged, c.magic, c.total, c.kills, c.taken, c.friendlyFire,
                    c.healed, c.given, c.generated, c.spent, c.wasted, c.deaths};
  return Numbers(values, 13);
}

static std::string MonsterJson(const HeroStatsMonsterLine &line)
{
  std::string out = "[" + HeroStatsExport_JsonString(line.name) + ",";
  out += Numbers(line.byColumn.data(), line.byColumn.size());
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
    out += "]";
  }
  return out + "]";
}

static std::string PageJson(const HeroStatsHeroPage &page)
{
  const HeroStatsSummaries &s = page.summaries;
  std::string out = "{\"sum\":[" + HeroStatsExport_JsonString(s.attacks) + "," + HeroStatsExport_JsonString(s.spells) +
                    "," + HeroStatsExport_JsonString(s.harm) + "," + HeroStatsExport_JsonString(s.healing) + "," +
                    HeroStatsExport_JsonString(s.hardship) + "]";
  out += ",\"weapons\":[";
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
    out += "]]";
  }
  out += "],\"spells\":[";
  for (size_t i = 0; i < page.spells.size(); i++)
  {
    const HeroStatsSpellLine &spell = page.spells[i];
    int counts[7] = {spell.casts, spell.successful, spell.failed, spell.mana, spell.wasted, spell.damage, spell.kills};
    out += (i > 0 ? ",[" : "[") + HeroStatsExport_JsonString(spell.name);
    for (int k = 0; k < 7; k++) out += "," + Number(counts[k]);
    out += "]";
  }
  out += "],\"taken\":" + AmountLinesJson(page.taken, true);
  out += ",\"healed\":" + Number(page.healedTotal);
  out += ",\"healing\":" + AmountLinesJson(page.healing, false);
  out += ",\"given\":" + Number(page.given) + ",\"resurrected\":" + Number(page.resurrected);
  out += ",\"deaths\":[";
  for (size_t i = 0; i < page.deathLines.size(); i++)
    out += (i > 0 ? "," : "") + HeroStatsExport_JsonString(page.deathLines[i]);
  out += "],\"wipes\":" + Number(page.wipes) + ",\"reloads\":" + Number(page.reloads) + "}";
  return out;
}

static std::string ViewJson(const HeroStats &stats, const std::vector<HeroStatsColumn> &columns,
                            const HeroStatsNames &names, int level)
{
  HeroStatsOverview overview = HeroStatsPages_Overview(stats, columns, level);
  std::string out = "{\"cells\":[";
  for (size_t i = 0; i < overview.cells.size(); i++) out += (i > 0 ? "," : "") + CellJson(overview.cells[i]);
  int move[HERO_MOVE_COLUMN_COUNT];
  for (int c = 0; c < HERO_MOVE_COLUMN_COUNT; c++) move[c] = HeroStatsPages_MovementValue(overview.movement, c);
  out += "],\"move\":" + Numbers(move, HERO_MOVE_COLUMN_COUNT);
  std::vector<HeroStatsMonsterLine> monsters = HeroStatsPages_Monsters(stats, columns, level, names);
  out += ",\"monsters\":[";
  for (size_t i = 0; i < monsters.size(); i++) out += (i > 0 ? "," : "") + MonsterJson(monsters[i]);
  out += "],\"pages\":[";
  for (size_t i = 0; i <= columns.size(); i++)
  {
    HeroStatsChampion who = i < columns.size() ? columns[i].id : HERO_STATS_PARTY;
    out += (i > 0 ? "," : "") + PageJson(HeroStatsPages_Hero(stats, columns, who, level, names));
  }
  return out + "]}";
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
  out += "],\"levels\":" + Numbers(levels.data(), levels.size());
  out += ",\"wipes\":" + Number(stats.PartyWipes()) + ",\"reloads\":" + Number(stats.Reloads());
  out += ",\"views\":{\"-1\":" + ViewJson(stats, columns, names, HERO_STATS_WHOLE_RUN);
  for (size_t i = 0; i < levels.size(); i++)
    out += ",\"" + Number(levels[i]) + "\":" + ViewJson(stats, columns, names, levels[i]);
  return out + "}}";
}

// ---------------------------------------------------------------------------------------------
// The page

// The look of the Statistics window (the colours of HeroStatsWindow.cpp)
static const char *const PAGE_HEAD = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Statistics</title>
<style>
  :root { color-scheme: dark }
  body { background:#111216; color:#ccc; font:14px system-ui,sans-serif; margin:24px }
  h1 { font-size:18px; color:#e8c66a; margin:0 0 4px }
  .about { color:#999; margin-bottom:12px }
  .im { max-width:1000px; background:#1e1f24; color:#d8d8d8; font:12px/1.45 "DejaVu Sans Mono","Bitstream Vera Sans Mono",Menlo,Consolas,monospace; border:1px solid #3a3d46; border-radius:4px; overflow:hidden }
  .im .title { background:#294a7a; color:#fff; padding:3px 8px; font-weight:bold; display:flex }
  .im .title span:last-child { margin-left:auto; color:#9fb4d8; font-weight:normal }
  .bar { display:flex; gap:4px; align-items:center; padding:6px 8px; border-bottom:1px solid #33363e; flex-wrap:wrap }
  .btn { background:#2c3e5c; padding:1px 6px; border-radius:3px; cursor:pointer; user-select:none; color:#d8d8d8; border:0; font:inherit }
  .btn:hover { background:#36507a }
  .btn.on { background:#4272b8; color:#fff }
  .btn:disabled { opacity:.35; cursor:default }
  .btn.left { color:#888 }
  .sp { flex:1 }
  select { background:#2a2c33; color:#d8d8d8; border:1px solid #4a4d57; font:inherit; border-radius:3px }
  #view { max-height:640px; overflow:auto }
  .im table { border-collapse:collapse }
  .im td, .im th { padding:2px 8px; text-align:right; white-space:nowrap }
  .im th { color:#9fb4d8; font-weight:normal; border-bottom:1px solid #3a3d46; position:sticky; top:0; background:#1e1f24 }
  .im th.sort { cursor:pointer; user-select:none }
  .im th.sort:hover { color:#fff }
  .im th .arrow { color:#e8c66a }
  .im td:first-child, .im th:first-child { text-align:left }
  .im .hero { cursor:pointer; color:#e8c66a }
  .im .hero.left { color:#888 }
  .im .hero:hover { color:#fff }
  .im tr.grp td { color:#e8c66a; padding-top:8px }
  .im tr.sub td:first-child { padding-left:20px }
  .im tr.lv { cursor:pointer }
  .im tr.lv:hover td { background:#2a3550 }
  .im tr.all td { background:#2a2c33; color:#fff }
  .im tr.opt td { color:#aaa }
  .im tr.opt td:first-child { padding-left:28px }
  .im tr.wrow { cursor:pointer }
  .im tr.wrow:hover td { background:#262a33 }
  .dim { color:#888 } .bad { color:#e07a6a } .melee { color:#c9764a } .ranged { color:#5a9bd8 } .magic { color:#a77ad8 }
  .kills { color:#c9a24a } .healed { color:#4ab0a0 } .given { color:#e0c060 } .gen { color:#4a9a6a } .white { color:#fff }
  .body { padding:6px 8px }
  .h { color:#e8c66a; margin:10px 0 3px }
  .legend { color:#888; margin:2px 0 4px } .legend i { display:inline-block; width:10px; height:10px; margin:0 4px 0 12px; vertical-align:-1px }
  .legend i:first-child { margin-left:0 }
  .brow { display:flex; align-items:center; margin:3px 0 }
  .brow .n { width:100px; flex:none; overflow:hidden; text-overflow:ellipsis }
  .brow .v { margin-left:8px; color:#d8d8d8 }
  .seg { display:inline-block; height:10px }
  .share { display:flex; width:220px; height:10px; background:#2a2c33 }
  .sec { border-bottom:1px solid #2c2f37 }
  .sech { display:flex; align-items:baseline; gap:8px; padding:5px 2px; cursor:pointer; user-select:none }
  .sech:hover { background:#262a33 }
  .sech .st { color:#e8c66a; width:130px }
  .sech .ss { color:#aaa; margin-left:auto; padding-right:6px }
  .arr { color:#9fb4d8; display:inline-block; width:12px }
  .secb { padding:0 0 8px 14px }
  .vl { display:flex; max-width:420px } .vl span:last-child { margin-left:auto; padding-left:16px }
  .vl.in span:first-child { padding-left:14px; color:#888 }
  .hint { color:#777; padding:6px 0 }
  .empty { color:#888; padding:8px }
</style></head>
<body>
)HTML";

static const char *const PAGE_SCRIPT = R"HTML(
<script>
const D = JSON.parse(document.getElementById('stats').textContent);
const H = D.heroes, N = H.length;
const HERO_RGB = ['#c9764a', '#5aa86a', '#5a9bd8', '#a77ad8'];
const PAGES = ['Table', 'Bars', 'By level', 'Monsters'];
// By level's buttons and the cell each adds up; Movement shows the party's movement table instead
const METRICS = [['Damage dealt', 3], ['Kills', 4], ['Damage taken', 5], ['Healed', 7], ['Mana spent', 10], ['Movement', -1]];
const MOVES = ['Steps', 'Turns', 'Wall bumps', 'Teleports', 'Pit falls', 'Pit climbs', 'Time (ticks)', 'Asleep (ticks)'];
const st = {page: 0, level: -1, hero: -1, metric: 0, open: {0: true, 4: true}, folded: {}, sort: {}};

const esc = s => String(s).replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'})[c]);
const fmt = n => String(n).replace(/\B(?=(\d{3})+(?!\d))/g, D.separator);
const pct = (part, whole) => whole === 0 ? '-' : Math.round(part * 100 / whole) + '%';
const lvName = l => l === -1 ? 'Whole run' : 'Lv ' + l;
const heroName = i => i === N ? 'Party' : H[i].name;
const view = () => D.views[String(st.level)];
const btn = (label, on, act, extra) =>
  `<button class="btn${on ? ' on' : ''}${extra || ''}" onclick="${act}">${esc(label)}</button>`;
const heroLink = (i, tag) =>
  `<${tag} class="hero${i < N && H[i].left ? ' left' : ''}" onclick="openHero(${i})">${esc(heroName(i))}</${tag}>`;

function set(changes) { Object.assign(st, changes); draw(); }
function openHero(i) { set({hero: i}); }
function pickLevel(l) { set({level: l}); }

// The level picker: a button per level up to 16, else arrows and a list
function picker() {
  const L = D.levels;
  let out = btn('Whole run', st.level === -1, 'pickLevel(-1)');
  if (L.length <= 16) return out + L.map(l => btn(String(l), st.level === l, `pickLevel(${l})`)).join('');
  const at = L.indexOf(st.level);
  const prev = st.level === -1 ? L[L.length - 1] : at > 0 ? L[at - 1] : null;
  const next = st.level === -1 ? L[0] : at < L.length - 1 ? L[at + 1] : null;
  out += `<button class="btn" ${prev === null ? 'disabled' : `onclick="pickLevel(${prev})"`}>&#9664;</button>`;
  out += `<select onchange="pickLevel(+this.value)"><option value="-1">All ${L.length} levels</option>` +
    L.map(l => `<option value="${l}"${l === st.level ? ' selected' : ''}>Lv ${l}</option>`).join('') + '</select>';
  return out + `<button class="btn" ${next === null ? 'disabled' : `onclick="pickLevel(${next})"`}>&#9654;</button>`;
}

function topBar() {
  let left;
  if (st.hero < 0) left = PAGES.map((p, i) => btn(p, st.page === i, `set({page:${i}})`)).join('');
  else {
    left = btn('◀ Overview', false, 'set({hero:-1})');
    for (let i = 0; i <= N; i++) left += btn(heroName(i), st.hero === i, `openHero(${i})`, i < N && H[i].left ? ' left' : '');
  }
  const showPicker = st.hero >= 0 || st.page !== 2;
  return left + '<span class="sp"></span>' + (showPicker ? picker() : '');
}

// A table whose columns sort when their header is clicked. cols: [{label, html?, num, desc}],
// rows: [{cells: [{v, html}], cls, act, sub: [rows that stay under it]}], foot: one row kept last
function sortTable(id, cols, rows, foot, defaultCol, defaultDesc) {
  const s = st.sort[id] || {col: defaultCol, desc: defaultDesc};
  const key = r => r.cells[s.col].v;
  if (s.col !== null) rows = rows.slice().sort((a, b) => {
    const x = key(a), y = key(b);
    const c = typeof x === 'number' && typeof y === 'number' ? x - y : String(x).localeCompare(String(y));
    return s.desc ? -c : c;
  });
  const head = cols.map((c, i) => {
    const arrow = s.col === i ? `<span class="arrow">${s.desc ? ' ▼' : ' ▲'}</span>` : '';
    return `<th class="sort" onclick="sortBy('${id}',${i},${c.num ? 'true' : 'false'})">${c.html || esc(c.label)}${arrow}</th>`;
  }).join('');
  const line = r => `<tr class="${r.cls || ''}"${r.act ? ` onclick="${r.act}"` : ''}>` +
    r.cells.map(c => `<td>${c.html !== undefined ? c.html : esc(c.v)}</td>`).join('') + '</tr>';
  let body = '';
  for (const r of rows) {
    body += line(r);
    if (r.sub) {
      let sub = r.sub;
      if (s.col !== null) sub = sub.slice().sort((a, b) => {
        const x = key(a), y = key(b);
        const c = typeof x === 'number' && typeof y === 'number' ? x - y : String(x).localeCompare(String(y));
        return s.desc ? -c : c;
      });
      body += sub.map(line).join('');
    }
  }
  return `<table><thead><tr>${head}</tr></thead><tbody>${body}${foot ? line(foot) : ''}</tbody></table>`;
}
function sortBy(id, col, num) {
  const s = st.sort[id];
  st.sort[id] = s && s.col === col ? {col, desc: !s.desc} : {col, desc: num};
  draw();
}
const num = (v, cls) => ({v, html: cls ? `<span class="${cls}">${fmt(v)}</span>` : fmt(v)});
const dot = (v, dim) => ({v, html: v === 0 ? '<span class="dim">·</span>' : dim ? `<span class="dim">${fmt(v)}</span>` : fmt(v)});

function shareBar(parts, total) {
  if (total <= 0) return '<div class="share"></div>';
  return '<div class="share">' + parts.filter(p => p[0] > 0)
    .map(p => `<span class="seg" style="width:${(p[0] * 100 / total).toFixed(2)}%;background:${p[1]}"></span>`).join('') + '</div>';
}

// ---- Table
function drawTable() {
  const v = view(), cells = v.cells, party = cells[N];
  let out = '<table><tr><th></th>';
  for (let i = 0; i <= N; i++) out += heroLink(i, 'th');
  out += '</tr>';
  const group = t => `<tr class="grp"><td colspan="${N + 2}">${t}</td></tr>`;
  const row = (label, k, cls, bad, sub) => {
    let r = `<tr${sub ? ' class="sub"' : ''}><td>${label}</td>`;
    for (let i = 0; i <= N; i++) {
      const n = cells[i][k], left = i < N && H[i].left;
      const c = left ? 'dim' : bad && n > 0 ? 'bad' : cls;
      r += `<td${c ? ` class="${c}"` : ''}>${fmt(n)}</td>`;
    }
    return r + '</tr>';
  };
  const partyOnly = (label, n, bad) =>
    `<tr class="sub"><td>${label}</td>${'<td></td>'.repeat(N)}<td${bad && n > 0 ? ' class="bad"' : ''}>${fmt(n)}</td></tr>`;
  out += group('Damage dealt') + row('Melee', 0, 'melee', false, true) + row('Ranged', 1, 'ranged', false, true) +
    row('Magic', 2, 'magic', false, true) + row('Total', 3, '', false, false);
  out += '<tr class="sub"><td>share of party damage</td>';
  for (let i = 0; i <= N; i++) out += `<td class="dim">${pct(cells[i][3], party[3])}</td>`;
  out += '</tr>' + row('Kills', 4, 'kills', false, false);
  out += group('Damage taken') + row('Total', 5, '', false, false) + row('Friendly fire caused', 6, '', true, false);
  out += group('Healing') + row('Healed', 7, 'healed', false, true) + row('Healing given (potions brewed)', 8, 'given', false, true);
  out += group('Hardship') + row('Deaths', 12, '', true, true);
  if (st.level === -1) out += partyOnly('Party wiped out', D.wipes, true) + partyOnly('Reloads', D.reloads, false);
  out += group('Mana') + row('Generated', 9, 'gen', false, true) + row('Spent', 10, '', false, true) +
    row('Wasted on failures', 11, '', true, true);
  out += group('Movement') + MOVES.map((label, i) => partyOnly(label, v.move[i])).join('');
  return out + '</table>';
}

// ---- Bars
function drawBars() {
  const cells = view().cells;
  const bar = (i, parts, scale, text) => {
    const name = i === null ? '<span class="n"></span>' : heroLink(i, 'span').replace('class="hero', 'class="n hero');
    return `<div class="brow">${name}` + parts.filter(p => p[0] > 0)
      .map(p => `<span class="seg" style="width:${Math.max(2, Math.round(p[0] * scale))}px;background:${p[1]}"></span>`).join('') +
      `<span class="v">${text}</span></div>`;
  };
  const legend = items => '<div class="legend">' + items.map(x => `<i style="background:${x[0]}"></i>${x[1]}`).join('') + '</div>';
  const largest = f => Math.max(1, ...cells.slice(0, N).map(f));
  let out = '<div class="h">Damage dealt</div>' + legend([['#c9764a', 'melee'], ['#5a9bd8', 'ranged'], ['#a77ad8', 'magic']]);
  let scale = 300 / largest(c => c[3]);
  for (let i = 0; i < N; i++) out += bar(i, [[cells[i][0], '#c9764a'], [cells[i][1], '#5a9bd8'], [cells[i][2], '#a77ad8']], scale, fmt(cells[i][3]));
  out += '<div class="h">Kills</div>';
  scale = 220 / largest(c => c[4]);
  for (let i = 0; i < N; i++) out += bar(i, [[cells[i][4], '#c9a24a']], scale, fmt(cells[i][4]));
  out += '<div class="h">Damage taken</div>' + legend([['#888', 'taken'], ['#e07a6a', 'friendly fire caused']]);
  scale = 300 / largest(c => c[5] + c[6]);
  for (let i = 0; i < N; i++) out += bar(i, [[cells[i][5], '#888'], [cells[i][6], '#e07a6a']], scale, fmt(cells[i][5]));
  out += '<div class="h">Healing</div>' + legend([['#4ab0a0', 'healed'], ['#e0c060', 'healing given']]);
  scale = 300 / largest(c => Math.max(c[7], c[8]));
  for (let i = 0; i < N; i++) {
    out += bar(i, [[cells[i][7], '#4ab0a0']], scale, fmt(cells[i][7]));
    if (cells[i][8] > 0) out += bar(null, [[cells[i][8], '#e0c060']], scale, fmt(cells[i][8]) + ' given');
  }
  out += '<div class="h">Mana</div>' + legend([['#4a9a6a', 'generated'], ['#5a9bd8', 'spent'], ['#e07a6a', 'wasted']]);
  scale = 220 / largest(c => Math.max(c[9], c[10]));
  for (let i = 0; i < N; i++) {
    if (cells[i][9] + cells[i][10] <= 0) continue;
    out += bar(i, [[cells[i][9], '#4a9a6a']], scale, fmt(cells[i][9]));
    out += bar(null, [[Math.max(0, cells[i][10] - cells[i][11]), '#5a9bd8'], [cells[i][11], '#e07a6a']], scale,
      `${fmt(cells[i][10])} (${fmt(cells[i][11])} wasted)`);
  }
  return '<div class="body">' + out + '</div>';
}

// ---- By level
function drawByLevel() {
  const k = METRICS[st.metric][1];
  const buttons = METRICS.map((m, i) => btn(m[0], st.metric === i, `set({metric:${i}})`)).join('');
  if (k < 0) return '<div class="bar">' + buttons + '</div>' + drawMovement();
  let out = '<div class="bar">' + buttons +
    '<span class="legend" style="margin-left:20px">' + H.map((h, i) => `<i style="background:${HERO_RGB[i % 4]}"></i>${esc(h.name)}`).join('') + '</span></div>';
  const cols = [{label: 'Level', num: false}].concat(H.map((h, i) => ({label: h.name, num: true})),
    [{label: 'Party', num: true}, {label: 'Share', num: true}]);
  const line = (l, cls, act) => {
    const c = D.views[String(l)].cells;
    const cells = [{v: l, html: esc(lvName(l))}];
    for (let i = 0; i <= N; i++) cells.push(dot(c[i][k], i < N && H[i].left));
    const total = c[N][k];
    cells.push({v: total, html: shareBar(H.map((h, i) => [c[i][k], HERO_RGB[i % 4]]), total)});
    return {cells, cls, act};
  };
  const rows = D.levels.map(l => line(l, 'lv', `set({level:${l},page:0})`));
  return out + sortTable('bylevel' + st.metric, cols, rows, line(-1, 'all'), 0, false);
}

// ---- Monsters
function drawMonsters() {
  const lines = view().monsters;
  if (lines.length === 0) return '<div class="empty">No monsters killed yet.</div>';
  const all = ['All monsters', new Array(N).fill(0), 0, 0, 0, 0];
  for (const m of lines) {
    for (let i = 0; i < N; i++) all[1][i] += m[1][i];
    for (let e = 2; e < 6; e++) all[e] += m[e];
  }
  const extras = [['Door', 2, '#b0a070'], ['Pit', 3, '#7a7a7a'], ['Other', 4, '#555']].filter(e => all[e[1]] > 0);
  const cols = [{label: 'Monster', num: false}].concat(H.map(h => ({label: h.name, num: true})),
    extras.map(e => ({label: e[0], num: true})), [{label: 'All causes', num: true}, {label: 'Killed by', num: true}]);
  const row = (m, cls) => {
    const cells = [{v: m[0]}];
    const parts = [];
    for (let i = 0; i < N; i++) { cells.push(dot(m[1][i], H[i].left)); parts.push([m[1][i], HERO_RGB[i % 4]]); }
    for (const e of extras) cells.push(dot(m[e[1]]));
    for (const e of [['Door', 2, '#b0a070'], ['Pit', 3, '#7a7a7a'], ['Other', 4, '#555']]) parts.push([m[e[1]], e[2]]);
    cells.push(num(m[5], 'kills'));
    cells.push({v: m[5], html: shareBar(parts, m[5])});
    return {cells, cls};
  };
  const legend = '<div class="legend body">' + H.map((h, i) => `<i style="background:${HERO_RGB[i % 4]}"></i>${esc(h.name)}`)
    .concat(extras.map(e => `<i style="background:${e[2]}"></i>${e[0]}`)).join('') + ' &nbsp; (Other = clouds, traps and monsters)</div>';
  return sortTable('monsters', cols, lines.map(m => row(m)), row(all, 'all'), N + extras.length + 1, true) + legend;
}

// ---- By level's Movement
function drawMovement() {
  const cols = [{label: 'Level', num: false}].concat(MOVES.map(label => ({label, num: true})));
  const line = (l, cls, act) => {
    const m = D.views[String(l)].move;
    return {cells: [{v: l, html: esc(lvName(l))}].concat(m.map(n => dot(n))), cls, act};
  };
  return sortTable('movement', cols, D.levels.map(l => line(l, 'lv', `set({level:${l},page:0})`)), line(-1, 'all'), 0, false);
}

// ---- Hero and Party pages
function section(bit, title, summary, body, bad) {
  const open = !!st.open[bit];
  return `<div class="sec"><div class="sech" onclick="st.open[${bit}]=!st.open[${bit}];draw()">` +
    `<span class="arr">${open ? '▼' : '▶'}</span><span class="st">${title}</span>` +
    `<span class="ss${bad ? ' bad' : ''}">${esc(summary)}</span></div>` + (open ? `<div class="secb">${body}</div>` : '') + '</div>';
}
// A label and its value; cls colours the value, and the label too when labelToo
const vline = (label, value, cls, inner, labelToo) =>
  `<div class="vl${inner ? ' in' : ''}"><span${cls && labelToo ? ` class="${cls}"` : ''}>${esc(label)}</span>` +
  `<span${cls ? ` class="${cls}"` : ''}>${esc(value)}</span></div>`;

// Weapons are folded until opened, per hero and weapon name
function toggleWeapon(i) {
  const key = st.hero + ':' + view().pages[st.hero].weapons[i][0];
  st.folded[key] = !st.folded[key];
  draw();
}

function drawHero() {
  const p = view().pages[st.hero];
  let attacks;
  if (p.weapons.length === 0) attacks = '<div class="dim">No attacks yet.</div>';
  else {
    const cols = ['Weapon / option', 'Tries', 'Hits', 'Rate', 'Avg', 'Damage', 'Kills'].map((label, i) => ({label, num: i > 0}));
    // Tries adds the actions that cannot hit (War Cry ...); a line of those only has "-" for the
    // rest. Rate is hits per blow or missile, Avg the damage per hit.
    const none = {v: -1, html: '<span class="dim">-</span>'};
    const cells = (name, w) => {
      const first = [{v: name, html: name}, num(w[1] + w[5])];
      if (w[1] === 0 && w[5] > 0) return first.concat([none, none, none, none, none]);
      const avg = w[2] ? Math.round(w[3] / w[2]) : -1;
      return first.concat([num(w[2]), {v: w[1] ? w[2] / w[1] : -1, html: pct(w[2], w[1])},
        {v: avg, html: avg < 0 ? '-' : fmt(avg)}, num(w[3]), num(w[4])]);
    };
    const rows = p.weapons.map((w, i) => {
      const open = !!st.folded[st.hero + ':' + w[0]];
      const cs = cells(`<span class="arr">${open ? '▼' : '▶'}</span><span class="white">${esc(w[0])}</span>`, w);
      cs[0].v = w[0];
      return {cells: cs, cls: 'wrow', act: `toggleWeapon(${i})`,
        sub: open ? w[6].map(o => ({cells: cells(esc(o[0]), o).map((c, k) => k === 0 ? {v: o[0], html: c.html} : c), cls: 'opt'})) : []};
    });
    attacks = sortTable('attacks', cols, rows, null, 5, true);
  }
  let spells;
  if (p.spells.length === 0) spells = '<div class="dim">No spells cast.</div>';
  else {
    const cols = ['Spell', 'Casts', 'Successful', 'Failed', 'Mana', 'Wasted', 'Damage', 'Kills']
      .map((label, i) => ({label, num: i > 0}));
    const rows = p.spells.map(s => ({cells: [{v: s[0]}, num(s[1]), num(s[2]), num(s[3]), num(s[4]),
      num(s[5], s[5] > 0 ? 'bad' : ''), num(s[6]), num(s[7])]}));
    spells = sortTable('spells', cols, rows, null, 4, true);
  }
  const taken = p.taken.length === 0 ? '<div class="dim">No damage taken.</div>'
    : p.taken.map(t => vline(t[0], fmt(t[1]), t[2] ? 'bad' : '', false, true)).join('');
  let healing = vline('Healed', fmt(p.healed), 'healed') + p.healing.map(h => vline(h[0], fmt(h[1]), '', true)).join('');
  if (p.given) healing += vline('Healing given (potions brewed)', fmt(p.given), 'given');
  if (p.resurrected) healing += vline('Resurrected', p.resurrected + '×', '');
  const hurt = p.deaths.length > 0 || p.wipes > 0 || p.reloads > 0;
  let hardship = hurt ? '' : '<div class="dim">Never died.</div>';
  if (p.deaths.length) {
    hardship += vline(st.hero === N ? 'Champion deaths' : 'Died', p.deaths.length + '×', 'bad');
    hardship += p.deaths.map(d => vline(d, '', '', true)).join('');
  }
  if (p.wipes) hardship += vline('Party wiped out', p.wipes + '×', 'bad');
  if (p.reloads) hardship += vline('Reloads', p.reloads + '×', '');
  return '<div class="body">' + section(0, 'Attacks', p.sum[0], attacks) + section(1, 'Spells', p.sum[1], spells) +
    section(2, 'Damage taken', p.sum[2], taken) + section(3, 'Healing', p.sum[3], healing) +
    section(4, 'Hardship', p.sum[4], hardship, hurt) +
    '<div class="hint">Click a section or a weapon to fold it, a column header to sort.</div></div>';
}

function draw() {
  if (st.level !== -1 && !D.views[String(st.level)]) st.level = -1;
  document.getElementById('top').innerHTML = topBar();
  let where = st.hero >= 0 ? heroName(st.hero) + ' · ' : PAGES[st.page] + ' · ';
  where += st.hero < 0 && st.page === 2 ? 'all levels' : lvName(st.level);
  document.getElementById('where').textContent = where;
  let html;
  if (N === 0) html = '<div class="empty">No statistics yet.</div>';
  else if (st.hero >= 0) html = drawHero();
  else html = [drawTable, drawBars, drawByLevel, drawMonsters][st.page]();
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
  html += "  <div class=\"bar\" id=\"top\"></div>\n  <div id=\"view\"></div>\n</div>\n";
  html += "<script type=\"application/json\" id=\"stats\">" + HeroStatsExport_Json(stats, columns, names, info) +
          "</script>\n";
  // The line under the title, from the JSON so that names are escaped once
  html += "<script>{const d=JSON.parse(document.getElementById('stats').textContent);"
          "document.getElementById('about').textContent=[d.heroes.map(h=>h.name).join(', '),d.dungeon,d.date]"
          ".filter(s=>s).join(' \\u00b7 ');}</script>";
  html += PAGE_SCRIPT;
  return html;
}
