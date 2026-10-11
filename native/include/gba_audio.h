#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H

#include "gba_types.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 2048

typedef struct {
    s16 buffer[AUDIO_BUFFER_SIZE * 2]; // Stereo interleaved
    u32 buffer_pos;
    bool master_enable;
    float master_volume;

    // DirectSound FIFO buffers
    s8 fifo_a[32];
    s8 fifo_b[32];
    u8 fifo_a_count;
    u8 fifo_b_count;
} GbaAudio;

void audio_init(GbaAudio* audio);
void audio_mix_samples(GbaAudio* audio, u32 sample_count);
void audio_push_fifo_a(GbaAudio* audio, s8 sample);
void audio_push_fifo_b(GbaAudio* audio, s8 sample);

#endif // GBA_AUDIO_H

