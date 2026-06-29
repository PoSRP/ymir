import os
from pathlib import Path

import pytest

FIRMWARE_BUILD = Path(os.environ.get("FIRMWARE_BUILD", "/firmware/build"))
TEST_APP_BUILD = Path(os.environ.get("TEST_APP_BUILD", "/firmware/test_app_baremetal/build"))


@pytest.fixture
def ymir_elf():
    p = FIRMWARE_BUILD / "Debug" / "ymir.elf"
    if not p.exists():
        pytest.skip(f"ymir.elf not found at {p} - run ./build.sh build first")
    return p


@pytest.fixture
def test_app_a_elf():
    p = TEST_APP_BUILD / "test_app_baremetal_a" / "test_app_baremetal.elf"
    if not p.exists():
        pytest.skip(f"test_app_baremetal.elf not found at {p} - run ./build.sh build-test-images first")
    return p


@pytest.fixture
def test_app_b_elf():
    p = TEST_APP_BUILD / "test_app_baremetal_b" / "test_app_baremetal.elf"
    if not p.exists():
        pytest.skip(f"test_app_baremetal.elf not found at {p} - run ./build.sh build-test-images first")
    return p
