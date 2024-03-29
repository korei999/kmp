#pragma once
#include "main.hh"
#include "wav.hh"

#include <alsa/asoundlib.h>
#include <opus/opusfile.h>

namespace player
{

enum class file_t : int
{
    OPUS,
    WAV,
};

struct alsa
{
    std::string_view current_file;

    u8 channels;
    unsigned period_time;
    unsigned sample_rate;
    unsigned buffer_time;

    snd_pcm_format_t format;
    int format_size; /* S16 == 2, S32 == 4, S24 == 3? */

    snd_pcm_sframes_t buffer_size;
    snd_pcm_sframes_t period_size;

    [[maybe_unused]] int period_event = 0; /* produce poll event after each period */

    snd_pcm_t* handle {};
    snd_pcm_hw_params_t* hw_params {};
    snd_pcm_sw_params_t* sw_params {};

    enum file_t type {};
    OggOpusFile* opus_parser {};
    wav_file wav_parser {};

    std::vector<s8> chunk;
    s8* chunk_ptr;
    long pcmtotal;
    u64 now;

    alsa(std::string_view file_path,
         unsigned _period_time = def::period_time,
         unsigned _sample_rate = def::sample_rate,
         unsigned _bufferTime = def::buffer_time,
         const std::string_view device = "default",
         enum _snd_pcm_format _format = SND_PCM_FORMAT_S16_LE,
         snd_pcm_access_t _access = SND_PCM_ACCESS_RW_INTERLEAVED,
         int resample = 0);

    ~alsa();

    int set_channels([[maybe_unused]] u8 _channels);
    int set_hwparams2(snd_pcm_access_t access, int resample);
    int set_swparams();

    void print();

    int play_chunk();
    void resume();
    void pause();
    void left();
    void right();

    int next_chunk();

    void init_file_type();
    void init_opus();
    void init_wav();
};

}; /* namespace player */
