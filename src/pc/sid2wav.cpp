// sid2wav: render a SID tune to a WAV file with libsidplay 1 (SIDPLAY/w's "original" engine).
// The PC reference for the SID player project.  GPL v2 or later, like libsidplay.
//
// Usage: sid2wav in.sid out.wav [options]
//   -s N       subtune (default: the tune's start song)
//   -t SEC     length in seconds (default 120)
//   -r HZ      sample rate (default 44100)
//   -6581      6581 waveforms (default 8580, as in the user's SIDPLAY/w)
//   -nofilter  filter off
//   -pal/-ntsc force the clock (default: the tune's own clock, PAL if unknown)
//   -info      print the tune's details and stop
//   -tables F  (test aid) write the filter tables to file F and stop
//   -regs F    (test aid) log the SID registers to F every 1/50 s

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "player.h"

static void put16(FILE* f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE* f, unsigned long v) { put16(f, v & 0xffff); put16(f, (v >> 16) & 0xffff); }

static void wavHeader(FILE* f, unsigned rate, unsigned long dataBytes)
{
    fwrite("RIFF", 1, 4, f); put32(f, 36 + dataBytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1);          // PCM, mono
    put32(f, rate); put32(f, rate * 2); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, dataBytes);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sid2wav in.sid [out.wav] [-s N] [-t SEC] [-r HZ] [-6581] [-nofilter] [-pal|-ntsc] [-info]\n");
        return 2;
    }
    const char* in = argv[1];
    const char* out = 0;
    int song = 0, seconds = 120, rate = 44100, clock = 0;
    bool m6581 = false, filter = true, infoOnly = false;
    const char* tablesFile = 0;
    const char* regsFile = 0;
    for (int i = 2; i < argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "-s") && i + 1 < argc) song = atoi(argv[++i]);
        else if (!strcmp(a, "-t") && i + 1 < argc) seconds = atoi(argv[++i]);
        else if (!strcmp(a, "-r") && i + 1 < argc) rate = atoi(argv[++i]);
        else if (!strcmp(a, "-6581")) m6581 = true;
        else if (!strcmp(a, "-nofilter")) filter = false;
        else if (!strcmp(a, "-pal")) clock = SIDTUNE_CLOCK_PAL;
        else if (!strcmp(a, "-ntsc")) clock = SIDTUNE_CLOCK_NTSC;
        else if (!strcmp(a, "-info")) infoOnly = true;
        else if (!strcmp(a, "-tables") && i + 1 < argc) tablesFile = argv[++i];
        else if (!strcmp(a, "-regs") && i + 1 < argc) regsFile = argv[++i];
        else if (a[0] != '-' && !out) out = a;
        else { fprintf(stderr, "unknown option %s\n", a); return 2; }
    }

    emuEngine engine;
    if (!engine) { fprintf(stderr, "engine failed to start\n"); return 1; }
    emuConfig cfg;
    engine.getConfig(cfg);
    cfg.frequency = (uword)rate;
    cfg.bitsPerSample = SIDEMU_16BIT;
    cfg.sampleFormat = SIDEMU_SIGNED_PCM;
    cfg.channels = SIDEMU_MONO;
    cfg.mos8580 = !m6581;
    cfg.measuredVolume = true;
    cfg.emulateFilter = filter;
    cfg.memoryMode = MPU_BANK_SWITCHING;
    if (clock) { cfg.clockSpeed = clock; cfg.forceSongSpeed = true; }
    else { cfg.clockSpeed = SIDTUNE_CLOCK_PAL; cfg.forceSongSpeed = false; }
    if (!engine.setConfig(cfg)) { fprintf(stderr, "bad engine settings\n"); return 1; }

    sidTune tune(in);
    sidTuneInfo info;
    tune.getInfo(info);
    if (!tune) { fprintf(stderr, "%s: %s\n", in, info.statusString); return 1; }
    printf("%s | %s | %s | %s | songs %u (start %u) | clock %u model %u\n",
           info.nameString ? info.nameString : "?", info.authorString ? info.authorString : "?",
           info.copyrightString ? info.copyrightString : "?", info.formatString,
           info.songs, info.startSong, info.clock, info.sidModel);
    if (infoOnly) return 0;
    if (tablesFile) {                         // test aid: dump the filter tables to a file
        FILE* tf = fopen(tablesFile, "wb");
        extern filterfloat filterTable[0x800], bandPassParam[0x800], filterResTable[16];
        fwrite(filterTable, sizeof(filterfloat), 0x800, tf);
        fwrite(bandPassParam, sizeof(filterfloat), 0x800, tf);
        fwrite(filterResTable, sizeof(filterfloat), 16, tf);
        fclose(tf);
        return 0;
    }
    if (!out) { fprintf(stderr, "no output file\n"); return 2; }

    if (!sidEmuInitializeSong(engine, tune, (uword)song)) {
        tune.getInfo(info);
        fprintf(stderr, "song init failed: %s\n", info.statusString);
        return 1;
    }

    FILE* f = fopen(out, "wb");
    if (!f) { perror(out); return 1; }
    unsigned long frames = (unsigned long)rate * seconds;
    wavHeader(f, rate, frames * 2);
    // 0.1 s blocks; with -regs (test aid) 1/50 s blocks, logging the SID registers after each
    unsigned long block = regsFile ? rate / 50 : rate / 10;
    FILE* rf = regsFile ? fopen(regsFile, "wb") : 0;
    extern ubyte* c64mem2;
    std::vector<unsigned char> buf(block * 2);
    for (unsigned long done = 0; done < frames; done += block) {
        unsigned long n = frames - done < block ? frames - done : block;
        sidEmuFillBuffer(engine, tune, &buf[0], n * 2);
        fwrite(&buf[0], 1, n * 2, f);                 // libsidplay's 16-bit output is little-endian here
        if (rf) fwrite(c64mem2 + 0xd400, 1, 0x19, rf);
    }
    fclose(f);
    if (rf) fclose(rf);
    return 0;
}
