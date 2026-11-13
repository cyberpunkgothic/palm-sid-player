/* sidcore: libsidplay 1 as one relocatable block of ARM code ("the core"), for the Palm (and any
 * host without a C library for ARM).  GPL v2 or later, like libsidplay.
 *
 * Loading (done by the host, see tests/blob_harness.c and tools/blob_pack.py for the file layout):
 *   copy the image to writable memory, zero the bss after it, add the image's base address to every
 *   32-bit word listed in the relocation table, then call base + entryOffset.
 *
 * In the Palm OS Simulator (x86) the same code is a Windows DLL, build/pc/sidcore.dll (tools/pc_build.bat
 * dll), next to PalmSim.exe: PceNativeCall("sidcore.dll\0sidcoreEntry", call).
 *
 * Calling: entry(emulStateP, call, call68KFuncP) -- the Palm OS PceNativeCall signature; the first and
 * last arguments are ignored.  `call` points to a SidCoreCall block.  All its fields are in the ARM's
 * byte order (little-endian): a 68k host must byte-swap what it writes and reads.
 *
 * Memory: the core never calls the OS.  SC_INIT hands it one arena; all its allocations come from there.
 * It needs about 400 KB with a tune loaded (SC_SHUTDOWN reports the peak); SC_INIT refuses less than
 * SIDCORE_MIN_ARENA, because libsidplay does not check all its start-up allocations.  640 KB is ample.
 * The core's own image plus bss is about 210 KB more (see the packed file's header).
 */
#ifndef SIDCORE_API_H
#define SIDCORE_API_H

#define SIDCORE_MAGIC   0x43444953UL   /* 'SIDC' read as a little-endian word */
#define SIDCORE_VERSION 1
#define SIDCORE_MIN_ARENA (512UL * 1024)

enum {
	SC_INIT = 1,      /* a0 = arena, a1 = arena size, a2 = random seed (e.g. the time).
	                     Returns a0 = the native stream callback for SndStreamCreate(..., armNative
	                     = true): it plays the started song, or silence                   -> 0 ok */
	SC_CONFIG = 2,    /* a0 = sample rate, a1 = 1 for 6581 waveforms (else 8580),
	                     a2 = 1 filter on, a3 = clock: 0 the tune's own, 1 PAL, 2 NTSC       -> 0 ok */
	SC_LOAD = 3,      /* a0 = PSID file data, a1 = its length.  Fills a0..a4 with songs, start song,
	                     clock, SID model, load status; str[0..2] = name, author, copyright  -> 0 ok */
	SC_START = 4,     /* a0 = subtune (0 = the tune's start song); returns a0 = the song    -> 0 ok */
	SC_FILL = 5,      /* a0 = buffer, a1 = length in bytes (16-bit signed mono samples)    -> 0 ok
	                     (for tests; the player uses the stream callback instead).  Stop the
	                     stream before SC_LOAD, SC_START, SC_CONFIG and SC_SHUTDOWN. */
	SC_SHUTDOWN = 6,  /* free everything; a0 = peak arena use in bytes                      -> 0    */
	SC_LEVELS = 7     /* meters, voices 1-3, since the last call: a0..a2 = peak output 0..255,
	                     a3..a5 = 1 if a new note started (gate on or pitch jump)          -> 0 ok */
};

/* Results other than 0 */
enum { SC_ERR_COMMAND = 1, SC_ERR_STATE = 2, SC_ERR_MEMORY = 3, SC_ERR_TUNE = 4, SC_ERR_CONFIG = 5 };

typedef struct {
	unsigned long cmd;
	unsigned long result;
	unsigned long a[6];
	const char* str[3];
} SidCoreCall;

/* The packed file (build/blob/sidcore.bin), all words little-endian:
 *   0  magic SIDCORE_MAGIC        4  version
 *   8  image size (bytes)        12  bss size (bytes, zeroed after the image)
 *  16  entry offset              20  number of relocations (n)
 *  24  image ... then n words: offsets in the image of 32-bit words to add the base address to */
typedef struct {
	unsigned long magic, version, imageSize, bssSize, entryOffset, relocCount;
} SidCoreFileHeader;

#endif
