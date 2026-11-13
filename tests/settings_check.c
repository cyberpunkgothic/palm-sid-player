/* Checks that changing the settings with a tune loaded (as the Palm apps' Settings box does: SC_CONFIG,
   then SC_START again) gives exactly the sound of a fresh start with those settings.
   Uses build/pc/sidcore.dll.  Usage: settings_check tune.sid   (prints one line per settings combination) */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sidcore_api.h"
typedef unsigned long (*Entry)(const void*, void*, void*);
static Entry entry;
static unsigned char tuneData[70000];
static long tuneLen;
#define SECS 60
#define BYTES (44100 * 2 * SECS)

static unsigned long call(unsigned long cmd, unsigned long a0, unsigned long a1, unsigned long a2, unsigned long a3)
{
	SidCoreCall c;
	memset(&c, 0, sizeof c);
	c.cmd = cmd; c.a[0] = a0; c.a[1] = a1; c.a[2] = a2; c.a[3] = a3;
	entry(0, &c, 0);
	return c.result;
}

static void render(unsigned char* out)
{
	unsigned long i;
	for (i = 0; i < BYTES; i += 8820) call(SC_FILL, (unsigned long)(out + i), 8820, 0, 0);
}

static void* arena;

/* fresh: init, config, load, start, render */
static void fresh(int chip, int filter, int clock, unsigned char* out)
{
	call(SC_INIT, (unsigned long)arena, 640 * 1024, 1, 0);
	call(SC_CONFIG, 44100, chip, filter, clock);
	call(SC_LOAD, (unsigned long)tuneData, tuneLen, 0, 0);
	call(SC_START, 0, 0, 0, 0);
	render(out);
	call(SC_SHUTDOWN, 0, 0, 0, 0);
}

/* changed: start with the defaults, play a while, then change the settings and restart the song */
static void changed(int chip, int filter, int clock, unsigned char* out)
{
	call(SC_INIT, (unsigned long)arena, 640 * 1024, 1, 0);
	call(SC_CONFIG, 44100, 0, 1, 0);
	call(SC_LOAD, (unsigned long)tuneData, tuneLen, 0, 0);
	call(SC_START, 0, 0, 0, 0);
	render(out);                                   /* 10 s with the defaults (thrown away) */
	call(SC_CONFIG, 44100, chip, filter, clock);
	call(SC_START, 0, 0, 0, 0);
	render(out);
	call(SC_SHUTDOWN, 0, 0, 0, 0);
}

int main(int argc, char** argv)
{
	static unsigned char a[BYTES], b[BYTES];
	HMODULE dll = LoadLibraryA("build/pc/sidcore.dll");
	FILE* f;
	int chip, filter, clock, bad = 0;
	entry = (Entry)GetProcAddress(dll, "sidcoreEntry");
	arena = malloc(640 * 1024);
	f = fopen(argv[1], "rb");
	tuneLen = (long)fread(tuneData, 1, sizeof tuneData, f);
	fclose(f);
	for (chip = 0; chip <= 1; chip++)
		for (filter = 0; filter <= 1; filter++)
			for (clock = 0; clock <= 2; clock++) {
				int same;
				fresh(chip, filter, clock, a);
				changed(chip, filter, clock, b);
				same = memcmp(a, b, BYTES) == 0;
				if (!same) bad++;
				{
					unsigned long sum = 0, i;                   /* a fingerprint, to see each setting matters */
					for (i = 0; i < BYTES; i++) sum = sum * 31 + a[i];
					printf("%s filter %s clock %-5s: %s  (sound %08lx)\n", chip ? "6581" : "8580",
					       filter ? "on " : "off", clock == 1 ? "PAL" : clock == 2 ? "NTSC" : "auto",
					       same ? "identical" : "DIFFERENT", sum);
				}
			}
	return bad;
}
