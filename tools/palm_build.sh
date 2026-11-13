#!/bin/bash
# Build the Palm app (run from Git Bash on Windows):
#   1. the ARM core (tools/blob_build.sh) and its resources (tools/core_rcp.py)
#   2. the simulator's x86 copy of the core (tools/pc_build.bat dll; copied next to the TX simulator)
#   3. the two 68k apps and their .prc files (in WSL):
#      src/palm_std (the standard Palm OS interface) -> build/palm/SIDPlayer.prc
#      src/palm_c64 (the C64-style skin)             -> build/palm/SIDPlayerDX.prc
#   4. both copied to the project folder, beside the README and the manual (as the other projects
#      keep their finished disks there)
set -e
cd "$(dirname "$0")/.."
bash tools/blob_build.sh
python -I tools/core_rcp.py
cmd //c "$(cygpath -w "$PWD/tools/pc_build.bat")" dll
for app in palm_std palm_c64; do
	MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -u chris -- make -C /mnt/c/Projects/sid-player/src/$app
done
cp build/palm/SIDPlayer.prc build/palm/SIDPlayerDX.prc .
ls -l SIDPlayer.prc SIDPlayerDX.prc
