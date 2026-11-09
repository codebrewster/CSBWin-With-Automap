#!/usr/bin/env python3
"""Check damage_exact.py (exact) against damage_calc.py (Monte Carlo).

Many random scenarios (character, weapon, attack, monster, stamina,
drain on/off) are run through both.  For every scenario the Monte Carlo
mean damage/try, hit rate, stamina/try and xp/try must lie within a few
standard errors of the exact value (the standard error is computed from
the exact distribution, not estimated), and every Monte Carlo min/max hit
must lie inside the exact support.  Uses all cores.

Run:  python3 verify_exact.py [scenarios] [trials]
"""

import copy
import math
import multiprocessing
import random
import sys

import damage_calc as engine
import damage_exact as ex

ZMAX = 4.5   # |z| above this is a failure (~7e-6 per check)


def scenario(rng, n):
    mons = [v for k, v in sorted(vars(engine).items())
            if k.startswith("MONSTER") and isinstance(v, dict)]
    ch = {
        "strength": rng.choice([8, 20, 35, 55, 70, 90]),
        "dexterity": rng.choice([10, 30, 43, 70]),
        "luck": rng.choice([0, 1, 25, 50, 80]),
        "load": rng.choice([0, 100, 200, 400, 900]),
        "elven_boots": rng.random() < 0.3,
        "max_stamina": rng.choice([200, 750, 1500]),
        "ouches": rng.choice([0, 0, 0, 1]),
        "mastery": {s: rng.randint(0, 15) for s in range(4, 18)},
    }
    ch["stamina"] = rng.choice([ch["max_stamina"], ch["max_stamina"],
                                ch["max_stamina"] // 3,
                                ch["max_stamina"] // 10])
    wn = rng.choice(sorted(engine.WEAPONS))
    an = rng.choice(sorted(engine.ATTACKS))
    mon = dict(rng.choice(mons))
    drain = rng.random() < 0.4
    fight = rng.choice([0, 25, 60]) if drain else 0
    return n, ch, wn, an, mon, drain, fight


def run_mc(ch, wn, an, mon, trials, drain, fight, seed):
    """The same loop as damage_calc_gui.simulate (without the xp-free
    parts it does not need)."""
    import damage_calc_gui as g
    return g.simulate(ch, [(wn, an)], mon, trials, seed, drain, fight)[0][0]


def check(args):
    n, ch, wn, an, mon, drain, fight = args
    trials = TRIALS
    if drain and fight:
        trials -= trials % fight
    random.seed(n)
    old = engine.STAMINA_DRAIN
    try:
        mc = run_mc(ch, wn, an, mon, trials, drain, fight, 1000 + n)
    finally:
        engine.STAMINA_DRAIN = old
    exr = ex.exact_rows(ch, [(wn, an)], mon, trials, drain, fight)[0][0]
    problems = []
    # standard errors from the exact one-swing distribution (stationary
    # approximation is exact for drain-off; for drain it is the mean of
    # per-state variances, which is what matters for the sum)
    model = ex._Model(ch, engine.WEAPONS[wn], engine.ATTACKS[an], mon)
    mt = model.at(min(ch["stamina"], ch["max_stamina"]))[1]
    var_d = max(mt["e_dmg2"] - mt["e_dmg"] ** 2, 0.0)
    var_h = mt["p_hit"] * (1 - mt["p_hit"])
    se_d = math.sqrt(var_d / trials) + 1e-12
    se_h = math.sqrt(var_h / trials) + 1e-12
    # with drain the per-swing law changes over the fight, so only the
    # tolerance (not the centre) is approximate there: widen it.
    widen = 3.0 if drain else 1.0
    for name, a, b, se in (
            ("dmg_per_try", mc["dmg_per_try"], exr["dmg_per_try"], se_d),
            ("hit_pct", mc["hit_pct"] / 100, exr["hit_pct"] / 100, se_h)):
        z = (a - b) / (se * widen)
        if abs(z) > ZMAX:
            problems.append("%s mc=%.5f exact=%.5f z=%.1f" % (name, a, b, z))
    for name in ("sta_per_try", "xp_per_try", "end_stamina"):
        a, b = mc[name], exr[name]
        # relative tolerance for the quantities we have no variance for
        tol = 0.02 * max(abs(b), 1.0) * (3 if drain else 1)
        if abs(a - b) > tol + 0.05:
            problems.append("%s mc=%.4f exact=%.4f" % (name, a, b))
    if mc["max_hit"] > exr["max_hit"] or (
            mc["min_hit"] and mc["min_hit"] < exr["min_hit"]):
        problems.append("MC hit range %d..%d outside exact %d..%d" % (
            mc["min_hit"], mc["max_hit"], exr["min_hit"], exr["max_hit"]))
    if mc["max_hit"] == 0 and exr["max_hit"] != 0 and exr["hit_pct"] > 0.01:
        problems.append("MC never hit but exact says %.3f%%" %
                        exr["hit_pct"])
    if not drain:
        problems += chi_square(ch, wn, an, mon, trials, n)
    return n, wn, an, mon["name"], drain, fight, problems


def chi_square(ch, wn, an, mon, trials, n):
    """Whole damage distribution (0 = no hit) against the exact one."""
    engine.STAMINA_DRAIN = False
    random.seed(77 + n)
    c = dict(ch)
    c["stamina"] = min(c["stamina"], c["max_stamina"])
    wpn, atk = engine.WEAPONS[wn], engine.ATTACKS[an]
    counts = {}
    for _ in range(trials):
        d = engine.one_attack(c, wpn, atk, mon)[0]
        counts[d] = counts.get(d, 0) + 1
    pm = dict(ex.swing_stats(c, wpn, atk, mon)["hit"])
    pm[0] = 1.0 - sum(pm.values())
    # merge cells until expected count >= 20
    cells, acc_e, acc_o = [], 0.0, 0
    for d in sorted(set(pm) | set(counts)):
        acc_e += pm.get(d, 0.0) * trials
        acc_o += counts.get(d, 0)
        if acc_e >= 20:
            cells.append((acc_e, acc_o))
            acc_e, acc_o = 0.0, 0
    if acc_e > 0 or acc_o:
        if cells:
            e, o = cells[-1]
            cells[-1] = (e + acc_e, o + acc_o)
        else:
            cells.append((acc_e, acc_o))
    if len(cells) < 2:
        return []
    chi = sum((o - e) ** 2 / e for e, o in cells if e > 0)
    df = len(cells) - 1
    z = (chi - df) / math.sqrt(2 * df)     # normal approx. of chi-square
    if z > 5:
        return ["damage histogram chi2=%.0f df=%d (z=%.1f)" % (chi, df, z)]
    return []


TRIALS = 200000


def main():
    global TRIALS
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    TRIALS = int(sys.argv[2]) if len(sys.argv) > 2 else 200000
    rng = random.Random(20260101)
    jobs = [scenario(rng, i) for i in range(count)]
    bad = 0
    with multiprocessing.Pool() as pool:
        for n, wn, an, mname, drain, fight, probs in pool.imap_unordered(
                check, jobs):
            tag = "%3d %-14s %-8s %-15s drain=%d fight=%d" % (
                n, wn, an, mname, drain, fight)
            if probs:
                bad += 1
                print("FAIL", tag)
                for p in probs:
                    print("      ", p)
            else:
                print("ok  ", tag)
    print("%d/%d scenarios failed" % (bad, count))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
