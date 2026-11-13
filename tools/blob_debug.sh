#!/bin/bash
# Run in WSL: run the core under qemu-arm with gdb attached and report where it crashes.
#   tools/blob_debug.sh tune.sid [harness options]
# The core's symbols are loaded at its run-time base (the harness prints it with BLOB_VERBOSE=1).
cd "$(dirname "$0")/.."
BASE=$(BLOB_VERBOSE=1 BLOB_ARENA_KB=${BLOB_ARENA_KB:-1024} qemu-arm build/arm/blob_harness build/blob/sidcore.bin "$1" /tmp/dbg.wav -t 1 2>&1 | sed -n 's/^core at //p')
tune=$1; shift
qemu-arm -g 12345 build/arm/blob_harness build/blob/sidcore.bin "$tune" /tmp/dbg.wav -t 1 "$@" &
sleep 1
gdb-multiarch -q -batch \
  -ex "file build/arm/blob_harness" \
  -ex "add-symbol-file build/blob/sidcore.elf -o $BASE" \
  -ex "target remote :12345" \
  -ex "continue" \
  -ex "info registers pc lr sp" \
  -ex "bt 12" \
  -ex "x/6i \$pc-12" 2>&1 | grep -v "^warning\|^Reading\|^add symbol\|^\s*(y or n)"
wait
