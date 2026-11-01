#!/usr/bin/env python3
"""Build automap-mockup.html from the page sources and the map data.

The page is page/head.html, page/map.js and page/ui.js. The data is what the real map code shows
for one or more saves, exported by export.patch: per save, index.json and one draw list per
level, vision and spoiler level (L<level>_v<vision>_s<spoilers>.json; vision 0 fog, 1 explored
live, 2 whole level; spoilers 0 All, 1 Learned, 2 Seen), with every cell's tooltip and links,
plus the game's pictures as .bmp.

Change only the drawing (page/*), keep the data:
    python3 -I pack-data.py
New data (another save, a custom dungeon, or after the map code changed):
1. git worktree add <scratch> HEAD; cd <scratch>; git apply docs/automap-mockup/export.patch;
   cd src; make RELEASE=1
2. In a game folder (dungeon.dat, graphics.dat, the save and its .map), per save:
     printf 'save <save>\\nat L0 (4,12) facing S light 0\\n' > case.txt
     CSB_MAP_EXPORT=<outdir>/<save> <scratch>/src/csb --visibility-test case.txt
   It loads the save without a window, writes the export and exits.
3. cd <outdir>/<save>; mkdir -p png; for f in *.bmp; do convert "$f" "png/${f%.bmp}.png"; done
4. python3 -I pack-data.py <outdir>/<save> [<outdir>/<save2> ...]
   The save buttons in page/head.html name the saves in this order.

Each save is stored gzip-compressed (the page unpacks it with DecompressionStream), the pictures
once for all saves.
"""
import base64, gzip, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = os.path.join(HERE, 'automap-mockup.html')
DATA_BLOCK = re.compile(r'<script id="data">\n(.*?)\n</script>', re.S)


def pack(dirs):
    sprites = {}
    saves = {}
    for d in dirs:
        name = os.path.basename(os.path.normpath(d))
        index = json.load(open(os.path.join(d, 'index.json')))
        views = {}
        for f in sorted(os.listdir(d)):
            if f.startswith('L') and f.endswith('.json'):
                views[f[:-5]] = json.load(open(os.path.join(d, f)))
        blob = json.dumps({'index': index, 'views': views}, separators=(',', ':')).encode()
        saves[name] = base64.b64encode(gzip.compress(blob, 9, mtime=0)).decode()
        png_dir = os.path.join(d, 'png')
        for f in sorted(os.listdir(png_dir)):
            key = f[:-4] + '.bmp'
            if key not in sprites:
                sprites[key] = 'data:image/png;base64,' + base64.b64encode(open(os.path.join(png_dir, f), 'rb').read()).decode()
    return 'window.MAPDATA=' + json.dumps({'saves': saves, 'sprites': sprites}, separators=(',', ':')) + ';'


def build(data):
    part = lambda name: open(os.path.join(HERE, 'page', name)).read()
    return (part('head.html') + '<script id="data">\n' + data + '\n</script>\n<script>\n'
            + part('map.js') + part('ui.js') + '\n</script>\n</body></html>\n')


if __name__ == '__main__':
    if sys.argv[1:]:
        data = pack(sys.argv[1:])
    else:
        found = DATA_BLOCK.search(open(PAGE).read())
        if not found:
            sys.exit('automap-mockup.html has no data block; pass export folders')
        data = found.group(1)
    open(PAGE, 'w').write(build(data))
    print('wrote', PAGE, os.path.getsize(PAGE), 'bytes')
