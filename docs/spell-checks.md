# How spell casting is checked (CSBwin / Dungeon Master)

All references are this repo. Success/failure lives in `CastSpell`
(`src/Magic.cpp:1093-1407`), mastery in `DetermineMastery`
(`src/Code17818.cpp:13-...`), rune mana costs in `TAG01b990`
(`src/CSBCode.cpp:9018-9036`), missile mana in `AttackWithSpell`
(`src/Attack.cpp:1290-1330`) + `CharacterThrowsSpell`
(`src/Magic.cpp:477-503`).

## 1. Mana is paid per rune, before the check

Each symbol clicked in the spell menu costs mana immediately
(`TAG01b990`):

```c
A0 = d.Byte19010 + 6*D7W;          // base cost by position + button
D6W = A0[button];
if (D7W != 0) {                    // not the power rune
  D5W = incantation[0] - 96;       // power 1..6 of first rune
  D1W = d.Byte19016[D5W];
  D6W = (D6W * D1W) >> 3;
}
if (D6W > Mana) return;            // rune refused
IncrMana(-D6W);
```

So a fizzled cast still costs its full rune mana. Totals per
spell/power are listed in §4 (aggregated from `graphics.dat` item 560
via DMExtract — same numbers as the DM Encyclopaedia spells page).

