// ============================================================================================
// Panels share one look: dark plate, gold frame, 3x5 text
// ============================================================================================
function plate(x, y, w, h) { rect(x, y, w, h, '#111'); frame(x, y, w, h, '#ffc83a'); }
function inRect(mx, my, r) { return r && mx >= r[0] && mx < r[0] + r[2] && my >= r[1] && my < r[1] + r[3]; }

// ---------------- the bar ----------------
// As the window's AutoMapBar.cpp: one label per item, '|' starts a group, '^' a group close to the
// one before, '#' marks a caption; each look (map_bar_style) draws the groups its own way
const BAR_LOOKS = ['stone', 'segmented', 'cyan', 'brass', 'classic'];
let barLook = 'stone';
const BAR_P = 2;
function hex(n) { return '#' + n.toString(16).padStart(6, '0'); }
function barSpacing() {
  if (barLook === 'classic') return { item: BAR_P, group: 7 * BAR_P, near: BAR_P, cap: 0, frame: 0 };
  return { item: 1, group: 5 * BAR_P, near: 2 * BAR_P, cap: 4 * BAR_P, frame: barLook === 'stone' ? 2 : 1 };
}
const BAR_LONG = ['EXPLORED', 'LEARNED', 'FOLLOW', 'VISION', 'SPOILERS'], BAR_SHORT = ['EXPL', 'LRND', 'FOL', 'VIS', 'SPOIL'];
function packBar(items, width, shorten) {
  const sp = barSpacing(); let x = 3 * BAR_P + sp.frame; const out = [];
  items.forEach((it, i) => {
    let label = it.label; const group = label[0] === '|', near = label[0] === '^'; if (group || near) label = label.slice(1);
    const caption = label[0] === '#'; if (caption) label = label.slice(1);
    if (shorten) { const k = BAR_LONG.indexOf(label); if (k >= 0) label = BAR_SHORT[k]; }
    if (i > 0) x += group ? sp.group + 2 * sp.frame : near ? sp.near + 2 * sp.frame : sp.item;
    const tw = label ? (label.length * 4 - 1) * BAR_P : 0, w = caption ? tw + sp.cap : tw + 5 * BAR_P;
    out.push({ ...it, shown: label, caption, starts: group || near || i === 0, x, y: 2 * BAR_P, w, h: 9 * BAR_P }); x += w;
  });
  if (!shorten && out.length && out[out.length - 1].x + out[out.length - 1].w + sp.frame > width) return packBar(items, width, true);
  return out;
}
function barFill() { return { stone: 0x6a6d6a, cyan: 0x000000, brass: 0x1f1c18 }[barLook] ?? 0x24221f; }
function barRect(x, y, w, h, n) { tg.fillStyle = hex(n); tg.fillRect(x, y, w, h); }
function bevel(r, light, dark) { const t = Math.max(1, BAR_P / 2); barRect(r.x, r.y, r.w, t, light); barRect(r.x, r.y, t, r.h, light); barRect(r.x, r.y + r.h - t, r.w, t, dark); barRect(r.x + r.w - t, r.y, t, r.h, dark); }
function drawGroupBack(r) {
  const g1 = (b) => ({ x: r.x - b, y: r.y - b, w: r.w + 2 * b, h: r.h + 2 * b });
  if (barLook === 'stone') { const w = g1(2); barRect(w.x, w.y, w.w, w.h, 0x949194); barRect(w.x, w.y, w.w - 1, w.h - 1, 0x4a484a); const f = g1(1); barRect(f.x, f.y, f.w, f.h, 0x000000); }
  else if (barLook === 'segmented' || barLook === 'cyan' || barLook === 'brass') { const f = g1(1); barRect(f.x, f.y, f.w, f.h, { segmented: 0x5d5548, cyan: 0x00dade, brass: 0x000000 }[barLook]); }
}
function drawCaption(r) {
  const y = r.y + 2 * BAR_P, l = r.shown;
  if (barLook === 'stone') return text(tg, r.x + 2 * BAR_P, y, l, hex(0x949194), BAR_P);
  if (barLook === 'classic') return text(tg, r.x, y, l, '#8f8676', BAR_P);
  const [fill, col] = { segmented: [0x3a352d, 0xb3a78f], cyan: [0x00494a, 0x7fecee], brass: [0x2b2824, 0xa8a39a] }[barLook];
  barRect(r.x, r.y, r.w, r.h, fill); text(tg, r.x + 2 * BAR_P, y, l, hex(col), BAR_P);
}
function drawButton(r) {
  const on = r.on; let col;
  if (barLook === 'stone') { barRect(r.x, r.y, r.w, r.h, on ? 0xffb500 : 0x944800); if (on) bevel(r, 0xa86e00, 0xffe08a); else bevel(r, 0xde916a, 0x6a2400); col = on ? 0x5a1e00 : 0xffb500; }
  else if (barLook === 'segmented') { barRect(r.x, r.y, r.w, r.h, on ? 0xffcc33 : 0x1d1a16); col = on ? 0x1a160f : 0xd8cfbd; }
  else if (barLook === 'cyan') { barRect(r.x, r.y, r.w, r.h, on ? 0x00dade : 0x000000); col = on ? 0x000000 : 0x00dade; }
  else if (barLook === 'brass') { barRect(r.x, r.y, r.w, r.h, on ? 0xffb500 : 0x6e3600); barRect(r.x, r.y, r.w, 1, on ? 0xffe08a : 0xa8603a); col = on ? 0x4a1a00 : 0xffb500; }
  else { barRect(r.x, r.y, r.w, r.h, on ? 0xffcc33 : 0x1d1a16); tg.fillStyle = hex(on ? 0xffcc33 : 0x4a443a); tg.fillRect(r.x, r.y, r.w, 1); tg.fillRect(r.x, r.y + r.h - 1, r.w, 1); tg.fillRect(r.x, r.y, 1, r.h); tg.fillRect(r.x + r.w - 1, r.y, 1, r.h); col = on ? 0x1a160f : 0xd8cfbd; }
  text(tg, r.x + 5 * BAR_P / 2, r.y + 2 * BAR_P, r.shown, hex(col), BAR_P);
  // A choice the page may not show (beyond the export's limits) is veiled and takes no click
  if (r.grey) { tg.fillStyle = 'rgba(17,17,17,.62)'; tg.fillRect(r.x, r.y, r.w, r.h); }
}
let levelBtnX = 0, showBtnX = 0, searchBtnX = 0, debugBtnX = 0;
// A bar wider than the window scrolls: arrows at the clipped ends, the wheel over the bar, as in the game
let barScroll = 0, barLeft = false, barRight = false;
const barArrowW = () => 9 * BAR_P;
function drawBar() {
  barHits = [];
  const items = [];
  const add = (label, on, act, grey) => items.push({ label, on: !!on, act, grey: !!grey });
  add('<', false, () => stepLevel(-1));
  add('L' + lvl + ' ~', levelOpen, () => { const o = !levelOpen; closePanels(); levelOpen = o; levelScroll = Math.max(0, lvl - 4); typed = ''; });
  add('>', false, () => stepLevel(1));
  add('|FOLLOW', follow, () => { if (follow) follow = false; else followParty(); }, !hasLevel(DATA.index.partyLevel));
  add('|#VISION');
  ['FOG', 'EXPLORED', 'ALL'].forEach((n, i) => add(n, vision === i, () => { vision = i; refreshAll(); }, !visionAllowed(i)));
  if (vision === 2) add('^TINT', tint, () => tint = !tint);
  add('^WALKED', walked, () => walked = !walked);
  add('|#SPOILERS');
  [['SEEN', 2], ['LEARNED', 1], ['ALL', 0]].forEach(([n, v]) => add(n, spoil === v, () => { spoil = v; refreshAll(); }, !spoilAllowed(v)));
  const hidden = Object.values(SHOW).filter(v => !v).length;
  add(hidden ? '|SHOW: ' + hidden + ' HIDDEN' : '|SHOW', showOpen, () => { const o = !showOpen; closePanels(); showOpen = o; });
  add('|/', searchOpen, () => { if (searchOpen) closeSearch(); else openSearch(); });
  add('?', helpOpen, () => { const o = !helpOpen; closePanels(); helpOpen = o; });
  if (debugOn) add('+', debugOpen, () => { const o = !debugOpen; closePanels(); debugOpen = o; });
  const rects = packBar(items, tbar.width, false);
  const reach = rects.length ? rects[rects.length - 1].x + rects[rects.length - 1].w + barSpacing().frame + 3 * BAR_P : 0;
  barScroll = Math.max(0, Math.min(barScroll, reach - tbar.width)); barLeft = barScroll > 0; barRight = reach - barScroll > tbar.width;
  rects.forEach(r => { r.x -= barScroll; });
  const h = 26; barRect(0, 0, tbar.width, h, barFill());
  if (barLook === 'stone') { barRect(0, 0, tbar.width, 1, 0x949194); barRect(0, h - 2, tbar.width, 1, 0x4a484a); barRect(0, h - 1, tbar.width, 1, 0x000000); }
  else barRect(0, h - 1, tbar.width, 1, 0x3a362f);
  // The groups' frames first: they show between the cells as their dividers
  for (let i = 0; i < rects.length;) { let e = i + 1; while (e < rects.length && !rects[e].starts) e++; const a = rects[i], b = rects[e - 1]; drawGroupBack({ x: a.x, y: a.y, w: b.x + b.w - a.x, h: a.h }); i = e; }
  for (const r of rects) {
    if (r.caption) { drawCaption(r); continue; }
    drawButton(r);
    if (r.act && !r.grey) barHits.push([r.x, r.w, r.act]);
    if (r.label === '|SHOW' || r.label.startsWith('|SHOW:')) showBtnX = r.x;
    if (r.label.endsWith(' ~')) levelBtnX = r.x;
    if (r.label === '|/') searchBtnX = r.x;
    if (r.label === '+') debugBtnX = r.x;
  }
  // The arrows at the clipped ends
  const aw = barArrowW();
  if (barLeft) { barRect(0, 1, aw, h - 3, barFill()); text(tg, 3 * BAR_P, 4 * BAR_P, '<', '#e0b64a', BAR_P); }
  if (barRight) { barRect(tbar.width - aw, 1, aw, h - 3, barFill()); text(tg, tbar.width - aw + 3 * BAR_P, 4 * BAR_P, '>', '#e0b64a', BAR_P); }
}
function closePanels() { showOpen = helpOpen = levelOpen = debugOpen = false; menu = null; }
function refreshAll() { refreshView(); buildIndex(); if (query) rebuild(true); }
function stepLevel(d) { let l = lvl + d; while (l >= 0 && l < DATA.index.levels.length && !hasLevel(l)) l += d; if (l < 0 || l >= DATA.index.levels.length) return; setLevel(l); follow = l === DATA.index.partyLevel; }

