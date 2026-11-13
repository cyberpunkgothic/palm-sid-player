/* Drives build/pc/sidcore.dll (the simulator's copy of the core) directly on Windows, with the same
   calls the Palm app makes.  Build: see tools/pc_build.bat dlltest.  Usage: dll_harness tune.sid out.wav */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sidcore_api.h"

typedef unsigned long (*Entry)(const void*, void*, void*);
typedef long (*Stream)(void*, unsigned long, void*, unsigned long);

int main(int argc, char** argv)
{
	HMODULE dll = LoadLibraryA("build/pc/sidcore.dll");
	if (!dll) { printf("no dll: error %lu\n", GetLastError()); return 1; }
	Entry entry = (Entry)GetProcAddress(dll, "sidcoreEntry");
	SidCoreCall c;
	size_t arena = 640 * 1024;
	memset(&c, 0, sizeof c); c.cmd = SC_INIT; c.a[0] = (unsigned long)malloc(arena); c.a[1] = arena; c.a[2] = 0;
	entry(0, &c, 0); printf("init %lu stream %p\n", c.result, (void*)c.a[0]);
	Stream stream = (Stream)c.a[0];
	memset(&c, 0, sizeof c); c.cmd = SC_CONFIG; c.a[0] = 44100; c.a[2] = 1;
	entry(0, &c, 0); printf("config %lu\n", c.result);
	if (argc < 3) return 0;
	FILE* f = fopen(argv[1], "rb"); static unsigned char buf[70000]; long n = (long)fread(buf, 1, sizeof buf, f); fclose(f);
	memset(&c, 0, sizeof c); c.cmd = SC_LOAD; c.a[0] = (unsigned long)buf; c.a[1] = n;
	entry(0, &c, 0); printf("load %lu songs %lu name %s\n", c.result, c.a[0], c.str[0]);
	memset(&c, 0, sizeof c); c.cmd = SC_START; c.a[0] = 0;
	entry(0, &c, 0); printf("start %lu song %lu\n", c.result, c.a[0]);
	FILE* o = fopen(argv[2], "wb"); static short pcm[2048];
	for (int i = 0; i < 44100 * 10 / 2048; i++) { stream(0, 0, pcm, 2048); fwrite(pcm, 2, 2048, o); }
	fclose(o);
	return 0;
}
