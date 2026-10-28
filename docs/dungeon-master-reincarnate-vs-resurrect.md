# Dungeon Master: Reincarnate vs Resurrect

> Expected-value comparison for Dungeon Master 1 (FTL, 1987).
> Sources: DM Encyclopaedia “Skills and Statistics” (ReDMCSB), forum level-up table from source code.

## 1. Core difference

- **Resurrect:** champion returns exactly as in the mirror. Name, all 4 basic XP + 16 hidden XP, Health/Stamina/Mana, Strength/Dexterity/Wisdom/Vitality/Anti-Magic/Anti-Fire kept.
- **Reincarnate (DM1):** all XP reset to 0, you rename. Health/Stamina/Mana kept, other 6 stats slightly increased (“skills converted to greater physical attributes”). Better long-term, harder start.
- **Reincarnate (CSB):** different! Health/Stamina/Mana divided by 2 (Atari ST) or 4 (Amiga/X68000/PC-98/FM-Towns). Other stats slightly up/down. Much harder start. This note is DM1-only.

Short term: resurrected is stronger (starts with Novice/Apprentice/Journeyman).
Long term: reincarnated catches up in levels in minutes/hours and keeps extra stat rolls forever.

## 2. Why levels converge: exponential XP

Game uses doubling, no table:

```
1 base: 0
2 Neophyte: 500
3 Novice: 1000
4 Apprentice: 2000
5 Journeyman: 4000
6 Craftsman: 8000
7 Artisan: 16000
8 Adept: 32000
9 Expert: 64000
10 Lo-Master: 128000
11 Um-Master: 256000
12 On-Master: 512000
13 Ee-Master: 1024000
14 Pal-Master: 2048000
15 Mon-Master: 4096000
16 Archmaster: 8192000
```

Highest DM1 starter is Journeyman = 4000 XP. End-game is hundreds of thousands per class. 4000 is noise. Reincarnated re-earns levels 2..5 for ~500-4000 XP each while resurrected needs 32k-64k for the next level.

Basic skill = Fighter/Ninja/Priest/Wizard shown. Each = sum of 4 hidden skills (Swing/Thrust/Club/Parry, Steal/Fight/Throw/Shoot, Identify/Heal/Influence/Defend, Fire/Air/Earth/Water). XP in hidden also added to basic.

## 3. What you gain per level-up

From ReDMCSB source + 200+ empirical level-ups:

### 3.1 Health: random X to 1.5*X, avg 1.25*X

- Fighter: `X = new_level * 3`
- Ninja: `X = new_level * 2`
- Priest: `X = new_level * 1.5`
- Wizard: `X = new_level`

Example Fighter 2..8: `6-9, 9-13, 12-18, 15-22, 18-27, 21-31, 24-36`.

### 3.2 Stamina: random X to 1.5*X, avg 1.25*X, compounded

- Fighter: `X = MaxStamina / 16` (~+7.8%/level)
- Ninja: `X = MaxStamina / 21` (~+6.0%)
- Priest: `X = MaxStamina / 25` (~+5.0%)
- Wizard: `X = MaxStamina / 32` (~+3.9%)

Internal value /10 for display. Max 9999.

### 3.3 Mana (Priest/Wizard only)

- Priest: `new_level + min(rand 0-3, base_level-1)`
- Wizard: `new_level*1.5 + min(rand 0-3, base_level-1)`

Avg bonus: level 2: +0.75, level 3: +1.25, level 4+: +1.5.

### 3.4 Other six (max 170, 255 in ST 1.0)

- Fighter: `Str 1-2, Dex 0-1, Vit 0-1 on odd, AF 0-1 on even`
- Ninja: `Dex 1-2, Str 0-1, Vit 0-1 on odd, AF 0-1 on even`
- Priest: `Wis 0-1, Vit 0-1 every level, AM 0-3, AF 0-1 on even`
- Wizard: `Wis 1-2, AM 0-3, Vit 0-1 on odd, AF 0-1 on even`

Full ranges Neophyte (2) to Archmaster (16), Atari/Amiga/PC:

