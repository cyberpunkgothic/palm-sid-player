#!/bin/bash
# Build the relocatable ARM core (ARMv4T, software floating point, tuned for the XScale):
#   build/blob/sidcore.elf  (ordinary code linked at address 0; the linker keeps its relocation records)
#   build/blob/sidcore.bin  (packed by tools/blob_pack.py: image + list of absolute addresses to fix up,
#                            see src/blob/sidcore_api.h)
# Run from Git Bash on Windows; uses the xPack arm-none-eabi GCC in %LOCALAPPDATA%\Programs\PalmDev.
set -e
cd "$(dirname "$0")/.."
G="$LOCALAPPDATA/Programs/PalmDev/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin"
OUT=${BLOB_OUT:-build/blob}          # (BLOB_OUT / BLOB_EXTRA: a copy built elsewhere / extra flags)
mkdir -p $OUT/obj
# ARMv4T, not ARMv5TE: some Palm OS 5 handhelds have ARMv4T processors (TI OMAP: Tungsten T/T2/E,
# Zire 21/71, Treo 600; Samsung S3C2410: Z22), where ARMv5 instructions (blx, clz, ldrd/strd, the DSP
# multiplies) are undefined.  The libraries linked are the default ARMv4T ones.  (2026-11-12)
FLAGS="-O2 -march=armv4t -mtune=xscale -marm -mfloat-abi=soft -fsigned-char -fno-exceptions -fno-rtti
  -ffunction-sections -fdata-sections -fno-threadsafe-statics -fno-use-cxa-atexit -w -Wno-narrowing
  -Isrc/blob/config -Isrc/core -Isrc/blob $BLOB_EXTRA"
OBJS=""
for f in src/blob/sidcore.cpp src/core/{6510_,6581_,eeconfig,envelope,mixing,player,psid_,samples,sidtune,mus_,fformat_}.cpp; do
  o=$OUT/obj/$(basename ${f%.cpp}).o
  "$G/arm-none-eabi-g++.exe" $FLAGS -c $f -o $o
  OBJS="$OBJS $o"
done
"$G/arm-none-eabi-g++.exe" -march=armv4t -marm -mfloat-abi=soft -nostartfiles -nostdlib -static \
  -Wl,-T,src/blob/sidcore.ld -Wl,--gc-sections -Wl,--emit-relocs -Wl,--no-warn-rwx-segments \
  $OBJS -Wl,--start-group -lc_nano -lm -lgcc -Wl,--end-group -o $OUT/sidcore.elf
python -I tools/blob_pack.py $OUT/sidcore.elf $OUT/sidcore.bin
# Refuse any ARMv5-only instruction.  (Decoded as ARMv5TE so they have names; objdump shows literal
# data as .word, so it can't match.)
V5=$("$G/arm-none-eabi-objdump.exe" -d -m armv5te $OUT/sidcore.elf | grep -P \
  '\t(blx|clz|ldrd|strd|smul[bt][bt]|smulw[bt]|smla[bt][bt]|smlaw[bt]|smlal[bt][bt]|qd?(add|sub)|pld|bkpt|mcrr|mrrc|ldc2|stc2|mcr2|mrc2|cdp2)(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?\s' || true)
if [ -n "$V5" ]; then
  echo "ARMv5-only instructions in the core (it must run on ARMv4T):"; echo "$V5" | head -20
  exit 1
fi
