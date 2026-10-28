#!/usr/bin/env python3
"""Tkinter GUI for damage_calc.py (CSB / Dungeon Master damage simulator).

Lets the user:
  * tick which weapons and, indented directly below each weapon, which
    of that weapon's attack methods to test (only valid weapon+attack
    pairs are offered and simulated),
  * pick the monster to attack (armor / dex / level difficulty /
    non-material are shown and remain editable; hit points are shown
    as computed from Base Health x level multiplier),
  * edit all character details (strength, dexterity, luck, load,
    stamina, max stamina, ouches, skill masteries, trials, seed,
    stamina drain) or load a champion from a DM/CSB savegame,
  * with stamina drain on, set the fight length (swings per fight;
    every fight starts at the pane's current stamina and TRIALS is
    rounded up to whole fights, so TRIALS can stay large for stable
    averages while modelling single fights),
  * run the Monte-Carlo simulation from ``damage_calc`` and inspect
    the result table (click a column heading to sort), including the
    weapon weight limits for the current character.

Run with:  python3 damage_calc_gui.py
"""

import copy
import os
import random
import re
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import damage_calc as engine
import savefile
import adf
import dps_graph


# ---------------------------------------------------------------- data model

def collect_monsters():
    """Return [(label, dict), ...] for MONSTER, MONSTER1..MONSTER25."""
    monsters = []
    if hasattr(engine, "MONSTER"):
        monsters.append(("MONSTER (default: %s)" % engine.MONSTER.get("name", "?"),
                         copy.deepcopy(engine.MONSTER)))
    numbered = []
    for attr in dir(engine):
        if attr.startswith("MONSTER") and attr[7:].isdigit():
            numbered.append(attr)
    numbered.sort(key=lambda a: int(a[7:]))
    for attr in numbered:
        m = getattr(engine, attr)
        if isinstance(m, dict) and "name" in m:
            monsters.append(("%s (%s)" % (attr, m["name"]), copy.deepcopy(m)))
    return monsters


MONSTER_CHOICES = collect_monsters()
WEAPON_NAMES = sorted(engine.WEAPONS.keys())
ATTACK_NAMES = sorted(engine.ATTACKS.keys())
MASTERY_KEYS = sorted(
    {sk for a in engine.ATTACKS.values() for sk in (a.get("skill"),)}
    | set(engine.CHARACTER.get("mastery", {}).keys())
)


def derive_weapon_attacks():
    """Return {weapon: [attack, ...]} of valid pairs.

    Parsed from the (active *and* commented-out) ("weapon", "attack")
    entries of the COMBOS section in damage_calc.py, so the GUI offers
    exactly the pairs the engine's config knows about. Falls back to
    every weapon x every attack if parsing fails.
    """
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "damage_calc.py")
    try:
        with open(path) as fh:
            src = fh.read()
    except OSError:
        return {w: list(ATTACK_NAMES) for w in WEAPON_NAMES}
    m = re.search(r"COMBOS\s*=\s*\[(.*?)\]", src, re.S)
    if not m:
        return {w: list(ATTACK_NAMES) for w in WEAPON_NAMES}
    pairs = re.findall(r'\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\)', m.group(1))
    mapping = {}
    for wn, an in pairs:
        if wn not in engine.WEAPONS or an not in engine.ATTACKS:
            continue
        mapping.setdefault(wn, [])
        if an not in mapping[wn]:
            mapping[wn].append(an)
    for w in WEAPON_NAMES:
        mapping.setdefault(w, list(ATTACK_NAMES))
    return mapping


WEAPON_ATTACKS = derive_weapon_attacks()


def weight_limits(max_load):
    """Return (full_bonus, no_penalty) weapon-weight limits.

    Port of the weight branches in DetermineThrowingDistance
    (Character.cpp:2204-2225):
      * weight <= MaxLoad/16: full weight bonus (weight - 12), so
        heavier is strictly better up to this limit;
      * weight <= (D5-12)/2 + D5: reduced but still non-negative bonus;
      * heavier: -2 per excess unit penalty.
    Weights are in 0.1 kg units, like engine.WEAPONS["weight"].
    """
    d5 = max_load // 16
    w2 = int((d5 - 12) / 2) + d5  # C++ truncating division, like (D5W-12)/2
    return d5, w2


