// How many samples the sound library (libmus, its n_audio build) makes each retrace, so that what it queues keeps
// pace with what plays.
//
// libmus's audio thread (lib/mariogolf64's src/libmus/aud_thread.c) reads how much is still queued (osAiGetLength)
// and asks __MusIntSamplesCurrent (src/libmus/aud_samples.c) for this frame's count: whole blocks of 184, 552 at the
// game's 32000 Hz, where a retrace plays about 533. Above its band it makes one short frame (368), below it one long
// one (736), and then the nominal 552 until the queue is back in the band, however long that takes. On a console the
// rest is the audio interface's: while its two-buffer FIFO is full the thread skips the frame
// (osAiGetStatus's AI_STATUS_FIFO_FULL). The runtime's FIFO is never full, so the queue grew by about 19 samples a
// retrace, past the band and on to the 100 ms where src/main/main.cpp's queue_samples drops every other sample of a
// buffer to catch up: one buffer in 15, a click about four times a second, through all of the game's sound.
//
// Here the short or long frame comes every retrace the queue is outside the band, not only the first, so the queue
// stays in it (27-60 ms queued in all, with the two retraces src/main/main.cpp leaves out of osAiGetLength) and nothing
// is dropped.

#include "patches.h"

#define N_SAMPLES 184

extern u32 g_mus_frame_samples;
extern u32 g_mus_frame_samples_min;
extern u32 g_mus_frame_samples_max;
extern u32 g_mus_extra_samples;

RECOMP_PATCH u32 __MusIntSamplesCurrent(u32 samples) {
    if (samples > N_SAMPLES + g_mus_extra_samples) {
        return g_mus_frame_samples_min;
    }
    if (samples < g_mus_extra_samples) {
        return g_mus_frame_samples_max;
    }
    return g_mus_frame_samples;
}
