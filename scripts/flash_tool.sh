#!/usr/bin/env bash

set -euo pipefail

BAUD=115200
APP_PID=22337  # 0x5741  default; override with -p/--pid
SERIAL=""  # USB serial filter; empty = first match wins
PORT=""  # set just before port_open; used in send_frame error messages

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMG_PREP_SCRIPT="$ROOT/scripts/prepare_image.py"
IMG_TMP=""

# USB identifiers
readonly VID="0483"
readonly BL_PID=57088   # 0xDF00  bootloader

# Protocol constants
readonly SOF=170 # 0xAA
readonly CMD_START=1
readonly CMD_CHUNK=2
readonly CMD_FINISH=3
readonly CMD_STATUS=5
readonly CMD_BOOT=6
readonly RESP_ACK=6 # 0x06
readonly RESP_NAK=21 # 0x15
readonly CHUNK_SIZE=256

# Error printing helper
function die() {
    printf '%s\n' "$1" >&2
    exit 1
}

# Generate byte octet lookup table
declare -a BYTE_OCT
for (( _bi=0; _bi<256; _bi++ )); do
    _d0=$(( _bi >> 6 )); _d1=$(( (_bi >> 3) & 7 )); _d2=$(( _bi & 7 ))
    BYTE_OCT[_bi]="\\${_d0}${_d1}${_d2}"
done
unset _bi _d0 _d1 _d2

function elf_to_img() {
    IMG_TMP=$(mktemp --suffix=.img)
    trap 'rm -f "$IMG_TMP"' EXIT
    python3 "$IMG_PREP_SCRIPT" "$1" --out "$IMG_TMP" \
        || die "prepare_image.py failed for $1"
}

# MPEG-2 CRC32
function crc32_mpeg2() {
    local crc
    local b
    local j
    local tmp

    crc=0xFFFFFFFF
    for b in "$@"; do
        tmp=$((b & 0xFF))
        tmp=$((tmp << 24))
        crc=$((crc ^ tmp))
        j=0
        while [ $j -lt 8 ]; do
            if [ $((crc & 0x80000000)) -ne 0 ]; then
                crc=$((crc << 1))
                crc=$((crc ^ 0x04C11DB7))
                crc=$((crc & 0xFFFFFFFF))
            else
                crc=$((crc << 1))
                crc=$((crc & 0xFFFFFFFF))
            fi
            j=$((j + 1))
        done
    done

    printf '%s\n' "$crc"
}

# USB PID detection via sysfs
function find_port_by_pid() {
    local pid_hex
    pid_hex=$(printf '%04x' "$1")
    local dev tty
    for dev in /sys/bus/usb/devices/*/; do
        [[ "${dev}" == *:* ]] && continue
        [[ -f "${dev}idVendor" ]] || continue
        [[ "$(< "${dev}idVendor")" == "$VID" ]] || continue
        [[ "$(< "${dev}idProduct")" == "$pid_hex" ]] || continue
        if [[ -n "$SERIAL" ]]; then
            [[ -f "${dev}serial" ]] || continue
            [[ "$(< "${dev}serial")" == "$SERIAL" ]] || continue
        fi
        for tty in "${dev}"*/tty/tty*/; do
            [[ -d "$tty" ]] || continue
            printf "/dev/%s\n" "$(basename "$tty")"
            return 0
        done
    done
    return 1
}

# Wait for a USB PID to show up
function wait_for_pid() {
    local -i pid=$1 timeout_s=${2:-15} i
    for (( i=0; i < timeout_s * 5; i++ )); do
        find_port_by_pid "$pid" &>/dev/null && return 0
        sleep 0.2
    done
    return 1
}

# Wait for a USB PID to disappear
function wait_gone_pid() {
    local -i pid=$1 timeout_s=${2:-10} i
    for (( i=0; i < timeout_s * 5; i++ )); do
        find_port_by_pid "$pid" &>/dev/null || return 0
        sleep 0.2
    done
    return 1
}

# Raw serial port open as FD3
function port_open() {
    local -i i ok=0
    for (( i=0; i<25; i++ )); do
        if { exec 3<>"$1"; } 2>/dev/null;
        then
            ok=1
            break
        fi
        sleep 0.2
    done
    (( ok )) || die "cannot open $1\n"
    stty -F "$1" "$2" raw -echo cs8 -cstopb -parenb 2>/dev/null || \
    stty -f "$1" "$2" raw -echo cs8 -cstopb -parenb 2>/dev/null || true
}

# Raw serial port close (still FD3)
function port_close() { { exec 3>&-; } 2>/dev/null || true; }

# Read exactly N bytes from FD3; print as space-separated decimal values.
# Always returns 0, failures surface as empty output.
function port_read() {
    local -i n=$1 t=${2:-10}
    timeout "$t" dd <&3 bs=1 count="$n" 2>/dev/null | od -v -An -tu1 | tr -s ' \n' ' ' || true
}

