// sidcore: the entry point and runtime of the relocatable ARM core (see sidcore_api.h).
// GPL v2 or later, like libsidplay.
#include <stddef.h>
#include <string.h>
#include <time.h>
#include "player.h"

#include "sidcore_api.h"

// ---------------------------------------------------------------- arena allocator
// First fit with coalescing, inside the one block the host gives us.  Every block starts with
// a header; free blocks form a list sorted by address.

struct Block { size_t size; Block* next; };           // size includes the header
static const size_t ALIGN = 8, HDR = (sizeof(Block) + ALIGN - 1) & ~(ALIGN - 1);
static Block* freeList = 0;
static size_t inUse = 0, peakUse = 0;
static bool allocFailed = false;

static void arenaInit(void* mem, size_t len)
{
	size_t skip = (ALIGN - ((size_t)mem & (ALIGN - 1))) & (ALIGN - 1);
	freeList = (Block*)((char*)mem + skip);
	freeList->size = (len - skip) & ~(ALIGN - 1);
	freeList->next = 0;
	inUse = peakUse = 0;
	allocFailed = false;
}

static void* arenaAlloc(size_t n)
{
	size_t need = (n + HDR + ALIGN - 1) & ~(ALIGN - 1);
	for (Block **pp = &freeList, *b = freeList; b; pp = &b->next, b = b->next) {
		if (b->size < need) continue;
		if (b->size - need >= HDR + ALIGN) {           // split
			Block* rest = (Block*)((char*)b + need);
			rest->size = b->size - need;
			rest->next = b->next;
			*pp = rest;
			b->size = need;
		} else {
			*pp = b->next;
		}
		inUse += b->size;
		if (inUse > peakUse) peakUse = inUse;
		return (char*)b + HDR;
	}
	allocFailed = true;
	return 0;
}

static void arenaFree(void* p)
{
	if (!p) return;
	Block* b = (Block*)((char*)p - HDR);
	inUse -= b->size;
	Block **pp = &freeList;
	while (*pp && *pp < b) pp = &(*pp)->next;
	b->next = *pp;
	*pp = b;
	if (b->next && (char*)b + b->size == (char*)b->next) {          // merge with the next
		b->size += b->next->size;
		b->next = b->next->next;
	}
	if (pp != &freeList) {                                           // merge with the previous
		Block* prev = (Block*)((char*)pp - offsetof(Block, next));
		if ((char*)prev + prev->size == (char*)b) {
			prev->size += b->size;
			prev->next = b->next;
		}
	}
}

void* operator new(size_t n) { return arenaAlloc(n); }
void* operator new[](size_t n) { return arenaAlloc(n); }
void operator delete(void* p) noexcept { arenaFree(p); }
void operator delete[](void* p) noexcept { arenaFree(p); }
void operator delete(void* p, size_t) noexcept { arenaFree(p); }
void operator delete[](void* p, size_t) noexcept { arenaFree(p); }

// ---------------------------------------------------------------- C/C++ runtime bits
// (Only in the ARM block: the simulator DLL (SIDCORE_DLL) has the normal Windows C runtime.)
static unsigned long randomSeed = 0;
#if !defined(SIDCORE_DLL)
extern "C" time_t time(time_t* t) { if (t) *t = (time_t)randomSeed; return (time_t)randomSeed; }
// newlib's strlen is Thumb code, which the linker reaches through a veneer holding an absolute address
// that has no relocation record.  Our own ARM version avoids that (blob_pack.py rejects veneers).
extern "C" size_t strlen(const char* s) { const char* p = s; while (*p) p++; return (size_t)(p - s); }
extern "C" void __cxa_pure_virtual() { for (;;) ; }
extern "C" int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }   // nothing runs at exit
void* __dso_handle = 0;

// Static constructors (run once at SC_INIT; the host has already relocated the pointers).
typedef void (*InitFunc)();
extern "C" InitFunc __init_array_start[], __init_array_end[];
#define RUN_STATIC_CONSTRUCTORS() \
	for (InitFunc* f = __init_array_start; f < __init_array_end; f++) (*f)()
#else
#define RUN_STATIC_CONSTRUCTORS()   // the DLL's own start-up does it
// The simulator unloads the DLL after each PceNativeCall, which would lose the player's state (and
// the stream callback).  Pin it: once loaded it stays until the simulator exits.
extern "C" __declspec(dllimport) int __stdcall GetModuleHandleExA(unsigned long, const char*, void**);
static void pinDll()
{
	void* module;
	GetModuleHandleExA(0x4 /* FROM_ADDRESS */ | 0x1 /* PIN */, (const char*)&pinDll, &module);
}
#endif

