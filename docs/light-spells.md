# Light spells (FUL and OH IR RA): durations, brightness, mana efficiency

All numbers traced to source (`src/Magic.cpp`, `src/Timer.cpp`,
`src/CSBCode.cpp`) and to the actual game data (`Word1074`,
`PaletteBrightness`, rune costs, spell table) extracted from
`graphics.dat` (identical in `Dungeons/Dungeonmaster` and
`Dungeons/Chaos Strikes Back`).

## How they work

- Both are spell class 3 (`case 3: // Light`, `Magic.cpp:1290-1309`).
  `missileType 5` = FUL (2 runes: power + FUL), `missileType 0` =
  OH IR RA (4 runes: power + OH + IR + RA).
- Duration and brightness depend **only on the power rune**
  (LO=1 … MON=6, `spellPower = incantation[0]-95`). No randomness.
- Caster skill/Wisdom affects only success vs. fizzle, never the
  duration of a successful cast. Mana is charged **per rune at
  selection time** (`CSBCode.cpp:9026-9036`), so fizzles still cost mana.
- Each cast adds `Word1074[step]` to `d.Brightness` and arms one decay
  timer. Casts stack additively, each with its own timer. After the
  full-bright plateau expires, the contribution steps down one level
  every 4 ticks (`Timer.cpp:2879-2914`) — a seconds-long fade, not an
  instant drop.
- Real-time scales with game speed (≈6.4 game-ticks/s at Normal;
  minutes below use that). Tick counts are engine-exact.

## Duration and brightness per power

With `D6W = 4*(spellPower+1)`:
- FUL: `delay = 4000 + 128*(D6W-3)`, step `D6W/4+1`
- OH IR RA: `delay = 10000 + (D6W-8)*512`, step `D6W/2-1`

| Power | FUL step/light | FUL total | OH step/light | OH total |
|---|---|---|---|---|
| LO | 3 / 24 | 4648 t (~12 min) | 3 / 24 | 10008 t (~26 min) |
| UM | 4 / 33 | 5164 t (~13.5 min) | 5 / 40 | 12064 t (~31 min) |
| ON | 5 / 40 | 5680 t (~15 min) | 7 / 51 | 14120 t (~37 min) |
| EE | 6 / 46 | 6196 t (~16 min) | 9 / 68 | 16176 t (~42 min) |
| PAL | 7 / 51 | 6712 t (~17.5 min) | 11 / 82 | 18232 t (~47.5 min) |
| MON | 8 / 59 | 7228 t (~19 min) | 13 / 94 | 20288 t (~53 min) |

Totals include the fade tail (`(steps-1)*4` ticks, ≤48 ticks).

## Brightness data (`Word1074`, `PaletteBrightness`)

```
step:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
W:     0  5 12 24 33 40 46 51 59 68 76 82 89 94 97 100
```

Visible palettes (0 = brightest): total light ≥99 → 0, 75–98 → 1,
50–74 → 2, 25–49 → 3, 1–24 → 4, ≤0 → 5 (dark).

Consequences:
- No single cast reaches palette 0 (max is MON OH IR RA at 94).
  Palette 0 needs stacked casts or torches.
- Brightness steps are table indices, not lumens — they don't add.
  Two EE FULs give `46 + 46 = 92` light units, not "brightness 12".

## Mana costs (from `Byte19010`/`Byte19016`)

- FUL: [6, 9, 13, 16, 20, 23] per power LO…MON
- OH IR RA: [18, 27, 37, 46, 56, 65]
- FUL needs skill 1 + power; OH IR RA needs skill 4 + power.
  Disable (vulnerable) time: FUL 15 ticks, OH IR RA 22 ticks.

## Efficiency (exhaustive search over 1–5 simultaneous casts)

- Raw ticks/mana favors FUL, but that ignores delivered light.
- Light-weighted (light-units × ticks per mana), OH IR RA wins from
  ON power upward; **MON OH IR RA is the best single cast overall**.
- Palette 1 (≥75, enough to see hidden switches): **single PAL OH IR RA**
  (3.078 mana/1000 ticks) beats single MON OH IR RA (3.210) — same
  visible palette for less mana. No top-up combo beats it.
- Palette 0 (≥99) ranking:

| Rank | Combo | Light | Mana | Coverage | Mana/1000t |
|---|---|---|---|---|---|
| 1 | 3× UM FUL | 99 (exact!) | 27 | ~13.4 min | 5.241 |
| 2 | 2× ON OH IR RA | 102 | 74 | ~36.7 min | 5.250 |
| 3 | 2× EE OH IR RA | 136 | 92 | ~42 min | 5.696 |
| 4 | 2× PAL FUL | 102 | 40 | ~17.4 min | 5.981 |

  3× UM FUL wins by a hair; 2× ON OH IR RA is far more convenient
  (2 casts, 2.7× duration).
- FUL+OH hybrids never win: coverage ends when the shortest cast
  expires, stranding the long cast's duration.
- When pipelining repeat rounds, offset recasts by ~20+ ticks so the
  short fade tails don't overlap into a sub-threshold dip.

## Practical recipes

- Dim but cheapest: LO/UM FUL (~0.5–0.67 mana/min, palettes 3–4).
- Clearly see everything (palette 1): one PAL OH IR RA per ~47 min.
- Brightest (palette 0): 2× ON OH IR RA per ~37 min
  (or 3× UM FUL per ~13 min at low skill).
