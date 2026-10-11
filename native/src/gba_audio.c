#include "gba_audio.h"
#include <string.h>

void audio_init(GbaAudio* audio) {
    memset(audio, 0, sizeof(GbaAudio));
    audio->master_enable = true;
    audio->master_volume = 1.0f;
}

void audio_push_fifo_a(GbaAudio* audio, s8 sample) {
    if (audio->fifo_a_count < 32) {
        audio->fifo_a[audio->fifo_a_count++] = sample;
    }
}

void audio_push_fifo_b(GbaAudio* audio, s8 sample) {
    if (audio->fifo_b_count < 32) {
        audio->fifo_b[audio->fifo_b_count++] = sample;
    }
}

void audio_mix_samples(GbaAudio* audio, u32 sample_count) {
    if (!audio->master_enable) {
        memset(audio->buffer, 0, sample_count * 2 * sizeof(s16));
        return;
    }

    for (u32 i = 0; i < sample_count; i++) {
        s16 left = 0;
        s16 right = 0;

        // Mix DirectSound FIFO A & B
        if (audio->fifo_a_count > 0) {
            s8 s = audio->fifo_a[0];
            left += (s16)s * 256;
            right += (s16)s * 256;
        }

        audio->buffer[i * 2 + 0] = (s16)(left * audio->master_volume);
        audio->buffer[i * 2 + 1] = (s16)(right * audio->master_volume);
    }
}

