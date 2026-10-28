#!/usr/bin/env python3
"""Convert a Dungeon Master Amiga 2.0 save (DMGame.dat) to Amiga 3.6 format.

Amiga 2.0/2.1/2.2 and Amiga 3.6 saves share the same encryption
(CSBCode.cpp:8999 Unscramble), endianness (big) and Block1 key offset
(word 10 / byte 20).  The only structural difference is the champion
section (dmweb.free.fr saved-game-files doc):

  2.0 : 3328 B = 4 x 800 + 128, portraits embedded at record offset 336
  3.6 : 1408 B = 4 x 320 + 128, plus 1856 B = 4 x 464 unencrypted
        portraits after the timer queue, before the dungeon data.

  800 = 336 + 464, 320 = 336 - 16, so per champion 16 filler bytes
  (320..335, always zero in 2.0 saves) are dropped and the 464 portrait
  bytes move to the external block.

  Two more 3.6 normalizations are applied (both verified against saves
  written by the real 3.6 engine; the ITEM16 one is required by DMute):

  1. Portrait pointer: the 3.6 engine keeps a 32-bit big-endian pointer
     at record offset 276 (4 bytes).  In 2.0 records these bytes are zero
     filler; native 3.6 saves always carry non-zero values, spaced exactly
     0x1D4 = 468 bytes apart (464 portrait bytes + 4 header), e.g.
     001DFC30 / 001DFA5C / 001DF888 / 001DF6B4.  The value is a stale
     Amiga heap address: the engine re-links it on load (it loads
     zero-pointer files fine) and rewrites it on save.  The converter
     writes the canonical pattern by default (override with
     --pointer-base, disable with --zero-pointers).
  2. ITEM16 capacity: 2.0 saves with MaxITEM16 < 110 (typically 60) are
     rejected by DMute ("Not a recognised save game file"); native 3.6
     saves carry 110.  The converter expands the table exactly like the
     engine's EnsureItem16Available() (Monster.cpp:83): new slots zeroed
     with word0 = -1 (empty), checksums recomputed (disable with
     --no-expand-item16).

Everything else (Block1 filler, Block2 game state, timers, timer queue,
dungeon data) is copied verbatim; affected checksums in the Block1 tail
plus the Block1 first-half fixup word are recomputed.

Usage:
  python3 convert-Amiga20-to-Amiga36.py DMGame.dat DMGame36.dat
"""

import argparse
import os
import sys

KEY_OFF_DM = 20          # byte offset of Block2 key for DM (word 10)
HASH_CHAMP = 314         # byte offset of champion hash in decoded Block1
CHECK_BLOCK2 = 342       # byte offset of Block2 checksum in decoded Block1
CHECK_ITEM16 = 344       # byte offset of ITEM16 checksum in decoded Block1
CHECK_CHAMP = 346        # byte offset of champion checksum in decoded Block1
CHAMP_20 = 3328
CHAMP_36 = 1408
REC_20 = 800
REC_36 = 320
PORTRAIT_OFF = 336
PORTRAIT_SIZE = 464
FILLER_OFF = 320
FILLER_SIZE = 16
GLOBAL_SIZE = 128
PTR_OFF = 276            # 32-bit BE portrait pointer in 3.6 records
PTR_STEP = 0x1D4         # 468 = 464 portrait bytes + 4 header
PTR_BASE_DEFAULT = 0x001DFC30  # canonical 3.6 pattern (session heap base)
MIN_ITEM16_36 = 110      # 3.6-era ITEM16 capacity; 2.0 saves with less
                         # (e.g. 60) are rejected by tools like DMute.
                         # The engine itself grows 60 -> 110 via
                         # EnsureItem16Available() (Monster.cpp:83).


def _u16(buf, off):
    return (buf[off] << 8) | buf[off + 1]


def _set_u16(buf, off, val):
    buf[off] = (val >> 8) & 0xFF
    buf[off + 1] = val & 0xFF


def _unscramble(buf, init_hash, num_words):
    """Port of Unscramble() (CSBCode.cpp:8999), big-endian words.

    Symmetric: the same call encrypts and decrypts in place.
    Returns the checksum to compare against the stored value.
    """
    d7 = init_hash & 0xFFFF
    d6 = num_words
    d5 = init_hash & 0xFFFF
    for i in range(num_words):
        off = 2 * i
        w = ((buf[off] << 8) | buf[off + 1]) & 0xFFFF
        d5 = (d5 + w) & 0xFFFF
        w2 = w ^ d7
        buf[off] = (w2 >> 8) & 0xFF
        buf[off + 1] = w2 & 0xFF
        d5 = (d5 + w2) & 0xFFFF
        d7 = (d7 + d6) & 0xFFFF
        d6 -= 1
    return d5


