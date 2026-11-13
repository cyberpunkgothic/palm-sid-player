/* SID Player DX for Palm OS 5, the C64-style version (68k app; the sound core is native ARM code, see src/blob).
 * GPL v2 or later.  Built with Dmitry Grinberg's GCC 9.1 for Palm OS: no global variables
 * (state lives in one Globals block found through the Feature Manager).
 */
#include <PalmOS.h>
#include <VFSMgr.h>
#include <PceNativeCall.h>
#include <SoundMgr.h>
#include <PenInputMgr.h>
#include "resource.h"
#include "sidcore_api.h"

/* palmOne 5-way navigator bits in a key event's keyCode (palmOneNavigator.h: navBitsAll,
   navChangeBitsAll), defined here rather than including palmOne's headers */
#define NAV_BITS_ALL        0x001F
#define NAV_CHANGE_BITS_ALL 0x1F00

#define appCreator      'SdPX'           /* the standard version (src/palm_std) is 'SdPl' */
#define ftrGlobals      0
#define MAX_FILES       256
#define LOADED_GONE     MAX_FILES        /* g->loaded: a tune is loaded but no longer listed */
#define CARD_UPDATE_CODE 1               /* frmUpdateEvent: a card went in or out */
#define NAME_LEN        64
#define SID_DIR         "/SID/"
#define SAMPLE_RATE     44100
#define ARENA_SIZE      (640UL * 1024)
#define STREAM_FRAMES   2048
#define RING_BYTES      (SAMPLE_RATE * 2UL)        /* simulator: one second of 16-bit mono */
/* MemChunkNew flags: non-movable, and allow more than 64 KB (as MemGluePtrNew does) */
#define chunkFlagsLarge (memNewChunkFlagNonMovable | 0x1000)

/* Settings, saved with PrefSetAppPreferences.  clock: 0 = the tune's own (PAL if it doesn't say),
   1 = force PAL, 2 = force NTSC.  volume: in steps of 5%, 0-VOL_MAX (20 = 100%, libsidplay's own
   level, as v1.0 played; 200% is the most: on the TX's speaker more was uncomfortably loud and
   clipped, and 100% was too loud on earbuds).  The defaults are the user's SIDPLAY/w settings.
   (Version 1, v1.0, had no volume: its settings are kept.  Version 2, a test build, counted in tens
   of per cent up to 400%.) */
typedef struct { UInt8 version, chip6581, filter, clock, volume, pad; } SidPrefs;
#define PREFS_VERSION 3
#define VOL_MAX 40
#define VOL_DEFAULT 20

typedef struct {
	/* the core */
	Boolean simulator;          /* Palm OS Simulator: the core is sidcore.dll (x86) */
	UInt8* coreBase;            /* the core, 8-byte aligned (inside coreChunk) */
	UInt8* coreChunk;           /* its memory block, as allocated (freed on exit) */
	UInt32 coreEntry;
	UInt8* arena;
	UInt32 streamFunc;          /* native stream callback inside the core */
	SndStreamRef stream;
	Boolean streamOpen, coreReady;
	/* files */
	UInt16 volRef;
	Boolean haveVolume;
	UInt16 nFiles;
	char names[MAX_FILES][NAME_LEN];
	/* the loaded tune */
	Int16 loaded;               /* index in names; -1 = none; LOADED_GONE = not in the list */
	char loadedName[NAME_LEN];  /* its file name, to find it again after a card change */
	UInt16 songs, song;
	Boolean playing;
	UInt32 startTicks, playedTicks, shownSecs;
	UInt16 fillLogs, callbackLogs;      /* debug log: only the first few of these */
	volatile UInt32 callbackCount;      /* simulator: stream callbacks so far (debug log) */
	/* simulator only: samples made on the main thread for the stream callback (see simFillRing) */
	UInt8* ring;
	volatile UInt32 ringRead, ringWrite;
	char title[33], author[33], copyright[33];
	SidPrefs prefs;                      /* the settings */
	UInt32 awakeTicks;                   /* last auto-off timer reset (see keepAwake) */
	volatile Boolean sleepPaused;        /* paused by the power button (see sleepNotify) */
	volatile Boolean cardChanged;        /* a card went in or out (see cardNotify) */
	/* the C64-style screen (see drawScene) */
	UInt16 blocks[MAX_FILES];           /* file sizes in 254-byte blocks, as a 1541 lists them */
	Int16 cursor, topIndex;              /* the highlighted tune, the first one shown */
	UInt8 levels[3];                     /* voice meters, 0..255 */
	UInt32 meterTicks;
	Coord scrW, scrH, panelX, stripY;    /* native pixels */
	BitmapType* stripBmp;
	BitmapTypeV3* stripV3;
	UInt16* stripBits;
} Globals;

/* --------------------------------------------------------------- 32-bit arithmetic
   The 68000 has no 32-bit multiply or divide, so GCC calls these.  libgcc's own versions call each
   other at absolute addresses, which are wrong once Palm OS loads the code, so we supply our own
   (pure shifts and adds; nothing here may use * / or % on 32-bit values).  The Makefile checks that
   no absolute jumps remain. */
UInt32 __udivsi3(UInt32 n, UInt32 d)
{
	UInt32 q = 0, r = 0;
	Int16 i;
	for (i = 31; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) { r -= d; q |= 1UL << i; }
	}
	return q;
}

UInt32 __umodsi3(UInt32 n, UInt32 d)
{
	UInt32 r = 0;
	Int16 i;
	for (i = 31; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) r -= d;
	}
	return r;
}

UInt32 __mulsi3(UInt32 a, UInt32 b)
{
	UInt32 r = 0;
	while (b) {
		if (b & 1) r += a;
		a <<= 1;
		b >>= 1;
	}
	return r;
}