#if !defined(SIDCORE_DLL)
// ---------------------------------------------------------------- the core's own stacks
// Both ways into the core move onto a stack of its own first: the stack Palm OS gives native code is
// too small for the engine on the Tungsten E2 (SC_INIT crashed it with a "Fatal Exception"; found in
// CloudpilotEmu 2026-11-12).  sidcoreEntry (the app's thread) and sidcoreStream (the Sound Manager's
// thread) can run at the same time, so each has its own.  Each is a few instructions that save the
// caller's sp, switch, call the C function (arguments r0-r3 untouched) and switch back.
static const size_t CORE_STACK = 16384;
extern "C" {
unsigned long long entryStack[CORE_STACK / 8], streamStack[CORE_STACK / 8];   // (8-byte aligned)
unsigned long sidcoreMain(const void* emulStateP, void* userData, void* call68KFuncP);
long sidcoreStreamMain(void* userData, unsigned long channel, void* buffer, unsigned long frames);
long sidcoreStream(void* userData, unsigned long channel, void* buffer, unsigned long frames);
}
#define CORE_STACK_SWITCH(section, name, target, stack) \
	"	.section " section "\n" \
	"	.global " name "\n" \
	"	.type " name ", %function\n" \
	name ":\n" \
	"	push	{r4, lr}\n" \
	"	mov	r4, sp\n" \
	"	ldr	ip, 1f\n" \
	"	add	sp, ip, #16384\n"       /* (CORE_STACK) the top of the stack */ \
	"	bl	" target "\n" \
	"	mov	sp, r4\n" \
	"	pop	{r4, lr}\n" \
	"	bx	lr\n"                    /* (ARMv4T: a pop into pc can't return to Thumb code) */ \
	"1:	.word	" stack "\n" \
	"	.previous\n"
__asm__(CORE_STACK_SWITCH(".entry,\"ax\",%progbits", "sidcoreEntry", "sidcoreMain", "entryStack"));
__asm__(CORE_STACK_SWITCH(".text", "sidcoreStream", "sidcoreStreamMain", "streamStack"));
static_assert(CORE_STACK == 16384, "the stack switch above adds 16384");
#endif

// ---------------------------------------------------------------- the player
static emuEngine* engine = 0;
static sidTune* tune = 0;
static bool initDone = false;
static volatile bool playing = false;
static volatile long volume = 256;      // SC_VOLUME: 256 = libsidplay's own level

// The volume, after the mixer: libsidplay leaves room for four voices, so most tunes peak near half
// of full scale and sound quiet on a handheld's speaker.  Louder than that clips (hard, at full
// scale).  256 leaves the samples untouched (the render tests' checksums still hold).
static void applyVolume(void* buffer, unsigned long samples)
{
	long v = volume;
	if (v == 256)
		return;
	short* s = (short*)buffer;
	for (unsigned long i = 0; i < samples; i++) {
		long x = ((long)s[i] * v) >> 8;
		s[i] = (short)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
	}
}

// Sound Manager stream callback (SndStreamCreate with armNative = true): called from the sound
// thread for each buffer.  Arguments are native (no byte swapping).  The app stops the stream
// before sending commands that change the tune, so this never runs during one.
#if defined(SIDCORE_DLL)
extern "C" __declspec(dllexport)
long sidcoreStream(void* userData, unsigned long channel, void* buffer, unsigned long frames)
#else
extern "C" long sidcoreStreamMain(void* userData, unsigned long channel, void* buffer, unsigned long frames)
#endif
{
	if (playing && engine && tune) {
		sidEmuFillBuffer(*engine, *tune, buffer, (udword)(frames * 2));
		applyVolume(buffer, frames);
	} else
		memset(buffer, 0, frames * 2);
	return 0;
}

