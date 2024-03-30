#pragma once
#include "ultratypes.h"

#include <alsa/asoundlib.h>
#include <mpg123.h>
#include <string_view>
#include <vector>

struct mp3_file
{
    mpg123_handle* handle;

    long sample_rate;
    int channels;
    off_t total;

    snd_pcm_sframes_t period_size;
    unsigned period_time;

    std::vector<s16>* chunk_ptr;

    ~mp3_file();

    int open_file(std::string_view path);
    int next_chunk();
    long now();
    void left(u64 pos);
    void right(u64 pos);
};
