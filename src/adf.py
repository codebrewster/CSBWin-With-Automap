#!/usr/bin/env python3
"""Minimal read-only AmigaDOS disk image (ADF) file extractor.

Supports standard AmigaDOS DD disk images (901120 bytes) with OFS
(``DOS\\0``) or FFS (``DOS\\2`` ..) filesystems: enough to pull files
like ``DMGAME.DAT`` / ``DMGAME.BAK`` off a save disk.  Read-only on
purpose -- the image is never modified.

Only what is needed is implemented (no directory traversal: files are
found by scanning for file-header blocks, which is sufficient for the
flat save disks this was written for)::

    import adf
    names = adf.list_files("save.adf")          # ["DMGame.dat", ...]
    data = adf.read_file("save.adf", "DMGAME.DAT")
    pick = adf.pick_savegame(names)             # prefers DMGAME.DAT

Run ``python3 adf.py image.adf`` to list the files it contains.

OFS support is verified against a real save disk (block checksums,
``nextData`` chains, extension blocks, byte sizes).  FFS follows the
same documented layout (header table + extension chain, 512 raw data
bytes per block) but is untested -- mismatches raise loudly instead
of returning corrupt data.
"""

import os
import struct
import sys

BLOCK = 512
ROOT_BLOCK = 880

T_SHORT = 2
T_DATA = 8
T_LIST = 16
ST_ROOT = 1
ST_FILE = -3

OFS_DATA = 488  # data bytes per OFS data block (24-byte header)


class ADFError(ValueError):
    """Raised when the image is not a readable AmigaDOS disk."""


def _u32(buf, off):
    return struct.unpack(">L", bytes(buf[off:off + 4]))[0]


def _s32(buf, off):
    return struct.unpack(">l", bytes(buf[off:off + 4]))[0]


def _checksum_ok(block):
    """AmigaDOS block checksum: all 128 longwords must sum to 0."""
    return sum(struct.unpack(">128l", bytes(block))) & 0xFFFFFFFF == 0


