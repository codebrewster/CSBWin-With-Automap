#!/usr/bin/env python3
"""Read champion stats from a Dungeon Master / Chaos Strikes Back
savegame (DMGAME.DAT, CSBGAME.DAT, ...) for use in damage_calc_gui.

Format reference: http://dmweb.free.fr/community/documentation/
file-formats/saved-game-files/ plus the loader in this repo
(SaveGame.cpp, CSBCode.cpp:8999 Unscramble, Chaos.cpp load path).

Layout (big-endian versions: Atari ST, Amiga, X68000):
  Block1 (512 B): first 256 B random filler, last 256 B scrambled
    data. The word at offset 20 (DM) or 58 (CSB) is the key that
    decodes the tail; hashes/checksums for the following streams
    live in the decoded tail (Chaos offsets: hashes at 310..318,
    checksums at 342..350).
  Block2 (128 B): descrambled with hash@310, holds numcharacter
    (+10), MaxITEM16 (+46), etc.
  ITEM16 (16 * MaxITEM16 B): hash@312.
  Champion data: hash@314. Size depends on the game version --
    3328 B (4 x 800 + 128, portraits included: Atari DM, Amiga
    DM 2.x, Atari CSB) or 1408 B (4 x 320 + 128, portraits stored
    separately: Amiga DM 3.6, X68000, Amiga CSB). The first 286
    bytes of each record share the CHARDESC layout (CSB.h:2449):
    name[8] @0, title[16] @8, ouches @50, HP @52, maxHP @54,
    stamina @56, maxStamina @58, mana @60, maxMana @62,
    Attributes[7] (max,cur,min triples) @70, skills[20]
    (tempAdjust i16 + experience i32) @92, load u16 @272.

Only big-endian variants are supported (little-endian PC/IIGS
saves decode the same way with LE word assembly).
"""

import os
import struct

import dmitems

# dungeon DB record sizes (packed structs, src/CSB.h). DB11 holds
# 256-byte container records despite its 254 bytes of fields.
DB_ENTRY_SIZES = {0: 4, 1: 6, 2: 4, 3: 10, 4: 16, 5: 4, 6: 4, 7: 6,
                  8: 4, 9: 8, 10: 4, 11: 256, 12: 2, 13: 2, 14: 8,
                  15: 4}
# DescIndex bases (RN::DescIndex, data.cpp): type field + base.
_OBJDI_BASE = {5: 23, 6: 69, 10: 127}
OBJDI_ELVEN_BOOTS = 84
_EFB_SENTINEL = b" Extended Features \x00"

# (stream size, record stride) candidates, big-endian versions.
_CHAMPION_VARIANTS = ((1408, 320), (3328, 800))

_ATTR_NAMES = ("luck", "strength", "dexterity", "wisdom",
               "vitality", "antimagic", "antifire")


def _unscramble(buf, init_hash, num_words, le=False):
    """Port of Unscramble() (CSBCode.cpp:8999).

    Words are assembled big-endian (Atari ST, Amiga) or little-endian
    (PC, CSBwin).  Decodes buf in place; returns the checksum to
    compare against the value stored in Block1.
    """
    d7 = init_hash & 0xFFFF
    d6 = num_words
    d5 = init_hash & 0xFFFF
    for i in range(num_words):
        off = 2 * i
        if le:
            w = (buf[off] | (buf[off + 1] << 8)) & 0xFFFF
        else:
            w = ((buf[off] << 8) | buf[off + 1]) & 0xFFFF
        d5 = (d5 + w) & 0xFFFF
        w2 = w ^ d7
        if le:
            buf[off] = w2 & 0xFF
            buf[off + 1] = (w2 >> 8) & 0xFF
        else:
            buf[off] = (w2 >> 8) & 0xFF
            buf[off + 1] = w2 & 0xFF
        d5 = (d5 + w2) & 0xFFFF
        d7 = (d7 + d6) & 0xFFFF
        d6 -= 1
    return d5