/* GCC calls memset/memcpy itself (e.g. to zero `UInt32 a[6] = { 0 }`).  The toolchain's library
   versions were built with 32-bit int, but this app uses -mshort (16-bit int), so every argument after
   the int lands 2 bytes off and memset wiped the stack.  Ours are built with the same settings.
   (no-tree-loop-distribute-patterns: stop GCC turning these loops back into memset calls) */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void* memset(void* dst, int c, unsigned long n)
{
	UInt8* d = (UInt8*)dst;
	while (n--) *d++ = (UInt8)c;
	return dst;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void* memcpy(void* dst, const void* src, unsigned long n)
{
	UInt8* d = (UInt8*)dst;
	const UInt8* s = (const UInt8*)src;
	while (n--) *d++ = *s++;
	return dst;
}

/* --------------------------------------------------------------- helpers */

static Globals* G(void)
{
	UInt32 v = 0;
	FtrGet(appCreator, ftrGlobals, &v);
	return (Globals*)v;
}

static UInt32 swap32(UInt32 v)
{
	return (v >> 24) | ((v >> 8) & 0xFF00UL) | ((v << 8) & 0xFF0000UL) | (v << 24);
}

static UInt32 readLE32(const UInt8* p)
{
	return (UInt32)p[0] | ((UInt32)p[1] << 8) | ((UInt32)p[2] << 16) | ((UInt32)p[3] << 24);
}

static void writeLE32(UInt8* p, UInt32 v)
{
	p[0] = (UInt8)v; p[1] = (UInt8)(v >> 8); p[2] = (UInt8)(v >> 16); p[3] = (UInt8)(v >> 24);
}

static void showError(const char* a, const char* b, const char* c)
{
	FrmCustomAlert(ErrorAlert, a, b ? b : "", c ? c : "");
}

/* Debug log: appends lines to /sidlog.txt on the card (in the simulator that is
   <simulator>\Card1\sidlog.txt).  Each line is written and closed at once, so it survives a crash. */
#define DEBUG_LOG 0
/* Simulator test switch: 0 = no sound in the simulator (everything else runs) */
#define SIM_SOUND 1
#if DEBUG_LOG
static void dlog(const char* msg, UInt32 v)
{
	Globals* g = G();
	FileRef f;
	char line[80];
	UInt32 n;
	if (!g || !g->haveVolume) return;
	if (VFSFileOpen(g->volRef, "/sidlog.txt", vfsModeReadWrite | vfsModeCreate, &f) != errNone) return;
	VFSFileSeek(f, vfsOriginEnd, 0);
	StrPrintF(line, "%s %lx\r\n", msg, v);
	VFSFileWrite(f, StrLen(line), line, &n);
	VFSFileClose(f);
}
#else
#define dlog(m, v)
#endif

static void copyText(char* dst, const char* src, UInt16 max)
{
	UInt16 i = 0;
	if (src)
		for (; src[i] && i < max - 1; i++) dst[i] = src[i];
	dst[i] = 0;
}

static void loadPrefs(Globals* g)
{
	SidPrefs p;
	UInt16 size = sizeof p;
	g->prefs.version = PREFS_VERSION;
	g->prefs.chip6581 = 0;                 /* 8580 waveforms */
	g->prefs.filter = 1;
	g->prefs.clock = 0;
	g->prefs.volume = VOL_DEFAULT;
	g->prefs.pad = 0;
	if (PrefGetAppPreferences(appCreator, 0, &p, &size, true) == noPreferenceFound)
		return;
	if (size == sizeof p && p.version == PREFS_VERSION)
		g->prefs = p;
	else if (size == sizeof p && p.version == 2) {  /* the test build's: tens of per cent */
		g->prefs = p;
		g->prefs.version = PREFS_VERSION;
		g->prefs.volume = p.volume * 2 > VOL_MAX ? VOL_MAX : p.volume * 2;
	} else if (size == 4 && p.version == 1) {     /* v1.0's: the same, no volume */
		g->prefs.chip6581 = p.chip6581;
		g->prefs.filter = p.filter;
		g->prefs.clock = p.clock;
	}
	if (g->prefs.volume > VOL_MAX)
		g->prefs.volume = VOL_DEFAULT;
}

static void savePrefs(Globals* g)
{
	PrefSetAppPreferences(appCreator, 0, PREFS_VERSION, &g->prefs, sizeof g->prefs, true);
}

/* --------------------------------------------------------------- the core */

/* Send one command to the core.  The SidCoreCall block is read by native code, so every field is
   stored little-endian.  a[] (6 words) is in/out; str (3 pointers) is out, may be NULL. */
static UInt32 coreCall(Globals* g, UInt32 cmd, UInt32* a, char** str)
{
	/* The block must be 4-byte aligned: the core reads it with word loads, and the Tungsten E2 faults
	   on unaligned ones ("Fatal Exception").  This compiler only aligns the 68k stack to 2 bytes
	   (-mno-align-int), so a plain local was aligned or not depending on the caller (2026-11-13). */
	UInt32 raw[sizeof(SidCoreCall) / 4 + 1];
	SidCoreCall* c = (SidCoreCall*)(((UInt32)raw + 3) & ~3UL);
	UInt16 i;
	Boolean log = cmd != SC_FILL || g->fillLogs < 1;
	if (cmd == SC_FILL) g->fillLogs++;
	if (log) { dlog("core call", cmd); dlog("  a0", a[0]); dlog("  a1", a[1]); }
	MemSet(c, sizeof *c, 0);
	c->cmd = swap32(cmd);
	for (i = 0; i < 6; i++) c->a[i] = swap32(a[i]);
	if (g->simulator)
		PceNativeCall((NativeFuncType*)"sidcore.dll\0sidcoreEntry", c);
	else
		PceNativeCall((NativeFuncType*)g->coreEntry, c);
	for (i = 0; i < 6; i++) a[i] = swap32(c->a[i]);
	if (str)
		for (i = 0; i < 3; i++) str[i] = (char*)swap32((UInt32)c->str[i]);
	if (log) {
		dlog("  result", swap32(c->result));
		dlog("  out a0", a[0]);
		if (str) { dlog("  str0", (UInt32)str[0]); dlog("  str1", (UInt32)str[1]); dlog("  str2", (UInt32)str[2]); }
	}
	return swap32(c->result);
}

/* Read the packed core (build/blob/sidcore.bin) from 'armc' resources 1, 2, ... as one byte stream. */
typedef struct { UInt16 id; MemHandle h; const UInt8* p; UInt32 left; } ResReader;

static Boolean resRead(ResReader* r, UInt8* dst, UInt32 n)
{
	while (n) {
		UInt32 k;
		if (!r->left) {
			if (r->h) { MemHandleUnlock(r->h); DmReleaseResource(r->h); }
			r->h = DmGetResource('armc', ++r->id);
			if (!r->h) return false;
			r->p = (const UInt8*)MemHandleLock(r->h);
			r->left = MemHandleSize(r->h);
		}
		k = n < r->left ? n : r->left;
		if (dst) { MemMove(dst, r->p, k); dst += k; }
		r->p += k; r->left -= k; n -= k;
	}
	return true;
}

static void resDone(ResReader* r)
{
	if (r->h) { MemHandleUnlock(r->h); DmReleaseResource(r->h); r->h = 0; }
}

/* Copy the core into memory and add its base address to every listed word. */
static Err loadArmCore(Globals* g)
{
	ResReader r;
	UInt8 hdr[24], w[4];
	UInt32 imageSize, bssSize, entryOffset, relocCount, i;

	MemSet(&r, sizeof r, 0);
	if (!resRead(&r, hdr, 24) || readLE32(hdr) != SIDCORE_MAGIC || readLE32(hdr + 4) != SIDCORE_VERSION) {
		resDone(&r);
		return 1;
	}
	imageSize = readLE32(hdr + 8);
	bssSize = readLE32(hdr + 12);
	entryOffset = readLE32(hdr + 16);
	relocCount = readLE32(hdr + 20);
	/* The core is linked for an 8-byte aligned address (its stacks and 64-bit values need it: strd
	   faults on a 4-byte boundary), but MemChunkNew only promises 4.  Allocate 8 more and round up.
	   (rePalm crashed on playback with "FAULT: DABT" when its block came 4 bytes off, 2026-11-12.) */
	g->coreChunk = (UInt8*)MemChunkNew(0, imageSize + bssSize + 8, chunkFlagsLarge);
	if (!g->coreChunk) { resDone(&r); return memErrNotEnoughSpace; }
	g->coreBase = (UInt8*)(((UInt32)g->coreChunk + 7) & ~7UL);
	if (!resRead(&r, g->coreBase, imageSize)) { resDone(&r); return 1; }
	MemSet(g->coreBase + imageSize, bssSize, 0);
	for (i = 0; i < relocCount; i++) {
		UInt8* p;
		if (!resRead(&r, w, 4)) { resDone(&r); return 1; }
		p = g->coreBase + readLE32(w);
		writeLE32(p, readLE32(p) + (UInt32)g->coreBase);
	}
	resDone(&r);
	g->coreEntry = (UInt32)g->coreBase + entryOffset;
	return errNone;
}

/* Simulator only.  Its emulator cannot make native calls from the Sound Manager's thread, so the
   main thread keeps a ring of samples filled from the core (simFillRing, while idle) and the
   stream callback only copies bytes out of it: no system calls on the sound thread. */
static void simFillRing(Globals* g)
{
	/* ringRead/ringWrite are byte positions in the ring, wrapping at RING_BYTES; read == write means
	   empty, so the writer always leaves 2 bytes free.  Only the callback moves ringRead. */
	UInt32 a[6];
	for (;;) {
		UInt32 r = g->ringRead, w = g->ringWrite;
		UInt32 used = (w >= r) ? w - r : RING_BYTES - r + w;
		UInt32 n = RING_BYTES - 2 - used;
		if (n > RING_BYTES - w) n = RING_BYTES - w;      /* up to the end of the ring */
		if (n > STREAM_FRAMES * 2) n = STREAM_FRAMES * 2;
		/* skip tiny refills, except the last piece before the end (else the ring could never wrap) */
		if (n == 0 || (n < 512 && n < RING_BYTES - w)) return;
		MemSet(a, sizeof a, 0);
		a[0] = (UInt32)(g->ring + w);
		a[1] = n;
		coreCall(g, SC_FILL, a, 0);
		w += n;
		if (w == RING_BYTES) w = 0;
		g->ringWrite = w;
	}
}

static Err simStreamCallback(void* userData, SndStreamRef channel, void* buffer, UInt32 frames)
{
	Globals* g = (Globals*)userData;
	UInt8* out = (UInt8*)buffer;
	UInt32 want = frames * 2, i, r, w;
	if (!g || !out) return errNone;
	g->callbackCount++;
	/* positions wrap at RING_BYTES by comparison: no division in this hot loop (the 68000 has none) */
	r = g->ringRead;
	w = g->ringWrite;
	for (i = 0; i < want; i++) {
		if (r == w) { out[i] = 0; continue; }   /* ran dry: silence */
		out[i] = g->ring[r];
		if (++r == RING_BYTES) r = 0;
	}
	g->ringRead = r;
	return errNone;
}

/* On the device: hold off auto-off while a song plays (Palm OS would otherwise switch off a couple
   of minutes after the last tap, mid-song).  Called from every event loop; resets every 5 s. */
static void keepAwake(Globals* g)
{
	UInt32 now = TimGetTicks();
	if (g->playing && now - g->awakeTicks >= 5 * SysTicksPerSecond()) {
		EvtResetAutoOffTimer();
		g->awakeTicks = now;
	}
}

/* After the core's code has been copied and relocated: push any of it still held in the XScale's data
   cache (32 KB, write-back) out to memory, by reading through 64 KB of other memory, so the processor
   fetches the finished instructions.  (Palm OS has no public call for this; the instruction cache is
   not touched, but it can only hold stale lines for these addresses if other code ran there recently.) */
static void cleanDataCache(const UInt8* other)
{
	volatile const UInt8* p = other;
	UInt32 i;
	UInt8 x = 0;
	for (i = 0; i < 65536UL; i += 32) x ^= p[i];
	(void)x;
}

/* The volume to the core (it can change while the stream runs): in steps of 5%. */
static void setVolume(Globals* g, UInt8 vol)
{
	UInt32 a[6] = { 0 };
	a[0] = (UInt32)vol * 256 / 20;
	coreCall(g, SC_VOLUME, a, 0);
}

/* Send the settings to the core (the stream must be stopped). */
static UInt32 configCore(Globals* g)
{
	UInt32 a[6] = { 0 }, r;
	a[0] = SAMPLE_RATE;
	a[1] = g->prefs.chip6581 ? 1 : 0;
	a[2] = g->prefs.filter ? 1 : 0;
	a[3] = g->prefs.clock;
	r = coreCall(g, SC_CONFIG, a, 0);
	setVolume(g, g->prefs.volume);
	return r;
}

static Boolean startCore(Globals* g)
{
	UInt32 a[6] = { 0 };
	UInt32 cpu = 0, r;
	Err err;

	FtrGet(sysFtrCreator, sysFtrNumProcessorID, &cpu);
	g->simulator = (cpu & sysFtrNumProcessorMask) == sysFtrNumProcessorx86;
	if (!g->simulator && !sysFtrNumProcessorIsARM(cpu)) {
		showError("This player needs a Palm with an ARM processor (Palm OS 5).", 0, 0);
		return false;
	}
	if (!g->simulator && (err = loadArmCore(g)) != errNone) {
		showError(err == memErrNotEnoughSpace ? "Not enough memory for the sound core." :
		           "The sound core is missing or damaged.", 0, 0);
		return false;
	}
	{
		UInt32 freeB = 0, maxB = 0;
		MemHeapFreeBytes(0, &freeB, &maxB);
		dlog("heap free before arena", freeB);
		dlog("  largest free", maxB);
	}
	g->arena = (UInt8*)MemChunkNew(0, ARENA_SIZE, chunkFlagsLarge);
	{
		UInt32 freeB = 0, maxB = 0;
		MemHeapFreeBytes(0, &freeB, &maxB);
		dlog("arena", (UInt32)g->arena);
		dlog("  size", g->arena ? MemPtrSize(g->arena) : 0);
		dlog("  heap free after", freeB);
	}
	if (!g->arena) { showError("Not enough memory for the player.", 0, 0); return false; }
	if (!g->simulator) cleanDataCache(g->arena);       /* after loading the core (see there) */
	if (MemPtrSize(g->arena) < ARENA_SIZE) {
		char num[12];
		StrIToA(num, (Int32)MemPtrSize(g->arena));
		showError("The player's memory block is too small: ", num, " bytes");
		MemPtrFree(g->arena); g->arena = 0;
		return false;
	}

	a[0] = (UInt32)g->arena; a[1] = ARENA_SIZE; a[2] = TimGetSeconds();
	if (coreCall(g, SC_INIT, a, 0) != 0) { showError("The sound core did not start.", 0, 0); return false; }
	g->streamFunc = a[0];
	g->coreReady = true;

	MemSet(a, sizeof a, 0);
	if ((r = configCore(g)) != 0) {
		char num[12];
		StrIToA(num, (Int32)r);
		showError("The sound core refused its settings: error ", num, 0);
		return false;
	}

	/* On the device the core feeds the stream directly (native callback).  The simulator rejects
	   native stream callbacks, so there a 68k callback asks the core for each buffer (SC_FILL). */
	if (g->simulator) {
		g->ring = (UInt8*)MemChunkNew(0, RING_BYTES, chunkFlagsLarge);
		if (!g->ring) { showError("Not enough memory for the sound buffer.", 0, 0); return false; }
		dlog("ring", (UInt32)g->ring);
		dlog("  ring size", MemPtrSize(g->ring));
		dlog("  arena size", MemPtrSize(g->arena));
	}
	if (g->simulator)
		err = SndStreamCreate(&g->stream, sndOutput, SAMPLE_RATE, sndInt16Little, sndMono,
		                      simStreamCallback, g, STREAM_FRAMES, false);
	else
		err = SndStreamCreate(&g->stream, sndOutput, SAMPLE_RATE, sndInt16Little, sndMono,
		                      (SndStreamBufferCallback)g->streamFunc, 0, STREAM_FRAMES, true);
	if (err) { showError("Could not open the sound output.", 0, 0); return false; }
	g->streamOpen = true;
	return true;
}

static void stopCore(Globals* g)
{
	UInt32 a[6] = { 0 };
	if (g->streamOpen) { SndStreamStop(g->stream); SndStreamDelete(g->stream); g->streamOpen = false; }
	if (g->coreReady) { coreCall(g, SC_SHUTDOWN, a, 0); g->coreReady = false; }
	if (g->ring) { MemPtrFree(g->ring); g->ring = 0; }
	if (g->arena) { MemPtrFree(g->arena); g->arena = 0; }
	if (g->coreChunk) { MemPtrFree(g->coreChunk); g->coreChunk = 0; g->coreBase = 0; }
}

/* --------------------------------------------------------------- files */

static Boolean isSidName(const char* s)
{
	UInt16 n = StrLen(s);
	return n > 4 && StrCaselessCompare(s + n - 4, ".sid") == 0;
}

static void scanFiles(Globals* g)
{
	UInt32 it = vfsIteratorStart, di;
	FileRef dir;
	FileInfoType info;
	char buf[256];
	UInt16 i, j;

	g->nFiles = 0;
	g->haveVolume = false;
	/* The first volume with a /SID folder: a handheld can have more than one (some keep a built-in
	   card or internal drive in the first slot, like the deNVFS Tungsten E2 ROM).  Without one, the
	   first volume (for the debug log). */
	dir = 0;
	while (it != vfsIteratorStop) {
		UInt16 vol;
		if (VFSVolumeEnumerate(&vol, &it) != errNone) break;
		if (!g->haveVolume) { g->volRef = vol; g->haveVolume = true; }
		if (VFSFileOpen(vol, "/SID", vfsModeRead, &dir) == errNone) { g->volRef = vol; break; }
		dir = 0;
	}
	if (!dir) return;
	di = vfsIteratorStart;
	info.nameP = buf;
	info.nameBufLen = sizeof buf;
	while (di != vfsIteratorStop && g->nFiles < MAX_FILES) {
		if (VFSDirEntryEnumerate(dir, &di, &info) != errNone) break;
		if ((info.attributes & vfsFileAttrDirectory) || !isSidName(buf) || StrLen(buf) >= NAME_LEN) continue;
		StrCopy(g->names[g->nFiles++], buf);
	}
	VFSFileClose(dir);
	/* sort by name (insertion sort; at most 256 names) */
	for (i = 1; i < g->nFiles; i++) {
		char tmp[NAME_LEN];
		StrCopy(tmp, g->names[i]);
		for (j = i; j > 0 && StrCaselessCompare(g->names[j - 1], tmp) > 0; j--)
			StrCopy(g->names[j], g->names[j - 1]);
		StrCopy(g->names[j], tmp);
	}
	for (i = 0; i < g->nFiles; i++) {
		char path[NAME_LEN + 8];
		FileRef f;
		UInt32 size = 0;
		StrCopy(path, SID_DIR);
		StrCat(path, g->names[i]);
		if (VFSFileOpen(g->volRef, path, vfsModeRead, &f) == errNone) {
			VFSFileSize(f, &size);
			VFSFileClose(f);
		}
		g->blocks[i] = (UInt16)((size + 253) / 254);   /* file size in 1541 blocks, for the list */
	}
}

/* --------------------------------------------------------------- playback */

static void pausePlay(Globals* g)
{
	if (g->playing) {
		if (!g->simulator || SIM_SOUND) SndStreamStop(g->stream);
		g->playedTicks += TimGetTicks() - g->startTicks;
		g->playing = false;
	}
}

static void resumePlay(Globals* g)
{
	if (!g->playing && g->loaded >= 0 && g->streamOpen) {
		Err e;
		if (g->simulator) simFillRing(g);
		dlog("ring filled", 0);
		dlog("  ring write", g->ringWrite);
		g->startTicks = TimGetTicks();
		dlog("stream start", (UInt32)g->stream);
		e = (g->simulator && !SIM_SOUND) ? 0 : SndStreamStart(g->stream);
		dlog("  stream start err", e);
		(void)e;
		g->playing = true;
		dlog("resumePlay done", 0);
	}
}

static Boolean startSong(Globals* g, UInt16 song)
{
	UInt32 a[6] = { 0 };
	pausePlay(g);
	a[0] = song;
	if (coreCall(g, SC_START, a, 0) != 0) return false;
	g->song = (UInt16)a[0];
	g->playedTicks = 0;
	g->ringRead = g->ringWrite = 0;          /* simulator: drop the old song's samples */
	resumePlay(g);
	dlog("startSong done", 0);
	return true;
}

static Boolean loadTune(Globals* g, Int16 index)
{
	char path[NAME_LEN + 8];
	FileRef f;
	UInt32 len = 0, got = 0;
	UInt8* data;
	UInt32 a[6] = { 0 };
	char* str[3];
	UInt32 r;

	pausePlay(g);
	g->loaded = -1;
	StrCopy(path, SID_DIR);
	StrCat(path, g->names[index]);
	dlog("load tune index", index);
	if (VFSFileOpen(g->volRef, path, vfsModeRead, &f) != errNone) {
		showError("Could not open ", g->names[index], 0);
		return false;
	}
	VFSFileSize(f, &len);
	if (len < 0x7c || len > 0x20000) { VFSFileClose(f); showError(g->names[index], " is not a PSID tune.", 0); return false; }
	data = (UInt8*)MemChunkNew(0, len, chunkFlagsLarge);
	if (!data) { VFSFileClose(f); showError("Not enough memory for ", g->names[index], 0); return false; }
	dlog("  file size", len);
	dlog("  data", (UInt32)data);
	VFSFileRead(f, len, data, &got);
	VFSFileClose(f);
	dlog("  read", got);

	a[0] = (UInt32)data; a[1] = got;
	r = coreCall(g, SC_LOAD, a, str);
	MemPtrFree(data);                     /* the core keeps its own copy */
	if (r != 0) { showError(g->names[index], ": ", r == SC_ERR_MEMORY ? "not enough memory" : "not a playable tune"); return false; }
	g->songs = (UInt16)a[0];
	dlog("  copy strings", 0);
	copyText(g->title, str[0], sizeof g->title);
	copyText(g->author, str[1], sizeof g->author);
	copyText(g->copyright, str[2], sizeof g->copyright);
	g->loaded = index;
	StrCopy(g->loadedName, g->names[index]);
	return startSong(g, 0);
}

/* --------------------------------------------------------------- the C64-style screen
   The whole screen is drawn by the app at the TX's native resolution (320 px wide), in C64 colours
   with an 8x8 C64-style font (font8.h), as in the "hybrid panels" mock-up (tools/mock_c64.py): a light blue
   border, a blue screen, and three boxed panels (now playing, voices, tunes).  Rows are 12 px apart.
   Drawing goes through a 16-bit strip bitmap, STRIP_H rows at a time (a full-screen bitmap would be
   over Palm OS's 64 KB chunk limit), each strip rendering the whole scene clipped to itself. */

#include "font8.h"

#define PANEL_W     320                 /* the drawn screen is always 320 native px wide */
#define STRIP_H     30
#define ROW         12                  /* text rows: 8 px glyphs + 4 px leading */
#define RGB565(r, g, b) ((UInt16)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define C_LIGHTBLUE RGB565(108, 94, 181)
#define C_BLUE      RGB565(53, 40, 121)
#define C_WHITE     RGB565(255, 255, 255)
#define NO_BG       0x0001              /* "transparent" background (never a used colour) */

/* layout, native px from the top of the panel */
#define TITLE_Y     8
#define NOW_Y       24                  /* now playing box: 9 rows */
#define BUTTONS_Y   96
#define VOICES_Y    136                 /* voices box: 6 rows (left out on short screens) */
#define BTN_PLAY_X  24
#define BTN_PREV_X  72
#define BTN_NEXT_X  120

static Coord tunesTop(Globals* g) { return g->scrH >= 330 ? 212 : VOICES_Y; }
static Int16 tunesRows(Globals* g) { return (Int16)((g->scrH - 8 - tunesTop(g)) / ROW); }      /* box rows */
static Int16 tunesLines(Globals* g) { Int16 r = tunesRows(g) - 2; return r < 1 ? 1 : r; }    /* list lines */

static void fillRect(Globals* g, Coord x, Coord y, Coord w, Coord h, UInt16 col)
{
	Coord x1 = x + w, y1 = y + h, yy, xx;
	if (x < 0) x = 0;
	if (x1 > PANEL_W) x1 = PANEL_W;
	if (y < g->stripY) y = g->stripY;
	if (y1 > g->stripY + STRIP_H) y1 = g->stripY + STRIP_H;
	for (yy = y; yy < y1; yy++) {
		UInt16* p = g->stripBits + (yy - g->stripY) * PANEL_W + x;
		for (xx = x; xx < x1; xx++) *p++ = col;
	}
}

/* Draw text in the C64 font: lower case shows as upper case; G_* codes are graphics characters. */
static void drawText(Globals* g, Coord x, Coord y, const char* s, UInt16 fg, UInt16 bg, Boolean reverse)
{
	if (y + 8 <= g->stripY || y >= g->stripY + STRIP_H) return;
	for (; *s; s++, x += 8) {
		UInt8 c = (UInt8)*s;
		const UInt8* gl;
		Coord gy;
		if (x >= PANEL_W) break;
		if (c >= 'a' && c <= 'z') c -= 32;
		if (c >= 128 && c < 128 + sizeof gfx8 / 8) gl = gfx8[c - 128];
		else if (c >= 32 && c < 96) gl = font8[c - 32];
		else gl = font8['?' - 32];
		for (gy = 0; gy < 8; gy++) {
			Coord py = y + gy, gx;
			UInt16* p;
			UInt8 bits;
			if (py < g->stripY || py >= g->stripY + STRIP_H) continue;
			p = g->stripBits + (py - g->stripY) * PANEL_W + x;
			bits = gl[gy];
			if (reverse) bits = (UInt8)~bits;
			for (gx = 0; gx < 8; gx++) {
				if (x + gx < 0 || x + gx >= PANEL_W) continue;
				if (bits & (0x80 >> gx)) p[gx] = fg;
				else if (bg != NO_BG) p[gx] = bg;
			}
		}
	}
}

/* Copy at most n characters of s, padded with spaces to width w (s may be dst). */
static void fitText(char* dst, const char* s, Int16 n, Int16 w)
{
	Int16 i = 0;
	for (; s && s[i] && i < n; i++) dst[i] = s[i];
	for (; i < w; i++) dst[i] = ' ';
	dst[i] = 0;
}

/* A button: light blue with the label in blue, from 1 px above the text to 1 px below it (the
   font's glyphs sit at the top of their 8x8 cells, so plain reverse video looked top-heavy). */
static void drawButton(Globals* g, Coord x, Coord y, const char* label)
{
	fillRect(g, x, y - 1, (Coord)StrLen(label) * 8, 10, C_LIGHTBLUE);
	drawText(g, x, y, label, C_BLUE, NO_BG, false);
}

/* A box frame: rows*ROW tall, cols characters wide, with a title set into the top line. */
static void drawBox(Globals* g, Coord x, Coord y, Int16 cols, Int16 rows, const char* title)
{
	char line[41];
	Int16 i;
	line[0] = G_TOPLEFT;
	for (i = 1; i < cols - 1; i++) line[i] = G_HLINE;
	line[cols - 1] = G_TOPRIGHT;
	line[cols] = 0;
	drawText(g, x, y, line, C_LIGHTBLUE, NO_BG, false);
	line[0] = G_BOTLEFT;
	line[cols - 1] = G_BOTRIGHT;
	drawText(g, x, y + (rows - 1) * ROW, line, C_LIGHTBLUE, NO_BG, false);
	fillRect(g, x + 3, y + 8, 2, (rows - 1) * ROW - 8, C_LIGHTBLUE);
	fillRect(g, x + (cols - 1) * 8 + 3, y + 8, 2, (rows - 1) * ROW - 8, C_LIGHTBLUE);
	if (title) {
		char t[40];
		t[0] = ' ';
		StrCopy(t + 1, title);
		StrCat(t, " ");
		drawText(g, x + 16, y, t, C_WHITE, C_BLUE, false);
	}
}

/* The list shows file names without ".sid", with '_' as spaces. */
static void displayName(char* dst, const char* file, Int16 max)
{
	Int16 i, n = (Int16)StrLen(file);
	if (n > 4) n -= 4;
	if (n > max) n = max;
	for (i = 0; i < n; i++) dst[i] = file[i] == '_' ? ' ' : file[i];
	dst[n] = 0;
}

static void playedTime(Globals* g, char* out)
{
	UInt32 secs = (g->playedTicks + (g->playing ? TimGetTicks() - g->startTicks : 0)) / SysTicksPerSecond();
	char num[8];
	g->shownSecs = secs;
	out[0] = 0;
	if (secs / 60 < 10) StrCat(out, "0");
	StrIToA(num, secs / 60); StrCat(out, num);
	StrCat(out, ":");
	if (secs % 60 < 10) StrCat(out, "0");
	StrIToA(num, secs % 60); StrCat(out, num);
}

/* Render the whole scene into the current strip (everything is clipped to it). */
static void drawScene(Globals* g)
{
	char line[48], num[8];
	Int16 i, top = tunesTop(g), rows = tunesRows(g), lines = tunesLines(g);

	fillRect(g, 0, g->stripY, PANEL_W, STRIP_H, C_LIGHTBLUE);              /* the border */
	fillRect(g, 8, 8, PANEL_W - 16, g->scrH - 16, C_BLUE);                 /* the screen */

	StrCopy(line, " *** SID PLAYER DX V1.01 *** ");
	StrIToA(num, g->nFiles);
	StrCat(line, num);
	StrCat(line, g->nFiles == 1 ? " TUNE" : " TUNES");
	fitText(line, line, 38, 38);
	drawText(g, 8, TITLE_Y, line, C_LIGHTBLUE, C_BLUE, true);

	/* now playing */
	drawBox(g, 8, NOW_Y, 38, 9, "NOW PLAYING");
	if (g->loaded < 0) {
		drawText(g, 24, NOW_Y + 12, g->nFiles ? "TAP A TUNE BELOW TO PLAY IT" : "NO TUNES", C_WHITE, NO_BG, false);
		if (!g->nFiles) {
			drawText(g, 24, NOW_Y + 30, "PUT .SID FILES IN THE SID", C_LIGHTBLUE, NO_BG, false);
			drawText(g, 24, NOW_Y + 42, "FOLDER ON THE CARD", C_LIGHTBLUE, NO_BG, false);
		}
	} else {
		fitText(line, g->title, 34, 0);  drawText(g, 24, NOW_Y + 12, line, C_WHITE, NO_BG, false);
		fitText(line, g->author, 34, 0); drawText(g, 24, NOW_Y + 24, line, C_LIGHTBLUE, NO_BG, false);
		fitText(line, g->copyright, 34, 0); drawText(g, 24, NOW_Y + 36, line, C_LIGHTBLUE, NO_BG, false);
		StrCopy(line, "SONG ");
		StrIToA(num, g->song); StrCat(line, num);
		StrCat(line, "/");
		StrIToA(num, g->songs); StrCat(line, num);
		drawText(g, 24, NOW_Y + 54, line, C_LIGHTBLUE, NO_BG, false);
		playedTime(g, line);
		drawText(g, 232, NOW_Y + 54, line, C_WHITE, NO_BG, false);
	}
	line[0] = ' '; line[1] = g->playing ? G_PAUSE : G_PLAY; line[2] = ' '; line[3] = 0;   /* pause resumes */
	drawButton(g, BTN_PLAY_X, BUTTONS_Y, line);
	drawButton(g, BTN_PREV_X, BUTTONS_Y, " << ");
	drawButton(g, BTN_NEXT_X, BUTTONS_Y, " >> ");

	/* voices: level bars (envelope x master volume), 32 cells */
	if (top > VOICES_Y) {
		drawBox(g, 8, VOICES_Y, 38, 6, "VOICES");
		for (i = 0; i < 3; i++) {
			Int16 n = (Int16)(g->levels[i] >> 3), k;          /* 0..31 */
			Coord y = VOICES_Y + 12 + i * ROW;
			num[0] = (char)('1' + i); num[1] = 0;
			drawText(g, 24, y, num, C_WHITE, NO_BG, false);
			for (k = 0; k < 32; k++) line[k] = k < n ? G_FULL : G_DIM;
			line[32] = 0;
			drawText(g, 40, y, line, C_LIGHTBLUE, NO_BG, false);
		}
	}

	/* the tunes on the card */
	drawBox(g, 8, top, 38, rows, "TUNES ON CARD");
	if (g->topIndex > 0) { line[0] = G_UP; line[1] = 0; drawText(g, 296, top, line, C_WHITE, C_BLUE, false); }
	if (g->topIndex + lines < g->nFiles) {
		line[0] = G_DOWN; line[1] = 0;
		drawText(g, 296, top + (rows - 1) * ROW, line, C_WHITE, C_BLUE, false);
	}
	for (i = 0; i < lines && g->topIndex + i < g->nFiles; i++) {
		Int16 idx = g->topIndex + i;
		char name[40];
		StrIToA(num, g->blocks[idx]);
		fitText(line, num, 3, 4);
		displayName(name, g->names[idx], 29);
		StrCat(line, name);
		if (idx == g->cursor) {
			fitText(line, line, 34, 34);
			drawText(g, 24, top + 12 + i * ROW, line, C_LIGHTBLUE, C_BLUE, true);
		} else
			drawText(g, 24, top + 12 + i * ROW, line, idx == g->loaded ? C_WHITE : C_LIGHTBLUE, NO_BG, false);
	}
}

/* Redraw native rows y0..y1 of the panel (the whole scene is clipped to each strip). */
static void redrawRows(Globals* g, Coord y0, Coord y1)
{
	Coord y;
	if (!g->stripBits) return;
	if (y0 < 0) y0 = 0;
	if (y1 > g->scrH) y1 = g->scrH;
	for (y = y0 & ~1; y < y1; y += STRIP_H) {
		g->stripY = y;
		drawScene(g);
		WinDrawBitmap((BitmapType*)g->stripV3, g->panelX / 2, y / 2);
	}
}

static void redrawAll(Globals* g)
{
	/* on screens wider than the panel (landscape), the sides are border colour */
	if (g->scrW > PANEL_W) {
		RGBColorType rgb = { 0, 108, 94, 181 }, old;
		RectangleType r;
		WinSetForeColorRGB(&rgb, &old);
		r.topLeft.x = 0; r.topLeft.y = 0; r.extent.x = g->panelX / 2; r.extent.y = g->scrH / 2;
		WinDrawRectangle(&r, 0);
		r.topLeft.x = (g->panelX + PANEL_W) / 2; r.extent.x = (g->scrW - g->panelX - PANEL_W) / 2 + 1;
		WinDrawRectangle(&r, 0);
		WinSetForeColorRGB(&old, 0);
	}
	redrawRows(g, 0, g->scrH);
}

static void redrawNowPlaying(Globals* g) { redrawRows(g, NOW_Y, NOW_Y + 9 * ROW); }
static void redrawTunes(Globals* g) { redrawRows(g, tunesTop(g), g->scrH); }

/* The window's size in native pixels (the form fills the screen; taller with the input area closed). */
static void measureScreen(Globals* g, FormType* frm)
{
	RectangleType r;
	WinGetBounds(FrmGetWindowHandle(frm), &r);
	g->scrW = r.extent.x * 2;
	g->scrH = r.extent.y * 2;
	g->panelX = g->scrW > PANEL_W ? ((g->scrW - PANEL_W) / 2) & ~1 : 0;
}

static Boolean startScreen(Globals* g)
{
	UInt32 depth = 16;
	Err err;
	WinScreenMode(winScreenModeSet, 0, 0, &depth, 0);         /* C64 colours need 16-bit colour */
	g->stripBmp = BmpCreate(PANEL_W, STRIP_H, 16, 0, &err);
	if (!g->stripBmp) return false;
	g->stripBits = (UInt16*)BmpGetBits(g->stripBmp);
	g->stripV3 = BmpCreateBitmapV3(g->stripBmp, kDensityDouble, g->stripBits, 0);
	return g->stripV3 != 0;
}

static void stopScreen(Globals* g)
{
	if (g->stripV3) { BmpDelete((BitmapType*)g->stripV3); g->stripV3 = 0; }
	if (g->stripBmp) { BmpDelete(g->stripBmp); g->stripBmp = 0; }
	g->stripBits = 0;
	WinScreenMode(winScreenModeSetToDefaults, 0, 0, 0, 0);
}

/* Keep the cursor in view. */
static void scrollToCursor(Globals* g)
{
	Int16 lines = tunesLines(g);
	if (g->cursor < g->topIndex) g->topIndex = g->cursor;
	if (g->cursor >= g->topIndex + lines) g->topIndex = g->cursor - lines + 1;
	if (g->topIndex > g->nFiles - lines) g->topIndex = g->nFiles - lines;
	if (g->topIndex < 0) g->topIndex = 0;
}

/* The meters, like a C64 music player's: a voice's bar kicks up to full when it starts a new note
   (or jumps in pitch), and otherwise falls back by METER_FALL per reading towards the voice's real
   loudness (its peak output, scaled to METER_REST of full so the kicks stand out).  Readings come
   10 times a second. */
#define METER_FALL 40
#define METER_REST 170            /* out of 256 */
static void readLevels(Globals* g)
{
	UInt32 a[6] = { 0 };
	UInt16 i;
	if (coreCall(g, SC_LEVELS, a, 0) != 0) return;
	for (i = 0; i < 3; i++) {
		UInt16 rest = (UInt16)((a[i] * METER_REST) >> 8);
		UInt16 fallen = g->levels[i] > METER_FALL ? g->levels[i] - METER_FALL : 0;
		UInt16 v = a[3 + i] ? 255 : (rest > fallen ? rest : fallen);
		g->levels[i] = (UInt8)v;
	}
}

static void chooseTune(Globals* g, Int16 index)
{
	if (index < 0 || index >= g->nFiles) return;
	g->cursor = index;
	scrollToCursor(g);
	loadTune(g, index);
	redrawNowPlaying(g);
	redrawTunes(g);
}

static void changeSong(Globals* g, Int16 delta)
{
	Int16 s;
	if (g->loaded < 0 || g->songs < 2) return;
	s = (Int16)g->song + delta;
	if (s < 1) s = g->songs;
	if (s > g->songs) s = 1;
	startSong(g, (UInt16)s);
	redrawNowPlaying(g);
}

static void togglePlay(Globals* g)
{
	if (g->loaded < 0) { chooseTune(g, g->cursor); return; }
	if (g->playing) {
		pausePlay(g);
		g->levels[0] = g->levels[1] = g->levels[2] = 0;          /* paused: the meters drop */
		if (tunesTop(g) > VOICES_Y) redrawRows(g, VOICES_Y + 12, VOICES_Y + 12 + 3 * ROW);
	} else
		resumePlay(g);
	redrawNowPlaying(g);
}

static void moveCursor(Globals* g, Int16 delta)
{
	if (!g->nFiles) return;
	g->cursor += delta;
	if (g->cursor < 0) g->cursor = 0;
	if (g->cursor >= g->nFiles) g->cursor = g->nFiles - 1;
	scrollToCursor(g);
	redrawTunes(g);
}

static void pageList(Globals* g, Int16 dir)
{
	Int16 lines = tunesLines(g);
	g->topIndex += dir * lines;
	if (g->topIndex > g->nFiles - lines) g->topIndex = g->nFiles - lines;
	if (g->topIndex < 0) g->topIndex = 0;
	if (g->cursor < g->topIndex) g->cursor = g->topIndex;
	if (g->cursor >= g->topIndex + lines) g->cursor = g->topIndex + lines - 1;
	if (g->cursor >= g->nFiles) g->cursor = g->nFiles - 1;
	redrawTunes(g);
}

/* A tap, in native pixels relative to the panel. */
static void handleTap(Globals* g, Coord x, Coord y)
{
	Coord top = tunesTop(g);
	Int16 rows = tunesRows(g), lines = tunesLines(g);
	if (y >= BUTTONS_Y - 4 && y < BUTTONS_Y + 12) {
		if (x >= BTN_PLAY_X && x < BTN_PLAY_X + 24) togglePlay(g);
		else if (x >= BTN_PREV_X && x < BTN_PREV_X + 32) changeSong(g, -1);
		else if (x >= BTN_NEXT_X && x < BTN_NEXT_X + 32) changeSong(g, 1);
	} else if (y >= top && y < top + 10) {
		pageList(g, -1);                                      /* the box's top edge: page up */
	} else if (y >= top + (rows - 1) * ROW - 2 && y < top + rows * ROW) {
		pageList(g, 1);                                       /* the bottom edge: page down */
	} else if (y >= top + 10 && y < top + 12 + lines * ROW) {
		Int16 i = (Int16)((y - top - 10) / ROW);
		if (g->topIndex + i < g->nFiles) chooseTune(g, g->topIndex + i);
	}
}

/* The About box, with its own event loop (not FrmDoDialog/FrmAlert, which run the system's loop):
   in the simulator the sound buffer is filled from the app's loop, so this one keeps doing it. */
static void showAbout(Globals* g)
{
	FormType* prev = FrmGetActiveForm();
	FormType* frm = FrmInitForm(AboutForm);
	EventType e;
	Boolean done = false;

	FrmSetActiveForm(frm);
	FrmDrawForm(frm);
	while (!done) {
		if (g->playing && g->simulator) simFillRing(g);
		keepAwake(g);
		EvtGetEvent(&e, !g->playing ? evtWaitForever :
		                g->simulator ? SysTicksPerSecond() / 20 : SysTicksPerSecond());
		if (SysHandleEvent(&e)) continue;
		if (e.eType == ctlSelectEvent && e.data.ctlSelect.controlID == AboutOKButton)
			done = true;
		else if (e.eType == appStopEvent) {
			EvtAddEventToQueue(&e);          /* leave the app: let the main loop see it too */
			done = true;
		} else
			FrmHandleEvent(frm, &e);
	}
	FrmEraseForm(frm);
	FrmDeleteForm(frm);
	FrmSetActiveForm(prev);
}

static Boolean hasPins(void)
{
	UInt32 v;
	return FtrGet(pinCreator, pinFtrAPIVersion, &v) == errNone;
}

/* Apply changed settings: the current song restarts, because the engine resets the SID's clock to
   its default when the chip or filter changes (an NTSC tune would change speed mid-song). */
static void applySettings(Globals* g)
{
	UInt32 r;
	pausePlay(g);
	if ((r = configCore(g)) != 0) {
		char num[12];
		StrIToA(num, (Int32)r);
		showError("The sound core refused its settings: error ", num, 0);
	}
	if (g->loaded >= 0) startSong(g, g->song);
}

static UInt16 groupChoice(FormType* frm, UInt8 group)
{
	UInt16 idx = FrmGetControlGroupSelection(frm, group);
	return idx == frmNoSelectedControl ? 0 : FrmGetObjectId(frm, idx);
}

/* The volume's label: "100%" */
static void volumeLabel(FormType* frm, UInt8 vol, Boolean draw)
{
	char t[8];
	UInt16 n;
	StrIToA(t, (Int32)vol * 5);
	n = StrLen(t);
	t[n] = '%';
	t[n + 1] = 0;
	if (draw) FrmHideObject(frm, FrmGetObjectIndex(frm, SetVolumeLabel));
	FrmCopyLabel(frm, SetVolumeLabel, t);
	if (draw) FrmShowObject(frm, FrmGetObjectIndex(frm, SetVolumeLabel));
}

/* The Settings box, with its own loop like the About box.  Returns true if the settings changed
   (they are then saved and applied).  The volume changes as the slider moves (Cancel puts it back);
   on its own it's saved without restarting the song. */
static Boolean showSettings(Globals* g)
{
	FormType* prev = FrmGetActiveForm();
	FormType* frm = FrmInitForm(SettingsForm);
	ControlType* filter = (ControlType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, SetFilterCheck));
	ControlType* slider = (ControlType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, SetVolumeSlider));
	SidPrefs p = g->prefs;
	EventType e;
	UInt16 pressed = 0, id;
	UInt8 vol = p.volume;

	FrmSetControlGroupSelection(frm, 1, p.chip6581 ? SetModel6581 : SetModel8580);
	FrmSetControlGroupSelection(frm, 2, p.clock == 1 ? SetClockPAL : p.clock == 2 ? SetClockNTSC : SetClockAuto);
	CtlSetValue(filter, p.filter);
	CtlSetValue(slider, vol);
	volumeLabel(frm, vol, false);
	FrmSetActiveForm(frm);
	FrmDrawForm(frm);
	while (!pressed) {
		if (g->playing && g->simulator) simFillRing(g);
		keepAwake(g);
		EvtGetEvent(&e, !g->playing ? evtWaitForever :
		                g->simulator ? SysTicksPerSecond() / 20 : SysTicksPerSecond());
		if (SysHandleEvent(&e)) continue;
		if (e.eType == ctlSelectEvent &&
		    (e.data.ctlSelect.controlID == SetOKButton || e.data.ctlSelect.controlID == SetCancelButton))
			pressed = e.data.ctlSelect.controlID;
		else if ((e.eType == ctlRepeatEvent && e.data.ctlRepeat.controlID == SetVolumeSlider) ||
		         (e.eType == ctlSelectEvent && e.data.ctlSelect.controlID == SetVolumeSlider)) {
			FrmHandleEvent(frm, &e);             /* (the slider moves, then its value is read) */
			if ((UInt8)CtlGetValue(slider) != vol) {
				vol = (UInt8)CtlGetValue(slider);
				volumeLabel(frm, vol, true);
				if (g->coreReady) setVolume(g, vol);
			}
		}
		else if (e.eType == appStopEvent) {
			EvtAddEventToQueue(&e);              /* leave the app: let the main loop see it too */
			pressed = SetCancelButton;
		} else
			FrmHandleEvent(frm, &e);
	}
	if (pressed == SetOKButton) {
		p.chip6581 = groupChoice(frm, 1) == SetModel6581;
		id = groupChoice(frm, 2);
		p.clock = id == SetClockPAL ? 1 : id == SetClockNTSC ? 2 : 0;
		p.filter = CtlGetValue(filter) ? 1 : 0;
		p.volume = vol;
	}
	FrmEraseForm(frm);
	FrmDeleteForm(frm);
	FrmSetActiveForm(prev);
	if (pressed != SetOKButton) {
		if (vol != g->prefs.volume && g->coreReady)
			setVolume(g, g->prefs.volume);   /* (Cancel: as it was) */
		return false;
	}
	if (p.chip6581 == g->prefs.chip6581 && p.filter == g->prefs.filter && p.clock == g->prefs.clock) {
		if (p.volume != g->prefs.volume) {   /* (only the volume: already playing at it) */
			g->prefs.volume = p.volume;
			savePrefs(g);
		}
		return false;
	}
	g->prefs = p;
	savePrefs(g);
	applySettings(g);
	return true;
}

