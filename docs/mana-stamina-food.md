# Mana → Stamina → Food in CSB / Dungeon Master

How spell mana becomes stamina drain, how stamina regenerates, and what it costs in food and water. All numbers traced to source.

## 1. Mana regeneration

Runs in `TenSecondUpdate()`, `src/Code17818.cpp:209-254`.

* Tick rate: every `64 Time` awake (`~10–16s` real at normal speed), every `16 Time` sleeping (`~2.5s` nominal) — `src/CSBCode.cpp:700`:
  ```c
  D0L &= d.PartySleeping ? 15 : 63;
  if (D0L==0) TenSecondUpdate();
  ```
* Only if `Mana < MaxMana`. If `Mana > MaxMana`, decays `-1`/tick.
* Gate: `random112 = (Time&0x40)|((Time&0x80)>>2)|((Time&0x100)>>4)` — 8 steps `0,16,...,112` — must be `< WizardMastery + PriestMastery + CurrentWisdom`.
* Amount:
  ```c
  manaAdjust = MaxMana/40 + 1;  // x2 if PartySleeping
  ```
  Example `MaxMana 229`: `229/40+1 = 6`, sleeping `12`. Example `MaxMana 500`: `13` awake, `26` sleeping.
* Stamina cost — `src/Code17818.cpp:236,243`:
  ```c
  staminaAdjust = manaAdjust * Larger(7, 16-magicMastery);
  AdjustStamina(chIdx, staminaAdjust); // Stamina -= cost
  ```
  `magicMastery = WizardMastery + PriestMastery` (`1=Neophyte, 2=Novice, ...`, `src/CSBlinux.cpp:1759`).

| Mastery sum | Factor | 100 mana costs |
|---|---|---|
| 0–1 | x16 | 1600 stamina |
| 5 | x11 | 1100 stamina |
| ≥9 | x7 | 700 stamina |

> **Sleeping trap:** `DetermineMastery()` returns `1` while `PartySleeping` (`src/Code17818.cpp:29-35`). So sleeping mana always costs `x14` (`16-2`) and the gate is `2+Wisdom` only. Fast but stamina-expensive.

`AdjustStamina` (`src/Code11f52.cpp:4520-4549`): `Stamina -= decrement`; if driven `<= 0`, spills into `DamageCharacter(absStamina/2)` — HP damage.

Health regen (`:351-373`) costs no stamina but is gated by it: only if `Stamina >= MaxStamina/4` and `random112 < Vitality+12`, amount `(MaxHP>>7)+1` (x2 sleeping, +50% with Ekkhard Cross).

## 2. Stamina regeneration

Same tick, right after mana — `src/Code17818.cpp:264-349`. `AdjustStamina(ch,D4W)` with negative `D4W` = gain.

### Base rate

```c
LOCAL_4 = clamp((MaxStamina>>8)-1, 1..6); // ApplyLimits
if (PartySleeping) LOCAL_4 *= 2;
if (Time-LastPartyMoveTime > 80) LOCAL_4++;
if (Time-LastPartyMoveTime > 250) LOCAL_4++;
```

* `~1000 MaxStamina → 2`, `~1500 → 4`, `1800 → 6` awake active.
* Sleeping doubles; standing still adds up to +2 (stacks with sleep).

### Tired vs rested (`D6W`)

```c
for (D6W=4, D4W=MaxStamina; D4W>>=1, Stamina<LOCAL_26; D6W+=2);
D5W = (D6W<=4) ? 1 /*rested*/ : 0 /*tired*/;
```

* `rested (D6W=4)`: `Stamina >= Max/2`
* `tired`: `6 = <1/2`, `8 = <1/4`, `10 = <1/8`, …

These are internal-loop labels, not screen text.

### The loop (per iteration)

```c
if (food >= 0)  D4W -= LOCAL_4;   // gain
if (water >= 0) D4W -= LOCAL_4;   // gain, stacks
// burn:
rested: food -= 2, water -= 1;
tired:  food -= D6W/2, water -= D6W/2;
```

* `food/water >= 0` (fed/watered): regenerates.
* `[-512,-1]` (hungry/thirsty): no gain, still burns.
* `< -512` (starving/dehydrated) **and** rested: `D4W += LOCAL_4` → net drain.
* Loop runs `D6W` iterations max (4 rested, more tired) but exits early when `Stamina >= MaxStamina + D4W` — it only fills the deficit.
* Even at full stamina it runs once: passive metabolism `2 food + 1 water` per tick rested.
* Floor clamp `-1024` for both (`:342-349`).

