#!/usr/bin/env bash

set -uo pipefail

readonly VID="0483"
APP_PID=22337  # 0x5741 default; override with -p/--pid

function usage() {
    cat >&2 <<'EOF'
Usage: ota_loop.sh [-p PID]

Options:
  -p, --pid PID   Application USB PID in decimal or hex (default: 0x5741)

Stress-tests the OTA path by alternating between slot A and slot B
indefinitely. Stops on the first failure or on SIGINT/SIGTERM.

Requires test_app_a and test_app_b ELFs to be built first (./build.sh build-test-images).
EOF
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--pid)  APP_PID=$(( $2 )); shift 2 ;;
        -h|--help) usage ;;
        *) echo "unknown option: $1" >&2; usage ;;
    esac
done

function find_app_port() {
    local pid_hex
    pid_hex=$(printf '%04x' "$APP_PID")
    local dev tty
    for dev in /sys/bus/usb/devices/*/; do
        [[ "${dev}" == *:* ]] && continue
        [[ -f "${dev}idVendor" ]] || continue
        [[ "$(< "${dev}idVendor")" == "$VID" ]] || continue
        [[ "$(< "${dev}idProduct")" == "$pid_hex" ]] || continue
        for tty in "${dev}"*/tty/tty*/; do
            [[ -d "$tty" ]] || continue
            printf "/dev/%s\n" "$(basename "$tty")"
            return 0
        done
    done
    return 1
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ELF_A="$ROOT/firmware/test_app_baremetal/build/test_app_baremetal_a/test_app_baremetal.elf"
ELF_B="$ROOT/firmware/test_app_baremetal/build/test_app_baremetal_b/test_app_baremetal.elf"

for elf in "$ELF_A" "$ELF_B"; do
    [[ -f $elf ]] || { echo "Missing ELF: $elf" >&2; exit 2; }
done

count=0
slot=1  # next slot to flash; alternates each iteration

trap 'echo; echo "Stopped after $count successful cycles."; exit 0' INT TERM

while true; do
    count=$(( count + 1 ))
    if [[ $slot -eq 1 ]]; then elf="$ELF_B"; else elf="$ELF_A"; fi
    start=$(date +%s)
    echo "─── Cycle $count → slot $slot ───"
    if ! "$ROOT/scripts/flash_tool.sh" --pid "$APP_PID" update "$elf"; then
        echo
        echo "FAILED on cycle $count (slot $slot, ${SECONDS}s elapsed total)" >&2
        exit 1
    fi
    elapsed=$(( $(date +%s) - start ))
    echo "OK    : cycle $count completed in ${elapsed}s"
    slot=$(( 1 - slot ))

    sleep 1
    deadline=$(( $(date +%s) + 30 ))
    while [[ $(date +%s) -lt $deadline ]]; do
        find_app_port &>/dev/null && break
        sleep 0.2
    done
    if ! find_app_port &>/dev/null; then
        echo "FAILED: app (PID 0x$(printf '%04X' "$APP_PID")) did not appear within 30s after cycle $count" >&2
        exit 1
    fi
    sleep 0.5
done
