"use strict";
// ============================================================================================
// State
// ============================================================================================
const SAVE_NAMES = Object.keys(MAPDATA.saves);
const SAVES = {};            // unpacked saves
let DATA = null;             // the shown save: {index, views}
let saveNo = 0, lvl = 0, vision = 0, spoil = 1 /* 0 All, 1 Learned, 2 Seen */, tint = true;
let follow = true, cs = 36, panX = 0, panY = 0, hover = null, shiftHeld = false, debugOn = true;
let V = null, grid = {}, actsAt = {}, monAt = {}, W = 0, H = 0, OX = 0, OY = 0;
let tipScroll = 0;
const SHOW = {};
const ITEM_KINDS = ['WEAPONS', 'ARMOUR', 'POTIONS', 'SCROLLS', 'CHESTS', 'FOOD', 'KEYS', 'OTHER ITEMS'];
[...ITEM_KINDS, 'MONSTERS', 'WALL DECORATIONS', 'WALL WRITING', 'FLOOR DECORATIONS'].forEach(k => SHOW[k] = true);
// panels
let showOpen = false, helpOpen = false, levelOpen = false, searchOpen = false, debugOpen = false;
let menu = null;             // right-click menu: {x, y, title, rows:[{label, act, grey}], cell}
let linkMode = null;         // {x, y, side}
const notes = {};            // per save/level: {pits:{key:0|1}, links:[[sx,sy,dx,dy,wall,side]]}
let barHits = [], panelRects = {};
let partyOverride = null;
let lightCol = 0, frozen = false;
const RUL = 18;

const img = {};
for (const k in MAPDATA.sprites) { const i = new Image(); i.src = MAPDATA.sprites[k]; img[k] = i; }
const PLATE = Object.keys(MAPDATA.sprites).find(k => /_p258_t10\.bmp$/.test(k));

const cv = document.getElementById('c'), g = cv.getContext('2d');
const tbar = document.getElementById('tb'), tg = tbar.getContext('2d');

// ============================================================================================
// Data
// ============================================================================================
async function unpack(b64) {
  const bytes = Uint8Array.from(atob(b64), ch => ch.charCodeAt(0));
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'));
  return JSON.parse(await new Response(stream).text());
}
async function loadSave(n) {
  saveNo = n;
  if (!SAVES[n]) SAVES[n] = await unpack(MAPDATA.saves[SAVE_NAMES[n]]);
  DATA = SAVES[n];
  document.getElementById('status').textContent = SAVE_NAMES[n] + ': ' + DATA.index.levels.length + ' levels, party on L' + DATA.index.partyLevel;
  partyOverride = null; menu = null; linkMode = null;
  setLevel(DATA.index.partyLevel); follow = true; fitLevel(); cs = Math.max(minCs(), 36); centreOnParty(); buildIndex(); if (query) rebuild(false);
}
function noteOf() { const k = saveNo + ':' + lvl; return notes[k] || (notes[k] = { pits: {}, links: [] }); }
function viewOf(l) { return DATA.views[`L${l}_v${vision}_s${spoil}`]; }
function setLevel(l) {
  lvl = l; const L = DATA.index.levels[l]; W = L.w; H = L.h; OX = L.offsetX; OY = L.offsetY;
  refreshView(); hover = null; tipScroll = 0;
  if (cv.width && cs < minCs()) fitLevel();
}
function refreshView() {
  V = viewOf(lvl); grid = {}; actsAt = {}; monAt = {};
  V.cells.forEach(c => grid[c.x + ',' + c.y] = c);
  V.acts.forEach(a => { const k = a[0] + ',' + a[1]; (actsAt[k] = actsAt[k] || []).push(a); });
  V.monsters.forEach(m => monAt[m.x + ',' + m.y] = m);
}
const C = (x, y) => grid[x + ',' + y];
const isRock = c => c && c.k === 0;
// The names the player reads (GetMonsterDisplayName, Statistics.cpp), not the engine's internal
// ones: the internal table has "25" for Lord Order and run-together words
const INTERNAL = ['Scorpion', 'SlimeDevil', 'Giggler', 'FlyingEye', 'PainRat', 'Ruster', 'Screamer', 'RockPile', 'Rive', 'StoneGolem', 'Mummy', 'BlackFlame', 'Skeleton', 'Couatl', 'Vexirk', 'Worm', 'Trolin', 'Wasp', 'DethKnight', 'Zytaz', 'WaterElemental', 'Oitu', 'Demon', 'LordChaos', 'Dragon', '25', 'GreyLord'];
const DISPLAY = ['Scorpion', 'Slime Devil', 'Giggler', 'Flying Eye', 'Pain Rat', 'Ruster', 'Screamer', 'Rock Pile', 'Rive', 'Stone Golem', 'Mummy', 'Black Flame', 'Skeleton', 'Couatl', 'Vexirk', 'Worm', 'Trolin', 'Wasp', 'Deth Knight', 'Zytaz', 'Water Elemental', 'Oitu', 'Demon', 'Lord Chaos', 'Dragon', 'Lord Order', 'Grey Lord'];
const nice = n => { const i = INTERNAL.indexOf(n || ''); return (i >= 0 ? DISPLAY[i] : (n || '').replace(/([a-z])([A-Z])/g, '$1 $2')).toUpperCase(); };
function party() {
  if (partyOverride && partyOverride.l === lvl) return partyOverride;
  return V.party[3] ? { x: V.party[0], y: V.party[1], f: V.party[2] } : null;
}