def _u16(buf, off, le=False):
    if le:
        return buf[off] | (buf[off + 1] << 8)
    return (buf[off] << 8) | buf[off + 1]


def _decode_block1(raw, le=False):
    """Decode Block1, trying the DM (offset 20) then CSB (offset 58)
    key. Returns the decoded 512 bytes or raises ValueError."""
    for key_off in (20, 58):
        buf = bytearray(raw)
        tail = bytearray(buf[256:512])
        _unscramble(tail, _u16(buf, key_off, le), 128, le)
        buf[256:512] = tail
        # Verify: checksum over first 256 B (32 groups of 4 words:
        # +=, ^=, -=, ^=) must equal the sum of the decoded tail.
        d6 = 0
        for g in range(32):
            d6 = (d6 + _u16(buf, 8 * g, le)) & 0xFFFF
            d6 ^= _u16(buf, 8 * g + 2, le)
            d6 = (d6 - _u16(buf, 8 * g + 4, le)) & 0xFFFF
            d6 ^= _u16(buf, 8 * g + 6, le)
        d5 = sum(_u16(buf, 256 + 2 * i, le) for i in range(128)) & 0xFFFF
        if d6 == d5:
            return bytes(buf)
    raise ValueError("Block1 checksum mismatch (not a DM/CSB save?)")


