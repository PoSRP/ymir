FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    gdb-multiarch \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    cmake \
    ninja-build \
    openocd \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /firmware
