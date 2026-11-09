#!/usr/bin/env python3
"""
Exact (no Monte Carlo) evaluation of damage_calc.one_attack().

Every random step of the game's physical-attack pipeline is a uniform
integer roll over a tiny range, so the complete probability distribution
of one swing -- (damage, hit?, stamina cost) -- can be computed by
propagating probability tables (PMFs) through exactly the same steps as
damage_calc.one_attack(), in the same order:

  gate    quickness vs. need, 3/4 chance, IsCharacterLucky
  power   throwing distance, power*P8/32, armor, randomisation
  graze   the D6<=1 chain
  shrink  the final randomize/shrink chain, Vorpal halving, mastery +10

Everything damage_calc.py computes by sampling (hit %, min/avg/max
damage, damage per try/hit, dps, damage per stamina, xp, ...) is a
function of that distribution, so it is obtained here without any
sampling error.  Stamina drain is handled by propagating the stamina
distribution swing by swing (stamina only changes the outcome table via
StaminaAdjustedAttribute, below half of max stamina).

The pure-Python Monte Carlo in damage_calc.py stays as the reference
implementation; verify_exact.py checks this module against it.
"""

from collections import defaultdict

import damage_calc as engine


# ------------------------------------------------------------ PMF helpers

def _add(dst, key, p):
    dst[key] = dst.get(key, 0.0) + p


def _conv_add(a, b):
    """PMF of X+Y for independent X~a, Y~b."""
    out = {}
    for x, px in a.items():
        for y, py in b.items():
            k = x + y
            out[k] = out.get(k, 0.0) + px * py
    return out


def _shift_scale(pmf, shift, scale):
    return {k + shift: p * scale for k, p in pmf.items()}


# ------------------------------------------------------------- stages

def _gate_miss_probability(ch, atk, mon, ml):
    """P(clean miss) of the to-hit gate (Attack.cpp:601-634)."""
    qp = defaultdict(float)
    for a in range(8):
        for b in range(8):
            for c in range(8):
                qp[engine.quickness(ch, ml, (a, b, c))] += 1.0 / 512
    base = mon["dex"] + mon["level_difficulty"] - 16
    # P(q <= need), need = base + R32
    p_le = 0.0
    for q, pq in qp.items():
        n_ok = sum(1 for r in range(32) if q <= base + r)
        p_le += pq * n_ok / 32.0
    # IsCharacterLucky(75 - P7)
    need_l = 75 - atk["P7"]
    p100 = sum(1 for r in range(100) if r > need_l) / 100.0
    p1 = 0.5 * p100
    luck = ch["luck"]
    if luck:
        p2 = sum(1 for r in range(luck) if r > need_l) / float(luck)
    else:
        p2 = 1.0 if 0 > need_l else 0.0
    p_lucky = p1 + (1.0 - p1) * p2
    return p_le * 0.75 * (1.0 - p_lucky)


def _power_pmf(ch, wpn, atk, mon, ml):
    """Distribution of d6 before the graze test.

    Returns (t0_mass, d6_pmf) where t0_mass is P(throwing distance==0)
    (these swings go straight into the graze chain with d6=0) and d6_pmf
    holds the mass for t != 0 (unnormalised, sums to 1-t0_mass)."""
    t0 = 0.0
    amix = defaultdict(float)
    for r in range(16):
        t = engine.throwing_distance(ch, wpn, ml, r)
        if t == 0:
            t0 += 1.0 / 16
            continue
        n = t // 2 + 1
        for rr in range(n):
            d7 = ((t + rr) * atk["P8"]) // 32
            amix[d7] += 1.0 / 16 / n
    # X = R32 - d4
    div = wpn["armor_div"]
    x = defaultdict(float)
    for r4 in range(32):
        d4 = r4 + mon["armor"] + mon["level_difficulty"]
        if div:
            d4 = d4 - d4 // div
        for r in range(32):
            x[r - d4] += 1.0 / 1024
    return t0, _conv_add(amix, x)


