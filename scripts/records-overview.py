#!/usr/bin/env python3
# Writes records.html beside the statistics pages of export-all-html.sh: every number of the
# statistics pages ranked over the replays, one tab per category (the top folder), with the top
# three runs at each end, the average, and a link to each run's statistics.html. Reads the
# statistics.html files under OUTDIR (their embedded JSON, the whole run only) and puts their numbers
# into scripts/records-page.html, whose script ranks them (per run, per hour, one champion).
# Stdlib only.
#
#   scripts/records-overview.py OUTDIR
#
# The categories are the top folders below; other folders are left out. A replay that is in a
# folder twice (the same numbers) counts once, and the runs in EXCLUDED or with "cheat" in their
# folder name hold no record (the page names them and why).
import json, os, re, sys

CATEGORIES = ["Race to Fulya Pit", "Race to fusion",
              "Chaos Strikes Back minimum item picking", "Dungeon Master minimum item picking"]
GAME = {"Race to Fulya Pit": "Chaos Strikes Back", "Chaos Strikes Back minimum item picking": "Chaos Strikes Back",
        "Race to fusion": "Dungeon Master", "Dungeon Master minimum item picking": "Dungeon Master"}


# Runs that hold no record: the movie (the folder name up to " - ") and why. A folder whose name
# says "cheating" is left out as well.
EXCLUDED = {
    "CSBMovie0640": "used about 10 saves, says its readme.txt",
}


def excluded(name):
    movie = name.split(" - ")[0]
    if movie in EXCLUDED:
        return EXCLUDED[movie]
    return "by cheating" if "cheat" in name.lower() else ""


def read_stats(path):
    with open(path, encoding="utf-8") as f:
        m = re.search(r'<script type="application/json" id="stats">(.*?)</script>', f.read(), re.S)
    return json.loads(m.group(1)) if m else None


def player_of(name):
    parts = name.split(" - ")
    if len(parts) < 2:
        return "Unknown player"
    return parts[1].split(" (")[0].strip()


def num(v):
    if v is None:
        return None
    return round(v, 2) if isinstance(v, float) and v != int(v) else int(v)


def extras(view, pages, heroes):
    """The best entry of the detail tables in one run: {key: [value, what, who]}."""
    e = {}
    def best(key, value, what, who=""):
        if value and (key not in e or value > e[key][0]):
            e[key] = [value, what, who]
    weapons, spells = {}, {}
    for hi, p in enumerate(pages[:len(heroes)]):
        for w in p.get("weapons", []):
            d = weapons.setdefault(w[0], [0, 0, 0])
            d[0] += w[3]; d[1] += w[4]; d[2] += w[1]
            best("heroWeapon", w[3], w[0], heroes[hi])
        for sp in p.get("spells", []):
            spells[sp[0]] = spells.get(sp[0], 0) + sp[1]
    for name, (dmg, kills, tries) in weapons.items():
        best("weaponDamage", dmg, name)
        best("weaponKills", kills, name)
        best("weaponTries", tries, name)
    for name, casts in spells.items():
        best("spellCasts", casts, name)
    if spells:
        e["spellKinds"] = [len([c for c in spells.values() if c]), "", ""]
    kinds = 0
    for m in view.get("monsters", []):
        best("monsterKind", m[7], m[0])
        kinds += 1 if m[7] else 0
    e["monsterKinds"] = [kinds, "", ""]
    items = view.get("items", [])
    e["itemKinds"] = [len([i for i in items if i[1]]), "", ""]
    for i in items:
        best("itemMost", i[1], i[0])
    party = pages[-1]
    for f in party.get("eaten", []):
        best("food", f[1], f[0])
    for d in party.get("drunk", []):
        best("drink", d[1], d[0])
    return e


def collect(out_dir):
    runs = {c: [] for c in CATEGORIES}
    meta = None
    for root, _, files in os.walk(out_dir):
        if "statistics.html" not in files:
            continue
        rel = os.path.relpath(root, out_dir).split(os.sep)
        if rel[0] not in runs:
            continue
        s = read_stats(os.path.join(root, "statistics.html"))
        if not s or "-1" not in s.get("views", {}):
            continue
        if meta is None:
            meta = {"numbers": [{k: n[k] for k in ("label", "help", "party", "run", "bad", "ratio", "pct", "ticks", "rec", "unit")}
                                for n in s["numbers"]],
                    "groups": [{"tab": t["label"], "title": g["title"], "rows": g["rows"]}
                               for t in s["topics"] if t["label"] != "Overview" for g in t["groups"]],
                    "recFirst": s["recFirst"]}
        view = s["views"]["-1"]
        heroes = [h["name"] for h in s["heroes"]]
        pages = view["pages"]
        def rec_lines(page):
            return {str(l[0] + s["recFirst"]): [l[4], l[5], l[6], l[7]] for l in page.get("records", [])}
        name = rel[-1]
        runs[rel[0]].append({
            "n": name, "p": player_of(name), "h": heroes,
            "x": excluded(name),
            "u": "/".join(rel) + "/statistics.html",
            "v": [num(x) for x in view["records"][-1]],
            "hv": [[num(x) for x in view["records"][i]] for i in range(len(heroes))],
            "r": rec_lines(pages[-1]),
            "hr": [rec_lines(pages[i]) for i in range(len(heroes))],
            "e": extras(view, pages, heroes),
            "ch": sum(i[3] for i in view.get("items", []) if len(i) > 3),
        })
    # one copy of a replay that is in a folder twice (the same numbers): the shorter name stays
    for c in runs:
        keep = {}
        for r in sorted(runs[c], key=lambda r: ("annotated" in r["n"].lower(), len(r["n"]))):
            key = json.dumps(r["v"])
            if key in keep:
                keep[key].setdefault("also", []).append(r["n"])
            else:
                keep[key] = r
        runs[c] = sorted(keep.values(), key=lambda r: r["n"])
    return meta, [{"name": c, "game": GAME[c], "race": c.startswith("Race"), "runs": runs[c]} for c in CATEGORIES if runs[c]]


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: records-overview.py OUTDIR")
    out_dir = sys.argv[1]
    template = os.path.join(os.path.dirname(os.path.abspath(__file__)), "records-page.html")
    dest = os.path.join(out_dir, "records.html")
    meta, cats = collect(out_dir)
    if meta is None:
        sys.exit(f"no statistics.html files of the known categories under {out_dir}")
    data = json.dumps({"meta": meta, "cats": cats}, separators=(",", ":")).replace("</", "<\\/")
    with open(template, encoding="utf-8") as f:
        page = f.read().replace("/*DATA*/null", data)
    with open(dest, "w", encoding="utf-8") as f:
        f.write(page)
    left = sum(1 for c in cats for r in c["runs"] if r["x"])
    print(f"records.html written to {dest} "
          + ", ".join(f"{c['name']}: {len(c['runs'])} replays" for c in cats)
          + (f" ({left} left out)" if left else ""))


if __name__ == "__main__":
    main()