// ============================================================================================
// Pixel helpers: everything lands on whole pixels
// ============================================================================================
const LET = [[2,5,7,5,5],[6,5,6,5,6],[3,4,4,4,3],[6,5,5,5,6],[7,4,6,4,7],[7,4,6,4,4],[3,4,5,5,3],[5,5,7,5,5],[7,2,2,2,7],[1,1,1,5,2],[5,5,6,5,5],[4,4,4,4,7],[5,7,5,5,5],[6,5,5,5,5],[2,5,5,5,2],[6,5,6,4,4],[2,5,5,6,3],[6,5,6,5,5],[3,4,2,1,6],[7,2,2,2,2],[5,5,5,5,7],[5,5,5,5,2],[5,5,5,7,5],[5,5,2,5,5],[5,5,2,2,2],[7,1,2,4,7]];
const DIG = [[7,5,5,5,7],[2,6,2,2,7],[7,1,7,4,7],[7,1,7,1,7],[5,5,7,1,1],[7,4,7,1,7],[7,4,7,5,7],[7,1,2,2,2],[7,5,7,5,7],[7,5,7,1,7]];
const PUN = {' ':[0,0,0,0,0],'+':[0,2,7,2,0],'-':[0,0,7,0,0],'.':[0,0,0,0,2],',':[0,0,0,2,4],':':[0,2,0,2,0],'?':[6,1,2,0,2],'!':[2,2,2,0,2],'/':[1,1,2,4,4],'(':[2,4,4,4,2],')':[2,1,1,1,2],'=':[0,7,0,7,0],'>':[4,2,1,2,4],'<':[1,2,4,2,1],'~':[0,0,7,2,0],"'":[2,2,0,0,0],'%':[5,1,2,4,5],'_':[0,0,0,0,7]};
function glyph(ch) { ch = ch.toUpperCase(); if (ch >= 'A' && ch <= 'Z') return LET[ch.charCodeAt(0) - 65]; if (ch >= '0' && ch <= '9') return DIG[+ch]; return PUN[ch] || PUN[' ']; }
function text(c2, x, y, s, col, p) { x = Math.round(x); y = Math.round(y); c2.fillStyle = col; for (let i = 0; i < s.length; i++) { const r = glyph(s[i]); for (let row = 0; row < 5; row++) for (let c = 0; c < 3; c++) if (r[row] & (1 << (2 - c))) c2.fillRect(x + (i * 4 + c) * p, y + row * p, p, p); } }
const textW = (s, p) => s.length ? (s.length * 4 - 1) * p : 0;
function rect(x, y, w, h, col) { g.fillStyle = col; g.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h)); }
function frame(x, y, w, h, col, t = 1) { rect(x, y, w, t, col); rect(x, y + h - t, w, t, col); rect(x, y, t, h, col); rect(x + w - t, y, t, h, col); }
function dashed(x, y, w, h, col, d = 3) { g.save(); g.setLineDash([d, d]); g.strokeStyle = col; g.lineWidth = 1; g.strokeRect(Math.round(x) + .5, Math.round(y) + .5, Math.round(w) - 1, Math.round(h) - 1); g.restore(); }
function dither(x, y, w, h, col, alpha) { x = Math.round(x); y = Math.round(y); w = Math.round(w); h = Math.round(h); const q = Math.max(2, Math.round(cs / 14)); g.globalAlpha = alpha; g.fillStyle = col; for (let j = 0; j * q < h; j++) for (let i = 0; i * q < w; i++) if ((i + j) % 2 === 0) g.fillRect(x + i * q, y + j * q, Math.min(q, w - i * q), Math.min(q, h - j * q)); g.globalAlpha = 1; }
function tri(pts, col) { g.fillStyle = col; g.beginPath(); g.moveTo(...pts[0]); g.lineTo(...pts[1]); g.lineTo(...pts[2]); g.closePath(); g.fill(); }
function spr(name, x, y, w, h, alpha) { const i = img[name]; if (!name || !i || !i.complete || !i.width) return false; g.globalAlpha = alpha ?? 1; g.imageSmoothingEnabled = false; const s = Math.min(w / i.width, h / i.height); const dw = i.width * s, dh = i.height * s; g.drawImage(i, Math.round(x + (w - dw) / 2), Math.round(y + (h - dh) / 2), Math.round(dw), Math.round(dh)); g.globalAlpha = 1; return true; }
const px = x => Math.round(RUL + panX + x * cs), py = y => Math.round(RUL + panY + y * cs);
function partyArrow(cx, cy, r, f, col, outline) { const [dx, dy] = [[0, -1], [1, 0], [0, 1], [-1, 0]][f & 3], qx = -dy, qy = dx; const P = (a, b) => [cx + dx * a * r + qx * b * r, cy + dy * a * r + qy * b * r]; const pts = [P(1, 0), P(-.8, .8), P(-.3, 0), P(-.8, -.8)]; g.fillStyle = col; g.beginPath(); g.moveTo(...pts[0]); pts.slice(1).forEach(p => g.lineTo(...p)); g.closePath(); g.fill(); if (outline) { g.strokeStyle = outline; g.lineWidth = 1; g.stroke(); } }
// three slanted strokes of four dots, inside the box with a margin, as AutoMapPaint_Claw
function claw(x, y, s, col) { x = Math.round(x); y = Math.round(y); s = Math.floor(s); const m = Math.max(1, Math.floor(s * 15 / 100)), inner = s - 2 * m, p = Math.max(1, Math.floor(inner * 2 / 13)), down = p + (p >> 1), across = p >> 1, h = 3 * down + 2 * p, gap = Math.max(0, (inner - 3 * across - p) >> 1), w = 2 * gap + 3 * across + p, l = x + ((s - w) >> 1), t = y + ((s - h) >> 1); g.fillStyle = col; for (let k = 0; k < 3; k++) for (let i = 0; i < 4; i++) g.fillRect(l + k * gap + i * across, t + i * down, p, p * 2); }
// The "who" tag: lower-left corner, one square per kind of trigger
function chip(x, y, who, border) {
  const s = Math.max(10, Math.round(cs * .36)), pad = 1; const pTxt = Math.max(1, Math.floor(s / 9));
  const w = who.reduce((n, it) => n + (it[0] === 'txt' ? textW(it[1], pTxt) + 4 : s), 0) + 2 * pad;
  const cx = Math.round(x + Math.max(2, cs * .06)), cy = Math.round(y + cs - s - 2 * pad - Math.max(2, cs * .06));
  rect(cx, cy, w, s + 2 * pad, '#0d0c0a'); frame(cx, cy, w, s + 2 * pad, border);
  let ox = cx + pad;
  for (const it of who) {
    if (it[0] === 'party') partyArrow(ox + s / 2, cy + pad + s / 2, s * .36, it[1], '#ff4d4d');
    if (it[0] === 'claw') claw(ox, cy + pad, s, '#d8cfbd');
    if (it[0] === 'box') frame(ox + s * .22, cy + pad + s * .22, s * .56, s * .56, '#d8cfbd', Math.max(1, Math.round(s / 10)));
    if (it[0] === 'icon') { if (!spr(it[1], ox + 1, cy + pad + 1, s - 2, s - 2)) keyhole(ox, cy + pad, s); }
    if (it[0] === 'txt') { text(g, ox + 2, cy + pad + (s - 5 * pTxt) / 2, it[1], '#ff8080', pTxt); ox += textW(it[1], pTxt) + 4; continue; }
    ox += s;
  }
}
function keyhole(x, y, s) { g.strokeStyle = '#fff'; g.lineWidth = 1; g.beginPath(); g.arc(x + s / 2, y + s * .4, s * .18, 0, 2 * Math.PI); g.stroke(); rect(x + s / 2 - 1, y + s * .45, 2, s * .3, '#fff'); }
function levelPlate(x, y, label, col, arrow) {
  const p = Math.max(1, Math.floor(cs / 18)); const aw = arrow ? 6 * p : 0; const w = aw + textW(label, p) + 4 * p, h = 9 * p;
  const X = Math.round(x + (cs - w) / 2), Y = Math.round(y + (cs - h) / 2); rect(X, Y, w, h, '#0d0c0a'); frame(X, Y, w, h, col);
  if (arrow) { const A = arrow === 'up' ? [[0,0,1,0,0],[0,1,1,1,0],[1,0,1,0,1],[0,0,1,0,0],[0,0,1,0,0]] : [[0,0,1,0,0],[0,0,1,0,0],[1,0,1,0,1],[0,1,1,1,0],[0,0,1,0,0]]; g.fillStyle = col; for (let r = 0; r < 5; r++) for (let k = 0; k < 5; k++) if (A[r][k]) g.fillRect(X + 2 * p + k * p, Y + 2 * p + r * p, p, p); }
  text(g, X + 2 * p + aw, Y + 2 * p, label, col, p);
}

