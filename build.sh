#!/usr/bin/env bash
set -euo pipefail

CMD="${1:-build}"
CPUS=$(( $(nproc) > 2 ? $(nproc) - 2 : 1 ))
FIRMWARE_IMAGE="ymir-builder"
TEST_IMAGE="ymir-tester"

case "$CMD" in
help|--help|-h)
    cat <<'EOF'
Usage: ./build.sh [command]

Commands:
  build               Build the ymir bootloader ELF

  build-test-images   Build slot A and slot B test app images

  flash               Erase all flash and upload bootloader via ST-Link

  debug               Start OpenOCD GDB server on :3333 (Ctrl+C to stop and clean up)

  gdb                 Launch GDB connected to the OpenOCD server on :3333

  test                Run build verification tests (requires build and build-test-images first)

  test-hw             Build everything, flash bootloader via ST-Link, then run hardware OTA tests

  help                Show this message

All firmware commands run inside Docker. CPU usage capped at (nproc - 2).
EOF
    ;;

build)
    docker build -t "$FIRMWARE_IMAGE" docker/firmware
    docker run --rm --cpus "$CPUS" \
        -v "$(pwd)/firmware:/firmware" \
        "$FIRMWARE_IMAGE" \
        bash -c "cd /firmware/ymir \
                 && cmake --fresh --preset Debug \
                 && cmake --build --preset Debug -j ${CPUS}"
    ;;

build-test-images)
    docker build -t "$FIRMWARE_IMAGE" docker/firmware
    docker run --rm --cpus "$CPUS" \
        -v "$(pwd)/firmware:/firmware" \
        "$FIRMWARE_IMAGE" \
        bash -c "cd /firmware/test_app_baremetal/app \
                 && cmake --fresh --preset slot-a \
                 && cmake --build --preset slot-a -j ${CPUS} \
                 && cmake --fresh --preset slot-b \
                 && cmake --build --preset slot-b -j ${CPUS}"
    docker run --rm --cpus "$CPUS" \
        -v "$(pwd)/firmware:/firmware" \
        "$FIRMWARE_IMAGE" \
        bash -c "cd /firmware/test_app_rtos/app \
                 && cmake --fresh --preset slot-a \
                 && cmake --build --preset slot-a -j ${CPUS} \
                 && cmake --fresh --preset slot-b \
                 && cmake --build --preset slot-b -j ${CPUS}"
    ;;

flash)
    docker build -t "$FIRMWARE_IMAGE" docker/firmware
    docker run --rm \
        -v "$(pwd)/firmware:/firmware" \
        --privileged -v /dev/bus/usb:/dev/bus/usb \
        "$FIRMWARE_IMAGE" \
        openocd \
            -f interface/stlink.cfg \
            -f target/stm32f4x.cfg \
            -c 'init' \
            -c 'reset halt' \
            -c 'stm32f4x mass_erase 0' \
            -c 'program /firmware/build/Debug/ymir.elf verify' \
            -c 'reset run' \
            -c 'exit'
    ;;

debug)
    docker build -t "$FIRMWARE_IMAGE" docker/firmware
    docker run --rm \
        --privileged -v /dev/bus/usb:/dev/bus/usb \
        --network host \
        "$FIRMWARE_IMAGE" \
        openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
    ;;

gdb)
    docker run --rm -it --init \
        --network host \
        -v "$(pwd)/firmware:/firmware" \
        "$FIRMWARE_IMAGE" \
        gdb-multiarch /firmware/build/Debug/ymir.elf \
        -ex "target extended-remote :3333"
    ;;

test)
    docker build -t "$TEST_IMAGE" docker/tests
    docker run --rm \
        -v "$(pwd)/firmware:/firmware:ro" \
        -v "$(pwd)/scripts:/scripts:ro" \
        -v "$(pwd)/tests:/tests:ro" \
        "$TEST_IMAGE" \
        bash -c "cmake -S /tests/unit -B /tmp/unit_build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
                 && cmake --build /tmp/unit_build \
                 && /tmp/unit_build/unit_tests \
                 && pytest /tests/build/ /tests/unit/ -v -p no:cacheprovider"
    ;;

test-hw)
    docker build -t "$FIRMWARE_IMAGE" docker/firmware
    docker run --rm --cpus "$CPUS" \
        -v "$(pwd)/firmware:/firmware" \
        "$FIRMWARE_IMAGE" \
        bash -c "cd /firmware/ymir \
                 && cmake --fresh --preset Debug \
                 && cmake --build --preset Debug -j ${CPUS} \
                 && cd /firmware/test_app_baremetal/app \
                 && cmake --fresh --preset slot-a \
                 && cmake --build --preset slot-a -j ${CPUS} \
                 && cmake --fresh --preset slot-b \
                 && cmake --build --preset slot-b -j ${CPUS}"
    docker run --rm \
        --privileged -v /dev/bus/usb:/dev/bus/usb \
        -v "$(pwd)/firmware:/firmware" \
        -v "$(pwd)/scripts:/scripts" \
        "$FIRMWARE_IMAGE" \
        bash -c "openocd \
                 -f interface/stlink.cfg \
                 -f target/stm32f4x.cfg \
                 -c 'init' \
                 -c 'reset halt' \
                 -c 'stm32f4x mass_erase 0' \
                 -c 'program /firmware/build/Debug/ymir.elf verify' \
                 -c 'reset run' \
                 -c 'exit'"
    [[ -d .venv ]] || python3 -m venv --system-site-packages .venv
    .venv/bin/pip install -q -r requirements.txt
    .venv/bin/pytest tests/hardware/ -v -p no:cacheprovider
    ;;

*)
    echo "Unknown command: ${CMD}" >&2
    echo "Run ./build.sh help for usage." >&2
    exit 1
    ;;
esac