### Per-tick maximum (rested, fed+watered)

`4 iters × 2×LOCAL_4 = 8×LOCAL_4`, costing `8 food + 4 water`.

For an `1800 MaxStamina` character (shown as `180`, since bars display `/10` — `src/Character.cpp:1089`):

| State | LOCAL_4 | Stamina / tick | Tick length |
|---|---|---|---|
| Awake, active | 6 | 48 | ~10s |
| Awake, idle >250 | 8 | 64 | ~10s |
| Sleeping, active | 12 | 96 | ~2.5s nominal |
| Sleeping, idle >250 | 14 | 112 | ~2.5s nominal |

Per rested iteration in the god case: `2 food + 1 water → 14+14 = 28 stamina`. Thirsty halves it (`14 per 2 food`). Tired costs ~1.5–2.5× more food for the same gain.

## 3. Food → stamina examples

Best case = rested + fed + watered, gain `LOCAL_4` stamina per `1` food.

* `LOCAL_4=2` (awake, `MaxStamina ~1000`): `700 stamina → ~350 food + ~175 water`.
* `LOCAL_4=4` (sleeping / high max / idle): `700 → ~175 food`.
* Food-only, thirsty: double — `~700–1400 food`.
* Tired/starving phases: more.

Scale: stomach `2048` max, `-1024` min; fresh spawn `1500+rand(0..255)` (`src/Character.cpp:519,4481`). So `350 ≈ ¼ stomach`, `1400 ≈ full stomach`.

With user-quoted dungeon values (apple `500`, dragon steak `1400` — from `d.FoodValue[8]`, `src/Data.h:1094`, applied at `src/Character.cpp:4335`):

* `350 food` = less than 1 apple ≈ ¼ dragon steak.
* `1400 food` = ~3 apples ≈ 1 dragon steak.

Sleeping + idle halves the food bill (doubled `LOCAL_4`) on top of the 4× tick rate.

## 4. Food / water bars and thresholds

`DrawFoodWaterBar(value)` — `src/Viewport.cpp:7200-7211`:

```c
if (value < 0) color = 11;    // yellow
if (value < -512) color = 8;  // red
length = (value+1024)/32;     // px
```

| Value | Bar | Color |
|---|---|---|
| `2048` | full, 96px | normal |
| `0` | 32px, ~⅓ | turns yellow |
| `-512` | 16px | turns red |
| `-1024` | 0px, visually empty | red |

Same function/thresholds for food and water. Additionally, `food<0 || water<0` flips the mouth-box graphic `33→34` (`src/Character.cpp:1365-1375`) as a hunger/thirst hint.

Gameplay meaning (both stats): `≥0` regenerates, `[-512,-1]` stalls, `<-512` drains while rested.

## 5. Water values

* Water flask (`potion_WaterFlask`): `+1600` — `src/Character.cpp:4319-4321`.
* Waterskin / water (`objNI_Waterskin..objNI_Water`): `+800` per gulp — `src/Character.cpp:4158-4174`. Full skin holds 3 gulps (`value(3)` on fountain refill, `src/Mouse.cpp:1232-1234`) = `2400` total.
* Applied with the same clamp as food — `src/Character.cpp:4480-4481`:
  ```c
  water = LIMIT(-1023, water+value, 2048);
  food  = LIMIT(-1023, food+value, 2048);
  ```

## 6. Why sleep recovers so fast in real time

Entering sleep sets `d.Speed = 0` and skips viewport drawing (`src/Mouse.cpp:1858-1864`, `src/CSBCode.cpp:652-653`), so `d.Time++` runs at ~VBL rate instead of ~1/15th of it. Observed case: `227/229 mana` in ~8s of sleep = ~19 procs × 12, i.e. ≥304 `Time` plus failed rolls — trivial while sleeping (≈38+ `Time`/s), ~5 min awake (`19×64 Time` at ~4 `Time`/s). Sleeping cost then was ~`19×168 ≈ 3200` stamina (x14 factor, §1), covered by equally accelerated stamina regen and food/water burn.

## 7. Units: sheet numbers are `/10`, regen math is internal

Health/stamina/mana numbers on the character sheet are `value/10` (`PrintLifeForce(stamina,124,Stamina()/10,MaxStamina()/10)`, `src/Character.cpp:1089`). All regen/cost math in §1–§2 is internal. Mixing the two produces phantom exchange rates — e.g. a displayed `−10.6` stamina loss next to an internal `+6` mana gain looks like `~1.8 per mana` but is really `106` internal (`§8`).