def simulate(character, combos, monster, trials, seed, stamina_drain,
             swings_per_fight=0):
    """Run the engine Monte-Carlo loop over explicit (weapon, attack) pairs.

    Each combo starts at character["stamina"] (clamped to max); with
    stamina_drain on and swings_per_fight > 0, stamina is restored
    to that starting value at the start of every further fight, so
    TRIALS should be a multiple of swings_per_fight (the GUI rounds
    up automatically) and end_stamina reports one full fight's drain.

    Returns (rows, max_load). Each row is a dict with keys:
    combo, weapon, attack, hit_pct, dmg_per_try, dmg_per_hit,
    min_hit, max_hit, dps, dmg_per_sta, sta_per_try, end_stamina.
    min_hit/max_hit are the smallest/largest damage of a single
    successful hit (0/0 when the combo never hit). sta_per_try is
    the average stamina spent per attack attempt. end_stamina is
    the average end-of-fight stamina over all fights (like every
    other value, an average over all trials)."""
    old_drain = engine.STAMINA_DRAIN
    engine.STAMINA_DRAIN = stamina_drain
    try:
        random.seed(seed)
        ml = engine.max_load(character)
        rows = []
        for wn, an in combos:
            ch = copy.deepcopy(character)
            # Every fight starts at the pane's current stamina (a tired
            # fighter stays tired); the periodic restore also goes
            # back to that value, not to max.
            ch["stamina"] = min(ch["stamina"], ch["max_stamina"])
            start_stamina = ch["stamina"]
            wpn = engine.WEAPONS[wn]
            # allow GUI overrides of P7/P8 etc.? No - use engine tables.
            atk = engine.ATTACKS[an]
            hits = tot = 0
            ticks = spent = xp = 0
            min_hit = None
            max_hit = 0
            end_total = 0
            fights = 0
            for n in range(trials):
                if (stamina_drain and swings_per_fight > 0
                        and n > 0 and n % swings_per_fight == 0):
                    # Next fight: record this fight's end stamina,
                    # then reset to the starting stamina.
                    end_total += ch["stamina"]
                    fights += 1
                    ch["stamina"] = start_stamina
                dmg, hit, cost = engine.one_attack(ch, wpn, atk, monster)
                tot += dmg
                hits += hit
                spent += cost
                ticks += atk["time"] if hit else atk["time"] / 2
                # Experience for the improved skill (AdjustSkills,
                # Magic.cpp:211-240): base gain, halved when the attack
                # is unsuccessful (Attack.cpp:1829-1832), plus the
                # damage bonus on a hit ((dmg x monsterExp/16) + 3,
                # Attack.cpp:887); the peaceful halving never triggers
                # in a real fight, then the level multiplier (0 = no-op)
                # and the danger doubling (attacked within 25 ticks)
                # always apply.
                gained = atk["exp"] if hit else atk["exp"] // 2
                if hit:
                    gained += (dmg * monster["exp"]) // 16 + 3
                mult = monster["level_difficulty"]
                if mult != 0:
                    gained *= mult
                xp += gained * 2
                if hit:
                    if min_hit is None or dmg < min_hit:
                        min_hit = dmg
                    if dmg > max_hit:
                        max_hit = dmg
            end_total += ch["stamina"]
            fights += 1
            rows.append({
                "combo": "%s+%s" % (wn, an),
                "weapon": wn,
                "attack": an,
                "hit_pct": 100.0 * hits / trials if trials else 0.0,
                "dmg_per_try": tot / trials if trials else 0.0,
                "dmg_per_hit": tot / max(hits, 1),
                "min_hit": min_hit if min_hit is not None else 0,
                "max_hit": max_hit,
                "dps": 6 * tot / max(ticks, 1),
                "dmg_per_sta": tot / max(spent, 1),
                "sta_per_try": spent / trials if trials else 0.0,
                "xp_per_try": xp / trials if trials else 0.0,
                "xps": 6 * xp / max(ticks, 1),
                "end_stamina": end_total / fights if fights else 0.0,
            })
        return rows, ml
    finally:
        engine.STAMINA_DRAIN = old_drain


# ---------------------------------------------------------------- GUI

class DamageCalcGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Damage Calculator - CSB")
        self.minsize(1000, 640)

        # ---- state vars: character ----
        dch = engine.CHARACTER
        self.v_strength = tk.IntVar(value=dch["strength"])
        self.v_dexterity = tk.IntVar(value=dch["dexterity"])
        self.v_luck = tk.IntVar(value=dch["luck"])
        self.v_load = tk.IntVar(value=dch["load"])
        self.v_stamina = tk.IntVar(value=dch["stamina"])
        self.v_max_stamina = tk.IntVar(value=dch["max_stamina"])
        self.v_ouches = tk.IntVar(value=dch["ouches"])
        self.v_boots = tk.BooleanVar(value=dch.get("elven_boots", False))
        self.v_mastery = {}
        for k in MASTERY_KEYS:
            self.v_mastery[k] = tk.IntVar(
                value=dch.get("mastery", {}).get(k, 0))

        # ---- state vars: simulation ----
        self.v_trials = tk.IntVar(value=20000)
        self.v_seed = tk.IntVar(value=engine.SEED)
        self.v_drain = tk.BooleanVar(value=engine.STAMINA_DRAIN)
        # swings modelled per fight before stamina is restored; only
        # used when stamina drain is on (0 = never restore)
        self.v_fight_len = tk.IntVar(value=50)
        # result-table sorting (column key + direction)
        self._rows = []
        self._sort_col = "dps"
        self._sort_rev = True

        # ---- state vars: monster ----
        self.v_monster = tk.StringVar(
            value=MONSTER_CHOICES[0][0] if MONSTER_CHOICES else "")
        start_mon = copy.deepcopy(
            MONSTER_CHOICES[0][1] if MONSTER_CHOICES else
            {"name": "?", "armor": 0, "dex": 0,
             "level_difficulty": 1, "non_material": False})
        self.v_mon_name = tk.StringVar(value=start_mon["name"])
        self.v_mon_armor = tk.IntVar(value=start_mon["armor"])
        self.v_mon_dex = tk.IntVar(value=start_mon["dex"])
        self.v_mon_level = tk.IntVar(value=start_mon["level_difficulty"])
        self.v_mon_nonmat = tk.BooleanVar(value=start_mon["non_material"])

        # ---- state vars: savegame champions ----
        # Filled by "Load from save..."; picking an entry copies
        # that champion's stats into the character pane.
        self._champions = []
        self.v_champion = tk.StringVar(value="")
        # Last folder a savegame was successfully loaded from; the
        # file dialog returns there on subsequent loads.
        self._last_save_dir = None
        # File-type filter to preselect ("save" or "adf"); the last
        # used one is moved to the front of the dialog's list.
        self._last_file_kind = None
        # Experience multiplier of the loaded savegame's level
        # (party dict "exp_mult"); None until a save is loaded.
        self._party_mult = None

        # ---- state vars: weapons / attacks ----
        # One tick-box per valid (weapon, attack) pair; the attack boxes
        # are shown indented directly below their weapon. The weapon box
        # is a select-all toggle for its attacks.
        self.v_weapons = {w: tk.BooleanVar(value=False) for w in WEAPON_NAMES}
        self.v_combo = {(w, a): tk.BooleanVar(value=False)
                        for w in WEAPON_NAMES for a in WEAPON_ATTACKS[w]}
        # pre-tick the engine's default COMBOS so first Run shows something
        for wn, an in getattr(engine, "COMBOS", []):
            if (wn, an) in self.v_combo:
                self.v_combo[(wn, an)].set(True)
        if not any(v.get() for v in self.v_combo.values()):
            w0 = WEAPON_NAMES[0]
            self.v_combo[(w0, WEAPON_ATTACKS[w0][0])].set(True)
        self._sync_weapon_boxes()

        self._build_layout()
        self._on_monster_selected()
        self._update_fight_len_state()
        self._update_weight_info()
        # weight limits depend on strength/stamina: refresh live
        for var in (self.v_strength, self.v_stamina, self.v_max_stamina):
            var.trace_add("write", lambda *a: self._update_weight_info())

    # -- layout ------------------------------------------------------
    def _build_layout(self):
        root = ttk.Frame(self, padding=8)
        root.pack(fill="both", expand=True)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(1, weight=1)

        # ===== top bar: sim settings + run =====
        top = ttk.Frame(root)
        top.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        ttk.Label(top, text="Trials:").pack(side="left")
        ttk.Spinbox(top, from_=1, to=1000000, textvariable=self.v_trials,
                    width=9).pack(side="left", padx=(2, 10))
        ttk.Label(top, text="Seed:").pack(side="left")
        ttk.Spinbox(top, from_=0, to=9999999, textvariable=self.v_seed,
                    width=9).pack(side="left", padx=(2, 10))
        ttk.Checkbutton(top, text="Stamina drain",
                        variable=self.v_drain,
                        command=self._update_fight_len_state).pack(
            side="left", padx=(0, 6))
        ttk.Label(top, text="Swings/fight:").pack(side="left")
        self._fight_spin = ttk.Spinbox(top, from_=0, to=100000,
                                       textvariable=self.v_fight_len,
                                       width=7)
        self._fight_spin.pack(side="left", padx=(2, 10))
        ttk.Label(top, text="Click a result heading to sort.").pack(
            side="left")
        ttk.Button(top, text="Run simulation",
                   command=self.on_run).pack(side="right")
        ttk.Button(top, text="Graph DPS vs Str",
                   command=self.on_graph).pack(side="right",
                                               padx=(0, 6))
        ttk.Button(top, text="Select all",
                   command=self.select_all).pack(side="right", padx=(0, 6))
        ttk.Button(top, text="Clear",
                   command=self.select_none).pack(side="right", padx=(0, 6))

        # ===== resizable main columns (drag the sash between panes) =====
        panes = ttk.PanedWindow(root, orient="horizontal")
        panes.grid(row=1, column=0, sticky="nsew")

        # ===== left: character =====
        char = ttk.LabelFrame(panes, text="Character", padding=8)
        char.rowconfigure(1, weight=1)
        char.columnconfigure(0, weight=1)
        # Savegame loader stays fixed at the top; only the fields
        # below it scroll.
        loader = ttk.Frame(char)
        loader.grid(row=0, column=0, sticky="ew", pady=(0, 6))
        loader.columnconfigure(1, weight=1)
        ttk.Button(loader, text="Load from save...",
                   command=self.on_load_save).grid(row=0, column=0,
                                                   padx=(0, 6))
        self._champ_combo = ttk.Combobox(
            loader, textvariable=self.v_champion, state="disabled",
            values=[], width=22)
        self._champ_combo.grid(row=0, column=1, sticky="ew")
        self._champ_combo.bind("<<ComboboxSelected>>",
                               lambda e: self.on_champion_selected())
        self.v_partyinfo = tk.StringVar(value="")
        ttk.Label(loader, textvariable=self.v_partyinfo).grid(
            row=1, column=0, columnspan=2, sticky="w")
        ttk.Button(loader, text="Show inventory...",
                   command=self.on_show_inventory).grid(
            row=2, column=0, columnspan=2, sticky="ew", pady=(4, 0))
        body = ttk.Frame(char)
        body.grid(row=1, column=0, sticky="nsew")
        body.rowconfigure(0, weight=1)
        body.columnconfigure(0, weight=1)
        # Scrollable content so narrowing the pane scrolls instead of
        # clipping (same helper as the weapons pane; the h-scrollbar
        # auto-hides while everything fits).
        ccanvas, cinner = self._scrollable(body)
        fields = [
            ("Strength", self.v_strength, 0, 2000),
            ("Dexterity", self.v_dexterity, 0, 2000),
            ("Luck", self.v_luck, 0, 2000),
            ("Load", self.v_load, 0, 5000),
            ("Stamina", self.v_stamina, 0, 5000),
            ("Max stamina", self.v_max_stamina, 1, 5000),
            ("Ouches", self.v_ouches, 0, 20),
        ]
        for i, (label, var, lo, hi) in enumerate(fields):
            ttk.Label(cinner, text=label).grid(row=i, column=0, sticky="w")
            ttk.Spinbox(cinner, from_=lo, to=hi, textvariable=var,
                        width=8).grid(row=i, column=1, sticky="ew", pady=1)
        ttk.Checkbutton(cinner, text="Elven Boots",
                        variable=self.v_boots).grid(
            row=len(fields), column=0, columnspan=2, sticky="w", pady=1)
        ttk.Separator(cinner).grid(row=len(fields) + 1, column=0,
                                   columnspan=2, sticky="ew", pady=6)
        ttk.Label(cinner, text="Skill masteries").grid(
            row=len(fields) + 2, column=0, columnspan=2, sticky="w")
        for j, k in enumerate(MASTERY_KEYS):
            ttk.Label(cinner, text="skill %d" % k).grid(
                row=len(fields) + 3 + j, column=0, sticky="w")
            ttk.Spinbox(cinner, from_=0, to=30,
                        textvariable=self.v_mastery[k],
                        width=8).grid(row=len(fields) + 3 + j, column=1,
                                      sticky="ew", pady=1)
        ttk.Separator(cinner).grid(row=len(fields) + 3 + len(MASTERY_KEYS),
                                   column=0, columnspan=2, sticky="ew", pady=6)
        ttk.Label(cinner, text="Monster", font=("", 10, "bold")).grid(
            row=len(fields) + 4 + len(MASTERY_KEYS), column=0,
            columnspan=2, sticky="w")
        ttk.Combobox(cinner, textvariable=self.v_monster, width=28,
                     values=[label for label, _ in MONSTER_CHOICES],
                     state="readonly").grid(
            row=len(fields) + 5 + len(MASTERY_KEYS), column=0,
            columnspan=2, sticky="ew", pady=2)
        ttk.Button(cinner, text="Load monster stats",
                   command=self._on_monster_selected).grid(
            row=len(fields) + 6 + len(MASTERY_KEYS), column=0,
            columnspan=2, sticky="ew", pady=2)
        mon_fields = [
            ("Name", self.v_mon_name, None),
            ("Armor", self.v_mon_armor, (0, 999)),
            ("Dex", self.v_mon_dex, (0, 999)),
            ("Level diff.", self.v_mon_level, (1, 9)),
        ]
        base = len(fields) + 7 + len(MASTERY_KEYS)
        for j, (label, var, rng) in enumerate(mon_fields):
            ttk.Label(cinner, text=label).grid(row=base + j, column=0,
                                               sticky="w")
            if rng is None:
                ttk.Entry(cinner, textvariable=var, width=10).grid(
                    row=base + j, column=1, sticky="ew", pady=1)
            else:
                ttk.Spinbox(cinner, from_=rng[0], to=rng[1],
                            textvariable=var, width=8).grid(
                    row=base + j, column=1, sticky="ew", pady=1)
        ttk.Checkbutton(cinner, text="Non-material",
                        variable=self.v_mon_nonmat).grid(
            row=base + len(mon_fields), column=0, columnspan=2,
            sticky="w", pady=2)
        self.v_hpinfo = tk.StringVar()
        ttk.Label(cinner, textvariable=self.v_hpinfo,
                  wraplength=300).grid(
            row=base + len(mon_fields) + 1, column=0, columnspan=2,
            sticky="w", pady=(2, 0))
        ttk.Button(cinner, text="Use savegame level mult",
                   command=self.on_use_save_mult).grid(
            row=base + len(mon_fields) + 2, column=0, columnspan=2,
            sticky="ew", pady=2)
        self.v_monster.trace_add("write",
                                 lambda *a: self._on_monster_selected())
        self.v_mon_level.trace_add("write",
                                   lambda *a: self._update_hp_info())

        # ===== middle: weapons with their attack methods =====
        mid = ttk.Frame(panes)
        mid.rowconfigure(0, weight=1)
        mid.columnconfigure(0, weight=1)

        wbox = ttk.LabelFrame(mid, text="Weapons & attack methods",
                              padding=6)
        wbox.grid(row=0, column=0, sticky="nsew")
        wbox.rowconfigure(0, weight=1)
        wbox.columnconfigure(0, weight=1)
        wcanvas, winner = self._scrollable(wbox)
        for w in WEAPON_NAMES:
            info = engine.WEAPONS[w]
            wtxt = "%s  (str+%d, wt=%d%s)" % (
                w, info.get("ubyte2", 0), info.get("weight", 0),
                ", armor/-%d" % info["armor_div"] if info.get("armor_div")
                else (" (vorpal)" if info.get("vorpal") else ""))
            ttk.Checkbutton(winner, text=wtxt, style="Weapon.TCheckbutton",
                            variable=self.v_weapons[w],
                            command=lambda w=w: self.on_weapon_toggle(w)
                            ).pack(anchor="w", pady=(6 if w != WEAPON_NAMES[0]
                                                   else 0, 0))
            for a in WEAPON_ATTACKS[w]:
                ainfo = engine.ATTACKS[a]
                extra = " [disrupt]" if ainfo.get("disrupt") else ""
                atxt = "%s  (P7=%d P8=%d skill=%d t=%d e=%d)%s" % (
                    a, ainfo["P7"], ainfo["P8"], ainfo["skill"],
                    ainfo["time"], ainfo["energy"], extra)
                ttk.Checkbutton(winner, text=atxt,
                                variable=self.v_combo[(w, a)],
                                command=lambda w=w: self.on_attack_toggle(w)
                                ).pack(anchor="w", padx=(24, 0))

        # ===== right: results =====
        res = ttk.LabelFrame(panes, text="Results", padding=6)
        res.rowconfigure(2, weight=1)
        res.columnconfigure(0, weight=1)
        self.v_status = tk.StringVar(value="Tick weapons/attacks, then Run.")
        ttk.Label(res, textvariable=self.v_status,
                  wraplength=420).grid(row=0, column=0, sticky="w")
        self.v_weightinfo = tk.StringVar()
        ttk.Label(res, textvariable=self.v_weightinfo,
                  wraplength=420).grid(row=1, column=0, sticky="w",
                                       pady=(2, 4))
        cols = ("combo", "hit%", "dmg/try", "dmg/hit", "min_hit",
                "max_hit", "dps", "dmg/sta", "sta/try", "xp/try", "xps",
                "end_stam")
        self.tree = ttk.Treeview(res, columns=cols, show="headings",
                                 height=22)
        widths = {"combo": 170, "hit%": 60, "dmg/try": 70, "dmg/hit": 70,
                  "min_hit": 60, "max_hit": 60,
                  "dps": 70, "dmg/sta": 70, "sta/try": 70,
                  "xp/try": 70, "xps": 60,
                  "end_stam": 70}
        for c in cols:
            self.tree.heading(c, text=c,
                              command=lambda c=c: self.sort_by_column(c))
            # stretch=True + minwidth keeps every column user-resizable
            # by dragging the heading separator; refresh_tree() never
            # touches widths so manual sizes survive sorting/re-runs.
            self.tree.column(c, width=widths[c], minwidth=20,
                             stretch=True, anchor="e")
        self.tree.column("combo", anchor="w")
        self.tree.grid(row=2, column=0, sticky="nsew")
        sb = ttk.Scrollbar(res, orient="vertical",
                           command=self.tree.yview)
        sb.grid(row=2, column=1, sticky="ns")

        def _update_sb(first, last):
            # Auto-hide the scrollbar when all rows fit (same
            # helper pattern as the character/weapons panes).
            sb.set(first, last)
            if float(first) <= 0.0 and float(last) >= 1.0:
                sb.grid_remove()
            else:
                sb.grid()

        self.tree.configure(yscrollcommand=_update_sb)
        # bold font for the per-weapon toggle rows
        s = ttk.Style(self)
        try:
            s.configure("Weapon.TCheckbutton", font=("", 9, "bold"))
        except tk.TclError:
            pass
        # add panes last so requested sizes are honoured. Character and
        # weapons keep fixed widths (weight=0); only results absorbs
        # extra space when the window grows. Sashes stay draggable.
        #
        # Size the weapons/character canvases to their content so no
        # horizontal scrollbar shows initially (measured, not a magic
        # number, so it stays correct across fonts/DPIs). Dragging a
        # pane narrower later re-shows its scrollbar via auto-hide.
        # Heights are fitted the same way so all weapons are visible
        # at startup; capped at 85% of the screen height so small
        # displays still work (the auto-hiding scrollbar covers any
        # overflow). The window then maps at its full requested size.
        self.update_idletasks()
        wcanvas.configure(width=winner.winfo_reqwidth())
        ccanvas.configure(width=cinner.winfo_reqwidth())
        max_h = int(self.winfo_screenheight() * 0.85)
        wcanvas.configure(height=min(winner.winfo_reqheight(), max_h))
        ccanvas.configure(height=min(cinner.winfo_reqheight(), max_h))
        panes.add(char, weight=0)
        panes.add(mid, weight=0)
        panes.add(res, weight=1)

    @staticmethod
    def _scrollable(parent):
        canvas = tk.Canvas(parent, highlightthickness=0)
        inner = ttk.Frame(canvas)
        vsb = ttk.Scrollbar(parent, orient="vertical",
                            command=canvas.yview)
        hsb = ttk.Scrollbar(parent, orient="horizontal",
                            command=canvas.xview)
        canvas.configure(yscrollcommand=vsb.set)
        canvas.grid(row=0, column=0, sticky="nsew")
        vsb.grid(row=0, column=1, sticky="ns")
        hsb.grid(row=1, column=0, sticky="ew")

        def _update_vsb(first, last):
            # Auto-hide the vertical scrollbar when everything fits.
            vsb.set(first, last)
            if float(first) <= 0.0 and float(last) >= 1.0:
                vsb.grid_remove()
            else:
                vsb.grid()

        def _update_hsb(first, last):
            # Auto-hide the horizontal scrollbar when everything fits.
            hsb.set(first, last)
            if float(first) <= 0.0 and float(last) >= 1.0:
                hsb.grid_remove()
            else:
                hsb.grid()

        canvas.configure(yscrollcommand=_update_vsb,
                         xscrollcommand=_update_hsb)
        win = canvas.create_window((0, 0), window=inner, anchor="nw")
        inner.bind("<Configure>",
                   lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

        def _fit_inner_width(event):
            # Stretch content to the canvas when there is room (so no
            # h-scrolling is needed); let it keep its natural width and
            # scroll horizontally when the pane is narrower than that.
            # Safe: the canvas has a fixed requested width, so this does
            # not feed back into the pane's requested size.
            canvas.itemconfig(win, width=max(event.width,
                                             inner.winfo_reqwidth()))

        canvas.bind("<Configure>", _fit_inner_width)

        def _on_wheel(event):
            # Ignore the wheel when everything already fits (no
            # scrollbar visible) so fully-visible panes stay put.
            try:
                first, last = canvas.yview()
            except tk.TclError:
                return
            if float(first) <= 0.0 and float(last) >= 1.0:
                return
            # Linux sends Button-4 (up) / Button-5 (down); Windows and
            # macOS send MouseWheel with event.delta.
            if getattr(event, "num", None) == 4:
                steps = -1
            elif getattr(event, "num", None) == 5:
                steps = 1
            else:
                d = getattr(event, "delta", 0)
                steps = int(-d / 120) if abs(d) >= 120 else (
                    -1 if d > 0 else 1)
            canvas.yview_scroll(steps, "units")
            return "break"

        def _bind_wheel(_event=None):
            canvas.bind_all("<MouseWheel>", _on_wheel)
            canvas.bind_all("<Button-4>", _on_wheel)
            canvas.bind_all("<Button-5>", _on_wheel)

        def _unbind_wheel(_event=None):
            canvas.unbind_all("<MouseWheel>")
            canvas.unbind_all("<Button-4>")
            canvas.unbind_all("<Button-5>")

        canvas.bind("<Enter>", _bind_wheel)
        canvas.bind("<Leave>", _unbind_wheel)
        return canvas, inner

    # -- actions -----------------------------------------------------
    def _update_fight_len_state(self):
        self._fight_spin.config(
            state="normal" if self.v_drain.get() else "disabled")

    def _update_weight_info(self, *args):
        try:
            ml = engine.max_load(self.current_character())
        except (tk.TclError, ValueError):
            self.v_weightinfo.set("")
            return
        full, nopen = weight_limits(ml)
        self.v_weightinfo.set(
            "MaxLoad=%d: full weight bonus \u2264 %d (%0.1f kg), "
            "no penalty \u2264 %d (%0.1f kg); heavier weapons lose power." % (
                ml, full, full / 10.0, nopen, nopen / 10.0))

    def _on_monster_selected(self, *args):
        for label, m in MONSTER_CHOICES:
            if label == self.v_monster.get():
                self.v_mon_name.set(m["name"])
                self.v_mon_armor.set(m["armor"])
                self.v_mon_dex.set(m["dex"])
                self.v_mon_level.set(m["level_difficulty"])
                self.v_mon_nonmat.set(bool(m.get("non_material", False)))
                break
        self._update_hp_info()

    def on_use_save_mult(self):
        """Set the monster multiplier to the loaded savegame's level."""
        if self._party_mult is None:
            messagebox.showinfo("No savegame",
                                "Load a savegame first.")
            return
        self.v_mon_level.set(self._party_mult)

    def _update_hp_info(self, *args):
        """Refresh the monster hit-point readout.

        HP = BaseHealth x multiplier + 0..BaseHealth//4 (Monster.cpp);
        the multiplier is the pane's level difficulty (a generator can
        override it in-game, Timer.cpp:1759-1763).
        """
        for label, m in MONSTER_CHOICES:
            if label == self.v_monster.get():
                try:
                    mult = self.v_mon_level.get()
                except tk.TclError:
                    return
                base = m.get("base_health", 0)
                lo, hi = engine.monster_hp_range(
                    dict(m, level_difficulty=mult))
                self.v_hpinfo.set(
                    "HP %d-%d (base %d x mult %d)" % (lo, hi, base, mult))
                break

    # -- savegame champion loading ---------------------------------
    @staticmethod
    def _champion_label(ch):
        title = ch["title"].strip()
        return "%s - %s" % (ch["name"], title) if title else ch["name"]

    def on_load_save(self):
        start = savefile.default_path()
        first_dir = (self._last_save_dir
                     or (os.path.dirname(start) if start else None))
        save_entry = ("Savegame", "DMGAME.DAT CSBGAME.DAT *.DAT *.dat")
        adf_entry = ("Amiga disk image", "*.adf *.ADF")
        if self._last_file_kind == "adf":
            types = [adf_entry, save_entry, ("All files", "*")]
        else:
            types = [save_entry, adf_entry, ("All files", "*")]
        path = filedialog.askopenfilename(
            title="Open DM/CSB savegame",
            initialdir=first_dir,
            initialfile=os.path.basename(start) if start else None,
            filetypes=types)
        if not path:
            return
        if path.lower().endswith(".adf"):
            champions, label, party = self._load_champions_from_adf(path)
            if champions is None:
                return
            from_adf = " [%s]" % label
        else:
            try:
                champions = savefile.load_champions(path)
            except ValueError as exc:
                messagebox.showerror("Cannot load savegame", str(exc))
                return
            try:
                party = savefile.load_party(path)
            except ValueError:
                party = None
            from_adf = ""
        self._last_save_dir = os.path.dirname(path)
        self._last_file_kind = ("adf" if path.lower().endswith(".adf")
                                else "save")
        self._party_mult = party["exp_mult"] if party else None
        self.v_partyinfo.set(self._party_label(party))
        self._champions = champions
        labels = [self._champion_label(c) for c in champions]
        self._champ_combo.config(state="readonly", values=labels)
        self.v_champion.set(labels[0])
        self.on_champion_selected()
        if from_adf:
            self.v_status.set(self.v_status.get() + from_adf)

    @staticmethod
    def _party_label(party):
        if not party:
            return ""
        return "Level %d @ (%d, %d) facing %s" % (
            party["level"], party["gx"], party["gy"],
            party["facing_name"])

    @staticmethod
    def _load_champions_from_adf(path):
        """Return ([champions], source label, party) or (None, None, None)."""
        try:
            names = [n for n, _s, _b in adf.list_files(path)]
        except ValueError as exc:
            messagebox.showerror("Cannot read ADF image", str(exc))
            return None, None, None
        pick = adf.pick_savegame(names)
        if pick is None:
            messagebox.showerror(
                "No savegame on disk",
                "%s contains no DMGAME.DAT (has: %s)" % (
                    os.path.basename(path), ", ".join(sorted(names))
                    or "nothing"))
            return None, None, None
        try:
            data = adf.read_file(path, pick)
            champions = savefile.load_champions_bytes(data)
        except ValueError as exc:
            messagebox.showerror(
                "Cannot load savegame",
                "%s inside %s: %s" % (pick, os.path.basename(path), exc))
            return None, None, None
        try:
            party = savefile.load_party_bytes(data)
        except ValueError:
            party = None
        return champions, "%s in %s" % (pick, os.path.basename(path)), party

    def _selected_champion(self):
        label = self.v_champion.get()
        for c in self._champions:
            if self._champion_label(c) == label:
                return c
        return None

    def on_champion_selected(self):
        """Copy the picked savegame champion into the character pane."""
        champ = self._selected_champion()
        label = self.v_champion.get()
        if champ is None:
            return
        self.v_strength.set(champ["strength"])
        self.v_dexterity.set(champ["dexterity"])
        self.v_luck.set(champ["luck"])
        self.v_load.set(champ["load"])
        self.v_stamina.set(champ["stamina"])
        self.v_max_stamina.set(max(champ["max_stamina"], 1))
        self.v_ouches.set(champ["ouches"])
        self.v_boots.set(bool(champ.get("boots", False)))
        for k, var in self.v_mastery.items():
            var.set(champ["mastery"].get(k, 0))
        self._update_weight_info()
        self.v_status.set(
            "Loaded %s (hp %d, mana %d, stamina %d) - "
            "tick weapons/attacks, then Run." % (
                label, champ["hp"], champ["mana"],
                champ["stamina"]))

    def on_show_inventory(self):
        """Show the picked champion's 30 inventory slots."""
        import dmitems
        champ = self._selected_champion()
        if champ is None:
            return
        win = tk.Toplevel(self)
        win.title("Inventory - %s" % self.v_champion.get())
        text = tk.Text(win, width=44, height=32, wrap="none")
        sb = ttk.Scrollbar(win, orient="vertical", command=text.yview)
        text.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        text.pack(side="left", fill="both", expand=True)
        for slot, name in enumerate(champ.get("inventory", [])):
            text.insert("end", "%-12s %s\n" % (
                dmitems.SLOT_NAMES.get(slot, slot),
                name if name else "-"))
        text.configure(state="disabled")

    # -- weapon/attack selection -----------------------------------
    def _sync_weapon_boxes(self):
        """Check a weapon box iff all of its attacks are ticked."""
        for w in WEAPON_NAMES:
            all_on = all(self.v_combo[(w, a)].get()
                         for a in WEAPON_ATTACKS[w])
            self.v_weapons[w].set(all_on)

    def on_weapon_toggle(self, weapon):
        """Weapon box clicked: tick/untick all of its attacks."""
        on = self.v_weapons[weapon].get()
        for a in WEAPON_ATTACKS[weapon]:
            self.v_combo[(weapon, a)].set(on)

    def on_attack_toggle(self, weapon):
        """Attack box clicked: refresh the weapon's select-all box."""
        all_on = all(self.v_combo[(weapon, a)].get()
                     for a in WEAPON_ATTACKS[weapon])
        self.v_weapons[weapon].set(all_on)

    def select_all(self):
        for v in self.v_combo.values():
            v.set(True)
        self._sync_weapon_boxes()

    def select_none(self):
        for v in self.v_combo.values():
            v.set(False)
        self._sync_weapon_boxes()

    def selected_combos(self):
        return [(w, a) for w in WEAPON_NAMES for a in WEAPON_ATTACKS[w]
                if self.v_combo[(w, a)].get()]

    def current_character(self):
        return {
            "strength": self.v_strength.get(),
            "dexterity": self.v_dexterity.get(),
            "luck": self.v_luck.get(),
            "load": self.v_load.get(),
            "stamina": self.v_stamina.get(),
            "max_stamina": max(self.v_max_stamina.get(), 1),
            "ouches": self.v_ouches.get(),
            "elven_boots": bool(self.v_boots.get()),
            "mastery": {k: v.get() for k, v in self.v_mastery.items()},
        }

    def current_monster(self):
        exp = 0
        for label, m in MONSTER_CHOICES:
            if label == self.v_monster.get():
                exp = m.get("exp", 0)
                break
        return {
            "name": self.v_mon_name.get(),
            "armor": self.v_mon_armor.get(),
            "dex": self.v_mon_dex.get(),
            "level_difficulty": self.v_mon_level.get(),
            "non_material": bool(self.v_mon_nonmat.get()),
            "exp": exp,
        }

    # -- result sorting ----------------------------------------------
    COL_KEYS = {"combo": "combo", "hit%": "hit_pct", "dmg/try": "dmg_per_try",
                "dmg/hit": "dmg_per_hit", "min_hit": "min_hit",
                "max_hit": "max_hit", "dps": "dps", "dmg/sta": "dmg_per_sta",
                "sta/try": "sta_per_try", "xp/try": "xp_per_try",
                "xps": "xps", "end_stam": "end_stamina"}

    def sort_by_column(self, col):
        """Heading clicked: sort by that column, toggling direction."""
        if col == self._sort_col:
            self._sort_rev = not self._sort_rev
        else:
            self._sort_col = col
            # numbers: best first; names: A-Z first
            self._sort_rev = col != "combo"
        self.refresh_tree()

    def refresh_tree(self):
        key = self.COL_KEYS[self._sort_col]
        rows = sorted(self._rows,
                      key=lambda r: r[key],
                      reverse=self._sort_rev)
        for item in self.tree.get_children():
            self.tree.delete(item)
        for r in rows:
            self.tree.insert("", "end", values=(
                r["combo"], "%.1f" % r["hit_pct"],
                "%.2f" % r["dmg_per_try"], "%.2f" % r["dmg_per_hit"],
                r["min_hit"], r["max_hit"],
                "%.2f" % r["dps"], "%.2f" % r["dmg_per_sta"],
                "%.2f" % r["sta_per_try"], "%.2f" % r["xp_per_try"],
                "%.2f" % r["xps"], "%.1f" % r["end_stamina"]))
        for c in self.tree["columns"]:
            arrow = (" \u25bc" if self._sort_rev else " \u25b2"
                     ) if c == self._sort_col else ""
            self.tree.heading(c, text=c + arrow)

    def on_graph(self):
        """Plot DPS vs strength for the single ticked combo."""
        combos = self.selected_combos()
        if len(combos) != 1:
            messagebox.showinfo(
                "Graph DPS vs strength",
                "Tick exactly one weapon+attack for the graph.")
            return
        wn, an = combos[0]
        try:
            trials = self.v_trials.get()
            seed = self.v_seed.get()
        except tk.TclError:
            messagebox.showerror("Invalid input",
                                 "Trials and seed must be numbers.")
            return
        trials = min(max(trials, 1), 10000)
        monster = self.current_monster()
        base = self.current_character()
        self.v_status.set("Computing graph for %s+%s ..." % (wn, an))
        self.update_idletasks()
        self.config(cursor="watch")
        try:
            rows = dps_graph.compute(wn, an, monster, 10, 120, trials,
                                     seed, base, step=2)
        finally:
            self.config(cursor="")
        win = tk.Toplevel(self)
        win.title("DPS vs strength - %s+%s vs %s" % (
            wn, an, monster["name"]))
        dps_graph.show_in(win, rows, wn, an, monster["name"])
        self.v_status.set("Graphed %s+%s (strength 10-120)." % (wn, an))

    def on_run(self):
        combos = self.selected_combos()
        if not combos:
            messagebox.showwarning("Nothing selected",
                                   "Tick at least one weapon attack.")
            return
        try:
            character = self.current_character()
            monster = self.current_monster()
            trials = self.v_trials.get()
            seed = self.v_seed.get()
            fight_len = self.v_fight_len.get()
        except tk.TclError:
            messagebox.showerror("Invalid input",
                                 "Character/monster fields must be numbers.")
            return
        if trials < 1:
            messagebox.showerror("Invalid input",
                                 "Trials must be >= 1.")
            return
        drain = self.v_drain.get()
        if drain and fight_len < 0:
            messagebox.showerror("Invalid input",
                                 "Swings/fight must be >= 0 "
                                 "(0 = never restore).")
            return
        # Whole fights only: round trials up to a multiple of the
        # fight length so no partial fight skews the averages.
        snapped = False
        if drain and fight_len > 0 and trials % fight_len != 0:
            trials += fight_len - trials % fight_len
            self.v_trials.set(trials)
            snapped = True
        self.v_status.set("Running %d combos x %d trials ..."
                          % (len(combos), trials))
        self.update_idletasks()
        rows, ml = simulate(character, combos, monster,
                            trials, seed, drain, fight_len)
        self._rows = rows
        self._sort_col = "dps"
        self._sort_rev = True
        self.refresh_tree()
        self._update_weight_info()
        if drain:
            if fight_len > 0:
                extra = (" | drain on, %d x %d swings/fight%s" % (
                    trials // fight_len, fight_len,
                    " (trials rounded up)" if snapped else ""))
            else:
                extra = " | drain on, no restore"
        else:
            extra = ""
        self.v_status.set(
            "MaxLoad=%d  str=%d dex=%d luck=%d load=%d | Monster: %s "
            "(armor=%d dex=%d lvl=%d%s) | %d combos%s" % (
                ml, character["strength"], character["dexterity"],
                character["luck"], character["load"], monster["name"],
                monster["armor"], monster["dex"],
                monster["level_difficulty"],
                ", non-material" if monster["non_material"] else "",
                len(rows), extra))


def main():
    app = DamageCalcGUI()
    app.mainloop()


if __name__ == "__main__":
    main()