// ---------------- SHOW ----------------
function drawShowPanel() {
  panelRects.show = null; if (!showOpen) return; const p = 2, rows = ['SHOW ALL', 'HIDE ALL', null, ...ITEM_KINDS, null, 'MONSTERS', null, 'WALL DECORATIONS', 'WALL WRITING', 'FLOOR DECORATIONS', null, 'PITS'];
  const w = textW('X FLOOR DECORATIONS', p) + 24, h = rows.reduce((n, r) => n + (r ? 16 : 6), 0) + 8; const x = Math.min(showBtnX, cv.width - w - 4), y = 2;
  plate(x, y, w, h); let yy = y + 5; const hits = [];
  for (const r of rows) { if (!r) { rect(x + 6, yy + 2, w - 12, 1, '#3a362f'); yy += 6; continue; }
    const box = r === 'SHOW ALL' || r === 'HIDE ALL' || r === 'PITS' ? null : SHOW[r]; if (box !== null) { frame(x + 8, yy + 2, 10, 10, '#8f8676'); if (box) rect(x + 10, yy + 4, 6, 6, '#ffcc33'); }
    text(g, x + (r === 'PITS' ? 8 : 24), yy + 3, r === 'PITS' ? 'PITS: ' + PIT_STYLES[pitStyle] : r, box === false ? '#8f8676' : '#fff', p); hits.push([x, yy, w, 16, r]); yy += 16; }
  panelRects.show = [x, y, w, h, hits];
}
function showClick(mx, my) { const P = panelRects.show; if (!P) return false; if (!inRect(mx, my, P)) { showOpen = false; return true; } for (const [x, y, w, h, r] of P[4]) if (inRect(mx, my, [x, y, w, h])) { if (r === 'SHOW ALL') Object.keys(SHOW).forEach(k => SHOW[k] = true); else if (r === 'HIDE ALL') Object.keys(SHOW).forEach(k => SHOW[k] = false); else if (r === 'PITS') { pitStyle = (pitStyle + 1) % 3; try { localStorage.setItem('csb-map-pit-style', String(pitStyle)); } catch (e) { } } else SHOW[r] = !SHOW[r]; } return true; }

