// ============================================================================================
// Panels share one look: dark plate, gold frame, 3x5 text
// ============================================================================================
function plate(x, y, w, h) { rect(x, y, w, h, '#111'); frame(x, y, w, h, '#ffc83a'); }
function inRect(mx, my, r) { return r && mx >= r[0] && mx < r[0] + r[2] && my >= r[1] && my < r[1] + r[3]; }

// ---------------- the bar ----------------
function btn(x, label, on, act, grey) {
  const p = 2, w = textW(label, p) + 10; tg.fillStyle = on ? '#ffcc33' : '#1d1a16'; tg.fillRect(x, 4, w, 18);
  tg.fillStyle = on ? '#ffcc33' : '#4a443a'; tg.fillRect(x, 4, w, 1); tg.fillRect(x, 21, w, 1); tg.fillRect(x, 4, 1, 18); tg.fillRect(x + w - 1, 4, 1, 18);
  text(tg, x + 5, 8, label, on ? '#1a160f' : grey ? '#5a554c' : '#d8cfbd', p); if (!grey) barHits.push([x, w, act]); return x + w;
}
function lab(x, s) { text(tg, x, 8, s, '#8f8676', 2); return x + textW(s, 2) + 6; }
let levelBtnX = 0, showBtnX = 0, searchBtnX = 0, debugBtnX = 0;
function drawBar() {
  barHits = []; tg.fillStyle = '#24221f'; tg.fillRect(0, 0, tbar.width, 26); tg.fillStyle = '#3a362f'; tg.fillRect(0, 25, tbar.width, 1);
  let x = 6;
  x = btn(x, '<', false, () => stepLevel(-1)) + 2; levelBtnX = x;
  x = btn(x, 'L' + lvl + ' ~', levelOpen, () => { const o = !levelOpen; closePanels(); levelOpen = o; levelScroll = Math.max(0, lvl - 4); typed = ''; }) + 2;
  x = btn(x, '>', false, () => stepLevel(1)) + 4;
  x = btn(x, 'FOLLOW', follow, () => { follow = !follow; if (follow) { setLevel(DATA.index.partyLevel); centreOnParty(); } }) + 14;
  x = lab(x, 'VISION'); ['FOG', 'EXPLORED', 'ALL'].forEach((n, i) => { x = btn(x, n, vision === i, () => { vision = i; refreshAll(); }) - 1; });
  if (vision === 2) x = btn(x + 4, 'TINT', tint, () => tint = !tint);
  x += 14; x = lab(x, 'SPOILERS'); [['SEEN', 2], ['LEARNED', 1], ['ALL', 0]].forEach(([n, v]) => { x = btn(x, n, spoil === v, () => { spoil = v; refreshAll(); }) - 1; }); x += 14;
  const hidden = Object.values(SHOW).filter(v => !v).length; showBtnX = x;
  x = btn(x, hidden ? 'SHOW: ' + hidden + ' HIDDEN' : 'SHOW', showOpen, () => { const o = !showOpen; closePanels(); showOpen = o; }) + 10;
  searchBtnX = x; x = btn(x, '/', searchOpen, () => { if (searchOpen) closeSearch(); else openSearch(); }) + 3;
  x = btn(x, '?', helpOpen, () => { const o = !helpOpen; closePanels(); helpOpen = o; }) + 3;
  if (debugOn) { debugBtnX = x; x = btn(x, '+', debugOpen, () => { const o = !debugOpen; closePanels(); debugOpen = o; }); }
}
function closePanels() { showOpen = helpOpen = levelOpen = debugOpen = false; menu = null; }
function refreshAll() { refreshView(); buildIndex(); if (query) rebuild(true); }
function stepLevel(d) { const l = Math.max(0, Math.min(DATA.index.levels.length - 1, lvl + d)); if (l === lvl) return; setLevel(l); follow = l === DATA.index.partyLevel; }