def _graze(gd6):
    """The D6<=1 chain (Attack.cpp:711-798).

    gd6: {d6: mass}.  Returns (miss_mass, {d7: mass})."""
    total = sum(gd6.values())
    miss = total / 4.0
    pre = defaultdict(float)             # d6' = d6 + R16
    for d6, w in gd6.items():
        for r in range(16):
            pre[d6 + r] += w / 16.0
    out = defaultdict(float)
    for d6p, m in pre.items():
        ent = 1.0 if d6p > 0 else 0.5    # d6>0 or randrange(2)
        for a in (1, 2, 3):              # d7 = R4 given nonzero (3/4 total)
            pa = m * 0.25
            out[a] += pa * (1.0 - ent)
            pe = pa * ent
            for b in range(4):           # d7 += R4
                pb = pe * 0.25
                out[a + b] += pb * 0.75
                pq = pb * 0.25 / 16.0    # R4==0: d7 += max(0, R16 + d6')
                for r in range(16):
                    out[a + b + max(0, r + d6p)] += pq
    return miss, out


def _shrink(d7pmf):
    """The randomize/shrink chain (Attack.cpp:799-878) up to, not
    including, the Vorpal halving.  d7pmf values are >= 0."""
    hmass = defaultdict(float)
    for v, w in d7pmf.items():
        hmass[v // 2] += w
    emix = defaultdict(float)            # e = h + R4 + R(h)
    for h, w in hmass.items():
        if h == 0:
            for j in range(4):
                emix[j] += w / 4.0
        else:
            q = w / (4.0 * h)
            for j in range(4):
                for c in range(h):
                    emix[h + j + c] += q
    kmass = defaultdict(float)           # (e + R(e)) // 4
    for e, w in emix.items():
        if e == 0:
            kmass[0] += w
            continue
        lo, hi = e, 2 * e - 1
        per = w / e
        for k in range(lo // 4, hi // 4 + 1):
            cnt = min(hi, 4 * k + 3) - max(lo, 4 * k) + 1
            kmass[k] += per * cnt
    out = defaultdict(float)             # + R4 + 1
    for k, w in kmass.items():
        for j in range(4):
            out[k + j + 1] += w / 4.0
    return out


# ------------------------------------------------------- one swing

# cost = energy + R2 + sadj; sadj depends on the path
_SADJ = {
    "plain": {0: 1.0},
    "miss": {2: 0.5, 3: 0.5},
    "clean": {4: 0.25, 5: 0.25, 6: 0.25, 7: 0.25},
    "graze": {0: 1.0},
}


def swing_stats(ch, wpn, atk, mon):
    """Exact distribution of one swing at ch['stamina'].

    Returns dict:
      hit      {damage: probability} for successful hits (damage >= 1)
      kinds    {path: probability} with path in none/plain/miss/clean/
               graze, used for the stamina cost
      cost     {stamina cost: probability} over all outcomes
    P(hit) = sum(hit.values())."""
    ml = engine.max_load(ch)
    hit = {}
    kinds = defaultdict(float)
    vorpal_hit = wpn.get("vorpal", False) or atk.get("disrupt", False)
    if mon["non_material"] and not vorpal_hit:
        return {"hit": {}, "kinds": {"none": 1.0}, "cost": {0: 1.0}}
    if atk.get("disrupt", False) and not mon["non_material"]:
        kinds["plain"] = 1.0
    else:
        gmiss = _gate_miss_probability(ch, atk, mon, ml)
        kinds["miss"] += gmiss
        t0, d6 = _power_pmf(ch, wpn, atk, mon, ml)
        scale = 1.0 - gmiss
        gd6 = defaultdict(float)
        gd6[0] += t0 * scale
        clean = {}
        for v, p in d6.items():
            if v <= 1:
                gd6[v] += p * scale
            else:
                clean[v] = p * scale
        gm, gd7 = _graze(gd6)
        kinds["miss"] += gm
        shrunk = []
        if clean:
            shrunk.append(("clean", _shrink(clean)))
        if gd7:
            shrunk.append(("graze", _shrink(gd7)))
        vorp = wpn.get("vorpal", False) and not mon["non_material"]
        m = ch_mastery = engine_mastery(ch, atk)
        pm = min(max(m, 0), 64) / 64.0
        for kind, pmf in shrunk:
            for d7, p in pmf.items():
                if vorp:
                    d7 //= 2
                    if d7 == 0:
                        kinds["miss"] += p
                        continue
                kinds[kind] += p
                if pm:
                    _add(hit, d7 + 10, p * pm)
                if pm < 1.0:
                    _add(hit, d7, p * (1.0 - pm))
    energy = atk["energy"]
    cost = defaultdict(float)
    for kind, pk in kinds.items():
        if kind == "none":
            cost[0] += pk
            continue
        for sa, ps in _SADJ[kind].items():
            for r in range(2):
                cost[energy + r + sa] += pk * ps * 0.5
    return {"hit": hit, "kinds": dict(kinds), "cost": dict(cost)}


def engine_mastery(ch, atk):
    return ch["mastery"].get(atk["skill"], 0)


def swing_metrics(st, atk, mon):
    """Per-swing expectations from swing_stats()."""
    hit = st["hit"]
    p_hit = sum(hit.values())
    e_dmg = sum(d * p for d, p in hit.items())
    e_dmg2 = sum(d * d * p for d, p in hit.items())
    e_cost = sum(c * p for c, p in st["cost"].items())
    time = atk["time"]
    e_ticks = time * p_hit + (time / 2) * (1.0 - p_hit)
    mult = mon["level_difficulty"]
    base_miss = atk["exp"] // 2
    e_xp = 0.0
    for d, p in hit.items():
        g = atk["exp"] + (d * mon["exp"]) // 16 + 3
        if mult != 0:
            g *= mult
        e_xp += p * g * 2
    g = base_miss * mult if mult != 0 else base_miss
    e_xp += (1.0 - p_hit) * g * 2
    return {
        "p_hit": p_hit, "e_dmg": e_dmg, "e_dmg2": e_dmg2,
        "e_cost": e_cost, "e_ticks": e_ticks, "e_xp": e_xp,
        "min_hit": min(hit) if hit else None,
        "max_hit": max(hit) if hit else None,
    }


# ----------------------------------------------- rows (GUI/CLI result)

class _Model:
    """Caches the swing tables of one (character, weapon, attack,
    monster) by the only thing stamina changes: StaminaAdjusted."""

    def __init__(self, character, wpn, atk, mon):
        self.ch = dict(character)
        self.ch["mastery"] = dict(character["mastery"])
        self.wpn, self.atk, self.mon = wpn, atk, mon
        self.half = self.ch["max_stamina"] // 2
        self._cache = {}

    def at(self, stamina):
        key = stamina if stamina < self.half else -1
        r = self._cache.get(key)
        if r is None:
            ch = dict(self.ch)
            ch["stamina"] = stamina
            st = swing_stats(ch, self.wpn, self.atk, self.mon)
            r = (st, swing_metrics(st, self.atk, self.mon))
            self._cache[key] = r
        return r


def _run_fight(model, start, steps, drain, stop_when_stable=True):
    """Propagate the stamina distribution for `steps` swings.

    Returns (totals, end_mass, lo, hi): totals are the summed per-swing
    expectations, end_mass the stamina distribution afterwards."""
    tot = defaultdict(float)
    lo = hi = None
    mass = {start: 1.0}
    n = 0
    while n < steps:
        new = defaultdict(float)
        for s, m in mass.items():
            st, mt = model.at(s)
            for k in ("p_hit", "e_dmg", "e_dmg2", "e_cost", "e_ticks",
                      "e_xp"):
                tot[k] += m * mt[k]
            if mt["min_hit"] is not None:
                lo = mt["min_hit"] if lo is None else min(lo, mt["min_hit"])
                hi = mt["max_hit"] if hi is None else max(hi, mt["max_hit"])
            if drain:
                for c, pc in st["cost"].items():
                    new[max(s - c, 0)] += m * pc
            else:
                new[s] += m
        n += 1
        stable = (set(new) == set(mass) and
                  all(abs(new[k] - mass[k]) < 1e-15 for k in new))
        mass = dict(new)
        if stable and stop_when_stable and n < steps:
            # steady state: the remaining swings repeat this one
            left = steps - n
            for s, m in mass.items():
                _, mt = model.at(s)
                for k in ("p_hit", "e_dmg", "e_dmg2", "e_cost",
                          "e_ticks", "e_xp"):
                    tot[k] += left * m * mt[k]
            n = steps
    return tot, mass, lo, hi


def exact_rows(character, combos, monster, trials, stamina_drain,
               swings_per_fight=0):
    """Drop-in for damage_calc_gui.simulate(): same arguments (minus the
    seed), same row dicts, but exact.  `trials` only fixes how many
    swings/fights the averages cover (it matters for the drain model);
    there is no sampling noise.  Returns (rows, max_load)."""
    ml = engine.max_load(character)
    rows = []
    for wn, an in combos:
        wpn = engine.WEAPONS[wn]
        atk = engine.ATTACKS[an]
        start = min(character["stamina"], character["max_stamina"])
        model = _Model(character, wpn, atk, monster)
        if stamina_drain and swings_per_fight > 0:
            full, rem = divmod(trials, swings_per_fight)
            parts = []
            tf, mf, lo1, hi1 = _run_fight(model, start, swings_per_fight,
                                          True)
            parts.append((full, tf, mf, lo1, hi1))
            fights = full
            if rem:
                tr, mr, lo2, hi2 = _run_fight(model, start, rem, True)
                parts.append((1, tr, mr, lo2, hi2))
                fights += 1
            total = defaultdict(float)
            end_total = 0.0
            lo = hi = None
            for cnt, t, mass, l, h in parts:
                if cnt == 0:
                    continue
                for k, v in t.items():
                    total[k] += cnt * v
                end_total += cnt * sum(s * m for s, m in mass.items())
                if l is not None:
                    lo = l if lo is None else min(lo, l)
                    hi = h if hi is None else max(hi, h)
            end_stamina = end_total / fights if fights else 0.0
        else:
            t, mass, lo, hi = _run_fight(model, start, trials,
                                         bool(stamina_drain))
            total = t
            end_stamina = sum(s * m for s, m in mass.items())
        n = float(trials)
        e_hit = total["p_hit"]
        e_dmg = total["e_dmg"]
        rows.append({
            "combo": "%s+%s" % (wn, an),
            "weapon": wn,
            "attack": an,
            "hit_pct": 100.0 * e_hit / n if n else 0.0,
            "dmg_per_try": e_dmg / n if n else 0.0,
            "dmg_per_hit": e_dmg / max(e_hit, 1e-300) if e_hit > 0 else 0.0,
            "min_hit": lo if lo is not None else 0,
            "max_hit": hi if hi is not None else 0,
            "dps": 6 * e_dmg / max(total["e_ticks"], 1e-300)
            if total["e_ticks"] else 0.0,
            "dmg_per_sta": e_dmg / total["e_cost"]
            if total["e_cost"] else 0.0,
            "sta_per_try": total["e_cost"] / n if n else 0.0,
            "xp_per_try": total["e_xp"] / n if n else 0.0,
            "xps": 6 * total["e_xp"] / total["e_ticks"]
            if total["e_ticks"] else 0.0,
            "end_stamina": end_stamina,
        })
    return rows, ml


def dps_exact(character, weapon, attack, monster):
    """DPS of one swing at the character's stamina, no drain (what
    dps_graph needs: a fresh arm per swing)."""
    model = _Model(character, engine.WEAPONS[weapon],
                   engine.ATTACKS[attack], monster)
    _, mt = model.at(min(character["stamina"], character["max_stamina"]))
    return 6 * mt["e_dmg"] / mt["e_ticks"] if mt["e_ticks"] else 0.0


if __name__ == "__main__":
    import time
    t0 = time.time()
    rows, ml = exact_rows(engine.CHARACTER, engine.COMBOS, engine.MONSTER,
                          1, False)
    print("MaxLoad=%d  (exact, %.2fs)" % (ml, time.time() - t0))
    print("%-24s %6s %8s %8s %6s %6s %8s" % (
        "combo", "hit%", "dmg/try", "dmg/hit", "min", "max", "dps"))
    for r in rows:
        print("%-24s %6.1f %8.2f %8.2f %6d %6d %8.2f" % (
            r["combo"], r["hit_pct"], r["dmg_per_try"], r["dmg_per_hit"],
            r["min_hit"], r["max_hit"], r["dps"]))
