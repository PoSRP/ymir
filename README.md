<h1 align="center">STM32F411 Black Pill OTA Bootloader</h1>

<p align="center">
  Black Pill (STM32F411CEU6) bootloader with USB OTA updates and A/B layout
</p>

<p align="center">
  <a href="https://github.com/PoSRP/ymir/actions/workflows/ci.yaml">
    <img alt="CI" src="https://github.com/PoSRP/ymir/actions/workflows/ci.yaml/badge.svg">
  </a>
</p>

## Flash layout

```
0x08000000  sectors 0-2   48 KB   Bootloader
0x0800C000  sector  3     16 KB   Boot metadata
0x08010000  sectors 4-5  192 KB   Slot A
0x08040000  sectors 6-7  256 KB   Slot B
```

Maximum image size is 192 KB. Every image begins with a 512-byte header; the
vector table sits at `slot_base + 512`.

## Application API

Link `firmware/ymir_api/` and include `firmware/include/ymir/api.h`.

**C API**:
```c
// 0=bootloader, 1=A, 2=B
int  ymir_current_slot(void);
// mark boot good
void ymir_confirm_boot(void);
// reset into the other slot
void ymir_request_rollback(void);
// reset into DFU mode
void ymir_enter_update(void);
// 1 if valid 8-byte DFU trigger
int  ymir_is_enter_update_request(const uint8_t *header);
// Feed the watchdog to avoid getting force rebooted (~16 seconds)
void ymir_feed_watchdog(void);
```

**C++20 API**:
```cpp
int  ymir::current_slot();
void ymir::confirm_boot();
void ymir::request_rollback();
void ymir::enter_update();
bool ymir::is_enter_update_request(std::span<const uint8_t, 8> header);
void ymir::feed_watchdog();
```

Rollback policy: if `boot_count >= 3 && !confirmed` the bootloader switches slots.
If neither slot validates, it halts with a 2-blink fault pattern.

## Usage guide

*I'm sure I'll write this some day*

## flash_tool.sh

```
./scripts/flash_tool.sh --help
Usage: flash_tool.sh [OPTIONS] COMMAND

Options:
  -b, --baud   BAUD    Baud rate (default: 115200)
  -p, --pid    PID     Application USB PID in decimal or hex (default: 0x5741)
  -s, --serial SERIAL  USB serial number to target (default: first match wins)

Commands:
  update  <file.elf>   Send DFU trigger, wait for bootloader, flash image
                         Slot is auto-detected from the ELF load address.
  status               Show boot metadata (enters bootloader briefly)

Device detection is automatic via USB VID/PID (bootloader: 0xDF00, app: per --pid).
```

## build.sh

```
./build.sh --help
Usage: ./build.sh [command]

Commands:
  build               Build bootloader ELF

  build-test-images   Build slot A and slot B test app images

  flash               Erase all flash and upload bootloader via ST-Link

  debug               Start OpenOCD GDB server on :3333 (Ctrl+C to stop and clean up)

  gdb                 Launch GDB connected to the OpenOCD server on :3333

  test                Run build verification tests (requires build and build-test-images first)

  test-hw             Build everything, flash bootloader via ST-Link, then run hardware OTA tests

  help                Show this message

All firmware commands run inside Docker. CPU usage capped at (nproc - 2).
```