// ---------------- controls help ----------------
const HELP = [['LEFT-DRAG', 'PAN THE MAP'], ['WHEEL', 'ZOOM TOWARDS THE POINTER'], ['MIDDLE-CLICK', 'CENTRE ON THE PARTY AND FOLLOW'], ['SIDE BUTTONS', 'LEVEL DOWN / UP UNDER THE POINTER'], ['PAGE DOWN / UP', 'LEVEL DOWN / UP (CTRL IN THE GAME)'], ['/ OR CTRL+F', 'SEARCH ALL LEVELS'], ['HOVER', 'WHAT IS THERE, AND ITS LINKS'], ['SHIFT', 'THE WHOLE PRESS CHAIN (SPOILERS ALL)'], ['SHIFT+WHEEL', 'SCROLL A LONG TOOLTIP'], ['CTRL+WHEEL', 'SIZE OF THE TOOLTIP'], ['CTRL+CLICK', 'ON A STAIR: COME OUT ON THE OTHER LEVEL'], ['RIGHT-CLICK', 'MARKS, LINKS, ASCEND / DESCEND'], ['', ''], ['', 'CLICK ANYWHERE TO CLOSE']];
function drawHelp() { if (!helpOpen) return; const widest = (i, q) => Math.max(...HELP.map(r => textW(r[i], q))); const fit = q => widest(0, q) + widest(1, q) + 40; const p = fit(2) <= cv.width - 8 ? 2 : 1, c1 = widest(0, p) + 20, w = Math.min(c1 + widest(1, p) + 20, cv.width - 4), h = HELP.length * 16 + 30; const x = Math.max(2, Math.round((cv.width - w) / 2)), y = 40; plate(x, y, w, h); text(g, x + 10, y + 8, 'CONTROLS', '#ffcc33', p); HELP.forEach((r, i) => { text(g, x + 10, y + 26 + i * 16, r[0], '#ffcc33', p); text(g, x + 10 + c1, y + 26 + i * 16, r[1], '#fff', p); }); }

// ---------------- level list ----------------
let levelScroll = 0, typed = '', levelRows = [], levelHover = -1; const ROWS_SHOWN = 12;
function drawLevels() {
  levelRows = []; panelRects.levels = null; if (!levelOpen) return; const n = DATA.index.levels.length, p = 2, rh = 16, w = textW('L00  NOT EXPORTED  00 HITS', p) + 30, rows = Math.min(ROWS_SHOWN, n), h = rows * rh + 10 + (typed ? 18 : 0);
  const x = Math.max(4, levelBtnX - 4), y = 2; plate(x, y, w, h); let yy = y + 5;
  if (typed) { text(g, x + 8, yy + 2, 'GO TO L' + typed, '#ffcc33', p); yy += 18; }
  for (let i = 0; i < rows; i++) { const l = levelScroll + i; if (l >= n) break; const sel = l === lvl, vis = DATA.index.levels[l].visited;
    if (sel) rect(x + 3, yy, w - 14, rh, '#ffcc33'); else if (l === levelHover) rect(x + 3, yy, w - 14, rh, '#2c2822');
    const gone = !hasLevel(l); text(g, x + 8, yy + 3, 'L' + l, sel ? '#1a160f' : vis && !gone ? '#fff' : '#6f685c', p); if (l === DATA.index.partyLevel) partyArrow(x + 8 + textW('L00', p) + 10, yy + rh / 2, 5, 1, '#ff4d4d');
    let tx = x + 8 + textW('L00', p) + 22; if (gone) { text(g, tx, yy + 3, 'NOT EXPORTED', '#6f685c', p); tx += textW('NOT EXPORTED', p) + 10; } else if (!vis) { text(g, tx, yy + 3, 'NOT VISITED', sel ? '#1a160f' : '#6f685c', p); tx += textW('NOT VISITED', p) + 10; }
    if (searchOpen && query && hitsByLevel[l]) text(g, tx, yy + 3, hitsByLevel[l] + (hitsByLevel[l] > 1 ? ' HITS' : ' HIT'), sel ? '#1a160f' : '#ffcc33', p);
    if (!gone) levelRows.push([x, yy, w - 12, rh, l]); yy += rh; }
  if (n > ROWS_SHOWN) { const th = Math.max(16, (ROWS_SHOWN / n) * (h - 10)), ty = y + 5 + (levelScroll / (n - ROWS_SHOWN)) * (h - 10 - th); rect(x + w - 8, y + 5, 4, h - 10, '#2c2822'); rect(x + w - 8, ty, 4, th, '#8f8676'); }
  panelRects.levels = [x, y, w, h];
}
function pickLevel(l) { if (!hasLevel(l)) return; setLevel(l); follow = l === DATA.index.partyLevel; levelOpen = false; typed = ''; }