```
Fighter HP: 6-9, 9-13, 12-18, 15-22, 18-27, 21-31, 24-36, 27-40, 30-45, 33-49, 36-54, 39-58, 42-63, 45-67, 48-72
 Str 1-2 always, Dex 0-1 always, Vit 0-1 on 3,5,7,9..., AF 0-1 on 2,4,6,8...
Ninja HP: 4-6, 6-9, 8-12, 10-15, 12-18, 14-21, 16-24, 18-27, 20-30, 22-33, 24-36, 26-39, 28-42, 30-45, 32-48
 Dex 1-2 always, Str 0-1 always, Vit/AF same odd/even as Fighter
Priest HP: 3-4, 5-7, 6-9, 8-12, 9-13, 11-16, 12-18, 14-21, 15-22, 17-25, 18-27, 20-30, 21-31, 23-34, 24-36
 Mana: 2-3, 3-5, 4-7, 5-8, 6-9, 7-10, 8-11, 9-12, 10-13, 11-14, 12-15, 13-16, 14-17, 15-18, 16-19
 Wis 0-1, Vit 0-1, AM 0-3 always, AF 0-1 on even
Wizard HP: 2-3, 3-4, 4-6, 5-7, 6-9, 7-10, 8-12, 9-13, 10-15, 11-16, 12-18, 13-19, 14-21, 15-22, 16-24
 Mana: 3-4, 4-6, 6-9, 7-10, 9-12, 10-13, 12-15, 13-16, 15-18, 16-19, 18-21, 19-22, 21-24, 22-25, 24-27
 Wis 1-2, AM 0-3 always, Vit 0-1 on odd, AF 0-1 on even
```

Note: Atari ST ignores Vitality vs poison/wounds and ignores AM/AF (bugs, fixed elsewhere). Save/reload to reroll — full stats usually costs HP/Stamina.

## 4. The 32k XP thought experiment

Give every class +32000 XP:

- Reincarnated: `0 + 32000 = Adept (8)` in F+N+P+W. 28 level-ups.
- Resurrected: `start (0-4000) + 32000 = 32000-36000`, still Adept (needs 64000 for Expert). Same titles, fewer rolls.

Expected from 0 -> Adept all four:

- HP +328 (F131+N88+P66+W44)
- Mana +107 (P44.5+W62)
- Stamina x4.67
- Str +14, Dex +14, Wis +14, Vit +8, AF +8, AM +21 (averages)

Resurrected misses early rolls = starting_level-1 rolls. E.g. Journeyman Fighter misses H+52 / Str+6 Dex+2.

## 5. All 24 champions — expected Adept

`Re / Rs` = reincarnated vs resurrected, averages, before small DM1 reincarnate bump (+few pts to re). Actual = ± random.

Base stats in mirror order: Health, Stamina, Mana, Luck, Str, Dex, Wis, Vit, AM, AF.

| Champion | HP Re / Rs | Stam Re / Rs | Mana Re / Rs | Str/Dex/Wis/Vit/AM/AF Re | Str/Dex/Wis/Vit/AM/AF Rs |
|---|---|---|---|---|---|
| Iaido | 376 / 333 | 304 / 220 | 118 / 110 | 57/69/54/43/66/58 | 52/68/53/42/63/56 |
| Zed | 388 / 341 | 280 / 180 | 116 / 100 | 54/54/54/58/61/48 | 50/50/50/56/55/46 |
| Chani | 375 / 345 | 313 / 240 | 124 / 106 | 51/61/71/45/68/45 | 48/60/66/44/64/44 |
| Hawk | 398 / 362 | 397 / 295 | 116 / 104 | 59/49/52/63/56/43 | 56/48/50/61/52/42 |
| Boris | 363 / 339 | 304 / 241 | 134 / 118 | 49/59/69/48/66/48 | 48/56/64/47/62/46 |
| Alex | 378 / 349 | 266 / 207 | 120 / 110 | 58/69/59/48/56/48 | 56/64/56/47/53/46 |
| Nabi | 383 / 360 | 304 / 243 | 120 / 98 | 55/50/59/53/76/63 | 55/50/54/51/68/62 |
| Hissssa | 408 / 362 | 285 / 202 | 112 / 112 | 72/62/49/43/64/63 | 66/58/49/42/64/62 |
| Gothmog | 388 / 371 | 257 / 220 | 124 / 98 | 54/49/62/42/71/67 | 54/49/56/41/65/66 |
| Sonja | 393 / 341 | 327 / 242 | 108 / 108 | 68/59/53/57/61/48 | 62/57/53/56/61/47 |
| Leyla | 376 / 341 | 280 / 222 | 110 / 110 | 54/67/59/55/66/43 | 52/61/59/54/66/42 |
| Mophus | 383 / 357 | 257 / 211 | 126 / 106 | 56/49/54/56/61/53 | 56/49/52/54/55/52 |
| Wuuf | 368 / 336 | 233 / 178 | 136 / 130 | 47/71/59/48/56/48 | 46/66/58/46/53/46 |
| Stamm | 403 / 351 | 374 / 276 | 106 / 106 | 66/57/49/58/56/63 | 60/55/49/57/56/62 |
| Azizi | 389 / 348 | 360 / 260 | 114 / 114 | 61/62/56/53/51/43 | 56/56/56/52/51/42 |
| Leif | 403 / 360 | 327 / 237 | 114 / 106 | 60/54/53/58/66/53 | 56/52/52/56/63/52 |
| Tiggy | 353 / 329 | 210 / 167 | 142 / 124 | 44/59/64/43/80/48 | 43/56/60/42/76/46 |
| Wu Tse | 373 / 344 | 219 / 169 | 126 / 114 | 52/49/67/53/68/48 | 51/46/66/51/64/46 |
| Daroou | 428 / 392 | 304 / 233 | 112 / 109 | 64/44/49/53/51/53 | 60/42/48/52/50/52 |
| Halk | 418 / 366 | 350 / 259 | 106 / 106 | 69/57/44/54/59/56 | 63/55/44/53/59/55 |
| Syra | 381 / 360 | 336 / 272 | 122 / 98 | 52/49/57/53/63/48 | 52/49/52/52/56/46 |
| Gando | 367 / 338 | 294 / 229 | 132 / 123 | 53/59/61/41/69/51 | 52/54/58/40/66/50 |
| Linflas | 393 / 353 | 233 / 173 | 118 / 109 | 59/59/61/43/71/43 | 54/58/58/42/68/42 |
| Elija | 388 / 352 | 271 / 201 | 128 / 116 | 56/54/56/44/74/48 | 53/53/54/42/70/46 |

