#pragma once
#include "ultratypes.h"

#include <alsa/asoundlib.h>
#include <string_view>
#include <vector>

struct wav_file
{
    unsigned sample_rate;
    unsigned period_time;
    snd_pcm_sframes_t period_size;

    s8 channels;
    s32 riff_chunk_size;
    s32 block_align;
    std::vector<s16> song_data {};
    std::vector<s16>* chunk_ptr;
    long offset = 0;

    wav_file() = default;

    int open_file(const std::string_view path);

    int next_chunk();

    long now();
    long pcmtotal();

    void left();
    void right();
};