# Framing
RECV_CMD=0
declare -a RECV_PAYLOAD=()

function send_frame() {
    local -i cmd=$1; shift
    local -ai payload=("$@")
    local -i plen=${#payload[@]}
    local -i lo=$(( plen & 0xFF )) hi=$(( (plen >> 8) & 0xFF ))
    local -i crc
    if (( plen > 0 )); then
        crc=$(crc32_mpeg2 "$cmd" "$lo" "$hi" "${payload[@]}")
    else
        crc=$(crc32_mpeg2 "$cmd" "$lo" "$hi")
    fi

    local oct="${BYTE_OCT[$SOF]}${BYTE_OCT[$cmd]}${BYTE_OCT[$lo]}${BYTE_OCT[$hi]}"
    if (( plen > 0 )); then
        local b
        for b in "${payload[@]}"; do oct+="${BYTE_OCT[$b]}"; done
    fi
    oct+="${BYTE_OCT[$(( crc & 0xFF ))]}${BYTE_OCT[$(( (crc >> 8) & 0xFF ))]}${BYTE_OCT[$(( (crc >> 16) & 0xFF ))]}${BYTE_OCT[$(( (crc >> 24) & 0xFF ))]}"
    printf '%b' "$oct" >&3 || die "write error on $PORT"
}

function recv_frame() {
    local -i t=${1:-10}
    local -a raw

    # Sync on SOF byte
    while true; do
        read -ra raw < <(port_read 1 "$t") || true
        [[ ${#raw[@]} -eq 0 ]] && return 1
        (( raw[0] == SOF )) && break
    done

    # CMD + LEN_LO + LEN_HI
    read -ra raw < <(port_read 3 5) || true
    [[ ${#raw[@]} -lt 3 ]] && return 1
    RECV_CMD=${raw[0]}
    local -i plen=$(( raw[1] | (raw[2] << 8) ))

    # Payload
    RECV_PAYLOAD=()
    if (( plen > 0 )); then
        read -ra RECV_PAYLOAD < <(port_read "$plen" 30) || true
        [[ ${#RECV_PAYLOAD[@]} -lt plen ]] && return 1
    fi

    # CRC (4 bytes, consumed)
    port_read 4 5 > /dev/null
    return 0
}

function expect_ack() {
    recv_frame "${1:-30}" || die "no response from device (timeout)"
    if (( RECV_CMD == RESP_NAK )); then
        local -i r=${RECV_PAYLOAD[0]:-255}
        case $r in
            1) die "NAK: bad CRC";;
            2) die "NAK: bad offset";;
            3) die "NAK: flash error";;
            4) die "NAK: invalid state";;
            *) die "NAK: code $r";;
        esac
    fi
    (( RECV_CMD == RESP_ACK )) || die "unexpected response 0x$(printf '%02X' "$RECV_CMD")"
}

# Image
# Slot is stored at byte offset 48 of the 512-byte image header.
function img_slot() {
    local s
    s=$(dd if="$1" bs=1 skip=48 count=1 2>/dev/null | od -v -An -tu1 | tr -d ' \n')
    [[ -n "$s" ]] || die "cannot read image header from $1"
    (( s == 0 || s == 1 )) || die "unexpected slot value $s in $1"
    printf '%s\n' "$s"
}

# DFU trigger; sends trigger to app, waits for bootloader to appear.
function do_enter_dfu() {
    if find_port_by_pid "$BL_PID" &>/dev/null; then
        printf "Bootloader already running\n"
        return 0
    fi

    local app_port
    app_port=$(find_port_by_pid "$APP_PID") || die "no device found (expected PID 0x5741 or 0xDF00)"

    # 8-byte magic: [0xAD 0xF0 0x00 0x00][CRC-32/MPEG-2 of those 4 bytes, big-endian]
    local -i crc
    crc=$(crc32_mpeg2 0xAD 0xF0 0x00 0x00)
    local trig
    trig="${BYTE_OCT[0xAD]}${BYTE_OCT[0xF0]}${BYTE_OCT[0]}${BYTE_OCT[0]}"
    trig+="${BYTE_OCT[$(( (crc >> 24) & 0xFF ))]}"
    trig+="${BYTE_OCT[$(( (crc >> 16) & 0xFF ))]}"
    trig+="${BYTE_OCT[$(( (crc >>  8) & 0xFF ))]}"
    trig+="${BYTE_OCT[$(( crc & 0xFF ))]}"

    port_open "$app_port" "$BAUD"
    sleep 0.1
    printf "%b" "$trig" >&3
    port_close

    printf "DFU trigger sent, waiting for bootloader ...\n"

    wait_gone_pid "$APP_PID" 10 || die "app did not disappear - DFU trigger likely failed"
    wait_for_pid "$BL_PID" 15  || die "bootloader did not appear - check device"
    sleep 0.5

    printf "Bootloader ready\n"
}

# Flash
function do_flash() {
    local img=$1
    local -i slot total
    slot=$(img_slot "$img")
    total=$(wc -c < "$img")
    printf "Image is %d bytes, slot %d\n" "$total" "$slot"

    printf "Erasing slot ...\n"
    local -a spl=( "$slot"
        $(( total & 0xFF )) $(( (total >> 8) & 0xFF ))
        $(( (total >> 16) & 0xFF )) $(( (total >> 24) & 0xFF )) )
    send_frame $CMD_START "${spl[@]}"
    expect_ack 60
    printf "Slot erased\n"

    local -i offset=0
    while (( offset < total )); do
        local -i n=$(( total - offset ))
        (( n > CHUNK_SIZE )) && n=$CHUNK_SIZE

        local -a chunk_data
        read -ra chunk_data < <(dd if="$img" bs=1 skip="$offset" count="$n" 2>/dev/null \
                                 | od -v -An -tu1 | tr -s ' \n' ' ') || true

        local -a cpl=(
            $(( offset & 0xFF )) $(( (offset >> 8) & 0xFF ))
            $(( (offset >> 16) & 0xFF )) $(( (offset >> 24) & 0xFF ))
            "${chunk_data[@]}"
        )
        send_frame $CMD_CHUNK "${cpl[@]}"
        expect_ack 10

        offset=$(( offset + n ))
        printf "\r  %3d%%  %d/%d bytes" $(( offset * 100 / total )) "$offset" "$total"
    done
    printf "\n"

    printf "Validating image ...\n"
    send_frame $CMD_FINISH
    expect_ack 30
    printf "Image validated\n"

    printf "Booting slot %d ...\n" "$slot"
    send_frame $CMD_BOOT "$slot"
    recv_frame 5 || true   # device may reboot before ACK reaches host
    printf "Boot triggered\n"
}

# Status
function do_status() {
    send_frame $CMD_STATUS
    recv_frame 10 || die "no STATUS response"
    (( RECV_CMD == CMD_STATUS )) || die "unexpected response 0x$(printf '%02X' "$RECV_CMD")"
    local -a d=("${RECV_PAYLOAD[@]}")
    (( ${#d[@]} >= 32 )) || die "short STATUS response: ${#d[@]} bytes"

    local ts="${d[11]}"
    (( ts == 255 )) && ts="none"

    local -i magic generation transfer_offset transfer_size

    # Using integer mul instead of shifts, vscode syntax highlighter cannot
    # handle nested shifting apparently ...
    magic=$(( d[0] + d[1]*256 + d[2]*65536 + d[3]*16777216 ))
    generation=$(( d[4] + d[5]*256 + d[6]*65536 + d[7]*16777216 ))
    transfer_offset=$(( d[12] + d[13]*256 + d[14]*65536 + d[15]*16777216 ))
    transfer_size=$(( d[16] + d[17]*256 + d[18]*65536 + d[19]*16777216 ))

    printf "magic:           0x%08X\n" "$magic"
    printf "generation:      %d\n"     "$generation"
    printf "active_slot:     %d\n"     "${d[8]}"
    printf "boot_count:      %d\n"     "${d[9]}"
    printf "confirmed:       %d\n"     "${d[10]}"
    printf "transfer_slot:   %s\n"     "$ts"
    printf "transfer_offset: %d\n"     "$transfer_offset"
    printf "transfer_size:   %d\n"     "$transfer_size"

    local -i active_slot="${d[8]}"
    send_frame $CMD_BOOT "$active_slot"
    recv_frame 5 || true   # device may reboot before ACK reaches host
}

# Argument parsing and real work
function usage() {
    cat >&2 <<'EOF'
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
EOF
    exit 1
}

[[ $# -eq 0 ]] && usage

while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--baud)   BAUD=$2; shift 2 ;;
        -p|--pid)    APP_PID=$(( $2 )); shift 2 ;;
        -s|--serial) SERIAL=$2; shift 2 ;;
        update|status) break ;;
        -h|--help) usage ;;
        *) die "unknown option: $1" ;;
    esac
done

CMD=$1; shift

case $CMD in
    update)
        [[ $# -ge 1 ]] || die "'update' requires a .elf file"
        [[ -f $1 ]]    || die "file not found: $1"
        elf_to_img "$1"
        do_enter_dfu
        PORT=$(find_port_by_pid "$BL_PID") || die "bootloader port not found after DFU"
        port_open "$PORT" "$BAUD"
        sleep 0.1
        do_flash "$IMG_TMP"
        port_close
        ;;
    status)
        do_enter_dfu
        PORT=$(find_port_by_pid "$BL_PID") || die "bootloader port not found after DFU"
        port_open "$PORT" "$BAUD"
        sleep 0.1
        do_status
        port_close
        ;;
esac
