#!/usr/bin/env python3
# Writes records.html beside the statistics pages of export-all-html.sh: the Records tab over all
# the replays of each game. One row per record kind, for the party and for each champion: the
# lowest value above 0, the average and the highest, each with the replay it came from and the
# hero, the weapon or spell and the monster behind it. Reads the statistics.html files under
# OUTDIR (their embedded JSON, the whole run only); stdlib only.
#
#   scripts/records-overview.py OUTDIR
#
# The games are told by the top folder of a replay, as export-all-html.sh does:
#   Chaos Strikes Back minimum item picking, Race to Fulya Pit -> Chaos Strikes Back
#   Dungeon Master minimum item picking, Race to fusion        -> Dungeon Master
# Other folders are left out. Values: a record that does not apply (null, or below 0) is not
# counted; the lowest counts only values above 0; the average and the highest count every value.
import html
import json
import os
import re
import sys

GAME_OF_FOLDER = {
    "Chaos Strikes Back minimum item picking": "Chaos Strikes Back",
    "Race to Fulya Pit": "Chaos Strikes Back",
    "Dungeon Master minimum item picking": "Dungeon Master",
    "Race to fusion": "Dungeon Master",
}
GAME_ORDER = ["Chaos Strikes Back", "Dungeon Master"]
PARTY = "Party"


