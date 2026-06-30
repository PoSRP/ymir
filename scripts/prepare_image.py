#!/usr/bin/env python3
"""Prepend a 512-byte image header to an ARM ELF and emit a .img file.

Slot is auto-detected from the ELF load address:
  [0x08010000, 0x08040000) -> slot 0 (Slot A)
  [0x08040000, 0x08080000) -> slot 1 (Slot B)
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

MAGIC      = 0xB007AB1E
VERSION    = 0x00010000
FLASH_BASE = 0x08000000
FLASH_END  = 0x08080000
SLOT_RANGES = [(0, 0x08010000, 0x08040000), (1, 0x08040000, 0x08080000)]


def elf_to_bin_and_slot(path: Path) -> tuple[bytes, int]:
    data = path.read_bytes()
    if data[:4] != b'\x7fELF':
        sys.exit(f"error: {path} is not an ELF file")
    if data[4] != 1 or data[5] != 1:
        sys.exit("error: only 32-bit little-endian ELF supported")

    e_phoff,     = struct.unpack_from('<I', data, 28)
    e_phentsize, = struct.unpack_from('<H', data, 42)
    e_phnum,     = struct.unpack_from('<H', data, 44)

    PT_LOAD = 1
    flash_segs = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, _, p_paddr, p_filesz = struct.unpack_from('<IIIII', data, off)
        if p_type == PT_LOAD and p_filesz > 0 and FLASH_BASE <= p_paddr < FLASH_END:
            flash_segs.append((p_paddr, p_offset, p_filesz))

    if not flash_segs:
        sys.exit(f"error: no flash LOAD segments found in {path}")

    base = min(p for p, _, _ in flash_segs)
    end  = max(p + s for p, _, s in flash_segs)

    buf = bytearray(b'\xff' * (end - base))
    for p_paddr, p_offset, p_filesz in flash_segs:
        buf[p_paddr - base: p_paddr - base + p_filesz] = data[p_offset: p_offset + p_filesz]

    slot_entry = next(((s, lo) for s, lo, hi in SLOT_RANGES if lo <= base < hi), None)
    if slot_entry is None:
        ranges = ', '.join(f'[0x{lo:08X}, 0x{hi:08X}) = slot {s}' for s, lo, hi in SLOT_RANGES)
        sys.exit(f"error: ELF base 0x{base:08X} does not fall in any slot ({ranges})")
    slot, slot_lo = slot_entry

    # The image header occupies [slot_lo, slot_lo+512); strip that prefix from the payload
    # so the header we prepend lands in flash at slot_lo and firmware follows at slot_lo+512.
    header_skip = max(0, slot_lo + 512 - base)
    return bytes(buf[header_skip:]), slot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path, help="Firmware .elf")
    parser.add_argument("--out", type=Path, default=None,
                        help="Output path (default: <elf>.img)")
    args = parser.parse_args()

    if not args.elf.exists():
        sys.exit(f"error: {args.elf} not found")

    payload, slot = elf_to_bin_and_slot(args.elf)

    image_size = 512 + len(payload)
    sha        = hashlib.sha256(payload).digest()

    header = struct.pack("<IIII32sB463s",
        MAGIC, VERSION, image_size, 0,
        sha, slot, bytes(463))
    assert len(header) == 512

    out = args.out or args.elf.with_suffix(".img")
    out.write_bytes(header + payload)

    print(f"slot={slot}  image_size={image_size}  sha256={sha.hex()}")
    print(f"written {out}  ({len(header) + len(payload)} bytes)")


if __name__ == "__main__":
    main()