Base mirror stats for reference (H, S, M, Luck, Str, Dex, Wis, Vit, AM, AF):

- Iaido 48,65,11,40,43,55,40,35,45,50 — Appr F, Nov P
- Zed 60,60,10,58,40,40,40,50,40,40 — Nov F/N/P/W
- Chani 47,67,17,57,37,47,57,37,47,37 — Nov F, Appr W
- Hawk 70,85,10,40,45,35,38,55,35,35 — Nov F, Appr P
- Boris 35,65,28,35,35,45,55,40,45,40 — Nov N, Appr W
- Alex 50,57,13,47,44,55,45,40,35,40 — Appr N, Nov W
- Nabi 55,65,13,40,41,36,45,45,55,55 — Appr P, Nov W
- Hissssa 80,61,5,40,58,48,35,35,43,55 — Appr F, Nov N
- Gothmog 60,55,18,30,40,35,48,34,50,59 — Journ W
- Sonja 65,70,2,40,54,45,39,49,40,40 — Journ F
- Leyla 48,60,3,50,40,53,45,47,45,35 — Journ N
- Mophus 55,55,19,40,42,35,40,48,40,45 — Journ P
- Wuuf 40,50,30,60,33,57,45,40,35,40 — Appr N, Nov P
- Stamm 75,80,0,35,52,43,35,50,35,55 — Journ F
- Azizi 61,77,7,47,47,48,42,45,30,35 — Nov F, Appr N
- Leif 75,70,7,35,46,40,39,50,45,45 — Appr F, Nov P
- Tiggy 25,45,35,45,30,45,50,35,59,40 — Nov N, Appr W
- Wu Tse 45,47,20,40,38,35,53,45,47,40 — Nov N, Appr P
- Daroou 100,65,6,35,50,30,35,45,30,45 — Appr F, Neoph W
- Halk 90,75,0,40,55,43,30,46,38,48 — Journ F
- Syra 53,72,15,55,38,35,43,45,42,40 — Nov P, Appr W
- Gando 39,63,26,50,39,45,47,33,48,43 — Appr N, Nov W
- Linflas 65,50,12,45,45,45,47,35,50,35 — Appr F, Nov W
- Elija 60,58,22,50,42,40,42,36,53,40 — Nov F, Appr P

Takeaway: biggest reincarnate gains for high starters — Halk/Stamm/Sonja (+52 HP), Gothmog (+26 Mana), Leyla/Stamm (+6 in Dex/Str).

## 6. Practical advice

1. DM1, normal play: always reincarnate. Early pain until worms, then permanently ahead.
2. DM1, speedrun / early only: resurrect Halk/Stamm/Gothmog for free Journeyman.
3. CSB: resurrect unless you want challenge — reincarnate guts H/S/M.
4. To maximise: sleep/walk into wall before level-up tick, save often, reroll for 1-2 Str / 1-2 Dex / 1-2 Wis + mana.