// ---------------- SHOW ----------------
function drawShowPanel() {
  panelRects.show = null; if (!showOpen) return; const p = 2, rows = ['SHOW ALL', 'HIDE ALL', null, ...ITEM_KINDS, null, 'MONSTERS', null, 'WALL DECORATIONS', 'WALL WRITING', 'FLOOR DECORATIONS'];
  const w = textW('X FLOOR DECORATIONS', p) + 24, h = rows.reduce((n, r) => n + (r ? 16 : 6), 0) + 8; const x = Math.min(showBtnX, cv.width - w - 4), y = 2;
  plate(x, y, w, h); let yy = y + 5; const hits = [];
  for (const r of rows) { if (!r) { rect(x + 6, yy + 2, w - 12, 1, '#3a362f'); yy += 6; continue; }
    const box = r === 'SHOW ALL' || r === 'HIDE ALL' ? null : SHOW[r]; if (box !== null) { frame(x + 8, yy + 2, 10, 10, '#8f8676'); if (box) rect(x + 10, yy + 4, 6, 6, '#ffcc33'); }
    text(g, x + 24, yy + 3, r, box === false ? '#8f8676' : '#fff', p); hits.push([x, yy, w, 16, r]); yy += 16; }
  panelRects.show = [x, y, w, h, hits];
}
function showClick(mx, my) { const P = panelRects.show; if (!P) return false; if (!inRect(mx, my, P)) { showOpen = false; return true; } for (const [x, y, w, h, r] of P[4]) if (inRect(mx, my, [x, y, w, h])) { if (r === 'SHOW ALL') Object.keys(SHOW).forEach(k => SHOW[k] = true); else if (r === 'HIDE ALL') Object.keys(SHOW).forEach(k => SHOW[k] = false); else SHOW[r] = !SHOW[r]; } return true; }

// ---------------- controls help ----------------
const HELP = [['LEFT-DRAG', 'PAN THE MAP'], ['WHEEL', 'ZOOM TOWARDS THE POINTER'], ['MIDDLE-CLICK', 'CENTRE ON THE PARTY AND FOLLOW'], ['SIDE BUTTONS', 'LEVEL DOWN / UP UNDER THE POINTER'], ['PAGE DOWN / UP', 'LEVEL DOWN / UP (CTRL IN THE GAME)'], ['/ OR CTRL+F', 'SEARCH ALL LEVELS'], ['HOVER', 'WHAT IS THERE, AND ITS LINKS'], ['SHIFT', 'THE WHOLE PRESS CHAIN (SPOILERS ALL)'], ['SHIFT+WHEEL', 'SCROLL A LONG TOOLTIP'], ['RIGHT-CLICK', 'YOUR OWN MARKS AND LINKS'], ['', ''], ['', 'CLICK ANYWHERE TO CLOSE']];
function drawHelp() { if (!helpOpen) return; const p = 2, c1 = textW('PAGE DOWN / UP', p) + 20, w = c1 + textW('LEVEL DOWN / UP UNDER THE POINTER', p) + 20, h = HELP.length * 16 + 30; const x = Math.round((cv.width - w) / 2), y = 40; plate(x, y, w, h); text(g, x + 10, y + 8, 'CONTROLS', '#ffcc33', p); HELP.forEach((r, i) => { text(g, x + 10, y + 26 + i * 16, r[0], '#ffcc33', p); text(g, x + 10 + c1, y + 26 + i * 16, r[1], '#fff', p); }); }

// ---------------- level list ----------------
let levelScroll = 0, typed = '', levelRows = [], levelHover = -1; const ROWS_SHOWN = 12;
function drawLevels() {
  levelRows = []; panelRects.levels = null; if (!levelOpen) return; const n = DATA.index.levels.length, p = 2, rh = 16, w = textW('L00  NOT VISITED  00 HITS', p) + 30, rows = Math.min(ROWS_SHOWN, n), h = rows * rh + 10 + (typed ? 18 : 0);
  const x = Math.max(4, levelBtnX - 4), y = 2; plate(x, y, w, h); let yy = y + 5;
  if (typed) { text(g, x + 8, yy + 2, 'GO TO L' + typed, '#ffcc33', p); yy += 18; }
  for (let i = 0; i < rows; i++) { const l = levelScroll + i; if (l >= n) break; const sel = l === lvl, vis = DATA.index.levels[l].visited;
    if (sel) rect(x + 3, yy, w - 14, rh, '#ffcc33'); else if (l === levelHover) rect(x + 3, yy, w - 14, rh, '#2c2822');
    text(g, x + 8, yy + 3, 'L' + l, sel ? '#1a160f' : vis ? '#fff' : '#6f685c', p); if (l === DATA.index.partyLevel) partyArrow(x + 8 + textW('L00', p) + 10, yy + rh / 2, 5, 1, '#ff4d4d');
    let tx = x + 8 + textW('L00', p) + 22; if (!vis) { text(g, tx, yy + 3, 'NOT VISITED', sel ? '#1a160f' : '#6f685c', p); tx += textW('NOT VISITED', p) + 10; }
    if (searchOpen && query && hitsByLevel[l]) text(g, tx, yy + 3, hitsByLevel[l] + (hitsByLevel[l] > 1 ? ' HITS' : ' HIT'), sel ? '#1a160f' : '#ffcc33', p);
    levelRows.push([x, yy, w - 12, rh, l]); yy += rh; }
  if (n > ROWS_SHOWN) { const th = Math.max(16, (ROWS_SHOWN / n) * (h - 10)), ty = y + 5 + (levelScroll / (n - ROWS_SHOWN)) * (h - 10 - th); rect(x + w - 8, y + 5, 4, h - 10, '#2c2822'); rect(x + w - 8, ty, 4, th, '#8f8676'); }
  panelRects.levels = [x, y, w, h];
}
function pickLevel(l) { setLevel(l); follow = l === DATA.index.partyLevel; levelOpen = false; typed = ''; }