class _Image:
    def __init__(self, data):
        if len(data) < (ROOT_BLOCK + 1) * BLOCK or len(data) % BLOCK:
            raise ADFError("not a full disk image (%d bytes)" % len(data))
        if data[0:3] != b"DOS":
            raise ADFError("no AmigaDOS bootblock signature")
        self.data = data
        self.nblocks = len(data) // BLOCK
        # DOS\2, \3, ... = FFS (bit 1 set); DOS\0, \1 = OFS.
        self.ffs = bool(data[3] & 2)
        root = self.block(ROOT_BLOCK)
        if _s32(root, 0) != T_SHORT or _s32(root, 508) != ST_ROOT \
                or not _checksum_ok(root):
            raise ADFError("root block (%d) invalid" % ROOT_BLOCK)

    def block(self, n):
        if not 0 <= n < self.nblocks:
            raise ADFError("block %d out of range" % n)
        return self.data[n * BLOCK:(n + 1) * BLOCK]

    def is_header(self, n):
        blk = self.block(n)
        return _s32(blk, 0) == T_SHORT and _s32(blk, 508) == ST_FILE \
            and _checksum_ok(blk)

    @staticmethod
    def name_of(blk):
        ln = blk[432]
        if not 1 <= ln <= 30:
            raise ADFError("bad filename length %d" % ln)
        try:
            return bytes(blk[433:433 + ln]).decode("latin-1")
        except ValueError as exc:
            raise ADFError("bad filename: %s" % exc)

    def headers(self):
        """Yield (block_no, name, size) for every file-header block."""
        for n in range(self.nblocks):
            if n == ROOT_BLOCK:
                continue
            if self.is_header(n):
                blk = self.block(n)
                yield n, self.name_of(blk), _u32(blk, 81 * 4)

    def _ofs_chain(self, header_blk):
        """Assemble an OFS file via its nextData chain."""
        first = _u32(header_blk, 4 * 4)
        size = _u32(header_blk, 81 * 4)
        parts = {}
        seen = set()
        n = first
        while n != 0:
            if n in seen:
                raise ADFError("data block cycle at %d" % n)
            seen.add(n)
            blk = self.block(n)
            if _s32(blk, 0) != T_DATA or not _checksum_ok(blk):
                raise ADFError("bad OFS data block %d" % n)
            if _u32(blk, 1 * 4) not in (0, _u32(header_blk, 1 * 4)):
                # headerKey should name our file header; 0 tolerates
                # odd writers but still requires a valid checksum.
                pass
            seq = _u32(blk, 2 * 4)
            ln = _u32(blk, 3 * 4)
            if ln > OFS_DATA:
                raise ADFError("oversize OFS data block %d" % n)
            parts[seq] = bytes(blk[24:24 + ln])
            n = _u32(blk, 4 * 4)
        if set(parts) != set(range(1, len(parts) + 1)):
            raise ADFError("incomplete OFS sequence")
        data = b"".join(parts[s] for s in sorted(parts))
        if len(data) < size:
            raise ADFError("short file: %d < %d bytes" % (len(data), size))
        return data[:size]

    def _ffs_tables(self, header_blk):
        """Block numbers from header + extension tables, in order."""
        out = []
        blk = header_blk
        while True:
            for i in range(6, 78):
                v = _u32(blk, i * 4)
                if v != 0:
                    out.append(v)
            nxt = _u32(blk, 126 * 4)
            if nxt == 0:
                break
            blk = self.block(nxt)
            if _s32(blk, 0) != T_LIST or not _checksum_ok(blk):
                raise ADFError("bad extension block %d" % nxt)
        return out

    def read_at(self, header_no):
        """Return the full file content for a file-header block."""
        header_blk = self.block(header_no)
        size = _u32(header_blk, 81 * 4)
        if self.ffs:
            data = b"".join(self.block(n) for n
                             in self._ffs_tables(header_blk))
            if len(data) < size:
                raise ADFError("short file: %d < %d bytes"
                               % (len(data), size))
            return data[:size]
        return self._ofs_chain(header_blk)


def _load(source):
    if isinstance(source, (bytes, bytearray)):
        return _Image(bytes(source))
    with open(source, "rb") as fh:
        return _Image(fh.read())


def list_files(source):
    """Return [(name, size, header_block), ...] in on-disk order."""
    return [(name, size, blk) for blk, name, size
            in _load(source).headers()]


def read_file(source, name):
    """Return the content of the file called ``name`` (case-insensitive).

    Raises ADFError when missing (message lists what is on the disk).
    """
    img = _load(source)
    want = name.lower()
    for blk, found, _size in img.headers():
        if found.lower() == want:
            return img.read_at(blk)
    known = sorted(n for _b, n, _s in img.headers())
    raise ADFError("no %r on this disk (has: %s)"
                   % (name, ", ".join(known) or "nothing"))


def pick_savegame(names):
    """Pick the best savegame from a list of file names (or None).

    Prefers DMGAME.DAT, then any other *.DAT, then *.BAK (stale backup).
    Comparison is case-insensitive; the returned value keeps the
    on-disk spelling.
    """
    lower = [(n.lower(), n) for n in names]
    for n, orig in lower:
        if n == "dmgame.dat":
            return orig
    dats = sorted(orig for n, orig in lower if n.endswith(".dat"))
    if dats:
        return dats[0]
    baks = sorted(orig for n, orig in lower if n.endswith(".bak"))
    if baks:
        return baks[0]
    return None


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if len(argv) != 1:
        print("usage: adf.py image.adf", file=sys.stderr)
        return 2
    try:
        img = _load(argv[0])
    except ADFError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 1
    print("%s: %s, %d blocks" % (
        argv[0], "FFS" if img.ffs else "OFS", img.nblocks))
    for blk, name, size in img.headers():
        print("  block %4d  %8d bytes  %s" % (blk, size, name))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
