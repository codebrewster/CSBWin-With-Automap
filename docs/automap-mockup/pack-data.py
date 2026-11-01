#!/usr/bin/env python3
"""Pack an automap export into the data block of the reference mockup.

1. Apply export.patch (made against commit 56446fc) to a scratch copy of the repo and build it
   (make RELEASE=1 in src).
2. In a game folder, write a case file with one line, "save <savegame>", plus any "at ..."
   line, and run:  CSB_MAP_EXPORT=<outdir> <scratch>/src/csb --visibility-test case.txt
   It loads the save without a window, writes level<N>.json and the pictures as .bmp, and exits.
3. Convert the pictures:  cd <outdir>; mkdir -p png; for f in *.bmp; do convert "$f" "png/${f%.bmp}.png"; done
4. python3 -I pack-data.py <outdir> > dmdata.js
5. Replace the <script>window.DM=...</script> block in automap-mockup.html with it.
"""
import base64, json, os, sys

def main(outdir):
    used = set()

    def walk(o):
        if isinstance(o, dict):
            for v in o.values():
                if isinstance(v, str) and v.endswith('.bmp'):
                    used.add(v)
                else:
                    walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)

    levels = []
    level = 0
    while os.path.exists(os.path.join(outdir, 'level%d.json' % level)):
        d = json.load(open(os.path.join(outdir, 'level%d.json' % level)))
        for c in d['cells']:
            c['sides'] = [({k: v for k, v in s.items() if k != 'text'}
                           if any(s.get(k) for k in ('pic', 'writing', 'hidden', 'portrait', 'alcove', 'functional'))
                           else {}) for s in c['sides']]
        d.update(x0=0, y0=0, w=d['width'], h=d['height'])
        walk(d)
        levels.append(d)
        level += 1
    sprites = {}
    for name in sorted(used):
        png = os.path.join(outdir, 'png', name[:-4] + '.png')
        if os.path.exists(png):
            sprites[name] = 'data:image/png;base64,' + base64.b64encode(open(png, 'rb').read()).decode()
    sys.stdout.write('window.DM=' + json.dumps({'levels': levels, 'sprites': sprites}, separators=(',', ':')) + ';')

if __name__ == '__main__':
    main(sys.argv[1])
