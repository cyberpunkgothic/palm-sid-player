// crsidengine: cRSID (Hermit's integer-only C64/SID emulator, src/crsid) as a third engine.
// It replaces libsidplay 1 while it plays: cRSID runs the tune on its own 6510, CIA and VIC.  It also
// plays RSID tunes (libsidplay 1 can't load them) and old rips using PlaySID's sample registers.
#ifndef CRSIDENGINE_H
#define CRSIDENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

// Keep a copy of the file (cRSID reads its header while playing).  0 = too big or not a PSID/RSID.
int crsidLoad(const unsigned char* data, unsigned long len);
// The loaded tune's header: song count, start song, clock and chip as the file gives them
// (0 unknown, 1 PAL / 6581, 2 NTSC / 8580, 3 both), RSID or not, and the three text lines.
// 0 = nothing loaded.
int crsidInfo(unsigned* songs, unsigned* startSong, int* clock, int* model, int* rsid,
              const char** name, const char** author, const char** released);
// Start a song (1-based; 0 = the tune's start song) with this chip (1 = 8580), clock (1 = NTSC),
// filter (0 = off) and sample rate; playsid: play PlaySID's sample registers (old digi rips).  The
// tune is reloaded into a fresh C64 each time.  Returns the song started, 0 = nothing loaded.
int crsidStart(int song, int mos8580, int ntsc, int filter, unsigned long sampleRate, int playsid);
// `samples` 16-bit mono samples.
void crsidFill(short* out, unsigned long samples);
// The level meters, as libsidplay's mixer keeps them (mixing.cpp): the highest envelope level
// (0-128) and new notes (gate on) of each voice since the meters were last read.
void crsidMeters(unsigned char peak[3], unsigned char note[3]);

#ifdef __cplusplus
}
#endif

#endif