/* The power button (or auto-off) is putting the device to sleep: pause, so the sound doesn't stop
   behind the app's back.  Play resumes after waking.  The screen is updated from the event loop. */
static Err sleepNotify(SysNotifyParamType* p)
{
	Globals* g = (Globals*)p->userDataP;
	if (g && g->playing) {
		pausePlay(g);
		g->sleepPaused = true;
	}
	return errNone;
}

/* A card went in or out.  When one goes in, tell Palm OS the app has dealt with it, so the system
   doesn't switch to the launcher's card view.  Either way the event loop reads the list again
   (cardChanged; this runs inside the system's call, so it does no more than that). */
static Err cardNotify(SysNotifyParamType* p)
{
	Globals* g = (Globals*)p->userDataP;
	if (p->notifyType == sysNotifyVolumeMountedEvent) p->handled |= vfsHandledUIAppSwitch;
	if (g) g->cardChanged = true;
	return errNone;
}

/* After a card went in or out: read the list again.  A tune that's playing carries on (it's in
   memory) and is found again by its name: at its place in the new list, or LOADED_GONE. */
static void rescanCard(Globals* g)
{
	Int16 i;
	scanFiles(g);
	if (g->loaded < 0) return;
	g->loaded = LOADED_GONE;
	for (i = 0; i < (Int16)g->nFiles; i++)
		if (StrCompare(g->names[i], g->loadedName) == 0) { g->loaded = i; break; }
}