// ---------------- search: over the draw lists of every level, so it keeps the fog ----------------
let query = '', sRows = [], sSel = -1, sTop = 0, folded = new Set(), hitsByLevel = {}, sRects = [], sHover = -1, INDEX = [];
const S_ROWS = 16, S_RH = 20;
function buildIndex() {
  INDEX = []; if (!DATA) return;
  DATA.index.levels.forEach((L, l) => {
    const v = viewOf(l); if (!v) return;
    // food: the item is of the food kind (index 6 of KIND_SHOW), which the word FOOD finds as in the game
    const add = (x, y, name, tag, icon, kind, food) => INDEX.push({ l, x, y, gx: x + L.offsetX, gy: y + L.offsetY, name: (name || '').toUpperCase(), tag, icon, kind, food: !!food });
    const FOOD_KIND = KIND_SHOW.indexOf('FOOD');
    const monsterAt = {}; v.monsters.forEach(m => monsterAt[m.x + ',' + m.y] = m);
    for (const c of v.cells) {
      // A stack of more than one says how many, as the game's tags: X2 ALCOVE, X3 IN CHEST
      const counted = (n, tag) => n > 1 ? 'X' + n + ' ' + tag : tag;
      const h = v.hover[c.x + ',' + c.y], m = monsterAt[c.x + ',' + c.y];
      if (h && h.icons) {
        // As AutoMapScene_SearchItems: the tooltip's stacks (the floor shows at most three pictures),
        // and the stacks of a cell that read the same are one row, their counts added: three
        // opened chests are CHEST X3. What a chest holds or a monster carries follows its stack.
        const rows = {};
        h.icons.forEach(([it, n, nested, predicted, required, carried, side]) => {
          if (!it[2] || predicted || required) return;
          const tag = nested ? (m ? 'CARRIED BY ' + nice(m.name) : side >= 0 ? 'ALCOVE' : 'IN CHEST') : side >= 0 ? 'ALCOVE' : '';
          const key = it[2] + '|' + tag; (rows[key] = rows[key] || { n: 0, it, tag }).n += n;
        });
        for (const key in rows) { const { n, it, tag } = rows[key]; add(c.x, c.y, it[2], tag ? counted(n, tag) : n > 1 ? 'X' + n : '', it[1] || it[0], 'item', it[3] === FOOD_KIND); }
      } else {
        if (c.items) { const by = {}; c.items.forEach(i => { if (!i[2]) return; (by[i[2]] = by[i[2]] || { n: 0, icon: i[1] || i[0], food: i[3] === FOOD_KIND }).n++; }); for (const n in by) add(c.x, c.y, n, by[n].n > 1 ? 'X' + by[n].n : '', by[n].icon, 'item', by[n].food); }
        const inAlcove = {}; (c.alc || []).forEach(a => { if (!a[1][2]) return; const key = a[0] + '|' + a[1][2]; (inAlcove[key] = inAlcove[key] || { n: 0, it: a[1] }).n++; });
        for (const key in inAlcove) { const { n, it } = inAlcove[key]; add(c.x, c.y, it[2], counted(n, 'ALCOVE'), it[1] || it[0], 'item', it[3] === FOOD_KIND); }
      }
      (c.locks || []).forEach(k => { if (k[1] && k[2][2]) add(c.x, c.y, k[2][2], 'NEEDS', k[2][1] || k[2][0], 'needs', k[2][3] === FOOD_KIND); });
      (c.sides || []).forEach((s, i) => { if (s && s[7] === 1) add(c.x, c.y, 'FOUNTAIN', 'NESW'[i], s[0], 'feature'); if (s && s[7] === 2) add(c.x, c.y, 'ALTAR OF VI', 'NESW'[i], s[0], 'feature'); });
      if (c.k === 3 && c.st) { const up = !!c.st[0]; add(c.x, c.y, up ? 'STAIRS UP' : 'STAIRS DOWN', 'TO L' + (l + (up ? -1 : 1)), null, up ? 'stairs-up' : 'stairs-down'); }
    }
    const seen = new Set();
    v.acts.forEach(a => { const k = a[0] + ',' + a[1] + ',' + a[2] + ',' + a[3]; if (seen.has(k)) return; seen.add(k);
      if ((a[3] === 4 || a[3] === 6) && a[2] === 0 && a[5][2]) add(a[0], a[1], a[5][2], a[3] === 6 ? 'PAD, CARRIED' : 'PAD NEEDS', a[5][1] || a[5][0], 'needs', a[5][3] === FOOD_KIND);
      if (a[2] === 2 && a[7]) add(a[0], a[1], nice(a[7]), 'GENERATOR', a[6], 'gen'); });
    v.monsters.forEach(m => add(m.x, m.y, nice(m.name) || 'MONSTER', m.m.length > 1 ? 'X' + m.m.length : '', m.m[0] && m.m[0][0], 'monster'));
  });
}
// As AutoMapScene_SearchHitRank: the name, the word FOOD for anything edible, GEN, MON
function matches(h, q) { if (!q) return false; if (h.name.includes(q)) return true; if (h.food && 'FOOD'.includes(q)) return true; if (('GENERATOR'.startsWith(q) || q === 'GEN') && h.kind === 'gen') return true; if ('MONSTER'.startsWith(q) && q.length >= 3 && h.kind === 'monster') return true; return false; }
function rebuild(keepSel) {
  const q = query.trim().toUpperCase(); const old = sRows[sSel] && sRows[sSel].hit; const hs = INDEX.filter(h => matches(h, q));
  hitsByLevel = {}; hs.forEach(h => hitsByLevel[h.l] = (hitsByLevel[h.l] || 0) + 1);
  const order = Object.keys(hitsByLevel).map(Number).sort((a, b) => (a === lvl ? -1 : b === lvl ? 1 : a - b));
  sRows = []; for (const l of order) { sRows.push({ head: true, l, n: hitsByLevel[l] }); if (folded.has(l)) continue; hs.filter(h => h.l === l).sort((a, b) => a.y - b.y || a.x - b.x).forEach(h => sRows.push({ hit: h })); }
  if (keepSel && old) { const i = sRows.findIndex(r => r.hit && r.hit.l === old.l && r.hit.x === old.x && r.hit.y === old.y && r.hit.name === old.name); if (i >= 0) { sSel = i; return; } }
  sSel = sRows.findIndex(r => r.hit); sTop = 0;
}
function openSearch() { closePanels(); searchOpen = true; query = ''; sRows = []; sSel = -1; sTop = 0; hitsByLevel = {}; }
function closeSearch() { searchOpen = false; query = ''; sRows = []; hitsByLevel = {}; }
function goTo(i) { const r = sRows[i]; if (!r || !r.hit) return; sSel = i; const h = r.hit; if (h.l !== lvl) { setLevel(h.l); follow = h.l === DATA.index.partyLevel; rebuild(true); } centreOn(h.x, h.y); keepVisible(); }
function keepVisible() { if (sSel < sTop) sTop = sSel; if (sSel >= sTop + S_ROWS) sTop = sSel - S_ROWS + 1; sTop = Math.max(0, Math.min(sTop, Math.max(0, sRows.length - S_ROWS))); }
function step(d) { let i = sSel; do { i += d; } while (i >= 0 && i < sRows.length && !sRows[i].hit); if (i >= 0 && i < sRows.length) { sSel = i; keepVisible(); const h = sRows[i].hit; if (h.l === lvl) centreOn(h.x, h.y); } }
function drawSearchPanel() {
  sRects = []; panelRects.search = null; if (!searchOpen) return; const p = 2, w = 440; const x = Math.max(4, Math.min(searchBtnX - 220, cv.width - w - 4)), y = 2;
  const rowsShown = Math.min(S_ROWS, sRows.length), total = sRows.filter(r => r.hit).length, lv = Object.keys(hitsByLevel).length; const h = 30 + Math.max(1, rowsShown) * S_RH + 6;
  plate(x, y, w, h); text(g, x + 8, y + 8, 'FIND ' + query + ((Date.now() / 500 | 0) % 2 ? '_' : ' '), '#fff', p);
  if (query && total) { const info = `${total} ON ${lv} ${lv > 1 ? 'LEVELS' : 'LEVEL'}`; text(g, x + w - 12 - textW(info, p), y + 8, info, '#8f8676', p); }
  rect(x + 4, y + 24, w - 8, 1, '#3a362f'); let yy = y + 28;
  if (query && !total) text(g, x + 8, yy + 6, 'NOTHING SEEN MATCHES', '#8f8676', p);
  if (!query) text(g, x + 8, yy + 6, 'TYPE A NAME: ITEM, MONSTER, GEN', '#6f685c', p);
  for (let i = sTop; i < Math.min(sRows.length, sTop + S_ROWS); i++) { const r = sRows[i];
    if (r.head) { const f = folded.has(r.l); rect(x + 4, yy + 1, w - 16, S_RH - 2, '#1d1a16'); text(g, x + 10, yy + 5, (f ? '+ ' : '- ') + 'L' + r.l, '#ffcc33', p); text(g, x + 10 + textW('- L00', p) + 12, yy + 5, r.n + (r.n > 1 ? ' HITS' : ' HIT'), '#8f8676', p); if (r.l === lvl) text(g, x + w - 16 - textW('SHOWN', p), yy + 5, 'SHOWN', '#8f8676', p); if (r.l === DATA.index.partyLevel) partyArrow(x + w - 16 - textW('SHOWN', p) - 14, yy + S_RH / 2, 5, 1, '#ff4d4d'); }
    else { const hh = r.hit, sel = i === sSel; if (sel) rect(x + 4, yy, w - 16, S_RH, '#ffcc33'); else if (i === sHover) rect(x + 4, yy, w - 16, S_RH, '#2c2822');
      if (hh.kind === 'needs') { rect(x + 21, yy + 1, 18, 18, '#0c0b09'); frame(x + 21, yy + 1, 18, 18, '#e0b64a'); } if (hh.kind.startsWith('stairs')) { const L = hh.kind === 'stairs-up' ? ['#9a9488', '#5e594f'] : ['#6d685f', '#3a3731']; for (let k = 0; k < 4; k++) rect(x + 22, yy + 2 + 4 * k, 16, 4, L[k % 2]); } else spr(hh.icon, x + 22, yy + 2, 16, 16); if (hh.kind === 'gen') { rect(x + 31, yy + 11, 9, 8, '#0d0c0a'); text(g, x + 33, yy + 12, 'G', '#ff8080', 1); }
      text(g, x + 46, yy + 5, `(${hh.gx},${hh.gy})`, sel ? '#1a160f' : '#b8b0a0', p); text(g, x + 46 + textW('(00,00)', p) + 10, yy + 5, hh.name, sel ? '#1a160f' : '#fff', p);
      if (hh.tag) { const tc = sel ? '#1a160f' : hh.kind === 'needs' ? '#e0b64a' : hh.kind === 'gen' ? '#ff8080' : '#8f8676'; text(g, x + w - 16 - textW(hh.tag, p), yy + 5, hh.tag, tc, p); } }
    sRects.push([x, yy, w - 12, S_RH, i]); yy += S_RH; }
  if (sRows.length > S_ROWS) { const tr = h - 34, th = Math.max(14, tr * S_ROWS / sRows.length), ty = y + 28 + (tr - th) * sTop / (sRows.length - S_ROWS); rect(x + w - 8, y + 28, 4, tr, '#2c2822'); rect(x + w - 8, ty, 4, th, '#8f8676'); }
  panelRects.search = [x, y, w, h];
}
function drawSearchMarks() {
  if (!searchOpen || !query) return; const sel = sRows[sSel] && sRows[sSel].hit; const mine = sRows.filter(r => r.hit && r.hit.l === lvl).map(r => r.hit);
  if (sel && mine.length) spotlight(mine.map(h => h.x + ',' + h.y));
  // Several hits can share a cell; the selected one is drawn last so its light frame stays on top
  for (const h of [...mine.filter(h => h !== sel), ...mine.filter(h => h === sel)]) { const x = px(h.x), y = py(h.y), t = Math.max(2, Math.round(cs / 14)); frame(x + 1, y + 1, cs - 2, cs - 2, '#0b0a08', t + 2); frame(x + 2, y + 2, cs - 4, cs - 4, h === sel ? '#fff6dc' : '#ffcc33', t); }
}
function searchClick(mx, my) { const P = panelRects.search; if (!P || !inRect(mx, my, P)) return false; for (const [rx, ry, rw, rh, i] of sRects) if (inRect(mx, my, [rx, ry, rw, rh])) { const r = sRows[i]; if (r.head) { folded.has(r.l) ? folded.delete(r.l) : folded.add(r.l); rebuild(true); keepVisible(); } else goTo(i); } return true; }