// ============================================================================================
// Cells
// ============================================================================================
const COL = { wall: '#8d877a', wallEdge: '#a39c8d', floor: '#2e2b27', floorEdge: '#3f3b35', door: '#8b5a2b', doorSealed: '#8d877a', doorGone: '#5a3a1c', cyan: '#6ad8e8', link: '#ffcc33', feed: '#4a9eff', warn: '#ff6a2b', note: '#c77dff', item: '#f2d544', pad: '#c89a4a' };
function drawCellBase(c) {
  const x = px(c.x), y = py(c.y), k = c.k;
  if (k === 0) { rect(x, y, cs, cs, COL.wall); frame(x, y, cs, cs, COL.wallEdge); return; }
  if (k === 6) { rect(x, y, cs, cs, '#4a3856'); g.fillStyle = '#664e7a'; for (let i = 3; i < cs; i += 4) g.fillRect(x + i, y + 1, 1, cs - 2); frame(x, y, cs, cs, '#8a6ea0'); return; }
  rect(x, y, cs, cs, COL.floor); frame(x, y, cs, cs, COL.floorEdge);
  if (k === 4) drawDoor(c, x, y);
  if (k === 3) drawStairs(c, x, y);
  if (k === 7) dashed(x, y, cs, cs, '#8a6ea0', 4);
  const i = cs * .1;
  if (k === 5) { dither(x + i, y + i, cs - 2 * i, cs - 2 * i, COL.cyan, .55); frame(x + i, y + i, cs - 2 * i, cs - 2 * i, COL.cyan); }
  if (k === 8) { dither(x + i, y + i, cs - 2 * i, cs - 2 * i, '#8a8a9a', .22); dashed(x + i, y + i, cs - 2 * i, cs - 2 * i, '#8a8a9a'); }
  if (k === 9) dashed(x + i, y + i, cs - 2 * i, cs - 2 * i, '#5a5a66');
  if (k === 10 || k === 11) spinner(c, x, y, k === 10 ? COL.cyan : '#406e76');
  // floor picture: pits always, functional pictures always, dressing when shown
  if (k === 2 || (c.fp && (!c.flav || SHOW['FLOOR DECORATIONS']))) { if (!spr(c.fp, x + cs * .05, y + cs * .05, cs * .9, cs * .9)) { if (k === 2) { rect(x + cs * .15, y + cs * .15, cs * .7, cs * .7, '#000'); frame(x + cs * .15, y + cs * .15, cs * .7, cs * .7, '#8a4a4a'); } } }
  if (c.pit && ((k === 2 && c.pit[0]) || c.pit[2])) dashed(x, y, cs, cs, '#8a4a4a');
  if ((k === 5 || k === 8) && c.tele[6]) { const sc = c.tele[3]; const who = sc === 2 ? [['party', 0], ['box']] : sc === 1 ? [['claw']] : sc === 0 ? [['box']] : null; if (who) chip(x, y, who, '#3f7d88'); }
  if ((k === 5 || k === 8) && c.tele[7] && c.tele[0] !== lvl) levelPlate(x, y, 'L' + c.tele[0], COL.cyan);
}
function drawCellOverlay(c) {
  const x = px(c.x), y = py(c.y), k = c.k;
  if (vision === 2 && tint && c.hl) { g.fillStyle = 'rgba(76,208,106,.22)'; g.fillRect(x, y, cs, cs); }
  if (k === 9) { const d = Math.max(2, Math.round(cs / 12)); rect(x + cs / 2 - d / 2, y + cs / 2 - d / 2, d, d, '#5a5a66'); }
  if (k === 2 && c.pit && c.pit[0] && c.pit[1]) { const d = Math.max(2, Math.round(cs / 12)); rect(x + cs / 2 - d / 2, y + cs / 2 - d / 2, d, d, '#8a4a4a'); }
  if (k === 4) breakMarks(c, x, y);
  if (c.linked && (k === 4 || k === 0 || k === 6 || k === 7)) { const s = Math.max(4, Math.round(cs / 7)), bx = x + cs - s - 3, by = y + 3; rect(bx - 1, by - 1, s + 2, s + 2, '#0b0a08'); rect(bx, by, s, s, COL.link); }
}
function drawDoor(c, x, y) {
  const [state, ns, , , sealed] = c.door; const col = state === 5 ? COL.doorGone : sealed ? COL.doorSealed : COL.door;
  const gap = [.7, .5, .3, .15, 0, .7][state] ?? 0; const th = Math.floor(cs / 5), len = cs * (1 - gap) / 2;
  if (ns) { rect(x, y + (cs - th) / 2, len, th, col); rect(x + cs - len, y + (cs - th) / 2, len, th, col); }
  else { rect(x + (cs - th) / 2, y, th, len, col); rect(x + (cs - th) / 2, y + cs - len, th, len, col); }
}
function doorGhost(x, y, state, ns) {
  const gap = [.7, .5, .3, .15, 0, .7][state] ?? 0; const th = Math.floor(cs / 5), len = cs * (1 - gap) / 2;
  const R = ns ? [[x, y + (cs - th) / 2, len, th], [x + cs - len, y + (cs - th) / 2, len, th]] : [[x + (cs - th) / 2, y, th, len], [x + (cs - th) / 2, y + cs - len, th, len]];
  for (const r of R) { g.fillStyle = 'rgba(255,200,58,.55)'; g.fillRect(...r.map(Math.round)); frame(...r, COL.link); }
}
function breakMarks(c, x, y) {
  const [state, , axe, fire] = c.door; if (state === 5 || (!axe && !fire)) return;
  const bx = x + cs * .08, by = y + cs * .58, bw = cs * .3, bh = cs * .34;
  g.strokeStyle = '#fff'; g.lineWidth = 1;
  const X = (a, b, w, h) => { g.beginPath(); g.moveTo(a, b); g.lineTo(a + w, b + h); g.moveTo(a, b + h); g.lineTo(a + w, b); g.stroke(); };
  const O = (a, b, w, h) => { g.beginPath(); g.arc(a + w / 2, b + h / 2, Math.max(0, Math.min(w, h) / 2 - 1), 0, 2 * Math.PI); g.stroke(); };
  if (axe && fire) { X(bx, by, bw * .45, bh); O(bx + bw * .55, by, bw * .45, bh); } else if (axe) X(bx, by, bw, bh); else O(bx, by, bw, bh);
}
function drawStairs(c, x, y) {
  const up = !!c.st[0], ns = c.st[1], L = up ? ['#9a9488', '#5e594f'] : ['#6d685f', '#3a3731'];
  for (let i = 0; i < 4; i++) { const a = Math.round(cs * i / 4), b = Math.round(cs * (i + 1) / 4) - a; if (ns) rect(x, y + a, cs, b, L[i % 2]); else rect(x + a, y, b, cs, L[i % 2]); }
  levelPlate(x, y, String(lvl + (up ? -1 : 1)), up ? '#fff6dc' : '#c9c0ad', up ? 'up' : 'down');
}
// spinners: the map's own glyphs (AutoMapScene_SpinnerGeometry), thicker, with a dark rim
function spinGeom(facing, rot) { const PI = Math.PI, N = PI / 2; if (facing >= 0 && facing <= 3) return { glyph: 'abs', head: (1 - facing) * PI / 2 }; if (rot === 1) return { glyph: 'turn', start: N, sweep: -PI / 2, head: 0 }; if (rot === 2) return { glyph: 'turn', start: N, sweep: PI, head: N + PI }; if (rot === 3) return { glyph: 'turn', start: N, sweep: PI / 2, head: PI }; if (rot === 0) return { glyph: 'straight' }; return { glyph: 'unknown' }; }
function spinner(c, x, y, col) {
  const cx = x + cs / 2, cy = y + cs / 2, r = cs * .24, G = spinGeom(c.spin[0], c.spin[1]), lw = Math.max(2, Math.round(cs / 18));
  const P = a => [cx + r * Math.cos(a), cy - r * Math.sin(a)];
  const arc = (st, sw, colr, w) => { g.strokeStyle = colr; g.lineWidth = w; g.lineCap = 'square'; g.beginPath(); const n = Math.max(2, Math.ceil(24 * Math.abs(sw) / (2 * Math.PI))); for (let i = 0; i <= n; i++) { const p = P(st + sw * i / n); i ? g.lineTo(...p) : g.moveTo(...p); } g.stroke(); };
  const tang = (a, cw, colr, k) => { const sg = cw ? -1 : 1, ax = sg * -Math.sin(a), ay = sg * -Math.cos(a), ox = Math.cos(a), oy = -Math.sin(a); const [hx, hy] = P(a); tri([[hx + ax * r * .75 * k, hy + ay * r * .75 * k], [hx - ax * r * .2 * k + ox * r * .45 * k, hy - ay * r * .2 * k + oy * r * .45 * k], [hx - ax * r * .2 * k - ox * r * .45 * k, hy - ay * r * .2 * k - oy * r * .45 * k]], colr); };
  for (const [colr, w, k] of [['#0b0a08', lw + 2, 1.18], [col, lw, 1]]) {
    // a compass dial: ring, centre dot, and a head on the rim at the facing it sets
    if (G.glyph === 'abs') { arc(0, 2 * Math.PI, colr, w); const d = Math.max(2, cs / 12) + (k > 1 ? 2 : 0); rect(cx - d / 2, cy - d / 2, d, d, colr); const [hx, hy] = P(G.head), ox = Math.cos(G.head), oy = -Math.sin(G.head), qx = -oy, qy = ox, tx = hx + ox * r * .6, ty = hy + oy * r * .6, bx = tx - ox * r * .85 * k, by = ty - oy * r * .85 * k, h = r * .5 * k; tri([[tx + ox * (k - 1) * 4, ty + oy * (k - 1) * 4], [bx + qx * h, by + qy * h], [bx - qx * h, by - qy * h]], colr); }
    else if (G.glyph === 'turn') { arc(G.start, G.sweep, colr, w); tang(G.head, G.sweep < 0, colr, k); }
    else if (G.glyph === 'straight') { const gap = Math.PI / 3; arc(Math.PI / 2 + gap / 2, 2 * Math.PI - gap, colr, w); const d = Math.max(2, cs / 12) * (k > 1 ? 1.6 : 1); rect(cx - d / 2, cy - d / 2, d, d, colr); }
    else { arc(0, 2 * Math.PI, colr, w); tang(0, false, colr, k); }
  }
  g.lineCap = 'butt';
}