Related waste: on a successful mana tick the stamina price is always full, but the mana gain is clamped — `Mana + Smaller(manaAdjust, MaxMana−Mana)` (`:244`). Topping off the last point of a deficit still costs the whole `staminaAdjust`.

## 8. Gross vs net: mana runs first, stamina refunds in the same tick

Mana regen (`:214-246`) executes *before* the stamina loop (`:264-341`) in the same `TenSecondUpdate`. The mana cost deepens the stamina deficit, then the stamina loop immediately refunds up to `8×LOCAL_4` of it (capped by deficit and food/water). What you observe per tick is always **net = refund − cost**, never the true price.

Worked observation (verified in-game, `Dungeons/Dungeonmaster`, awake, idle, both bars pinned red at the `−1024` floor): heroine with internal `MaxStamina 8350` (sheet `835`), `MaxMana 229`, Wizard+Priest Master (~11 each), Wisdom 66.

* `LOCAL_4 = 6` (clamped) `+2` idle = `8`.
* Mana: `+6` per proc, price `6×7 = 42`. Gate `< ~88` passes 6/8 ticks (75%).
* Starvation drain: starving sources only drain on rested iterations, and there are always exactly 4 of those (`D6W` 4,3,2,1 — tired iterations are no-ops for starving food/water). So drain = `4×8 + 4×8 = 64` internal per tick (6.4 displayed), independent of tiredness.

| Tick | Mana (int) | Stamina (int) | Stamina (sheet) |
|---|---|---|---|
| Gate fails (25%) | 0 | −64 | **−6, "no mana"** |
| Gate passes (75%) | +6 | −42−64 = −106 | **−10.6, "10 to 11"** |

Average over 8 ticks: `(6×10.6 + 2×6.4)/36 ≈ 2.12` displayed per internal mana — the measured "~2.1". The true internal price is `(42+64)/6 ≈ 17.7`, of which only `42` is the spell and `64` is starvation. The giggler attacking throughout is coincidence: gigglers only steal objects (`StealFromCharacter`, `src/Monster.cpp:239-288,1236-1238`) and touch neither mana nor stamina — the `6/tick` is her own regen.

## 9. Food is optional: the `Mon` potion loop

`Mon` potions (`potion_MonPotionB`) grant stamina directly, bypassing food/water entirely (`src/Character.cpp:4237-4246`):

```c
fp.staminaAdjust = Smaller(MaxStamina - Stamina, MaxStamina/w_14);
AdjustStamina(ch, -staminaAdjust); // pure gain
```

`w_14 = ((511−strength)/w_30)/2`; a strength-~255 brew has `w_14 = 2`, i.e. half-max per gulp — two gulps take `0 → 8350`. If brewing one costs `13` mana, refilling `26` mana costs ~`550–650` internal stamina (§8 math: 5 procs × 42 + ~6–7 ticks × 64 drain). Net per cycle: `+8350 − ~600 ≈ +7700`. Sustainable forever with a flask supply and reliable casts. Starvation only ever attacks stamina, so red bars at `−1024` are harmless while the loop runs. Caveats: each drink empties its flask (`:4326`); fizzled casts still eat mana; never actually touch 0 stamina (overflow damages HP, and HP regen needs `Stamina ≥ Max/4`); sleeping 4×es the refill but forces the ×14 mastery penalty (§1).

## 10. Death and the Vi altar

Dying (`KillCharacter`, `src/Character.cpp:2510-2649`) costs nothing by itself: `HP=0`, possessions dropped, bones + fingerprint record created, poison cured. No XP/skill/stat loss at that moment.

Resurrecting those bones at a Vi altar (`BringCharacterToLife` via `ProcessTT_ViAltar`, `src/Character.cpp:791-813`, `src/Timer.cpp:2748-2854`) taxes only max health, integer division (floor):

```c
MaxHP = Larger(25, MaxHP - MaxHP/64 - 1); // loss = floor(MaxHP/64)+1
HP = MaxHP/2;
```

E.g. `50 → 49` (revived at 24 HP); `500 → 492`; `25 → 25` (free at the floor). No change to skills, attributes, stamina/mana or their maxes; repeat deaths stack.

Not to be confused with **Reincarnate** (the other resurrect-menu button, `src/CSBCode.cpp:6740-6788`): wipes all `skills92`, cuts attributes by `reincarnateAttributePenalty/16` (dungeon default `2`) and HP/Stamina/Mana maxes by `reincarnateStatPenalty/16` (default `8` = halved), then adds 12 random attribute points. Skipped entirely when **DM Rules** is checked — which otherwise has no effect on any regen math in this document.