Missile spells (`Fireball`, `Lightning`, ...) pass `neededMana` at
throw time instead: if short, range scales down proportionally and the
remaining mana is consumed (`Attack.cpp:1301-1307`); `CharacterThrowsSpell`
returns 0 (and halves that attack's XP) if mana is insufficient.

## 2. The success check (`Magic.cpp:1209-1237`)

```c
spellPower = incantation[0] - 95;  // 1 Lo .. 6 Mon
skillRequired = pSpell->skillRequired;  // "Difficulty" column, 0..6
spellByte5 = pSpell->byte5;             // school tested + trained:
                                        // 12 Identify, 13 Heal, 14 Influence,
                                        // 15 Defend, 16 Fire, 17 Air,
                                        // 18 Earth, 19 Water,
                                        // 2 Priest / 3 Wizard basic (only
                                        // Ee potion / Zokathra)

D0 = (STRandom() & 7);                  // 0..7
D4 = skillRequired + spellPower;        // difficulty
D0 += 16 * D4;
D1 = 8 * ((spellPower - 1) * skillRequired);
D0 += D1;
w_4 = D0 + D4 * D4;                     // XP on success (see §3)

D6 = DetermineMastery(chIdx, spellByte5);  // 1..16 from XP:
                                           // 1:<500, 2:500+, 3:1000+,
                                           // doubling per level
if (D6 < D4) {
  for (w_6 = D4 - D6; w_6-- != 0;) {
    D0 = STRandom() & 127;              // 0..127 per missing level
    D1 = min(Wisdom.Current() + 15, 115);
    if (D0 > D1) {
      AdjustSkills(chIdx, spellByte5, w_4 >> (D4 - D6), ASW_CastSpell1);
      SpellErrorMsg(pcA3, 0, spellByte5);  // "NEEDS MORE PRACTICE ..."
      return 0;                            // fizzle
    }
  }
}
// ... effect happens ...
AdjustSkills(chIdx, spellByte5, w_4, ASW_CastSpell2);
```

Meaning:

- `D6 >= D4` → always succeeds.
- Otherwise one `0..127 vs min(Wisdom+15, 115)` save **per missing
  level**. A single failure fizzles. High power / high base difficulty
  hurts (larger `D4`), high school mastery / high Wisdom helps.
- Failure still trains: `w_4 >> deficit` XP (halved per missing level),
  plus missile attacks halve `experienceGained` again on a failed throw.

## 3. XP on success (assumed below)

Expected value (`rand&7` → 3.5):

```
D  = S + P                     (S = base difficulty, P = power 1..6)
E  = 3.5 + 16*D + 8*(P-1)*S + D*D
```

This goes to the spell's school (`spellByte5`), including the two
special cases that train basics directly: `Zo Bro Ra` (Ee/mana potion)
→ basic Priest (2), `Zo Kath Ra` (Zokathra) → basic Wizard (3).

## 4. XP per mana (success assumed)

`XP/mana = E / mana(P)`, mana from the spell list. Best trainers:
`Ya` stamina (~20), `Vi` health (~10-13 at high power),
`Oh Ven` poison cloud (~9-10). Worst: `Des Ir Sar` darkness (~2),
`Zo Kath Ra` (~1-2, but it is the only direct basic-Wizard XP).

### Priest spells — expected XP / mana by power

| Spell (S, school) | LO | UM | ON | EE | PAL | MON |
|---|---|---|---|---|---|---|
| Vi Health (S1→Heal 13) mana 4/6/9/11/14/16 | 9.88 | 11.42 | 11.06 | 12.05 | 11.96 | 12.78 |
| Ya Stamina (S2→Heal 13) 3/5/7/9/11/13 | 20.17 | 19.90 | 20.07 | 20.39 | 20.77 | 21.19 |
| Zo Bro Ra Mana/Ee (S3→Priest 2) 21/31/43/53/65/75 | 3.98 | 4.27 | 4.27 | 4.46 | 4.48 | 4.65 |
| Ful Bro Ku Strength (S4→Heal 13) 15/22/31/38/47/54 | 7.23 | 7.61 | 7.37 | 7.67 | 7.59 | 7.84 |
| Oh Bro Ros Dexterity (S4→Heal 13) 14/21/29/36/44/51 | 7.75 | 7.98 | 7.88 | 8.10 | 8.10 | 8.30 |
| Ya Bro Dain Wisdom (S4→Heal 13) 13/19/27/33/41/47 | 8.35 | 8.82 | 8.46 | 8.83 | 8.70 | 9.01 |
| Ya Bro Neta Vitality (S4→Heal 13) 14/21/29/36/44/51 | 7.75 | 7.98 | 7.88 | 8.10 | 8.10 | 8.30 |
| Vi Bro Cure poison (S1→Heal 13) 11/16/23/28/35/40 | 3.59 | 4.28 | 4.33 | 4.73 | 4.79 | 5.11 |
| Ya Bro Shield/Ya (S2→Defend 15) 10/15/21/26/32/37 | 6.05 | 6.63 | 6.69 | 7.06 | 7.14 | 7.45 |
| Ful Bro Neta Fire shield (S4→Defend 15) 17/25/35/43/53/61 | 6.38 | 6.70 | 6.53 | 6.78 | 6.73 | 6.94 |
| Ya Ir Shield party (S2→Defend 15) 10/15/21/26/32/37 | 6.05 | 6.63 | 6.69 | 7.06 | 7.14 | 7.45 |
| Des Ir Sar Darkness (S1→Defend 15) 21/31/43/53/65/75 | 1.88 | 2.21 | 2.31 | 2.50 | 2.58 | 2.73 |

### Wizard spells — expected XP / mana by power

| Spell (S, school) | LO | UM | ON | EE | PAL | MON |
|---|---|---|---|---|---|---|
| Ful Torch (S1→Fire 16) 6/9/13/16/20/23 | 6.58 | 7.61 | 7.65 | 8.28 | 8.38 | 8.89 |
| Oh Ir Ra Light (S4→Air 17) 18/27/37/46/56/65 | 6.03 | 6.20 | 6.18 | 6.34 | 6.37 | 6.52 |
| Zo Open door (S1→Air 17) 8/12/17/21/26/30 | 4.94 | 5.71 | 5.85 | 6.31 | 6.44 | 6.82 |
| Ya Bro Ros Footprints (S1→Earth 18) 12/18/25/31/38/44 | 3.29 | 3.81 | 3.98 | 4.27 | 4.41 | 4.65 |
| Oh Ew Ra See-through-walls (S3→Earth 18) 16/24/33/41/50/58 | 5.22 | 5.52 | 5.56 | 5.77 | 5.83 | 6.01 |
| Oh Ew Sar Invisibility (S3→Air 17) 17/25/35/43/53/61 | 4.91 | 5.30 | 5.24 | 5.50 | 5.50 | 5.71 |
| Zo Ven Poison potion (S2→Water 19) 12/18/25/31/38/44 | 5.04 | 5.53 | 5.62 | 5.92 | 6.01 | 6.26 |
| Des Ven Poison bolt (S1→Water 19) 11/17/23/29/35/41 | 3.59 | 4.03 | 4.33 | 4.57 | 4.79 | 4.99 |
| Oh Ven Poison cloud (S3→Water 19) 9/14/19/24/29/34 | 9.28 | 9.46 | 9.66 | 9.85 | 10.05 | 10.25 |
| Des Ew Weaken nonmaterial (S1→Earth 18) 12/18/25/31/38/44 | 3.29 | 3.81 | 3.98 | 4.27 | 4.41 | 4.65 |
| Ful Ir Fireball (S3→Fire 16) 13/19/27/33/41/47 | 6.42 | 6.97 | 6.80 | 7.17 | 7.11 | 7.41 |
| Oh Kath Ra Lightning (S4→Air 17) 17/26/35/44/53/62 | 6.38 | 6.44 | 6.53 | 6.62 | 6.73 | 6.83 |
| Zo Kath Ra Zokathra (S0→Wizard 3) 20/30/41/51/62/72 | 1.02 | 1.32 | 1.48 | 1.64 | 1.75 | 1.88 |

Absolute expected XP (same order LO..MON): Vi 39.5/68.5/99.5/132.5/167.5/204.5;
Ya 60.5/99.5/140.5/183.5/228.5/275.5; Ee 83.5/132.5/183.5/236.5/291.5/348.5;
Ku/Dane/Neta family 108.5/167.5-167.5/228.5/291.5/356.5-423.5 (Dane +0/0/+0/+0/+0/+0 vs same-mana Ros/Neta — identical formula, cheaper mana);
Vi Bro 39.5/68.5/99.5/132.5/167.5/204.5; Ya Bro/Ya Ir 60.5/99.5/140.5/183.5/228.5/275.5;
Fire shield 108.5/167.5/228.5/291.5/356.5/423.5; Darkness 39.5/68.5/99.5/132.5/167.5/204.5;
Torch/Open/Footprints/Weaken scale 39.5-60.5 at LO to 204.5-275.5 at MON by S;
Light/Lightning 108.5→423.5; Fireball/Invis/Cloud/See-through 83.5→348.5;
Zokathra 20.5/39.5/60.5/83.5/108.5/135.5.

Notes: actual XP per cast varies ±3.5 around these means
(`STRandom()&7`); mana is exact (rune sum). Difficulty column is
`skillRequired` from item 560; school column is `byte5`. CSB-only magic-map
spells (`Oh Gor *`) omitted — same formulas apply.