// ---------------- search: over the draw lists of every level, so it keeps the fog ----------------
let query = '', sRows = [], sSel = -1, sTop = 0, folded = new Set(), hitsByLevel = {}, sRects = [], sHover = -1, INDEX = [];
const S_ROWS = 16, S_RH = 20;
function buildIndex() {
  INDEX = []; if (!DATA) return;
  DATA.index.levels.forEach((L, l) => {
    const v = viewOf(l); if (!v) return;
    const add = (x, y, name, tag, icon, kind) => INDEX.push({ l, x, y, gx: x + L.offsetX, gy: y + L.offsetY, name: (name || '').toUpperCase(), tag, icon, kind });
    const monsterAt = {}; v.monsters.forEach(m => monsterAt[m.x + ',' + m.y] = m);
    for (const c of v.cells) {
      if (c.items) { const by = {}; c.items.forEach(i => { if (!i[2]) return; (by[i[2]] = by[i[2]] || { n: 0, icon: i[1] || i[0] }).n++; }); for (const n in by) add(c.x, c.y, n, by[n].n > 1 ? 'X' + by[n].n : '', by[n].icon, 'item'); }
      (c.alc || []).forEach(a => { if (a[1][2]) add(c.x, c.y, a[1][2], 'ALCOVE', a[1][1] || a[1][0], 'item'); });
      (c.locks || []).forEach(k => { if (k[1] && k[2][2]) add(c.x, c.y, k[2][2], 'NEEDS', k[2][1] || k[2][0], 'needs'); });
      (c.sides || []).forEach((s, i) => { if (s && s[7] === 1) add(c.x, c.y, 'FOUNTAIN', 'NESW'[i], s[0], 'feature'); if (s && s[7] === 2) add(c.x, c.y, 'ALTAR OF VI', 'NESW'[i], s[0], 'feature'); });
      const h = v.hover[c.x + ',' + c.y];
      if (h && h.icons) h.icons.forEach(ic => { if (!ic[2] || ic[3] || !ic[0][2]) return; const m = monsterAt[c.x + ',' + c.y]; add(c.x, c.y, ic[0][2], m ? 'CARRIED BY ' + nice(m.name) : 'IN CHEST', ic[0][1] || ic[0][0], 'item'); });
    }
    const seen = new Set();
    v.acts.forEach(a => { const k = a[0] + ',' + a[1] + ',' + a[2] + ',' + a[3]; if (seen.has(k)) return; seen.add(k);
      if ((a[3] === 4 || a[3] === 6) && a[2] === 0 && a[5][2]) add(a[0], a[1], a[5][2], a[3] === 6 ? 'PAD, CARRIED' : 'PAD NEEDS', a[5][1] || a[5][0], 'needs');
      if (a[2] === 2 && a[7]) add(a[0], a[1], nice(a[7]), 'GENERATOR', a[6], 'gen'); });
    v.monsters.forEach(m => add(m.x, m.y, nice(m.name) || 'MONSTER', m.m.length > 1 ? 'X' + m.m.length : '', m.m[0] && m.m[0][0], 'monster'));
  });
}
function matches(h, q) { if (!q) return false; if (h.name.includes(q)) return true; if (('GENERATOR'.startsWith(q) || q === 'GEN') && h.kind === 'gen') return true; if ('MONSTER'.startsWith(q) && q.length >= 3 && h.kind === 'monster') return true; return false; }
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
      if (hh.kind === 'needs') { rect(x + 21, yy + 1, 18, 18, '#0c0b09'); frame(x + 21, yy + 1, 18, 18, '#e0b64a'); } spr(hh.icon, x + 22, yy + 2, 16, 16); if (hh.kind === 'gen') { rect(x + 31, yy + 11, 9, 8, '#0d0c0a'); text(g, x + 33, yy + 12, 'G', '#ff8080', 1); }
      text(g, x + 46, yy + 5, `(${hh.gx},${hh.gy})`, sel ? '#1a160f' : '#b8b0a0', p); text(g, x + 46 + textW('(00,00)', p) + 10, yy + 5, hh.name, sel ? '#1a160f' : '#fff', p);
      if (hh.tag) { const tc = sel ? '#1a160f' : hh.kind === 'needs' ? '#e0b64a' : hh.kind === 'gen' ? '#ff8080' : '#8f8676'; text(g, x + w - 16 - textW(hh.tag, p), yy + 5, hh.tag, tc, p); } }
    sRects.push([x, yy, w - 12, S_RH, i]); yy += S_RH; }
  if (sRows.length > S_ROWS) { const tr = h - 34, th = Math.max(14, tr * S_ROWS / sRows.length), ty = y + 28 + (tr - th) * sTop / (sRows.length - S_ROWS); rect(x + w - 8, y + 28, 4, tr, '#2c2822'); rect(x + w - 8, ty, 4, th, '#8f8676'); }
  panelRects.search = [x, y, w, h];
}
function drawSearchMarks() {
  if (!searchOpen || !query) return; const sel = sRows[sSel] && sRows[sSel].hit; const mine = sRows.filter(r => r.hit && r.hit.l === lvl).map(r => r.hit);
  if (sel && sel.l === lvl && !hover) spotlight(mine.map(h => h.x + ',' + h.y));
  for (const h of mine) { const x = px(h.x), y = py(h.y), t = Math.max(2, Math.round(cs / 14)); frame(x + 1, y + 1, cs - 2, cs - 2, '#0b0a08', t + 2); frame(x + 2, y + 2, cs - 4, cs - 4, h === sel ? '#fff6dc' : '#ffcc33', t); }
}
function searchClick(mx, my) { const P = panelRects.search; if (!P || !inRect(mx, my, P)) return false; for (const [rx, ry, rw, rh, i] of sRects) if (inRect(mx, my, [rx, ry, rw, rh])) { const r = sRows[i]; if (r.head) { folded.has(r.l) ? folded.delete(r.l) : folded.add(r.l); rebuild(true); keepVisible(); } else goTo(i); } return true; }