/* The system notifications the app takes while it runs (sleep, cards); all undone on exit. */
static void watchSystem(Boolean on)
{
	UInt16 card;
	LocalID db;
	if (SysCurAppDatabase(&card, &db) != errNone) return;
	if (on) {
		SysNotifyRegister(card, db, sysNotifySleepNotifyEvent, sleepNotify, sysNotifyNormalPriority, G());
		SysNotifyRegister(card, db, sysNotifyVolumeMountedEvent, cardNotify, sysNotifyNormalPriority, G());
		SysNotifyRegister(card, db, sysNotifyVolumeUnmountedEvent, cardNotify, sysNotifyNormalPriority, G());
	} else {
		SysNotifyUnregister(card, db, sysNotifySleepNotifyEvent, sysNotifyNormalPriority);
		SysNotifyUnregister(card, db, sysNotifyVolumeMountedEvent, sysNotifyNormalPriority);
		SysNotifyUnregister(card, db, sysNotifyVolumeUnmountedEvent, sysNotifyNormalPriority);
	}
}

/* Speed test: make 10 s of music (the current song, from its start, with the current settings) as
   fast as possible and report how long it took, i.e. how much of the processor playing takes.
   Playback restarts afterwards (paused if it was paused). */
static void speedTest(Globals* g)
{
	FormType* prev = FrmGetActiveForm();
	FormType* frm;
	UInt8* buf;
	UInt32 a[6], t, pct, hs, i;
	Boolean was = g->playing;
	char l1[96], l2[48], num[12];

	if (g->loaded < 0) {
		FrmCustomAlert(InfoAlert, "Choose a tune first: the test uses the current song.", "", "");
		return;
	}
	buf = (UInt8*)MemPtrNew(8820);                       /* 0.1 s of 16-bit mono */
	if (!buf) { showError("Not enough memory for the speed test.", 0, 0); return; }
	pausePlay(g);
	frm = FrmInitForm(SpeedForm);
	FrmSetActiveForm(frm);
	FrmDrawForm(frm);

	MemSet(a, sizeof a, 0);
	a[0] = g->song;
	coreCall(g, SC_START, a, 0);
	t = TimGetTicks();
	for (i = 0; i < 100; i++) {
		MemSet(a, sizeof a, 0);
		a[0] = (UInt32)buf;
		a[1] = 8820;
		coreCall(g, SC_FILL, a, 0);
	}
	t = TimGetTicks() - t;
	MemPtrFree(buf);
	FrmEraseForm(frm);
	FrmDeleteForm(frm);
	FrmSetActiveForm(prev);

	startSong(g, g->song);
	if (!was) pausePlay(g);

	hs = t * 100 / SysTicksPerSecond();                  /* hundredths of a second */
	pct = t * 10 / SysTicksPerSecond();                  /* of 10 s of playing time */
	StrCopy(l1, "10 s of music took ");
	StrIToA(num, hs / 100); StrCat(l1, num);
	StrCat(l1, hs % 100 < 10 ? ".0" : ".");
	StrIToA(num, hs % 100); StrCat(l1, num);
	StrCat(l1, " s to make: about ");
	StrIToA(num, pct); StrCat(l1, num);
	StrCat(l1, "% of the processor. ");
	StrCopy(l2, pct < 50 ? "Plenty of headroom." : pct < 80 ? "Should play smoothly." :
	            pct < 100 ? "Borderline: it may stutter." : "Too slow to play in real time.");
	FrmCustomAlert(InfoAlert, l1, l2, g->simulator ? " (Simulator: this measures the PC, not a Palm.)" : "");
}