// ============================================================================================
// Walls: one slot per face, flush against it
// ============================================================================================
const sideHas = s => s && (s[1] || s[2] || s[4] || s[6]);
function slotRect(c, side, x, y) {
  const sides = [0, 1, 2, 3].filter(i => c.sides && sideHas(c.sides[i]));
  const adj = (a, b) => ((a - b + 4) % 4 === 1 || (b - a + 4) % 4 === 1);
  const crowded = sides.filter(s => s !== side && adj(s, side));
  let s = crowded.length ? .44 : .56, along = .5;
  if (crowded.length === 1) { const o = crowded[0]; if (side === 0 || side === 2) along = (o === 1) ? .04 + s / 2 : 1 - .04 - s / 2; else along = (o === 0) ? 1 - .04 - s / 2 : .04 + s / 2; }
  if (crowded.length === 2) { s = .34; along = .5; }
  const S = Math.round(s * cs), A = along * cs, inset = Math.max(2, Math.round(cs / 14));
  if (side === 0) return [Math.round(x + A - S / 2), y + inset, S, S];
  if (side === 2) return [Math.round(x + A - S / 2), y + cs - inset - S, S, S];
  if (side === 1) return [x + cs - inset - S, Math.round(y + A - S / 2), S, S];
  return [x + inset, Math.round(y + A - S / 2), S, S];
}
function faceLine(side, r, x, y, col) { const t = Math.max(2, Math.round(cs / 14)); if (side === 0) rect(r[0], y, r[2], t, col); if (side === 2) rect(r[0], y + cs - t, r[2], t, col); if (side === 1) rect(x + cs - t, r[1], t, r[3], col); if (side === 3) rect(x, r[1], t, r[3], col); }
function drawWallFaces(c) {
  if (!c.sides || (c.k !== 0 && c.k !== 6)) return; const x = px(c.x), y = py(c.y);
  for (let side = 0; side < 4; side++) {
    const s = c.sides[side]; if (!sideHas(s)) continue;
    const [pic, hasPic, writing, functional, hidden, isAlcove, portrait, , torchHolder] = s;
    const showPic = SHOW['WALL DECORATIONS'] && (hasPic || portrait), showText = SHOW['WALL WRITING'] && writing;
    const r = slotRect(c, side, x, y);
    if (hidden) { g.save(); g.setLineDash([3, 3]); g.strokeStyle = '#e8e2d2'; g.strokeRect(r[0] + .5, r[1] + .5, r[2] - 1, r[3] - 1); g.restore(); const cx = r[0] + r[2] / 2, cy = r[1] + r[3] / 2, rr = r[2] * .28; g.strokeStyle = '#ff8a2b'; g.beginPath(); g.moveTo(cx, cy - rr); g.lineTo(cx + rr, cy); g.lineTo(cx, cy + rr); g.lineTo(cx - rr, cy); g.closePath(); g.stroke(); faceLine(side, r, x, y, '#ff8a2b'); continue; }
    if (!showPic && !showText) { if (functional) faceLine(side, r, x, y, '#fff6dc'); continue; }
    if (showPic && hasPic && !spr(pic, r[0] + 1, r[1] + 1, r[2] - 2, r[3] - 2)) rect(r[0] + r[2] * .3, r[1] + r[3] * .3, r[2] * .4, r[3] * .4, '#ff8a2b');
    // the face sits inside its mirror frame, as DrawPortrait insets it
    if (showPic && portrait) { const m = Math.floor(r[2] * 28 / 100); spr(portrait, r[0] + m, r[1] + m, r[2] - 2 * m, r[3] - 2 * m); }
    if (showText) { const t = Math.max(1, Math.round(r[3] / 10)); for (let l = 0; l < 3; l++) rect(r[0] + r[2] * .25, r[1] + r[3] * (2 * l + 1) / 6, r[2] * .5, t, '#e8e2d2'); }
    faceLine(side, r, x, y, functional ? '#fff6dc' : '#3b3128');
    const b = Math.max(8, Math.round(r[2] * .5));
    // goods show on a switch, and below All in an alcove; a torch holder's torch is in its picture already
    const al = (c.alc || []).filter(a => a[0] === side && !torchHolder && kindShown(a[1][3]) && (functional || (isAlcove && spoil !== 0))), lk = (c.locks || []).filter(l => l[0] === side && l[1]);
    if (al.length) { rect(r[0] + r[2] - b - 1, r[1] + r[3] - b - 1, b + 2, b + 2, '#0c0b09'); spr(al[0][1][1] || al[0][1][0], r[0] + r[2] - b, r[1] + r[3] - b, b, b); }
    if (lk.length) { rect(r[0] - 1, r[1] + r[3] - b - 1, b + 2, b + 2, '#0c0b09'); frame(r[0] - 1, r[1] + r[3] - b - 1, b + 2, b + 2, '#e0b64a'); spr(lk[0][2][1] || lk[0][2][0], r[0], r[1] + r[3] - b, b, b); }
  }
}
// ============================================================================================
// Pads, generators, cogs
// ============================================================================================
function drawActuators(c) {
  const list = actsAt[c.x + ',' + c.y]; if (!list) return; const x = px(c.x), y = py(c.y); const seen = new Set();
  for (const a of list) {
    const [, , kind, op, facing, item, genPic] = a; const key = kind + ':' + op; if (seen.has(key)) continue; seen.add(key);
    if (kind === 2) { if (genPic) spr(genPic, x + cs * .1, y + cs * .05, cs * .8, cs * .8, .45); chip(x, y, [['txt', 'GEN']], '#c05050'); continue; }
    if (kind === 3) { const cx = x + cs / 2, cy = y + cs / 2, r = cs * .25; g.strokeStyle = '#8a5a5a'; g.lineWidth = 1; g.beginPath(); g.moveTo(cx, cy - r); g.lineTo(cx + r, cy); g.lineTo(cx, cy + r); g.lineTo(cx - r, cy); g.closePath(); g.stroke(); continue; }
    if (kind === 4) { const cx = x + cs / 2, cy = y + cs / 2, r = cs * .2; g.strokeStyle = '#9a9488'; g.lineWidth = 1; g.beginPath(); for (let i = 0; i <= 8; i++) { const an = 2 * Math.PI * i / 8; i ? g.lineTo(cx + r * Math.cos(an), cy + r * Math.sin(an)) : g.moveTo(cx + r, cy); } g.moveTo(cx - r, cy); g.lineTo(cx + r, cy); g.moveTo(cx, cy - r); g.lineTo(cx, cy + r); g.stroke(); continue; }
    if (!c.fp) { if (PLATE) spr(PLATE, x + cs * .08, y + cs * .18, cs * .84, cs * .64, kind === 1 ? .2 : .35); dashed(x + cs * .12, y + cs * .22, cs * .76, cs * .56, kind === 1 ? '#8a847a' : COL.pad, 2); }
    if (kind === 1) continue;
    const who = []; const icon = item[1] || item[0];
    if (op === 1) who.push(['party', 0], ['claw']);
    if (op === 2) who.push(['party', 0]);
    if (op === 3) who.push(['party', Math.max(0, facing)]);
    if (op === 5) who.push(['claw']);
    if (op === 4) who.push(['icon', icon]);
    if (op === 6) who.push(['party', 0], ['icon', icon]);
    if (who.length) chip(x, y, who, op === 3 ? '#e0b64a' : '#6b6255');
  }
}
// ============================================================================================
// Items, clouds, missiles, glimpses, monsters, party
// ============================================================================================
// The SHOW choice of each AutoMapItemKind, in the enum's order
const KIND_SHOW = ['SCROLLS', 'CHESTS', 'POTIONS', 'WEAPONS', 'ARMOUR', 'OTHER ITEMS', 'FOOD', 'KEYS'];
function kindShown(k) { return SHOW[KIND_SHOW[k] || 'OTHER ITEMS']; }
function drawItems(c) {
  if (!c.items) return; const items = c.items.filter(it => kindShown(it[3])); if (!items.length) return;
  const all = c.allKinds ? c.allKinds.filter(kindShown).length : c.all;
  const x = px(c.x), y = py(c.y); const n = Math.min(3, items.length);
  items.slice(0, 3).forEach((it, i) => { if (!spr(it[0], x + cs * i / n + cs * .1 / n, y + cs * .3, cs / n * .8, cs * .4)) rect(x + cs * i / n + cs * .3 / n, y + cs * .4, cs * .4 / n, cs * .2, COL.item); });
  if (all > 3) { const p = Math.max(1, Math.floor(cs / 24)), s = String(all), w = textW(s, p) + 4 * p, h = 9 * p; rect(x + cs - w - 2, y + 2, w, h, '#0d0c0a'); frame(x + cs - w - 2, y + 2, w, h, COL.item); text(g, x + cs - w - 2 + 2 * p, y + 2 + 2 * p, s, COL.item, p); }
}
function drawClouds() { for (const [x, y, s] of V.clouds) if (!spr(s, px(x), py(y), cs, cs)) { g.fillStyle = 'rgba(80,200,120,.45)'; g.fillRect(px(x) + cs * .05, py(y) + cs * .05, cs * .9, cs * .9); } }
function drawMissiles() { const n = {}; V.missiles.forEach(m => n[m[0] + ',' + m[1]] = (n[m[0] + ',' + m[1]] || 0) + 1); const k = {}; for (const [x, y, s, sc] of V.missiles) { const key = x + ',' + y, i = (k[key] = (k[key] || 0) + 1) - 1, cnt = n[key]; const w = cs / cnt; if (!spr(s, px(x) + w * i + w * (1 - sc) / 2, py(y) + cs * .6 * (1 - sc) / 2, w * sc, cs * .6 * sc)) rect(px(x) + w * i + w * .35, py(y) + cs * .2, w * .3, cs * .2, '#ff6a2b'); } }
function glimpseMark(pp, ax, ay, aw, ah) { const r = PUN['?']; const left = ax + (aw - 3 * pp) / 2, top = ay + (ah - 5 * pp) / 2; g.fillStyle = COL.item; for (let row = 0; row < 5; row++) for (let c = 0; c < 3; c++) if (r[row] & (1 << (2 - c))) g.fillRect(Math.round(left + c * pp), Math.round(top + row * pp), pp, pp); }
function drawGlimpses() {
  for (const [x, y] of V.glimpses) glimpseMark(Math.max(1, Math.round(cs / 9)), px(x), py(y), cs, cs);
  for (const [x, y, side] of V.gfaces) { let ax = px(x), ay = py(y), aw = cs, ah = cs; if (side === 0 || side === 2) ah = cs / 3; else aw = cs / 3; if (side === 2) ay += cs - ah; if (side === 1) ax += cs - aw; glimpseMark(Math.max(1, Math.round(cs / 16)), ax, ay, aw, ah); }
}
// Where a member stands on its tile, as AutoMapLayout_MemberArea: its corner (a quarter), its half, or the whole tile
function memberArea(pos, member, size, facing0, c) {
  let a = { x: 0, y: 0, w: c, h: c };
  if (pos === 255 || size < 0 || size > 1) return a;
  const corner = (pos >> (2 * member)) & 3, half = Math.floor(c / 2), rest = c - half, east = corner === 1 || corner === 2, south = corner === 2 || corner === 3, ns = (facing0 & 1) !== 0;
  if (size === 0 || !ns) { a.x = east ? half : 0; a.w = east ? rest : half; }
  if (size === 0 || ns) { a.y = south ? half : 0; a.h = south ? rest : half; }
  return a;
}
function figureFeet(a, c) { return [a.x + a.w * .5, (a.w === c && a.h === c) ? c * .72 : a.y + a.h * .94]; }
// A triangle filled in whole pixels, row by row, as the game's FillTriangle (no antialiasing, so a small mark stays white)
function pixelTri(p, col) {
  let [a, b, c] = [...p].sort((u, v) => u[1] - v[1]); g.fillStyle = col;
  const part = (f, t, v) => t === f ? 0 : Math.max(0, Math.min((v - f) / (t - f), 1)), lerp = (f, t, k) => f + (t - f) * k;
  for (let yy = Math.trunc(a[1]); yy <= Math.trunc(c[1]); yy++) {
    const [f, t] = yy > b[1] ? [b, c] : [a, b];
    const l = Math.trunc(lerp(a[0], c[0], part(a[1], c[1], yy))), r = Math.trunc(lerp(f[0], t[0], part(f[1], t[1], yy)));
    g.fillRect(Math.min(l, r), yy, Math.abs(r - l) + 1, 1);
  }
}
// The triangle grown away from its middle so that each side moves out by d, as GrowTriangle
function grownTri(p, d) {
  const mx = (p[0][0] + p[1][0] + p[2][0]) / 3, my = (p[0][1] + p[1][1] + p[2][1]) / 3;
  const area2 = Math.abs((p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) - (p[2][0] - p[0][0]) * (p[1][1] - p[0][1]));
  const per = p.reduce((s, q, i) => s + Math.hypot(p[(i + 1) % 3][0] - q[0], p[(i + 1) % 3][1] - q[1]), 0);
  if (area2 <= 0 || per <= 0) return p; const r = area2 / per, k = (r + d) / r;
  return p.map(([qx, qy]) => [mx + (qx - mx) * k, my + (qy - my) * k]);
}
// The facing mark sits against the side of the area the member faces, in a square of the area's shorter side but at least 14 px
// (never more than the cell, kept inside it); a 1-px black rim outside the white triangle keeps it apart from a pale figure
function facingMark(a, facing, x, y) {
  const side = Math.min(Math.max(Math.min(a.w, a.h), 14), cs); let sx = a.x + Math.trunc((a.w - side) / 2), sy = a.y + Math.trunc((a.h - side) / 2);
  if (facing === 0) sy = a.y; if (facing === 1) sx = a.x + a.w - side; if (facing === 2) sy = a.y + a.h - side; if (facing === 3) sx = a.x;
  sx = x + Math.max(0, Math.min(sx, cs - side)); sy = y + Math.max(0, Math.min(sy, cs - side));
  const turn = ([fx, fy]) => { fx -= .5; fy -= .5; const [tx, ty] = facing === 2 ? [-fy, fx] : facing === 3 ? [-fx, -fy] : facing === 0 ? [fy, -fx] : [fx, fy]; return [sx + (tx + .5) * side, sy + (ty + .5) * side]; };
  const pts = [[1, .5], [.82, .36], [.82, .64]].map(turn);
  g.save(); g.beginPath(); g.rect(x, y, cs, cs); g.clip(); pixelTri(grownTri(pts, 1), '#000'); pixelTri(pts, '#fff'); g.restore();
}
function drawMonsters() {
  if (!SHOW.MONSTERS) return;
  const ms = [...V.monsters].sort((a, b) => a.y - b.y);
  for (const m of ms) {
    const x = px(m.x), y = py(m.y), n = m.m.length, formed = m.size !== undefined; // old data has no size and pos: a line across the tile
    const areas = m.m.map((mb, i) => formed ? memberArea(m.pos === undefined ? 255 : m.pos, i, m.size, m.m[0][2], cs) : null);
    const feetOf = i => formed ? y + figureFeet(areas[i], cs)[1] : y + cs * .62;
    const order = m.m.map((_, i) => i).sort((a, b) => feetOf(a) - feetOf(b) || a - b); // the back row first
    for (const i of order) {
      const mb = m.m[i], im = img[mb[0]]; const cx = formed ? x + figureFeet(areas[i], cs)[0] : x + cs * (2 * i + 1) / (2 * n), feet = feetOf(i);
      if (!im || !im.width) { rect(cx - cs * .12, feet - cs * .25, cs * .24, cs * .24, '#c05050'); continue; }
      const sc = .5 * cs / 60, w = im.width * sc, h = im.height * sc;
      g.fillStyle = 'rgba(0,0,0,.45)'; g.fillRect(Math.round(cx - w * .38), Math.round(feet - 4), Math.round(w * .76), 4);
      g.save(); if (mb[1]) { g.translate(cx, 0); g.scale(-1, 1); g.translate(-cx, 0); } g.imageSmoothingEnabled = false; g.drawImage(im, Math.round(cx - w / 2), Math.round(feet - h), Math.round(w), Math.round(h)); g.restore();
    }
  }
  for (const m of ms) { // over the figures, so a tall one does not hide a mark
    const x = px(m.x), y = py(m.y);
    if (m.size !== undefined) m.m.forEach((mb, i) => facingMark(memberArea(m.pos === undefined ? 255 : m.pos, i, m.size, m.m[0][2], cs), mb[2] & 3, x, y));
    else m.m.forEach(mb => { const [dx, dy] = [[0, -1], [1, 0], [0, 1], [-1, 0]][mb[2] & 3]; const tx = x + cs / 2 + dx * cs / 2, ty = y + cs / 2 + dy * cs / 2, qx = -dy, qy = dx; tri([[tx, ty], [tx - dx * cs * .18 + qx * cs * .14, ty - dy * cs * .18 + qy * cs * .14], [tx - dx * cs * .18 - qx * cs * .14, ty - dy * cs * .18 - qy * cs * .14]], '#e9e2d2'); });
  }
}
function drawParty() { const p = party(); if (!p) return; partyArrow(px(p.x) + cs / 2, py(p.y) + cs / 2, cs * .38, p.f, '#ff4d4d', '#fff'); }
function drawNotes() {
  const N = noteOf();
  for (const c of V.cells) {
    const key = c.x + ',' + c.y; const pit = key in N.pits ? N.pits[key] : c.ppit; if (pit === undefined || pit < 0) continue;
    const x = px(c.x), y = py(c.y); dashed(x + cs * .06, y + cs * .06, cs * .88, cs * .88, COL.note); if (pit === 1) { const d = Math.max(2, Math.round(cs / 12)); rect(x + cs / 2 - d / 2, y + cs / 2 - d / 2, d, d, COL.note); }
  }
  for (const l of allPlayerLinks()) { const s = Math.max(3, Math.round(cs / 6)); rect(px(l[2]) + 2, py(l[3]) + cs - s - 2, s, s, COL.note); }
}
function allPlayerLinks() { return [...V.plinks, ...noteOf().links]; }

