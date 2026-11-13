#!/bin/bash
# Run in WSL Ubuntu: build tests/blob_harness.c for ARM Linux and use it to run the relocatable core
# (build/blob/sidcore.bin, made on Windows by tools/blob_build.sh) under qemu-arm.
#   tools/blob_check.sh                           build the harness only
#   tools/blob_check.sh tune.sid out.wav [opts]   render (opts: -s N -t SEC -r HZ -v VOL, 256 = 100%)
set -e
cd "$(dirname "$0")/.."
mkdir -p build/arm
arm-linux-gnueabi-gcc -O2 -march=armv5te -marm -mfloat-abi=soft -static -Wall -Isrc/blob \
  tests/blob_harness.c -o build/arm/blob_harness
if [ -n "$1" ]; then
  qemu-arm build/arm/blob_harness build/blob/sidcore.bin "$@"
fi