// ---------------- ascend / descend: come out of a stair on the level it leads to ----------------
// The stair the plate names (lvl + 1 or - 1), when the page has that level and the cell is a stair.
function stairTarget(c) { if (!c || c.k !== 3) return -1; const t = lvl + (c.st[0] ? -1 : 1); return t >= 0 && t < DATA.index.levels.length && hasLevel(t) ? t : -1; }
// The dungeon position stays; the level it lands on is framed when the page shows that cell as a stair
function followStairs(cx, cy) {
  const t = stairTarget(C(cx, cy)); if (t < 0) return false;
  const gx = cx + OX, gy = cy + OY; setLevel(t); follow = t === DATA.index.partyLevel;
  centreOn(gx - OX, gy - OY); stairMark = { l: t, x: gx, y: gy }; return true;
}
function drawStairMark() {
  if (!stairMark || stairMark.l !== lvl) return; const x = stairMark.x - OX, y = stairMark.y - OY, c = C(x, y); if (!c || c.k !== 3) return;
  spotlight([x + ',' + y]); const t = Math.max(2, Math.round(cs / 14)); frame(px(x) + 1, py(y) + 1, cs - 2, cs - 2, '#0b0a08', t + 2); frame(px(x) + 2, py(y) + 2, cs - 4, cs - 4, '#fff6dc', t);
}
// ---------------- right-click menu: your notes, and with map_debug the testing aids ----------------
const NESW = 'NESW';
function nearestSide(mx, my, x0, y0) { const ox = mx - px(x0) - cs / 2, oy = my - py(y0) - cs / 2; return Math.abs(ox) > Math.abs(oy) ? (ox > 0 ? 1 : 3) : (oy > 0 ? 2 : 0); }
function openMenu(mx, my, cx, cy) {
  const c = C(cx, cy); const rows = []; const key = cx + ',' + cy, N = noteOf();
  // the testing aids first, then the player's own notes
  if (debugOn) {
    // any tile of the level, explored or not: what lies there comes from the whole level
    const whole = DATA.views[`L${lvl}_v2_s0`].cells.find(o => o.x === cx && o.y === cy);
    const stand = whole && whole.k !== 0 && whole.k !== 6;
    if (!stand) rows.push({ label: 'NOT A FLOOR', title: true });
    rows.push({ label: 'TELEPORT HERE', grey: !stand, act: () => { partyOverride = { l: lvl, x: cx, y: cy, f: 2 }; } });
    DATA.index.levels[lvl].types.forEach(t => rows.push({ label: 'SPAWN ' + nice(t), grey: !stand, act: () => spawnSteps(nice(t), mx, my, cx, cy) }));
  }
  const notesRows = [];
  if (stairTarget(c) >= 0) { if (rows.length) rows.push(null); rows.push({ label: c.st[0] ? 'ASCEND' : 'DESCEND', act: () => { followStairs(cx, cy); } }); }
  if (c && c.k === 1) { notesRows.push({ label: 'HIDDEN PIT (OPEN)', act: () => N.pits[key] = 1 }); notesRows.push({ label: 'HIDDEN PIT (CLOSED)', act: () => N.pits[key] = 0 }); }
  if (c && (c.k === 1 || c.k === 0)) { const side = c.k === 0 ? nearestSide(mx, my, cx, cy) : -1; notesRows.push({ label: 'LINK FROM HERE' + (side >= 0 ? ' (' + NESW[side] + ')' : ''), act: () => linkMode = { x: cx, y: cy, side } }); }
  if (key in N.pits) notesRows.push({ label: 'REMOVE MARK', act: () => delete N.pits[key] });
  if (N.links.some(l => (l[0] === cx && l[1] === cy) || (l[2] === cx && l[3] === cy))) notesRows.push({ label: 'REMOVE LINKS', act: () => N.links = N.links.filter(l => !((l[0] === cx && l[1] === cy) || (l[2] === cx && l[3] === cy))) });
  if (rows.length && notesRows.length) rows.push(null);
  rows.push(...notesRows);
  if (!rows.length) return;
  menu = { x: mx, y: my, title: `L${lvl} (${cx + OX}, ${cy + OY})`, rows };
}
function spawnSteps(name, mx, my, cx, cy) {
  const ask = (title, opts, next) => { menu = { x: mx, y: my, title, rows: opts.map(o => ({ label: o, act: () => next(o) })) }; };
  ask('HOW MANY ' + name + '?', ['1', '2', '3', '4'], n => ask('WHERE?', ['ANY', 'NW', 'NE', 'SW', 'SE'], () => ask('FACING?', ['ANY', 'NORTH', 'EAST', 'SOUTH', 'WEST'], () => { menu = null; })));
  return 'keep';
}
function drawMenu() {
  panelRects.menu = null; if (!menu) return; const p = 2, rh = 16; const rows = menu.rows; const w = Math.max(textW(menu.title, p), ...rows.filter(Boolean).map(r => textW(r.label, p))) + 20; const h = 26 + rows.reduce((n, r) => n + (r ? rh : 6), 0) + 4;
  const x = Math.round(Math.max(4, Math.min(menu.x, cv.width - w - 4))), y = Math.round(Math.max(4, Math.min(menu.y, cv.height - h - 4))); plate(x, y, w, h); text(g, x + 8, y + 7, menu.title, '#ffcc33', p); rect(x + 4, y + 21, w - 8, 1, '#3a362f');
  let yy = y + 25; const hits = [];
  for (const r of rows) { if (!r) { rect(x + 6, yy + 2, w - 12, 1, '#3a362f'); yy += 6; continue; } if (r.hov) rect(x + 3, yy, w - 6, rh, '#2c2822'); text(g, x + 8, yy + 3, r.label, r.title ? '#ff8a2b' : r.grey ? '#5a554c' : '#fff', p); hits.push([x, yy, w, rh, r]); yy += rh; }
  panelRects.menu = [x, y, w, h, hits];
}
function menuClick(mx, my) { const P = panelRects.menu; if (!P) return false; if (!inRect(mx, my, P)) { menu = null; return true; } for (const [x, y, w, h, r] of P[4]) if (inRect(mx, my, [x, y, w, h]) && !r.grey && !r.title) { const before = menu; const res = r.act(); if (menu === before && res !== 'keep') menu = null; } return true; }
function drawLinkMode() {
  if (!linkMode) return; const N = noteOf(); const x = px(linkMode.x), y = py(linkMode.y); frame(x + 1, y + 1, cs - 2, cs - 2, COL.note, 2);
  N.links.filter(l => l[0] === linkMode.x && l[1] === linkMode.y).forEach(l => frame(px(l[2]) + 2, py(l[3]) + 2, cs - 4, cs - 4, COL.note, 2));
  const s = 'LINK MODE: CLICK CELLS TO ADD OR REMOVE, RIGHT-CLICK OR ESCAPE ENDS', p = 2, w = textW(s, p) + 12; const X = Math.round((cv.width - w) / 2); plate(X, cv.height - RUL - 26, w, 20); text(g, X + 6, cv.height - RUL - 20, s, COL.note, p);
}
// ---------------- the debug panel (+) ----------------
function drawDebugPanel() {
  panelRects.debug = null; if (!debugOpen) return; const p = 2, rh = 18, champs = DATA.index.champions;
  const rows = [...champs.map(c => [c[0], ['FIG ' + c[1], 'NIN ' + c[2], 'PRI ' + c[3], 'WIZ ' + c[4]]]), ['LIGHT', ['REAL 3', '0', '1', '2', '3', '4', '5']], ['MONSTERS', ['FROZEN', 'MOVE']], ['FORGET', ['LEVEL', 'DUNGEON', 'LEARNED', 'MARKS']], ['MEASURE', ['WALLS']]];
  const nameW = Math.max(...rows.map(r => textW(r[0], p))) + 16; const w = nameW + Math.max(...rows.map(r => r[1].reduce((n, c) => n + textW(c, p) + 14, 0))) + 10, h = 26 + rows.length * rh + 6;
  const x = Math.max(4, Math.min(debugBtnX - w + 20, cv.width - w - 4)), y = 2; plate(x, y, w, h); text(g, x + 8, y + 7, champs.length ? 'GIVE ONE LEVEL, OR A TESTING AID' : 'NO CHAMPION TO GAIN A LEVEL', '#ffcc33', p);
  const hits = []; rows.forEach((r, i) => { const yy = y + 26 + i * rh; text(g, x + 8, yy + 4, r[0], '#d8cfbd', p); let cx = x + nameW;
    r[1].forEach((cell, j) => { const cw = textW(cell, p) + 10; const on = (r[0] === 'LIGHT' && j === lightCol) || (r[0] === 'MONSTERS' && (j === 0) === frozen); rect(cx, yy, cw, rh - 3, on ? '#ffcc33' : '#1d1a16'); frame(cx, yy, cw, rh - 3, '#4a443a'); text(g, cx + 5, yy + 4, cell, on ? '#1a160f' : '#fff', p); hits.push([cx, yy, cw, rh - 3, r[0], j]); cx += cw + 4; }); });
  panelRects.debug = [x, y, w, h, hits];
}
function debugClick(mx, my) { const P = panelRects.debug; if (!P) return false; if (!inRect(mx, my, P)) { debugOpen = false; return true; } for (const [x, y, w, h, row, j] of P[4]) if (inRect(mx, my, [x, y, w, h])) { if (row === 'LIGHT') lightCol = j; if (row === 'MONSTERS') frozen = j === 0; } return true; }

