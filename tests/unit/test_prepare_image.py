"""Round-trip: prepare_image.py wraps a payload, validate_image_cli validates it."""
import os
import secrets
import struct
import subprocess
from pathlib import Path

import pytest

PREPARE_IMAGE = Path(os.environ.get(
    "PREPARE_IMAGE", "/scripts/prepare_image.py"))
VALIDATE_CLI = Path(os.environ.get(
    "VALIDATE_CLI", "/tmp/unit_build/validate_image_cli"))

# Physical flash base for each slot (after the 512-byte image header region)
_SLOT_PADDR = [0x08010200, 0x08040200]


def _make_elf(payload: bytes, slot: int) -> bytes:
    """Minimal 32-bit LE ELF with a single PT_LOAD at the given slot's flash address."""
    paddr = _SLOT_PADDR[slot]
    e_ident = b'\x7fELF' + bytes([1, 1, 1, 0]) + bytes(8)
    elf_hdr = e_ident + struct.pack('<HHIIIIIHHHHHH',
        2, 0x28, 1, paddr, 52, 0, 0x05000400, 52, 32, 1, 40, 0, 0)
    phdr = struct.pack('<IIIIIIII',
        1, 512, paddr, paddr, len(payload), len(payload), 5, 0x200)
    pad = bytes(512 - len(elf_hdr) - len(phdr))
    return elf_hdr + phdr + pad + payload


def _check_available():
    if not PREPARE_IMAGE.exists():
        pytest.skip(f"prepare_image.py not found at {PREPARE_IMAGE}")
    if not VALIDATE_CLI.exists():
        pytest.skip(f"validate_image_cli not found at {VALIDATE_CLI}")


def _wrap(tmp_path, payload, slot=0):
    elf_path = tmp_path / "payload.elf"
    img_path = tmp_path / "payload.img"
    elf_path.write_bytes(_make_elf(payload, slot))
    subprocess.run(
        ["python3", str(PREPARE_IMAGE), str(elf_path), "--out", str(img_path)],
        check=True, capture_output=True,
    )
    return img_path


def _validate(img_path):
    return subprocess.run(
        [str(VALIDATE_CLI), str(img_path)], capture_output=True
    ).returncode


def test_valid_image_passes(tmp_path):
    _check_available()
    payload = secrets.token_bytes(1024)
    img = _wrap(tmp_path, payload)
    assert _validate(img) == 0


def test_payload_bitflip_fails(tmp_path):
    _check_available()
    payload = secrets.token_bytes(1024)
    img = _wrap(tmp_path, payload)
    data = bytearray(img.read_bytes())
    data[600] ^= 0xFF                # somewhere in the payload (after 512-byte header)
    img.write_bytes(data)
    assert _validate(img) == 1


def test_header_corruption_fails(tmp_path):
    _check_available()
    payload = secrets.token_bytes(1024)
    img = _wrap(tmp_path, payload)
    data = bytearray(img.read_bytes())
    data[0] ^= 0xFF                  # corrupt magic
    img.write_bytes(data)
    assert _validate(img) == 1


def test_slot_b_image(tmp_path):
    _check_available()
    payload = secrets.token_bytes(2048)
    img = _wrap(tmp_path, payload, slot=1)
    assert _validate(img) == 0
