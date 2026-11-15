// residengine: reSID 0.16 making the sound for libsidplay 1 (see residengine.h).  GPL v2 or later.

#include "mytypes.h"
#include "residengine.h"
#include "sid.h"
#include "spline.h"
#include "curves.h"

extern void (*sidWriteHook)(int reg, int value);           // 6510_.cpp
extern void* (*sidEmuFillOverride)(void*, udword);         // 6581_.cpp
extern udword C64_clockSpeed;                              // 6581_.cpp: the song's C64 clock
extern ubyte voicePeak[3];                                 // mixing.cpp: the level meters

static SID* sid = 0;

// The 6510's SID writes since the last samples were made, in order.  (If a routine writes more
// than this between two fills, the rest go to reSID straight away, without the 4-cycle gaps.)
enum { QUEUE = 1024, WRITE_GAP = 4 };
static unsigned short queue[QUEUE];
static int queued = 0;

// Samples reSID made while clocking the write gaps: they come first in the next fill.
enum { PENDING = 256 };
static short pending[PENDING];
static int pendingCount = 0;

static void writeHook(int reg, int value)
{
	if (queued < QUEUE)
		queue[queued++] = (unsigned short)((reg << 8) | (value & 0xff));
	else
		sid->write(reg, value);
}

// Hand the queued writes to reSID, WRITE_GAP cycles apart (as the 6510's stores would arrive).
static void flushWrites()
{
	for (int i = 0; i < queued; i++) {
		sid->write(queue[i] >> 8, queue[i] & 0xff);
		cycle_count d = WRITE_GAP;
		while (d > 0 && pendingCount < PENDING)
			pendingCount += sid->clock(d, pending + pendingCount, PENDING - pendingCount);
		if (d > 0)
			sid->clock(d);                     // (pending full: clock on without samples)
	}
	queued = 0;
}

// The fill function libsidplay calls instead of its own voices: `samples` 16-bit mono samples.
static void* residFill(void* buffer, udword samples)
{
	short* out = (short*)buffer;
	flushWrites();
	int n = (int)samples;
	int fromPending = pendingCount < n ? pendingCount : n;
	for (int i = 0; i < fromPending; i++)
		out[i] = pending[i];
	if (fromPending) {
		for (int i = fromPending; i < pendingCount; i++)
			pending[i - fromPending] = pending[i];
		pendingCount -= fromPending;
	}
	int done = fromPending;
	while (done < n) {
		// reSID stops when it has made the samples asked for; the cycle budget is only an upper bound
		cycle_count budget = (cycle_count)(n - done) * 64 + 64;
		done += sid->clock(budget, out + done, n - done);
	}
	// level meters: the voices' envelope levels (0-255; libsidplay's peaks are 0-128)
	SID::State st = sid->read_state();
	for (int v = 0; v < 3; v++) {
		ubyte e = (ubyte)(st.envelope_counter[v] >> 1);
		if (e > voicePeak[v]) voicePeak[v] = e;
	}
	return out + n;
}

bool residEnable(bool on)
{
	if (!on) {
		sidWriteHook = 0;
		sidEmuFillOverride = 0;
		return true;
	}
	if (!sid) {
		sid = new SID;
		if (!sid) return false;
	}
	sid->reset();
	queued = 0;
	pendingCount = 0;
	sidWriteHook = &writeHook;
	sidEmuFillOverride = &residFill;
	return true;
}

bool residActive()
{
	return sidEmuFillOverride == &residFill;
}

void residSongStart(bool mos8580, bool filter, bool bright, unsigned long sampleRate)
{
	if (!sid) return;
	sid->set_chip_model(mos8580 ? MOS8580 : MOS6581);
	// The cut-off curve for this chip: reSIDfp's measured one, or reSID 0.16's own (which also
	// puts it back after the bright one).
	if (bright) {
		const fc_point* pts = mos8580 ? brightCurve8580 : brightCurve6581;
		int n = mos8580 ? (int)(sizeof brightCurve8580 / sizeof brightCurve8580[0])
		                : (int)(sizeof brightCurve6581 / sizeof brightCurve6581[0]);
		interpolate(pts, pts + n - 1, sid->fc_plotter(), 1.0);
	} else {
		const fc_point* pts;
		int n;
		sid->fc_default(pts, n);
		interpolate(pts, pts + n - 1, sid->fc_plotter(), 1.0);
	}
	sid->enable_filter(filter);
	sid->set_sampling_parameters((double)C64_clockSpeed, SAMPLE_FAST, (double)sampleRate);
}
