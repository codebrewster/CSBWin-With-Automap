#!/usr/bin/env python3
"""Plot DPS vs strength for a weapon+attack, barefoot vs Elven Boots.

Shows how the boots' MaxLoad bonus moves the throw-weight branches
(light/medium/heavy, DetermineThrowingDistance, Character.cpp:2204),
including the "dip" window where a higher MaxLoad briefly lowers the
medium-branch bonus before flipping to the light branch.

Usage:
  python3 dps_graph.py axe chop
  python3 dps_graph.py morningstar melee --monster Dragon --str-lo 30 --str-hi 110
  python3 dps_graph.py axe chop --no-show   # print table only (headless)

Options: --monster NAME (substring match, default Oitu), --str-lo/hi,
--trials (default 20000), --seed (default 12345).
"""

import argparse
import copy
import random
import sys

import damage_calc as engine


def collect_monsters():
    out = {}
    for attr in dir(engine):
        if attr == "MONSTER" or (attr.startswith("MONSTER")
                                 and attr[7:].isdigit()):
            m = getattr(engine, attr)
            if isinstance(m, dict) and "name" in m:
                out.setdefault(m["name"], m)
    return out


def dps_at(weapon, attack, monster, base_char, strength, trials, seed,
           boots):
    ch = copy.deepcopy(base_char)
    ch["strength"] = strength
    ch["elven_boots"] = boots
    wpn = engine.WEAPONS[weapon]
    atk = engine.ATTACKS[attack]
    random.seed(seed)
    tot = ticks = 0
    for _ in range(trials):
        dmg, hit, _cost = engine.one_attack(dict(ch), wpn, atk,
                                            dict(monster))
        tot += dmg
        ticks += atk["time"] if hit else atk["time"] / 2
        ch["stamina"] = ch["max_stamina"]  # fresh arm per swing
    return 6 * tot / max(ticks, 1)


def compute(weapon, attack, monster, lo, hi, trials, seed,
            base_char=None, step=1):
    if base_char is None:
        base_char = engine.CHARACTER
    rows = []
    for s in range(lo, hi + 1, step):
        rows.append((s,
                     dps_at(weapon, attack, monster, base_char, s,
                            trials, seed, False),
                     dps_at(weapon, attack, monster, base_char, s,
                            trials, seed, True)))
    return rows


def show_in(parent, rows, weapon, attack, monster_name):
    import tkinter as tk
    from tkinter import ttk
    W, H = 920, 620
    ML, MR, MT, MB = 70, 20, 40, 50
    frame = ttk.Frame(parent, padding=4)
    frame.pack(fill="both", expand=True)
    cv = tk.Canvas(frame, width=W, height=H, bg="white")
    cv.pack(fill="both", expand=True)
    xs = [r[0] for r in rows]
    ys = [v for r in rows for v in r[1:]]
    x0, x1 = min(xs), max(xs)
    y0, y1 = 0.0, max(ys) * 1.05 or 1.0

    def px(x):
        return ML + (x - x0) / max(x1 - x0, 1) * (W - ML - MR)

    def py(y):
        return H - MB - (y - y0) / (y1 - y0) * (H - MT - MB)

    for i in range(6):  # horizontal gridlines
        yv = y0 + (y1 - y0) * i / 5
        cv.create_line(px(x0), py(yv), px(x1), py(yv), fill="#dddddd")
        cv.create_text(px(x0) - 8, py(yv), text="%.1f" % yv, anchor="e")
    step = max(1, (x1 - x0) // 10)
    for xv in range(x0, x1 + 1, step):  # vertical gridlines
        cv.create_line(px(xv), py(y0), px(xv), py(y1), fill="#dddddd")
        cv.create_text(px(xv), py(y0) + 16, text=str(xv))
    cv.create_text((W - ML - MR) / 2 + ML, 16,
                   text="DPS vs strength: %s+%s vs %s" % (
                       weapon, attack, monster_name))
    cv.create_text(16, (H - MB - MT) / 2 + MT, text="dps", angle=90)
    cv.create_text((W - ML - MR) / 2 + ML, H - 12, text="strength")
    for col, name, color in ((1, "barefoot", "blue"),
                              (2, "elven boots", "red")):
        pts = []
        for r in rows:
            pts += [px(r[0]), py(r[col])]
        cv.create_line(*pts, fill=color, width=2)
        cv.create_rectangle(W - 190, 44 + 22 * (col - 1),
                            W - 170, 56 + 22 * (col - 1),
                            fill=color, outline=color)
        cv.create_text(W - 162, 50 + 22 * (col - 1), text=name,
                       anchor="w")


def show(rows, weapon, attack, monster_name):
    import tkinter as tk
    root = tk.Tk()
    root.title("DPS vs strength - %s+%s vs %s" % (weapon, attack,
                                                  monster_name))
    show_in(root, rows, weapon, attack, monster_name)
    root.mainloop()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("weapon")
    ap.add_argument("attack")
    ap.add_argument("--monster", default="Oitu")
    ap.add_argument("--str-lo", type=int, default=30)
    ap.add_argument("--str-hi", type=int, default=110)
    ap.add_argument("--trials", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=engine.SEED)
    ap.add_argument("--no-show", action="store_true",
                    help="print table only, no window")
    args = ap.parse_args(argv)
    if args.weapon not in engine.WEAPONS:
        sys.exit("unknown weapon (have: %s)"
                 % ", ".join(sorted(engine.WEAPONS)))
    if args.attack not in engine.ATTACKS:
        sys.exit("unknown attack (have: %s)"
                 % ", ".join(sorted(engine.ATTACKS)))
    mons = collect_monsters()
    match = [m for n, m in mons.items()
             if args.monster.lower() in n.lower()]
    if not match:
        sys.exit("unknown monster (have: %s)"
                 % ", ".join(sorted(mons)))
    monster = match[0]
    rows = compute(args.weapon, args.attack, monster, args.str_lo,
                   args.str_hi, args.trials, args.seed)
    print("str  dps_bare  dps_boots")
    for s, a, b in rows:
        if s % 5 == 0 or s in (args.str_lo, args.str_hi):
            print("%3d  %8.2f  %9.2f" % (s, a, b))
    if not args.no_show:
        show(rows, args.weapon, args.attack, monster["name"])


if __name__ == "__main__":
    raise SystemExit(main())