static Boolean mainFormHandleEvent(EventType* e)
{
	Globals* g = G();
	FormType* frm = FrmGetActiveForm();

	switch (e->eType) {
	case frmOpenEvent:
		if (hasPins()) {
			FrmSetDIAPolicyAttr(frm, frmDIAPolicyCustom);
			PINSetInputTriggerState(pinInputTriggerEnabled);
			WinSetConstraintsSize(FrmGetWindowHandle(frm), 160, 225, 225, 160, 160, 240);
			PINSetInputAreaState(pinInputAreaClosed);
		}
		FrmDrawForm(frm);
		measureScreen(g, frm);
		redrawAll(g);
		return true;

	case winDisplayChangedEvent: {
		RectangleType r;
		WinGetBounds(WinGetDisplayWindow(), &r);
		WinSetBounds(FrmGetWindowHandle(frm), &r);
		measureScreen(g, frm);
		scrollToCursor(g);
		redrawAll(g);
		return true;
	}

	case frmUpdateEvent:                                   /* also after pausing for sleep */
		if (!g->playing) g->levels[0] = g->levels[1] = g->levels[2] = 0;
		measureScreen(g, frm);
		if (e->data.frmUpdate.updateCode == CARD_UPDATE_CODE) {     /* a card went in or out */
			rescanCard(g);
			g->cursor = (g->loaded >= 0 && g->loaded < (Int16)g->nFiles) ? g->loaded : 0;
			g->topIndex = 0;
			scrollToCursor(g);
		}
		redrawAll(g);
		return true;

	case penDownEvent:
		handleTap(g, e->screenX * 2 - g->panelX, e->screenY * 2);
		return true;

	case keyDownEvent:
		switch (e->data.keyDown.chr) {
		case vchrRockerUp: moveCursor(g, -1); return true;
		case vchrRockerDown: moveCursor(g, 1); return true;
		/* palmOne's 5-way (the TX's) sends up/down as page up/down with navigator bits in keyCode
		   (palmOneNavigator.h: navBitsAll | navChangeBitsAll): one line.  Real page buttons (older
		   handhelds, no bits): a page at a time. */
		case vchrPageUp: case vchrPageDown: {
			Int16 dir = e->data.keyDown.chr == vchrPageUp ? -1 : 1;
			if (e->data.keyDown.keyCode & (NAV_BITS_ALL | NAV_CHANGE_BITS_ALL)) moveCursor(g, dir);
			else pageList(g, dir);
			return true;
		}
		case vchrRockerLeft: changeSong(g, -1); return true;
		case vchrRockerRight: changeSong(g, 1); return true;
		case vchrRockerCenter: chooseTune(g, g->cursor); return true;
		}
		break;

	case nilEvent:
		if (g->playing) {
			UInt32 now = TimGetTicks();
			if ((g->playedTicks + now - g->startTicks) / SysTicksPerSecond() != g->shownSecs)
				redrawRows(g, NOW_Y + 54, NOW_Y + 62);           /* the time */
			if (tunesTop(g) > VOICES_Y && now - g->meterTicks >= SysTicksPerSecond() / 10) {
				g->meterTicks = now;
				readLevels(g);
				redrawRows(g, VOICES_Y + 12, VOICES_Y + 12 + 3 * ROW);
			}
		}
		return true;

	case menuEvent:
		if (e->data.menu.itemID == MenuAbout) { showAbout(g); redrawAll(g); return true; }
		if (e->data.menu.itemID == MenuSettings) { showSettings(g); redrawAll(g); return true; }
		if (e->data.menu.itemID == MenuSpeed) { speedTest(g); redrawAll(g); return true; }
		break;

	default:
		break;
	}
	return false;
}

