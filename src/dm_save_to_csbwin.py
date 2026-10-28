#!/usr/bin/env python3
"""Make a classic Amiga Dungeon Master (2.0) save loadable by CSBwin.

CSBwin reads classic saves through its own converter, but that converter
follows the inventory ``m_link`` chains with DuplicateOK=false and aborts
("SAVED GAME DAMAGED") because DM stores a champion's 30 possession slots
as a linked list (slot records already enumerate the items, so following
the links re-visits them).

This tool rewrites the save, in place, so CSBwin's normal loader accepts it:

  1. Block1 is reshaped from the DM layout to the CSB layout.  The only
     structural difference is RandomGameID (i16 in DM, i32 in CSB), which
     shifts every field from offset 310 on by 2 bytes.  The block's
     self-check word and the scrambling key are recomputed.
  2. Every object reachable *only* through a champion possession gets its
     ``m_link`` set to RNeof, so each inventory/backpack slot is processed
     as an independent item (map objects and container contents keep their
     chains untouched).
  3. The embedded dungeon's byte-sum checksum is recomputed.

CSBwin then loads the file directly; its File -> Save game writes a true
CSBwin-native (Extended Features) save.

Run with --help for options.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import savefile

RNeof = 0xFFFE
RNnul = 0xFFFF
# DM 2.0 stores old-format actuator (8 B) and scroll (4 B) records;
# CSBwin expands them itself, so locate the later DBs with these sizes.
DB_SIZES = dict(savefile.DB_ENTRY_SIZES)
DB_SIZES[3] = 8
DB_SIZES[7] = 4


def be16(buf, off):
    return (buf[off] << 8) | buf[off + 1]


def setbe16(buf, off, val):
    buf[off] = (val >> 8) & 0xFF
    buf[off + 1] = val & 0xFF


def reshape_block1(data, off, le):
    """DM-layout Block1 -> CSB-layout Block1, re-scrambled with key 0."""
    plain = savefile._decode_block1(bytes(data[off:off + 512]), le)
    csb = bytearray(512)
    csb[0:310] = plain[0:310]
    csb[310:312] = b"\x00\x00"           # RandomGameID high word
    csb[312:512] = plain[310:510]        # rest shifts by one word
    setbe16(csb, 20, 0)                  # DM key slot -> 0
    setbe16(csb, 58, 0)                  # CSB key slot -> 0

    # Self-check: combine(first 128 words) must equal sum(second 128).
    tail_sum = sum(be16(csb, 256 + 2 * i) for i in range(128)) & 0xFFFF
    comb = 0
    for g in range(31):
        comb = (comb + be16(csb, 8 * g)) & 0xFFFF
        comb ^= be16(csb, 8 * g + 2)
        comb = (comb - be16(csb, 8 * g + 4)) & 0xFFFF
        comb ^= be16(csb, 8 * g + 6)
    group31 = (comb + be16(csb, 248)) & 0xFFFF
    group31 = (group31 ^ be16(csb, 250)) & 0xFFFF
    group31 = (group31 - be16(csb, 252)) & 0xFFFF
    setbe16(csb, 254, group31 ^ tail_sum)   # compensation word
    tail = bytearray(csb[256:512])
    savefile._unscramble(tail, 0, 128, le)  # XOR: self-inverse
    csb[256:512] = tail
    savefile._decode_block1(bytes(csb), le)  # verify
    data[off:off + 512] = csb


def dungeon_db_layout(data, dung_off):
    """(counts, areas, objlist, stride) for the embedded dungeon copy."""
    dung = data[dung_off:]
    num_level = dung[4]
    nwords = be16(dung, 6)
    objlen = be16(dung, 10)
    counts = [be16(dung, 12 + 2 * i) for i in range(16)]
    ncols = 0
    for i in range(num_level):
        w = be16(dung, 44 + 16 * i + 8)
        ncols += ((w >> 6) & 0x1F) + 1
    ci = 44 + 16 * num_level
    ol = ci + 2 * ncols
    objlist = [be16(dung, ol + 2 * i) for i in range(objlen)]
    areas = []
    o = ol + 2 * objlen + 2 * nwords
    for db in range(16):
        areas.append(o)
        o += counts[db] * DB_SIZES[db]
    return counts, areas, objlist


def fix_possessions(data, b2, dung_off, counts, areas, objlist, le, coff,
                    champ_size, hashes, checks, num_char):
    """Break m_link on possession-only objects. Returns count changed."""
    dung = data[dung_off:]

    def entry(rn):
        db = (rn >> 10) & 15
        idx = rn & 0x3FF
        if idx >= counts[db]:
            return None
        return areas[db] + idx * DB_SIZES[db]

    def link(rn):
        e = entry(rn)
        return None if e is None else ((dung[e] << 8) | dung[e + 1])

    def sub(db, idx):
        e = areas[db] + idx * DB_SIZES[db]
        v = (dung[e + 2] << 8) | dung[e + 3]
        return None if v in (RNeof, 0) else v

    def magic(rn):
        return ((rn >> 10) & 15) == 11

    map_set = set()

    def walk_map(rn):
        for _ in range(6000):
            if rn is None or rn in (0, RNeof, RNnul) or magic(rn):
                return
            db, idx = (rn >> 10) & 15, rn & 0x3FF
            if idx >= counts[db] or (db, idx) in map_set:
                return
            map_set.add((db, idx))
            if db == 9:
                walk_map(sub(9, idx))
            elif db == 4:
                walk_map(sub(4, idx))
            elif db == 14:
                f = sub(14, idx)
                if f is not None and not magic(f):
                    walk_map(f)
            rn = link(rn)

    for e in objlist:
        if e not in (RNeof, RNnul, 0):
            walk_map(e)

    champ = savefile._decode_stream(
        bytes(data[coff:coff + champ_size]), hashes, checks, le)
    hand = savefile._u16(b2, 8, le)
    slots = []
    stride = 800
    for i in range(num_char):
        rec = champ[stride * i:stride * (i + 1)]
        for j in range(30):
            v = savefile._u16(rec, 212 + 2 * j, le)
            if v not in (0, RNeof, RNnul):
                slots.append(v)
    if hand not in (0, RNeof, RNnul):
        slots.append(hand)

    broken = 0
    for v in slots:
        if ((v >> 10) & 15, v & 0x3FF) in map_set:
            continue
        e = entry(v)
        if e is not None and ((dung[e] << 8) | dung[e + 1]) != RNeof:
            setbe16(dung, e, RNeof)
            broken += 1
    data[dung_off:dung_off + len(dung)] = dung
    return broken


def build_parser():
    epilog = (
        "examples:\n"
        "  %(prog)s DMGame.dat\n"
        "      convert DMGame.dat to DMGame_csbwin.dat\n\n"
        "  %(prog)s DMGame.dat ../Dungeons/Dungeonmaster/csbgame.dat\n"
        "      convert straight into CSBwin's first save slot\n\n"
        "  %(prog)s my.dm2 my.csb\n"
        "      convert an explicit input to an explicit output\n\n"
        "then in CSBwin: DUNGEON -> RESUME -> select the file (or File -> Load "
        "saved game),\nand File -> Save game to write a CSBwin-native "
        "(Extended Features) save."
    )
    p = argparse.ArgumentParser(
        prog="dm_save_to_csbwin.py",
        description="Rewrite a classic Amiga Dungeon Master (2.0) savegame so "
                    "CSBwin can load it.",
        epilog=epilog,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "input",
        help="input DM savegame, e.g. DMGame.dat")
    p.add_argument(
        "output", nargs="?",
        help="output file (default: <input>_csbwin.dat)")
    return p


def main(argv=None):
    args = build_parser().parse_args(argv)
    src = args.input
    dst = args.output or os.path.splitext(src)[0] + "_csbwin.dat"

    if not os.path.isfile(src):
        raise SystemExit("error: input file not found: %s" % src)
    if os.path.abspath(src) == os.path.abspath(dst):
        raise SystemExit("error: output must differ from the input path")

    data = bytearray(open(src, "rb").read())

    off, le, ho, co = savefile._find_classic(data)
    print("input : %s (%d bytes) classic@%d le=%s hashes@%d checks@%d"
          % (src, len(data), off, le, ho, co))
    if len(data) >= 512 and data[:20] == savefile._EFB_SENTINEL:
        sys.exit("refusing: input already has a CSBwin Extended Features header")
    if (ho, co) != (310, 342):
        sys.exit("refusing: input is already in CSB block layout")

    reshape_block1(data, off, le)

    off, le, ho, co = savefile._find_classic(data)
    sec = savefile._sections(data, le, 312, 344, off)
    counts, areas, objlist = dungeon_db_layout(data, sec["dung_off"])
    num_char = savefile._u16(sec["b2"], 10, le)
    broken = fix_possessions(data, sec["b2"], sec["dung_off"], counts, areas,
                             objlist, le, sec["coff"], sec["champ_size"],
                             sec["hashes"][2], sec["checks"][2], num_char)
    print("broke %d possession m_link chains" % broken)

    dung = data[sec["dung_off"]:]
    setbe16(dung, len(dung) - 2, sum(dung[:-2]) & 0xFFFF)
    data[sec["dung_off"]:sec["dung_off"] + len(dung)] = dung

    open(dst, "wb").write(bytes(data))
    # sanity: champions must still decode
    names = [c["name"] for c in savefile.load_champions_bytes(bytes(data))]
    print("output: %s  champions=%s" % (dst, ", ".join(names)))
    print("Load it in CSBwin (DUNGEON -> RESUME -> select file), then "
          "File -> Save game to write a CSBwin-native save.")


if __name__ == "__main__":
    main()