// ---------------- right-click menu: your notes, and with map_debug the testing aids ----------------
const NESW = 'NESW';
function nearestSide(mx, my, x0, y0) { const ox = mx - px(x0) - cs / 2, oy = my - py(y0) - cs / 2; return Math.abs(ox) > Math.abs(oy) ? (ox > 0 ? 1 : 3) : (oy > 0 ? 2 : 0); }
function openMenu(mx, my, cx, cy) {
  const c = C(cx, cy); const rows = []; const key = cx + ',' + cy, N = noteOf();
  if (c && c.k === 1) { rows.push({ label: 'HIDDEN PIT (OPEN)', act: () => N.pits[key] = 1 }); rows.push({ label: 'HIDDEN PIT (CLOSED)', act: () => N.pits[key] = 0 }); }
  if (c && (c.k === 1 || c.k === 0)) { const side = c.k === 0 ? nearestSide(mx, my, cx, cy) : -1; rows.push({ label: 'LINK FROM HERE' + (side >= 0 ? ' (' + NESW[side] + ')' : ''), act: () => linkMode = { x: cx, y: cy, side } }); }
  if (key in N.pits) rows.push({ label: 'REMOVE MARK', act: () => delete N.pits[key] });
  if (N.links.some(l => (l[0] === cx && l[1] === cy) || (l[2] === cx && l[3] === cy))) rows.push({ label: 'REMOVE LINKS', act: () => N.links = N.links.filter(l => !((l[0] === cx && l[1] === cy) || (l[2] === cx && l[3] === cy))) });
  if (debugOn) {
    const stand = c && c.k !== 0 && c.k !== 6; const why = stand ? '' : 'NOT A FLOOR'; if (rows.length) rows.push(null);
    rows.push({ label: 'TELEPORT HERE', grey: !stand, act: () => { partyOverride = { l: lvl, x: cx, y: cy, f: 2 }; } });
    DATA.index.levels[lvl].types.forEach(t => rows.push({ label: 'SPAWN ' + nice(t), grey: !stand, act: () => spawnSteps(nice(t), mx, my, cx, cy) }));
    if (why) rows.unshift({ label: why, title: true });
  }
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
  const x = Math.round(Math.min(menu.x, cv.width - w - 4)), y = Math.round(Math.min(menu.y, cv.height - h - 4)); plate(x, y, w, h); text(g, x + 8, y + 7, menu.title, '#ffcc33', p); rect(x + 4, y + 21, w - 8, 1, '#3a362f');
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
  if (!DATA) return; g.fillStyle = '#0c0b09'; g.fillRect(0, 0, cv.width, cv.height);
  g.save(); g.beginPath(); g.rect(RUL, RUL, cv.width - 2 * RUL, cv.height - 2 * RUL); g.clip();
  for (const c of V.cells) drawCellBase(c);
  for (const c of V.cells) drawWallFaces(c);
  drawClouds();
  for (const c of V.cells) { drawCellOverlay(c); drawActuators(c); drawItems(c); }
  drawNotes(); drawGlimpses(); drawMissiles(); drawMonsters(); drawParty();
  const L = drawHover(); drawSearchMarks(); drawLinkMode();
  g.restore(); rulers(); drawTooltip(L); drawSearchPanel(); drawShowPanel(); drawHelp(); drawLevels(); drawDebugPanel(); drawMenu(); drawBar();
}
// ============================================================================================
// View
// ============================================================================================
function resize() { const top = document.getElementById('wrap').offsetTop; cv.width = Math.max(400, window.innerWidth - 34); cv.height = Math.max(300, window.innerHeight - top - 26 - 14); tbar.width = cv.width; tbar.height = 26; }
function minCs() { return Math.min((cv.width - 2 * RUL) / W, (cv.height - 2 * RUL) / H); }
function fitLevel() { cs = minCs(); panX = ((cv.width - 2 * RUL) - W * cs) / 2; panY = ((cv.height - 2 * RUL) - H * cs) / 2; }
function centreOn(x, y) { let top = RUL; if (panelRects.search) top = panelRects.search[1] + panelRects.search[3]; panX = (cv.width - 2 * RUL) / 2 - (x + .5) * cs; panY = ((top + cv.height - RUL) / 2 - RUL) - (y + .5) * cs; }
function centreOnParty() { const p = party(); if (p && cs > minCs() + .01) centreOn(p.x, p.y); }
function cellAt(mx, my) { const x = Math.floor((mx - RUL - panX) / cs), y = Math.floor((my - RUL - panY) / cs); return (x >= 0 && x < W && y >= 0 && y < H && mx > RUL && my > RUL && mx < cv.width - RUL && my < cv.height - RUL) ? { x, y } : null; }