// ============================================================================================
// Lines: links, teleports, chains
// ============================================================================================
function faceExit(l) { const x = px(l[0]), y = py(l[1]); if (!l[4]) return [x + cs / 2, y + cs / 2]; const s = l[5]; return s === 0 ? [x + cs / 2, y] : s === 2 ? [x + cs / 2, y + cs] : s === 1 ? [x + cs, y + cs / 2] : [x, y + cs / 2]; }
function clipToCell(fx, fy, tx0, ty0) { const cx = px(tx0) + cs / 2, cy = py(ty0) + cs / 2, m = cs * .18; const dx = fx - cx, dy = fy - cy; const k = Math.min((cs / 2 - m) / Math.abs(dx || 1e-6), (cs / 2 - m) / Math.abs(dy || 1e-6)); return [cx + dx * Math.min(1, k), cy + dy * Math.min(1, k)]; }
function chunkyLine(x0, y0, x1, y1, col, w, dash) { const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0)); if (n < 1) return; const step = Math.max(1, Math.floor(w / 2)); for (const pass of [0, 1]) { g.fillStyle = pass ? col : '#0b0a08'; const b = pass ? w : w + 2; for (let i = 0; i <= n; i += step) { if (dash && ((i / (w * 4)) | 0) % 2) continue; const X = Math.round(x0 + (x1 - x0) * i / n), Y = Math.round(y0 + (y1 - y0) * i / n); g.fillRect(X - (b >> 1), Y - (b >> 1), b, b); } } }
function head(tx, ty, ang, size, col, mode) { // mode: 'fill' | 'hollow' | 'open'
  const ax = Math.cos(ang), ay = Math.sin(ang); const pts = [[tx, ty], [tx - ax * size - ay * size * .6, ty - ay * size + ax * size * .6], [tx - ax * size + ay * size * .6, ty - ay * size - ax * size * .6]];
  g.fillStyle = '#0b0a08'; g.beginPath(); g.moveTo(tx + ax * 2, ty + ay * 2); g.lineTo(pts[1][0] - ax * 1.5 - ay * 1.5, pts[1][1] - ay * 1.5 + ax * 1.5); g.lineTo(pts[2][0] - ax * 1.5 + ay * 1.5, pts[2][1] - ay * 1.5 - ax * 1.5); g.closePath(); g.fill();
  if (mode === 'fill') tri(pts, col);
  else { g.strokeStyle = col; g.lineWidth = 2; g.beginPath(); g.moveTo(...pts[1]); g.lineTo(...pts[0]); g.lineTo(...pts[2]); if (mode === 'hollow') g.closePath(); g.stroke(); }
}
// A toggle's head: a filled diamond, half as wide as long, on a rim (the game's AutoMapHeadRhomb)
function toggleHead(tx, ty, ang, size, col) {
  const ax = Math.cos(ang), ay = Math.sin(ang), qx = -ay, qy = ax, w = size * .5, P = (a, b) => [tx - ax * a + qx * b, ty - ay * a + qy * b];
  const ring = [P(-1.5, 0), P(size / 2, w + 1.5), P(size + 1.5, 0), P(size / 2, -w - 1.5)];
  g.fillStyle = '#0b0a08'; g.beginPath(); g.moveTo(...ring[0]); ring.slice(1).forEach(p => g.lineTo(...p)); g.closePath(); g.fill();
  const pts = [P(0, 0), P(size / 2, w), P(size, 0), P(size / 2, -w)];
  g.fillStyle = col; g.beginPath(); g.moveTo(...pts[0]); pts.slice(1).forEach(p => g.lineTo(...p)); g.closePath(); g.fill();
}
// lane: pixels off the axis, to the right of the line's direction (bundleLanes), so lines on
// one straight line never sit on one another; all of them still leave their source square
function drawEdge(l, col, learned, lane) {
  const w = Math.max(2, Math.round(cs / 12)), hs = Math.max(7, cs * .26);
  let [sx, sy] = faceExit(l);
  if (l[0] === l[2] && l[1] === l[3] && !l[4]) { const x = px(l[2]), y = py(l[3]); if (l[7]) dashed(x + cs * .25, y + cs * .25, cs * .5, cs * .5, col); else frame(x + cs * .25, y + cs * .25, cs * .5, cs * .5, col, 2); return; }
  let [tx, ty] = clipToCell(sx, sy, l[2], l[3]); const ang = Math.atan2(ty - sy, tx - sx);
  if (lane) { const nx = -Math.sin(ang) * lane, ny = Math.cos(ang) * lane; sx += nx; sy += ny; tx += nx; ty += ny; }
  chunkyLine(sx, sy, tx - Math.cos(ang) * hs * .8, ty - Math.sin(ang) * hs * .8, col, w, !!l[7]);
  const sig = l[6];
  if (learned) { head(tx, ty, ang, hs, col, 'open'); return; }
  if (sig === 2) toggleHead(tx, ty, ang, hs, col); else head(tx, ty, ang, hs, col, sig === 1 ? 'hollow' : 'fill');
  if (sig === 3) { const mx = (sx + tx) / 2, my = (sy + ty) / 2, q = Math.max(4, cs * .14); chunkyLine(mx - Math.sin(ang) * q, my + Math.cos(ang) * q, mx + Math.sin(ang) * q, my - Math.cos(ang) * q, col, w, false); }
}
// One line width: two lanes sit this far either side of the axis, a line apart, both inside the source square
const laneOffset = () => Math.max(2, Math.round(cs / 12));
// Where a line runs, in cells: from the source's centre (a switch: its face) to the target's centre
function linePoints(l) {
  let sx = l[0] + .5, sy = l[1] + .5;
  if (l[4]) { const s = l[5] & 3; if (s === 0) sy -= .5; else if (s === 2) sy += .5; else if (s === 1) sx += .5; else sx -= .5; }
  return [sx, sy, l[2] + .5, l[3] + .5];
}
// Two lines on one straight line that share a stretch (not just a point)
function sharesStretch(a, b) {
  const [ax, ay, bx, by] = linePoints(a), [cx, cy, dx2, dy2] = linePoints(b);
  const dx = bx - ax, dy = by - ay, len = dx * dx + dy * dy; if (!len) return false;
  const off = (x, y) => (x - ax) * dy - (y - ay) * dx; if (Math.abs(off(cx, cy)) > 1e-9 || Math.abs(off(dx2, dy2)) > 1e-9) return false;
  const t = (x, y) => ((x - ax) * dx + (y - ay) * dy) / len; const t1 = t(cx, cy), t2 = t(dx2, dy2);
  return Math.min(1, Math.max(t1, t2)) - Math.max(0, Math.min(t1, t2)) > 1e-6;
}
// Lines that share a stretch form a bundle and spread across lanes around the axis: two lines a
// line width either side, three or more 1.5 widths apart (the outer ones at the square's edge).
// Gold before blue, then the shorter first, so every bundle sorts alike whichever way it runs.
// Returns a Map line -> lane for drawEdge (pixels to the right of the line's own direction).
function bundleLanes(lines) {
  const n = lines.length, root = [...Array(n).keys()], find = i => { while (root[i] !== i) i = root[i]; return i; };
  for (let i = 0; i < n; i++) for (let j = i + 1; j < n; j++) if (sharesStretch(lines[i].l, lines[j].l)) root[find(i)] = find(j);
  const groups = {}; for (let i = 0; i < n; i++) (groups[find(i)] = groups[find(i)] || []).push(lines[i]);
  const lanes = new Map(), w = laneOffset();
  for (const g of Object.values(groups)) {
    if (g.length < 2) continue;
    const length = e => { const [a, b, c, d] = linePoints(e.l); return Math.hypot(c - a, d - b); };
    g.sort((p, q) => (p.feed - q.feed) || (length(p) - length(q)));
    const step = g.length === 2 ? 2 * w : 1.5 * w;
    // One normal for the whole bundle: of the axis pointing right (or down when upright)
    g.forEach((e, i) => {
      const [a, b, c, d] = linePoints(e.l), along = Math.abs(c - a) > 1e-9 ? c - a : d - b;
      lanes.set(e.l, (i - (g.length - 1) / 2) * step * (along > 0 ? 1 : -1));
    });
  }
  return lanes;
}
function sourceNode(l, col) { const w = Math.max(2, Math.round(cs / 12)); const [x, y] = faceExit(l); rect(x - w * 1.5 - 1, y - w * 1.5 - 1, w * 3 + 2, w * 3 + 2, '#0b0a08'); rect(x - w * 1.5, y - w * 1.5, w * 3, w * 3, col); }
function spotlight(cells) { g.save(); g.beginPath(); g.rect(RUL, RUL, cv.width - 2 * RUL, cv.height - 2 * RUL); cells.forEach(k => { const [a, b] = k.split(',').map(Number); g.rect(px(a) + cs, py(b), -cs, cs); }); g.fillStyle = 'rgba(8,7,5,.62)'; g.fill('evenodd'); g.restore(); }
function goalBadge(gl) {
  const [x0, y0, badge, good] = gl; const col = good ? COL.link : COL.warn; const x = px(x0), y = py(y0), t = Math.max(2, Math.round(cs / 14));
  // The badge is a tab on the frame's top edge, so the centre stays free for the source square
  frame(x + 2, y + 2, cs - 4, cs - 4, col, t); const p = 2, w = textW(badge, p) + 6, h = 5 * p + 6; const X = Math.round(x + (cs - w) / 2), Y = Math.round(y + 2 + t / 2 - h / 2); rect(X, Y, w, h, '#0b0a08'); frame(X, Y, w, h, col); text(g, X + 3, Y + 3, badge, col, p);
}
const teleCell = c => c && (c.k === 5 || c.k === 8) && c.tele[7] && c.tele[6];
function hoverLines() { // what hovering shows: {gold, blue, violet, cyan, dormant, goals, ghosts}
  const out = { gold: [], feed: [], violet: [], cyan: [], dormant: [], goals: [], ghosts: [], learned: spoil !== 0 };
  if (!hover) return out; const key = hover.x + ',' + hover.y; const h = V.hover[key] || {};
  if (spoil === 0 && h.chain && (h.chain.length || h.relay)) { out.gold = h.chain.length ? h.chain : (h.from || []); out.feed = h.to || []; out.goals = h.goals || []; out.ghosts = h.ghost || []; }
  else { out.gold = h.from || []; out.feed = h.to || []; if (!out.gold.length) { out.gold = out.feed; out.feed = []; } if (spoil === 0) out.ghosts = h.ghost || []; }
  // A cell nothing leaves keeps its incoming links gold, as before; blue is "into the hovered cell" only beside gold
  const pl = allPlayerLinks(); const fromHere = pl.filter(l => l[0] === hover.x && l[1] === hover.y); out.violet = fromHere.length ? fromHere : pl.filter(l => l[2] === hover.x && l[3] === hover.y);
  const c = C(hover.x, hover.y);
  if (teleCell(c) && c.tele[0] === lvl && !(c.tele[1] === c.x && c.tele[2] === c.y)) out.cyan.push([c.x, c.y, c.tele[1], c.tele[2]]);
  else if (c && c.k !== 9) V.cells.forEach(o => { if (teleCell(o) && o.tele[0] === lvl && o.tele[1] === hover.x && o.tele[2] === hover.y && o !== c) out.cyan.push([o.x, o.y, hover.x, hover.y]); });
  if (c && c.k === 9 && c.tele[6] && c.tele[7] && c.tele[0] === lvl) out.dormant.push([c.x, c.y, c.tele[1], c.tele[2]]);
  return out;
}
function drawHover() {
  const L = hoverLines(); const all = [...L.gold, ...L.feed, ...L.violet, ...L.cyan, ...L.dormant];
  if (!all.length && !L.goals.length) return L; L.lit = true;
  const keep = new Set(); all.forEach(l => { keep.add(l[0] + ',' + l[1]); keep.add(l[2] + ',' + l[3]); }); L.goals.forEach(gl => keep.add(gl[0] + ',' + gl[1])); keep.add(hover.x + ',' + hover.y);
  spotlight([...keep]);
  const w = Math.max(2, Math.round(cs / 12)), hs = Math.max(7, cs * .26);
  for (const [sx0, sy0, dx0, dy0] of L.cyan) { const sx = px(sx0) + cs / 2, sy = py(sy0) + cs / 2; const [tx, ty] = clipToCell(sx, sy, dx0, dy0); const a = Math.atan2(ty - sy, tx - sx); chunkyLine(sx, sy, tx - Math.cos(a) * hs * .8, ty - Math.sin(a) * hs * .8, COL.cyan, w, false); head(tx, ty, a, hs, COL.cyan, 'fill'); frame(px(dx0) + 2, py(dy0) + 2, cs - 4, cs - 4, COL.cyan, Math.max(2, Math.round(cs / 14))); }
  for (const [sx0, sy0, dx0, dy0] of L.dormant) { const sx = px(sx0) + cs / 2, sy = py(sy0) + cs / 2; const [tx, ty] = clipToCell(sx, sy, dx0, dy0); const a = Math.atan2(ty - sy, tx - sx); chunkyLine(sx, sy, tx - Math.cos(a) * hs * .8, ty - Math.sin(a) * hs * .8, '#5a8a94', w, true); head(tx, ty, a, hs, '#5a8a94', 'hollow'); dashed(px(dx0) + 2, py(dy0) + 2, cs - 4, cs - 4, '#5a8a94'); }
  // A link drawn in gold is not drawn again in blue (a relay that feeds itself is in both lists)
  const gold = dedupe(L.gold), same = (a, b) => a.slice(0, 6).join() === b.slice(0, 6).join();
  const feed = dedupe(L.feed).filter(l => !gold.some(g => same(g, l)));
  const lanes = bundleLanes([...gold.map(l => ({ l, feed: 0 })), ...feed.map(l => ({ l, feed: 1 }))]), lane = l => lanes.get(l) || 0;
  for (const l of feed) drawEdge(l, COL.feed, L.learned, lane(l));
  for (const l of gold) drawEdge(l, COL.link, L.learned, lane(l));
  for (const l of L.violet) drawEdge(l, COL.note, true);
  for (const gh of L.ghosts) doorGhost(px(gh[0]), py(gh[1]), gh[2], gh[3]);
  for (const gl of L.goals) goalBadge(gl);
  // Every source square sits at its cell's centre (the badges are tabs on the frame's top edge)
  const fed = new Set(); for (const l of feed) { const k = l.slice(0, 2).join() + ',' + l[4] + ',' + l[5]; if (!fed.has(k)) { fed.add(k); sourceNode(l, COL.feed); } }
  // As the game: every source gets its square, the hovered one and those of incoming gold lines
  const sent = new Set(); for (const l of gold) { const k = l.slice(0, 2).join() + ',' + l[4] + ',' + l[5]; if (!sent.has(k)) { sent.add(k); sourceNode(l, COL.link); } }
  return L;
}
function dedupe(ls) { const s = new Set(); return ls.filter(l => { const k = l.slice(0, 4).join(); if (s.has(k)) return false; s.add(k); return true; }); }