static unsigned long command(SidCoreCall* c)
{
	switch (c->cmd) {
	case SC_INIT:
		if (!initDone) {
			RUN_STATIC_CONSTRUCTORS();
			initDone = true;
		}
		if (engine) return SC_ERR_STATE;
		// libsidplay does not check all its start-up allocations, so refuse small arenas up front
		if (c->a[1] < SIDCORE_MIN_ARENA) return SC_ERR_MEMORY;
		arenaInit((void*)c->a[0], c->a[1]);
		randomSeed = c->a[2];
		engine = new emuEngine;
		if (!engine || !*engine) return SC_ERR_MEMORY;
		c->a[0] = (unsigned long)&sidcoreStream;
		return 0;

	case SC_CONFIG: {
		if (!engine) return SC_ERR_STATE;
		emuConfig cfg;
		engine->getConfig(cfg);
		cfg.frequency = (uword)c->a[0];
		cfg.bitsPerSample = SIDEMU_16BIT;
		cfg.sampleFormat = SIDEMU_SIGNED_PCM;
		cfg.channels = SIDEMU_MONO;
		cfg.mos8580 = (c->a[1] != 1);
		cfg.measuredVolume = true;
		cfg.emulateFilter = (c->a[2] == 1);
		cfg.memoryMode = MPU_BANK_SWITCHING;
		if (c->a[3] == 1 || c->a[3] == 2) {
			cfg.clockSpeed = (c->a[3] == 1) ? SIDTUNE_CLOCK_PAL : SIDTUNE_CLOCK_NTSC;
			cfg.forceSongSpeed = true;
		} else {
			cfg.clockSpeed = SIDTUNE_CLOCK_PAL;
			cfg.forceSongSpeed = false;
		}
		return engine->setConfig(cfg) ? 0 : SC_ERR_CONFIG;
	}

	case SC_LOAD: {
		if (!engine) return SC_ERR_STATE;
		playing = false;
		delete tune;
		tune = new sidTune((const ubyte*)c->a[0], (udword)c->a[1]);
		if (!tune) return SC_ERR_MEMORY;
		sidTuneInfo info;
		tune->getInfo(info);
		c->a[0] = info.songs;
		c->a[1] = info.startSong;
		c->a[2] = info.clock;
		c->a[3] = info.sidModel;
		c->a[4] = (unsigned long)info.statusString;
		c->str[0] = info.nameString;
		c->str[1] = info.authorString;
		c->str[2] = info.copyrightString;
		return *tune ? 0 : SC_ERR_TUNE;
	}

	case SC_START:
		if (!engine || !tune || !*tune) return SC_ERR_STATE;
		playing = false;
		if (!sidEmuInitializeSong(*engine, *tune, (uword)c->a[0])) return SC_ERR_TUNE;
		{
			sidTuneInfo info;                  // the song actually chosen (0 = the start song)
			tune->getInfo(info);
			c->a[0] = info.currentSong;
		}
		playing = true;
		return 0;

	case SC_FILL:
		if (!engine || !tune) return SC_ERR_STATE;
		sidEmuFillBuffer(*engine, *tune, (void*)c->a[0], (udword)c->a[1]);
		applyVolume((void*)c->a[0], c->a[1] / 2);
		return 0;

	case SC_VOLUME:
		volume = (long)(c->a[0] > 4096 ? 4096 : c->a[0]);
		return 0;

	case SC_LEVELS: {
		// for the level meters, per voice since the last call (reading resets them):
		//   a0..a2 = peak output 0..255, measured by the mixer (mixing.cpp, fill16bitMono)
		//   a3..a5 = 1 if a new note started (gate on, or a pitch jump; 6581_.cpp sidEmuSet)
		extern ubyte voicePeak[3], voiceNote[3];
		for (int v = 0; v < 3; v++) {
			unsigned long p = (unsigned long)voicePeak[v] * 2;
			c->a[v] = p > 255 ? 255 : p;
			c->a[3 + v] = voiceNote[v];
			voicePeak[v] = 0;
			voiceNote[v] = 0;
		}
		return engine ? 0 : SC_ERR_STATE;
	}

	case SC_SHUTDOWN:
		playing = false;
		c->a[0] = peakUse;
		delete tune; tune = 0;
		delete engine; engine = 0;
		return 0;
	}
	return SC_ERR_COMMAND;
}

// The entry point: PceNativeCall's signature.  On ARM the stack switch above (sidcoreEntry, placed
// first in the image by sidcore.ld) calls sidcoreMain; in the simulator DLL it is an export
// (PceNativeCall("sidcore.dll\0sidcoreEntry", ...)).
#if defined(SIDCORE_DLL)
extern "C" __declspec(dllexport)
unsigned long sidcoreEntry(const void* emulStateP, void* userData, void* call68KFuncP)
#else
extern "C" unsigned long sidcoreMain(const void* emulStateP, void* userData, void* call68KFuncP)
#endif
{
	SidCoreCall* c = (SidCoreCall*)userData;
#if defined(SIDCORE_DLL)
	pinDll();
#endif
	c->result = command(c);
	if (c->result == 0 && allocFailed) c->result = SC_ERR_MEMORY;
	return c->result;
}