// ============================================================================================
// Input
// ============================================================================================
let drag = null;
cv.addEventListener('contextmenu', e => e.preventDefault());
cv.addEventListener('mousedown', e => {
  const mx = e.offsetX, my = e.offsetY;
  if (e.button === 1) { e.preventDefault(); follow = true; setLevel(DATA.index.partyLevel); centreOnParty(); draw(); return; }
  if (e.button === 3 || e.button === 4) { e.preventDefault(); stepLevel(e.button === 3 ? 1 : -1); draw(); return; }
  if (e.button === 2) { if (linkMode) { linkMode = null; draw(); return; } const c = cellAt(mx, my); closePanels(); if (c && V.cells.some(o => o.x === c.x && o.y === c.y)) openMenu(mx, my, c.x, c.y); draw(); return; }
  if (e.button !== 0) return;
  if (menu) { menuClick(mx, my); draw(); return; }
  if (helpOpen) { helpOpen = false; draw(); return; }
  if (levelOpen) { const P = panelRects.levels; if (inRect(mx, my, P)) { for (const [x, y, w, h, l] of levelRows) if (inRect(mx, my, [x, y, w, h])) pickLevel(l); } else levelOpen = false; draw(); return; }
  if (showOpen && showClick(mx, my)) { draw(); return; }
  if (debugOpen && debugClick(mx, my)) { draw(); return; }
  if (searchClick(mx, my)) { draw(); return; }
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
  e.preventDefault(); const mx = e.offsetX, my = e.offsetY, d = e.deltaY > 0 ? 1 : -1;
  if (levelOpen && inRect(mx, my, panelRects.levels)) { levelScroll = Math.max(0, Math.min(DATA.index.levels.length - ROWS_SHOWN, levelScroll + d * 3)); draw(); return; }
  if (searchOpen && inRect(mx, my, panelRects.search)) { sTop = Math.max(0, Math.min(Math.max(0, sRows.length - S_ROWS), sTop + d * 3)); draw(); return; }
  if (e.shiftKey) { tipScroll = Math.max(0, tipScroll + d); draw(); return; }
  const old = cs; cs = Math.min(96, cs * (d < 0 ? 1.15 : 1 / 1.15)); if (cs <= minCs() + .01) { fitLevel(); draw(); return; }
  const ax = mx - RUL, ay = my - RUL; panX = ax - (ax - panX) * cs / old; panY = ay - (ay - panY) * cs / old; draw();
}, { passive: false });
tbar.addEventListener('mousedown', e => { for (const [x, w, act] of barHits) if (e.offsetX >= x && e.offsetX < x + w && e.offsetY >= 4 && e.offsetY < 22) { act(); draw(); return; } });
window.addEventListener('keydown', e => {
  if (e.key === 'Shift') { shiftHeld = true; draw(); }
  if (levelOpen) { if (e.key >= '0' && e.key <= '9') { typed = (typed + e.key).slice(-2); const l = Math.min(DATA.index.levels.length - 1, +typed); levelScroll = Math.max(0, Math.min(DATA.index.levels.length - ROWS_SHOWN, l - 4)); levelHover = l; } else if (e.key === 'Enter' && typed) pickLevel(Math.min(DATA.index.levels.length - 1, +typed)); else if (e.key === 'Escape') { levelOpen = false; typed = ''; } else if (e.key === 'Backspace') typed = typed.slice(0, -1); draw(); return; }
  if (e.key === 'Escape') { if (linkMode) linkMode = null; else if (menu) menu = null; else if (searchOpen) closeSearch(); else closePanels(); draw(); return; }
  if (e.key === 'PageDown' || e.key === 'PageUp') { e.preventDefault(); stepLevel(e.key === 'PageDown' ? 1 : -1); draw(); return; }
  if (!searchOpen) { if (e.key === '/' || (e.ctrlKey && e.key === 'f')) { e.preventDefault(); openSearch(); draw(); } return; }
  if (e.key === 'ArrowDown') { e.preventDefault(); step(1); } else if (e.key === 'ArrowUp') { e.preventDefault(); step(-1); } else if (e.key === 'Enter') goTo(sSel);
  else if (e.key === 'Backspace') { query = query.slice(0, -1); rebuild(false); } else if (e.key.length === 1 && !e.ctrlKey) { e.preventDefault(); query += e.key.toUpperCase(); rebuild(false); }
  draw();
});
window.addEventListener('keyup', e => { if (e.key === 'Shift') { shiftHeld = false; draw(); } });
window.addEventListener('resize', () => { resize(); if (cs < minCs()) fitLevel(); draw(); });
document.querySelectorAll('#g-save button').forEach(b => b.onclick = async () => { document.querySelectorAll('#g-save button').forEach(o => o.classList.toggle('on', o === b)); await loadSave(+b.dataset.v); draw(); });
document.getElementById('debug').onchange = e => { debugOn = e.target.checked; if (!debugOn) debugOpen = false; draw(); };
setInterval(() => { if (searchOpen) draw(); }, 500);

(async function init() {
  resize();
  const q = new URLSearchParams(location.search);
  const sv = +(q.get('save') || 0); document.querySelectorAll('#g-save button').forEach(b => b.classList.toggle('on', +b.dataset.v === sv));
  if (q.get('v')) vision = +q.get('v'); if (q.get('s')) spoil = +q.get('s'); if (q.get('debug')) { debugOn = true; document.getElementById('debug').checked = true; }
  await loadSave(sv);
  if (q.get('L')) { setLevel(+q.get('L')); follow = false; fitLevel(); }
  if (q.get('cs')) { cs = Math.max(minCs(), +q.get('cs')); if (cs <= minCs() + .01) fitLevel(); }
  if (q.get('hh')) { const [a, b] = q.get('hh').split(',').map(Number); hover = { x: a, y: b }; centreOn(a, b); }
  if (q.get('find')) { openSearch(); query = q.get('find').toUpperCase(); rebuild(false); }
  if (q.get('shift')) shiftHeld = true;
  let left = Object.keys(img).length; const done = () => { if (--left <= 0) draw(); }; Object.values(img).forEach(i => i.complete ? done() : (i.onload = i.onerror = done));
  draw(); setTimeout(draw, 400);
})();