// ============================================================================================
// Tooltip: the game's own lines, presented the new way
// ============================================================================================
// Lines are {t, s, i}: text with glyph codes (\x01\x01 party, \x02\x02 claw, \x03\x03 box, \x04<n> item n of i,
// \x05<facing> gold turned arrow) and a style (the game's AutoMapTipStyle); older data and the page's own lines are plain strings
const TOK = /(\{party\}|\{claw\}|\{box\}|\x01\x01|\x02\x02|\x03\x03|\x04[0-9]|\x05[0-3])/;
const TIPCOL = ['#fff', '#e0b64a', '#a8b8bc', '#a8b8bc', '#a8b8bc', '#6ad8e8', '#ffcc33', '#8a8a9a', '#ffc83a', '#7fb4ff'];
const isGlyph = t => t[0] === '{' || t.charCodeAt(0) < 6;
function richParts(s) { return s.split(TOK).filter(t => t !== ''); }
function richW(s, p) { return richParts(s).reduce((n, t) => n + (isGlyph(t) ? 8 * p + 2 : textW(t, p) + p), 0); }
function richText(x, y, s, col, p, items) {
  for (const t of richParts(s)) {
    if (isGlyph(t)) {
      const sz = 7 * p, k = t[0] === '{' ? { '{party}': 1, '{claw}': 2, '{box}': 3 }[t] : t.charCodeAt(0), arg = t.length > 1 && k >= 4 ? t.charCodeAt(1) - 48 : 0;
      if (k === 4) { const it = items && items[arg]; if (it) spr(it[1] || it[0], x, y - p, sz, sz, 1); }
      else {
        rect(x, y - p, sz, sz, '#0d0c0a'); frame(x, y - p, sz, sz, k === 5 ? '#e0b64a' : '#6b6255');
        if (k === 1) partyArrow(x + sz / 2, y - p + sz / 2, sz * .36, 0, '#ff4d4d');
        if (k === 5) partyArrow(x + sz / 2, y - p + sz / 2, sz * .36, arg, '#e0b64a');
        if (k === 2) claw(x, y - p, sz, '#d8cfbd');
        if (k === 3) frame(x + sz * .25, y - p + sz * .25, sz * .5, sz * .5, '#d8cfbd', 1);
      }
      x += 8 * p + 2;
    } else { text(g, x, y, t, col, p); x += textW(t, p) + p; }
  }
}
const TAKES = { 'PARTY+OBJECTS': '{party} PARTY {box} OBJECTS', 'OBJECTS': '{box} OBJECTS', 'MONSTERS': '{claw} MONSTERS', 'ANYTHING': 'ANYTHING' };
function presentLine(s) {
  let m;
  if ((m = s.match(/^(TAKES|WOULD TAKE): (.*)$/)) && TAKES[m[2]]) return m[1] + ': ' + TAKES[m[2]];
  if (s.startsWith('DEST: ')) return 'SENDS: ' + s.slice(6);
  if ((m = s.match(/^(\d+)x (.*)$/))) return m[2] + ' X' + m[1];
  return s;
}
function tooltipLines() {
  const h = V.hover[hover.x + ',' + hover.y]; const lines = [];
  const raw = (h && h.tip ? ((shiftHeld && h.tipFull) ? h.tipFull : h.tip) : []).map(l => typeof l === 'string' ? { t: l } : l);
  // The game's tooltip names the group right after POS (gold); older exports lacked it, so the page adds it then
  const m = monAt[hover.x + ',' + hover.y];
  const nameLine = m && m.name ? nice(m.name) + (m.m.length > 1 ? ' X' + m.m.length : '') : '';
  const hasName = nameLine && raw.length > 1 && raw[1].t.toUpperCase() === (m.name.toUpperCase() + (m.m.length > 1 ? ' X' + m.m.length : ''));
  raw.forEach((s, i) => {
    if (s.s === 8 && !SHOW.MONSTERS) return;
    if (i === 1 && hasName) { if (SHOW.MONSTERS) lines.push({ t: nameLine, s: 8 }); return; }
    lines.push(s.s === undefined ? { t: presentLine(s.t) } : s);
    if (i === 0 && !hasName && nameLine && SHOW.MONSTERS) lines.push({ t: nameLine, s: 8 });
  });
  insertActedOnBy(lines, h);
  const key = hover.x + ',' + hover.y, N = noteOf();
  if (key in N.pits) lines.push({ t: N.pits[key] ? 'YOUR MARK: HIDDEN PIT OPEN' : 'YOUR MARK: HIDDEN PIT CLOSED' });
  N.links.filter(l => l[0] === hover.x && l[1] === hover.y).forEach(l => lines.push({ t: `YOUR LINK TO (${l[2] + OX}, ${l[3] + OY})` }));
  return { lines, icons: h && h.icons ? h.icons : [] };
}
// What acts on the hovered cell, under the header of the part it acts on (PIT, DOOR, ...): one
// line per verb, the sources of a kind named once ("PADS L4 (21, 8), (20, 9)"). Relays have their FED BY.
const TARGET_HEAD = /^(PIT|HIDDEN PIT|DOOR|FALSE WALL|TELEPORTER|SPINNER|GENERATOR|LAUNCHER)/;
const OPENABLE = /^(PIT|HIDDEN PIT|DOOR|FALSE WALL)/;
function feederName(l) {
  if (l[4]) return 'SWITCH (' + 'NESW'[l[5] & 3] + ')';
  const t = (V.hover[l[0] + ',' + l[1]] || {}).tip || [];
  const relay = t.find(x => x && x.s === 1 && /^(GATE|COUNTER)/.test(x.t));
  return relay ? relay.t : 'PAD';
}
function actedOnBy(h, head) {
  const verbs = OPENABLE.test(head) ? ['OPENED', 'CLOSED', 'TOGGLED', 'HELD'] : ['SET', 'CLEARED', 'TOGGLED', 'HELD'];
  const by = {}; // verb -> name -> [loc]
  for (const l of dedupe(h.to || [])) {
    const v = verbs[l[6]] || 'SET', n = feederName(l), loc = `(${l[0] + OX}, ${l[1] + OY})`;
    const m = by[v] || (by[v] = {}); const a = m[n] || (m[n] = []); if (!a.includes(loc)) a.push(loc);
  }
  return verbs.filter(v => by[v]).map(v => '  ' + v + ' BY: ' + Object.entries(by[v]).map(([n, a]) =>
    (a.length > 1 ? n.replace(/^(PAD|SWITCH|GATE|COUNTER)/, '$1' + (n.startsWith('SWITCH') ? 'ES' : 'S')) : n) + ` L${lvl} ` + a.join(', ')).join(', '));
}
function insertActedOnBy(lines, h) {
  // Data exported after the game learnt these lines (style 9) has them already
  if (spoil !== 0 || !h || h.relay || !(h.to && h.to.length) || !(h.from && h.from.length) || lines.some(l => l.s === 9)) return;
  let at = lines.findIndex(l => l.s === 1 && TARGET_HEAD.test(l.t)); if (at < 0) return;
  const head = lines[at].t; at++; while (at < lines.length && lines[at].s !== 1 && /^ /.test(lines[at].t)) at++;
  lines.splice(at, 0, ...actedOnBy(h, head).map(t => ({ t, s: 9 })));
}
// Where the tooltip goes: the place that hides the least of what the hover lights up. Tried:
// beside the hovered cell and beside the whole lit group (left or right), each above, below or
// level with the hovered row or above or below the lit group; a place that does not fit is
// pushed into the map. Any overlap is a hit: the cells lines start or end in and the goals (with
// the tab above their frame) weigh 4, cells a line runs through 1, plus how much of them is
// covered; the hovered cell can never be covered. Of equal places the nearest to the hovered
// cell wins, then the old choice (away from where the lines lead).
function tooltipPlace(L, w, h) {
  const lines = [...L.gold, ...L.feed, ...L.cyan, ...L.dormant, ...L.violet], weight = new Map();
  const add = (x, y, v) => { const k = x + ',' + y; weight.set(k, Math.max(weight.get(k) || 0, v)); };
  for (const l of lines) {
    const n = Math.max(1, 2 * Math.max(Math.abs(l[2] - l[0]), Math.abs(l[3] - l[1])));
    for (let i = 1; i < n; i++) add(Math.round(l[0] + (l[2] - l[0]) * i / n), Math.round(l[1] + (l[3] - l[1]) * i / n), 1);
  }
  for (const l of lines) { add(l[0], l[1], 4); add(l[2], l[3], 4); }
  for (const gl of L.goals) add(gl[0], gl[1], 4);
  add(hover.x, hover.y, 1000);
  let lx = 0, ly = 0; [...L.gold, ...L.cyan, ...L.feed].forEach(l => { lx += l[2] + l[0] - 2 * hover.x; ly += l[3] + l[1] - 2 * hover.y; });
  const prefer = (lx > 0 ? 1 : 0) | (ly > 0 ? 2 : 0);
  const gap = 4, gapY = gap + (5 * 2 + 6) / 2; // above or below, the gap clears the goal tabs
  let minX = hover.x, maxX = hover.x, minY = hover.y, maxY = hover.y;
  for (const k of weight.keys()) { const [a, b] = k.split(',').map(Number); minX = Math.min(minX, a); maxX = Math.max(maxX, a); minY = Math.min(minY, b); maxY = Math.max(maxY, b); }
  const left = [px(hover.x) - w - gap, px(minX) - w - gap], right = [px(hover.x) + cs + gap, px(maxX) + cs + gap];
  const up = [py(hover.y) - h - gapY, py(minY) - h - gapY], down = [py(hover.y) + cs + gapY, py(maxY) + cs + gapY], level = [py(hover.y), py(hover.y) + cs - h];
  const tab = 5 * 2 + 6, cx0 = px(hover.x) + cs / 2, cy0 = py(hover.y) + cs / 2;
  let best = null;
  for (const side of [prefer, prefer ^ 2, prefer ^ 1, prefer ^ 3]) {
    for (const x0 of side & 1 ? left : right) for (const y0 of [...(side & 2 ? up : down), ...level]) {
      const x = Math.round(Math.max(RUL, Math.min(x0, cv.width - RUL - w))), y = Math.round(Math.max(RUL, Math.min(y0, cv.height - RUL - h)));
      let score = 0;
      for (const [k, v] of weight) {
        const [cx, cy] = k.split(',').map(Number), X = px(cx), Y = py(cy) - (v >= 4 ? tab / 2 : 0), H = py(cy) + cs - Y;
        const ox = Math.min(X + cs, x + w) - Math.max(X, x), oy = Math.min(Y + H, y + h) - Math.max(Y, y);
        if (ox > 0 && oy > 0) score += v * (1 + (ox * oy) / (cs * H));
      }
      // How far the box is from the hovered cell's centre, as a tie-breaker far below one hit
      const dx = Math.max(x - cx0, 0, cx0 - (x + w)), dy = Math.max(y - cy0, 0, cy0 - (y + h));
      score += Math.hypot(dx, dy) / 1e6;
      if (!best || score < best.score - 1e-9) best = { x, y, score };
    }
  }
  return [best.x, best.y];
}
function drawTooltip(L) {
  if (!hover || menu || drag) return; const { lines, icons } = tooltipLines(); if (!lines.length) return;
  const p = 2, lh = 8 * p;
  // icon grid: one tile per stack; nested contents follow their chest or monster, smaller; at most two rows of six
  const T = 36, PER = 6, MAXT = 12; const tiles = icons.length > MAXT ? icons.slice(0, MAXT - 1) : icons; const more = icons.length - tiles.length;
  const nt = tiles.length + (more ? 1 : 0), cols = Math.min(PER, nt), rowsT = Math.ceil(nt / PER), gridW = cols * (T + 4), gridH = nt ? rowsT * (T + 4) + 4 : 0;
  const maxLines = Math.max(4, Math.floor((cv.height - 2 * RUL - gridH - 40) / lh)); const scrollMax = Math.max(0, lines.length - maxLines); tipScroll = Math.min(tipScroll, scrollMax);
  const shown = lines.slice(tipScroll, tipScroll + maxLines); if (tipScroll > 0) shown[0] = { t: `-${tipScroll} MORE`, s: 7 }; if (tipScroll + maxLines < lines.length) shown[shown.length - 1] = { t: `+${lines.length - tipScroll - maxLines + 1} MORE, SHIFT+WHEEL`, s: 7 };
  const w = Math.max(gridW + 4, ...shown.map(l => richW(l.t, p))) + 10, h = gridH + shown.length * lh + 8;
  const [x, y] = tooltipPlace(L, w, h);
  rect(x, y, w, h, '#111'); frame(x, y, w, h, '#ffc83a');
  for (let i = 0; i < nt; i++) {
    const tx = x + 6 + (i % PER) * (T + 4), ty = y + 6 + Math.floor(i / PER) * (T + 4);
    if (i >= tiles.length) { rect(tx, ty, T, T, '#3a3733'); frame(tx, ty, T, T, '#5a554c'); const s = '+' + more; text(g, tx + (T - textW(s, 2)) / 2, ty + (T - 10) / 2, s, COL.item, 2); continue; }
    const [item, count, nested, predicted, required] = tiles[i];
    const inset = nested ? 6 : 0; rect(tx + inset, ty + inset, T - 2 * inset, T - 2 * inset, predicted ? '#24221f' : '#3a3733');
    frame(tx + inset, ty + inset, T - 2 * inset, T - 2 * inset, required ? '#e0b64a' : nested && !predicted ? COL.item : '#5a554c');
    spr(item[1] || item[0], tx + inset + 2, ty + inset + 2, T - 2 * inset - 4, T - 2 * inset - 4, predicted ? .5 : 1);
    if (count > 1) { const s = String(count), bw = textW(s, 2) + 4; rect(tx + T - bw - 1, ty + T - 15, bw, 14, '#0d0c0a'); text(g, tx + T - bw + 1, ty + T - 13, s, COL.item, 2); }
  }
  shown.forEach((l, i) => { const muted = l.s === undefined && /^(WOULD|DORMANT)/.test(l.t); richText(x + 5, y + gridH + 6 + i * lh, l.t, l.s === undefined ? (muted ? '#a8b8bc' : '#fff') : TIPCOL[l.s] || '#fff', p, l.i); });
}