// ============================================================================================
// Frame
// ============================================================================================
function draw() {
  if (!DATA) return; clampPan(); g.fillStyle = '#0c0b09'; g.fillRect(0, 0, cv.width, cv.height);
  g.save(); g.beginPath(); g.rect(RUL, RUL, cv.width - 2 * RUL, cv.height - 2 * RUL); g.clip();
  for (const c of V.cells) drawCellBase(c);
  for (const c of V.cells) drawWallFaces(c);
  drawClouds();
  for (const c of V.cells) { drawCellOverlay(c); drawActuators(c); drawItems(c); }
  drawNotes(); drawGlimpses(); drawMissiles(); drawMonsters(); drawParty();
  const L = drawHover(); if (!L.lit) { drawSearchMarks(); drawStairMark(); } drawLinkMode();
  g.restore(); rulers(); drawTooltip(L); drawSearchPanel(); drawShowPanel(); drawHelp(); drawLevels(); drawDebugPanel(); drawMenu(); drawBar();
  writeLink();
}
// The settings as a link in the address bar (read again in init): #L=3&v=all&s=seen&tint=0&walked=1&hide=MONSTERS,KEYS
// Following the party writes follow=1 instead of the level. Only a change is written.
const VISION_WORDS = ['fog', 'explored', 'all'], SPOIL_WORDS = ['all', 'learned', 'seen'];
let lastLink = null;
function writeLink() {
  const h = new URLSearchParams();
  if (follow && hasLevel(DATA.index.partyLevel)) h.set('follow', '1'); else h.set('L', lvl);
  h.set('v', VISION_WORDS[vision]); h.set('s', SPOIL_WORDS[spoil]);
  if (vision === 2 && !tint) h.set('tint', '0');
  if (walked) h.set('walked', '1');
  const hidden = Object.keys(SHOW).filter(k => !SHOW[k]);
  if (hidden.length) h.set('hide', hidden.length === Object.keys(SHOW).length ? 'ALL' : hidden.join(','));
  if (barLook !== 'stone') h.set('bar', barLook);
  const link = '#' + h.toString().replace(/\+/g, '%20').replace(/%2C/g, ',');
  if (link === lastLink) return; lastLink = link;
  try { history.replaceState(null, '', link); } catch (e) { }
}
// ============================================================================================
// View
// ============================================================================================
function resize() { const top = document.getElementById('wrap').offsetTop; cv.width = Math.max(400, window.innerWidth - 34); cv.height = Math.max(300, window.innerHeight - top - 26 - 14); tbar.width = cv.width; tbar.height = 26; }
// zoom in until about three cells span the shorter side of the map area
// cells are always a whole number of pixels: fractional sizes leave gaps and push patterns past their frames
function maxCs() { return Math.floor(Math.max(96, Math.min(cv.width - 2 * RUL, cv.height - 2 * RUL) / 3)); }
function minCs() { return Math.max(1, Math.floor(Math.min((cv.width - 2 * RUL) / W, (cv.height - 2 * RUL) / H))); }
var zoomFree = false; // set by a wheel zoom: a level that still fits an axis is placed by the pan, not centred, so the cell under the pointer stays
function fitLevel() { zoomFree = false; cs = minCs(); panX = Math.round(((cv.width - 2 * RUL) - W * cs) / 2); panY = Math.round(((cv.height - 2 * RUL) - H * cs) / 2); }
function centreOn(x, y) { let top = RUL; if (panelRects.search) top = panelRects.search[1] + panelRects.search[3]; panX = Math.round((cv.width - 2 * RUL) / 2 - (x + .5) * cs); panY = Math.round(((top + cv.height - RUL) / 2 - RUL) - (y + .5) * cs); }
// as in the game: a level smaller than the map stays centred, a larger one never leaves a gap at an edge
// the black margin a zoomed level may leave at either end (as AllowedMargin in AutoMapScene.cpp)
function allowedMargin(room, size) { return size <= room ? room - size / 2 : Math.floor(room / 2); }
function clampAxis(pan, room, cells) { const size = cells * cs; if (zoomFree) { const m = allowedMargin(room, size); return Math.max(room - size - m, Math.min(pan, m)); } return size <= room ? Math.round((room - size) / 2) : Math.max(room - size, Math.min(pan, 0)); }
function clampPan() { panX = clampAxis(panX, cv.width - 2 * RUL, W); panY = clampAxis(panY, cv.height - 2 * RUL, H); }
function centreOnParty() { const p = party(); if (p && cs > minCs() + .01) centreOn(p.x, p.y); }
function cellAt(mx, my) { const x = Math.floor((mx - RUL - panX) / cs), y = Math.floor((my - RUL - panY) / cs); return (x >= 0 && x < W && y >= 0 && y < H && mx > RUL && my > RUL && mx < cv.width - RUL && my < cv.height - RUL) ? { x, y } : null; }

