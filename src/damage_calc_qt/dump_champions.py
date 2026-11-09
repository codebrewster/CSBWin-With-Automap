#!/usr/bin/env python3
"""Print the champions (and party info) of a DM/CSB savegame or .adf image
as JSON, for the Qt damage calculator.  Uses ../savefile.py and ../adf.py.

Usage: dump_champions.py FILE
"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import adf        # noqa: E402
import savefile   # noqa: E402


def main():
    path = sys.argv[1]
    label = ""
    try:
        if path.lower().endswith(".adf"):
            names = [n for n, _s, _b in adf.list_files(path)]
            pick = adf.pick_savegame(names)
            if pick is None:
                raise ValueError("%s contains no DMGAME.DAT (has: %s)" % (
                    os.path.basename(path), ", ".join(sorted(names)) or "nothing"))
            data = adf.read_file(path, pick)
            champions = savefile.load_champions_bytes(data)
            try:
                party = savefile.load_party_bytes(data)
            except ValueError:
                party = None
            label = "%s in %s" % (pick, os.path.basename(path))
        else:
            champions = savefile.load_champions(path)
            try:
                party = savefile.load_party(path)
            except ValueError:
                party = None
    except (ValueError, OSError) as exc:
        json.dump({"error": str(exc)}, sys.stdout)
        return
    out = []
    for c in champions:
        a = c.get("attributes", {})
        out.append({
            "name": c["name"], "title": c["title"],
            "strength": c["strength"], "dexterity": c["dexterity"],
            "luck": c["luck"],
            "luck_min": a.get("luck", {}).get("min", 10),
            "luck_max": a.get("luck", {}).get("max", 100),
            "load": c["load"], "stamina": c["stamina"],
            "max_stamina": c["max_stamina"], "ouches": c["ouches"],
            "hp": c["hp"], "mana": c["mana"], "boots": bool(c.get("boots")),
            "mastery": {str(k): v for k, v in c["mastery"].items()},
            "inventory": c.get("inventory", []),
        })
    p = None
    if party:
        p = {"level": party["level"], "gx": party["gx"], "gy": party["gy"],
             "facing": party["facing_name"], "exp_mult": party["exp_mult"]}
    try:
        import dmitems
        slots = {str(k): v for k, v in dmitems.SLOT_NAMES.items()}
    except Exception:
        slots = {}
    json.dump({"champions": out, "party": p, "source": label,
               "slot_names": slots}, sys.stdout)


main()