/* --------------------------------------------------------------- app */

static Boolean appHandleEvent(EventType* e)
{
	if (e->eType == frmLoadEvent) {
		FormType* frm = FrmInitForm(e->data.frmLoad.formID);
		FrmSetActiveForm(frm);
		if (e->data.frmLoad.formID == MainForm) FrmSetEventHandler(frm, mainFormHandleEvent);
		return true;
	}
	return false;
}

static void eventLoop(void)
{
	EventType e;
	Err err;
	do {
		Globals* g = G();
		if (g->playing && g->simulator) simFillRing(g);
		keepAwake(g);
		EvtGetEvent(&e, !g->playing ? evtWaitForever :
		                g->simulator ? SysTicksPerSecond() / 20 : SysTicksPerSecond() / 10);
		if (g->sleepPaused) {
			g->sleepPaused = false;
			FrmUpdateForm(MainForm, frmRedrawUpdateCode);
		}
		if (g->cardChanged) {
			g->cardChanged = false;
			FrmUpdateForm(MainForm, CARD_UPDATE_CODE);
		}
		if (SysHandleEvent(&e)) continue;
		if (MenuHandleEvent(0, &e, &err)) continue;
		if (appHandleEvent(&e)) continue;
		FrmDispatchEvent(&e);
	} while (e.eType != appStopEvent);
}