// ============================================================================================
// Input
// ============================================================================================
let drag = null;
cv.addEventListener('contextmenu', e => e.preventDefault());
cv.addEventListener('mousedown', e => {
  const mx = e.offsetX, my = e.offsetY; stairMark = null;
  if (e.button === 1) { e.preventDefault(); followParty(); draw(); return; }
  if (e.button === 3 || e.button === 4) { e.preventDefault(); stepLevel(e.button === 3 ? 1 : -1); draw(); return; }
  if (e.button === 2) { if (linkMode) { linkMode = null; draw(); return; } const c = cellAt(mx, my); closePanels(); if (c && (debugOn || V.cells.some(o => o.x === c.x && o.y === c.y))) openMenu(mx, my, c.x, c.y); draw(); return; }
  if (e.button !== 0) return;
  if (menu) { menuClick(mx, my); draw(); return; }
  if (helpOpen) { helpOpen = false; draw(); return; }
  if (levelOpen) { const P = panelRects.levels; if (inRect(mx, my, P)) { for (const [x, y, w, h, l] of levelRows) if (inRect(mx, my, [x, y, w, h])) pickLevel(l); } else levelOpen = false; draw(); return; }
  if (showOpen && showClick(mx, my)) { draw(); return; }
  if (debugOpen && debugClick(mx, my)) { draw(); return; }
  if (searchClick(mx, my)) { draw(); return; }
  if (e.ctrlKey) { const c = cellAt(mx, my); if (c && followStairs(c.x, c.y)) { draw(); return; } }
  drag = { x: mx, y: my, px: panX, py: panY, moved: false };
});
window.addEventListener('mouseup', e => {
  if (drag && !drag.moved && e.button === 0 && linkMode) { const c = cellAt(drag.x, drag.y); if (c && !(c.x === linkMode.x && c.y === linkMode.y)) { const N = noteOf(); const i = N.links.findIndex(l => l[0] === linkMode.x && l[1] === linkMode.y && l[2] === c.x && l[3] === c.y); if (i >= 0) N.links.splice(i, 1); else N.links.push([linkMode.x, linkMode.y, c.x, c.y, linkMode.side >= 0 ? 1 : 0, linkMode.side, 0, 0, 0, 0]); } }
  drag = null; cv.classList.remove('drag'); draw();
});
cv.addEventListener('mousemove', e => {
  const mx = e.offsetX, my = e.offsetY;
  if (drag) { if (Math.abs(mx - drag.x) + Math.abs(my - drag.y) > 4) { drag.moved = true; cv.classList.add('drag'); follow = false; } if (drag.moved) { panX = drag.px + mx - drag.x; panY = drag.py + my - drag.y; } }
  levelHover = -1; for (const [x, y, w, h, l] of levelRows) if (inRect(mx, my, [x, y, w, h])) levelHover = l;
  sHover = -1; for (const [rx, ry, rw, rh, i] of sRects) if (inRect(mx, my, [rx, ry, rw, rh]) && sRows[i].hit) sHover = i;
  if (panelRects.menu) for (const [x, y, w, h, r] of panelRects.menu[4]) r.hov = inRect(mx, my, [x, y, w, h]);
  const overPanel = ['search', 'show', 'levels', 'debug', 'menu'].some(k => inRect(mx, my, panelRects[k]));
  const c = overPanel || (drag && drag.moved) ? null : cellAt(mx, my); if (!c || !hover || c.x !== hover.x || c.y !== hover.y) tipScroll = 0; hover = c && V.cells.some(o => o.x === c.x && o.y === c.y) ? c : null; draw();
});
cv.addEventListener('mouseleave', () => { hover = null; draw(); });
cv.addEventListener('wheel', e => {
  e.preventDefault(); const mx = e.offsetX, my = e.offsetY, d = e.deltaY > 0 ? 1 : -1; stairMark = null;
  if (levelOpen && inRect(mx, my, panelRects.levels)) { levelScroll = Math.max(0, Math.min(DATA.index.levels.length - ROWS_SHOWN, levelScroll + d * 3)); draw(); return; }
  if (searchOpen && inRect(mx, my, panelRects.search)) { sTop = Math.max(0, Math.min(Math.max(0, sRows.length - S_ROWS), sTop + d * 3)); draw(); return; }
  if (e.ctrlKey) { tipScale = Math.max(1, Math.min(TIP_SCALE_MOST, tipScale - d)); try { localStorage.setItem('csb-map-tip-scale', String(tipScale)); } catch (err) { } draw(); return; }
  if (e.shiftKey) { tipScroll = Math.max(0, tipScroll + d); draw(); return; }
  const old = cs; let next = Math.round(cs * (d < 0 ? 1.15 : 1 / 1.15)); if (next === cs) next += d < 0 ? 1 : -1; cs = Math.min(maxCs(), next); if (cs <= minCs()) { fitLevel(); draw(); return; }
  const ax = mx - RUL, ay = my - RUL; zoomFree = true; panX = Math.round(ax - (ax - panX) * cs / old); panY = Math.round(ay - (ay - panY) * cs / old); draw();
}, { passive: false });
tbar.addEventListener('wheel', e => { e.preventDefault(); barScroll = Math.max(0, barScroll + (e.deltaY > 0 ? 24 : -24)); draw(); }, { passive: false });
tbar.addEventListener('mousedown', e => { if (barLeft && e.offsetX < barArrowW()) { barScroll = Math.max(0, barScroll - tbar.width / 2); draw(); return; } if (barRight && e.offsetX >= tbar.width - barArrowW()) { barScroll += tbar.width / 2; draw(); return; } for (const [x, w, act] of barHits) if (e.offsetX >= x && e.offsetX < x + w && e.offsetY >= 4 && e.offsetY < 22) { act(); draw(); return; } });
window.addEventListener('keydown', e => {
  // The page has no text field: Backspace is never the browser's Back (Firefox can be set to that)
  if (e.key === 'Backspace') e.preventDefault();
  if (e.key === 'Shift') { shiftHeld = true; draw(); }
  if (levelOpen) { if (e.key >= '0' && e.key <= '9') { typed = (typed + e.key).slice(-2); const l = Math.min(DATA.index.levels.length - 1, +typed); levelScroll = Math.max(0, Math.min(DATA.index.levels.length - ROWS_SHOWN, l - 4)); levelHover = l; } else if (e.key === 'Enter' && typed) pickLevel(Math.min(DATA.index.levels.length - 1, +typed)); else if (e.key === 'Escape') { levelOpen = false; typed = ''; } else if (e.key === 'Backspace') { e.preventDefault(); typed = typed.slice(0, -1); } draw(); return; }
  if (e.key === 'Escape') { if (linkMode) linkMode = null; else if (menu) menu = null; else if (searchOpen) closeSearch(); else closePanels(); draw(); return; }
  if (e.key === 'PageDown' || e.key === 'PageUp') { e.preventDefault(); stepLevel(e.key === 'PageDown' ? 1 : -1); draw(); return; }
  if (!searchOpen) { if (e.key === '/' || (e.ctrlKey && e.key === 'f')) { e.preventDefault(); openSearch(); draw(); } return; }
  if (e.key === 'ArrowDown') { e.preventDefault(); step(1); } else if (e.key === 'ArrowUp') { e.preventDefault(); step(-1); } else if (e.key === 'Enter') goTo(sSel);
  else if (e.key === 'Backspace') { e.preventDefault(); query = query.slice(0, -1); rebuild(false); } else if (e.key.length === 1 && !e.ctrlKey) { e.preventDefault(); query += e.key.toUpperCase(); rebuild(false); }
  draw();
});
window.addEventListener('keyup', e => { if (e.key === 'Shift') { shiftHeld = false; draw(); } });
window.addEventListener('resize', () => { resize(); if (cs < minCs()) fitLevel(); draw(); });
document.querySelectorAll('#g-save button').forEach(b => b.onclick = async () => { document.querySelectorAll('#g-save button').forEach(o => o.classList.toggle('on', o === b)); await loadSave(+b.dataset.v); draw(); });
const debugBox = document.getElementById('debug');
if (debugBox) debugBox.onchange = e => { debugOn = e.target.checked; if (!debugOn) debugOpen = false; draw(); };
setInterval(() => { if (searchOpen) draw(); }, 500);

