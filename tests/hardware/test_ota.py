"""
Hardware OTA tests.

Prerequisites (handled by build.sh test-hw before invoking pytest):
  - Bootloader flashed via ST-Link
  - test_app_baremetal_a and test_app_baremetal_b built

Test order matters, each test leaves the device in a known state for the next.
"""

import pathlib
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
SCRIPTS = ROOT / "scripts"
FIRMWARE = ROOT / "firmware"
BAREMETAL_ELF_A = FIRMWARE / "test_app_baremetal/build/test_app_baremetal_a/test_app_baremetal.elf"
BAREMETAL_ELF_B = FIRMWARE / "test_app_baremetal/build/test_app_baremetal_b/test_app_baremetal.elf"
RTOS_ELF_A = FIRMWARE / "test_app_rtos/build/test_app_rtos_a/test_app_rtos.elf"
RTOS_ELF_B = FIRMWARE / "test_app_rtos/build/test_app_rtos_b/test_app_rtos.elf"

VID = "0483"
APP_PID = 0x5741  # application
BL_PID = 0xDF00   # bootloader


# USB detection

def _find_port(pid: int) -> str | None:
    pid_hex = f"{pid:04x}"
    for dev in pathlib.Path("/sys/bus/usb/devices").iterdir():
        if ":" in dev.name:
            continue
        try:
            if (dev / "idVendor").read_text().strip() != VID:
                continue
            if (dev / "idProduct").read_text().strip() != pid_hex:
                continue
        except OSError:
            continue
        for tty in dev.rglob("tty/tty*"):
            if tty.is_dir():
                return f"/dev/{tty.name}"
    return None


def wait_for(pid: int, timeout: float = 20.0) -> str:
    """Wait until the USB device with pid appears; return its /dev/ttyACM* path."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        port = _find_port(pid)
        if port:
            return port
        time.sleep(0.2)
    name = "bootloader" if pid == BL_PID else "app"
    raise TimeoutError(f"{name} (PID 0x{pid:04X}) did not appear within {timeout:.0f}s")


# Flash helper

def flash(img: pathlib.Path) -> None:
    subprocess.run([str(SCRIPTS / "flash_tool.sh"), "update", str(img)], check=True)

# Tests

def test_baremetal_ota_to_slot_a():
    flash(BAREMETAL_ELF_A)
    wait_for(APP_PID, timeout=20)


def test_baremetal_ota_to_slot_b():
    flash(BAREMETAL_ELF_B)
    wait_for(APP_PID, timeout=20)


def test_rtos_ota_to_slot_a():
    flash(RTOS_ELF_A)
    wait_for(APP_PID, timeout=20)


def test_rtos_ota_to_slot_b():
    flash(RTOS_ELF_B)
    wait_for(APP_PID, timeout=20)


def test_ota_stress():
    """5 alternating OTA cycles (B→A→B→A→B) starting from slot A."""
    for i in range(5):
        img = BAREMETAL_ELF_B if i % 2 == 0 else BAREMETAL_ELF_A
        slot = 1 if i % 2 == 0 else 0
        print(f"\n  cycle {i + 1}/5 → slot {slot}", flush=True)
        flash(img)
        wait_for(APP_PID, timeout=20)
