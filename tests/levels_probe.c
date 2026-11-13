/* Prints the SC_LEVELS meter values every 0.1 s for a tune (via build/pc/sidcore.dll), to see how the
   level bars would move.  Usage: levels_probe tune.sid [seconds] */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "sidcore_api.h"
typedef unsigned long (*Entry)(const void*, void*, void*);
typedef long (*Stream)(void*, unsigned long, void*, unsigned long);
int main(int argc, char** argv)
{
	HMODULE dll = LoadLibraryA("build/pc/sidcore.dll");
	Entry entry = (Entry)GetProcAddress(dll, "sidcoreEntry");
	SidCoreCall c; static unsigned char buf[70000]; static short pcm[4410];
	int secs = argc > 2 ? atoi(argv[2]) : 6;
	memset(&c, 0, sizeof c); c.cmd = SC_INIT; c.a[0] = (unsigned long)malloc(640 * 1024); c.a[1] = 640 * 1024; entry(0, &c, 0);
	Stream stream = (Stream)c.a[0];
	memset(&c, 0, sizeof c); c.cmd = SC_CONFIG; c.a[0] = 44100; c.a[2] = 1; entry(0, &c, 0);
	FILE* f = fopen(argv[1], "rb"); long n = (long)fread(buf, 1, sizeof buf, f); fclose(f);
	memset(&c, 0, sizeof c); c.cmd = SC_LOAD; c.a[0] = (unsigned long)buf; c.a[1] = n; entry(0, &c, 0);
	memset(&c, 0, sizeof c); c.cmd = SC_START; entry(0, &c, 0);
	for (int t = 0; t < secs * 10; t++) {
		stream(0, 0, pcm, 4410);                       /* 0.1 s */
		memset(&c, 0, sizeof c); c.cmd = SC_LEVELS; entry(0, &c, 0);
		printf("%4.1f  %3lu %3lu %3lu  notes %lu%lu%lu\n", t / 10.0, c.a[0], c.a[1], c.a[2], c.a[3], c.a[4], c.a[5]);
	}
	return 0;
}