UInt32 PilotMain(UInt16 cmd, void* cmdPBP, UInt16 launchFlags)
{
	Globals* g;
	UInt32 rom = 0;

	if (cmd != sysAppLaunchCmdNormalLaunch) return 0;
	FtrGet(sysFtrCreator, sysFtrNumROMVersion, &rom);
	if (rom < sysMakeROMVersion(5, 0, 0, sysROMStageRelease, 0)) {
		FrmCustomAlert(ErrorAlert, "This player needs Palm OS 5.", "", "");
		return 0;
	}
	g = (Globals*)MemPtrNew(sizeof(Globals));
	if (!g) return 0;
	MemSet(g, sizeof(Globals), 0);
	g->loaded = -1;
	FtrSet(appCreator, ftrGlobals, (UInt32)g);
	loadPrefs(g);

	scanFiles(g);
#if DEBUG_LOG
	if (g->haveVolume) VFSFileDelete(g->volRef, "/sidlog.txt");
	dlog("SID Player start, files", g->nFiles);
#endif
	if (!startScreen(g))
		showError("Not enough memory for the screen.", 0, 0);
	else if (startCore(g)) {
		dlog("core started, stream", (UInt32)g->stream);
		watchSystem(true);
		FrmGotoForm(MainForm);
		eventLoop();
		FrmCloseAllForms();
		watchSystem(false);
	}
	stopCore(g);
	stopScreen(g);
	FtrUnregister(appCreator, ftrGlobals);
	MemPtrFree(g);
	return 0;
}

UInt32 __attribute__((section(".vectors"))) __Startup__(void)
{
	SysAppInfoPtr appInfoP;
	void* prevGlobalsP;
	void* globalsP;
	UInt32 ret;

	SysAppStartup(&appInfoP, &prevGlobalsP, &globalsP);
	ret = PilotMain(appInfoP->cmd, appInfoP->cmdPBP, appInfoP->launchFlags);
	SysAppExit(appInfoP, prevGlobalsP, globalsP);
	return ret;
}
