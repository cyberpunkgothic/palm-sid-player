#!/bin/bash
# Cross-platform check (run in WSL Ubuntu): build sid2wav for an ARMv5TE without an FPU (the Palm TX's
# XScale), with software floating point, and run it under qemu-arm.
# Needs g++-arm-linux-gnueabi and qemu-user (and g++-multilib for the x86 build).
#   tools/arm_check.sh ref|core [tune.sid out.wav [sid2wav options]]
#   CPU=x86 tools/arm_check.sh ...   same, but 32-bit x86 GCC (to tell compiler from CPU differences)
# Without a tune it only builds build/arm/sid2wav_<which>_arm (or _x86gcc).
set -e
cd "$(dirname "$0")/.."
which=${1:-ref}
case $which in
  ref)  S=vendor/src/libsidplay-1.36.60/src ;;
  core) S=src/core ;;
  *) echo "usage: $0 ref|core [tune out [options]]"; exit 2 ;;
esac
mkdir -p build/arm
if [ "$CPU" = x86 ]; then
  exe=build/arm/sid2wav_${which}_x86gcc
  CXX="g++ -m32 -msse2 -mfpmath=sse"; RUN=""
else
  exe=build/arm/sid2wav_${which}_arm
  CXX="arm-linux-gnueabi-g++ -march=armv5te -mtune=xscale -marm -mfloat-abi=soft -fsigned-char"
  RUN=qemu-arm
fi
$CXX -O2 ${OPT} -static -w \
  -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Isrc/pc/config -I$S \
  src/pc/sid2wav.cpp $S/6510_.cpp $S/6581_.cpp $S/eeconfig.cpp $S/envelope.cpp $S/fformat_.cpp \
  $S/info_.cpp $S/mixing.cpp $S/mus_.cpp $S/player.cpp $S/pp_.cpp $S/psid_.cpp $S/samples.cpp \
  $S/sid_.cpp $S/sidtune.cpp -o $exe
if [ -n "$2" ]; then
  shift
  $RUN $exe "$@"
fi