def _decode_stream(raw, init_hash, checksum, le=False):
    """Decode one scrambled stream; raises ValueError on mismatch."""
    buf = bytearray(raw)
    if _unscramble(buf, init_hash, len(buf) // 2, le) != checksum:
        raise ValueError("stream checksum mismatch")
    return bytes(buf)


def mastery_from_exp(skill, experiences):
    """Port of DetermineMastery() (Code17818.cpp:13).

    experiences: 20 raw experience values (tempAdjust already
    added by the caller if desired). Subskills (>3) average with
    their base skill (0..3) before the log2 mapping.
    """
    exp = experiences[skill]
    if skill > 3:
        exp = (exp + experiences[(skill - 4) // 4]) // 2
    mastery = 1
    while exp >= 500:
        exp >>= 1
        mastery += 1
    return mastery


def _parse_champion(rec, le=False, csb=False):
    """Parse one 320/800-byte champion record into a dict matching
    damage_calc.CHARACTER plus display fields.

    possessions (30 words at 212) resolve through MCE's RN table
    (dmitems; column 0 = DM names, 1 = CSB names): "inventory" holds
    30 names (None for empty/unknown slots), "boots" tells whether the
    feet slot (5) holds Elven Boots (MaxLoad bonus).  Saved stats
    already include equipped-item bonuses (applied live by
    AdjustStatsForItemCarried, never re-applied on load), so stats
    must NOT be adjusted again."""
    name = bytes(rec[0:8]).split(b"\x00")[0].decode("ascii",
                                                     errors="replace")
    title = bytes(rec[8:24]).split(b"\x00")[0].decode("ascii",
                                                      errors="replace")
    attrs = {}
    for k in range(7):
        attrs[_ATTR_NAMES[k]] = {"max": rec[70 + 3 * k],
                                 "cur": rec[70 + 3 * k + 1],
                                 "min": rec[70 + 3 * k + 2]}
    experiences = []
    for s in range(20):
        so = 92 + 6 * s
        tmp = _u16(rec, so, le)
        if tmp >= 0x8000:
            tmp -= 0x10000
        if le:
            exp = ((rec[so + 5] << 24) | (rec[so + 4] << 16) |
                   (rec[so + 3] << 8) | rec[so + 2]) + tmp
        else:
            exp = ((rec[so + 2] << 24) | (rec[so + 3] << 16) |
                   (rec[so + 4] << 8) | rec[so + 5]) + tmp
        experiences.append(exp)
    result = {
        "name": name,
        "title": title,
        "strength": attrs["strength"]["cur"],
        "dexterity": attrs["dexterity"]["cur"],
        "luck": attrs["luck"]["cur"],
        "attributes": attrs,
        "load": _u16(rec, 272, le),
        "stamina": _u16(rec, 56, le),
        "max_stamina": _u16(rec, 58, le),
        "ouches": _u16(rec, 50, le),
        "hp": _u16(rec, 52, le),
        "max_hp": _u16(rec, 54, le),
        "mana": _u16(rec, 60, le),
        "max_mana": _u16(rec, 62, le),
        "experiences": experiences,
        "mastery": {s: mastery_from_exp(s, experiences)
                    for s in range(20)},
    }
    possessions = [_u16(rec, 212 + 2 * k, le) for k in range(30)]
    result["inventory"] = [dmitems.lookup(v, csb) for v in possessions]
    result["boots"] = possessions[5] in dmitems.ELVEN_BOOTS
    return result


def load_champions(path):
    """Decode all champions in a savegame file. Returns a list of dicts
    (only the party members, i.e. numcharacter entries).

    Raises ValueError with a plain message when the file cannot
    be decoded.
    """
    try:
        with open(path, "rb") as fh:
            data = fh.read()
    except OSError as exc:
        raise ValueError("cannot read %s: %s" % (path, exc))
    try:
        return load_champions_bytes(data)
    except ValueError as exc:
        raise ValueError("%s: %s" % (path, exc))


def load_champions_bytes(data):
    """Decode all champions from savegame bytes (e.g. extracted from
    an .adf image with adf.read_file).  Classic Amiga/Atari saves start
    at offset 0; CSBwin saves carry an " Extended Features " prefix
    which is skipped automatically (endianness and hash/checksum table
    layout are detected from the content).  See load_champions."""
    off, le, ho, co = _find_classic(data)
    hashes = _hashes_at(data, off, ho, le)
    checks = _hashes_at(data, off, co, le)
    try:
        b2 = _decode_stream(data[off + 512:off + 640], hashes[0],
                            checks[0], le)
    except ValueError:
        raise ValueError("Block2 checksum failed")
    num_char = _u16(b2, 10, le)
    max_item16 = _u16(b2, 46, le)
    if not 1 <= num_char <= 4:
        raise ValueError("implausible champion count %d" % num_char)
    coff = off + 640 + 16 * max_item16
    last_exc = None
    for size, stride in _CHAMPION_VARIANTS:
        if coff + size > len(data):
            continue
        try:
            ch = _decode_stream(data[coff:coff + size],
                                hashes[2], checks[2], le)
        except ValueError as exc:
            last_exc = exc
            continue
        recs = [ch[i * stride:(i + 1) * stride] for i in range(num_char)]
        champs = [_parse_champion(rec, le, ho == 312) for rec in recs]
        if data[:20] == _EFB_SENTINEL:
            _apply_csbwin_inventory(data, le, ho, co, off, recs, champs)
        return champs
    raise ValueError("champion data checksum failed (%s)" %
                     (last_exc or "truncated file"))


def _apply_csbwin_inventory(data, le, ho, co, off, recs, champs):
    """Resolve possessions of a CSBwin save via its extended maps.

    Classic-table (MCE) RN values do not apply to this lineage, whose
    RNs are small sequential indices.  Sets each champion's
    "inventory" to engine names via DescIndex (dmitems.desc_name;
    generic db#index when unresolvable) and "boots" when the feet
    slot (5) resolves to DescIndex 84 (Elven Boots).  Never raises:
    failures keep the MCE results.
    """
    try:
        maps = _extended_maps(data)
        if maps is None:
            return
        sec = _sections(data, le, ho, co, off)
    except ValueError:
        return
    for champ, rec in zip(champs, recs):
        labels = []
        boots = False
        for k in range(30):
            v = _u16(rec, 212 + 2 * k, le)
            try:
                dbt, dbi, desc, w2 = _resolve_desc(
                    data, le, sec["dung_off"], maps, v)
            except ValueError:
                labels.append(None)
                continue
            name = dmitems.desc_name(dbt, desc) if desc is not None \
                else None
            if name is None and dbt == 8:  # potion/flask/bomb
                name = dmitems.POTION_NAMES.get((w2 >> 8) & 0x7F)
            labels.append(name if name is not None
                          else "db%d#%d" % (dbt, dbi))
            if k == 5 and desc == OBJDI_ELVEN_BOOTS:
                boots = True
        champ["inventory"] = labels
        champ["boots"] = boots


def default_path():
    """DMGAME.DAT next to this module, if present."""
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "DMGAME.DAT")
    return here if os.path.isfile(here) else ""


FACING_NAMES = ("North", "East", "South", "West")

# (hash-offset, check-offset) table layouts: classic DM (PCGAMEBLOCK1)
# vs CSB (GAMEBLOCK1, also used by the CSBwin lineage in src/).
_HASH_LAYOUTS = ((310, 342), (312, 344))

# Upper bounds to keep auto-detection from accepting garbage.
_MAX_ITEM16 = 2000
_MAX_TIMERS = 3000


def _hashes_at(data, off, base, le):
    b1 = _decode_block1(data[off:off + 512], le)
    return [_u16(b1, o, le) for o in
            (base, base + 2, base + 4, base + 6, base + 8)]


def _chain_ok(data, off, le, ho, co):
    """True when Block2 + champion data decode with these parameters."""
    try:
        hashes = _hashes_at(data, off, ho, le)
        checks = _hashes_at(data, off, co, le)
        b2 = _decode_stream(data[off + 512:off + 640], hashes[0],
                            checks[0], le)
    except ValueError:
        return False
    num_char = _u16(b2, 10, le)
    max_item16 = _u16(b2, 46, le)
    if not 1 <= num_char <= 4:
        return False
    if not 0 <= max_item16 <= _MAX_ITEM16:
        return False
    if not 0 <= _u16(b2, 28, le) <= _MAX_TIMERS:
        return False
    coff = off + 640 + 16 * max_item16
    for size, _stride in _CHAMPION_VARIANTS:
        if coff + size > len(data):
            continue
        try:
            _decode_stream(data[coff:coff + size], hashes[2], checks[2],
                           le)
            return True
        except ValueError:
            continue
    return False


def _extended_maps(data):
    """(type_map, index_map) from a CSBwin extended prefix, or None.

    dataTypeMap/dataIndexMap live right after the 512-byte header
    (they resolve possession RNs: dbType = type_map[rn] & 0xF,
    dbIndex = u16LE(index_map, 2*rn)).  Enciphered maps are refused.
    """
    if data[:20] != _EFB_SENTINEL or len(data) < 512:
        return None
    data_map_len = struct.unpack("<l", data[20:24])[0]
    if not 0 < data_map_len <= 100000:
        return None
    if data[37] & 0x04:  # SimpleEncryption
        return None
    t_off, i_off = 512, 512 + data_map_len
    if len(data) < i_off + 2 * data_map_len:
        return None
    return bytes(data[t_off:t_off + data_map_len]), data[i_off:i_off + 2 * data_map_len]


def _db_areas(data, le, dung_off):
    """{db: area_offset} for the dungeon object databases.

    Walks index, descriptors, object lists and text sections; raises
    ValueError on any inconsistency (counts, sizes, total length
    against the cell-flag array + checksum trailer).
    """
    dung = data[dung_off:]
    if len(dung) < 44:
        raise ValueError("dungeon truncated")
    num_level = dung[4] if not le else dung[5]
    if not 1 <= num_level <= 64:
        raise ValueError("bad level count")
    if le:
        def u16(b, o):
            return b[o] | (b[o + 1] << 8)
    else:
        def u16(b, o):
            return (b[o] << 8) | b[o + 1]
    ncols = 0
    for i in range(num_level):
        w8 = u16(dung, 44 + 16 * i + 8)
        ncols += ((w8 >> 6) & 0x1F) + 1
    objlen = u16(dung, 10)
    nwords = u16(dung, 6)
    counts = [u16(dung, 12 + 2 * i) for i in range(16)]
    t_raw = dung[44 + 16 * num_level + 2 * ncols + 2 * objlen + 4 * nwords:]
    if len(t_raw) < 4:
        raise ValueError("dungeon truncated")
    # sizeOfCompressedText: usually file endianness, but at least one
    # lineage writes it the other way round -- try both.
    for tsize in (int.from_bytes(t_raw[:4], "little" if le else "big"),
                  int.from_bytes(t_raw[:4], "big" if le else "little")):
        o = (44 + 16 * num_level + 2 * ncols + 2 * objlen + 4 * nwords
             + 4 + 2 * tsize)
        areas = {}
        for db in range(16):
            areas[db] = dung_off + o
            o += counts[db] * DB_ENTRY_SIZES[db]
        if data[:20] == _EFB_SENTINEL and len(data) >= 68:
            cellflags = struct.unpack("<L", data[48:52])[0]
        else:
            cellflags = u16(dung, 2)
        if o + cellflags + 2 == len(dung):
            return areas, counts
    raise ValueError("dungeon layout mismatch")


def _resolve_desc(data, le, dung_off, maps, rn):
    """(dbType, dbIndex, desc_index or None, word2) for a possession RN
    in a CSBwin save (maps from its extended prefix).  desc_index
    follows RN::DescIndex (data.cpp) for weapon/clothing/misc/scroll;
    word2 (the record's second word) carries potion types etc."""
    type_map, index_map = maps
    if rn >= len(type_map):
        raise ValueError("RN out of range")
    dbt = type_map[rn] & 0xF
    dbi = struct.unpack("<H", index_map[2 * rn:2 * rn + 2])[0]
    areas, counts = _db_areas(data, le, dung_off)
    if dbi >= counts[dbt]:
        raise ValueError("db index out of range")
    if le:
        def u16(b, o):
            return b[o] | (b[o + 1] << 8)
    else:
        def u16(b, o):
            return (b[o] << 8) | b[o + 1]
    w2 = u16(data, areas[dbt] + dbi * DB_ENTRY_SIZES[dbt] + 2)
    if dbt in _OBJDI_BASE:
        return dbt, dbi, (w2 & 0x7F) + _OBJDI_BASE[dbt], w2
    if dbt == 7:  # scroll
        return dbt, dbi, 0, w2
    return dbt, dbi, None, w2


def _extended_end(data):
    """End offset of a CSBwin " Extended Features " prefix, or None.

    Parses the 512-byte header (little-endian, src/CSB.h), the data maps
    and the game info; files with DSAs fall back to scanning.  The
    DSALevelIndex (when flagged) ends at its [255,255,255] terminator.
    """
    if data[:20] != _EFB_SENTINEL:
        return None
    if len(data) < 512:
        return None
    import struct
    data_map_len = struct.unpack("<l", data[20:24])[0]
    flags = data[37]
    num_dsa = struct.unpack("<H", data[38:40])[0]
    game_info = struct.unpack("<L", data[44:48])[0]
    if not 0 <= data_map_len <= 100000 or game_info > 100000:
        return None
    if num_dsa != 0:
        return None  # DSA records need the full DSA parser; scan instead
    off = 512 + 3 * data_map_len + game_info
    if flags & 0x01:
        # DSALevelIndex triples up to the terminator.
        end = off
        while end + 3 <= len(data) and end - off < 64 * 32 * 3 + 3:
            if data[end] > 63:
                if data[end:end + 3] == b"\xff\xff\xff":
                    return end + 3
                return None
            end += 3
        return None
    return off if off <= len(data) else None


def _find_classic(data):
    """Locate the classic Block1. Returns (offset, le, hash_base,
    check_base).  Raises ValueError when nothing validates."""
    if len(data) < 640:
        raise ValueError("file too small for a DM/CSB save")
    candidates = [0]
    ext = _extended_end(data) if len(data) >= 512 else None
    if ext is not None and ext != 0:
        candidates.append(ext)
    # Bounded full scan (classic saves hit at 0; CSBwin prefixes are
    # small in practice).
    scan_end = min(len(data) - 512, 16384)
    for le in (False, True):
        for off in candidates:
            if off > len(data) - 512:
                continue
            try:
                _decode_block1(data[off:off + 512], le)
            except ValueError:
                continue
            for ho, co in _HASH_LAYOUTS:
                if _chain_ok(data, off, le, ho, co):
                    return off, le, ho, co
        for off in range(1, scan_end + 1):
            if off in candidates:
                continue
            try:
                _decode_block1(data[off:off + 512], le)
            except ValueError:
                continue
            for ho, co in _HASH_LAYOUTS:
                if _chain_ok(data, off, le, ho, co):
                    return off, le, ho, co
    raise ValueError("not a decodable DM/CSB save "
                     "(no classic Block1 found)")


def _timer_size(data, off):
    """Timer record size: 10 normally; CSBwin saves with SequencedTimers
    use 12, with ExtendedTimers 16 (extendedFlags at efb offset 64,
    src/CSB.h).  Classic saves (off == 0) always use 10."""
    if off and data[:20] == b" Extended Features \x00" and len(data) >= 68:
        import struct
        flags = struct.unpack("<L", data[64:68])[0]
        if flags & 0x80:
            return 16
        if flags & 0x20:
            return 12
    return 10


def _sections(data, le=False, ho=310, co=342, off=0):
    """Locate savegame sections. Returns dict with b2, coff (ITEM16
    start), champ_size, dung_off (embedded dungeon start)."""
    b2, hashes, checks = _decode_block2(data, le, ho, co, off)
    max_item16 = _u16(b2, 46, le)
    max_timers = _u16(b2, 28, le)
    coff = off + 640 + 16 * max_item16
    champ_size = None
    for size in _CHAMPION_VARIANTS:
        # _CHAMPION_VARIANTS holds (size, stride) pairs.
        if coff + size[0] > len(data):
            continue
        try:
            _decode_stream(data[coff:coff + size[0]], hashes[2],
                           checks[2], le)
        except ValueError:
            continue
        champ_size = size[0]
        break
    if champ_size is None:
        raise ValueError("champion data checksum failed")
    toff = coff + champ_size
    timer_size = _timer_size(data, off)
    dung_off = (toff + timer_size * max_timers + 2 * max_timers
                + (4 * 464 if champ_size == 1408 else 0))
    return {"b2": b2, "hashes": hashes, "checks": checks,
            "max_item16": max_item16, "max_timers": max_timers,
            "coff": coff, "champ_size": champ_size, "dung_off": dung_off,
            "le": le}


def _dungeon_levels(data, le=False, dung_off=None):
    """Per-level (offset_x, offset_y, exp_mult) from the savegame's
    embedded dungeon copy (uncompressed, same layout as dungeon.dat:
    44-byte index, then 16-byte LEVELDESC records with offsetX@6,
    offsetY@7 -- the absolute coordinates the engine itself uses in
    IncrementLevel, CSBCode.cpp:4689 -- and the experience multiplier
    in word12 bits 12-15, matching the dmweb experience table).
    Returns [] when unparsable."""
    try:
        if dung_off is None:
            sec = _sections(data, le)
            dung_off = sec["dung_off"]
        dung = data[dung_off:]
        if len(dung) < 44:
            return []
        # word4: NumLevel is the high byte (big-endian file) or the
        # low byte (little-endian file).
        num_level = dung[4] if not le else dung[5]
        if not 1 <= num_level <= 64 or len(dung) < 44 + 16 * num_level:
            return []
        out = []
        for i in range(num_level):
            o = 44 + 16 * i
            if le:
                w12 = dung[o + 12] | (dung[o + 13] << 8)
            else:
                w12 = (dung[o + 12] << 8) | dung[o + 13]
            out.append((dung[o + 6], dung[o + 7], (w12 >> 12) & 0xF))
        return out
    except (ValueError, IndexError):
        return []


def _decode_block2(data, le=False, ho=310, co=342, off=0):
    """Decode Block1+Block2, return (block2, hashes, checks)."""
    if len(data) < off + 640:
        raise ValueError("file too small for a DM/CSB save")
    try:
        b1 = _decode_block1(data[off:off + 512], le)
    except ValueError:
        raise ValueError("not a decodable DM/CSB save "
                         "(Block1 checksum failed)")
    hashes = [_u16(b1, o, le) for o in
              (ho, ho + 2, ho + 4, ho + 6, ho + 8)]
    checks = [_u16(b1, o, le) for o in
              (co, co + 2, co + 4, co + 6, co + 8)]
    try:
        b2 = _decode_stream(data[off + 512:off + 640], hashes[0],
                            checks[0], le)
    except ValueError:
        raise ValueError("Block2 checksum failed")
    return b2, hashes, checks


def load_party_bytes(data):
    """Return the party position/state dict from savegame bytes.

    Keys: x, y (map square, local to the level), gx, gy (global map
    coordinates: local + the level's offsetX/offsetY from the
    savegame's embedded dungeon copy -- the same absolute coordinates
    the engine itself uses in IncrementLevel, CSBCode.cpp:4689;
    falls back to x, y when the dungeon section is unparsable),
    facing (0=N,1=E,2=S,3=W, following the DeltaX/DeltaY convention),
    facing_name, level (0-based map index, as counted in the dmweb
    dungeon docs), level_display (level + 1), time (game clock),
    num_characters.  Layout: GAMEBLOCK2 (SaveGame.cpp) -- partyx@12,
    partyy@14, partyfacing@16, partyLevel@18.
    """
    off, le, ho, co = _find_classic(data)
    return _party_at(data, off, le, ho, co)


def _party_dict(num_characters, x, y, facing, level, time, levels):
    if 0 <= level < len(levels):
        ox, oy, mult = levels[level]
    else:
        ox, oy, mult = 0, 0, None
    return {
        "x": x,
        "y": y,
        "gx": ox + x,
        "gy": oy + y,
        "facing": facing,
        "facing_name": FACING_NAMES[facing] if facing < 4
        else "unknown(%d)" % facing,
        "level": level,
        "level_display": level + 1,
        "exp_mult": mult,
        "time": time,
        "num_characters": num_characters,
    }


def _party_at(data, off, le, ho, co):
    """Party dict for an already-located classic section."""
    b2, _hashes, _checks = _decode_block2(data, le, ho, co, off)
    x, y = _u16(b2, 12, le), _u16(b2, 14, le)
    facing = _u16(b2, 16, le)
    level = _u16(b2, 18, le)
    if le:
        time = b2[0] | (b2[1] << 8) | (b2[2] << 16) | (b2[3] << 24)
    else:
        time = ((b2[0] << 24) | (b2[1] << 16) | (b2[2] << 8) | b2[3])
    sec = _sections(data, le, ho, co, off)
    levels = _dungeon_levels(data, le, sec["dung_off"])
    return _party_dict(_u16(b2, 10, le), x, y, facing, level, time,
                       levels)


def load_party(path):
    """load_party_bytes for a savegame file.  Raises ValueError."""
    try:
        with open(path, "rb") as fh:
            data = fh.read()
    except OSError as exc:
        raise ValueError("cannot read %s: %s" % (path, exc))
    try:
        return load_party_bytes(data)
    except ValueError as exc:
        raise ValueError("%s: %s" % (path, exc))
