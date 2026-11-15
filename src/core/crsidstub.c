// crsidstub: the cRSID engine's calls for the Palm OS Simulator's DLL (MSVC can't compile cRSID: its
// 6510 is written as GCC nested functions).  Choosing cRSID there plays libsidplay instead; the real
// cRSID engine is in the ARM core only (crsidengine.c).  GPL v2 or later.
#include "crsidengine.h"
int crsidLoad(const unsigned char* data, unsigned long len) { (void)data; (void)len; return 0; }
int crsidInfo(unsigned* songs, unsigned* startSong, int* clock, int* model, int* rsid,
              const char** name, const char** author, const char** released)
{ (void)songs; (void)startSong; (void)clock; (void)model; (void)rsid; (void)name; (void)author; (void)released; return 0; }
int crsidStart(int song, int mos8580, int ntsc, int filter, unsigned long sampleRate, int playsid)
{ (void)song; (void)mos8580; (void)ntsc; (void)filter; (void)sampleRate; (void)playsid; return 0; }
void crsidFill(short* out, unsigned long samples) { unsigned long i; for (i = 0; i < samples; i++) out[i] = 0; }
void crsidMeters(unsigned char peak[3], unsigned char note[3]) { int v; for (v = 0; v < 3; v++) peak[v] = note[v] = 0; }