(async function init() {
  resize();
  // A link names the settings: after the ? or the # (the # wins), e.g. #L=3&v=all&s=seen&walked=1
  const q = new URLSearchParams(location.search);
  new URLSearchParams(location.hash.replace(/^#/, '')).forEach((v, k) => q.set(k, v));
  const sv = +(q.get('save') || 0); document.querySelectorAll('#g-save button').forEach(b => b.classList.toggle('on', +b.dataset.v === sv));
  if (q.get('bar') && BAR_LOOKS.includes(q.get('bar'))) barLook = q.get('bar'); if (q.get('debug') && debugBox) { debugOn = q.get('debug') !== '0'; debugBox.checked = debugOn; }
  await loadSave(sv);
  // Settings the exported page limits (vision, spoilers) are ignored when not allowed
  const word = (s, names) => { if (s === null) return -1; const i = names.indexOf(s.toLowerCase()); return i >= 0 ? i : /^[0-2]$/.test(s) ? +s : -1; };
  const flag = k => q.get(k) === '1' || q.get(k) === 'true' ? true : q.get(k) === '0' || q.get(k) === 'false' ? false : null;
  const v = word(q.get('v'), VISION_WORDS), s = word(q.get('s'), SPOIL_WORDS);
  if (v >= 0 && visionAllowed(v)) vision = v; if (s >= 0 && spoilAllowed(s)) spoil = s;
  if (flag('tint') !== null) tint = flag('tint'); if (flag('walked') !== null) walked = flag('walked');
  if (q.get('hide') !== null) { const names = q.get('hide').split(',').map(n => n.trim().toUpperCase()); Object.keys(SHOW).forEach(k => SHOW[k] = !(names.includes(k) || names.includes('ALL'))); }
  refreshAll();
  if (flag('follow') === true && followParty()) { /* the party's level */ }
  else if (q.get('L') && hasLevel(+q.get('L'))) { setLevel(+q.get('L')); follow = false; fitLevel(); }
  if (q.get('cs')) { cs = Math.round(Math.min(maxCs(), Math.max(minCs(), +q.get('cs')))); if (cs <= minCs()) fitLevel(); }
  if (q.get('hh')) { const [a, b] = q.get('hh').split(',').map(Number); hover = { x: a, y: b }; centreOn(a, b); }
  if (q.get('find')) { openSearch(); query = q.get('find').toUpperCase(); rebuild(false); }
  if (q.get('shift')) shiftHeld = true;
  let left = Object.keys(img).length; const done = () => { if (--left <= 0) draw(); }; Object.values(img).forEach(i => i.complete ? done() : (i.onload = i.onerror = done));
  draw(); setTimeout(draw, 400);
})();
