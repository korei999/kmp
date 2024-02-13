#pragma once
#include "util.hh"
#include "main.hh"

#include <alsa/asoundlib.h>

namespace player {
    struct Alsa {
        u8 channels;
        unsigned periodTime;
        unsigned sampleRate;
        unsigned bufferTime;

        snd_pcm_format_t format;

        snd_pcm_sframes_t bufferSize;
        snd_pcm_sframes_t periodSize;

        [[maybe_unused]] int periodEvent = 0;                /* produce poll event after each period */
    
        snd_pcm_t* handle;
        snd_pcm_hw_params_t* hwParams;
        snd_pcm_sw_params_t* swParams;

        Alsa(const std::string_view device,
                   u8 _channels,
                   unsigned _periodTime,
                   unsigned _sampleRate = g::sampleRate,
                   unsigned _bufferTime = g::bufferTime,
                   enum _snd_pcm_format _format = SND_PCM_FORMAT_S16_LE,
                   snd_pcm_access_t _access = SND_PCM_ACCESS_RW_INTERLEAVED,
                   int resample = 0)
            : channels { _channels }, periodTime { _periodTime }, sampleRate { _sampleRate }, bufferTime { _bufferTime }, format { _format }
        {
            int err;

            if ((err = snd_pcm_open(&handle, device.data(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
                Die("Playback open error: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }

            snd_pcm_hw_params_alloca(&hwParams);
            snd_pcm_sw_params_alloca(&swParams);

            err = set_hwparams(_access, resample);
            if (err < 0) {
                Die("Setting of hwParams failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }

            err = set_swparams();
            if (err < 0) {
                Die("Setting of swParams failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }
        }

        int
        SetChannels([[maybe_unused]] u8 _channels)
        {
            int err;

            /* set the count of channels */
            err = snd_pcm_hw_params_set_channels(handle, hwParams, channels);
            if (err < 0) {
                Die("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
                return err;
            }

            return 0;
        }

        int
        set_hwparams(snd_pcm_access_t access, int resample)
        {
            unsigned rrate;
            snd_pcm_uframes_t size;
            int err, dir;
         
            /* choose all parameters */
            err = snd_pcm_hw_params_any(handle, hwParams);
            if (err < 0) {
                Die("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
                return err;
            }
            /* set hardware resampling */
            err = snd_pcm_hw_params_set_rate_resample(handle, hwParams, resample);
            if (err < 0) {
                Die("Resampling setup failed for playback: %s\n", snd_strerror(err));
                return err;
            }
            /* set the interleaved read/write format */
            err = snd_pcm_hw_params_set_access(handle, hwParams, access);
            if (err < 0) {
                Die("Access type not available for playback: %s\n", snd_strerror(err));
                return err;
            }
            /* set the sample format */
            err = snd_pcm_hw_params_set_format(handle, hwParams, format);
            if (err < 0) {
                Die("Sample format not available for playback: %s\n", snd_strerror(err));
                return err;
            }
            /* set the count of channels */
            err = snd_pcm_hw_params_set_channels(handle, hwParams, channels);
            if (err < 0) {
                Die("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
                return err;
            }
            /* set the stream rate */
            rrate = sampleRate;
            err = snd_pcm_hw_params_set_rate_near(handle, hwParams, &rrate, 0);
            if (err < 0) {
                Die("Rate %uHz not available for playback: %s\n", sampleRate, snd_strerror(err));
                return err;
            }
            if (rrate != g::sampleRate) {
                Die("Rate doesn't match (requested %uHz, get %iHz)\n", sampleRate, err);
                return -EINVAL;
            }
            /* set the buffer time */
            err = snd_pcm_hw_params_set_buffer_time_near(handle, hwParams, &bufferTime, &dir);
            if (err < 0) {
                Die("Unable to set buffer time %u for playback: %s\n", bufferTime, snd_strerror(err));
                return err;
            }
            err = snd_pcm_hw_params_get_buffer_size(hwParams, &size);
            if (err < 0) {
                Die("Unable to get buffer size for playback: %s\n", snd_strerror(err));
                return err;
            }
            bufferSize = size;

            ////////////////////////////////////////
            // unsigned ptimeMin, ptimeMax;
            // snd_pcm_hw_params_get_period_time_min(hwParams, &ptimeMin, &dir);
            // snd_pcm_hw_params_get_period_time_max(hwParams, &ptimeMax, &dir);
            // g::periodTime = Clamp(g::periodTime, ptimeMin, ptimeMax);
            // periodTime = g::periodTime;
            ////////////////////////////////////////

            /* set the period time */
            err = snd_pcm_hw_params_set_period_time_near(handle, hwParams, &periodTime, &dir);
            if (err < 0) {
                Die("Unable to set period time %u for playback: %s\n", periodTime, snd_strerror(err));
                return err;
            }
            err = snd_pcm_hw_params_get_period_size(hwParams, &size, &dir);
            if (err < 0) {
                Die("Unable to get period size for playback: %s\n", snd_strerror(err));
                return err;
            }
            periodSize = size;
            /* write the parameters to device */
            err = snd_pcm_hw_params(handle, hwParams);
            if (err < 0) {
                Die("Unable to set hw hwParams for playback: %s\n", snd_strerror(err));
                return err;
            }
            return 0;
        }

        int
        set_swparams()
        {
            int err;
        
            /* get the current swparams */
            err = snd_pcm_sw_params_current(handle, swParams);
            if (err < 0) {
                Die("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
                return err;
            }
            /* start the transfer when the buffer is almost full: */
            /* (buffer_size / avail_min) * avail_min */
            err = snd_pcm_sw_params_set_start_threshold(handle, swParams, (bufferSize / periodSize) * periodSize);
            if (err < 0) {
                Die("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
                return err;
            }
            /* allow the transfer when at least period_size samples can be processed */
            /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
            err = snd_pcm_sw_params_set_avail_min(handle, swParams, periodEvent ? bufferSize : periodSize);
            if (err < 0) {
                Die("Unable to set avail min for playback: %s\n", snd_strerror(err));
                return err;
            }
            /* enable period events when requested */
            if (periodEvent) {
                err = snd_pcm_sw_params_set_period_event(handle, swParams, 1);
                if (err < 0) {
                    Die("Unable to set period event: %s\n", snd_strerror(err));
                    return err;
                }
            }
            /* write the parameters to the playback device */
            err = snd_pcm_sw_params(handle, swParams);
            if (err < 0) {
                Die("Unable to set sw params for playback: %s\n", snd_strerror(err));
                return err;
            }
            return 0;
        }
        
        int
        Play(s16* chunk, u64 size)
        {
            long frames = snd_pcm_writei(handle, chunk, size / channels);
    
            if (frames < 0)
                frames = snd_pcm_recover(handle, frames, 0);
            if (frames < 0) {
                Die("snd_pcm_writei failed: %s\n", snd_strerror(frames));
                return -1;
            }

            return 0;
        }

        void
        Resume()
        {
            snd_pcm_pause(handle, false);
        }

        void
        Pause()
        {
            snd_pcm_pause(handle, true);
        }
    
        void
        Print()
        {
#ifdef DEBUG
            std::lock_guard lock(printMtx);
            Printe("{}\n", State.songList[State.inQ]);
            Printe("channels: {}\n", channels);
            Printe("sampleRate: {}\n", sampleRate);
            Printe("bufferTime: {}\n", bufferTime);
            Printe("periodTime: {}\n", periodTime);
            Printe("bufferSize: {}\n", bufferSize);
            Printe("periodSize: {}\n", periodSize);
            refresh();
#endif
        }
    
        ~Alsa()
        {
            snd_pcm_drain(handle);
            snd_pcm_close(handle);
        }
    };
}; /* namespace player */
