#include "player.hh"
#include "util.hh"

using namespace player;

alsa::alsa(std::string_view file_path,
           unsigned _period_time,
           unsigned _sample_rate,
           unsigned _buffer_time,
           const std::string_view device,
           enum _snd_pcm_format _format,
           snd_pcm_access_t _access,
           int resample)
    : current_file {file_path},
      period_time {_period_time},
      sample_rate {_sample_rate},
      buffer_time {_buffer_time},
      format {_format}
{
    int err;

    ////////////////////////////////////////////////
    init_file_type();
    ////////////////////////////////////////////////

    if ((err = snd_pcm_open(&handle, device.data(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        Die("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_malloc(&hw_params);
    snd_pcm_sw_params_malloc(&sw_params);

    // err = set_hwparams(_access, resample);
    err = set_hwparams2(_access, resample);
    if (err < 0)
    {
        Die("Setting of hw_params failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    err = set_swparams();
    if (err < 0)
    {
        Die("Setting of sw_params failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
}

alsa::~alsa()
{
    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    snd_pcm_hw_params_free(hw_params);
    snd_pcm_sw_params_free(sw_params);

    if (this->type == file_t::OPUS)
    {
        op_free(opus_parser);
    }
}

int
alsa::set_channels(u8 _channels)
{
    int err;

    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, hw_params, _channels);
    if (err < 0)
    {
        Die("Channels count (%u) not available for playbacks: %s\n", _channels, snd_strerror(err));
        return err;
    }

    return 0;
}

int
alsa::set_hwparams2(snd_pcm_access_t access, int resample)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;

    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, hw_params);
    if (err < 0)
    {
        Die("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, hw_params, resample);
    if (err < 0)
    {
        Die("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, hw_params, access);
    if (err < 0)
    {
        Die("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, hw_params, format);
    if (err < 0)
    {
        Die("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, hw_params, channels);
    if (err < 0)
    {
        Die("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = sample_rate;
    err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rrate, 0);
    if (err < 0)
    {
        Die("Rate %uHz not available for playback: %s\n", sample_rate, snd_strerror(err));
        return err;
    }
    if (rrate != sample_rate)
    {
        Die("Rate doesn't match (requested %uHz, get %iHz)\n", sample_rate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, hw_params, &buffer_time, &dir);
    if (err < 0)
    {
        Die("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(hw_params, &size);
    if (err < 0)
    {
        Die("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    buffer_size = size;
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(handle, hw_params, &period_time, &dir);
    if (err < 0)
    {
        Die("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(hw_params, &size, &dir);
    if (err < 0)
    {
        Die("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    period_size = size;
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, hw_params);
    if (err < 0)
    {
        Die("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}

int
alsa::set_swparams()
{
    int err;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, sw_params);
    if (err < 0)
    {
        Die("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */ /* ????? */
    err = snd_pcm_sw_params_set_start_threshold(handle, sw_params, buffer_size);
    if (err < 0)
    {
        Die("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(handle, sw_params, period_event ? buffer_size : period_size);
    if (err < 0)
    {
        Die("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (period_event)
    {
        err = snd_pcm_sw_params_set_period_event(handle, sw_params, 1);
        if (err < 0)
        {
            Die("Unable to set period event: %s\n", snd_strerror(err));
            return err;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, sw_params);
    if (err < 0)
    {
        Die("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }

    return 0;
}

void
alsa::print()
{
#ifdef DEBUG
    std::lock_guard lock(print_mtx);
    Printe("{}\n", state.song_list[state.in_q]);
    Printe("channels: {}\n", channels);
    Printe("sample_rate: {}\n", sample_rate);
    Printe("buffer_time: {}\n", buffer_time);
    Printe("period_time: {}\n", period_time);
    Printe("buffer_size: {}\n", buffer_size);
    Printe("period_size: {}\n", period_size);
    refresh();
#endif
}

int
alsa::play_chunk()
{
    long frames = snd_pcm_writei(handle, chunk.data(), chunk.size() / channels);

    if (frames < 0)
        frames = snd_pcm_recover(handle, frames, 0);
    if (frames < 0)
    {
        Die("snd_pcm_writei failed: %s\n", snd_strerror(frames));
        return -1;
    }

    return 0;
}

void
alsa::resume()
{
    snd_pcm_pause(handle, false);
}

void
alsa::pause()
{
    snd_pcm_pause(handle, true);
}

void
alsa::left()
{
    switch (type)
    {
        case file_t::OPUS:
            op_pcm_seek(opus_parser, now - period_time * def::step);
            break;

        case file_t::WAV:
            wav_parser.left();
            break;

        case file_t::MP3:
            mp3_parser.left(now);
            break;

        default:
            Printe("file type ({}) is not set?\n", (int)type);
            break;
    }
}

void
alsa::right()
{
    switch (type)
    {
        case file_t::OPUS:
            op_pcm_seek(opus_parser, now + period_time * def::step);
            break;

        case file_t::WAV:
            wav_parser.right();
            break;

        case file_t::MP3:
            mp3_parser.right(now);
            break;

        default:
            Printe("file type ({}) is not set?\n", (int)type);
            break;
    }
}

int
alsa::next_chunk()
{
    int err = 1;

    switch (type)
    {
        case file_t::OPUS:
            err = op_read_stereo(opus_parser, (s16*)chunk.data(), sizeof(s16) * period_time);
            now = op_pcm_tell(opus_parser);
            break;

        case file_t::WAV:
            err = wav_parser.next_chunk();
            now = wav_parser.now();
            break;

        case file_t::MP3:
            err = mp3_parser.next_chunk();
            now = mp3_parser.now();
            break;

        default:
            Printe("file type ({}) is not set?\n", (int)type);
            break;
    }

    return err;
}

void 
alsa::init_file_type()
{
    if (current_file.ends_with(".opus"))
    {
#ifdef DEBUG
        Printe("init type: opus\n");
#endif
        type = player::file_t::OPUS;
        init_opus();
    }
    else if (current_file.ends_with(".wav"))
    {
        type = player::file_t::WAV;
        init_wav();
    }
    else if (current_file.ends_with(".mp3"))
    {
        type = player::file_t::MP3;
        init_mp3();
    }
}

void 
alsa::init_opus()
{
    /* TODO: i don't get why but this is what works for opus */
    period_time = 1920;
    sample_rate = 48000; /* opus is always 48KHz */
    opus_parser = op_open_file(current_file.data(), NULL);

    if (!opus_parser)
    {
        Die("error opening opus file '%s'\n", current_file.data());
        exit(EXIT_FAILURE);
    }

    auto link = op_current_link(opus_parser);
    channels = op_channel_count(opus_parser, link);
    pcmtotal = op_pcm_total(opus_parser, link);

    format_size = 2;
    format = SND_PCM_FORMAT_S16_LE;

    chunk.resize(period_time, 0);
}

void
alsa::init_wav()
{
    if (wav_parser.open_file(current_file) != 0)
    {
        Die("error opening wav file '%s'\n", current_file.data());
        exit(EXIT_FAILURE);
    }

    channels = wav_parser.channels;
    sample_rate = wav_parser.sample_rate;
    pcmtotal = wav_parser.pcmtotal();

    format = wav_parser.format;
    format_size = wav_parser.format_size;

    wav_parser.period_time = period_time;
    wav_parser.period_size = period_size;

    chunk.resize(period_time, 0);
    wav_parser.chunk_ptr = &chunk;
}

void
alsa::init_mp3()
{
    if (mp3_parser.open_file(current_file) != 0)
    {
        Die("error opening mp3 file '%s'\n", current_file.data());
        exit(EXIT_FAILURE);
    }

    channels = mp3_parser.channels;
    sample_rate = mp3_parser.sample_rate;
    pcmtotal = mp3_parser.total;

    format = SND_PCM_FORMAT_S16_LE;
    format_size = 2;

    mp3_parser.period_time = period_time;
    mp3_parser.period_size = period_size;

    chunk.resize(period_time, 0);
    mp3_parser.chunk_ptr = &chunk;
}