def _block1_tail_sum(decoded_512):
    return sum(_u16(decoded_512, 256 + 2 * i) for i in range(128)) & 0xFFFF


def _block1_filler_checksum(first_256):
    d6 = 0
    for g in range(32):
        d6 = (d6 + _u16(first_256, 8 * g)) & 0xFFFF
        d6 ^= _u16(first_256, 8 * g + 2)
        d6 = (d6 - _u16(first_256, 8 * g + 4)) & 0xFFFF
        d6 ^= _u16(first_256, 8 * g + 6)
    return d6


def _decode_block1_tail(raw512, key_off):
    """Decode Block1 tail with the key at key_off; raise on mismatch."""
    buf = bytearray(raw512)
    tail = bytearray(buf[256:512])
    _unscramble(tail, _u16(buf, key_off), 128)
    buf[256:512] = tail
    if _block1_filler_checksum(bytes(buf[0:256])) != _block1_tail_sum(bytes(buf)):
        raise ValueError("Block1 checksum mismatch with key offset %d" % key_off)
    return bytes(buf)


def _decode_stream(raw, init_hash, checksum):
    buf = bytearray(raw)
    if _unscramble(buf, init_hash, len(buf) // 2) != checksum:
        raise ValueError("stream checksum mismatch")
    return bytes(buf)


def convert(src: bytes, pointer_base=PTR_BASE_DEFAULT,
            expand_item16=True) -> bytes:
    if len(src) < 640:
        raise ValueError("file too small for a DM save (%d bytes)" % len(src))

    # Must be a DM save decodable with the 2.0/3.6 key offset (20).
    try:
        b1 = _decode_block1_tail(src[0:512], KEY_OFF_DM)
    except ValueError:
        raise ValueError("not a decodable DM Amiga save (Block1 key@20 failed)")
    key = _u16(src, KEY_OFF_DM)

    hashes = [_u16(b1, o) for o in (310, 312, 314, 316, 318)]
    checks = [_u16(b1, o) for o in (342, 344, 346, 348, 350)]

    b2 = bytearray(_decode_stream(src[512:640], hashes[0], checks[0]))
    num_char = _u16(b2, 10)
    max_item16 = _u16(b2, 46)
    max_timers = _u16(b2, 28)
    if not 1 <= num_char <= 4:
        raise ValueError("implausible champion count %d" % num_char)
    if max_timers > 3000:
        raise ValueError("implausible MaxTimers %d" % max_timers)

    coff = 640 + 16 * max_item16
    timers_len = max_timers * 10
    queue_len = max_timers * 2

    # Detect source variant: 2.0 must decode as 3328 and fail as 1408.
    try:
        champ20 = _decode_stream(src[coff:coff + CHAMP_20], hashes[2], checks[2])
    except ValueError:
        # Maybe it is already 3.6?
        try:
            _decode_stream(src[coff:coff + CHAMP_36], hashes[2], checks[2])
            raise ValueError("input already looks like Amiga 3.6 (1408 B "
                             "champion section); nothing to do")
        except ValueError as exc:
            if "already looks like" in str(exc):
                raise
            raise ValueError("champion section decodes as neither 2.0 "
                             "(3328 B) nor 3.6 (1408 B)")
    if len(src) < coff + CHAMP_20 + timers_len + queue_len:
        raise ValueError("file truncated (timers/queue exceed file size)")

    # Validate scrambled streams (ITEM16 decoded for possible expansion).
    toff = coff + CHAMP_20
    item16 = bytearray(_decode_stream(src[640:640 + 16 * max_item16],
                                      hashes[1], checks[1]))
    _decode_stream(src[toff:toff + timers_len], hashes[3], checks[3])
    _decode_stream(src[toff + timers_len:toff + timers_len + queue_len],
                   hashes[4], checks[4])
    dungeon = src[toff + timers_len + queue_len:]
    if not dungeon:
        raise ValueError("missing dungeon data after timer queue")

    # Expand ITEM16 to 3.6-era capacity, exactly like the engine's
    # EnsureItem16Available(): new slots zeroed with word0 = -1 (empty).
    # Files with the small 2.0 capacity (60) are rejected by DMute
    # ("Not a recognised save game file"); native 3.6 saves carry 110.
    if expand_item16 and max_item16 < MIN_ITEM16_36:
        item16 += b"".join(b"\xff\xff" + b"\x00" * 14
                           for _ in range(MIN_ITEM16_36 - max_item16))
        max_item16 = MIN_ITEM16_36
        _set_u16(b2, 46, max_item16)

    # Split 4 x 800 B records -> 4 x 320 B + 4 x 464 B portraits.
    # Bytes 276..279 become the 3.6 portrait pointer (stale heap address,
    # rewritten by the engine on every save; kept non-zero for tools).
    new_recs = []
    portraits = []
    for i in range(4):
        rec = champ20[i * REC_20:(i + 1) * REC_20]
        filler = rec[FILLER_OFF:FILLER_OFF + FILLER_SIZE]
        if any(filler):
            print("warning: champion %d filler bytes 320..335 not zero, "
                  "dropping anyway" % i, file=sys.stderr)
        new_rec = bytearray(rec[0:REC_36])
        if pointer_base is not None:
            ptr = (pointer_base - i * PTR_STEP) & 0xFFFFFFFF
            new_rec[PTR_OFF:PTR_OFF + 4] = ptr.to_bytes(4, "big")
        new_recs.append(bytes(new_rec))
        portraits.append(rec[PORTRAIT_OFF:PORTRAIT_OFF + PORTRAIT_SIZE])
    tail_global = champ20[4 * REC_20:4 * REC_20 + GLOBAL_SIZE]
    if len(tail_global) != GLOBAL_SIZE:
        raise ValueError("champion tail truncated")
    new_champ_plain = b"".join(new_recs) + tail_global
    assert len(new_champ_plain) == CHAMP_36

    # Re-encode changed streams with their ORIGINAL hashes (Unscramble is
    # symmetric); the returned checksums become the new stored values.
    # Timers/queue plaintext is untouched, so its ciphertext is copied
    # verbatim and checksums 348/350 stay valid.
    b2_buf = bytearray(b2)
    new_b2_check = _unscramble(b2_buf, hashes[0], 64)
    new_b2_cipher = bytes(b2_buf)
    item_buf = bytearray(item16)
    new_item_check = _unscramble(item_buf, hashes[1], len(item_buf) // 2)
    new_item_cipher = bytes(item_buf)
    buf = bytearray(new_champ_plain)
    new_champ_check = _unscramble(buf, hashes[2], CHAMP_36 // 2)
    new_champ_cipher = bytes(buf)

    # Patch decoded Block1 tail, then fix the first-half last word so the
    # Block1 self-check (WriteFirstBlock/UnscrambleBlock1 in Chaos.cpp)
    # still holds: last word = t ^ d5new.
    b1_new = bytearray(b1)
    _set_u16(b1_new, CHECK_BLOCK2, new_b2_check)
    _set_u16(b1_new, CHECK_ITEM16, new_item_check)
    _set_u16(b1_new, CHECK_CHAMP, new_champ_check)
    d5new = _block1_tail_sum(bytes(b1_new))
    first = bytearray(b1_new[0:256])
    t = 0
    for g in range(31):
        t = (t + _u16(first, 8 * g)) & 0xFFFF
        t ^= _u16(first, 8 * g + 2)
        t = (t - _u16(first, 8 * g + 4)) & 0xFFFF
        t ^= _u16(first, 8 * g + 6)
    t = (t + _u16(first, 8 * 31)) & 0xFFFF
    t ^= _u16(first, 8 * 31 + 2)
    t = (t - _u16(first, 8 * 31 + 4)) & 0xFFFF
    _set_u16(first, 254, t ^ d5new)
    b1_new[0:256] = first
    # Sanity: filler check must now match the new tail sum.
    assert _block1_filler_checksum(bytes(first)) == d5new

    # Re-scramble the tail with the original key.
    tail = bytearray(b1_new[256:512])
    _unscramble(tail, key, 128)

    out = bytearray()
    out += bytes(first)                       # Block1 first half (patched)
    out += bytes(tail)                        # Block1 second half (re-coded)
    out += new_b2_cipher                      # Block2 (MaxITEM16 updated)
    out += new_item_cipher                    # ITEM16 (possibly expanded)
    out += new_champ_cipher                   # Champion data 1408 B
    out += src[toff:toff + timers_len]        # Timers (verbatim)
    out += src[toff + timers_len:toff + timers_len + queue_len]  # queue
    for p in portraits:
        out += p                              # 4 x 464 B, unencrypted
    out += dungeon                            # dungeon data (verbatim)
    return bytes(out)


def verify(dst: bytes, src: bytes, pointer_base=PTR_BASE_DEFAULT,
           expanded=True):
    """Re-decode the converted file and cross-check against source."""
    b1 = _decode_block1_tail(dst[0:512], KEY_OFF_DM)
    hashes = [_u16(b1, o) for o in (310, 312, 314, 316, 318)]
    checks = [_u16(b1, o) for o in (342, 344, 346, 348, 350)]
    b2 = _decode_stream(dst[512:640], hashes[0], checks[0])
    max_item16 = _u16(b2, 46)
    max_timers = _u16(b2, 28)
    coff = 640 + 16 * max_item16
    champ = _decode_stream(dst[coff:coff + CHAMP_36], hashes[2], checks[2])
    item16 = _decode_stream(dst[640:640 + 16 * max_item16],
                            hashes[1], checks[1])
    toff = coff + CHAMP_36
    _decode_stream(dst[toff:toff + max_timers * 10], hashes[3], checks[3])
    qoff = toff + max_timers * 10
    _decode_stream(dst[qoff:qoff + max_timers * 2], hashes[4], checks[4])
    portraits = dst[qoff + max_timers * 2:qoff + max_timers * 2 + 4 * PORTRAIT_SIZE]
    assert len(portraits) == 4 * PORTRAIT_SIZE

    # Champion core bytes (names/stats/skills/possessions/load) must match.
    sb1 = _decode_block1_tail(src[0:512], KEY_OFF_DM)
    sh = [_u16(sb1, o) for o in (310, 312, 314, 316, 318)]
    sc = [_u16(sb1, o) for o in (342, 344, 346, 348, 350)]
    sb2 = _decode_stream(src[512:640], sh[0], sc[0])
    src_max = _u16(sb2, 46)
    scoff = 640 + 16 * src_max
    schamp = _decode_stream(src[scoff:scoff + CHAMP_20], sh[2], sc[2])
    sitem = _decode_stream(src[640:640 + 16 * src_max], sh[1], sc[1])
    # ITEM16: source prefix must be preserved; added slots engine-style.
    assert item16[:len(sitem)] == sitem, "ITEM16 prefix differs"
    for extra in (item16[len(sitem):],):
        assert len(extra) % 16 == 0
        for i in range(0, len(extra), 16):
            assert extra[i:i + 2] == b"\xff\xff", "new slot not empty"
    if expanded:
        assert max_item16 >= MIN_ITEM16_36, "ITEM16 not expanded"
    for i in range(4):
        a = schamp[i * REC_20:i * REC_20 + PTR_OFF]
        c = champ[i * REC_36:i * REC_36 + PTR_OFF]
        assert a == c, "champion %d core bytes differ" % i
        if pointer_base is not None:
            expect = (pointer_base - i * PTR_STEP) & 0xFFFFFFFF
            got = int.from_bytes(champ[i * REC_36 + PTR_OFF:
                                       i * REC_36 + PTR_OFF + 4], "big")
            assert got == expect, "champion %d pointer %08X != %08X" % (
                i, got, expect)
        assert portraits[i * PORTRAIT_SIZE:(i + 1) * PORTRAIT_SIZE] == \
            schamp[i * REC_20 + PORTRAIT_OFF:i * REC_20 + PORTRAIT_OFF + PORTRAIT_SIZE]
    # Dungeon + timers must be identical copies.
    st = scoff + CHAMP_20
    smax = _u16(sb2, 28)
    assert dst[qoff + max_timers * 2 + 4 * PORTRAIT_SIZE:] == \
        src[st + smax * 10 + smax * 2:]
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Convert Dungeon Master Amiga 2.0 save to Amiga 3.6 format.")
    ap.add_argument("input", help="source 2.0 save, e.g. DMGame.dat")
    ap.add_argument("output", help="destination 3.6 save")
    ap.add_argument("--force", action="store_true",
                    help="overwrite existing output file")
    ap.add_argument("--pointer-base", default="0x1DFC30",
                    help="base of 3.6 portrait pointers (hex, default "
                    "0x1DFC30); per-champion value is base - i*0x1D4")
    ap.add_argument("--zero-pointers", action="store_true",
                    help="leave portrait pointer bytes 276..279 zero "
                    "(2.0 filler) instead of 3.6 pattern")
    ap.add_argument("--no-expand-item16", action="store_true",
                    help="keep the 2.0 ITEM16 capacity (e.g. 60) instead of "
                    "expanding to 110 like the 3.6 engine does")
    args = ap.parse_args(argv)

    with open(args.input, "rb") as fh:
        src = fh.read()
    if args.zero_pointers:
        pointer_base = None
    else:
        try:
            pointer_base = int(args.pointer_base, 16)
        except ValueError:
            print("error: --pointer-base must be hex, e.g. 0x1DFC30",
                  file=sys.stderr)
            return 1
    try:
        dst = convert(src, pointer_base,
                      expand_item16=not args.no_expand_item16)
    except ValueError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 1
    if os.path.exists(args.output) and not args.force:
        print("error: %s exists (use --force)" % args.output, file=sys.stderr)
        return 1
    with open(args.output, "wb") as fh:
        fh.write(dst)
    try:
        verify(dst, src, pointer_base,
               expanded=not args.no_expand_item16)
        print("verified: %s (%d B) -> %s (%d B)" %
              (args.input, len(src), args.output, len(dst)))
    except Exception as exc:  # noqa: BLE001
        print("warning: post-verify failed: %s" % exc, file=sys.stderr)
        print("wrote %s (%d B) anyway" % (args.output, len(dst)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
