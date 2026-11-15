// crsidengine: cRSID making the sound and running the tune (see crsidengine.h).
// cRSID (src/crsid) is by Hermit (Mihaly Horvath), WTFPL ("please mention me as the original author").
// This file: GPL v2 or later, like the rest of sid-player.

static char crsidFilterOff = 0;          // read by src/crsid/C64/SID.c (sid-player's Filter Off)

#include "libcRSID.c"
#include "crsidengine.h"

// The file, with its header made into a v2 one that says the chip and clock wanted.  cRSID reads
// the v2 flags even from v1 headers (where they're the tune's first bytes), takes its chip and clock
// from them when a song starts, and expects a real init address; libsidplay 1 handles all three
// itself, so they're settled here.  (Largest PSID: header + 2-byte load address + 64 KB.)
enum { V2_HEADER = 0x7C, MAX_FILE = V2_HEADER + 2 + 0x10000 };
static unsigned char file[MAX_FILE];
static unsigned long fileLen = 0;
static cRSID_C64instance* c64 = 0;
static unsigned char lastGate[3], peaks[3], notes[3];
static unsigned char origFlags;         // the file's own flags byte ($77; 0 in v1 headers)
static char text[3][33];                // name, author, released (32 characters, ended here)

int crsidLoad(const unsigned char* d, unsigned long len)
{
	unsigned long hdr, i;
	fileLen = 0;
	if (len < 0x76 || !(d[0] == 'P' || d[0] == 'R') || d[1] != 'S' || d[2] != 'I' || d[3] != 'D')
		return 0;
	hdr = ((unsigned long)d[6] << 8) | d[7];
	if (hdr < 0x76 || hdr > len || len - hdr + V2_HEADER > MAX_FILE)
		return 0;
	for (i = 0; i < 0x76; i++) file[i] = d[i];
	if (hdr >= V2_HEADER)
		for (i = 0x76; i < V2_HEADER; i++) file[i] = d[i];
	else
		for (i = 0x76; i < V2_HEADER; i++) file[i] = 0;
	origFlags = hdr >= V2_HEADER ? d[0x77] : 0;
	for (i = 0; i < 3; i++) {
		int k;
		for (k = 0; k < 32; k++) text[i][k] = (char)d[0x16 + i * 0x20 + k];
		text[i][32] = 0;
	}
	if (file[5] < 2) file[5] = 2;
	file[6] = 0;
	file[7] = V2_HEADER;
	for (i = hdr; i < len; i++) file[V2_HEADER + i - hdr] = d[i];
	fileLen = V2_HEADER + len - hdr;
	// init address 0: the load address (from the header, or the data's first two bytes)
	if (file[0x0A] == 0 && file[0x0B] == 0) {
		if (file[0x08] || file[0x09]) { file[0x0A] = file[0x08]; file[0x0B] = file[0x09]; }
		else if (fileLen >= V2_HEADER + 2) { file[0x0A] = file[V2_HEADER + 1]; file[0x0B] = file[V2_HEADER]; }
	}
	return 1;
}

int crsidInfo(unsigned* songs, unsigned* startSong, int* clock, int* model, int* rsid,
              const char** name, const char** author, const char** released)
{
	if (!fileLen) return 0;
	*songs = ((unsigned)file[0x0E] << 8) | file[0x0F];
	*startSong = ((unsigned)file[0x10] << 8) | file[0x11];
	if (*songs == 0) *songs = 1;
	if (*startSong == 0 || *startSong > *songs) *startSong = 1;
	*clock = (origFlags >> 2) & 3;
	*model = (origFlags >> 4) & 3;
	*rsid = file[0] == 'R';
	*name = text[0];
	*author = text[1];
	*released = text[2];
	return 1;
}

int crsidStart(int song, int mos8580, int ntsc, int filter, unsigned long sampleRate, int playsid)
{
	cRSID_SIDheader* h;
	int v;
	if (!fileLen) return 0;
	if (song <= 0) song = ((int)file[0x10] << 8) | file[0x11];
	if (song <= 0) song = 1;
	// flags (low byte, $77): bits 2-3 clock (1 PAL, 2 NTSC), bits 4-5 chip (1 6581, 2 8580); bit 1
	// in a PSID: PlaySID's sample registers (cRSID plays them only when it's set)
	file[0x77] = (unsigned char)((file[0x77] & ~0x3C) | (ntsc ? 0x08 : 0x04) | (mos8580 ? 0x20 : 0x10));
	if (file[0] == 'P')
		file[0x77] = (unsigned char)((file[0x77] & ~0x02) | (playsid || (origFlags & 0x02) ? 0x02 : 0));
#ifdef CRSID_PROBE_NOSAMPLES   // (test builds only: compare with the sample mode off)
	file[0x77] &= (unsigned char)~0x02;
#endif
	crsidFilterOff = !filter;
	c64 = cRSID_init((unsigned short)sampleRate, 0);
	h = cRSID_processSIDfile(c64, file, (int)fileLen);
	if (!h) return 0;
	cRSID_initSIDtune(c64, h, (char)song);
	for (v = 0; v < 3; v++) lastGate[v] = peaks[v] = notes[v] = 0;
	return c64->SubTune;
}

void crsidFill(short* out, unsigned long samples)
{
	unsigned long i;
	int v;
	if (!c64) {
		for (i = 0; i < samples; i++) out[i] = 0;
		return;
	}
	for (i = 0; i < samples; i++)
		out[i] = cRSID_generateSample(c64);
	// meters: once per buffer, like libsidplay's mixer does per fill
	for (v = 0; v < 3; v++) {
		const cRSID_SIDinstance* s = &c64->SID[1];
		unsigned char e = (unsigned char)(s->EnvelopeCounter[v * 7] >> 1);
		unsigned char gate = s->BasePtr[v * 7 + 4] & 1;
		if (e > peaks[v]) peaks[v] = e;
		if (gate && !lastGate[v]) notes[v] = 1;
		lastGate[v] = gate;
	}
}

void crsidMeters(unsigned char peak[3], unsigned char note[3])
{
	int v;
	for (v = 0; v < 3; v++) {
		peak[v] = peaks[v];
		note[v] = notes[v];
		peaks[v] = notes[v] = 0;
	}
}
