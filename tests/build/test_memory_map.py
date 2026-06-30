from elftools.elf.elffile import ELFFile


BOOTLOADER_FLASH_BASE  = 0x08000000
BOOTLOADER_FLASH_END   = 0x0800C000
APP_REGION_END         = 0x08080000

SLOT_A_VT_BASE         = 0x08010200
SLOT_B_VT_BASE         = 0x08040200


def _load_segments_in(elf, start, end):
    """Return LOAD segments whose physical address overlaps [start, end)."""
    return [
        seg for seg in elf.iter_segments()
        if seg["p_type"] == "PT_LOAD"
        and seg["p_paddr"] < end
        and seg["p_paddr"] + seg["p_filesz"] > start
    ]


def _isr_vector_addr(elf):
    section = elf.get_section_by_name(".isr_vector")
    assert section is not None, ".isr_vector section not found in ELF"
    return section["sh_addr"]


def test_bootloader_isr_vector_at_flash_origin(ymir_elf):
    with open(ymir_elf, "rb") as f:
        assert _isr_vector_addr(ELFFile(f)) == BOOTLOADER_FLASH_BASE


def test_bootloader_fits_in_48k(ymir_elf):
    with open(ymir_elf, "rb") as f:
        elf = ELFFile(f)
        segs = _load_segments_in(elf, BOOTLOADER_FLASH_BASE, BOOTLOADER_FLASH_END)
        used = sum(seg["p_filesz"] for seg in segs)
    budget = BOOTLOADER_FLASH_END - BOOTLOADER_FLASH_BASE
    assert used <= budget, f"Bootloader flash usage {used}B exceeds 48 KB budget"


def test_test_app_a_isr_vector_at_slot_a(test_app_a_elf):
    with open(test_app_a_elf, "rb") as f:
        assert _isr_vector_addr(ELFFile(f)) == SLOT_A_VT_BASE


def test_test_app_b_isr_vector_at_slot_b(test_app_b_elf):
    with open(test_app_b_elf, "rb") as f:
        assert _isr_vector_addr(ELFFile(f)) == SLOT_B_VT_BASE


def test_bootloader_no_load_segments_in_app_region(ymir_elf):
    with open(ymir_elf, "rb") as f:
        violations = _load_segments_in(ELFFile(f), BOOTLOADER_FLASH_END, APP_REGION_END)
    assert not violations, (
        f"Bootloader has {len(violations)} LOAD segment(s) in the app flash region "
        f"({BOOTLOADER_FLASH_END:#010x}–{APP_REGION_END:#010x})"
    )
