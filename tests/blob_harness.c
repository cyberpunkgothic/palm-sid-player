/* Test host for the relocatable ARM core (build/blob/sidcore.bin): loads it the way the Palm app will
 * (copy, zero bss, add the base address to each listed word, call the entry with SidCoreCall blocks)
 * and renders a tune to WAV.  Built for ARM Linux and run under qemu-arm (tools/blob_check.sh).
 *   blob_harness sidcore.bin tune.sid out.wav [-s N] [-t SEC] [-r HZ]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "sidcore_api.h"

typedef unsigned long (*EntryFunc)(const void*, void*, void*);
static EntryFunc entry;

static int verbose;
static unsigned long call(SidCoreCall* c)
{
	if (verbose) fprintf(stderr, "call %lu\n", c->cmd);
	unsigned long r = entry(0, c, 0);
	if (verbose) fprintf(stderr, "  -> %lu\n", r);
	return r;
}

static void* readFile(const char* name, long* len)
{
	FILE* f = fopen(name, "rb");
	if (!f) { perror(name); exit(1); }
	fseek(f, 0, SEEK_END); *len = ftell(f); fseek(f, 0, SEEK_SET);
	void* p = malloc(*len);
	if (fread(p, 1, *len, f) != (size_t)*len) { perror(name); exit(1); }
	fclose(f);
	return p;
}

static void put16(FILE* f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE* f, unsigned long v) { put16(f, v & 0xffff); put16(f, (v >> 16) & 0xffff); }

int main(int argc, char** argv)
{
	if (argc < 4) { fprintf(stderr, "usage: blob_harness sidcore.bin tune.sid out.wav [-s N] [-t SEC] [-r HZ]\n"); return 2; }
	int song = 0, seconds = 120, rate = 44100;
	verbose = getenv("BLOB_VERBOSE") != 0;
	for (int i = 4; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "-s")) song = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "-t")) seconds = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "-r")) rate = atoi(argv[i + 1]);
	}

	/* load and relocate the core */
	long fileLen;
	unsigned char* file = readFile(argv[1], &fileLen);
	SidCoreFileHeader h;
	memcpy(&h, file, sizeof h);
	if (h.magic != SIDCORE_MAGIC || h.version != SIDCORE_VERSION) { fprintf(stderr, "not a sidcore file\n"); return 1; }
	/* executable memory (Linux needs it marked so; the Palm does not) */
	unsigned char* base = mmap(0, h.imageSize + h.bssSize, PROT_READ | PROT_WRITE | PROT_EXEC,
	                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) { perror("mmap"); return 1; }
	memcpy(base, file + sizeof h, h.imageSize);
	memset(base + h.imageSize, 0, h.bssSize);
	const unsigned long* rel = (const unsigned long*)(file + sizeof h + h.imageSize);
	for (unsigned long i = 0; i < h.relocCount; i++)
		*(unsigned long*)(base + rel[i]) += (unsigned long)base;
	entry = (EntryFunc)(base + h.entryOffset);
	if (verbose) fprintf(stderr, "core at %p\n", (void*)base);

	/* drive it */
	SidCoreCall c;
	size_t arenaSize = getenv("BLOB_ARENA_KB") ? (size_t)atoi(getenv("BLOB_ARENA_KB")) * 1024 : 1024 * 1024;
	memset(&c, 0, sizeof c);
	c.cmd = SC_INIT; c.a[0] = (unsigned long)malloc(arenaSize); c.a[1] = arenaSize; c.a[2] = 0;
	if (call(&c)) { fprintf(stderr, "init failed: %lu\n", c.result); return 1; }

	memset(&c, 0, sizeof c);
	c.cmd = SC_CONFIG; c.a[0] = rate; c.a[1] = 0; c.a[2] = 1; c.a[3] = 0;
	if (call(&c)) { fprintf(stderr, "config failed: %lu\n", c.result); return 1; }

	long tuneLen;
	void* tuneData = readFile(argv[2], &tuneLen);
	memset(&c, 0, sizeof c);
	c.cmd = SC_LOAD; c.a[0] = (unsigned long)tuneData; c.a[1] = tuneLen;
	if (call(&c)) { fprintf(stderr, "load failed: %s\n", (const char*)c.a[4]); return 1; }
	printf("%s | %s | %s | songs %lu (start %lu) | clock %lu model %lu\n",
	       c.str[0], c.str[1], c.str[2], c.a[0], c.a[1], c.a[2], c.a[3]);

	memset(&c, 0, sizeof c);
	c.cmd = SC_START; c.a[0] = song;
	if (call(&c)) { fprintf(stderr, "start failed\n"); return 1; }

	FILE* out = fopen(argv[3], "wb");
	if (!out) { perror(argv[3]); return 1; }
	unsigned long frames = (unsigned long)rate * seconds, block = rate / 10;
	fwrite("RIFF", 1, 4, out); put32(out, 36 + frames * 2); fwrite("WAVEfmt ", 1, 8, out);
	put32(out, 16); put16(out, 1); put16(out, 1); put32(out, rate); put32(out, rate * 2); put16(out, 2); put16(out, 16);
	fwrite("data", 1, 4, out); put32(out, frames * 2);
	short* buf = malloc(block * 2);
	for (unsigned long done = 0; done < frames; done += block) {
		unsigned long n = frames - done < block ? frames - done : block;
		memset(&c, 0, sizeof c);
		c.cmd = SC_FILL; c.a[0] = (unsigned long)buf; c.a[1] = n * 2;
		call(&c);
		fwrite(buf, 2, n, out);
	}
	fclose(out);

	memset(&c, 0, sizeof c);
	c.cmd = SC_SHUTDOWN;
	call(&c);
	if (verbose) fprintf(stderr, "peak arena use %lu bytes\n", c.a[0]);
	return 0;
}