// ============================================================================================
// Rulers
// ============================================================================================
function rulers() {
  const p = 2; rect(0, 0, cv.width, RUL, '#24221f'); rect(0, cv.height - RUL, cv.width, RUL, '#24221f'); rect(0, 0, RUL, cv.height, '#24221f'); rect(cv.width - RUL, 0, RUL, cv.height, '#24221f');
  const every = Math.max(1, Math.ceil((textW('00', p) + 6) / cs)), everyY = Math.max(1, Math.ceil(16 / cs));
  for (let x = 0; x < W; x++) { const X = px(x) + cs / 2; if (X < RUL || X > cv.width - RUL) continue; const hot = hover && hover.x === x; if ((x + OX) % every && !hot) continue; const s = String(x + OX);
    if (hot) { rect(X - textW(s, p) / 2 - 2, 3, textW(s, p) + 4, 13, '#ffcc33'); text(g, X - textW(s, p) / 2, 4, s, '#1a160f', p); } else text(g, X - textW(s, p) / 2, 4, s, '#b8b0a0', p); text(g, X - textW(s, p) / 2, cv.height - 14, s, hot ? '#ffcc33' : '#b8b0a0', p); }
  for (let y = 0; y < H; y++) { const Y = py(y) + cs / 2; if (Y < RUL || Y > cv.height - RUL) continue; const hot = hover && hover.y === y; if ((y + OY) % everyY && !hot) continue; const s = String(y + OY);
    if (hot) { rect(1, Y - 7, RUL - 2, 13, '#ffcc33'); text(g, RUL / 2 - textW(s, p) / 2, Y - 5, s, '#1a160f', p); } else text(g, RUL / 2 - textW(s, p) / 2, Y - 5, s, '#b8b0a0', p); text(g, cv.width - RUL / 2 - textW(s, p) / 2, Y - 5, s, hot ? '#ffcc33' : '#b8b0a0', p); }
}
