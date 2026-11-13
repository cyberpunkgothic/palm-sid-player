# SID Player DX for Palm OS 5 (the C64-style skin): the 68k app (run in WSL; tools/palm_build.sh runs everything in order).
# Toolchain: Dmitry Grinberg's GCC 9.1 for Palm OS + PilRC 3.3 (palm2000 dev package v3).
PALMDEV  ?= $(HOME)/palmdev/palmdev_V3/buildtools
TOOLCHAIN = $(PALMDEV)/toolchain/bin
SDK       = $(PALMDEV)/palm-os-sdk-master/sdk-5r3/include
PILRC     = $(PALMDEV)/pilrc3_3_unofficial/bin/pilrc
CC        = $(TOOLCHAIN)/m68k-none-elf-gcc
OBJCOPY   = $(TOOLCHAIN)/m68k-none-elf-objcopy
OBJDUMP   = $(TOOLCHAIN)/m68k-none-elf-objdump
NM        = $(TOOLCHAIN)/m68k-none-elf-nm

# Intermediate files go in OUT; the finished .prc is copied to build/palm (with the shared sound core).
APP      = SIDPlayerDX
NAME     = SID Player DX
CREATOR  = SdPX
OUT      = ../../build/palm_c64
SHARED   = ../../build/palm
# -fcall-used-d2: Palm OS system calls may change d2 (selector traps load it), while GCC normally
# assumes d2 survives a call.  (A precaution: the simulator crashes it was tried for were memset's.)
OPT     ?= -Os
COMMON   = -Wno-multichar -funsafe-math-optimizations $(OPT) -fcall-used-d2 -m68000 -mno-align-int -mpcrel -fpic -fshort-enums -mshort
WARN     = -Wall -Wno-unused-parameter -Wno-old-style-declaration -Wno-unused-function -Wno-switch
CFLAGS   = $(WARN) $(COMMON) -I. -I../blob -ffunction-sections -fdata-sections
LDFLAGS  = $(WARN) $(COMMON) -Wl,--gc-sections -Wl,-T linker.lkr
INCS     = -isystem$(SDK) -isystem$(SDK)/Core -isystem$(SDK)/Core/Hardware -isystem$(SDK)/Core/System \
           -isystem$(SDK)/Core/UI -isystem$(SDK)/Dynamic -isystem$(SDK)/Libraries \
           -isystem$(SDK)/Libraries/PalmOSGlue -isystem$(SDK)/Extensions/ExpansionMgr

$(SHARED)/$(APP).prc: $(OUT)/code0001.bin sidplayer.rcp resource.h $(SHARED)/core.rcp $(OUT)/icon_lg_8.bmp $(OUT)/slider_bg.bmp
	cp $(SHARED)/arm*.bin resource.h $(OUT)/
	cat sidplayer.rcp $(SHARED)/core.rcp > $(OUT)/all.rcp
	cd $(OUT) && $(PILRC) -q -ro -o $(APP).prc -creator $(CREATOR) -type appl -name "$(NAME)" all.rcp
	cp $(OUT)/$(APP).prc $(SHARED)/

$(OUT)/icon_lg_8.bmp: ../../tools/gen_icon.py
	python3 ../../tools/gen_icon.py $(OUT)

$(OUT)/slider_bg.bmp: ../../tools/gen_slider.py ../../tools/gen_icon.py
	python3 ../../tools/gen_slider.py $(OUT)

$(OUT)/code0001.bin: $(OUT)/main.elf
	@# Palm OS loads the code anywhere: an absolute jsr/jmp into it (as libgcc's helpers do) would
	@# crash.  main.c supplies its own __udivsi3/__umodsi3/__mulsi3; fail if any absolute call remains.
	@if $(OBJDUMP) -d $< | grep -E "(jsr|jmp) [0-9a-f]{8} <"; then echo "absolute jump in code: see above"; exit 1; fi
	@# Nothing may come from the toolchain's libraries: they are built with 32-bit int, the app with
	@# -mshort (16-bit), so their arguments don't line up (library memset wiped the stack that way).
	@$(NM) $(OUT)/main.o | awk '$$2 ~ /[TtW]/ {print $$3}' | sort > $(OUT)/own.syms
	@$(NM) $< | awk '$$2 ~ /[TtW]/ {print $$3}' | sort > $(OUT)/all.syms
	@if comm -23 $(OUT)/all.syms $(OUT)/own.syms | grep .; then echo "library code linked in: see above"; exit 1; fi
	$(OBJCOPY) -O binary $< $@ -j.vec -j.text -j.rodata

$(OUT)/main.elf: $(OUT)/main.o linker.lkr
	$(CC) -o $@ $(LDFLAGS) $(OUT)/main.o

$(OUT)/main.o: main.c font8.h resource.h ../blob/sidcore_api.h Makefile
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

clean:
	rm -f $(OUT)/main.o $(OUT)/main.elf $(OUT)/code0001.bin $(OUT)/$(APP).prc $(SHARED)/$(APP).prc

.PHONY: clean