def read_stats(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    m = re.search(r'<script type="application/json" id="stats">(.*?)</script>', text, re.S)
    return json.loads(m.group(1)) if m else None


def collect(out_dir, style):
    """Returns {game: {subject: {label: [entry, ...]}}} and the replay count per game.
    An entry: value, replay (folder name), holder, how, whom, when, level, order (the number's place)."""
    data = {}
    replays = {}
    for root, _, files in os.walk(out_dir):
        if "statistics.html" not in files:
            continue
        rel = os.path.relpath(root, out_dir).split(os.sep)
        game = GAME_OF_FOLDER.get(rel[0])
        if game is None:
            continue
        stats = read_stats(os.path.join(root, "statistics.html"))
        if not stats or "-1" not in stats.get("views", {}):
            continue
        if style[0] is None:
            with open(os.path.join(root, "statistics.html"), encoding="utf-8") as f:
                m = re.search(r"<style>(.*?)</style>", f.read(), re.S)
                style[0] = m.group(1) if m else None
        replays[game] = replays.get(game, 0) + 1
        replay = rel[-1]
        numbers = stats["numbers"]
        heroes = [h["name"] for h in stats["heroes"]]
        view = stats["views"]["-1"]
        records = view["records"]
        pages = view["pages"]
        rec_first = stats["recFirst"]
        for i in range(len(records)):
            subject = heroes[i] if i < len(heroes) else PARTY
            lines = {}
            for line in pages[i]["records"]:
                lines[line[0]] = line
            for n, info in enumerate(numbers):
                if not info.get("rec") or n >= len(records[i]):
                    continue
                v = records[i][n]
                if v is None or v < 0:
                    continue
                line = lines.get(n - rec_first)
                entry = {
                    "value": v,
                    "replay": replay,
                    "holder": line[4] if line else subject,
                    "how": line[5] if line else "",
                    "whom": line[6] if line else "",
                    "when": line[7] if line else "",
                    "level": line[8] if line else None,
                    "order": n,
                }
                by_subject = data.setdefault(game, {}).setdefault(subject, {})
                by_subject.setdefault(info["label"], []).append(entry)
    return data, replays


def summarize(entries):
    """(lowest above 0, average, highest) entries; None where there is no value."""
    above = [e for e in entries if e["value"] > 0]
    low = min(above, key=lambda e: e["value"]) if above else None
    high = max(entries, key=lambda e: e["value"]) if entries else None
    avg = sum(e["value"] for e in entries) / len(entries) if entries else None
    return low, avg, high


def plural(n):
    return f"{n} replay" if n == 1 else f"{n} replays"


def fmt(v):
    if v is None:
        return "-"
    return f"{v:,.0f}" if v == int(v) else f"{v:,.1f}"


def cell(entry):
    """One value of a record: the number, then who and how, the target (the column On / from),
    the level, the tick, and the replay it came from."""
    if entry is None:
        return "<td class='z'>-</td>"
    who_how = " · ".join(x for x in (entry.get("holder"), entry.get("how")) if x)
    target = " · ".join(x for x in (entry.get("whom"), f"Lv {entry['level']}" if entry.get("level") is not None else "",
                                    entry.get("when")) if x)
    return ("<td class='l'>"
            f"<div class='gold'>{fmt(entry['value'])}</div>"
            f"<div>{html.escape(who_how)}</div>"
            f"<div class='nt'>{html.escape(target)}</div>"
            f"<div class='nt'>{html.escape(entry['replay'])}</div>"
            "</td>")


def subject_table(rows):
    body = []
    for label, entries in rows:
        low, avg, high = summarize(entries)
        avg_cell = (f"<td class='l'><div class='gold'>{fmt(avg)}</div>"
                    f"<div class='nt'>over {plural(len(entries))}</div></td>"
                    if avg is not None else "<td class='z'>-</td>")
        body.append(f"<tr><td class='l'>{html.escape(label)}</td>{cell(low)}{avg_cell}{cell(high)}</tr>")
    return ("<div class='wide'><table><tr><th class='l'>Record</th><th class='l'>Lowest above 0</th>"
            "<th class='l'>Average</th><th class='l'>Highest</th></tr>"
            f"{''.join(body)}</table></div>")


def page(data, replays, style):
    games = [g for g in GAME_ORDER if g in data]
    game_tabs, panels = [], []
    for gi, game in enumerate(games):
        subjects = data[game]
        names = sorted(x for x in subjects if x != PARTY)
        order = [PARTY] + names
        who_tabs = "".join(
            f"<button type='button' class='who' data-who='{si}' aria-pressed='{str(si == 0).lower()}'>{html.escape(x)}</button>"
            for si, x in enumerate(order))
        sections = []
        for si, subject in enumerate(order):
            rows = sorted(subjects[subject].items(), key=lambda kv: min(e["order"] for e in kv[1]))
            sections.append(f"<div class='who-view' data-who='{si}' hidden>{subject_table(rows)}</div>")
        game_tabs.append(f"<button type='button' class='game-tab' data-game='{gi}' aria-pressed='{str(gi == 0).lower()}'>"
                         f"{html.escape(game)} <span class='nt'>{plural(replays[game])}</span></button>")
        panels.append(f"<div class='game-view' data-game='{gi}' hidden>"
                      f"<div class='whos'>{who_tabs}</div>"
                      f"<div class='sec'><div class='sech'><span class='t'>{html.escape(game)}</span>"
                      f"<span class='s'>the record tabs of {plural(replays[game])}, whole run</span></div>"
                      f"<div class='secb'>{''.join(sections)}</div></div></div>")
    # the bar stays put; #view (the statistics page's scrolling area) holds the tables
    body = ("<div class='bar1'><div class='seg'>" + "".join(game_tabs) + "</div></div>"
            + "<div id='view'>" + "".join(panels) + "</div>"
            if panels else "<p class='about'>No statistics.html files found.</p>")
    css = style or ""
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CSB records over all replays</title>
<style>{css}
  .wide {{ overflow-x:auto }}
  .game-tab[aria-pressed="true"], .who[aria-pressed="true"] {{ background:#2a3a57; border-color:#5b8fd9; color:#fff }}
  [hidden] {{ display:none !important }}
</style></head>
<body>
<h1>Records</h1>
<div class="about">The record tabs over every replay of each game, whole run. The lowest counts only values above 0;
the average and the highest count every value. Records that did not apply are not counted.
A closest call is best when lowest.</div>
<div class="im">
{body}
</div>
<script>
(function () {{
  const games = [...document.querySelectorAll('.game-tab')];
  const views = [...document.querySelectorAll('.game-view')];
  function selectGame(i) {{
    games.forEach(b => b.setAttribute('aria-pressed', String(b.dataset.game === i)));
    views.forEach(v => v.hidden = v.dataset.game !== i);
    const panel = views.find(v => v.dataset.game === i);
    if (panel) selectWho(panel, panel.querySelector('.who').dataset.who);
  }}
  function selectWho(panel, w) {{
    panel.querySelectorAll('.who').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.who === w)));
    panel.querySelectorAll('.who-view').forEach(v => v.hidden = v.dataset.who !== w);
  }}
  games.forEach(b => b.addEventListener('click', () => selectGame(b.dataset.game)));
  views.forEach(panel => panel.querySelectorAll('.who').forEach(b =>
    b.addEventListener('click', () => selectWho(panel, b.dataset.who))));
  if (games.length) selectGame(games[0].dataset.game);
}})();
</script>
</body></html>
"""


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: records-overview.py OUTDIR")
    out_dir = sys.argv[1]
    style = [None]
    data, replays = collect(out_dir, style)
    with open(os.path.join(out_dir, "records.html"), "w", encoding="utf-8") as f:
        f.write(page(data, replays, style[0]))
    print(f"records.html written to {os.path.join(out_dir, 'records.html')} "
          + ", ".join(f"{g}: {replays[g]} replays" for g in GAME_ORDER if g in replays))


if __name__ == "__main__":
    main()
